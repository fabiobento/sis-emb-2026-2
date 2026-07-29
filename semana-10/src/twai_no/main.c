// Semana 10 — no CAN com TWAI: selftest (1 placa) ou rede sensor/atuador (2 placas)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/twai.h"
#include <stdio.h>

#define MODO_SELFTEST 1      // 1: uma placa (NO_ACK) | 0: rede real
#define PAPEL_SENSOR  1      // na rede: 1 = envia T (ID 0x0A0); 0 = recebe/comanda (ID 0x120)
#define TX_PIN 21
#define RX_PIN 22

void app_main(void)
{
#if MODO_SELFTEST
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NO_ACK);
#else
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
#endif
    twai_timing_config_t tcfg = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    twai_driver_install(&g, &tcfg, &f);
    twai_start();
    printf("TWAI iniciado @500 kbit/s\n");

    float temp = 25.0f;
    uint32_t tx_ok = 0, rx_ok = 0;
    while (1) {
#if MODO_SELFTEST || PAPEL_SENSOR
        temp += 0.5f; if (temp > 40) temp = 25;
        twai_message_t m = { .identifier = 0x0A0, .data_length_code = 4 };
#if MODO_SELFTEST
        m.self = 1;                                  // recebe a propria msg
#endif
        int t10 = (int)(temp * 10);
        m.data[0] = t10 & 0xFF; m.data[1] = t10 >> 8;
        if (twai_transmit(&m, pdMS_TO_TICKS(100)) == ESP_OK) tx_ok++;
#endif
        twai_message_t r;
        while (twai_receive(&r, 0) == ESP_OK) {
            rx_ok++;
            if (r.identifier == 0x0A0) {
                float t = ((r.data[1] << 8) | r.data[0]) / 10.0f;
                printf("RX ID=0x%03lX T=%.1f C  (tx=%lu rx=%lu)\n",
                       (unsigned long)r.identifier, t,
                       (unsigned long)tx_ok, (unsigned long)rx_ok);
#if !MODO_SELFTEST && !PAPEL_SENSOR
                if (t > 30.0f) {                     // comando de maior prioridade? NAO:
                    twai_message_t cmd = { .identifier = 0x120,  // 0x120 > 0x0A0 => menor prioridade
                                           .data_length_code = 1 };
                    cmd.data[0] = 1;                 // "ligar ventilador"
                    twai_transmit(&cmd, pdMS_TO_TICKS(100));
                }
#endif
            } else if (r.identifier == 0x120) {
                printf("RX comando ventilador=%d\n", r.data[0]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
