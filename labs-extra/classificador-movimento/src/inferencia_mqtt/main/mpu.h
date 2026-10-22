// mpu.h — leitura do MPU-6050 (identica ao firmware de coleta). Semana 9: wr()/rd().
#ifndef MPU_H
#define MPU_H
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "projeto_config.h"

#define REG_DLPF   0x1A
#define REG_ACFG   0x1C
#define REG_ACCEL  0x3B
#define REG_PWR    0x6B
#define REG_WHOAMI 0x75
#define LSB_POR_G  (16384.0f / (1 << MPU_RANGE))

static inline esp_err_t mpu_wr(uint8_t reg, uint8_t val)
{ uint8_t b[2] = { reg, val };
  return i2c_master_write_to_device(I2C_NUM_0, MPU_ADDR, b, 2, pdMS_TO_TICKS(50)); }

static inline esp_err_t mpu_rd(uint8_t reg, uint8_t *dst, size_t n)
{ return i2c_master_write_read_device(I2C_NUM_0, MPU_ADDR, &reg, 1, dst, n, pdMS_TO_TICKS(50)); }

static inline uint8_t mpu_init(void)
{
    i2c_config_t cfg = {}; cfg.mode = I2C_MODE_MASTER; cfg.sda_io_num = PIN_SDA; cfg.scl_io_num = PIN_SCL;
    cfg.sda_pullup_en = GPIO_PULLUP_ENABLE; cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.master.clk_speed = 400000;
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    uint8_t who = 0; mpu_rd(REG_WHOAMI, &who, 1);
    mpu_wr(REG_PWR, 0x00);
    mpu_wr(REG_DLPF, MPU_DLPF);
    mpu_wr(REG_ACFG, MPU_RANGE << 3);
    return who;
}

// Le (ax,ay,az) em g. Retorna ESP_OK ou o erro do I2C (a amostra anterior fica em a[]).
static inline esp_err_t mpu_le_g(float a[3])
{
    uint8_t b[6];
    esp_err_t e = mpu_rd(REG_ACCEL, b, 6);
    if (e != ESP_OK) return e;
    a[0] = (int16_t)((b[0] << 8) | b[1]) / LSB_POR_G;
    a[1] = (int16_t)((b[2] << 8) | b[3]) / LSB_POR_G;
    a[2] = (int16_t)((b[4] << 8) | b[5]) / LSB_POR_G;
    return ESP_OK;
}
#endif
