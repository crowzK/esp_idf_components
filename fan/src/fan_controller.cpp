#include "fan_controller.hpp"
#include "esp_log.h"

static const char *TAG = "FanController";
constexpr int FAN_PWM_FREQ_HZ = 25000;
constexpr int PCNT_HIGH_LIMIT = 1000;
constexpr int RPM_CALC_INTERVAL_US = 1000000; // 1 second in microseconds

FanController::FanController(gpio_num_t pwm_pin, gpio_num_t tacho_pin, ledc_timer_t ledc_timer,
                             ledc_channel_t ledc_channel, pcnt_unit_handle_t pcnt_unit)
    : _pwm_pin(pwm_pin),
      _tacho_pin(tacho_pin),
      _ledc_timer(ledc_timer),
      _ledc_channel(ledc_channel),
      _pcnt_unit(pcnt_unit),
      _rpm(0),
      _rpm_timer_handle(NULL) 
{

}

FanController::~FanController()
{
    if (_rpm_timer_handle)
    {
        esp_timer_stop(_rpm_timer_handle);
        esp_timer_delete(_rpm_timer_handle);
    }
    if (_pcnt_unit)
    {
        pcnt_unit_disable(_pcnt_unit);
        // Add cleanup code for pcnt resources here if needed, e.g., pcnt_del_unit().
    }
    // Add cleanup for ledc resources if needed.
}

void FanController::init_pwm()
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = _ledc_timer,
        .freq_hz = FAN_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t channel_conf = {
        .gpio_num = _pwm_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = _ledc_channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = _ledc_timer,
        .duty = 0,
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));
}

void FanController::init_rpm_counter()
{
    // FIX: The order of designated initializers must match the struct definition.
    pcnt_unit_config_t unit_config = {
        .low_limit = -PCNT_HIGH_LIMIT,
        .high_limit = PCNT_HIGH_LIMIT,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &_pcnt_unit));

    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = _tacho_pin,
        .level_gpio_num = -1,
    };
    pcnt_channel_handle_t pcnt_chan = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(_pcnt_unit, &chan_config, &pcnt_chan));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD));
    ESP_ERROR_CHECK(pcnt_unit_enable(_pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(_pcnt_unit));
}

void FanController::begin()
{
    init_pwm();
    init_rpm_counter();

    // Configure the timer
    const esp_timer_create_args_t timer_args = {
        .callback = &FanController::rpm_timer_callback,
        .arg = this, // Pass the current object instance to the callback
        .name = "rpm_timer"};

    // Create and start the periodic timer
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &_rpm_timer_handle));
    ESP_ERROR_CHECK(esp_timer_start_periodic(_rpm_timer_handle, RPM_CALC_INTERVAL_US));

    ESP_LOGI(TAG, "Fan Controller on PWM_PIN: %d, TACHO_PIN: %d initialized with esp_timer.", _pwm_pin, _tacho_pin);
}

void FanController::set_speed(uint8_t percentage)
{
    if (percentage > 100)
    {
        percentage = 100;
    }
    uint32_t duty = (1023 * percentage) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, _ledc_channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, _ledc_channel));
    ESP_LOGI(TAG, "Fan on pin %d speed set to %d%%", _pwm_pin, percentage);
}

int FanController::get_rpm() const
{
    return _rpm;
}

void FanController::rpm_timer_callback(void *arg)
{
    FanController *fan = static_cast<FanController *>(arg);
    int pulse_count = 0;

    // Get the pulse count from PCNT
    ESP_ERROR_CHECK(pcnt_unit_get_count(fan->_pcnt_unit, &pulse_count));

    // Fans typically generate 2 pulses per revolution.
    // RPM = (pulses_per_second * 60) / pulses_per_revolution
    fan->_rpm = (pulse_count * 60) / 2;

    // Clear the PCNT counter for the next interval
    ESP_ERROR_CHECK(pcnt_unit_clear_count(fan->_pcnt_unit));

    // Note: Avoid heavy logging or blocking operations inside a timer callback
}
