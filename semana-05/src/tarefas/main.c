// Semana 5 — três tarefas com prioridades distintas + medição de período
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdio.h>

static void tarefa(void *arg)
{
    const char *nome = (const char *)arg;
    TickType_t proximo = xTaskGetTickCount();
    int64_t t_ant = esp_timer_get_time();
    while (1) {
        // Período nominal de 500 ms SEM deriva (vTaskDelayUntil)
        vTaskDelayUntil(&proximo, pdMS_TO_TICKS(500));
        int64_t t = esp_timer_get_time();
        printf("[%s] core=%d  periodo=%.1f ms\n",
               nome, xPortGetCoreID(), (t - t_ant) / 1000.0);
        t_ant = t;
    }
}

// Descomente para o passo 2/4 do roteiro:
// static void cpu_bound(void *arg)
// {
//     volatile uint32_t x = 0;
//     while (1) { x++; }        // sem delay: monopoliza o nucleo
// }

void app_main(void)
{
    xTaskCreate(tarefa, "A", 2048, "A", 5, NULL);
    xTaskCreate(tarefa, "B", 2048, "B", 3, NULL);
    xTaskCreate(tarefa, "C", 2048, "C", 1, NULL);
    // xTaskCreatePinnedToCore(cpu_bound, "HOG", 2048, NULL, 6, NULL, 1); // passo 4
}
