// Lab Extra — Classificador de movimento no ESP32 (inferencia na borda + MQTT)
// ESP-IDF 5.x. Arquivo principal do projeto montado sobre o exemplo
//   github.com/edgeimpulse/example-standalone-inferencing-espressif-esp32
// (substitui o arquivo principal do exemplo; ver o lab, Parte D).
//
// Arquitetura (semanas 5, 6 e 14):
//
//   amostra_task (prio 6)              inferencia_task (prio 5)
//   vTaskDelayUntil, 100 Hz            espera notificacao
//   le MPU-6050 ──▶ janela_push        copia janela ──▶ clf_classifica ──▶ debounce
//   (mutex curto)    │ stride?  ───────▶ xTaskNotifyGive                      │ mudou?
//                                                                              ▼
//                                                       esp_mqtt_client_publish(ifes/<b>/movimento)
//
// Compilar SEM o Edge Impulse (testar sensor + Wi-Fi + MQTT com o classificador falso):
//   idf.py build -DCMAKE_CXX_FLAGS="-DUSAR_EI=0"      (ou coloque #define USAR_EI 0 em classificador.h)
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "driver/gpio.h"

#include "projeto_config.h"
#include "mpu.h"
#include "janela.h"
#include "debounce.h"
#include "classificador.h"

static const char *TAG = "movimento";

static float          g_mem[JANELA_AMOSTRAS * EIXOS];   // memoria do buffer circular
static janela_t       g_jan;
static SemaphoreHandle_t g_mutex;
static TaskHandle_t   g_task_inf;
static volatile long  g_perdidas = 0;                   // erros de I2C (amostra repetida)

static EventGroupHandle_t eg;
#define BIT_GOT_IP  BIT0
#define BIT_MQTT_OK BIT1
static esp_mqtt_client_handle_t cli;

/* ============================ Wi-Fi + MQTT (padrao da semana 14) ============================ */
static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(eg, BIT_GOT_IP); esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) xEventGroupSetBits(eg, BIT_GOT_IP);
}

static void wifi_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handler, NULL);
    wifi_config_t wc = {};
    strncpy((char *)wc.sta.ssid, WIFI_SSID, sizeof wc.sta.ssid);
    strncpy((char *)wc.sta.password, WIFI_PASS, sizeof wc.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void mqtt_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if ((esp_mqtt_event_id_t)id == MQTT_EVENT_CONNECTED) {
        esp_mqtt_client_publish(cli, T_STATUS, "online", 0, 1, 1);   // retido
        xEventGroupSetBits(eg, BIT_MQTT_OK);
    } else if ((esp_mqtt_event_id_t)id == MQTT_EVENT_DISCONNECTED) {
        xEventGroupClearBits(eg, BIT_MQTT_OK);
    }
}

static void mqtt_start(void)
{
    esp_mqtt_client_config_t mc = {};
    mc.broker.address.uri = BROKER_URI;
    mc.session.last_will.topic = T_STATUS;
    mc.session.last_will.msg = "offline";
    mc.session.last_will.msg_len = 7;
    mc.session.last_will.qos = 1;
    mc.session.last_will.retain = 1;
    mc.session.keepalive = 10;
    cli = esp_mqtt_client_init(&mc);
    esp_mqtt_client_register_event(cli, MQTT_EVENT_ANY, mqtt_handler, NULL);
    esp_mqtt_client_start(cli);
}

/* ============================ Tarefa 1: amostragem a 100 Hz ============================ */
static void amostra_task(void *arg)
{
    const TickType_t T = pdMS_TO_TICKS(1000 / FS_HZ);
    TickType_t prox = xTaskGetTickCount();
    float a[3] = { 0, 0, 1 };
    for (;;) {
        if (mpu_le_g(a) != ESP_OK) g_perdidas++;          // repete a ultima amostra boa
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        int pronta = janela_push(&g_jan, a);              // secao critica curtissima
        xSemaphoreGive(g_mutex);
        if (pronta) xTaskNotifyGive(g_task_inf);          // acorda a inferencia
        vTaskDelayUntil(&prox, T);                        // periodo cravado (semana 5)
    }
}

/* ============================ Tarefa 2: inferencia + decisao + MQTT ============================ */
static void inferencia_task(void *arg)
{
    static float janela[JANELA_AMOSTRAS * EIXOS];         // copia linearizada (cronologica)
    float prob[CLF_N_CLASSES];
    debounce_t deb; debounce_init(&deb, N_CONFIRMA, LIMIAR_CONF);
    int contador = 0;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);          // bloqueia ate haver janela nova
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        janela_copia(&g_jan, janela);
        xSemaphoreGive(g_mutex);

        float ms_dsp = 0, ms_clf = 0;
        int melhor = clf_classifica(janela, prob, &ms_dsp, &ms_clf);
        if (melhor < 0) { ESP_LOGE(TAG, "run_classifier falhou"); continue; }

        // diagnostico serial: uma linha por janela, com todas as probabilidades
        printf("[janela] ");
        for (int k = 0; k < CLF_N_CLASSES; k++) printf("%s=%.2f ", clf_nome(k), prob[k]);
        printf("| dsp=%.0f ms clf=%.0f ms perdidas_i2c=%ld\n", ms_dsp, ms_clf, g_perdidas);

        if (debounce_update(&deb, melhor, prob[melhor])) {
            const char *classe = clf_nome(deb.estado);
            char msg[96];
            snprintf(msg, sizeof msg, "{\"classe\":\"%s\",\"conf\":%.2f,\"t\":%lld}",
                     classe, prob[melhor], (long long)(esp_timer_get_time() / 1000));
            esp_mqtt_client_publish(cli, T_MOV, msg, 0, 0, 1);   // QoS 0, retido: painel novo ve o estado atual
            ESP_LOGI(TAG, "mudou -> %s", msg);
            gpio_set_level((gpio_num_t)PINO_LED, strcmp(classe, CLASSE_ATUA) == 0);  // acao local
        }
        if (++contador % INFO_A_CADA == 0) {
            char info[96];
            snprintf(info, sizeof info, "{\"dsp_ms\":%.0f,\"clf_ms\":%.0f,\"i2c_err\":%ld}",
                     ms_dsp, ms_clf, g_perdidas);
            esp_mqtt_client_publish(cli, T_INFO, info, 0, 0, 0);
        }
    }
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    eg = xEventGroupCreate();
    gpio_set_direction((gpio_num_t)PINO_LED, GPIO_MODE_OUTPUT);

    uint8_t who = mpu_init();
    ESP_LOGI(TAG, "MPU-6050 WHO_AM_I=0x%02X (esperado 0x68)", who);

    janela_init(&g_jan, g_mem, JANELA_AMOSTRAS, STRIDE_AMOSTRAS);
    g_mutex = xSemaphoreCreateMutex();

    wifi_start();
    xEventGroupWaitBits(eg, BIT_GOT_IP, pdFALSE, pdTRUE, portMAX_DELAY);
    mqtt_start();
    xEventGroupWaitBits(eg, BIT_MQTT_OK, pdFALSE, pdTRUE, portMAX_DELAY);

    // A inferencia precisa de pilha grande (o SDK do EI usa bastante); 16 kB e um ponto de partida.
    xTaskCreate(inferencia_task, "inferencia", 16384, NULL, 5, &g_task_inf);
    xTaskCreate(amostra_task,    "amostra",      4096, NULL, 6, NULL);
}
