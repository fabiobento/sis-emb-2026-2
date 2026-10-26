// Semana 7 — ADC (LDR em divisor) + media movel O(1) + estatistica simples
#include "freertos/FreeRTOS.h"      // Core do sistema operacional de tempo real
#include "freertos/task.h"          // Funções de manipulação e atraso de tarefas (tasks)
#include "esp_adc/adc_oneshot.h"    // Novo driver do ESP-IDF (v5+) para leituras pontuais do ADC
#include <stdio.h>                  // Funções de entrada/saída padrão (printf)
#include <math.h>                   // Funções matemáticas (sqrt para o desvio padrão)

#define M 8                      // Tamanho da janela da média móvel (M=1 equivale a não ter filtro)
#define FS_HZ 20                 // Taxa de amostragem em Hertz (20 leituras por segundo)

void app_main(void)
{
    // =========================================================================
    // 1. CONFIGURAÇÃO DA UNIDADE DO ADC (Módulo de hardware)
    // =========================================================================
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t ucfg = { 
        .unit_id = ADC_UNIT_1    // Seleciona o ADC1 (seguro para usar junto com o Wi-Fi)
    };
    // Inicializa a unidade de hardware do ADC1
    adc_oneshot_new_unit(&ucfg, &adc);

    // =========================================================================
    // 2. CONFIGURAÇÃO DO CANAL DO ADC (Pino específico)
    // =========================================================================
    adc_oneshot_chan_cfg_t ccfg = {
        .bitwidth = ADC_BITWIDTH_12,  // Resolução de 12 bits (valores de 0 a 4095)
        .atten    = ADC_ATTEN_DB_11,  // Atenuação de 11dB (amplia a faixa de leitura para ~0 a 3.1V)
    };
    // Aplica a configuração ao Canal 6 (que corresponde ao pino GPIO34 no ESP32)
    adc_oneshot_config_channel(adc, ADC_CHANNEL_6, &ccfg);

    // =========================================================================
    // 3. INICIALIZAÇÃO DE VARIÁVEIS
    // =========================================================================
    // Variáveis para o Filtro de Média Móvel O(1)
    int buf[M] = {0};            // Buffer circular para guardar as últimas M amostras
    int idx = 0;                 // Índice atual para inserção no buffer circular
    long soma = 0;               // Acumulador da soma das amostras na janela atual

    // Variáveis para a Estatística de 1 segundo (Média e Desvio Padrão)
    double s1 = 0;               // Acumulador linear (Soma de x)
    double s2 = 0;               // Acumulador quadrático (Soma de x^2) para calcular a variância
    int n = 0;                   // Contador de amostras no ciclo de 1 segundo

    // Marca o tempo inicial para garantir uma taxa de amostragem cravada (sem drift)
    TickType_t prox = xTaskGetTickCount();

    // =========================================================================
    // 4. LOOP PRINCIPAL (Tarefa)
    // =========================================================================
    while (1) {
        // Bloqueia a tarefa até o próximo instante exato de leitura (1000ms / FS_HZ)
        // Isso garante precisão temporal superior ao vTaskDelay comum
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(1000 / FS_HZ));

        int raw;
        // Realiza uma leitura única no canal configurado e guarda em 'raw'
        adc_oneshot_read(adc, ADC_CHANNEL_6, &raw);

        // ---------------------------------------------------------------------
        // ALGORITMO: Média Móvel de complexidade O(1)
        // Em vez de somar todos os elementos do vetor a cada iteração, 
        // subtraímos o valor mais antigo que está saindo da janela e somamos o novo.
        // ---------------------------------------------------------------------
        soma += raw - buf[idx];  // Adiciona a nova amostra e remove a mais antiga da soma
        buf[idx] = raw;          // Sobrescreve a amostra mais antiga no buffer com a nova
        idx = (idx + 1) % M;     // Incrementa o índice circularmente (volta a 0 quando chega em M)
        
        int filtrado = soma / M; // Calcula o valor filtrado dividindo a soma atual pelo tamanho da janela

        // ---------------------------------------------------------------------
        // ALGORITMO: Estatística Básica (Executada sobre o sinal já filtrado)
        // ---------------------------------------------------------------------
        s1 += filtrado;                          // Soma simples para a média
        s2 += (double)filtrado * filtrado;       // Soma dos quadrados para a variância/desvio padrão
        n++;                                     // Incrementa o contador de amostras

        // Se atingiu a quantidade de amostras correspondente a 1 segundo (FS_HZ):
        if (n == FS_HZ) {
            double media = s1 / n;               // E[X]: Esperança matemática (Média)
            
            // Variância = E[X^2] - (E[X])^2. O Desvio Padrão é a raiz quadrada da variância.
            double dp = sqrt(s2 / n - media * media);
            
            // O valor 'filtrado' (0-4095) é convertido para Tensão (V) assumindo limite de 3.1V do ADC
            printf("raw=%4d  filt=%4d  V=%.3f  media_1s=%.1f  desvio=%.1f\n",
                   raw, filtrado, filtrado * 3.1 / 4095.0, media, dp);
            
            // Reseta os acumuladores e o contador para o próximo ciclo de 1 segundo
            s1 = s2 = 0; 
            n = 0;
        }
    }
}