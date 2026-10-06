// Semana 6A — condicao de corrida e correcao com mutex
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include <stdio.h>

#define USAR_MUTEX 0
#define N 1000000
#define N_ESPERADO (2 * N)

// BONUS: se voce tiver um display 16x2 + adaptador I2C (modulo "LCM1602 IIC", chip
// PCF8574), troque para 1 e ligue SDA->GPIO21, SCL->GPIO22, VCC->5V, GND->GND. O
// display mostra o contador subindo em tempo real e, ao final, se o resultado deu
// "OK! 2000000" ou "PERDEU <n>" - sem precisar olhar o monitor serial. O barramento
// I2C sera estudado formalmente na Semana 9; aqui e so uma previa pratica.
#define USAR_LCD 0

static volatile uint32_t g_contador = 0;
static SemaphoreHandle_t g_mutex;

#if USAR_LCD
#include "driver/i2c.h"
#include "rom/ets_sys.h"

#define LCD_ADDR 0x27   // se nao responder, troque para 0x3F (segundo endereco mais comum)
#define SDA 21
#define SCL 22

// --- driver minimo do HD44780 em modo 4 bits atraves do expansor PCF8574 ---
// Pinagem padrao do modulo "LCM1602 IIC": P0=RS P1=RW P2=EN P3=luz de fundo P4..P7=D4..D7
#define LCD_RS 0x01
#define LCD_EN 0x04
#define LCD_BL 0x08

static void lcd_wr_byte(uint8_t b)
{
    i2c_master_write_to_device(I2C_NUM_0, LCD_ADDR, &b, 1, pdMS_TO_TICKS(20));
}
static void lcd_pulse(uint8_t nibble)
{
    lcd_wr_byte(nibble | LCD_EN);
    ets_delay_us(1);
    lcd_wr_byte(nibble & ~LCD_EN);
    ets_delay_us(50);
}
static void lcd_send(uint8_t valor, uint8_t rs)
{
    uint8_t base = LCD_BL | (rs ? LCD_RS : 0);
    lcd_pulse(base | (valor & 0xF0));
    lcd_pulse(base | ((valor << 4) & 0xF0));
}
static void lcd_cmd(uint8_t c)  { lcd_send(c, 0); }
static void lcd_data(uint8_t d) { lcd_send(d, 1); }

static void lcd_init(void)
{
    i2c_config_t cfg = { .mode = I2C_MODE_MASTER, .sda_io_num = SDA, .scl_io_num = SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000 };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));          // tempo de ligar exigido pelo HD44780

    // sequencia de "despertar" em 4 bits (pagina de inicializacao do datasheet HD44780)
    lcd_pulse(LCD_BL | 0x30); vTaskDelay(pdMS_TO_TICKS(5));
    lcd_pulse(LCD_BL | 0x30); ets_delay_us(150);
    lcd_pulse(LCD_BL | 0x30);
    lcd_pulse(LCD_BL | 0x20);               // a partir daqui, 4 bits para sempre
    lcd_cmd(0x28);                           // 4 bits, 2 linhas, fonte 5x8
    lcd_cmd(0x0C);                           // display ligado, cursor e blink desligados
    lcd_cmd(0x06);                           // incrementa cursor, sem deslocar a tela
    lcd_cmd(0x01);                           // limpa
    vTaskDelay(pdMS_TO_TICKS(2));
}
static void lcd_goto(uint8_t linha, uint8_t col)
{
    lcd_cmd(0x80 | ((linha ? 0x40 : 0x00) + col));
}
static void lcd_print(const char *s) { while (*s) lcd_data(*s++); }

// Tarefa de prioridade baixa: so LE g_contador (leitura solta, nunca escreve) a cada
// 150 ms - nao participa da corrida, so a exibe. Detecta o fim "pela estabilidade": se
// o valor parar de mudar por ~500 ms, as duas tarefas ja terminaram e ja se autodeletaram.
static void tarefa_lcd(void *arg)
{
    lcd_init();
    lcd_goto(0, 0);
    lcd_print(USAR_MUTEX ? "COM mutex" : "SEM mutex");

    uint32_t anterior = 0, estavel_ms = 0;
    while (1) {
        uint32_t atual = g_contador;
        char linha[17];
        snprintf(linha, sizeof(linha), "cont=%-10lu", (unsigned long)atual);
        lcd_goto(1, 0);
        lcd_print(linha);

        if (atual == anterior) {
            estavel_ms += 150;
        } else {
            estavel_ms = 0;
            anterior = atual;
        }
        if (estavel_ms >= 500 && atual > 0) {
            lcd_goto(0, 0);
            if (atual == N_ESPERADO) {
                lcd_print("OK! 2000000   ");
            } else {
                char resumo[17];
                snprintf(resumo, sizeof(resumo), "PERDEU %6lu", (unsigned long)(N_ESPERADO - atual));
                lcd_print(resumo);
            }
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}
#endif // USAR_LCD

// Em hardware real (240 MHz), a janela de risco entre o LOAD e o STORE de "g_contador++"
// dura poucos nanossegundos - curta demais para o escalonador "acertar" com frequencia
// em 1 milhao de tentativas. Esta funcao alarga essa janela de proposito (so para tornar
// a corrida observavel neste experimento), sem mudar a natureza do bug: continua sendo
// leitura-modificacao-escrita nao atomica, so que agora com tempo de sobra para colidir.
static inline void amplia_janela_de_risco(void)
{
    for (volatile int k = 0; k < 30; k++) { }
}

static void incrementador(void *arg)
{
    for (int i = 0; i < N; i++) {
#if USAR_MUTEX
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        uint32_t tmp = g_contador;      // LOAD
        amplia_janela_de_risco();
        g_contador = tmp + 1;           // STORE
        xSemaphoreGive(g_mutex);
#else
        uint32_t tmp = g_contador;      // LOAD
        amplia_janela_de_risco();       // leitura-modificacao-escrita NAO atomica
        g_contador = tmp + 1;           // STORE
#endif
    }
    printf("tarefa %s terminou; contador=%lu\n",
           (char *)arg, (unsigned long)g_contador);
    vTaskDelete(NULL);
}

void app_main(void)
{
    // T1 e T2 ficam no mesmo nucleo, mesma prioridade, e (com USAR_MUTEX 1) fazem
    // 2 milhoes de Take/Give SEM nunca ceder a CPU -> a IDLE1 daquele nucleo fica
    // sem rodar tempo suficiente para alimentar o Task Watchdog (timeout padrao 5 s),
    // e o sistema reiniciaria antes de voce ler o resultado. Provocar o watchdog nao
    // e o assunto desta parte (isso ja foi coberto nos Labs 4 e 5) - desligamos aqui
    // de proposito, so para esta medicao.
    esp_task_wdt_deinit();

    g_mutex = xSemaphoreCreateMutex();
    // mesmo nucleo p/ maximizar preempcoes visiveis
    xTaskCreatePinnedToCore(incrementador, "T1", 2048, "T1", 3, NULL, 1);
    xTaskCreatePinnedToCore(incrementador, "T2", 2048, "T2", 3, NULL, 1);

#if USAR_LCD
    // prioridade baixa e de leitura apenas: nao disputa CPU com T1/T2, so observa.
    xTaskCreate(tarefa_lcd, "lcd", 2560, NULL, 1, NULL);
#endif
}
