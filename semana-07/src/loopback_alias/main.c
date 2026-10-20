// Semana 7E — loopback DAC -> ADC na MESMA placa: ver o aliasing acontecer
// Jumper: GPIO25 (saida do DAC) ligado ao GPIO34 (entrada do ADC).
// O DAC gera uma senoide de F_HZ; o ADC a amostra a FS_HZ. Mude F_HZ e compare
// o que o firmware PREVE com o que o ADC realmente enxerga (teoria, secao 3).
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include <math.h>
#include <stdio.h>

#define NPTS  32
#define F_HZ  2          // frequencia gerada pelo DAC (tente 2, 8, 95 e 100)
#define FS_HZ 20         // taxa de amostragem do ADC (frequencia de Nyquist = 10 Hz)

static dac_oneshot_handle_t s_dac;
static uint8_t s_tab[NPTS];
static volatile int s_i = 0;

static void tick_cb(void *arg)               // atualiza o DAC a NPTS*F_HZ
{
    dac_oneshot_output_voltage(s_dac, s_tab[s_i]);
    s_i = (s_i + 1) % NPTS;
}

void app_main(void)
{
    for (int i = 0; i < NPTS; i++)           // amplitude 100 (e nao 127,5): fica em ~0,35-2,9 V,
        s_tab[i] = (uint8_t)(127.5 + 100.0 * sin(2 * M_PI * i / NPTS));   // abaixo do teto do ADC

    dac_oneshot_config_t dcfg = { .chan_id = DAC_CHAN_0 };      // GPIO25
    dac_oneshot_new_channel(&dcfg, &s_dac);

    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&ucfg, &adc);
    adc_oneshot_chan_cfg_t ccfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_11 };
    adc_oneshot_config_channel(adc, ADC_CHANNEL_6, &ccfg);      // GPIO34

    // o periodo do timer e inteiro em us, entao a frequencia REAL difere um pouco da nominal
    int per_us = 1000000 / (NPTS * F_HZ);
    double f_real = 1e6 / per_us / NPTS;
    double k = round(f_real / FS_HZ);
    double f_alias = fabs(f_real - k * FS_HZ);               // formula da teoria, secao 3.3
    printf("DAC gera %.2f Hz | ADC amostra a %d Hz (Nyquist = %.1f Hz) | alias previsto = %.2f Hz\n",
           f_real, FS_HZ, FS_HZ / 2.0, f_alias);

    const esp_timer_create_args_t targs = { .callback = tick_cb, .name = "dac" };
    esp_timer_handle_t t;
    esp_timer_create(&targs, &t);
    esp_timer_start_periodic(t, per_us);

    TickType_t prox = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(1000 / FS_HZ));   // periodo cravado (teoria, secao 3.5)
        int raw;
        adc_oneshot_read(adc, ADC_CHANNEL_6, &raw);
        printf("%d\n", raw);                                   // um numero por linha: serve ao Serial Plotter
    }
}
