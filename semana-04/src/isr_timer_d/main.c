// Semana 4 — Parte D: medindo a largura de um pulso (ponte com o Exemplo 4.3 / HC-SR04)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>

#define LED   GPIO_NUM_2
#define BTN   GPIO_NUM_4
#define DEBOUNCE_US 50000

static volatile uint32_t s_pulsos      = 0;   // incrementado só quando o pulso fecha (borda de subida)
static volatile int64_t  s_duracao_us  = 0;   // duração do último pulso medido
static int64_t s_t1               = 0;   // instante da borda de descida (início do pulso)
static volatile bool     s_medindo     = false;
static int64_t s_ultimo_evento_us = 0;        // debounce aplicado só no início do pulso

static void IRAM_ATTR btn_isr(void *arg)
{
    int64_t agora = esp_timer_get_time();
    int nivel = gpio_get_level(BTN);

    if (nivel == 0) {                                    // borda de DESCIDA: início do pulso
        if (agora - s_ultimo_evento_us > DEBOUNCE_US) {   // debounce filtra o bounce do início
            s_t1 = agora;
            s_medindo = true;
        }
    } else if (s_medindo) {                               // borda de SUBIDA: fim do pulso
        s_duracao_us = agora - s_t1;
        s_ultimo_evento_us = agora;
        s_medindo = false;
        s_pulsos++;
    }
}

static void heartbeat_cb(void *arg)
{
    static int nivel = 0;
    gpio_set_level(LED, nivel ^= 1);
}

void app_main(void)
{
    gpio_reset_pin(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    gpio_reset_pin(BTN);
    gpio_set_direction(BTN, GPIO_MODE_INPUT);
    gpio_pullup_en(BTN);
    gpio_set_intr_type(BTN, GPIO_INTR_ANYEDGE);      // Parte D, item 12: ambas as bordas
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN, btn_isr, NULL);

    const esp_timer_create_args_t targs = { .callback = heartbeat_cb, .name = "hb" };
    esp_timer_handle_t timer;
    esp_timer_create(&targs, &timer);
    esp_timer_start_periodic(timer, 500000);

    uint32_t vistos = 0;
    while (1) {
        if (s_pulsos != vistos) {
            vistos = s_pulsos;
            printf("pulso #%lu | duracao: %lld us (%.1f ms)\n",
                   (unsigned long)vistos, s_duracao_us, s_duracao_us / 1000.0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}