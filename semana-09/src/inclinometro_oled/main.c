// Semana 9C — inclinometro: MPU-6050 (0x68) + OLED SSD1306 128x64 (0x3C) no MESMO barramento I2C
// Le a aceleracao, filtra com media movel M=4 (Lab 7 reciclado), calcula theta = atan2(ax, az)
// e desenha no display: valor grande, lado da inclinacao e uma barra -90..+90 graus.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define SDA 21
#define SCL 22
#define MPU_ADDR  0x68
#define OLED_ADDR 0x3C           // alguns modulos usam 0x3D: confira com o scanner (Parte A)
#define M 4                      // janela da media movel

// ---------- I2C: as mesmas duas funcoes da Parte B, agora com o endereco como parametro ----------
static esp_err_t wr(uint8_t addr, const uint8_t *buf, size_t n)
{
    return i2c_master_write_to_device(I2C_NUM_0, addr, buf, n, pdMS_TO_TICKS(50));
}
static esp_err_t rd(uint8_t addr, uint8_t reg, uint8_t *dst, size_t n)
{
    return i2c_master_write_read_device(I2C_NUM_0, addr, &reg, 1, dst, n, pdMS_TO_TICKS(50));
}

// ---------- SSD1306: byte de controle 0x00 = "o que vem e comando", 0x40 = "o que vem e pixel" ----------
static uint8_t fb[128 * 8];      // frame buffer: 128 colunas x 8 paginas (1 bit por pixel) = 1024 bytes

static void oled_cmd(uint8_t c) { uint8_t b[2] = { 0x00, c }; wr(OLED_ADDR, b, 2); }

static void oled_init(void)
{
    static const uint8_t seq[] = {
        0xAE,                    // display OFF
        0xD5, 0x80,              // clock do display
        0xA8, 0x3F,              // multiplex: 64 linhas
        0xD3, 0x00,              // sem deslocamento vertical
        0x40,                    // linha inicial 0
        0x8D, 0x14,              // bomba de carga ligada (gera a tensao do painel)
        0x20, 0x00,              // enderecamento HORIZONTAL (coluna avanca sozinha)
        0xA1, 0xC8,              // espelha colunas/linhas: imagem "em pe"
        0xDA, 0x12,              // configuracao de pinos COM
        0x81, 0xCF,              // contraste
        0xD9, 0xF1, 0xDB, 0x40,  // pre-carga e nivel Vcomh
        0xA4, 0xA6,              // mostra a RAM, polaridade normal
        0xAF };                  // display ON
    for (size_t i = 0; i < sizeof seq; i++) oled_cmd(seq[i]);
}

static void oled_flush(void)     // envia o frame buffer inteiro: 8 blocos de 128 bytes
{
    oled_cmd(0x21); oled_cmd(0); oled_cmd(127);     // janela de colunas 0..127
    oled_cmd(0x22); oled_cmd(0); oled_cmd(7);       // janela de paginas 0..7
    uint8_t b[129];
    b[0] = 0x40;                                    // controle: dados de pixel
    for (int p = 0; p < 8; p++) {
        memcpy(b + 1, fb + p * 128, 128);
        wr(OLED_ADDR, b, sizeof b);
    }
}

// ---------- fonte 5x7 minima: cada glifo = 5 colunas, bit 0 = linha de cima ----------
typedef struct { char c; uint8_t col[5]; } glifo_t;
static const glifo_t FONTE[] = {
    {'0',{0x3E,0x51,0x49,0x45,0x3E}}, {'1',{0x00,0x42,0x7F,0x40,0x00}},
    {'2',{0x42,0x61,0x51,0x49,0x46}}, {'3',{0x21,0x41,0x45,0x4B,0x31}},
    {'4',{0x18,0x14,0x12,0x7F,0x10}}, {'5',{0x27,0x45,0x45,0x45,0x39}},
    {'6',{0x3C,0x4A,0x49,0x49,0x30}}, {'7',{0x01,0x71,0x09,0x05,0x03}},
    {'8',{0x36,0x49,0x49,0x49,0x36}}, {'9',{0x06,0x49,0x49,0x29,0x1E}},
    {'+',{0x08,0x08,0x3E,0x08,0x08}}, {'-',{0x08,0x08,0x08,0x08,0x08}},
    {'.',{0x00,0x60,0x60,0x00,0x00}}, {'*',{0x06,0x09,0x09,0x06,0x00}},   // '*' = sinal de grau
    {'<',{0x08,0x14,0x22,0x41,0x00}}, {'>',{0x00,0x41,0x22,0x14,0x08}},
    {'=',{0x14,0x14,0x14,0x14,0x14}},
    {'T',{0x01,0x01,0x7F,0x01,0x01}}, {'H',{0x7F,0x08,0x08,0x08,0x7F}},
    {'E',{0x7F,0x49,0x49,0x49,0x41}}, {'A',{0x7E,0x11,0x11,0x11,0x7E}},
};

static void px(int x, int y)     // liga o pixel (x,y): pagina = y/8, bit = y%8
{
    if (x < 0 || x > 127 || y < 0 || y > 63) return;
    fb[x + (y / 8) * 128] |= (uint8_t)(1u << (y % 8));
}
static void rect(int x, int y, int w, int h)
{
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j);
}
static void caractere(int x, int y, char c, int s)   // desenha um glifo ampliado s vezes
{
    for (size_t k = 0; k < sizeof FONTE / sizeof FONTE[0]; k++) {
        if (FONTE[k].c != c) continue;
        for (int col = 0; col < 5; col++)
            for (int lin = 0; lin < 7; lin++)
                if (FONTE[k].col[col] & (1u << lin)) rect(x + col * s, y + lin * s, s, s);
        return;
    }
}
static void texto(int x, int y, const char *t, int s)
{
    for (; *t; t++, x += 6 * s) caractere(x, y, *t, s);   // 5 colunas + 1 de espaco
}

static void desenha(float theta)
{
    memset(fb, 0, sizeof fb);
    texto(2, 2, "THETA", 1);
    char s[12];
    snprintf(s, sizeof s, "%+6.1f", theta);            // ex.: " +12.3"
    texto(2, 14, s, 3);
    caractere(2 + 6 * 18, 14, '*', 3);                  // grau, logo depois do numero
    if (theta > 2.f)       texto(34, 40, ">>>>>", 2);   // inclinada para o lado +ax
    else if (theta < -2.f) texto(34, 40, "<<<<<", 2);   // inclinada para o lado -ax
    else                   texto(34, 40, "=====", 2);   // nivelada (|theta| <= 2 graus)
    rect(0, 61, 128, 1);                                // trilho da barra: -90 ... +90 graus
    float t = theta > 90.f ? 90.f : (theta < -90.f ? -90.f : theta);
    rect(63 + (int)(t * 63.f / 90.f) - 1, 58, 3, 6);    // cursor da barra
    rect(63, 56, 1, 3);                                 // marca do zero
}

void app_main(void)
{
    i2c_config_t cfg = { .mode = I2C_MODE_MASTER, .sda_io_num = SDA, .scl_io_num = SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000 };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    const uint8_t acorda[2] = { 0x6B, 0x00 };           // PWR_MGMT_1 = 0: tira o MPU do sleep
    wr(MPU_ADDR, acorda, 2);
    oled_init();

    float bx[M] = {0}, bz[M] = {0};                      // buffers circulares (Lab 7)
    float sx = 0, sz = 0; int idx = 0; int n = 0;
    TickType_t prox = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(100));      // 10 Hz, periodo cravado
        uint8_t b[14];
        if (rd(MPU_ADDR, 0x3B, b, 14) != ESP_OK) { printf("MPU nao respondeu\n"); continue; }
        float ax = (int16_t)((b[0] << 8) | b[1]) / 16384.f;   // big-endian -> g
        float az = (int16_t)((b[4] << 8) | b[5]) / 16384.f;

        if (n == 0) { for (int i = 0; i < M; i++) { bx[i] = ax; bz[i] = az; } sx = M * ax; sz = M * az; }
        sx += ax - bx[idx]; sz += az - bz[idx];          // media movel O(1) em ax e em az
        bx[idx] = ax; bz[idx] = az; idx = (idx + 1) % M;
        float theta = atan2f(sx / M, sz / M) * 180.f / (float)M_PI;   // filtra os EIXOS, depois atan2

        if (++n % 2 == 0) {                              // desenha a cada 2 leituras: 5 Hz
            desenha(theta);
            oled_flush();
            printf("theta=%+.1f o\n", theta);
        }
    }
}
