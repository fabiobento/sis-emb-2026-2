// Semana 6A — condicao de corrida e correcao com mutex
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>

#define USAR_MUTEX 0
#define N 1000000

static volatile uint32_t g_contador = 0;
static SemaphoreHandle_t g_mutex;

static void incrementador(void *arg)
{
    for (int i = 0; i < N; i++) {
#if USAR_MUTEX
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        g_contador++;
        xSemaphoreGive(g_mutex);
#else
        g_contador++;               // leitura-modificacao-escrita NAO atomica
#endif
    }
    printf("tarefa %s terminou; contador=%lu\n",
           (char *)arg, (unsigned long)g_contador);
    vTaskDelete(NULL);
}

void app_main(void)
{
    g_mutex = xSemaphoreCreateMutex();
    // mesmo nucleo p/ maximizar preempcoes visiveis
    xTaskCreatePinnedToCore(incrementador, "T1", 2048, "T1", 3, NULL, 1);
    xTaskCreatePinnedToCore(incrementador, "T2", 2048, "T2", 3, NULL, 1);
}
