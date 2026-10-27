// Semana 9C — inclinometro: MPU-6050 (0x68) + OLED SSD1306 128x64 (0x3C) no MESMO barramento I2C
// -----------------------------------------------------------------------------
// PRÁTICA: Integração de um sensor e um atuador/display no mesmo barramento I2C.
// OBJETIVO: 
// 1. Ler a aceleração (eixos X e Z) do MPU-6050.
// 2. Filtrar o ruído mecânico/elétrico usando o Filtro de Média Móvel O(1) do Lab 7.
// 3. Calcular a inclinação (theta) da placa em graus.
// 4. Desenhar os dados e gráficos (valor numérico ampliado, setas direcionais e barra 
//    de rolagem) no display OLED SSD1306 em tempo real.
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

#define M 4                      // Tamanho da janela da média móvel. M=4 suaviza bem sem atrasar muito a tela.

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

// Estrutura para criar uma fonte bitmap compacta na memória Flash (ROM).
// Cada caractere gasta apenas 5 bytes. O bit 0 de cada byte é o topo do caractere.
typedef struct { char c; uint8_t col[5]; } glifo_t;
static const glifo_t FONTE[] = {
    // Apenas os caracteres que usaremos para não gastar memória à toa
    {'0',{0x3E,0x51,0x49,0x45,0x3E}}, {'1',{0x00,0x42,0x7F,0x40,0x00}},
    {'2',{0x42,0x61,0x51,0x49,0x46}}, {'3',{0x21,0x41,0x45,0x4B,0x31}},
    {'4',{0x18,0x14,0x12,0x7F,0x10}}, {'5',{0x27,0x45,0x45,0x45,0x39}},
    {'6',{0x3C,0x4A,0x49,0x49,0x30}}, {'7',{0x01,0x71,0x09,0x05,0x03}},
    {'8',{0x36,0x49,0x49,0x49,0x36}}, {'9',{0x06,0x49,0x49,0x29,0x1E}},
    {'+',{0x08,0x08,0x3E,0x08,0x08}}, {'-',{0x08,0x08,0x08,0x08,0x08}},
    {'.',{0x00,0x60,0x60,0x00,0x00}}, {'*',{0x06,0x09,0x09,0x06,0x00}},   // '*' mapeado como bolinha de grau
    {'<',{0x08,0x14,0x22,0x41,0x00}}, {'>',{0x00,0x41,0x22,0x14,0x08}},
    {'=',{0x14,0x14,0x14,0x14,0x14}},
    {'T',{0x01,0x01,0x7F,0x01,0x01}}, {'H',{0x7F,0x08,0x08,0x08,0x7F}},
    {'E',{0x7F,0x49,0x49,0x49,0x41}}, {'A',{0x7E,0x11,0x11,0x11,0x7E}},
};

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

// Renderiza um caractere na tela usando a FONTE. 
// O parâmetro 's' define a escala (zoom). s=1 (5x7 normal), s=3 (15x21 ampliado).
static void caractere(int x, int y, char c, int s)   
{
    for (size_t k = 0; k < sizeof FONTE / sizeof FONTE[0]; k++) {
        if (FONTE[k].c != c) continue;
        for (int col = 0; col < 5; col++)
            for (int lin = 0; lin < 7; lin++)
                if (FONTE[k].col[col] & (1u << lin)) 
                    rect(x + col * s, y + lin * s, s, s); // Substitui 1 pixel por um quadrado s*s
        return;
    }
}

// Imprime uma string ('t') completa coordenada
static void texto(int x, int y, const char *t, int s)
{
    for (; *t; t++, x += 6 * s) caractere(x, y, *t, s);   // 5 colunas do glifo + 1 coluna de espaçamento
}

// =========================================================================
// MONTAGEM DA INTERFACE GRÁFICA (UI) DO INCLINÓMETRO
// =========================================================================
static void desenha(float theta)
{
    memset(fb, 0, sizeof fb); // Limpa o quadro anterior (pinta de preto)
    
    // Título fixo, pequeno no topo
    texto(2, 2, "THETA", 1);
    
    // Formatação do número: %+6.1f garante que terá o sinal de + ou - sempre visível
    // e terá sempre o mesmo número de caracteres, evitando "flicker" de deslocamento.
    char s[12];
    snprintf(s, sizeof s, "%+6.1f", theta);            
    
    // Escreve o número com zoom (s=3)
    texto(2, 14, s, 3);
    // Desenha o símbolo de grau (mapeado como '*') grudado no final do número
    caractere(2 + 6 * 18, 14, '*', 3);                  
    
    // Setas indicativas de inclinação
    if (theta > 2.f)       texto(34, 40, ">>>>>", 2);   // Placa levantada à direita
    else if (theta < -2.f) texto(34, 40, "<<<<<", 2);   // Placa levantada à esquerda
    else                   texto(34, 40, "=====", 2);   // Nivelada (deadband de +-2 graus p/ evitar jitter)
    
    // Barra de rolagem analógica inferior
    rect(0, 61, 128, 1);                                // Desenha a trilha base (-90 a +90 graus)
    float t = theta > 90.f ? 90.f : (theta < -90.f ? -90.f : theta); // Satura/Clipa em 90 graus
    
    // Posição no eixo X (cursor da barra)
    // 63 é o centro da tela. (t * 63 / 90) mapeia o ângulo -90..90 na tela -63..63
    rect(63 + (int)(t * 63.f / 90.f) - 1, 58, 3, 6);    // Cursor retangular (bolinha)
    
    // Tracinho no meio para referenciar o zero
    rect(63, 56, 1, 3);                                 
}

// =========================================================================
// TASK PRINCIPAL: Lógica de Sensor, Filtro e Tela
// =========================================================================
void app_main(void)
{
    // Instala o motor I2C a 400kHz. Como o OLED SSD1306 e o MPU-6050 
    // operam perfeitamente no Fast Mode, o barramento roda liso.
    i2c_config_t cfg = { .mode = I2C_MODE_MASTER, .sda_io_num = SDA, .scl_io_num = SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000 };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    // Acorda o Acelerômetro
    const uint8_t acorda[2] = { 0x6B, 0x00 };           
    wr(MPU_ADDR, acorda, 2);
    
    // Configura fisicamente o OLED
    oled_init();

    // =========================================================================
    // INICIALIZAÇÃO DO FILTRO (Laboratório 7 reaproveitado)
    // =========================================================================
    float bx[M] = {0}, bz[M] = {0};                      // Filas circulares
    float sx = 0, sz = 0; int idx = 0; int n = 0;
    TickType_t prox = xTaskGetTickCount();

    while (1) {
        // Amostra o acelerômetro cravado a 10 Hz
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(100));      
        
        uint8_t b[14];
        if (rd(MPU_ADDR, 0x3B, b, 14) != ESP_OK) { 
            printf("MPU nao respondeu\n"); 
            continue; 
        }
        
        // Conversão raw -> g (Big-Endian)
        float ax = (int16_t)((b[0] << 8) | b[1]) / 16384.f;   
        float az = (int16_t)((b[4] << 8) | b[5]) / 16384.f;

        // Efeito prático do Filtro O(1):
        // Antes de encher o buffer na primeira vez, preenchemos ele inteiro com 
        // a primeira amostra para o gráfico não vir subindo gradativamente do zero.
        if (n == 0) { 
            for (int i = 0; i < M; i++) { bx[i] = ax; bz[i] = az; } 
            sx = M * ax; sz = M * az; 
        }
        
        // Executa a Média Móvel nos eixos RAW separadamente
        sx += ax - bx[idx]; sz += az - bz[idx];          
        bx[idx] = ax; bz[idx] = az; 
        idx = (idx + 1) % M;
        
        // BOAS PRÁTICAS DE MATEMÁTICA EMBARCADA: 
        // Filtramos a aceleração (eixos independentes) PRIMEIRO, e depois aplicamos atan2 
        // em cima da média. Filtrar o ângulo final causaria um bug no cruzamento 
        // entre +180 e -180 graus (efeito wrap-around / Aliasing direcional).
        float theta = atan2f(sx / M, sz / M) * 180.f / (float)M_PI;   

        // Decimação de Tela: O sensor lê a 10 Hz, mas atualizar o OLED 
        // a 10Hz causaria um leve travamento/piscar pela velocidade do barramento I2C.
        // Assim, pulamos uma leitura e só desenhamos na tela a 5 Hz (a cada n par).
        if (++n % 2 == 0) {                              
            desenha(theta);      // Compõe os pixels na RAM
            oled_flush();        // Despacha a RAM pro painel via I2C
            printf("theta=%+.1f o\n", theta); // Manda pro PC como log redundante
        }
    }
}