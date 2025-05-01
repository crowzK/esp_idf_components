#pragma once
#include "gpio_def.hpp"
#include <mutex>

class Fan
{
public:
    ~Fan();
    void setFan1Pwm(float duty);
    void setFan2Pwm(float duty);
    float getFan1Pwm() { return currentFan1Rpm; };
    float getFan2Pwm() { return currentFan2Rpm; };
    static Fan& get();

private:
    static constexpr gpio_num_t GPIO_PWM0A_OUT = GPIO_FAN1_PWM;
    static constexpr gpio_num_t GPIO_PWM0B_OUT = GPIO_FAN2_PWM;
    volatile float currentFan1Rpm;
    volatile float currentFan2Rpm;
    std::mutex mutex;

    Fan();
};


