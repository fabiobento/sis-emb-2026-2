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

> **Onde cada dispositivo mora**: o mapa de endereços (Figura L9-B) mostra os 128
> endereços possíveis. O scanner varre de 0x03 a 0x77 (os demais são reservados pela
> norma) e vai "bater em todas as portas" procurando quem atende.

![Grade de 128 endereços I2C em 8 linhas de 16, com os reservados em cinza, o OLED em 0x3C/0x3D, o MPU-6050 em 0x68/0x69 e o LCD com PCF8574 em 0x27/0x3F](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab09_mapa_enderecos.png)

*Figura L9-B — o mapa dos 128 endereços I²C. Guarde: 0x68 (MPU) e 0x3C (OLED).*

### A.2 Wokwi primeiro

1. **Wokwi primeiro**: crie um projeto ESP32 (ESP-IDF) no [Wokwi](https://wokwi.com/projects/479066590644966401) um circuito com o seguinte `diagram.json`:

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
    { "type": "wokwi-mpu6050", "id": "imu", "top": 167.02, "left": -151.28, "attrs": {} },
    { "type": "wokwi-logic-analyzer", "id": "logic1", "top": 9.95, "left": -278.4, "attrs": {} },
    {
      "type": "board-ssd1306",
      "id": "oled1",
      "top": 281.54,
      "left": -76.57,
      "attrs": { "i2cAddress": "0x3c" }
    }
  ],
  "connections": [
    [ "esp:TX", "$serialMonitor:RX", "", [] ],
    [ "esp:RX", "$serialMonitor:TX", "", [] ],
    [ "imu:VCC", "esp:3V3", "red", [ "h0.08", "v-144" ] ],
    [ "imu:GND", "esp:GND.1", "black", [ "v-19.2", "h57.68", "v182.4" ] ],
    [ "imu:SDA", "esp:21", "orange", [ "v-182.4", "h230.48", "v105.6" ] ],
    [ "imu:SCL", "esp:22", "blue", [ "v-192", "h230.48", "v86.4" ] ],
    [ "logic1:GND", "esp:GND.1", "black", [ "h-28.8", "v144", "h268.8", "v-86.4" ] ],
    [ "logic1:D0", "imu:SCL", "blue", [ "h-19.2", "v-38.4", "h201.52" ] ],
    [ "logic1:D1", "imu:SDA", "orange", [ "h-38.4", "v-67.2", "h211.12" ] ],
    [ "oled1:GND", "esp:GND.1", "black", [ "v-134.4" ] ],
    [ "oled1:VCC", "esp:3V3", "red", [ "v0" ] ],
    [ "oled1:SCL", "esp:22", "blue", [ "v-48", "h153.9", "v-201.6" ] ],
    [ "oled1:SDA", "esp:21", "orange", [ "v-57.6", "h134.47", "v-163.2" ] ]
  ],
  "dependencies": {}
}
```

> ℹ️ Se o Wokwi reclamar de algum nome de pino ao carregar, abra o componente (clique nele)
> e confira os nomes na própria peça; a ideia é só **VCC→3V3, GND→GND, SDA→21, SCL→22**.

2. No terminal integrado, crie o projeto do scanner e copie o firmware do repositório:

```bash
mkdir -p ~/sis-emb/lab9/
cd ~/sis-emb/lab9/
idf.py create-project i2c_scanner
cd i2c_scanner
cp ~/sis-emb-2026-2/semana-09/src/i2c_scanner/main.c ~/sis-emb/lab9/i2c_scanner/main/i2c_scanner.c
```

3. Abra o arquivo **com a teoria do lado** (Figura 9-K). Código-fonte completo:

```c
// Semana 9 — scanner I2C: varre 0x03-0x77 e lista quem responde ACK
// -----------------------------------------------------------------------------
// PRÁTICA: O barramento I2C permite ligar dezenas de sensores usando apenas 2 fios.
// OBJETIVO: Este código é uma ferramenta de diagnóstico. Ele tenta se comunicar 
//           com todos os endereços I2C possíveis. Se algum componente físico estiver
//           conectado corretamente e funcionando, ele responderá com um sinal de 
//           reconhecimento (ACK - Acknowledge) e o scanner imprimirá seu endereço.
// -----------------------------------------------------------------------------

#include "freertos/FreeRTOS.h"  // Core do sistema operacional de tempo real
#include "freertos/task.h"      // Necessário para as macros de tempo (pdMS_TO_TICKS)
#include "driver/i2c.h"         // Driver padrão de I2C do ESP-IDF
#include <stdio.h>              // Funções de entrada e saída (printf)

// Define os pinos padrão do I2C no ESP32 genérico
// DICA: No ESP32, você pode rotear o I2C para praticamente qualquer pino digital.
#define SDA 21                  // Serial Data (Linha de Dados)
#define SCL 22                  // Serial Clock (Linha de Clock)

void app_main(void)
{
    // =========================================================================
    // 1. CONFIGURAÇÃO DO HARDWARE I2C (Modo Master)
    // =========================================================================
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,             // O ESP32 será o Mestre (quem gera o clock)
        .sda_io_num = SDA,                   // Pino de dados
        .scl_io_num = SCL,                   // Pino de clock
        .sda_pullup_en = GPIO_PULLUP_ENABLE, // Habilita o resistor de pull-up interno do ESP32 para o SDA
        .scl_pullup_en = GPIO_PULLUP_ENABLE, // Habilita o resistor de pull-up interno do ESP32 para o SCL
        .master.clk_speed = 100000           // Frequência do clock: 100 kHz (Standard Mode)
    };
    
    // Aplica as configurações ao controlador I2C número 0 (o ESP32 possui dois: I2C_NUM_0 e I2C_NUM_1)
    i2c_param_config(I2C_NUM_0, &cfg);
    
    // Instala o driver no sistema operacional alocando recursos (sem buffers para o Master)
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    printf("varredura I2C...\n");
    
    // =========================================================================
    // 2. LOOP DE VARREDURA DE ENDEREÇOS
    // =========================================================================
    // O I2C padrão usa endereços de 7 bits (0x00 a 0x7F).
    // Pulamos os endereços 0x00-0x02 e 0x78-0x7F porque, no protocolo I2C, 
    // eles são reservados para funções especiais (ex: General Call, 10-bit addressing).
    for (uint8_t a = 0x03; a <= 0x77; a++) {
        
        // No ESP-IDF, as operações I2C são montadas em uma "fila de comandos" (link)
        // antes de serem enviadas efetivamente para o hardware.
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        
        // Adiciona à fila o bit de START (início da comunicação)
        i2c_master_start(cmd);
        
        // Adiciona à fila o byte de endereço. 
        // MATEMÁTICA DO I2C: O endereço real tem 7 bits. Ele deve ser deslocado 1 casa para a esquerda (a << 1).
        // O bit menos significativo (LSB) que sobra indica a operação: 0 para Write, 1 para Read.
        // O 'true' no terceiro argumento significa que exigimos que o escravo responda com um bit de ACK.
        i2c_master_write_byte(cmd, (a << 1) | I2C_MASTER_WRITE, true);
        
        // Adiciona à fila o bit de STOP (fim da comunicação)
        i2c_master_stop(cmd);
        
        // =========================================================================
        // 3. EXECUÇÃO DA FILA DE COMANDOS
        // =========================================================================
        // Dispara a comunicação fisicamente no barramento e aguarda até 20ms pela resposta
        esp_err_t r = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(20));
        
        // Deleta a fila de comandos para liberar a memória alocada, prevenindo memory leak
        i2c_cmd_link_delete(cmd);
        
        // =========================================================================
        // 4. ANÁLISE DO RESULTADO
        // =========================================================================
        // Se a função retornou ESP_OK, significa que ocorreu um ACK! 
        // Ou seja, existe um dispositivo físico nesse endereço que escutou o chamado e respondeu.
        if (r == ESP_OK) {
            // Imprime o endereço em formato hexadecimal (ex: 0x27, 0x68)
            printf("  dispositivo em 0x%02X\n", a);
        }
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
mkdir -p ~/sis-emb/lab9/
cd ~/sis-emb/lab9/
idf.py create-project mpu6050
cd mpu6050
cp ~/sis-emb-2026-2/semana-09/src/mpu6050/main.c ~/sis-emb/lab9/mpu6050/main/mpu6050.c
```

10. Código-fonte completo (leia junto com a teoria, seções 4.2 e 4.6):

```c
// Semana 9 — MPU-6050 via I2C: WHO_AM_I + leitura de acel/giro + angulo
// -----------------------------------------------------------------------------
// PRÁTICA: Comunicação I2C com o sensor inercial (IMU) MPU-6050.
// OBJETIVO: Configurar o I2C, acordar o sensor, ler os dados brutos (burst read)
//           de aceleração e rotação, converter para unidades físicas (g e graus/s)
//           e calcular a inclinação estática usando trigonometria básica.
// -----------------------------------------------------------------------------

#include "freertos/FreeRTOS.h"  // Core do RTOS
#include "freertos/task.h"      // Controle de tarefas (vTaskDelay)
#include "driver/i2c.h"         // Driver I2C do ESP-IDF
#include <stdio.h>              // Funções de I/O (printf)
#include <math.h>               // Funções matemáticas (atan2f, M_PI)

// =========================================================================
// DEFINIÇÕES E MAPA DE REGISTRADORES
// =========================================================================
#define SDA 21               // Pino de dados do barramento I2C
#define SCL 22               // Pino de clock do barramento I2C

#define ADDR 0x68            // Endereço I2C do MPU-6050 (quando o pino AD0 está no GND)
                             // Se AD0 estivesse no VCC, o endereço seria 0x69.

// Registradores internos do MPU-6050 (consultados no datasheet)
#define REG_PWR    0x6B      // Power Management 1: controla o modo de energia (sleep)
#define REG_WHOAMI 0x75      // Who Am I: contém a identidade do chip (deve retornar 0x68)
#define REG_ACCEL  0x3B      // Primeiro registrador de dados (Accel X High Byte)
                             // A partir daqui, são 14 bytes sequenciais: 
                             // ax (2), ay (2), az (2), temp (2), gx (2), gy (2), gz (2)

// =========================================================================
// FUNÇÕES AUXILIARES DE COMUNICAÇÃO I2C (Wrappers)
// =========================================================================
// O ESP-IDF possui funções de alto nível que simplificam a escrita/leitura.

// Escreve 1 byte (val) em um registrador específico (reg) do sensor
static esp_err_t wr(uint8_t reg, uint8_t val)
{
    // O pacote I2C para escrita precisa mandar o endereço do registrador seguido do valor
    uint8_t b[2] = { reg, val };
    
    // Função padrão do IDF v4.x+: Manda o buffer para o endereço do dispositivo (ADDR)
    return i2c_master_write_to_device(I2C_NUM_0, ADDR, b, 2, pdMS_TO_TICKS(50));
}

// Lê 'n' bytes a partir de um registrador inicial (reg) e guarda no buffer 'dst'
static esp_err_t rd(uint8_t reg, uint8_t *dst, size_t n)
{
    // Envia o endereço do registrador que queremos ler e já engata um START repetido
    // para receber os 'n' bytes em seguida, sem soltar o barramento.
    return i2c_master_write_read_device(I2C_NUM_0, ADDR, &reg, 1, dst, n,
                                        pdMS_TO_TICKS(50));
}

void app_main(void)
{
    // =========================================================================
    // 1. CONFIGURAÇÃO DO HARDWARE I2C
    // =========================================================================
    i2c_config_t cfg = { 
        .mode = I2C_MODE_MASTER, 
        .sda_io_num = SDA,
        .scl_io_num = SCL, 
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE, 
        .master.clk_speed = 400000 // Velocidade: 400 kHz (Fast Mode). 
                                   // O MPU-6050 suporta essa velocidade superior aos 100kHz padrão.
    };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    // =========================================================================
    // 2. INICIALIZAÇÃO DO SENSOR
    // =========================================================================
    uint8_t who = 0;
    
    // Lê o registrador de identidade. É uma boa prática para verificar se a 
    // solda e os fios estão OK antes de tentar configurar o chip.
    rd(REG_WHOAMI, &who, 1);
    printf("WHO_AM_I = 0x%02X (esperado 0x68)\n", who);
    
    // IMPORTANTE: O MPU-6050 liga em modo SLEEP por padrão para economizar bateria.
    // Escrever 0x00 no REG_PWR acorda o sensor e o coloca para usar o oscilador interno.
    wr(REG_PWR, 0x00);                       

    // =========================================================================
    // 3. LOOP PRINCIPAL DE LEITURA
    // =========================================================================
    while (1) {
        uint8_t b[14];
        
        // BURST READ: Lê os 14 bytes de dados de uma só vez.
        // É muito mais eficiente no barramento I2C ler tudo num pacote sequencial
        // do que fazer 14 requisições separadas.
        rd(REG_ACCEL, b, 14);
        
        // RECONSTRUÇÃO DOS DADOS (O MPU-6050 é Big-Endian)
        // Cada grandeza tem 16 bits (2 bytes). O byte mais significativo (MSB) vem primeiro.
        // Deslocamos o MSB 8 posições para a esquerda e fazemos uma operação OR (ou soma) com o LSB.
        int16_t ax = (b[0]<<8) | b[1];
        int16_t ay = (b[2]<<8) | b[3];
        int16_t az = (b[4]<<8) | b[5];
        // b[6] e b[7] seriam a temperatura. Pulamos direto para o Giroscópio (X):
        int16_t gx = (b[8]<<8) | b[9];
        
        // CONVERSÃO PARA UNIDADES FÍSICAS
        // fundo de escala padrao do chip (power-on reset): 
        // Acelerômetro: +-2 g -> Logo, 1 'g' equivale a 16384 LSB.
        // Giroscópio: +-250 dps (graus por segundo) -> Logo, 1 grau/s equivale a 131 LSB.
        float axg = ax / 16384.f;
        float ayg = ay / 16384.f;
        float azg = az / 16384.f;
        
        // CÁLCULO DE INCLINAÇÃO (TILT)
        // Usa a função arco-tangente de 2 argumentos (atan2) para descobrir o ângulo 
        // em relação à gravidade no plano XZ. atan2f resolve o problema do quadrante.
        // O resultado sai em radianos, então multiplicamos por (180/PI) para converter para graus.
        float theta = atan2f(axg, azg) * 180.f / M_PI;   
        
        // Imprime os valores físicos e o ângulo
        printf("a=(%.2f,%.2f,%.2f) g  gx=%.1f o/s  theta=%.1f o\n",
               axg, ayg, azg, gx/131.f, theta);
               
        // Atraso de 100ms (~10 Hz de atualização na tela)
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // NOTA PEDAGÓGICA PARA O ROTEIRO DO LABORATÓRIO:
        // O display OLED entra na Parte C (src/inclinometro_oled): mesmas wr()/rd(), endereco 0x3C.
        // Como o I2C é um barramento, o OLED usará os mesmos fios SDA/SCL, bastando 
        // endereçar os pacotes para 0x3C em vez de 0x68.
    }
}
```
> **Pontos de atenção:**
> 
>- `wr(reg, val)` envia **2 bytes** (registrador + valor): é como se escreve num registrador.
>- `rd(reg, dst, n)` usa `i2c_master_write_read_device`: envia o endereço do registrador,
    faz o **START repetido** e lê `n` bytes (a Figura L9-E).
>- `wr(REG_PWR, 0x00)` **tira o chip do sleep**. Sem isso, o MPU responde ao WHO_AM_I mas
    os registradores de medida ficam **zerados** (um erro clássico).
>- `atan2f(axg, azg)` é a inclinação no plano XZ (Figura L9-F). Use `atan2f`, não
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
mkdir -p ~/sis-emb/lab9/
cd ~/sis-emb/lab9/
idf.py create-project inclinometro
cd inclinometro
cp ~/sis-emb-2026-2/semana-09/src/inclinometro_oled/main.c ~/sis-emb/lab9/inclinometro/main/inclinometro.c
```

19. Código-fonte completo:

```c
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
```

> **Roteiro de leitura (ponha a Figura L9-G ao lado):**
>
>- **`wr`/`rd`** agora recebem o **endereço como parâmetro**: as mesmas duas funções
    falam com o MPU (0x68) e com o OLED (0x3C). Isso é o I²C: o "quem" é só um argumento.
>- **`oled_init`** é uma lista de comandos do datasheet. Os que mudam a imagem: `0x8D 0x14`
    liga a bomba de carga (sem ela o painel **não acende**), `0x20 0x00` escolhe o
    endereçamento horizontal (a coluna avança sozinha) e `0xA1 0xC8` orienta a imagem.
>- **`oled_flush`** manda 2 comandos de janela + **8 transações de 129 bytes** (1 de
    controle + 128 de pixels): Figura L9-H.
>- **`FONTE[]`** é uma fonte 5×7 minúscula: cada glifo são **5 colunas**, bit 0 = linha
    de cima (o mesmo layout da Figura L9-G, em miniatura!). `caractere(..., s)` amplia
    cada pixel `s` vezes.
>- **`app_main`**: `vTaskDelayUntil` cravada em 10 Hz; **média móvel M = 4** nos
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
