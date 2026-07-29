// Semana 13 — malha de luminosidade: LDR (ADC GPIO34) + LED (LEDC GPIO2) + PID 50 Hz
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/ledc.h"
#include <stdio.h>

#define MODO_PID    1        // 0: malha aberta (degraus de duty p/ identificacao)
#define ANTI_WINDUP 1
#define TS_MS       20       // 50 Hz  (Exemplo 13.1)
#define REF         2000     // alvo em contagens do ADC (0..4095)

static float Kp = 0.8f, Ki = 2.0f, Kd = 0.01f;   // sintonia inicial (Exemplo 13.4)

static adc_oneshot_unit_handle_t s_adc;

static int le_ldr(void)
{
    int raw; adc_oneshot_read(s_adc, ADC_CHANNEL_6, &raw); return raw;
}
static void aplica_duty(int d)                    // 0..4095 (12 bits)
{
    if (d < 0) d = 0; if (d > 4095) d = 4095;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, d);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void app_main(void)
{
    adc_oneshot_unit_init_cfg_t u = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&u, &s_adc);
    adc_oneshot_chan_cfg_t cc = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_11 };
    adc_oneshot_config_channel(s_adc, ADC_CHANNEL_6, &cc);

    ledc_timer_config_t t = { .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0, .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = 5000, .clk_cfg = LEDC_AUTO_CLK };
    ledc_timer_config(&t);
    ledc_channel_config_t c = { .gpio_num = 2, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0, .duty = 0 };
    ledc_channel_config(&c);

    printf("t_ms,ref,y,u\n");                     // cabecalho CSV
    TickType_t prox = xTaskGetTickCount();
    float I = 0, e_ant = 0;
    int64_t t0 = xTaskGetTickCount() * portTICK_PERIOD_MS;

#if !MODO_PID
    int degraus[] = { 800, 2400, 800, 3600 };     // malha aberta: degraus de duty
    int i = 0;
#endif

    // Filtro de medida: media movel M=4 (deriva menos ruidosa — teoria, secao 3)
    int buf[4] = {0}; int bi = 0; long soma = 0;

    while (1) {
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(TS_MS));
        int raw = le_ldr();
        soma += raw - buf[bi]; buf[bi] = raw; bi = (bi + 1) & 3;
        float y = soma / 4.0f;

#if MODO_PID
        float e = REF - y;
        float P = Kp * e;
        float D = Kd * (e - e_ant) / (TS_MS / 1000.0f);
        float u = P + I + D;
        int sat = (u > 4095) ? 1 : (u < 0 ? -1 : 0);
#if ANTI_WINDUP
        if (!(sat == 1 && e > 0) && !(sat == -1 && e < 0))   // clamping
#endif
            I += Ki * (TS_MS / 1000.0f) * e;
        u = P + I + D;
        aplica_duty((int)u);
        e_ant = e;
        float u_log = u;
#else
        static int n = 0;
        if (++n % 250 == 0) i = (i + 1) % 4;      // troca degrau a cada 5 s
        aplica_duty(degraus[i]);
        float u_log = degraus[i];
#endif
        int64_t tm = xTaskGetTickCount() * portTICK_PERIOD_MS - t0;
        printf("%lld,%d,%.0f,%.0f\n", tm, REF, y, u_log);
    }
}
