// Lab Extra — Classificador de movimento (ETAPA DE COLETA)
// Deriva de semana-09/src/mpu6050. Le o acelerometro do MPU-6050 por I2C e imprime
// APENAS os 3 eixos (em g), uma linha "aX,aY,aZ" por amostra, com periodo CRAVADO
// (vTaskDelayUntil, semana 5). Esse e o formato que o edge-impulse-data-forwarder
// e o tools/gravar_csv.py esperam.
//
// REGRA DE OURO: estas configuracoes (taxa, fundo de escala, filtro DLPF, ordem e
// unidade dos eixos) tem de ser IDENTICAS no firmware de inferencia. Se mudar aqui,
// mude la — senao o modelo recebe "outra coisa" (erro n. 1 de integracao).
//
// Uso:
//   idf.py -p /dev/ttyUSB0 flash monitor     (confira ~100 linhas/s; sai com Ctrl+])
//   feche o monitor e rode:  edge-impulse-data-forwarder --frequency 100
//   nomes dos eixos: accX,accY,accZ
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include <stdio.h>

// ---------------- configuracao (manter igual ao firmware de inferencia) ----------------
#define SDA        21
#define SCL        22
#define ADDR       0x68      // AD0 = GND
#define FS_HZ      100       // Nyquist: > 2x a maior freq. util do gesto (~10-15 Hz)
#define MPU_RANGE  0         // 0:+-2g  1:+-4g  2:+-8g  3:+-16g  (se saturar em +-2,0 g, suba)
#define MPU_DLPF   3         // 3 => banda do acelerometro ~44 Hz (anti-aliasing p/ fs=100 Hz)
// ----------------------------------------------------------------------------------------
#define REG_DLPF    0x1A     // CONFIG: DLPF_CFG nos 3 bits baixos
#define REG_ACFG    0x1C     // ACCEL_CONFIG: AFS_SEL nos bits 4:3
#define REG_ACCEL   0x3B     // ax..az nos 6 primeiros bytes (big-endian)
#define REG_PWR     0x6B
#define REG_WHOAMI  0x75
#define LSB_POR_G   (16384.0f / (1 << MPU_RANGE))

static esp_err_t wr(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_write_to_device(I2C_NUM_0, ADDR, b, 2, pdMS_TO_TICKS(50));
}
static esp_err_t rd(uint8_t reg, uint8_t *dst, size_t n)
{
    return i2c_master_write_read_device(I2C_NUM_0, ADDR, &reg, 1, dst, n, pdMS_TO_TICKS(50));
}

void app_main(void)
{
    i2c_config_t cfg = { .mode = I2C_MODE_MASTER, .sda_io_num = SDA, .scl_io_num = SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000 };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    uint8_t who = 0;
    rd(REG_WHOAMI, &who, 1);
    // Linhas que NAO sao "num,num,num" sao ignoradas pelo forwarder; servem de diagnostico.
    printf("# WHO_AM_I=0x%02X (esperado 0x68) fs=%d Hz range=%d dlpf=%d\n", who, FS_HZ, MPU_RANGE, MPU_DLPF);

    wr(REG_PWR, 0x00);                          // sai do sleep (oscilador interno)
    wr(REG_DLPF, MPU_DLPF);                     // filtro passa-baixas interno (semana 7: anti-aliasing)
    wr(REG_ACFG, MPU_RANGE << 3);               // fundo de escala

    const TickType_t periodo = pdMS_TO_TICKS(1000 / FS_HZ);   // 10 ticks (CONFIG_FREERTOS_HZ=1000)
    TickType_t prox = xTaskGetTickCount();      // referencia absoluta: sem deriva acumulada
    float ult[3] = { 0, 0, 1 };                 // ultima amostra boa (repetida se o I2C falhar)

    while (1) {
        uint8_t b[6];
        if (rd(REG_ACCEL, b, 6) == ESP_OK) {
            ult[0] = (int16_t)((b[0] << 8) | b[1]) / LSB_POR_G;
            ult[1] = (int16_t)((b[2] << 8) | b[3]) / LSB_POR_G;
            ult[2] = (int16_t)((b[4] << 8) | b[5]) / LSB_POR_G;
        }                                       // se falhar: repete a anterior e mantem a taxa
        printf("%.4f,%.4f,%.4f\n", ult[0], ult[1], ult[2]);
        vTaskDelayUntil(&prox, periodo);
    }
}
