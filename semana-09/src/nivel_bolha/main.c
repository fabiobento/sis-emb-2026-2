// Semana 9 — DESAFIO (gabarito): nível de bolha digital com MPU-6050 + OLED SSD1306
// -----------------------------------------------------------------------------
// Usa ax e ay (e não theta) para desenhar uma "bolha" que desliza no display
// conforme a inclinação em 2D, como um nível de pedreiro.
//   - ±0,5 g (≈ ±30°) mapeados para ±27 px a partir do centro do display;
//   - círculo desenhado com px() varrendo ângulos de 0 a 2π;
//   - BÔNUS: inércia = filtro passa-baixas de 1ª ordem:  x += ALFA*(alvo - x);
//   - EXTRA: "tara" no boot (subtrai o offset de fábrica do MPU-6050).
// Barramento, driver do OLED e frame buffer: idênticos ao inclinometro_oled.
// -----------------------------------------------------------------------------

#include "freertos/FreeRTOS.h"  // RTOS core
#include "freertos/task.h"      // Controle de atraso preciso (vTaskDelayUntil)
#include "driver/i2c.h"         // Driver I2C nativo do ESP-IDF
#include <stdio.h>              // E/S padrão (snprintf, printf)
#include <string.h>             // Manipulação de memória (memset, memcpy) para o buffer de tela
#include <math.h>               // Funções matemáticas (atan2f, M_PI) para trigonometria

#define SDA 21
#define SCL 22
// Endereços físicos dos dispositivos no barramento I2C:
#define MPU_ADDR  0x68           // MPU-6050 (Acelerômetro/Giroscópio)
#define OLED_ADDR 0x3C           // Display OLED SSD1306 (ATENÇÃO: alguns módulos chineses usam 0x3D)


// =========================================================================
// FUNÇÕES DE COMUNICAÇÃO I2C (Genéricas para múltiplos endereços)
// =========================================================================
// Diferente da parte B, agora passamos 'addr' como argumento, pois temos dois 
// chips diferentes conversando no mesmo par de fios (SDA/SCL).

static esp_err_t wr(uint8_t addr, const uint8_t *buf, size_t n)
{
    return i2c_master_write_to_device(I2C_NUM_0, addr, buf, n, pdMS_TO_TICKS(50));
}

static esp_err_t rd(uint8_t addr, uint8_t reg, uint8_t *dst, size_t n)
{
    return i2c_master_write_read_device(I2C_NUM_0, addr, &reg, 1, dst, n, pdMS_TO_TICKS(50));
}

// =========================================================================
// DRIVER DO DISPLAY OLED SSD1306 (Implementação minimalista sem bibliotecas externas)
// =========================================================================
// O SSD1306 exige que o primeiro byte transmitido indique se os bytes seguintes 
// são Comandos (0x00) de configuração ou Dados (0x40) para acender os pixels da tela.

// Frame buffer: A tela tem 128x64 pixels. Como é monocromática (1 bit por pixel),
// precisamos de 128 * (64 / 8) = 1024 bytes de RAM do ESP32 para armazenar a imagem.
static uint8_t fb[128 * 8];      

// Envia 1 byte de Comando para o display
static void oled_cmd(uint8_t c) { 
    uint8_t b[2] = { 0x00, c }; 
    wr(OLED_ADDR, b, 2); 
}

// Sequência mágica de inicialização do painel OLED extraída do datasheet do SSD1306
static void oled_init(void)
{
    static const uint8_t seq[] = {
        0xAE,                    // Display OFF (desliga a tela durante a configuração)
        0xD5, 0x80,              // Configura o clock do oscilador interno do display
        0xA8, 0x3F,              // Multiplex ratio: habilita as 64 linhas (0x3F = 63 + 1)
        0xD3, 0x00,              // Display offset: sem deslocamento vertical (scroll zero)
        0x40,                    // Linha inicial de RAM mapeada na tela: linha 0
        0x8D, 0x14,              // Charge pump: bomba de carga interna ligada (gera a alta tensão pros pixels)
        0x20, 0x00,              // Modo de endereçamento de memória: HORIZONTAL (O cursor pula de linha sozinho)
        0xA1, 0xC8,              // Espelhamento: Remapeia as colunas/linhas para a imagem não ficar de cabeça para baixo
        0xDA, 0x12,              // Configuração de hardware dos pinos COM (linhas)
        0x81, 0xCF,              // Configura o contraste (brilho): 0xCF (intermediário para alto)
        0xD9, 0xF1, 0xDB, 0x40,  // Ajustes elétricos finos (pré-carga e nível Vcomh)
        0xA4, 0xA6,              // Exibe o conteúdo real da RAM, define polaridade normal (bit 1 = pixel aceso)
        0xAF                     // Display ON (liga a tela)
    };
    for (size_t i = 0; i < sizeof seq; i++) oled_cmd(seq[i]);
}

// Transfere o array 'fb' (Frame Buffer) inteiro da memória do ESP32 para a memória do Display
static void oled_flush(void)     
{
    oled_cmd(0x21); oled_cmd(0); oled_cmd(127);     // Reseta a janela de renderização das colunas (0 a 127)
    oled_cmd(0x22); oled_cmd(0); oled_cmd(7);       // Reseta a janela de renderização das páginas (0 a 7, pois 8x8 = 64)
    
    // O pacote I2C envia o byte de controle (0x40 = pixel) + 128 bytes (uma página inteira) por vez
    uint8_t b[129];
    b[0] = 0x40;                                    
    for (int p = 0; p < 8; p++) {
        memcpy(b + 1, fb + p * 128, 128); // Copia 1 página do frame buffer para o buffer de transmissão
        wr(OLED_ADDR, b, sizeof b);       // Despacha para o hardware I2C
    }
}

// =========================================================================
// MOTOR GRÁFICO 2D (Desenho de Primitivas e Textos)
// =========================================================================

// Liga fisicamente um único pixel (x,y) no array bidimensional do Frame Buffer
static void px(int x, int y)     
{
    if (x < 0 || x > 127 || y < 0 || y > 63) return; // Clip de segurança (fora da tela)
    // O SSD1306 mapeia as linhas verticalmente dentro dos bytes (páginas)
    fb[x + (y / 8) * 128] |= (uint8_t)(1u << (y % 8)); 
}

// Desenha um retângulo sólido (usado para compor as fontes escaladas e barras)
static void rect(int x, int y, int w, int h)
{
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j);
}


// =========================================================================
// PARÂMETROS DO DESAFIO (mexa aqui, não no meio do código)
// =========================================================================
#define PERIODO_MS   50          // 20 Hz: leitura (0,4 ms) + quadro do OLED (~24 ms) cabem folgados (Fig. L9-I)
#define ALFA         0.2f        // inércia da bolha: x += ALFA*(alvo - x)  (τ ≈ 0,22 s a 20 Hz)
#define K_PX_POR_G   54.f        // ±0,5 g  ->  ±27 px   (≈ 0,94 px por grau perto do nível)
#define CX           64          // centro do campo no display
#define CY           32
#define R_CAMPO      31          // anel externo (a tela tem 64 px de altura: 32 ± 31 cabe)
#define R_BOLHA      4
#define R_ALVO       6           // anel-alvo: a bolha "cabe" dentro dele quando está nivelado
#define R_MAX        (R_CAMPO - R_BOLHA)    // 27 px: deslocamento máximo do CENTRO da bolha
#define TOL_PX       (R_ALVO - R_BOLHA)     // 2 px (≈ 2°): dentro disto = NIVELADO
#define SINAL_X      (+1)        // se a bolha andar para o lado errado, troque o sinal
#define SINAL_Y      (+1)
#define TARA         1           // 1 = zera o offset de fábrica no boot (placa DEITADA e parada!)

// =========================================================================
// PRIMITIVAS: círculo vazio e disco cheio
// =========================================================================
// Círculo por varredura de ângulo (a dica do enunciado): x = cx + r*cos(a), y = cy + r*sin(a).
// O passo de ângulo é 1/r rad (≈ 1 px de arco) — com folga, para não deixar buracos.
static void circulo(int cx, int cy, int r)
{
    int passos = (int)(2.f * (float)M_PI * r) + 8;
    for (int k = 0; k < passos; k++) {
        float a = 2.f * (float)M_PI * k / passos;
        px(cx + (int)lroundf(r * cosf(a)), cy + (int)lroundf(r * sinf(a)));
    }
}

static void disco(int cx, int cy, int r)
{
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r) px(cx + dx, cy + dy);
}

// =========================================================================
// CENA: campo + alvo + bolha. (gx, gy) = inclinação já filtrada e sem offset, em g.
// Devolve 1 se está nivelado.
// =========================================================================
static int desenha_nivel(float gx, float gy)
{
    // 1) g -> pixels. Uma bolha de nível real vai para o lado ALTO: ax > 0 quando o lado +X
    //    está mais alto (tabela do item 13), então +X da MPU empurra a bolha para +X da tela.
    //    Na tela o y cresce para BAIXO, por isso o "-" em y.
    float dx =  SINAL_X * gx * K_PX_POR_G;
    float dy = -SINAL_Y * gy * K_PX_POR_G;

    // 2) Limita o deslocamento a um DISCO de raio R_MAX (e não a um quadrado): se a placa
    //    inclina 45° nos dois eixos, a bolha encosta no anel em vez de sair pelo canto.
    float d = hypotf(dx, dy);
    if (d > R_MAX) { dx *= R_MAX / d; dy *= R_MAX / d; d = R_MAX; }

    int nivelado = (d <= TOL_PX);

    memset(fb, 0, sizeof fb);
    circulo(CX, CY, R_CAMPO);                         // anel externo = limite de ±30°
    circulo(CX, CY, R_ALVO);                          // anel-alvo no centro
    rect(CX - R_CAMPO - 0, CY, 4, 1);                 // marcas de mira nos 4 pontos cardeais
    rect(CX + R_CAMPO - 3, CY, 4, 1);
    rect(CX, CY - R_CAMPO, 1, 4);
    rect(CX, CY + R_CAMPO - 3, 1, 4);

    int bx = CX + (int)lroundf(dx), by = CY + (int)lroundf(dy);
    if (nivelado) disco(bx, by, R_BOLHA);             // bolha CHEIA = nivelado (feedback sem texto)
    else          { circulo(bx, by, R_BOLHA); px(bx, by); }   // bolha vazada com um ponto no meio
    return nivelado;
}

// =========================================================================
// LEITURA DO MPU: ax e ay em g (±2 g -> 16384 LSB/g). Devolve ESP_OK ou erro.
// =========================================================================
static esp_err_t le_ax_ay(float *ax, float *ay)
{
    uint8_t b[14];
    esp_err_t e = rd(MPU_ADDR, 0x3B, b, 14);
    if (e != ESP_OK) return e;
    *ax = (int16_t)((b[0] << 8) | b[1]) / 16384.f;    // big-endian, como na Parte B
    *ay = (int16_t)((b[2] << 8) | b[3]) / 16384.f;
    return ESP_OK;
}

void app_main(void)
{
    i2c_config_t cfg = { .mode = I2C_MODE_MASTER, .sda_io_num = SDA, .scl_io_num = SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000 };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    const uint8_t acorda[2] = { 0x6B, 0x00 };
    wr(MPU_ADDR, acorda, 2);
    oled_init();

    // ---- TARA: média de 16 leituras com a placa deitada e parada -----------------------
    // O MPU-6050 sai de fábrica com 0,02–0,05 g de offset (item 14): sem a tara, a bolha
    // "nivelada" ficaria alguns pixels fora do centro (0,03 g = 1,6 px por eixo).
    float ox = 0, oy = 0;
#if TARA
    {
        float sx = 0, sy = 0; int ok = 0;
        for (int i = 0; i < 16; i++) {
            vTaskDelay(pdMS_TO_TICKS(30));
            float ax, ay;
            if (le_ax_ay(&ax, &ay) == ESP_OK) { sx += ax; sy += ay; ok++; }
        }
        if (ok > 0) { ox = sx / ok; oy = sy / ok; }
        printf("tara: ox=%+.3f g, oy=%+.3f g (%d/16 leituras)\n", ox, oy, ok);
    }
#endif

    // ---- Estado da bolha: já FILTRADO, em g -------------------------------------------
    float fx = 0, fy = 0;
    int primeira = 1, ciclo = 0;
    TickType_t prox = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(PERIODO_MS));

        float ax, ay;
        if (le_ax_ay(&ax, &ay) != ESP_OK) { printf("MPU nao respondeu\n"); continue; }
        ax -= ox; ay -= oy;

        // Inércia (BÔNUS): passa-baixas de 1ª ordem nas DUAS coordenadas.
        // Na 1ª amostra parte direto da leitura, para a bolha não "vir do canto".
        if (primeira) { fx = ax; fy = ay; primeira = 0; }
        else          { fx += ALFA * (ax - fx); fy += ALFA * (ay - fy); }

        int ok = desenha_nivel(fx, fy);
        oled_flush();

        if (++ciclo % 10 == 0)                        // log a 2 Hz (não polui o monitor)
            printf("ax=%+.3f ay=%+.3f g  |  filtrado=(%+.3f, %+.3f)  %s\n",
                   ax, ay, fx, fy, ok ? "NIVELADO" : "");
    }
}
