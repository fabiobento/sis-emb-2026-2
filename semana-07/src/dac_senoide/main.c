// Semana 7 — DAC: senoide de 100 Hz com 32 pontos/ciclo (GPIO25)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_timer.h"
#include <math.h>

#define NPTS 32
#define F_HZ 100

static dac_oneshot_handle_t s_dac;
static uint8_t s_tab[NPTS];
static volatile int s_i = 0;

static void tick_cb(void *arg)               // chamado a NPTS*F_HZ = 3200 Hz
{
    dac_oneshot_output_voltage(s_dac, s_tab[s_i]);
    s_i = (s_i + 1) % NPTS;
}

void app_main(void)
{
    for (int i = 0; i < NPTS; i++)           // tabela pre-computada (economiza CPU)
        s_tab[i] = (uint8_t)(127.5 + 127.5 * sin(2 * M_PI * i / NPTS));

    dac_oneshot_config_t cfg = { .chan_id = DAC_CHAN_0 };   // GPIO25
    dac_oneshot_new_channel(&cfg, &s_dac);

    const esp_timer_create_args_t targs = { .callback = tick_cb, .name = "dac" };
    esp_timer_handle_t t;
    esp_timer_create(&targs, &t);
    esp_timer_start_periodic(t, 1000000 / (NPTS * F_HZ));   // ~312 us
}
