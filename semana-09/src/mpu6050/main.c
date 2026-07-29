// Semana 9 — MPU-6050 via I2C: WHO_AM_I + leitura de acel/giro + angulo
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <stdio.h>
#include <math.h>

#define SDA 21
#define SCL 22
#define ADDR 0x68            // AD0=GND
#define REG_PWR    0x6B
#define REG_WHOAMI 0x75
#define REG_ACCEL  0x3B      // 14 bytes: ax..az, temp, gx..gz

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

    uint8_t who = 0;
    rd(REG_WHOAMI, &who, 1);
    printf("WHO_AM_I = 0x%02X (esperado 0x68)\n", who);
    wr(REG_PWR, 0x00);                       // sai do sleep

    while (1) {
        uint8_t b[14];
        rd(REG_ACCEL, b, 14);
        int16_t ax = (b[0]<<8)|b[1], ay = (b[2]<<8)|b[3], az = (b[4]<<8)|b[5];
        int16_t gx = (b[8]<<8)|b[9];
        // fundo de escala padrao: +-2 g -> 16384 LSB/g ; +-250 dps -> 131 LSB/(o/s)
        float axg = ax/16384.f, ayg = ay/16384.f, azg = az/16384.f;
        float theta = atan2f(axg, azg) * 180.f / M_PI;   // inclinacao no plano XZ
        printf("a=(%.2f,%.2f,%.2f) g  gx=%.1f o/s  theta=%.1f o\n",
               axg, ayg, azg, gx/131.f, theta);
        vTaskDelay(pdMS_TO_TICKS(100));
        // EXTRA: enviar p/ SSD1306 — instale o componente ssd1306 (idf.py add-dependency)
        // e escreva theta no display. Estrutura identica de wr()/rd() no endereco 0x3C.
    }
}
