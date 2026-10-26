// Semana 7E — loopback DAC -> ADC na MESMA placa: ver o aliasing acontecer
// -----------------------------------------------------------------------------
// PRÁTICA: Conecte fisicamente um Jumper do GPIO25 (saída do DAC) ao GPIO34 (entrada do ADC).
// OBJETIVO: O DAC gera uma senoide contínua na frequência F_HZ. O ADC fará a amostragem
//           desse sinal a uma taxa FS_HZ. Isso permite observar na prática o Teorema de Nyquist.
//           Experimente alterar F_HZ para 2, 8, 95 e 100 Hz e veja o efeito de "dobramento" (aliasing)
//           no Serial Plotter quando F_HZ ultrapassa a frequência de Nyquist (FS_HZ / 2).
// -----------------------------------------------------------------------------

#include "freertos/FreeRTOS.h"      // Core do sistema operacional
#include "freertos/task.h"          // Controle de tarefas e atrasos de precisão
#include "driver/dac_oneshot.h"     // Driver para o Conversor Digital-Analógico (DAC)
#include "esp_adc/adc_oneshot.h"    // Driver para o Conversor Analógico-Digital (ADC)
#include "esp_timer.h"              // Timer de alta resolução (hardware/software)
#include <math.h>                   // Funções matemáticas (sin, round, fabs, M_PI)
#include <stdio.h>                  // printf para plotagem serial

#define NPTS  32         // Número de amostras (pontos) para formar 1 ciclo da senoide no DAC
#define F_HZ  2          // Frequência desejada da senoide gerada pelo DAC (Mude para testar o aliasing!)
#define FS_HZ 20         // Taxa de amostragem do ADC. Segundo Nyquist, a frequência máxima 
                         // observável sem aliasing é FS_HZ / 2 = 10 Hz.

// =========================================================================
// VARIÁVEIS GLOBAIS (DAC)
// =========================================================================
static dac_oneshot_handle_t s_dac; // Handle de controle do DAC
static uint8_t s_tab[NPTS];        // Tabela Look-up (LUT) contendo os pontos da senoide
static volatile int s_i = 0;       // Índice atual (volatile pois é alterado dentro da interrupção/timer)

// =========================================================================
// FUNÇÃO CALLBACK DO TIMER (Geração do sinal analógico)
// =========================================================================
// Chamada a uma taxa de NPTS * F_HZ. Atualiza a saída do DAC rapidamente em background.
static void tick_cb(void *arg)               
{
    dac_oneshot_output_voltage(s_dac, s_tab[s_i]); // Joga o nível de tensão no pino
    s_i = (s_i + 1) % NPTS;                        // Avança circularmente na tabela
}

void app_main(void)
{
    // =========================================================================
    // 1. PRÉ-COMPUTAÇÃO DA SENOIDE (LUT)
    // =========================================================================
    for (int i = 0; i < NPTS; i++) {
        // CUIDADO DE HARDWARE: O DAC do ESP32 vai de 0 a 255 (0 a ~3.3V).
        // Porém, o ADC com atenuação de 11dB (padrão) satura em cerca de 3.1V.
        // Se usássemos amplitude 127.5 (máxima do DAC), o ADC "cortaria" o topo da onda.
        // Por isso, usamos amplitude 100.0. O sinal oscilará entre 27.5 e 227.5 no DAC,
        // o que equivale fisicamente a ~0.35V até ~2.9V, ficando perfeitamente na zona
        // linear de leitura do ADC.
        s_tab[i] = (uint8_t)(127.5 + 100.0 * sin(2 * M_PI * i / NPTS));
    }

    // =========================================================================
    // 2. CONFIGURAÇÃO DOS PERIFÉRICOS (DAC e ADC)
    // =========================================================================
    // Configura DAC (Saída)
    dac_oneshot_config_t dcfg = { .chan_id = DAC_CHAN_0 };      // DAC Canal 0 = GPIO25
    dac_oneshot_new_channel(&dcfg, &s_dac);

    // Configura ADC (Entrada)
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&ucfg, &adc);
    
    adc_oneshot_chan_cfg_t ccfg = { 
        .bitwidth = ADC_BITWIDTH_12,   // Resolução: 12 bits (0-4095)
        .atten = ADC_ATTEN_DB_11       // Atenuação: 11dB (mede até ~3.1V)
    };
    adc_oneshot_config_channel(adc, ADC_CHANNEL_6, &ccfg);      // ADC1 Canal 6 = GPIO34

    // =========================================================================
    // 3. CÁLCULO TEÓRICO DE ALIASING (Física e Matemática aplicada)
    // =========================================================================
    // O esp_timer recebe o período em números inteiros (microssegundos).
    // O truncamento/arredondamento faz com que a frequência física real gerada seja 
    // ligeiramente diferente da nominal solicitada.
    int per_us = 1000000 / (NPTS * F_HZ);
    double f_real = 1e6 / per_us / NPTS; // Recalcula a frequência com base no timer real
    
    // Cálculo da frequência aparente (alias) vista pelo ADC.
    // Baseado na teoria de processamento de sinais: f_alias = | f_real - k * FS |
    // onde 'k' é o número inteiro mais próximo da razão (f_real / FS).
    double k = round(f_real / FS_HZ);
    double f_alias = fabs(f_real - k * FS_HZ);               
    
    // Imprime as informações no console para o aluno conferir a teoria com a prática
    printf("DAC gera %.2f Hz | ADC amostra a %d Hz (Nyquist = %.1f Hz) | alias previsto = %.2f Hz\n",
           f_real, FS_HZ, FS_HZ / 2.0, f_alias);

    // =========================================================================
    // 4. INICIALIZAÇÃO DO TIMER DO DAC
    // =========================================================================
    const esp_timer_create_args_t targs = { .callback = tick_cb, .name = "dac" };
    esp_timer_handle_t t;
    esp_timer_create(&targs, &t);
    esp_timer_start_periodic(t, per_us); // Inicia a geração da senoide em background

    // =========================================================================
    // 5. LOOP DE AMOSTRAGEM DO ADC (Task principal)
    // =========================================================================
    TickType_t prox = xTaskGetTickCount();
    
    while (1) {
        // vTaskDelayUntil garante que a amostragem ocorra EXATAMENTE a cada ciclo 
        // de FS_HZ, evitando atrasos cumulativos que arruinariam a análise de sinais.
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(1000 / FS_HZ));   
        
        int raw;
        adc_oneshot_read(adc, ADC_CHANNEL_6, &raw); // Lê o pino GPIO34
        
        // Imprime apenas o valor puro, sem textos. 
        // Isso permite que o Serial Plotter do VS Code/Arduino IDE desenhe o gráfico nativamente.
        printf("%d\n", raw);                                   
    }
}