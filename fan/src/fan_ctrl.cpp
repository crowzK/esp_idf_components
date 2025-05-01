#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"

#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include "gpio_def.hpp"
#include "fan_ctrl.hpp"

static const char *TAG = "Fan";

Fan& Fan::get()
{
    static Fan fan;
    return fan;
}

Fan::Fan() :
    currentFan1Rpm(0),
    currentFan2Rpm(0)
{
    ESP_LOGI(TAG, "initializing mcpwm gpio...");

    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_PWM0A_OUT);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, GPIO_PWM0B_OUT);

    // 2. initial mcpwm configuration
    ESP_LOGI(TAG, "Configuring Initial Parameters of mcpwm...");

    mcpwm_config_t pwm_config;
    pwm_config.frequency = 25000;
    pwm_config.cmpr_a = 0;
    pwm_config.cmpr_b = 0;
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config); // Configure PWM0A & PWM0B with above settings

}

Fan::~Fan()
{
}

void Fan::setFan1Pwm(float duty)
{
    std::lock_guard<std::mutex> guard(mutex);
    if(duty > 100)
    {
        return;
    }
    else if(duty > 0)
    {
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, duty);
        mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0); // call this each time, if operator was previously in low/high state
    }
    else
    {
        mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
    }
    currentFan1Rpm = duty;
}

void Fan::setFan2Pwm(float duty)
{
    std::lock_guard<std::mutex> guard(mutex);
    if(duty > 100)
    {
        return;
    }
    else if(duty > 0)
    {
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, duty);
        mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_0); // call this each time, if operator was previously in low/high state
    }
    else
    {
        mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
    }
    currentFan2Rpm = duty;
}
