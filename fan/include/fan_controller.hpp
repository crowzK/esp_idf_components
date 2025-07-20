#pragma once

#include "driver/ledc.h"
#include "driver/pulse_cnt.h"
#include "esp_timer.h"

/**
 * @class FanPidController
 * @brief A fan controller that uses a PID loop to maintain a target RPM.
 */
class FanPidController {
public:
    /**
     * @brief Constructor for the PID Fan Controller.
     * @param pwm_pin GPIO for PWM output.
     * @param tacho_pin GPIO for tachometer input.
     * @param ledc_timer LEDC timer to use.
     * @param ledc_channel LEDC channel to use.
     */
    FanPidController(gpio_num_t pwm_pin, gpio_num_t tacho_pin, ledc_timer_t ledc_timer, ledc_channel_t ledc_channel);
    ~FanPidController();

    /**
     * @brief Initializes the hardware and starts the control loops.
     */
    void begin();

    /**
     * @brief Sets the desired target speed in RPM.
     * @param rpm The target RPM.
     */
    void set_target_rpm(int rpm);

    /**
     * @brief Gets the current measured RPM.
     * @return The current RPM.
     */
    int get_current_rpm() const;

    /**
     * @brief Gets the current PWM duty cycle (0-1023).
     * @return The current PWM duty value.
     */
    int get_current_pwm_duty() const;

private:
    // Hardware Configuration
    gpio_num_t _pwm_pin;
    gpio_num_t _tacho_pin;
    ledc_timer_t _ledc_timer;
    ledc_channel_t _ledc_channel;

    // RPM Measurement
    pcnt_unit_handle_t _pcnt_unit;
    esp_timer_handle_t _rpm_measure_timer;
    volatile int _current_rpm;

    // PID Control
    esp_timer_handle_t _pid_control_timer;
    volatile int _target_rpm;
    volatile int _current_pwm_duty; // Range: 0-1023
    double _kp, _ki, _kd;
    double _integral_sum;
    double _last_error;

    void init_pwm();
    void init_rpm_counter();

    static void rpm_measure_callback(void* arg);
    static void pid_control_callback(void* arg);
    void update_pid_control();
};