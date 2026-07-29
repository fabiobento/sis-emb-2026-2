// Semana 8C — motor DC via L298N: ENA=PWM(GPIO16), IN1=GPIO17, IN2=GPIO18
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include <stdio.h>

#define ENA 16
#define IN1 17
#define IN2 18

static void sentido(int frente)
{
    gpio_set_level(IN1, frente);
    gpio_set_level(IN2, !frente);
}
static void velocidade(int pct)              // 0..100 %
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, pct * 1023 / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}

void app_main(void)
{
    gpio_config_t io = { .pin_bit_mask = (1ULL<<IN1)|(1ULL<<IN2),
                         .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io);
    ledc_timer_config_t t = { .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_2, .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 1000, .clk_cfg = LEDC_AUTO_CLK };
    ledc_timer_config(&t);
    ledc_channel_config_t c = { .gpio_num = ENA, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_2, .timer_sel = LEDC_TIMER_2, .duty = 0 };
    ledc_channel_config(&c);

    while (1) {
        printf("frente: acelerando\n");
        sentido(1);
        for (int p = 0; p <= 100; p += 10) { velocidade(p); vTaskDelay(pdMS_TO_TICKS(300)); }
        printf("freio\n");
        velocidade(0); vTaskDelay(pdMS_TO_TICKS(1000));
        printf("re a 60%%\n");
        sentido(0); velocidade(60); vTaskDelay(pdMS_TO_TICKS(2000));
        velocidade(0); vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
