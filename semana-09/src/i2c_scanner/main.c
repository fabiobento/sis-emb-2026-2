// Semana 9 — scanner I2C: varre 0x03-0x77 e lista quem responde ACK
// -----------------------------------------------------------------------------
// PRÁTICA: O barramento I2C permite ligar dezenas de sensores usando apenas 2 fios.
// OBJETIVO: Este código é uma ferramenta de diagnóstico. Ele tenta se comunicar 
//           com todos os endereços I2C possíveis. Se algum componente físico estiver
//           conectado corretamente e funcionando, ele responderá com um sinal de 
//           reconhecimento (ACK - Acknowledge) e o scanner imprimirá seu endereço.
// -----------------------------------------------------------------------------

#include "freertos/FreeRTOS.h"  // Core do sistema operacional de tempo real
#include "freertos/task.h"      // Necessário para as macros de tempo (pdMS_TO_TICKS)
#include "driver/i2c.h"         // Driver padrão de I2C do ESP-IDF
#include <stdio.h>              // Funções de entrada e saída (printf)

// Define os pinos padrão do I2C no ESP32 genérico
// DICA: No ESP32, você pode rotear o I2C para praticamente qualquer pino digital.
#define SDA 21                  // Serial Data (Linha de Dados)
#define SCL 22                  // Serial Clock (Linha de Clock)

void app_main(void)
{
    // =========================================================================
    // 1. CONFIGURAÇÃO DO HARDWARE I2C (Modo Master)
    // =========================================================================
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,             // O ESP32 será o Mestre (quem gera o clock)
        .sda_io_num = SDA,                   // Pino de dados
        .scl_io_num = SCL,                   // Pino de clock
        .sda_pullup_en = GPIO_PULLUP_ENABLE, // Habilita o resistor de pull-up interno do ESP32 para o SDA
        .scl_pullup_en = GPIO_PULLUP_ENABLE, // Habilita o resistor de pull-up interno do ESP32 para o SCL
        .master.clk_speed = 100000           // Frequência do clock: 100 kHz (Standard Mode)
    };
    
    // Aplica as configurações ao controlador I2C número 0 (o ESP32 possui dois: I2C_NUM_0 e I2C_NUM_1)
    i2c_param_config(I2C_NUM_0, &cfg);
    
    // Instala o driver no sistema operacional alocando recursos (sem buffers para o Master)
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    printf("varredura I2C...\n");
    
    // =========================================================================
    // 2. LOOP DE VARREDURA DE ENDEREÇOS
    // =========================================================================
    // O I2C padrão usa endereços de 7 bits (0x00 a 0x7F).
    // Pulamos os endereços 0x00-0x02 e 0x78-0x7F porque, no protocolo I2C, 
    // eles são reservados para funções especiais (ex: General Call, 10-bit addressing).
    for (uint8_t a = 0x03; a <= 0x77; a++) {
        
        // No ESP-IDF, as operações I2C são montadas em uma "fila de comandos" (link)
        // antes de serem enviadas efetivamente para o hardware.
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        
        // Adiciona à fila o bit de START (início da comunicação)
        i2c_master_start(cmd);
        
        // Adiciona à fila o byte de endereço. 
        // MATEMÁTICA DO I2C: O endereço real tem 7 bits. Ele deve ser deslocado 1 casa para a esquerda (a << 1).
        // O bit menos significativo (LSB) que sobra indica a operação: 0 para Write, 1 para Read.
        // O 'true' no terceiro argumento significa que exigimos que o escravo responda com um bit de ACK.
        i2c_master_write_byte(cmd, (a << 1) | I2C_MASTER_WRITE, true);
        
        // Adiciona à fila o bit de STOP (fim da comunicação)
        i2c_master_stop(cmd);
        
        // =========================================================================
        // 3. EXECUÇÃO DA FILA DE COMANDOS
        // =========================================================================
        // Dispara a comunicação fisicamente no barramento e aguarda até 20ms pela resposta
        esp_err_t r = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(20));
        
        // Deleta a fila de comandos para liberar a memória alocada, prevenindo memory leak
        i2c_cmd_link_delete(cmd);
        
        // =========================================================================
        // 4. ANÁLISE DO RESULTADO
        // =========================================================================
        // Se a função retornou ESP_OK, significa que ocorreu um ACK! 
        // Ou seja, existe um dispositivo físico nesse endereço que escutou o chamado e respondeu.
        if (r == ESP_OK) {
            // Imprime o endereço em formato hexadecimal (ex: 0x27, 0x68)
            printf("  dispositivo em 0x%02X\n", a);
        }
    }
    printf("fim.\n");
}