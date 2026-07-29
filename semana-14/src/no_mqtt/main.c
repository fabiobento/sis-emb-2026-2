/*
 * Semana 14 — Nó sensor MQTT (ESP-IDF v5.x)
 * Wi-Fi station + ESP-MQTT: publica temperatura/umidade (DHT11 no GPIO4) a cada 5 s,
 * assina comando de LED e configura LWT em ifes/bancadaN/status.
 *
 * Edite: WIFI_SSID, WIFI_PASS, BROKER_URI, BANCADA.
 * Padrões do curso: eventos → event group (semana 6); NVS init (semana 2).
 * Leitura do DHT11 por GPIO bit-banging simplificado (didático; em produto use RMT).
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"

#define WIFI_SSID   "MINHA_REDE"
#define WIFI_PASS   "MINHA_SENHA"
#define BROKER_URI  "mqtt://192.168.0.10"   /* IP do RPi 3 */
#define BANCADA     "bancada1"

#define PINO_DHT    GPIO_NUM_4
#define PINO_LED    GPIO_NUM_2

#define T_STATUS    "ifes/" BANCADA "/status"
#define T_TEMP      "ifes/" BANCADA "/temp"
#define T_UMID      "ifes/" BANCADA "/umid"
#define T_CMD_LED   "ifes/" BANCADA "/cmd/led"

static const char *TAG = "no_mqtt";
static EventGroupHandle_t eg;
#define BIT_GOT_IP   BIT0
#define BIT_MQTT_OK  BIT1
static esp_mqtt_client_handle_t cli;

/* ---------- Wi-Fi (padrão event handler → event group, Exemplo 14.1) ---------- */
static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi caiu; reconectando...");
        xEventGroupClearBits(eg, BIT_GOT_IP);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "IP obtido");
        xEventGroupSetBits(eg, BIT_GOT_IP);
    }
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
    wifi_config_t wc = { 0 };
    strncpy((char *)wc.sta.ssid, WIFI_SSID, sizeof wc.sta.ssid);
    strncpy((char *)wc.sta.password, WIFI_PASS, sizeof wc.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* ---------- MQTT ---------- */
static void mqtt_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t ev = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT conectado");
        /* status online retained: novos assinantes veem o estado atual */
        esp_mqtt_client_publish(cli, T_STATUS, "online", 0, 1, 1);
        /* comandos com QoS 1 (Exemplo 14.2) */
        esp_mqtt_client_subscribe(cli, T_CMD_LED, 1);
        xEventGroupSetBits(eg, BIT_MQTT_OK);
        break;
    case MQTT_EVENT_DISCONNECTED:
        xEventGroupClearBits(eg, BIT_MQTT_OK);
        break;
    case MQTT_EVENT_DATA:
        if (!strncmp(ev->topic, T_CMD_LED, ev->topic_len)) {
            int liga = (ev->data_len > 0 && ev->data[0] == '1');
            gpio_set_level(PINO_LED, liga);
            ESP_LOGI(TAG, "LED <- %d (via MQTT)", liga);
        }
        break;
    default:
        break;
    }
}

static void mqtt_start(void)
{
    esp_mqtt_client_config_t mc = {
        .broker.address.uri = BROKER_URI,
        /* LWT: se o nó sumir sem DISCONNECT, o broker publica isto (lab, Parte B.5) */
        .session.last_will = {
            .topic = T_STATUS, .msg = "offline", .msg_len = 7, .qos = 1, .retain = 1,
        },
        .session.keepalive = 5,
    };
    cli = esp_mqtt_client_init(&mc);
    esp_mqtt_client_register_event(cli, ESP_EVENT_ANY_ID, mqtt_handler, NULL);
    esp_mqtt_client_start(cli);
}

/* ---------- DHT11 (bit-banging didático) ---------- */
static int dht11_ler(int *temp, int *umid)
{
    uint8_t d[5] = { 0 };
    gpio_set_direction(PINO_DHT, GPIO_MODE_OUTPUT);
    gpio_set_level(PINO_DHT, 0);
    ets_delay_us(20000);                      /* start: >18 ms em nível baixo   */
    gpio_set_level(PINO_DHT, 1);
    ets_delay_us(30);
    gpio_set_direction(PINO_DHT, GPIO_MODE_INPUT);

    /* resposta do sensor: 80 us L + 80 us H */
    int64_t t0;
    for (int esp = 0; esp < 2; esp++) {
        t0 = esp_timer_get_time();
        while (gpio_get_level(PINO_DHT) == esp % 2 ? 0 : 1) { }  /* aguarda borda */
        if (esp_timer_get_time() - t0 > 200) return -1;
    }
    /* 40 bits: 50 us L + (26–28 us H = 0 | 70 us H = 1) */
    for (int i = 0; i < 40; i++) {
        t0 = esp_timer_get_time();
        while (!gpio_get_level(PINO_DHT)) if (esp_timer_get_time() - t0 > 100) return -1;
        t0 = esp_timer_get_time();
        while (gpio_get_level(PINO_DHT))  if (esp_timer_get_time() - t0 > 120) return -1;
        d[i / 8] <<= 1;
        if (esp_timer_get_time() - t0 > 45) d[i / 8] |= 1;
    }
    if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) return -2;  /* checksum */
    *umid = d[0]; *temp = d[2];
    return 0;
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    eg = xEventGroupCreate();
    gpio_set_direction(PINO_LED, GPIO_MODE_OUTPUT);

    wifi_start();
    xEventGroupWaitBits(eg, BIT_GOT_IP, pdFALSE, pdTRUE, portMAX_DELAY);
    mqtt_start();
    xEventGroupWaitBits(eg, BIT_MQTT_OK, pdFALSE, pdTRUE, portMAX_DELAY);

    char msg[16];
    TickType_t prox = xTaskGetTickCount();
    while (1) {
        int t, u;
        int rc = dht11_ler(&t, &u);
        if (rc == 0) {
            snprintf(msg, sizeof msg, "%d", t);
            esp_mqtt_client_publish(cli, T_TEMP, msg, 0, 0, 0);  /* telemetria: QoS 0 */
            snprintf(msg, sizeof msg, "%d", u);
            esp_mqtt_client_publish(cli, T_UMID, msg, 0, 0, 0);
            ESP_LOGI(TAG, "pub temp=%d umid=%d", t, u);
        } else {
            ESP_LOGW(TAG, "DHT11 falhou (%d)", rc);
        }
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(5000));  /* período exato: semana 5 */
    }
}
