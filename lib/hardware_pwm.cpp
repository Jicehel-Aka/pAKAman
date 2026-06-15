#include "hardware_pwm.h"
#include "common.h"
#include "driver/ledc.h"
#include <stdio.h>

#define LEDC_TIMER     LEDC_TIMER_0
#define LEDC_MODE      LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL   LEDC_CHANNEL_0
#define LEDC_DUTY_RES  LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY PWM_LCD_FREQUENCY

void lcd_init_pwm()
{
    ledc_timer_config_t timer{};
    timer.speed_mode = LEDC_MODE;
    timer.duty_resolution = LEDC_DUTY_RES;
    timer.timer_num = LEDC_TIMER;
    timer.freq_hz = LEDC_FREQUENCY;
    timer.clk_cfg = LEDC_AUTO_CLK;

    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel{};
    channel.speed_mode = LEDC_MODE;
    channel.channel = LEDC_CHANNEL;
    channel.gpio_num = PWM_LCD_GPIO;
    channel.intr_type = LEDC_INTR_DISABLE;
    channel.timer_sel = LEDC_TIMER;
    channel.duty = 0;
    channel.hpoint = 0;

    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

void lcd_update_pwm(uint8_t duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}
