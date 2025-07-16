#pragma once

#include "driver/ledc.h"
#include "driver/pulse_cnt.h"
#include "esp_timer.h" // Add this for esp_timer
#include "freertos/FreeRTOS.h"

class FanController {
public:
    /**
     * @brief FanController constructor
     *
     * @param pwm_pin GPIO pin for fan PWM control
     * @param tacho_pin GPIO pin for fan tachometer (RPM)
     * @param ledc_timer LEDC timer to use
     * @param ledc_channel LEDC channel to use
     * @param pcnt_unit PCNT unit to use (pass a non-NULL handle if already initialized)
     */
    FanController(gpio_num_t pwm_pin, gpio_num_t tacho_pin, ledc_timer_t ledc_timer,
                  ledc_channel_t ledc_channel, pcnt_unit_handle_t pcnt_unit = nullptr);

    ~FanController();

    /**
     * @brief Initializes the fan driver and starts the RPM measurement timer.
     */
    void begin();

    /**
     * @brief Sets the fan speed as a percentage (0-100).
     * @param percentage The desired speed percentage.
     */
    void set_speed(uint8_t percentage);

    /**
     * @brief Returns the last measured RPM value.
     * @return int The fan's RPM.
     */
    int get_rpm() const;

private:
    // GPIO pins
    gpio_num_t _pwm_pin;
    gpio_num_t _tacho_pin;

    // LEDC configuration
    ledc_timer_t _ledc_timer;
    ledc_channel_t _ledc_channel;

    // PCNT configuration
    pcnt_unit_handle_t _pcnt_unit;

    // RPM
    volatile int _rpm;

    // esp_timer handle
    esp_timer_handle_t _rpm_timer_handle;

    void init_pwm();
    void init_rpm_counter();

    // Static timer callback function for RPM measurement.
    static void rpm_timer_callback(void *arg);
};
