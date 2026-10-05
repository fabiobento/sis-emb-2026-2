// Semana 6C — ISR -> tarefa com semaforo (binario ou contador)
// Mesma ideia do botao da Semana 4, agora na forma profissional: a ISR so GIVE,
// a tarefa bloqueia em TAKE e faz o trabalho pesado (debounce, print, latencia).
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>

#define BTN GPIO_NUM_4     // NUNCA GPIO0 com botao (pino de boot - ver Semana 3, Parte D)
#define DEBOUNCE_US 20000  // 20 ms - mesmo valor validado no Lab 3, Parte C

// item 23 do roteiro: troque para 1 e regrave para comparar binario x contador
#define USAR_CONTADOR 0

static SemaphoreHandle_t sem;
static volatile int64_t s_t_isr = 0;          // carimbo da borda, lido pela tarefa
static int64_t s_ultimo_evento_us = 0;        // debounce, dentro da propria ISR

static void IRAM_ATTR isr_botao(void *arg)
{
    int64_t agora = esp_timer_get_time();
    if (agora - s_ultimo_evento_us <= DEBOUNCE_US) {
        return;                               // bounce: ignora, nao conta, nao da
    }
    s_ultimo_evento_us = agora;
    s_t_isr = agora;

    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(sem, &woken);        // ~1 us: so sinaliza "aconteceu"
    portYIELD_FROM_ISR(woken);                 // troca de contexto JA, se preciso
}

static void tarefa_botao(void *arg)
{
    uint32_t eventos = 0;
    while (1) {
        xSemaphoreTake(sem, portMAX_DELAY);    // dorme sem gastar CPU ate o give
        int64_t latencia_us = esp_timer_get_time() - s_t_isr;
        printf("evento #%lu | latencia ISR->tarefa: %lld us\n",
               (unsigned long)(++eventos), latencia_us);
    }
}

void app_main(void)
{
    gpio_reset_pin(BTN);
    gpio_set_direction(BTN, GPIO_MODE_INPUT);
    gpio_pullup_en(BTN);
    gpio_set_intr_type(BTN, GPIO_INTR_NEGEDGE);   // borda de descida (ativo-baixo)
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN, isr_botao, NULL);

#if USAR_CONTADOR
    sem = xSemaphoreCreateCounting(10, 0);        // ate 10 eventos pendentes, comeca em 0
#else
    sem = xSemaphoreCreateBinary();               // satura em 1: rajada rapida "come" eventos
#endif

    xTaskCreate(tarefa_botao, "botao", 2048, NULL, 4, NULL);
}
