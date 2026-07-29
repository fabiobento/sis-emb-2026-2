// Lab Extra — Classificador de movimento (coleta)
// Deriva de semana-09/src/mpu6050: lê o acelerometro do MPU-6050 por I2C e imprime
// APENAS os 3 eixos (g) por linha, com periodo CRAVADO a 100 Hz (vTaskDelayUntil,
// semana 5). Esse formato "aX,aY,aZ" e o que o edge-impulse-data-forwarder espera.
//
// Uso:
//   idf.py flash monitor        (confira as linhas saindo a ~100 Hz)
//   feche o monitor e rode:  edge-impulse-data-forwarder --frequency 100
//   informe os nomes dos eixos: accX,accY,accZ
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <stdio.h>

#define SDA 21
#define SCL 22
#define ADDR 0x68            // AD0=GND
#define REG_PWR    0x6B
#define REG_ACCEL  0x3B      // ax..az nos primeiros 6 bytes
#define FS_HZ      100       // taxa de amostragem (Nyquist: > 2x a maior freq. util)

static esp_err_t wr(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_write_to_device(I2C_NUM_0, ADDR, b, 2, pdMS_TO_TICKS(50));
}
static esp_err_t rd(uint8_t reg, uint8_t *dst, size_t n)
{
    return i2c_master_write_read_device(I2C_NUM_0, ADDR, &reg, 1, dst, n,
                                        pdMS_TO_TICKS(50));
}

void app_main(void)
{
    i2c_config_t cfg = { .mode = I2C_MODE_MASTER, .sda_io_num = SDA,
        .scl_io_num = SCL, .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE, .master.clk_speed = 400000 };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    wr(REG_PWR, 0x00);                        // sai do sleep

    const TickType_t periodo = pdMS_TO_TICKS(1000 / FS_HZ);
    TickType_t prox = xTaskGetTickCount();    // referencia absoluta (semana 5)

    while (1) {
        uint8_t b[6];
        rd(REG_ACCEL, b, 6);
        int16_t ax = (b[0]<<8)|b[1], ay = (b[2]<<8)|b[3], az = (b[4]<<8)|b[5];
        // +-2 g de fundo de escala -> 16384 LSB/g
        printf("%.4f,%.4f,%.4f\n", ax/16384.f, ay/16384.f, az/16384.f);
        vTaskDelayUntil(&prox, periodo);      // periodo cravado: nada de jitter no espectro
    }
}
