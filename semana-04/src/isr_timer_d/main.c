// Semana 4 — Parte D: medindo a largura de um pulso (ponte com o Exemplo 4.3 / HC-SR04)
//
// O pulso agora é gerado pelo PRÓPRIO ESP32 (gerador_task, no GPIO18) e chega ao
// pino de medição (GPIO4) por um jumper físico — não é mais um botão. Isso elimina
// o bounce mecânico de vez: o sinal é uma saída digital limpa (push-pull), então a
// ISR não precisa de nenhum debounce para medir corretamente. O foco fica 100% no
// conceito de medição de largura de pulso (t1/t2, GPIO_INTR_ANYEDGE), sem o ruído
// de "meu botão quica diferente do seu".
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>

#define LED      GPIO_NUM_2
#define IN_PIN   GPIO_NUM_4     // mede a largura do pulso (ISR, GPIO_INTR_ANYEDGE)
#define OUT_PIN  GPIO_NUM_18    // gera o pulso — ligue um jumper físico daqui até o IN_PIN

static volatile uint32_t s_pulsos     = 0;   // incrementado só quando o pulso fecha (borda de subida)
static volatile int64_t  s_duracao_us = 0;   // duração do último pulso medido
static int64_t s_t1                   = 0;   // instante da borda de descida (início do pulso)
static volatile bool     s_medindo    = false;

static void IRAM_ATTR in_isr(void *arg)
{
    int64_t agora = esp_timer_get_time();
    int nivel = gpio_get_level(IN_PIN);

    // SEM debounce aqui: o sinal vem de outro GPIO do próprio chip (push-pull, sem
    // contato mecânico), então não existe bounce para filtrar — cada borda é real.
    if (nivel == 0) {                        // descida: início do pulso
        s_t1 = agora;
        s_medindo = true;
    } else if (s_medindo) {                  // subida: fim do pulso
        s_duracao_us = agora - s_t1;
        s_medindo = false;
        s_pulsos++;
    }
}

static void heartbeat_cb(void *arg)
{
    static int nivel = 0;
    gpio_set_level(LED, nivel ^= 1);
}

// Gera pulsos de largura CONHECIDA, alternando curto/longo — o "gabarito" contra o
// qual você compara a leitura da ISR. Ativo-baixo, igual ao botão das partes A-C.
static void gerador_task(void *arg)
{
    const int larguras_ms[] = { 150, 1200 };   // curto, longo (item 14)
    int i = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));        // "repouso" entre pulsos, nível alto
        gpio_set_level(OUT_PIN, 0);              // início do pulso
        vTaskDelay(pdMS_TO_TICKS(larguras_ms[i]));
        gpio_set_level(OUT_PIN, 1);               // fim do pulso
        i = (i + 1) % (sizeof(larguras_ms) / sizeof(larguras_ms[0]));
    }
}

void app_main(void)
{
    gpio_reset_pin(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    gpio_reset_pin(OUT_PIN);
    gpio_set_direction(OUT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(OUT_PIN, 1);              // repouso em nível alto (ativo-baixo)

    gpio_reset_pin(IN_PIN);
    gpio_set_direction(IN_PIN, GPIO_MODE_INPUT);   // sem pull-up: quem define o nível
                                                    // é sempre o OUT_PIN, ativamente
    gpio_set_intr_type(IN_PIN, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(IN_PIN, in_isr, NULL);

    const esp_timer_create_args_t targs = { .callback = heartbeat_cb, .name = "hb" };
    esp_timer_handle_t timer;
    esp_timer_create(&targs, &timer);
    esp_timer_start_periodic(timer, 500000);

    xTaskCreate(gerador_task, "gerador", 2048, NULL, 5, NULL);

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
