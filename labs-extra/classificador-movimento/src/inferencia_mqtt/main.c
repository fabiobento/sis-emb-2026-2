// Lab Extra — Classificador de movimento (inferencia na borda + MQTT)
//
// ESQUELETO de integracao. A parte de ML (run_classifier + o header do modelo) vem da
// biblioteca C++ que VOCE exporta do Edge Impulse (Deployment -> C++ library) e copia para
// dentro deste projeto ESP-IDF, junto do repo-base:
//   github.com/edgeimpulse/example-standalone-inferencing-espressif-esp32
//
// Duas tarefas (semanas 5 e 6):
//   - amostra_task: le o MPU-6050 a 100 Hz e enche a janela (buffer de JANELA amostras)
//   - inferencia_task: quando a janela enche, roda o classificador e publica a classe
// A publicacao MQTT reusa exatamente o cliente da semana 14 (mesmo broker do projeto).
//
// Este arquivo compila como referencia de estrutura; descomente os trechos marcados
// {{EI}} depois de adicionar a biblioteca exportada. Mantido curto de proposito.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <stdio.h>
#include <string.h>

// {{EI}} #include "edge-impulse-sdk/classifier/ei_run_classifier.h"
// {{EI}} #include "model-parameters/model_metadata.h"
// (EI_CLASSIFIER_RAW_SAMPLE_COUNT, EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME=3, e os rotulos
//  vem desses headers — nao invente os valores, eles descrevem o SEU impulse.)

#define ADDR 0x68
#define REG_PWR 0x6B
#define REG_ACCEL 0x3B
#define FS_HZ  100
#define JANELA 200            // 2 s a 100 Hz — CASE com a janela do Impulse no Studio

static float buf[JANELA * 3];  // aX,aY,aZ intercalados (layout que o Impulse espera)
static volatile int cheio = 0;

static esp_err_t rd(uint8_t reg, uint8_t *dst, size_t n) {
    return i2c_master_write_read_device(I2C_NUM_0, ADDR, &reg, 1, dst, n, pdMS_TO_TICKS(50));
}

static void amostra_task(void *arg) {
    const TickType_t T = pdMS_TO_TICKS(1000 / FS_HZ);
    TickType_t prox = xTaskGetTickCount();
    int i = 0;
    while (1) {
        uint8_t b[6]; rd(REG_ACCEL, b, 6);
        int16_t ax=(b[0]<<8)|b[1], ay=(b[2]<<8)|b[3], az=(b[4]<<8)|b[5];
        buf[i*3+0]=ax/16384.f; buf[i*3+1]=ay/16384.f; buf[i*3+2]=az/16384.f;
        if (++i >= JANELA) { i = 0; cheio = 1; }   // janela cheia -> libera inferencia
        // NOTA: esta e uma janela POR BLOCOS (enche, classifica, recomeca): uma leitura a cada
        // 2 s. Simples e suficiente para o lab. Melhoria natural p/ o projeto: janela DESLIZANTE
        // (buffer circular + stride), que classifica com mais frequencia — bom desafio de FreeRTOS.
        vTaskDelayUntil(&prox, T);
    }
}

static void inferencia_task(void *arg) {
    while (1) {
        if (!cheio) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        cheio = 0;

        // {{EI}} signal_t signal;
        // {{EI}} numpy::signal_from_buffer(buf, JANELA*3, &signal);
        // {{EI}} ei_impulse_result_t r = { 0 };
        // {{EI}} run_classifier(&signal, &r, false);
        // {{EI}} // acha a classe de maior probabilidade
        // {{EI}} int best = 0; for (int k=1;k<EI_CLASSIFIER_LABEL_COUNT;k++)
        // {{EI}}     if (r.classification[k].value > r.classification[best].value) best = k;
        // {{EI}} const char *classe = ei_classifier_inferencing_categories[best];
        // {{EI}} float conf = r.classification[best].value;

        const char *classe = "parado"; float conf = 0.0f;  // placeholder sem a lib

        // publica a CONCLUSAO (poucos bytes), nao o fluxo bruto — cliente MQTT da semana 14
        char msg[64];
        snprintf(msg, sizeof msg, "{\"classe\":\"%s\",\"conf\":%.2f}", classe, conf);
        // {{S14}} esp_mqtt_client_publish(cliente, "movimento/no01", msg, 0, 0, 0);
        printf("publicaria em movimento/no01: %s\n", msg);
    }
}

void app_main(void)
{
    i2c_config_t cfg = { .mode = I2C_MODE_MASTER, .sda_io_num = 21, .scl_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000 };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    uint8_t wake[2] = { REG_PWR, 0x00 };
    i2c_master_write_to_device(I2C_NUM_0, ADDR, wake, 2, pdMS_TO_TICKS(50));

    // {{S14}} inicialize o Wi-Fi + esp_mqtt_client como no lab da semana 14, antes das tarefas.

    xTaskCreate(amostra_task,    "amostra",    4096, NULL, 6, NULL);  // prioridade > inferencia
    xTaskCreate(inferencia_task, "inferencia", 8192, NULL, 5, NULL);
}
