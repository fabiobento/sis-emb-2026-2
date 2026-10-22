# Lab 9 — I²C na veia: scanner, MPU-6050 e display OLED

> **Antes de começar**: leia a [teoria-09](teoria-09.md) — a Figura 9-K (transação I²C)
> explica o que o scanner provoca na linha, a seção 4 detalha o firmware de hoje linha a
> linha, e os Exemplos 9.2 (tempo de barramento), 9.3 (endereços, AD0) e 9.4 (pull-up) são
> as contas que você vai conferir com a bancada.

**Objetivo**: montar seu primeiro barramento I²C com **dois dispositivos nos mesmos dois
fios**; diagnosticar o barramento com um scanner (e aprender a "cara" de cada falha); ler o
MPU-6050 por **mapa de registradores**; calcular a inclinação; e fechar o projeto com um
**inclinômetro** que mostra o ângulo num display OLED — um sensor e um atuador de saída
conversando com o ESP32 pelo mesmo barramento.

**Duração**: 2 aulas (a Parte C pode terminar em casa).
**Material**: ESP32, módulo MPU-6050 (GY-521), display **OLED SSD1306 128×64 I²C** (o de 4
pinos), protoboard, jumpers; multímetro. **Sem resistores extras**: os módulos já trazem
pull-ups (Exemplo 9.4). As Partes A e B têm um passo no Wokwi para validar o código antes
do hardware; a Parte C e o experimento de falhas pedem a placa real.

> ⚠️ **Alimente TODOS os módulos com o pino 3V3 do ESP32 — nunca com 5 V.** Os pinos do
> ESP32 toleram no máximo 3,3 V, e nos módulos I²C os **resistores de pull-up vão ligados
> ao VCC do próprio módulo**: se você alimentar o GY-521 ou o OLED com 5 V, o pull-up
> arrasta SDA e SCL a 5 V **para dentro do GPIO21/22**. Os módulos aceitam 3,3 V sem
> problema, então não há motivo para arriscar. (Pelo mesmo motivo evitamos os displays LCD
> 16×2 com "mochila" PCF8574: quase todos são de 5 V.) E **confira a serigrafia**: a
> ordem dos pinos (VCC, GND, SCL, SDA) varia de fabricante para fabricante.

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — O barramento e o scanner (30 min)

### A.1 Entendendo o que vamos montar

Dois dispositivos, **os mesmos dois fios**. A Figura L9-A mostra o diagrama completo: SDA
(GPIO21) e SCL (GPIO22) correm em paralelo até os dois módulos; 3V3 e GND também. Cada
módulo traz seus próprios pull-ups (tracejados) e responde apenas ao **seu endereço**.

![Esquema do barramento: ESP32 com 3V3, GPIO21 (SDA), GPIO22 (SCL) e GND ligados em paralelo ao MPU-6050 (0x68) e ao OLED SSD1306 (0x3C), com os pull-ups de 4,7 kΩ tracejados](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab09_esquema_barramento.png)

*Figura L9-A — o barramento I²C da bancada. Tabela de ligações (confira com a figura antes
de energizar):*

| ESP32 | MPU-6050 (GY-521) | OLED SSD1306 |
|---|---|---|
| 3V3 | VCC | VCC |
| GND | GND | GND |
| GPIO21 | SDA | SDA |
| GPIO22 | SCL | SCL |

O MPU tem ainda os pinos XDA, XCL, AD0 e INT, que **ficam livres** hoje (o AD0 solto é lido
como 0 ⇒ endereço **0x68**; ligá-lo ao 3V3 mudaria para 0x69 — Exemplo 9.3).

> 🧠 **Onde cada dispositivo mora**: o mapa de endereços (Figura L9-B) mostra os 128
> endereços possíveis. O scanner varre de 0x03 a 0x77 (os demais são reservados pela
> norma) e vai "bater em todas as portas" procurando quem atende.

![Grade de 128 endereços I2C em 8 linhas de 16, com os reservados em cinza, o OLED em 0x3C/0x3D, o MPU-6050 em 0x68/0x69 e o LCD com PCF8574 em 0x27/0x3F](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab09_mapa_enderecos.png)

*Figura L9-B — o mapa dos 128 endereços I²C. Guarde: 0x68 (MPU) e 0x3C (OLED).*

### A.2 Wokwi primeiro

1. **Wokwi primeiro**: crie um projeto ESP32 (ESP-IDF) em [wokwi.com](https://wokwi.com) e
   substitua o `diagram.json` por este (MPU-6050 em SDA=GPIO21, SCL=GPIO22, alimentado
   pelo 3V3):

```json
{
  "version": 1,
  "author": "sua-bancada",
  "editor": "wokwi",
  "parts": [
    {
      "type": "board-esp32-devkit-c-v4",
      "id": "esp",
      "top": 0,
      "left": 0,
      "attrs": { "builder": "esp-idf" }
    },
    {
      "type": "wokwi-mpu6050",
      "id": "imu",
      "top": 10,
      "left": -170,
      "attrs": {}
    }
  ],
  "connections": [
    [ "esp:TX", "$serialMonitor:RX", "", [] ],
    [ "esp:RX", "$serialMonitor:TX", "", [] ],
    [ "imu:VCC", "esp:3V3", "red", [] ],
    [ "imu:GND", "esp:GND.1", "black", [] ],
    [ "imu:SDA", "esp:21", "orange", [] ],
    [ "imu:SCL", "esp:22", "blue", [] ]
  ],
  "dependencies": {}
}
```

> ℹ️ Se o Wokwi reclamar de algum nome de pino ao carregar, abra o componente (clique nele)
> e confira os nomes na própria peça; a ideia é só **VCC→3V3, GND→GND, SDA→21, SCL→22**.

2. No terminal integrado, crie o projeto do scanner e copie o firmware do repositório:

```bash
mkdir -p ~/sis-emb/lab9/i2c_scanner
cd ~/sis-emb/lab9/i2c_scanner
idf.py create-project i2c_scanner
cd i2c_scanner
cp ~/sis-emb-2026-2/semana-09/src/i2c_scanner/main.c ~/sis-emb/lab9/i2c_scanner/i2c_scanner/main/i2c_scanner.c
```

3. Abra o arquivo **com a teoria do lado** (Figura 9-K). Código-fonte completo:

```c
// Semana 9 — scanner I2C: varre 0x03-0x77 e lista quem responde ACK
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <stdio.h>

#define SDA 21
#define SCL 22

void app_main(void)
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER, .sda_io_num = SDA, .scl_io_num = SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000 };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    printf("varredura I2C...\n");
    for (uint8_t a = 0x03; a <= 0x77; a++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (a << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t r = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(20));
        i2c_cmd_link_delete(cmd);
        if (r == ESP_OK) printf("  dispositivo em 0x%02X\n", a);
    }
    printf("fim.\n");
}
```

   Leia antes de rodar — são só quatro ideias:
   - `i2c_param_config` + `i2c_driver_install`: ligam o periférico I²C0 do ESP32 como
     **mestre**, em GPIO21/22, a **100 kHz** (velocidade conservadora: é um diagnóstico) e
     com os pull-ups internos habilitados (reforço fraco, ~45 kΩ; quem de fato "puxa" são
     os pull-ups do módulo).
   - O laço `for (a = 0x03; a <= 0x77; a++)` percorre os endereços de 7 bits.
   - `(a << 1) | I2C_MASTER_WRITE` monta o **primeiro byte da transação**: os 7 bits do
     endereço + o bit R/W̄ no menos significativo (0 = escrita). É o byte que você vê na
     Figura L9-C: `0x68` vira `0xD0`.
   - `i2c_master_cmd_begin(...) == ESP_OK` só acontece quando alguém deu **ACK** no 9º bit.

4. Compile e rode no Wokwi (`idf.py build` e carregue o binário, como nos labs anteriores).
   Saída esperada:

```
varredura I2C...
  dispositivo em 0x68
fim.
```

### A.3 No hardware

5. Monte o circuito **da Figura L9-A só com o MPU-6050** (ainda sem o OLED), com o ESP32
   **desligado do USB** durante a fiação. Confira a tabela de ligações, ligue o USB e
   regrave:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

   Apareceu o `0x68`? Então o barramento está vivo (se o LED do GY-521 acendeu e o scanner
   mostrou o endereço, você já passou pelos 3 maiores filtros: energia, fiação e
   endereçamento).

6. **O que o scanner provoca na linha.** Para cada endereço, o mestre gera START, os 8 bits
   (7 de endereço + W̄) e fica **escutando o 9º pulso de clock**: se algum escravo puxar SDA
   para 0, é ACK; se SDA ficar em 1 (o pull-up vence), é NACK. A Figura L9-C mostra os dois
   casos lado a lado.

![Duas formas de onda de SCL e SDA: acima, o endereço 0x68 com ACK no nono bit (SDA em zero) e STOP; abaixo, o endereço 0x69 sem ninguém respondendo (SDA permanece em um) e o scanner seguindo adiante](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab09_scanner_ack_nack.png)

*Figura L9-C — o scanner em duas formas de onda. Nenhum dado é lido: é um censo de "quem
está em casa", endereço por endereço.*

> 🔬 **Se houver osciloscópio na bancada**: sonde SCL e SDA durante o scanner. A cada
> endereço você verá o trem de 9 pulsos; no endereço 0x68, a linha SDA fica **baixa** no 9º
> pulso (ACK). Fotografe para o relatório.

### A.4 Diagnóstico proposital — aprenda a "cara" de cada falha

7. Com tudo funcionando, **provoque** as falhas abaixo, rode o scanner a cada uma e
   **anote** o que aparece (como nos Labs 4 e 5, aprender a diagnosticar é metade do lab):
   - **troque SDA↔SCL** (GPIO21 no SCL do módulo e vice-versa) → esperado: **nenhum**
     dispositivo;
   - **desligue só o GND do módulo** (deixe o VCC) → nada, ou endereços "fantasmas"
     aleatórios (o módulo fica sem referência e a linha flutua);
   - religue tudo certo e confirme o `0x68` de volta.

   > 90 % dos "meu I²C não funciona" do semestre morrem neste passo. A Figura L9-D resume o
   > raciocínio: sempre do mais barato (energia, fio) ao mais caro (trocar o módulo).

![Fluxograma de diagnóstico do scanner vazio: verificar 3V3 e GND, depois SDA e SCL nos GPIOs corretos, depois pinos corretos do módulo (SDA/SCL e não XDA/XCL), por fim linha travada em zero](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab09_diagnostico_scanner.png)

*Figura L9-D — "o scanner não achou nada": a árvore de diagnóstico.*

8. **Agora o segundo dispositivo.** Desligue o USB e ligue o **OLED nos MESMOS pinos**: VCC
   no 3V3, GND no GND, SDA no GPIO21, SCL no GPIO22 — **em paralelo** com o MPU (na
   protoboard, use as mesmas trilhas; não leve fios novos até o ESP32). Rode o scanner
   de novo. Esperado: **dois** endereços:

```
varredura I2C...
  dispositivo em 0x3C
  dispositivo em 0x68
fim.
```

   Anote os **seus**: o OLED costuma ser 0x3C, mas alguns módulos usam 0x3D. Dois chips, dois
   fios: o superpoder do I²C (Exemplo 9.3). **O endereço é a única "identidade" de cada
   um** — por isso o scanner é o ping do barramento.

   *(Wokwi, opcional)*: para simular os dois, acrescente ao `diagram.json` a peça
   `{ "type": "board-ssd1306", "id": "oled", "top": 10, "left": 150, "attrs": {} }` e as
   conexões `["oled:VCC","esp:3V3","red",[]]`, `["oled:GND","esp:GND.1","black",[]]`,
   `["oled:SDA","esp:21","orange",[]]`, `["oled:SCL","esp:22","blue",[]]`.

> 🧠 **Por que o scanner funciona**: ele envia só START + endereço + W̄ e verifica se
> **alguém puxou SDA para 0 no 9º bit** (o ACK da Figura 9-K da teoria). É a mesma
> mecânica de qualquer transação — o scanner apenas **não faz nada depois** do ACK.

## Parte B — MPU-6050: identidade e dados (40 min)

### B.1 Mapa de registradores

O MPU-6050 não "manda dados sozinho": ele é uma **tabela de registradores** com endereços
de 8 bits. Para ler, o mestre diz **qual** registrador quer (escrita do endereço do
registrador) e, com um **START repetido**, passa a ler os bytes. Os registradores que usaremos:

| Registrador | Endereço | Para quê | Valor que usaremos |
|---|---|---|---|
| `WHO_AM_I` | 0x75 | "aperto de mão": devolve a identidade do chip | lê **0x68** |
| `PWR_MGMT_1` | 0x6B | gerência de energia; o chip **liga em sleep** | escreva **0x00** (acorda) |
| `ACCEL_CONFIG` | 0x1C | fundo de escala do acelerômetro | 0x00 = ±2 g, **0x08 = ±4 g** |
| `ACCEL_XOUT_H` … `GYRO_ZOUT_L` | 0x3B … 0x48 | 14 bytes: ax, ay, az, temp, gx, gy, gz | **lidos de uma vez** |

A Figura L9-E mostra a transação de leitura em rajada e o "layout" dos 14 bytes. Repare em
duas coisas: (i) **cada grandeza ocupa 2 bytes, byte ALTO primeiro** (big-endian) — por
isso a montagem `(b[0] << 8) | b[1]`; (ii) como os registradores são consecutivos, **um único
pedido** traz tudo, e todas as medidas pertencem ao mesmo instante.

![Linha do tempo da transação I2C de leitura: START, 0xD0, registrador 0x3B, START repetido, 0xD1, 14 bytes lidos, STOP; abaixo, os 14 bytes divididos em ax, ay, az, temperatura, gx, gy e gz, cada um com byte alto e byte baixo](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab09_leitura_14bytes.png)

*Figura L9-E — ler 14 bytes do MPU-6050 em uma só transação, e como decodificá-los.*

### B.2 O firmware

9. Crie o projeto e copie o firmware (o OLED **pode ficar** no barramento: o MPU está em
   0x68 e o OLED em outro endereço, não há conflito):

```bash
mkdir -p ~/sis-emb/lab9/mpu6050
cd ~/sis-emb/lab9/mpu6050
idf.py create-project mpu6050
cd mpu6050
cp ~/sis-emb-2026-2/semana-09/src/mpu6050/main.c ~/sis-emb/lab9/mpu6050/mpu6050/main/mpu6050.c
```

10. Código-fonte completo (leia junto com a teoria, seções 4.2 e 4.6):

```c
// Semana 9 — MPU-6050 via I2C: WHO_AM_I + leitura de acel/giro + angulo
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <stdio.h>
#include <math.h>

#define SDA 21
#define SCL 22
#define ADDR 0x68            // AD0=GND
#define REG_PWR    0x6B
#define REG_WHOAMI 0x75
#define REG_ACCEL  0x3B      // 14 bytes: ax..az, temp, gx..gz

static esp_err_t wr(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_write_to_device(I2C_NUM_0, ADDR, b, 2, pdMS_TO_TICKS(50));
}
static esp_err_t rd(uint8_t reg, uint8_t *dst, size_t n)
{
    return i2c_master_write_read_device(I2C_NUM_0, ADDR, &reg, 1, dst, n,
                                        pdMS_TO_TICKS(50));
}

void app_main(void)
{
    i2c_config_t cfg = { .mode = I2C_MODE_MASTER, .sda_io_num = SDA,
        .scl_io_num = SCL, .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE, .master.clk_speed = 400000 };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    uint8_t who = 0;
    rd(REG_WHOAMI, &who, 1);
    printf("WHO_AM_I = 0x%02X (esperado 0x68)\n", who);
    wr(REG_PWR, 0x00);                       // sai do sleep

    while (1) {
        uint8_t b[14];
        rd(REG_ACCEL, b, 14);
        int16_t ax = (b[0]<<8)|b[1], ay = (b[2]<<8)|b[3], az = (b[4]<<8)|b[5];
        int16_t gx = (b[8]<<8)|b[9];
        // fundo de escala padrao: +-2 g -> 16384 LSB/g ; +-250 dps -> 131 LSB/(o/s)
        float axg = ax/16384.f, ayg = ay/16384.f, azg = az/16384.f;
        float theta = atan2f(axg, azg) * 180.f / M_PI;   // inclinacao no plano XZ
        printf("a=(%.2f,%.2f,%.2f) g  gx=%.1f o/s  theta=%.1f o\n",
               axg, ayg, azg, gx/131.f, theta);
        vTaskDelay(pdMS_TO_TICKS(100));
        // O display OLED entra na Parte C (src/inclinometro_oled): mesmas wr()/rd(), endereco 0x3C.
    }
}
```

    Pontos de atenção:
    - `wr(reg, val)` envia **2 bytes** (registrador + valor): é como se escreve num registrador.
    - `rd(reg, dst, n)` usa `i2c_master_write_read_device`: envia o endereço do registrador,
      faz o **START repetido** e lê `n` bytes (a Figura L9-E).
    - `wr(REG_PWR, 0x00)` **tira o chip do sleep**. Sem isso, o MPU responde ao WHO_AM_I mas
      os registradores de medida ficam **zerados** (um erro clássico).
    - `atan2f(axg, azg)` é a inclinação no plano XZ (Figura L9-F). Use `atan2f`, não
      `atanf(ax/az)`: o `atan2` enxerga os **sinais** e cobre os 360°.

11. Compile e grave. Primeira linha esperada:

```
WHO_AM_I = 0x68 (esperado 0x68)
```

    O WHO_AM_I é o "aperto de mão": se vier **0x00 ou lixo**, volte à Figura L9-D.
12. Fluxo contínuo esperado (10 Hz), com a placa **parada e deitada**:

```
a=(0.02,-0.01,0.99) g  gx=0.3 o/s  theta=1.2 o
```

### B.3 Verificações físicas

13. **Gravidade como referência grátis**: em repouso, o acelerômetro mede só a gravidade;
    o que muda com a inclinação é como o vetor **g** se reparte entre os eixos (Figura
    L9-F). Incline a placa (sem sacudir) e preencha:

![Três posições da placa: deitada com chip para cima dando ax,ay,az de 0,0,1 g e theta zero; em pé com eixo X para cima dando 1,0,0 g e theta de mais 90 graus; de cabeça para baixo dando 0,0,menos 1 g e theta de mais ou menos 180 graus](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab09_orientacoes_theta.png)

*Figura L9-F — o acelerômetro mede a gravidade: o vetor g projetado nos eixos da placa dá
(ax, ay, az) e o ângulo θ.*

| posição da placa | (ax, ay, az) esperado | theta esperado | medido |
|---|---|---|---|
| deitada, chip p/ cima | (0, 0, +1) g | ~0° | |
| em pé, eixo X p/ cima | (+1, 0, 0) g | ~+90° | |
| em pé, eixo X p/ baixo | (−1, 0, 0) g | ~−90° | |
| de cabeça p/ baixo | (0, 0, −1) g | ~±180° | |

    > Qual lado do GY-521 é o "+X"? Procure a seta de eixos na serigrafia da placa. Se o
    > seu θ der o sinal trocado, não é erro: é a **convenção do eixo** (anote no relatório).

14. A soma vetorial √(ax²+ay²+az²) dá ~1,00 g em repouso, **em qualquer posição**? É o
    teste de sanidade de todo acelerômetro. Calcule-a em três posições (com a calculadora
    ou acrescentando uma linha ao `printf`) e registre. Diferenças de 0,02–0,05 g são
    normais (offset de fábrica sem calibração).

15. **Sacuda** a placa: os valores estouram ±2 g? É a **saturação do fundo de escala** —
    como a saturação do ADC do Lab 7 (o sinal "bate no teto" e achata). O registrador
    `ACCEL_CONFIG (0x1C)` escolhe ±2/4/8/16 g. Faça:
    - acrescente, logo após o `wr(REG_PWR, 0x00)`, a linha `wr(0x1C, 0x08);` (±4 g);
    - troque `16384.f` por `8192.f` (a **escala mudou**: LSB/g caiu pela metade);
    - comprove que o mesmo sacolejo não satura mais e que a placa parada continua dando
      ~1 g. **Se esquecer de trocar a escala, a placa parada vai marcar ~0,5 g** — um bom
      jeito de "ver" o efeito da sensibilidade. Documente a mudança.

16. **Endianness ao vivo** (semana 3 cobrando ingresso): monte um dos int16 na ordem errada,
    p.ex. `ax = (b[1]<<8)|b[0]`. O que acontece com os valores em repouso? (Lixo com
    "saltos": às vezes plausível, às vezes absurdo, porque o byte BAIXO oscila muito mais
    que o ALTO.) Registre um print e desfaça.

## Parte C — Dois no barramento: inclinômetro com display OLED (40 min)

### C.1 O que o display precisa que a gente faça

O SSD1306 **não é um display de texto**: é uma memória de pixels. Cada transação I²C
começa com um **byte de controle** que diz se o que vem depois é **comando** (0x00) ou
**pixels** (0x40) — Figura L9-H. A memória interna é organizada em **8 páginas × 128
colunas**: cada byte pinta **8 pixels de uma coluna**, com o **bit 0 no topo** (Figura
L9-G). O firmware mantém uma cópia local (`fb[1024]`, o *frame buffer*), desenha nela com
`px()`/`rect()`/`texto()` e envia o quadro inteiro com `oled_flush()`.

![Memória do SSD1306: 8 páginas horizontais de 128 colunas; ao lado, o detalhe de um byte mostrando os oito bits como oito pixels de uma coluna, com o bit 0 no topo; fórmula do endereço fb[(y/8)*128+x]](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab09_oled_framebuffer.png)

*Figura L9-G — o SSD1306 por dentro: o pixel (x, y) vive no bit (y % 8) do byte
`fb[(y/8)*128 + x]`.*

![Três diagramas de transação I2C do display: um comando com byte de controle 0x00; uma página de 128 bytes de pixel com byte de controle 0x40; e a janela de escrita com os comandos 0x21 e 0x22](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab09_oled_protocolo.png)

*Figura L9-H — falando com o SSD1306: o byte de controle decide "comando" ou "pixel".*

> 📦 **Por que escrevemos o driver do display no próprio firmware?** O componente
> `espressif/ssd1306` do registry muda de API entre versões e **depende de baixar da
> internet no `idf.py build`**. Para você *ver* o mecanismo (e ele funcionar em qualquer
> bancada), o driver mínimo está dentro do `main.c` (≈ 40 linhas). Em projetos reais,
> use uma biblioteca madura (LVGL, u8g2) — agora você sabe o que ela faz por baixo.

### C.2 Montagem e firmware

17. Confirme o circuito da **Figura L9-A completa** (MPU + OLED no mesmo barramento) e rode
    o scanner uma última vez: devem aparecer **0x3C e 0x68** (se o seu OLED for 0x3D,
    ajuste `OLED_ADDR` no firmware).
18. Crie o projeto e copie o firmware:

```bash
mkdir -p ~/sis-emb/lab9/inclinometro
cd ~/sis-emb/lab9/inclinometro
idf.py create-project inclinometro
cd inclinometro
cp ~/sis-emb-2026-2/semana-09/src/inclinometro_oled/main.c ~/sis-emb/lab9/inclinometro/inclinometro/main/inclinometro.c
```

19. Código-fonte completo:

```c
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
```

    Roteiro de leitura (ponha a Figura L9-G ao lado):
    - **`wr`/`rd`** agora recebem o **endereço como parâmetro**: as mesmas duas funções
      falam com o MPU (0x68) e com o OLED (0x3C). Isso é o I²C: o "quem" é só um argumento.
    - **`oled_init`** é uma lista de comandos do datasheet. Os que mudam a imagem: `0x8D 0x14`
      liga a bomba de carga (sem ela o painel **não acende**), `0x20 0x00` escolhe o
      endereçamento horizontal (a coluna avança sozinha) e `0xA1 0xC8` orienta a imagem.
    - **`oled_flush`** manda 2 comandos de janela + **8 transações de 129 bytes** (1 de
      controle + 128 de pixels): Figura L9-H.
    - **`FONTE[]`** é uma fonte 5×7 minúscula: cada glifo são **5 colunas**, bit 0 = linha
      de cima (o mesmo layout da Figura L9-G, em miniatura!). `caractere(..., s)` amplia
      cada pixel `s` vezes.
    - **`app_main`**: `vTaskDelayUntil` cravada em 10 Hz; **média móvel M = 4** nos
      **eixos** `ax` e `az` (Lab 7 reciclado) e só **depois** o `atan2f` — filtrar o
      ângulo direto daria um salto de ±180° ao cruzar o ponto de "dobra"; o display é
      redesenhado a cada 2 leituras (**5 Hz**).

20. Compile e grave. O monitor mostra `theta=+12.3 o`; o display deve mostrar algo como a
    Figura L9-J. Requisitos mínimos (confira todos):
    - o valor de θ com **1 casa decimal** e sinal (`+12.3`);
    - **atualização fluida**, sem tremedeira (a média móvel resolve);
    - uma indicação do **lado** da inclinação (`>>>>>`, `<<<<<` ou `=====` quando |θ| ≤ 2°);
    - a **barra** de −90° a +90° com o cursor andando.

![Três quadros do display de 128 por 64 pixels: theta de mais 12,3 graus com setas para a direita, theta de menos 47,8 graus com setas para a esquerda e theta de mais 0,4 grau com sinais de igual, cada um com a barra e o cursor na posição correspondente](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab09_oled_mockup.png)

*Figura L9-J — os quadros que o firmware desenha para três inclinações (gerados pelo
próprio código, rodado num simulador de PC; no OLED real o contraste e a cor do pixel
dependem do módulo — branco, azul ou amarelo).*

21. **Teste de aceitação**: incline a placa até +90°, −90° e 0°; confira a seta, o sinal e o
    cursor. Depois, **tire o filtro** (`#define M 1`) e observe a diferença na estabilidade
    do número; recoloque `M 4`.

> ⚠️ **Display apagado ou com "neve"?** Revise na ordem: (1) endereço (0x3C × 0x3D, o
> scanner diz); (2) **bomba de carga** (`0x8D 0x14` no `oled_init`); (3) se for um módulo
> **SH1106** (1,3″) em vez de SSD1306, a imagem aparece **deslocada 2 colunas** e com
> lixo nas bordas — compre/peça o módulo SSD1306 128×64.

### C.3 O orçamento de tempo do barramento

22. **Pergunta de projeto** (para o relatório): o MPU e o display dividem o barramento a
    400 kHz. Estime com o método do **Exemplo 9.2** (t = n_bytes × 9 bits ÷ f_SCL; os 9 bits
    são os 8 de dado + o ACK) o tempo de:
    1. **ler 14 bytes do MPU** (conte: endereço+W, registrador, endereço+R e os 14 dados);
    2. **enviar um quadro de ~1 kB ao OLED** (conte: 8 × (endereço + controle + 128) mais os
       comandos de janela).

    Compare com o período de 200 ms (5 Hz) e responda: sobra tempo de sobra? Em que cenário
    o barramento viraria gargalo? (Resposta de referência na Figura L9-I; calcule **antes**
    de olhar.)

![Barras comparando 100 kHz e 400 kHz: leitura do MPU em 1,5 e 0,4 milissegundos, quadro do OLED em 94,7 e 23,7 milissegundos; e uma barra de 200 milissegundos com cerca de 88 por cento livres a 400 kHz](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab09_orcamento_tempo.png)

*Figura L9-I — orçamento de tempo do barramento: a leitura do MPU é quase de graça; o
quadro do display é o que pesa (e a 100 kHz seria 4× pior).*

23. **Experimento** (opcional, 10 min): mude `.master.clk_speed` para `100000`, grave e
    compare a fluidez. Em seguida troque o período de redesenho para **a cada leitura**
    (`n % 2` → sempre) e veja se o laço de 10 Hz ainda fecha (o quadro de ~95 ms a 100 kHz
    deixaria pouca folga). Registre o que viu.

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| Scanner vazio | SDA/SCL trocados, GND solto ou módulo sem 3V3 | a árvore da Figura L9-D / item 7 |
| Scanner acha endereços "aleatórios" | GND flutuando ou fio frouxo | refaça GND; troque o jumper |
| Scanner acha 0x68 mas não 0x3C | OLED mal ligado, ou endereço 0x3D | confira VCC/GND do OLED; olhe 0x3D na lista |
| WHO_AM_I = 0x00 | sensor não alimentado / endereço errado | AD0 define 0x68/0x69 |
| WHO_AM_I certo, mas ax/ay/az sempre 0 | faltou tirar do sleep | `wr(0x6B, 0x00)` antes do laço |
| Valores congelados | sensor desconectou no meio | reconecte; cheque os jumpers |
| θ com sinal trocado | convenção do eixo X do módulo | troque o sinal de ax (e anote) |
| Display mudo / escuro | endereço, bomba de carga ou 5 V indevido | itens 17 e a caixa ⚠️ da Parte C |
| Display "deslocado 2 colunas" | módulo SH1106, não SSD1306 | use módulo SSD1306 |
| Imagem de cabeça para baixo | `0xA1 0xC8` do `oled_init` | troque por `0xA0 0xC0` |
| Texto ilegível, "pixels bagunçados" | bit 0 não é o topo na sua conta | revise `px()` e a Figura L9-G |
| `i2c_master_cmd_begin` devolve timeout | linha travada em 0 (curto/5 V) | desligue e meça SDA/SCL em relação ao GND |
| `idf.py` não acha `driver/i2c.h` | versão antiga do ESP-IDF | use ESP-IDF 5.x (o mesmo dos labs anteriores) |

## Entrega (GitHub da bancada, `lab-09/relatorio.md`)

1. Prints do scanner: **saudável** (1 endereço, depois 2) e **das duas falhas provocadas**
   (item 7), com uma linha de diagnóstico para cada.
2. Tabela de verificação física do MPU (item 13) + o teste √(ax²+ay²+az²) (item 14).
3. Registro do experimento de fundo de escala (item 15) e do endianness invertido
   (item 16).
4. Foto/vídeo do **inclinômetro funcionando** (item 20/21), com o θ visível.
5. As estimativas de tempo da questão 22 (com a conta) e uma frase sobre quando o
   barramento viraria gargalo.
6. Duas linhas: por que os módulos já vêm com pull-up e o que acontece com a resistência
   equivalente quando você põe dois módulos no mesmo barramento? (Exemplo 9.4: pull-ups em
   **paralelo**.)

## Desafio (opcional)

**Nível de bolha digital**: use ax **e** ay para desenhar no OLED uma "bolha" (círculo) que
desliza conforme a inclinação em 2D, como um nível de pedreiro. Dicas: mapeie ax, ay ∈
[−0,5; +0,5] g para o raio do campo (±30 px do centro do display); desenhe o círculo com
`px()` varrendo ângulos de 0 a 2π; **bônus** se a bolha tiver inércia (filtro passa-baixas
de 1ª ordem nas duas coordenadas: `x += a*(alvo - x)`, com `a ≈ 0,2`).
