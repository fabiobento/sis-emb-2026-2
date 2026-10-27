// Semana 9 — MPU-6050 via I2C: WHO_AM_I + leitura de acel/giro + angulo
// -----------------------------------------------------------------------------
// PRÁTICA: Comunicação I2C com o sensor inercial (IMU) MPU-6050.
// OBJETIVO: Configurar o I2C, acordar o sensor, ler os dados brutos (burst read)
//           de aceleração e rotação, converter para unidades físicas (g e graus/s)
//           e calcular a inclinação estática usando trigonometria básica.
// -----------------------------------------------------------------------------

#include "freertos/FreeRTOS.h"  // Core do RTOS
#include "freertos/task.h"      // Controle de tarefas (vTaskDelay)
#include "driver/i2c.h"         // Driver I2C do ESP-IDF
#include <stdio.h>              // Funções de I/O (printf)
#include <math.h>               // Funções matemáticas (atan2f, M_PI)

// =========================================================================
// DEFINIÇÕES E MAPA DE REGISTRADORES
// =========================================================================
#define SDA 21               // Pino de dados do barramento I2C
#define SCL 22               // Pino de clock do barramento I2C

#define ADDR 0x68            // Endereço I2C do MPU-6050 (quando o pino AD0 está no GND)
                             // Se AD0 estivesse no VCC, o endereço seria 0x69.

// Registradores internos do MPU-6050 (consultados no datasheet)
#define REG_PWR    0x6B      // Power Management 1: controla o modo de energia (sleep)
#define REG_WHOAMI 0x75      // Who Am I: contém a identidade do chip (deve retornar 0x68)
#define REG_ACCEL  0x3B      // Primeiro registrador de dados (Accel X High Byte)
                             // A partir daqui, são 14 bytes sequenciais: 
                             // ax (2), ay (2), az (2), temp (2), gx (2), gy (2), gz (2)

// =========================================================================
// FUNÇÕES AUXILIARES DE COMUNICAÇÃO I2C (Wrappers)
// =========================================================================
// O ESP-IDF possui funções de alto nível que simplificam a escrita/leitura.

// Escreve 1 byte (val) em um registrador específico (reg) do sensor
static esp_err_t wr(uint8_t reg, uint8_t val)
{
    // O pacote I2C para escrita precisa mandar o endereço do registrador seguido do valor
    uint8_t b[2] = { reg, val };
    
    // Função padrão do IDF v4.x+: Manda o buffer para o endereço do dispositivo (ADDR)
    return i2c_master_write_to_device(I2C_NUM_0, ADDR, b, 2, pdMS_TO_TICKS(50));
}

// Lê 'n' bytes a partir de um registrador inicial (reg) e guarda no buffer 'dst'
static esp_err_t rd(uint8_t reg, uint8_t *dst, size_t n)
{
    // Envia o endereço do registrador que queremos ler e já engata um START repetido
    // para receber os 'n' bytes em seguida, sem soltar o barramento.
    return i2c_master_write_read_device(I2C_NUM_0, ADDR, &reg, 1, dst, n,
                                        pdMS_TO_TICKS(50));
}

void app_main(void)
{
    // =========================================================================
    // 1. CONFIGURAÇÃO DO HARDWARE I2C
    // =========================================================================
    i2c_config_t cfg = { 
        .mode = I2C_MODE_MASTER, 
        .sda_io_num = SDA,
        .scl_io_num = SCL, 
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE, 
        .master.clk_speed = 400000 // Velocidade: 400 kHz (Fast Mode). 
                                   // O MPU-6050 suporta essa velocidade superior aos 100kHz padrão.
    };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    // =========================================================================
    // 2. INICIALIZAÇÃO DO SENSOR
    // =========================================================================
    uint8_t who = 0;
    
    // Lê o registrador de identidade. É uma boa prática para verificar se a 
    // solda e os fios estão OK antes de tentar configurar o chip.
    rd(REG_WHOAMI, &who, 1);
    printf("WHO_AM_I = 0x%02X (esperado 0x68)\n", who);
    
    // IMPORTANTE: O MPU-6050 liga em modo SLEEP por padrão para economizar bateria.
    // Escrever 0x00 no REG_PWR acorda o sensor e o coloca para usar o oscilador interno.
    wr(REG_PWR, 0x00);                       

    // =========================================================================
    // 3. LOOP PRINCIPAL DE LEITURA
    // =========================================================================
    while (1) {
        uint8_t b[14];
        
        // BURST READ: Lê os 14 bytes de dados de uma só vez.
        // É muito mais eficiente no barramento I2C ler tudo num pacote sequencial
        // do que fazer 14 requisições separadas.
        rd(REG_ACCEL, b, 14);
        
        // RECONSTRUÇÃO DOS DADOS (O MPU-6050 é Big-Endian)
        // Cada grandeza tem 16 bits (2 bytes). O byte mais significativo (MSB) vem primeiro.
        // Deslocamos o MSB 8 posições para a esquerda e fazemos uma operação OR (ou soma) com o LSB.
        int16_t ax = (b[0]<<8) | b[1];
        int16_t ay = (b[2]<<8) | b[3];
        int16_t az = (b[4]<<8) | b[5];
        // b[6] e b[7] seriam a temperatura. Pulamos direto para o Giroscópio (X):
        int16_t gx = (b[8]<<8) | b[9];
        
        // CONVERSÃO PARA UNIDADES FÍSICAS
        // fundo de escala padrao do chip (power-on reset): 
        // Acelerômetro: +-2 g -> Logo, 1 'g' equivale a 16384 LSB.
        // Giroscópio: +-250 dps (graus por segundo) -> Logo, 1 grau/s equivale a 131 LSB.
        float axg = ax / 16384.f;
        float ayg = ay / 16384.f;
        float azg = az / 16384.f;
        
        // CÁLCULO DE INCLINAÇÃO (TILT)
        // Usa a função arco-tangente de 2 argumentos (atan2) para descobrir o ângulo 
        // em relação à gravidade no plano XZ. atan2f resolve o problema do quadrante.
        // O resultado sai em radianos, então multiplicamos por (180/PI) para converter para graus.
        float theta = atan2f(axg, azg) * 180.f / M_PI;   
        
        // Imprime os valores físicos e o ângulo
        printf("a=(%.2f,%.2f,%.2f) g  gx=%.1f o/s  theta=%.1f o\n",
               axg, ayg, azg, gx/131.f, theta);
               
        // Atraso de 100ms (~10 Hz de atualização na tela)
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // NOTA PEDAGÓGICA PARA O ROTEIRO DO LABORATÓRIO:
        // O display OLED entra na Parte C (src/inclinometro_oled): mesmas wr()/rd(), endereco 0x3C.
        // Como o I2C é um barramento, o OLED usará os mesmos fios SDA/SCL, bastando 
        // endereçar os pacotes para 0x3C em vez de 0x68.
    }
}