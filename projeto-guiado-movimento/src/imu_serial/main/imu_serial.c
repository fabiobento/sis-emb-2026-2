// =============================================================================
// Projeto guiado — Reconhecimento de movimentos contínuos (Etapa 2)
// Firmware de SERIALIZAÇÃO do acelerômetro MPU-6050 para o Edge Impulse
// -----------------------------------------------------------------------------
// O QUE FAZ: lê os 3 eixos do acelerômetro por I2C (semana 9) a uma taxa
//            CRAVADA de 62,5 Hz (período de 16 ms) e imprime cada amostra na
//            serial (UART0 → USB → PC) numa única linha "accX,accY,accZ",
//            em m/s², sem nenhum texto extra.
//
// POR QUE ESSE FORMATO: é o protocolo do edge-impulse-data-forwarder —
//   115200 bps, uma leitura por linha, valores separados por vírgula ou TAB.
//   Os mesmos dados alimentam os scripts Python das Etapas 3 e 4.
//
// POR QUE esp_timer E NÃO vTaskDelayUntil: o tick do FreeRTOS no ESP-IDF é de
//   100 Hz (resolução de 10 ms, semana 5). 16 ms não é múltiplo de 10 ms:
//   pdMS_TO_TICKS(16) vira 1 tick = 10 ms (!). O esp_timer (semana 4) conta em
//   µs, então 16 000 µs são 16 ms de verdade.
//
// ARQUITETURA (semanas 4 e 6):
//   esp_timer (16 ms) --xSemaphoreGive--> tarefa_amostragem --I2C--> MPU-6050
//                                                         \--printf--> UART0
//   O callback do esp_timer roda numa tarefa de alta prioridade compartilhada:
//   ele só "toca a campainha" (semáforo binário). O trabalho lento (I2C e
//   printf) fica na nossa tarefa.
//
// REGRA DE OURO (o "contrato"): taxa, fundo de escala, filtro, ordem e unidade
//   dos eixos usados aqui valem para TODO o projeto. O modelo treinado no
//   Studio só entende dados produzidos exatamente assim.
// =============================================================================

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2c.h"
#include "esp_timer.h"

// ------------------------- configuração (o contrato) -------------------------
#define PIN_SDA         21          // mesma ligação do Lab 9
#define PIN_SCL         22
#define MPU_ADDR        0x68        // AD0 em GND
#define FS_HZ           62.5f       // taxa de amostragem (igual à do projeto de referência)
#define PERIODO_US      16000       // 1 / 62,5 Hz = 16 ms = 16 000 µs
#define MPU_RANGE       1           // AFS_SEL: 0:±2g 1:±4g 2:±8g 3:±16g
#define MPU_DLPF        4           // DLPF_CFG=4 → banda do acelerômetro ≈ 21 Hz
                                    // (anti-aliasing: Nyquist = 62,5/2 = 31,25 Hz)
#define G_PARA_MS2      9.80665f    // 1 g em m/s²
// -----------------------------------------------------------------------------

// Registradores do MPU-6050 (Register Map rev. 4.2)
#define REG_CONFIG      0x1A        // DLPF_CFG nos bits 2:0
#define REG_ACCEL_CFG   0x1C        // AFS_SEL nos bits 4:3
#define REG_ACCEL_XOUT  0x3B        // AX_H, AX_L, AY_H, AY_L, AZ_H, AZ_L (big-endian)
#define REG_PWR_MGMT_1  0x6B        // bit 6 = SLEEP
#define REG_WHO_AM_I    0x75        // deve responder 0x68

// Sensibilidade: 16384 LSB/g em ±2g; cada degrau de AFS_SEL divide por 2.
#define LSB_POR_G       (16384.0f / (float)(1 << MPU_RANGE))

static SemaphoreHandle_t s_sem_amostra;     // "campainha" do timer → tarefa

// ---------------------------- I2C (igual ao Lab 9) ---------------------------
static esp_err_t mpu_escreve(uint8_t reg, uint8_t valor)
{
    uint8_t buf[2] = { reg, valor };
    return i2c_master_write_to_device(I2C_NUM_0, MPU_ADDR, buf, 2, pdMS_TO_TICKS(50));
}

static esp_err_t mpu_le(uint8_t reg, uint8_t *dst, size_t n)
{
    // escreve o endereço do registrador e, com START repetido, lê n bytes
    return i2c_master_write_read_device(I2C_NUM_0, MPU_ADDR, &reg, 1, dst, n,
                                        pdMS_TO_TICKS(50));
}

static void i2c_inicia(void)
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,   // o GY-521 já tem pull-ups de 4,7 kΩ
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,            // Fast Mode: 6 bytes em ~0,3 ms
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &cfg));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
}

// ------------------------------ callback do timer ----------------------------
// Roda a cada 16 ms na tarefa do esp_timer. Regra das ISRs (semana 4):
// curto, sem I2C, sem printf — só sinaliza.
static void timer_cb(void *arg)
{
    xSemaphoreGive(s_sem_amostra);
}

// --------------------------- tarefa de amostragem ---------------------------
static void tarefa_amostragem(void *arg)
{
    uint8_t b[6];
    float ult[3] = { 0.0f, 0.0f, G_PARA_MS2 };  // última amostra válida

    while (1) {
        // dorme até o timer tocar a campainha (não gasta CPU esperando)
        xSemaphoreTake(s_sem_amostra, portMAX_DELAY);

        if (mpu_le(REG_ACCEL_XOUT, b, 6) == ESP_OK) {
            // junta MSB e LSB em int16 (complemento de 2) e converte para m/s²
            ult[0] = (int16_t)((b[0] << 8) | b[1]) / LSB_POR_G * G_PARA_MS2;
            ult[1] = (int16_t)((b[2] << 8) | b[3]) / LSB_POR_G * G_PARA_MS2;
            ult[2] = (int16_t)((b[4] << 8) | b[5]) / LSB_POR_G * G_PARA_MS2;
        }
        // Se o I2C falhar, repetimos a última amostra: preferimos um valor
        // repetido a "furar" a taxa (amostras igualmente espaçadas, semana 7).

        // UMA linha, só números e vírgulas. ~24 caracteres × 62,5 Hz ≈ 1,5 kB/s
        // (≈ 15 kbit/s), bem abaixo dos 115 200 bit/s da UART.
        printf("%.4f,%.4f,%.4f\n", ult[0], ult[1], ult[2]);
    }
}

void app_main(void)
{
    i2c_inicia();

    // 1) Identidade: sem MPU-6050 não há o que serializar.
    //    Linhas que começam com "ERR" são exibidas pelo data forwarder como erro.
    uint8_t who = 0;
    while (mpu_le(REG_WHO_AM_I, &who, 1) != ESP_OK || who != 0x68) {
        printf("ERR: MPU-6050 nao respondeu (WHO_AM_I=0x%02X). Confira a fiacao.\n", who);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 2) Configuração: acorda, filtro anti-aliasing e fundo de escala
    mpu_escreve(REG_PWR_MGMT_1, 0x00);              // sai do modo SLEEP
    mpu_escreve(REG_CONFIG, MPU_DLPF);              // filtro passa-baixas interno
    mpu_escreve(REG_ACCEL_CFG, MPU_RANGE << 3);     // ±4 g
    vTaskDelay(pdMS_TO_TICKS(50));                  // deixa o filtro assentar

    // 3) Sincronização: semáforo binário (semana 6) + tarefa consumidora
    s_sem_amostra = xSemaphoreCreateBinary();
    xTaskCreate(tarefa_amostragem, "amostragem", 4096, NULL, 5, NULL);

    // 4) Relógio da amostragem: esp_timer periódico de 16 ms (semana 4)
    const esp_timer_create_args_t args = { .callback = timer_cb, .name = "amostra" };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, PERIODO_US));
    // app_main retorna; o timer e a tarefa continuam rodando.
}
