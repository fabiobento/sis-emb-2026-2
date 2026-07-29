// Semana 6B — produtor-consumidor com fila (dimensionamento do Exemplo 6.2)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>

static QueueHandle_t g_fila;

static void produtor(void *arg)          // 100 Hz
{
    TickType_t prox = xTaskGetTickCount();
    uint32_t amostra = 0;
    while (1) {
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(10));
        if (xQueueSend(g_fila, &amostra, 0) != pdTRUE)
            printf("FILA CHEIA! amostra %lu perdida\n", (unsigned long)amostra);
        amostra++;
    }
}

static void consumidor(void *arg)        // processa em rajadas (dorme 300 ms)
{
    uint32_t v; UBaseType_t max_ocup = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(300));  // simula ficar "ocupado" em outra coisa
        UBaseType_t ocup = uxQueueMessagesWaiting(g_fila);
        if (ocup > max_ocup) max_ocup = ocup;
        while (xQueueReceive(g_fila, &v, 0) == pdTRUE) { /* processa v */ }
        printf("rajada consumida; ocupacao antes=%u (max=%u)\n",
               (unsigned)ocup, (unsigned)max_ocup);
    }
}

void app_main(void)
{
    g_fila = xQueueCreate(64, sizeof(uint32_t));   // 64 itens (Exemplo 6.2)
    xTaskCreate(produtor,   "prod", 2048, NULL, 4, NULL);
    xTaskCreate(consumidor, "cons", 2048, NULL, 3, NULL);
}
