#pragma once
#include <driver/ledc.h>
#include "config.h"

// Same 50 Hz, 500..2400 us mapping as the already-tested servo diagnostic.
// No delay/pulseIn in the acquisition path; hardware generates the PWM.
class PowerServo {
public:
    bool begin() {
        ledc_timer_config_t timer={};
        timer.speed_mode=LEDC_LOW_SPEED_MODE;timer.duty_resolution=LEDC_TIMER_14_BIT;
        timer.timer_num=LEDC_TIMER_0;timer.freq_hz=50;timer.clk_cfg=LEDC_AUTO_CLK;
        if(ledc_timer_config(&timer)!=ESP_OK) return false;
        ledc_channel_config_t channel={};
        channel.gpio_num=cfg::servoPin;channel.speed_mode=LEDC_LOW_SPEED_MODE;
        channel.channel=LEDC_CHANNEL_0;channel.intr_type=LEDC_INTR_DISABLE;
        channel.timer_sel=LEDC_TIMER_0;channel.duty=0;channel.hpoint=0;
        return ledc_channel_config(&channel)==ESP_OK;
    }
    bool write(bool open) {
        uint32_t angle=open?cfg::servoAperto:cfg::servoChiuso;
        uint32_t pulseUs=500u+angle*1900u/180u;
        uint32_t duty=(pulseUs*(1u<<14)+10000u)/20000u;
        return ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,duty)==ESP_OK &&
               ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0)==ESP_OK;
    }
};
