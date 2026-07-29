// Semana 8B — servo SG90: 50 Hz, 14 bits; pulso 500-2400 us (ajuste fino aqui)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"

#define PIN        18
#define PULSO_MIN  500      // us  (-90 graus) — ajuste por servo
#define PULSO_MAX  2400     // us  (+90 graus)
#define PERIODO_US 20000    // 50 Hz

static uint32_t angulo_para_duty(int ang)   // ang em [-90,+90]
{
    uint32_t pulso = PULSO_MIN + (uint32_t)(ang + 90) * (PULSO_MAX - PULSO_MIN) / 180;
    return (uint32_t)((uint64_t)pulso * 16384 / PERIODO_US);   // 14 bits
}

void app_main(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .timer_num = LEDC_TIMER_1,
        .duty_resolution = LEDC_TIMER_14_BIT, .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK };
    ledc_timer_config(&t);
    ledc_channel_config_t c = {
        .gpio_num = PIN, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1, .timer_sel = LEDC_TIMER_1, .duty = 0 };
    ledc_channel_config(&c);

    while (1) {
        for (int a = -90; a <= 90; a += 5) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, angulo_para_duty(a));
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
