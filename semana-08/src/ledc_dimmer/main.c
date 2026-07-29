// Semana 8A — LEDC: dimmer 12 bits @ 5 kHz (Exemplo 8.1)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"

#define PIN 2

void app_main(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_12_BIT, .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK };
    ledc_timer_config(&t);
    ledc_channel_config_t c = {
        .gpio_num = PIN, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0, .duty = 0 };
    ledc_channel_config(&c);

    while (1) {
        for (int d = 0; d <= 4095; d += 32) {          // rampa de subida
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, d);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
