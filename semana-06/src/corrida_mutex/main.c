// Semana 6A — condicao de corrida e correcao com mutex
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include <stdio.h>

#define USAR_MUTEX 0
#define N 1000000

static volatile uint32_t g_contador = 0;
static SemaphoreHandle_t g_mutex;

// Em hardware real (240 MHz), a janela de risco entre o LOAD e o STORE de "g_contador++"
// dura poucos nanossegundos - curta demais para o escalonador "acertar" com frequencia
// em 1 milhao de tentativas. Esta funcao alarga essa janela de proposito (so para tornar
// a corrida observavel neste experimento), sem mudar a natureza do bug: continua sendo
// leitura-modificacao-escrita nao atomica, so que agora com tempo de sobra para colidir.
static inline void amplia_janela_de_risco(void)
{
    for (volatile int k = 0; k < 30; k++) { }
}

static void incrementador(void *arg)
{
    for (int i = 0; i < N; i++) {
#if USAR_MUTEX
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        uint32_t tmp = g_contador;      // LOAD
        amplia_janela_de_risco();
        g_contador = tmp + 1;           // STORE
        xSemaphoreGive(g_mutex);
#else
        uint32_t tmp = g_contador;      // LOAD
        amplia_janela_de_risco();       // leitura-modificacao-escrita NAO atomica
        g_contador = tmp + 1;           // STORE
#endif
    }
    printf("tarefa %s terminou; contador=%lu\n",
           (char *)arg, (unsigned long)g_contador);
    vTaskDelete(NULL);
}

void app_main(void)
{
    // T1 e T2 ficam no mesmo nucleo, mesma prioridade, e (com USAR_MUTEX 1) fazem
    // 2 milhoes de Take/Give SEM nunca ceder a CPU -> a IDLE1 daquele nucleo fica
    // sem rodar tempo suficiente para alimentar o Task Watchdog (timeout padrao 5 s),
    // e o sistema reiniciaria antes de voce ler o resultado. Provocar o watchdog nao
    // e o assunto desta parte (isso ja foi coberto nos Labs 4 e 5) - desligamos aqui
    // de proposito, so para esta medicao.
    esp_task_wdt_deinit();

    g_mutex = xSemaphoreCreateMutex();
    // mesmo nucleo p/ maximizar preempcoes visiveis
    xTaskCreatePinnedToCore(incrementador, "T1", 2048, "T1", 3, NULL, 1);
    xTaskCreatePinnedToCore(incrementador, "T2", 2048, "T2", 3, NULL, 1);
}
