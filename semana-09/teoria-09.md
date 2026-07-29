# Aula 9 — Comunicação Serial: UART, SPI e I2C (U4)

> **Pré-requisito**: Aulas 3 (open-drain, endianness) e 4 (latência).
> **Como usar**: texto autossuficiente. Os Exemplos 9.1–9.4 são o modelo das questões 11–15
> da Lista 3. A seção 5 (tabela de decisão) é para saber de cor — ela é cobrada em toda
> prova de embarcados do mundo.

Seu ESP32 já sente (ADC) e age (PWM). Mas um sistema embarcado raramente vive só: ele
conversa com sensores inteligentes, displays, cartões de memória, GPS, com o PC e com outros
processadores. Esta aula apresenta os **três barramentos seriais** que cobrem 95 % dessas
conversas — UART, SPI e I2C — item explícito da ementa ("interfaces de comunicação serial e
paralela") e ferramenta de trabalho de todas as semanas restantes. A pergunta de engenheiro
não é "qual é o melhor?", e sim "**qual é o certo para este caso?**" — ao final, você terá a
tabela de decisão na cabeça. No laboratório, o MPU-6050 (acelerômetro + giroscópio) vira seu
primeiro sensor digital inteligente.

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) descrever o frame UART e calcular tempos de transmissão;
- (b) explicar SPI (fios, CS, modos CPOL/CPHA) e por que é o mais rápido;
- (c) explicar I2C (endereçamento, ACK, dreno aberto + pull-up) e ler o mapa de registradores
  de um sensor;
- (d) escolher o barramento certo para cada aplicação.

---

## 1. Por que serial (e não paralelo)?

Paralelo = um fio por bit: rápido no papel, caro em pinos, cabos e sincronismo (os bits
"desalinham" em frequências altas — cada fio tem um atraso ligeiramente diferente, e acima de
certa velocidade os bits de um mesmo byte chegam em tempos diferentes; é o *skew*). Serial =
os bits em fila num único par de fios, com pinos de sobra e alcance maior. A indústria migrou
quase tudo para serial (até o barramento do seu SSD é serial — SATA, PCIe). A distinção viva
hoje é **assíncrono** (sem fio de clock: UART) × **síncrono** (com clock explícito: SPI,
I2C).

## 2. UART: o veterano assíncrono

A **UART** liga dois dispositivos ponto a ponto com dois fios cruzados (TX→RX, RX←TX) e
**sem clock compartilhado**: os dois lados combinam previamente a velocidade (**baud rate**)
e se ressincronizam a cada byte pelo **start bit** (a queda de 1 para 0 que anuncia “vem byte
aí”). Como não há fio de clock, cada lado mede o tempo com o seu próprio oscilador — daí a
exigência de baud rates *iguais* e razoavelmente precisos (tolerância de ~2 % no total). O
frame clássico "8N1" (8 dados, sem paridade, 1 stop):

![Estrutura do frame UART: repouso, start bit, 8 bits de dados LSB-first, stop bit](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/uart_frame.png)

*Figura 9-A — O frame UART: sem clock no fio, o start bit é o único marco de sincronismo.
Dez bits trafegam para entregar oito — overhead de 25 %.*

```
 ocioso ─▔▔▔╲__┌d0┬d1┬d2┬d3┬d4┬d5┬d6┬d7┐▔▔─ ocioso
            start  (LSB primeiro)       stop
            = 10 bits no fio para 8 bits úteis (overhead de 25 %)
```

É a sua velha conhecida: o `printf` do monitor serial viaja numa UART (115 200 baud, 8N1)
desde o Lab 1.

**Exemplo resolvido 9.1 (o custo real do printf)** — A 115 200 baud, quanto tempo leva para
enviar um buffer de 512 bytes?

*Solução passo a passo.* Taxa útil = 115 200/10 = **11 520 bytes/s** (são 10 bits no fio por
byte útil no 8N1). Tempo: t = 512/11 520 ≈ **44,4 ms** — uma eternidade para um laço de
controle de 10 ms! Duas lições: (i) log exagerado *dentro* de tarefas rápidas destrói a
temporização (imprima pouco, ou de uma tarefa lenta dedicada — padrão que você verá em
firmware profissional); (ii) o driver de UART do ESP-IDF usa fila + interrupção justamente
para o `printf` não bloquear tanto — mas a física do fio ninguém revoga: bytes entram na
fila na velocidade do seu código e **saem** na velocidade do baud.

## 3. SPI: o velocista síncrono

O **SPI** adiciona o fio de clock (SCLK) — o mestre dita o ritmo e os dados fluem
**full-duplex** por dois fios de dados (MOSI: mestre→escravo; MISO: escravo→mestre). Cada
escravo ganha um fio de seleção **CS** (ativo-baixo): é o "endereço por fio" — em vez de
perguntar “quem é 0x68?”, o mestre acorda fisicamente um escravo de cada vez.

![Sinais de uma transação SPI: CS, clock, MOSI e MISO](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/spi_transacao.png)

*Figura 9-B — Transação SPI: CS cai, o clock corre, e em cada pulso um bit vai (MOSI) e um
bit volta (MISO) — full-duplex de verdade.*

```
        MESTRE                      escravo 1        escravo 2
        SCLK  ──────────┬──────────────┬─────────────────┐
        MOSI  ──────────┼──────────────┼─────────────────┤
        MISO  ──────────┼──────────────┼─────────────────┤
        CS0   ──────────┼──────────────┘                 │
        CS1   ──────────┴────────────────────────────────┘
```

Sem endereço no protocolo, sem ACK, sem overhead: SPI alcança **dezenas de MHz**. O preço:
3 + N fios para N escravos, e quatro "modos" a casar entre as partes — **CPOL** (nível
ocioso do clock) e **CPHA** (qual borda amostra o dado). Modo errado = dados
deslocados/invertidos, o clássico "funciona pela metade" — o datasheet do escravo sempre
diz o modo (MPU-6050, aliás, também fala SPI: modos 0 e 3).

**Exemplo resolvido 9.2 (SPI × I2C em números)** — Ler 6 bytes de um acelerômetro a
SCLK = 8 MHz.

*Solução passo a passo.* t = 6 × 8 bits / 8·10⁶ = **6 µs**. O mesmo pacote por I2C a 400 kHz
levaria ~ (endereço + registrador + 6 dados + ACKs ≈ 9 bytes × 9 bits)/400 k ≈ **200 µs** —
mais de 30× mais lento. Conclusão de projeto: fluxo **rápido e volumoso** (display gráfico,
cartão SD, ADC de alta taxa) ⇒ SPI.

## 4. I2C: o diplomata de dois fios

O **I2C** liga *muitos* dispositivos com **dois fios apenas** — SDA (dados) e SCL (clock) —
compartilhados por todos. Cada escravo tem um **endereço de 7 bits**; a transação começa com
START + endereço + bit R/W̄, e o escravo endereçado responde **ACK** (puxa SDA para 0 no 9º
bit). Velocidades: 100 kHz (standard) e 400 kHz (fast) — lento, mas dois fios para um
barramento inteiro de sensores é imbatível em fiação.

![Sequência de uma transação I2C: START, endereço de 7 bits, R/W, ACK, dados](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/i2c_transacao.png)

*Figura 9-C — O I2C bit a bit: o mestre fala o endereço, e só o escravo endereçado responde
ACK. Dois fios, até 127 moradores.*

O detalhe elétrico que explica tudo: as saídas I2C são **dreno aberto** (semana 3, Figura
3-E!) — os dispositivos só sabem *puxar a linha para 0*; quem devolve a linha a 1 é o
resistor de **pull-up**. Assim vários chips coexistem sem curto (todo mundo pode "falar 0" ao
mesmo tempo sem briga elétrica — o wired-AND), e um escravo lento pode até segurar SCL em 0
para pedir tempo (*clock stretching*).

**Exemplo resolvido 9.4 (dimensionando o pull-up)** — Barramento com capacitância total de
100 pF e alvo de tempo de subida t_r ≤ 300 ns (exigência do modo 400 kHz). Qual o R máximo?

*Solução.* Com t_r ≈ 0,3·R·C (fórmula empírica do padrão I2C): R ≤ 300 ns/(0,3 × 100 pF) =
**10 kΩ** no limite; na prática usa-se **2,2–4,7 kΩ** para folga (e note: os módulos
MPU-6050/OLED já trazem pull-ups a bordo — dois módulos no mesmo barramento = pull-ups em
paralelo, resistência ainda menor, tudo bem até certo ponto). Menor R = subida mais rápida,
porém mais corrente a cada nível 0 — o compromisso da questão 14 da Lista 3.

### 4.1 Conversando com um sensor: o mapa de registradores

Sensores I2C "inteligentes" (MPU-6050, BME280, RTC DS3231…) expõem um **mapa de
registradores** — a mesma ideia dos periféricos mapeados da semana 2, agora do outro lado do
fio: cada função do sensor (identidade, configuração, dados) mora num endereço interno. O
protocolo de leitura tem duas fases: *escreve* o número do registrador desejado, *lê* os
bytes a partir dele. É exatamente o par `wr()`/`rd()` do firmware de hoje:

```c
#define ADDR 0x68            // MPU-6050 com AD0=GND (0x69 com AD0=VCC)
#define REG_WHOAMI 0x75      // registrador de identidade: responde 0x68

static esp_err_t rd(uint8_t reg, uint8_t *dst, size_t n)
{   // fase 1: escreve o nº do registrador; fase 2: lê n bytes a partir dele
    return i2c_master_write_read_device(I2C_NUM_0, ADDR, &reg, 1, dst, n, pdMS_TO_TICKS(50));
}
```

**Exemplo resolvido 9.3 (dois sensores idênticos, um barramento)** — O MPU-6050 responde em
0x68 (pino AD0 = GND) ou 0x69 (AD0 = V_cc): **dois** sensores idênticos convivem no mesmo
barramento escolhendo endereços diferentes por hardware — e o display OLED (0x3C) entra de
carona nos mesmos dois fios. É o superpoder do I2C: o barramento vira uma "rede local de
sensores" dentro da placa. (Mais de dois MPU-6050? Aí é multiplexador I2C ou SPI — pergunta
de projeto da Lista 3.)

E o **scanner** do laboratório? Ele explora o ACK: para cada endereço de 0x03 a 0x77, gera
START + endereço e verifica se *alguém* respondeu ACK — sem ler dado nenhum. Quem acena,
existe:

```c
for (uint8_t a = 0x03; a <= 0x77; a++) {
    ...i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (a << 1) | I2C_MASTER_WRITE, true);  // endereço + W, exige ACK
    i2c_master_stop(cmd);
    if (i2c_master_cmd_begin(...) == ESP_OK) printf("  dispositivo em 0x%02X\n", a);
}
```

É o "ping" do mundo I2C — a primeira ferramenta de diagnóstico quando "o sensor não
responde" (90 % das vezes: SDA/SCL trocados ou GND faltando — os outros 10 %: pull-up e
endereço errado, nessa ordem).

### 4.2 Decodificando o MPU-6050

O firmware lê 14 bytes a partir de `REG_ACCEL (0x3B)`: ax, ay, az, temperatura, gx, gy, gz —
cada eixo em 16 bits **big-endian** (byte alto primeiro — a endianness da semana 3 cobrando
pedágio: por isso o `(b[0]<<8)|b[1]`). No fundo de escala padrão, ±2 g → 16 384 LSB/g e
±250 °/s → 131 LSB/(°/s). E a inclinação sai da gravidade por trigonometria:

```c
int16_t ax = (b[0]<<8)|b[1], az = (b[4]<<8)|b[5];       // big-endian → int16
float axg = ax/16384.f, azg = az/16384.f;               // LSB → g
float theta = atan2f(axg, azg) * 180.f / M_PI;          // inclinação no plano XZ
```

Com a placa parada, o vetor aceleração ≈ gravidade (o acelerômetro “sente” o chão empurrando
a placa para cima a 1 g); `atan2` do par (ax, az) devolve o ângulo de inclinação — seu
primeiro "sensor de atitude", base de projetos de estabilização (robôs balanceadores, gimbals).

> 💡 **Pense aí**: por que `atan2(ax, az)` e não `asin(ax)`? *O `atan2` usa os dois eixos e
> não explode quando o denominador passa por zero; `asin` perde resolução perto de ±90° e
> exige normalização. Em firmware de atitude, `atan2` é o padrão — e é grátis na math.h.*

## 5. A tabela de decisão

| | **UART** | **SPI** | **I2C** |
|---|---|---|---|
| Fios | 2 (TX/RX) | 3 + 1 CS por escravo | 2 (SDA/SCL) |
| Topologia | ponto a ponto | 1 mestre, N escravos | multiponto (127 end.) |
| Clock | não (baud combinado) | sim (mestre) | sim (mestre) |
| Velocidade típica | 9,6 k–3 Mbaud | 1–80 MHz | 100/400 kHz (1 MHz FM+) |
| Endereçamento | — | por fio (CS) | 7 bits no protocolo |
| Forte em | console, GPS, módulos | volume/velocidade | muitos sensores, 2 fios |
| No curso | monitor serial; RPi↔ESP32 | RC522 RFID, SD, MCP3008 | MPU-6050, OLED, LCD, RTC |

Casos-teste (questão 15 da Lista 3): GPS que "fala NMEA" ⇒ UART; display gráfico com refresh
alto ⇒ SPI; 5 sensores lentos na mesma placa ⇒ I2C. Se você acertou os três sem olhar a
tabela, a aula cumpriu a missão.

---

## Resumindo

- Serial venceu o paralelo (pinos, alcance, skew); a divisão viva é assíncrono (UART) ×
  síncrono (SPI/I2C).
- UART 8N1: 10 bits no fio por byte ⇒ taxa útil = baud/10; log demais mata temporização
  (Exemplo 9.1).
- SPI: clock do mestre, full-duplex, CS por escravo, modos CPOL/CPHA; o mais rápido
  (Exemplo 9.2: 6 µs × 200 µs do I2C).
- I2C: 2 fios, dreno aberto + pull-up (dimensionado por t_r ≈ 0,3RC — Exemplo 9.4), endereço
  de 7 bits + ACK; sensores com mapa de registradores lidos por "escreve reg, lê dados";
  dois MPU-6050 convivem via AD0 (Exemplo 9.3); o scanner é o ping do barramento.
- MPU-6050: 14 bytes big-endian; ±2 g = 16 384 LSB/g; inclinação por atan2 — endianness e
  trigonometria pagando o almoço.
- Escolha pelo caso: console⇒UART, volume⇒SPI, muitos sensores⇒I2C.

> 🔭 **Onde isto reaparece:** este mesmo MPU-6050 lido por I2C é a **fonte de dados** de um
> classificador de movimento embarcado. Na [trilha TinyML](../docs/trilha-tinyml.md) (opcional, para
> o projeto final), o `main.c` desta semana vira o coletor de amostras que alimenta o Edge Impulse —
> o acelerômetro que hoje mede inclinação passa a distinguir *parado / aceno / sobe-desce / círculo*.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| baud rate | bits por segundo na linha serial |
| 8N1 | frame: 8 dados, sem paridade, 1 stop |
| full-duplex | transmissão simultânea nos dois sentidos |
| MOSI / MISO | fios de dados mestre→escravo / escravo→mestre |
| CS (chip select) | fio que acorda um escravo SPI |
| CPOL/CPHA | polaridade e fase do clock SPI (os 4 modos) |
| ACK | confirmação (SDA em 0 no 9º bit) |
| dreno aberto | saída que só puxa para 0 (precisa de pull-up) |
| clock stretching | escravo segurando SCL para pedir tempo |
| scanner I2C | varredura de endereços à procura de ACK |
| mapa de registradores | endereços internos de um sensor inteligente |

## 📖 Onde aprofundar (opcional)

- **Molloy**, *Exploring Raspberry Pi*, cap. 8 — barramentos no Linux (releitura na semana
  12).
- Datasheet + *Register Map* do **MPU-6050** (InvenSense, gratuito) — pratique achar o 0x75
  e o 0x3B você mesmo; saber ler mapa de registradores é metade da profissão.
- **ESP-IDF Guide**: *I2C Driver*, *SPI Master Driver*, *UART*.

## Indo além — UART não é só `printf`: o protocolo Modbus

O que trafega na UART pode ser texto solto (o nosso monitor) ou um **protocolo estruturado**.
O padrão industrial mais difundido é o **Modbus-RTU**: mestre-escravo, quadros com endereço,
função e **CRC-16**, sobre a mesma UART 9600 8N1 desta semana. Se o laboratório dispõe do
medidor de energia **PZEM-004T**, há um lab extra completo em `labs-extra/medidor-energia/`
que lê grandezas elétricas reais (V, I, potência ativa, kWh, fator de potência) por Modbus e
publica via MQTT — excelente aprofundamento desta aula e ótimo tema de projeto final.

## Exercícios

Lista 3, questões 11–15 (estilo dos Exemplos 9.1–9.4).
