// Semana 7 — ADC (LDR em divisor) + media movel O(1) + estatistica simples
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include <stdio.h>
#include <math.h>

#define M 8                      // janela da media movel (1 = sem filtro)
#define FS_HZ 20                 // taxa de amostragem

void app_main(void)
{
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&ucfg, &adc);
    adc_oneshot_chan_cfg_t ccfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_11,          // faixa ~0-3.1 V
    };
    adc_oneshot_config_channel(adc, ADC_CHANNEL_6, &ccfg);   // GPIO34

    int buf[M] = {0}; int idx = 0; long soma = 0;
    double s1 = 0, s2 = 0; int n = 0;
    TickType_t prox = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(1000 / FS_HZ)); // periodo cravado
        int raw;
        adc_oneshot_read(adc, ADC_CHANNEL_6, &raw);

        soma += raw - buf[idx];              // media movel O(1): soma corrente
        buf[idx] = raw;
        idx = (idx + 1) % M;
        int filtrado = soma / M;

        s1 += filtrado; s2 += (double)filtrado * filtrado; n++;
        if (n == FS_HZ) {                    // estatistica a cada 1 s
            double media = s1 / n;
            double dp = sqrt(s2 / n - media * media);
            printf("raw=%4d  filt=%4d  V=%.3f  media_1s=%.1f  desvio=%.1f\n",
                   raw, filtrado, filtrado * 3.1 / 4095.0, media, dp);
            s1 = s2 = 0; n = 0;
        }
    }
}
