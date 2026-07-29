// Semana 9 — scanner I2C: varre 0x03-0x77 e lista quem responde ACK
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <stdio.h>

#define SDA 21
#define SCL 22

void app_main(void)
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER, .sda_io_num = SDA, .scl_io_num = SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000 };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    printf("varredura I2C...\n");
    for (uint8_t a = 0x03; a <= 0x77; a++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (a << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t r = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(20));
        i2c_cmd_link_delete(cmd);
        if (r == ESP_OK) printf("  dispositivo em 0x%02X\n", a);
    }
    printf("fim.\n");
}
