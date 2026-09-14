// Semana 4 — interrupção de GPIO + esp_timer periódico + demonstração de WDT
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>

#define LED   GPIO_NUM_2
#define BTN   GPIO_NUM_4    // GPIO0 é o pino de boot — evite usá-lo com botão (ver Semana 3, Parte D)
#define DEBOUNCE_US 20000
#define PROVOCAR_WDT 0

static volatile uint32_t s_eventos = 0;     // regra 5: escrito na ISR, lido na tarefa
static volatile int64_t  s_t_isr   = 0;
static int64_t s_ultimo_evento_us  = 0;

// ISR: curta, sem bloqueio, sem printf!
static void IRAM_ATTR btn_isr(void *arg)    // regra 3: IRAM_ATTR
{
    int64_t agora = esp_timer_get_time();
    if (agora - s_ultimo_evento_us > DEBOUNCE_US) {    // debounce DENTRO da ISR: só aritmética
        s_ultimo_evento_us = agora;
        s_eventos++;                        // regra 1: só captura e conta
        s_t_isr = agora;                    // carimbo p/ medir latência na tarefa
    }
}                                           // regra 2: nada de printf/delay aqui!

static void heartbeat_cb(void *arg)         // callback: roda em contexto de TAREFA
{                                           // (uma tarefa dedicada do esp_timer)
    static int nivel = 0;
    gpio_set_level(LED, nivel ^= 1);        // XOR alterna (semana 3!)
}

void app_main(void)
{
    gpio_reset_pin(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    gpio_reset_pin(BTN);
    gpio_set_direction(BTN, GPIO_MODE_INPUT);
    gpio_pullup_en(BTN);
    gpio_set_intr_type(BTN, GPIO_INTR_NEGEDGE);     // dispare na borda de DESCIDA (ativo-baixo)
    gpio_install_isr_service(0);                    // serviço de despacho de ISRs de GPIO
    gpio_isr_handler_add(BTN, btn_isr, NULL);       // registra btn_isr na tabela de despacho
                                                    // por pino do driver (não é o vetor da CPU)

    const esp_timer_create_args_t targs = { .callback = heartbeat_cb, .name = "hb" };
    esp_timer_handle_t timer;
    esp_timer_create(&targs, &timer);
    esp_timer_start_periodic(timer, 500000);          // período em µs: 500 ms

#if PROVOCAR_WDT
    while (1) { }   // monopoliza a CPU: Task WDT dispara (veja o monitor)
#endif

    uint32_t vistos = 0;
    while (1) {
        if (s_eventos != vistos) {                    // a TAREFA imprime, não a ISR
            vistos = s_eventos;
            printf("evento #%lu | latencia ate a tarefa: %lld us\n",
                   (unsigned long)vistos, esp_timer_get_time() - s_t_isr);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
