// Semana 3 — botão com pull-up interno + debounce por software (polling)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>

#define LED   GPIO_NUM_2
#define BTN   GPIO_NUM_0
#define DEBOUNCE_MS 20

void app_main(void)
{
    gpio_reset_pin(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    gpio_reset_pin(BTN);
    gpio_set_direction(BTN, GPIO_MODE_INPUT);
    gpio_pullup_en(BTN);              // repouso = 1; pressionado = 0

    int  led = 0, eventos = 0;
    int  nivel_ant = 1;
    int64_t t_ok = 0;                 // instante a partir do qual aceitamos nova borda

    while (1) {
        int nivel = gpio_get_level(BTN);
        int64_t agora = esp_timer_get_time() / 1000;      // ms
        if (nivel_ant == 1 && nivel == 0 && agora >= t_ok) {  // borda de descida válida
            led = !led;
            gpio_set_level(LED, led);
            printf("evento #%d\n", ++eventos);
            t_ok = agora + DEBOUNCE_MS;
        }
        nivel_ant = nivel;
        // Garantir pelo menos 1 tick no padrão de 100Hz do ESP-IDF
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
