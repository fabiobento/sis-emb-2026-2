// Semana 7 — DAC: senoide de 100 Hz com 32 pontos/ciclo (GPIO25)
#include "freertos/FreeRTOS.h"     // Core do sistema operacional de tempo real
#include "freertos/task.h"         // Necessário para as macros e tipos do FreeRTOS
#include "driver/dac_oneshot.h"    // Novo driver do ESP-IDF (v5+) para controle do DAC
#include "esp_timer.h"             // Driver de timer de alta resolução por software/hardware do ESP32
#include <math.h>                  // Necessário para a função sin() e a constante M_PI

#define NPTS 32                    // Número de amostras (pontos) para formar 1 ciclo completo da senoide
#define F_HZ 100                   // Frequência desejada da senoide em Hertz (100 ciclos por segundo)

// =========================================================================
// VARIÁVEIS GLOBAIS
// =========================================================================
static dac_oneshot_handle_t s_dac; // Handle (ponteiro de controle) para a unidade do DAC
static uint8_t s_tab[NPTS];        // Tabela de busca (Look-up Table - LUT) armazenará os valores da senoide

// O modificador 'volatile' avisa ao compilador que essa variável será alterada 
// dentro de uma rotina de interrupção (ou callback de timer). Isso impede que
// o compilador faça otimizações perigosas, forçando a leitura real da memória.
static volatile int s_i = 0;       // Índice atual de leitura da tabela pre-computada

// =========================================================================
// FUNÇÃO CALLBACK DO TIMER (Executada a cada ~312 microssegundos)
// =========================================================================
// Esta função é chamada automaticamente pelo hardware/sistema (esp_timer).
// A frequência de chamada será NPTS * F_HZ = 32 * 100 = 3200 Hz.
// ATENÇÃO: Callbacks de timer devem ser extremamente rápidos e não podem conter delays!
static void tick_cb(void *arg)               
{
    // Escreve o valor atual da tabela no DAC.
    // O DAC do ESP32 tem resolução de 8 bits, portanto aceita valores de 0 a 255.
    dac_oneshot_output_voltage(s_dac, s_tab[s_i]);
    
    // Incrementa o índice e faz com que ele volte a 0 quando chegar em NPTS (32)
    s_i = (s_i + 1) % NPTS;
}

void app_main(void)
{
    // =========================================================================
    // 1. PRÉ-COMPUTAÇÃO DA SENOIDE (Look-up Table)
    // =========================================================================
    // Calcular seno com ponto flutuante dentro do timer gastaria muito processamento.
    // Em sistemas embarcados, calculamos tudo antes (no boot) e guardamos num vetor.
    for (int i = 0; i < NPTS; i++) {
        // A função sin() retorna de -1.0 a +1.0. 
        // Como o DAC é de 8 bits (0 a 255), precisamos ajustar a escala (offset e amplitude).
        // 127.5 é o nível DC (centro). A variação será de 127.5 - 127.5 = 0 até 127.5 + 127.5 = 255.
        s_tab[i] = (uint8_t)(127.5 + 127.5 * sin(2 * M_PI * i / NPTS));
    }

    // =========================================================================
    // 2. CONFIGURAÇÃO DO DAC
    // =========================================================================
    dac_oneshot_config_t cfg = { 
        .chan_id = DAC_CHAN_0 // O Canal 0 do DAC no ESP32 corresponde fisicamente ao pino GPIO25
    };
    // Inicializa o canal do DAC com as configurações acima
    dac_oneshot_new_channel(&cfg, &s_dac);

    // =========================================================================
    // 3. CONFIGURAÇÃO DO TIMER DE ALTA RESOLUÇÃO
    // =========================================================================
    // Não usamos vTaskDelay aqui porque sua resolução padrão (Tick) geralmente 
    // é de 1ms ou 10ms (1000 Hz ou 100 Hz). Nós precisamos de algo muito mais 
    // rápido (3200 Hz). Por isso, usamos o esp_timer, que opera em microssegundos.
    
    const esp_timer_create_args_t targs = { 
        .callback = tick_cb,  // Aponta para a função que será chamada a cada estouro do timer
        .name = "dac"         // Nome amigável para debug
    };
    
    esp_timer_handle_t t;
    // Cria o timer e o associa ao handle 't'
    esp_timer_create(&targs, &t);
    
    // Inicia o timer em modo periódico. O argumento é o período em microssegundos.
    // Conta: 1.000.000 us (1 segundo) dividido pela taxa de atualização (3200 Hz)
    // 1000000 / 3200 = 312.5 us. Como a divisão é inteira, o timer rodará a cada 312 us.
    esp_timer_start_periodic(t, 1000000 / (NPTS * F_HZ));   
}