#include "fan_controller.hpp"
#include "esp_log.h"
#include <algorithm> // For std::min and std::max

static const char *TAG = "FanPidController";

// --- PID Control Constants (Tuned for stability) ---
constexpr double PID_KP = 0.1;   // Proportional gain
constexpr double PID_KI = 0.02;  // Integral gain
constexpr double PID_KD = 0.01;  // Derivative gain
// ---------------------------------------------------------

constexpr int PWM_MIN_DUTY = 256; // 25% Duty (Minimum speed to keep the fan spinning)
constexpr int PWM_MAX_DUTY = 1023; // 100% Duty
constexpr int RPM_MEASURE_INTERVAL_US = 500000;  // Measure RPM every 0.5 seconds
constexpr int PID_CONTROL_INTERVAL_US = 1000000; // PID control every 1 second
constexpr double INTEGRAL_CLAMP_MAX = 4000.0; // Integral windup guard limit

FanPidController::FanPidController(gpio_num_t pwm_pin, gpio_num_t tacho_pin, ledc_timer_t ledc_timer, ledc_channel_t ledc_channel)
    : _pwm_pin(pwm_pin),
      _tacho_pin(tacho_pin),
      _ledc_timer(ledc_timer),
      _ledc_channel(ledc_channel),
      _pcnt_unit(nullptr),
      _rpm_measure_timer(nullptr),
      _current_rpm(0),
      _pid_control_timer(nullptr),
      _target_rpm(0),
      _current_pwm_duty(0),
      _kp(PID_KP),
      _ki(PID_KI),
      _kd(PID_KD),
      _integral_sum(0),
      _last_error(0) {}

FanPidController::~FanPidController() {
    if (_rpm_measure_timer) esp_timer_delete(_rpm_measure_timer);
    if (_pid_control_timer) esp_timer_delete(_pid_control_timer);
    if (_pcnt_unit) pcnt_del_unit(_pcnt_unit);
}

void FanPidController::begin() {
    init_pwm();
    init_rpm_counter();

    const esp_timer_create_args_t rpm_timer_args = {
        .callback = &FanPidController::rpm_measure_callback,
        .arg = this,
        .name = "rpm_measure"
    };
    ESP_ERROR_CHECK(esp_timer_create(&rpm_timer_args, &_rpm_measure_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(_rpm_measure_timer, RPM_MEASURE_INTERVAL_US));

    const esp_timer_create_args_t pid_timer_args = {
        .callback = &FanPidController::pid_control_callback,
        .arg = this,
        .name = "pid_control"
    };
    ESP_ERROR_CHECK(esp_timer_create(&pid_timer_args, &_pid_control_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(_pid_control_timer, PID_CONTROL_INTERVAL_US));

    ESP_LOGI(TAG, "PID Fan Controller initialized.");
}

void FanPidController::set_target_rpm(int rpm) {
    _target_rpm = std::max(0, rpm);
    if (_target_rpm == 0) {
        // If the target is 0, reset the integral immediately.
        _integral_sum = 0;
        _last_error = 0;
    }
    ESP_LOGI(TAG, "New target RPM set to: %d", _target_rpm);
}

int FanPidController::get_current_rpm() const {
    return _current_rpm;
}

int FanPidController::get_current_pwm_duty() const {
    return _current_pwm_duty;
}

void FanPidController::init_pwm() {
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = _ledc_timer,
        .freq_hz = 25000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t channel_conf = {
        .gpio_num = _pwm_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = _ledc_channel,
        .timer_sel = _ledc_timer,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));
}

void FanPidController::init_rpm_counter() {
    pcnt_unit_config_t unit_config = {
        .low_limit = -1000,
        .high_limit = 1000,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &_pcnt_unit));

    pcnt_chan_config_t chan_config = { .edge_gpio_num = _tacho_pin, .level_gpio_num = -1 };
    pcnt_channel_handle_t pcnt_chan = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(_pcnt_unit, &chan_config, &pcnt_chan));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD));
    
    ESP_ERROR_CHECK(pcnt_unit_enable(_pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(_pcnt_unit));
}

void FanPidController::rpm_measure_callback(void* arg) {
    FanPidController* controller = static_cast<FanPidController*>(arg);
    int pulse_count = 0;
    pcnt_unit_get_count(controller->_pcnt_unit, &pulse_count);
    
    // Convert pulse count to RPM (2 pulses per revolution, measured over 0.5 seconds)
    controller->_current_rpm = (pulse_count * 60 * (1000000 / RPM_MEASURE_INTERVAL_US)) / 2;
    
    pcnt_unit_clear_count(controller->_pcnt_unit);
}

void FanPidController::pid_control_callback(void* arg) {
    static_cast<FanPidController*>(arg)->update_pid_control();
}

void FanPidController::update_pid_control() {
    if (_target_rpm <= 0) {
        _current_pwm_duty = 0;
        _integral_sum = 0; // Reset integral when fan stops
        _last_error = 0;   // Reset last error when fan stops
    } else {
        double error = _target_rpm - _current_rpm;
        
        // Calculate integral term
        _integral_sum += error;
        // Clamp the integral sum to prevent windup
        _integral_sum = std::max(-INTEGRAL_CLAMP_MAX, std::min(INTEGRAL_CLAMP_MAX, _integral_sum));
        
        // Calculate derivative term
        double derivative = error - _last_error;
        
        // Calculate PID output
        double output = (_kp * error) + (_ki * _integral_sum) + (_kd * derivative);
        
        // Use a local variable for calculations to avoid issues with volatile.
        int next_pwm_duty = _current_pwm_duty + static_cast<int>(output);
        
        // Clamp the PWM duty to the min/max range
        next_pwm_duty = std::max(PWM_MIN_DUTY, std::min(PWM_MAX_DUTY, next_pwm_duty));
        
        _current_pwm_duty = next_pwm_duty;
        
        _last_error = error;
    }
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, _ledc_channel, _current_pwm_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, _ledc_channel);

    // Change log level to INFO for easier debugging.
    ESP_LOGD(TAG, "Target: %d, Current: %d, PWM: %d, Error: %.2f, Integral: %.2f", 
             (int)_target_rpm, (int)_current_rpm, (int)_current_pwm_duty, _last_error, _integral_sum);
}
