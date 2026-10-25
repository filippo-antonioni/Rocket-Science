#pragma once
#include <cstdint>
typedef int esp_err_t;
constexpr int ESP_OK=0, LEDC_LOW_SPEED_MODE=0, LEDC_CHANNEL_0=0,
    LEDC_TIMER_0=0, LEDC_TIMER_14_BIT=14, LEDC_AUTO_CLK=0, LEDC_INTR_DISABLE=0;
struct ledc_timer_config_t {int speed_mode,duty_resolution,timer_num;uint32_t freq_hz;int clk_cfg;};
struct ledc_channel_config_t {int gpio_num,speed_mode,channel,intr_type,timer_sel;uint32_t duty,hpoint;};
esp_err_t ledc_timer_config(const ledc_timer_config_t*);
esp_err_t ledc_channel_config(const ledc_channel_config_t*);
esp_err_t ledc_set_duty(int,int,uint32_t);esp_err_t ledc_update_duty(int,int);
esp_err_t ledc_stop(int,int,uint32_t);uint32_t ledc_get_freq(int,int);
