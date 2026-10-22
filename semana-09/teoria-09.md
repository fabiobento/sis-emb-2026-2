# Aula 9 — Comunicação Serial: UART, SPI e I2C (U4)

> **Pré-requisito**: Aulas 3 (open-drain, endianness) e 4 (latência).
> **Como usar**: texto autossuficiente. Os Exemplos 9.1–9.4 são o modelo das questões 11–15
> da Lista 3. A seção 5 (tabela e fluxograma de decisão) é para saber de cor — ela é cobrada
> em toda prova de embarcados do mundo. O [Lab 9](lab-09.md) coloca as seções 4.1–4.6 na
> bancada: scanner, MPU-6050 e display OLED no mesmo barramento.

Seu ESP32 já sente (ADC) e age (PWM). Mas um sistema embarcado raramente vive só: ele
conversa com sensores inteligentes, displays, cartões de memória, GPS, com o PC e com outros
processadores. Esta aula apresenta os **três barramentos seriais** que cobrem 95 % dessas
conversas — UART, SPI e I2C — item explícito da ementa ("interfaces de comunicação serial e
paralela") e ferramenta de trabalho de todas as semanas restantes. A pergunta de engenheiro
não é "qual é o melhor?", e sim "**qual é o certo para este caso?**" — ao final, você terá a
tabela de decisão na cabeça. No laboratório, o MPU-6050 (acelerômetro + giroscópio) vira seu
primeiro sensor digital inteligente.

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) descrever o frame UART, calcular tempos de transmissão e a tolerância de baud rate;
- (b) explicar SPI (fios, CS, modos CPOL/CPHA) e por que é o mais rápido;
- (c) explicar I2C (START/STOP, endereçamento, ACK, dreno aberto + pull-up) e ler o mapa de
  registradores de um sensor;
- (d) dimensionar um pull-up e diagnosticar um barramento que "não responde";
- (e) escolher o barramento certo para cada aplicação.

**Mapa da aula** — as três famílias lado a lado, antes de entrar nos detalhes:

| | UART | SPI | I2C |
|---|---|---|---|
| Ideia central | frame com start/stop, **sem clock** | clock do mestre + **fio de seleção** por escravo | **endereço** no protocolo, 2 fios para todos |
| Seção | 2 | 3 | 4 |
| Figuras | 9-B, 9-C, 9-D, 9-E | 9-F, 9-G, 9-H, 9-I | 9-J … 9-T |

---

## 1. Por que serial (e não paralelo)?

Paralelo = um fio por bit: rápido no papel, caro em pinos, cabos e sincronismo (os bits
"desalinham" em frequências altas — cada fio tem um atraso ligeiramente diferente, e acima de
certa velocidade os bits de um mesmo byte chegam em tempos diferentes; é o *skew*). Serial =
os bits em fila num único par de fios, com pinos de sobra e alcance maior. A indústria migrou
quase tudo para serial (até o barramento do seu SSD é serial — SATA, PCIe).

![Duas séries de oito fios paralelos com atrasos diferentes: à esquerda, com bits longos, existe uma janela comum para amostrar; à direita, com bits curtos, não existe instante em que todos os fios sejam válidos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_skew.png)

*Figura 9-A — O skew mata o paralelo: com bit longo, o desencontro entre os fios cabe folgado
na janela de amostragem (esquerda); quando o bit fica do tamanho do skew, não existe instante
em que o byte inteiro seja válido (direita).*

A conta mental é simples: se o pior desencontro entre fios é 1 ns, um barramento paralelo só
funciona com bits bem mais longos que 1 ns (frequências bem abaixo de 1 GHz, na prática muito
menos). Num par serial, **não há com quem desencontrar**: o relógio (ou o start bit) viaja
junto com o dado, e é possível ir muito mais rápido com muito menos cobre. A distinção viva
hoje é **assíncrono** (sem fio de clock: UART) × **síncrono** (com clock explícito: SPI,
I2C).

> 💡 **Pense aí**: o SPI, que tem clock *e* dados, ainda é "serial". Mas tem 4 fios, e o
> I2C tem 2. Por que ninguém chama o SPI de "paralelo"? *Porque os dados do SPI trafegam
> **um bit por vez** em cada fio de dados (MOSI/MISO) e o clock vem junto, na mesma fiação:
> não há bits de um mesmo byte competindo por alinhamento entre fios.*

## 2. UART: o veterano assíncrono

A **UART** liga dois dispositivos ponto a ponto com dois fios cruzados (TX→RX, RX←TX) e
**sem clock compartilhado**: os dois lados combinam previamente a velocidade (**baud rate**)
e se ressincronizam a cada byte pelo **start bit** (a queda de 1 para 0 que anuncia “vem byte
aí”). Como não há fio de clock, cada lado mede o tempo com o seu próprio oscilador — daí a
exigência de baud rates *iguais* e razoavelmente precisos (tolerância de ~2 % no total).

![ESP32 e módulo ligados por três fios: o TX de cada lado vai no RX do outro, cruzando, e o GND é comum; avisos sobre nível de 3,3 V e sobre RS-232](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_uart_ligacao.png)

*Figura 9-B — A ligação de duas UARTs: **TX de um no RX do outro** (cruzado — o erro número 1
de bancada é ligar TX com TX), GND em comum e o mesmo nível lógico nos dois lados.*

O frame clássico "8N1" (8 dados, sem paridade, 1 stop):

![Estrutura do frame UART: repouso, start bit, 8 bits de dados LSB-first, stop bit](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/uart_frame.png)

*Figura 9-C — O frame UART: sem clock no fio, o start bit é o único marco de sincronismo.
Dez bits trafegam para entregar oito — overhead de 25 %.*

```
 ocioso ─▔▔▔╲__┌d0┬d1┬d2┬d3┬d4┬d5┬d6┬d7┐▔▔─ ocioso
            start  (LSB primeiro)       stop
            = 10 bits no fio para 8 bits úteis (overhead de 25 %)
```

Três detalhes que costumam ser esquecidos e causam bugs:

- **A linha ociosa fica em 1.** O start bit é a *queda* para 0; o stop bit devolve a linha a
  1 — e é isso que garante que o *próximo* start bit seja, de novo, uma queda.
- **O bit menos significativo (LSB) vai primeiro.** O caractere `'A'` (0x41 = `0100 0001`)
  aparece no fio assim: start(0) · **1** 0 0 0 0 0 **1** 0 · stop(1) — os dois "1" são o
  bit 0 e o bit 6, na ordem inversa da que escrevemos no papel.
- **O frame é configurável.** Além de 8N1, existem 8E1 (paridade par), 8N2 (dois stops),
  7E1 etc. Os dois lados precisam usar o **mesmo** frame, senão chega lixo.

| Formato | Bits no fio por byte | Eficiência (úteis / total) | Quando aparece |
|---|---|---|---|
| 8N1 | 10 | 80 % | padrão de console, GPS, a maioria dos módulos |
| 8E1 | 11 | 72,7 % | sensores/industrial com detecção simples de erro (paridade) |
| 8N2 | 11 | 72,7 % | lado receptor lento que precisa de mais tempo entre bytes |
| 7E1 | 10 | 70 % (7 úteis em 10) | protocolos ASCII antigos |

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

![Linha do tempo: uma barra de 44,4 ms representando a UART transmitindo 512 bytes sobre quatro períodos de laço de 10 ms](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_uart_printf.png)

*Figura 9-D — O Exemplo 9.1 em linha do tempo: a UART fica ocupada por mais de quatro
períodos do laço de 10 ms. Se o `printf` bloqueia, o laço perde quatro prazos seguidos.*

**Exemplo resolvido 9.1b (por que o baud rate precisa ser *igual*)** — O receptor detecta a
borda de descida do start bit e, daí em diante, amostra cada bit **no meio** do tempo de bit
(T = 1/baud), *contando com o seu próprio relógio*. Se o relógio do receptor está e % mais
lento que o do transmissor, qual o erro máximo tolerável num frame 8N1?

*Solução.* A k-ésima amostra (k = 0 para o start, até k = 9 para o stop) ocorre em
(k + 0,5)·T·(1 + e), contada a partir da borda do start. Ela só é válida se cair dentro do
bit k, isto é, antes de (k + 1)·T. O pior caso é o **último** bit (k = 9): 9,5·(1 + e) < 10
⇒ e < 5,26 %. Como *os dois* lados têm erro, as bordas não são ideais e há ruído, o projeto
exige **|erro total| ≤ ~2 %**.

![Três faixas com os dez bits do frame e as amostras do receptor: com erro de 0 % e de 3 % todas caem dentro de cada bit; com 6 %, os dois últimos bits são amostrados no bit seguinte](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_uart_tolerancia.png)

*Figura 9-E — O erro de baud se acumula ao longo dos 10 bits: a 3 % ainda dá; a 6 %, os dois
últimos bits já são lidos no bit errado (os marcadores vermelhos).*

> 💡 **Pense aí**: por que a UART do ESP32 a 115 200 baud é confiável mesmo com um cristal
> comum, mas uma UART "bit-bang" feita à mão com `delay` impreciso falha? *O baud generator
> de hardware divide o clock do periférico com erro de fração de ponto percentual; um
> `delay` em software acumula jitter do escalonador, e 2 % de erro vão embora rápido.*

Na prática, no ESP32 a UART0 é a do console; as UART1 e UART2 aceitam **quaisquer pinos** (a
matriz de GPIO roteia). Um esboço de configuração (ESP-IDF 5.x; ver o *UART Driver* no guia):

```c
uart_config_t cfg = {
    .baud_rate = 9600, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT };
uart_driver_install(UART_NUM_1, 256, 0, 0, NULL, 0);          // buffer de RX de 256 bytes
uart_param_config(UART_NUM_1, &cfg);
uart_set_pin(UART_NUM_1, 17 /*TX*/, 16 /*RX*/, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
```

## 3. SPI: o velocista síncrono

O **SPI** adiciona o fio de clock (SCLK) — o mestre dita o ritmo e os dados fluem
**full-duplex** por dois fios de dados (MOSI: mestre→escravo; MISO: escravo→mestre). Cada
escravo ganha um fio de seleção **CS** (ativo-baixo): é o "endereço por fio" — em vez de
perguntar “quem é 0x68?”, o mestre acorda fisicamente um escravo de cada vez.

![Mestre ESP32 com três fios compartilhados (SCLK, MOSI, MISO) para três escravos e um fio CS exclusivo para cada escravo](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_spi_topologia.png)

*Figura 9-F — Topologia SPI: três fios compartilhados e um CS por escravo. Só o escravo com
CS em 0 escuta o clock e pode dirigir o MISO (os demais o deixam em alta impedância).*

![Sinais de uma transação SPI: CS, clock, MOSI e MISO](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/spi_transacao.png)

*Figura 9-G — Transação SPI: CS cai, o clock corre, e em cada pulso um bit vai (MOSI) e um
bit volta (MISO) — full-duplex de verdade.*

Sem endereço no protocolo, sem ACK, sem overhead: SPI alcança **dezenas de MHz**. O preço:
3 + N fios para N escravos, e quatro "modos" a casar entre as partes — **CPOL** (nível
ocioso do clock) e **CPHA** (qual borda amostra o dado). Modo errado = dados
deslocados/invertidos, o clássico "funciona pela metade" — o datasheet do escravo sempre
diz o modo. (O MPU-6000, "irmão" do nosso MPU-6050, é a versão com SPI; o 6050 é só I2C.)

![Quatro painéis com as formas de onda de CS, SCLK e MOSI para os modos SPI 0, 1, 2 e 3, com marcadores nas bordas de amostragem](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_spi_modos.png)

*Figura 9-H — Os quatro modos SPI. CPOL = nível ocioso do clock (0 = baixo, 1 = alto);
CPHA = 0 amostra na **primeira** borda do clock, CPHA = 1 na **segunda**. Os triângulos
verdes marcam onde o escravo "fotografa" o bit.*

| Modo | CPOL | CPHA | Clock ocioso | Escravo amostra na borda de | Dado muda na borda de |
|---|---|---|---|---|---|
| 0 | 0 | 0 | baixo | subida | descida |
| 1 | 0 | 1 | baixo | descida | subida |
| 2 | 1 | 0 | alto | descida | subida |
| 3 | 1 | 1 | alto | subida | descida |

Dois pontos de memória: os modos **0 e 3** são os mais comuns (ambos amostram na *subida*,
só diferem no nível ocioso), e a regra de ouro é a mesma de sempre — *o dado precisa estar
estável na borda em que o outro lado amostra*. Se CPHA estiver errado, o escravo amostra o
dado justamente quando ele está mudando.

> 📌 **No ESP32** os periféricos SPI livres para o usuário são o SPI2 (HSPI) e o SPI3 (VSPI);
> o SPI0/1 falam com a flash. O ESP-IDF cuida do CS por dispositivo (`spi_device_interface_config_t`)
> e permite clocks de até 80 MHz nos pinos "IO MUX" (menos nos pinos roteados pela matriz).

**Exemplo resolvido 9.2 (SPI × I2C em números)** — Ler 6 bytes de um acelerômetro a
SCLK = 8 MHz.

*Solução passo a passo.* t = 6 × 8 bits / 8·10⁶ = **6 µs**. O mesmo pacote por I2C a 400 kHz
levaria ~ (endereço + registrador + 6 dados + ACKs ≈ 9 bytes × 9 bits)/400 k ≈ **200 µs** —
mais de 30× mais lento. Conclusão de projeto: fluxo **rápido e volumoso** (display gráfico,
cartão SD, ADC de alta taxa) ⇒ SPI.

![Barras horizontais em escala logarítmica com o tempo para ler 6 bytes: SPI 8 MHz em 6 microssegundos, SPI 1 MHz em 48, I2C 400 kHz em 202, UART 115200 em 521 e I2C 100 kHz em 810](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_tempos_6bytes.png)

*Figura 9-I — O mesmo pacote de 6 bytes em cada barramento: o SPI a 8 MHz é ~34× mais rápido
que o I2C a 400 kHz. Note a escala logarítmica — as barras escondem diferenças enormes.*

**Exemplo resolvido 9.2b (qual modo SPI?)** — O datasheet de um ADC diz: "o clock fica em
nível **alto** quando ocioso; os dados são capturados na borda de **descida** do SCLK".
Qual modo configurar?

*Solução.* Clock ocioso alto ⇒ **CPOL = 1**. Se o clock *ocioso é alto*, a primeira borda
após o CS cair é de **descida**; como o dado é capturado nessa **primeira** borda ⇒
**CPHA = 0**. Modo **2** (CPOL = 1, CPHA = 0) — confira na tabela acima (modo 2: amostra na
descida). Se o texto dissesse "capturados na borda de *subida*", seria a segunda borda ⇒
CPHA = 1 ⇒ modo 3.

## 4. I2C: o diplomata de dois fios

O **I2C** liga *muitos* dispositivos com **dois fios apenas** — SDA (dados) e SCL (clock) —
compartilhados por todos. Cada escravo tem um **endereço de 7 bits**; a transação começa com
START + endereço + bit R/W̄, e o escravo endereçado responde **ACK** (puxa SDA para 0 no 9º
bit). Velocidades: 100 kHz (standard) e 400 kHz (fast) — lento, mas dois fios para um
barramento inteiro de sensores é imbatível em fiação.

O detalhe elétrico que explica tudo: as saídas I2C são **dreno aberto** (semana 3, Figura
3-E!) — os dispositivos só sabem *puxar a linha para 0*; quem devolve a linha a 1 é o
resistor de **pull-up**. Assim vários chips coexistem sem curto (todo mundo pode "falar 0" ao
mesmo tempo sem briga elétrica — o wired-AND), e um escravo lento pode até segurar SCL em 0
para pedir tempo (*clock stretching*).

![Circuito com um resistor de pull-up de 4,7 quilo-ohms ligando a linha SDA ao 3V3 e dois dispositivos com chaves que só puxam a linha para o GND, com a tabela-verdade do E lógico](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_dreno_aberto.png)

*Figura 9-J — Dreno aberto + pull-up: a linha só vai a 1 quando **ninguém** a puxa para 0.
A tabela-verdade é a de um AND — por isso se chama "wired-AND". Com saída push-pull, dois
dispositivos discordando criariam um curto entre 3V3 e GND.*

![Sequência de uma transação I2C: START, endereço de 7 bits, R/W, ACK, dados](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/i2c_transacao.png)

*Figura 9-K — O I2C bit a bit: o mestre fala o endereço, e só o escravo endereçado responde
ACK. Dois fios, até 127 moradores.*

### 4.1 Enquadramento, endereço e ACK

Como o escravo sabe onde uma transação começa e termina, se só há dois fios e nenhum "CS"?
Pelas **condições de enquadramento**, que violam de propósito a regra dos dados:

- **Dado**: a SDA só muda enquanto o SCL está **baixo**; com o SCL alto, a SDA tem de ficar
  estável (é quando o receptor "lê" o bit).
- **START**: a SDA cai com o SCL **alto**. **STOP**: a SDA sobe com o SCL **alto**.
- **START repetido (Sr)**: um novo START *sem* STOP antes — o mestre mantém o barramento
  "preso" para fazer duas fases seguidas (escreve o registrador, depois lê), sem dar chance
  a outro mestre de entrar no meio.

![Formas de onda de SCL e SDA mostrando um START, dois bits de dado, um START repetido e um STOP, com a SDA mudando sempre com SCL baixo exceto nas condições de enquadramento](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_i2c_start_stop.png)

*Figura 9-L — START, STOP e START repetido são os únicos momentos em que a SDA muda com a SCL
alta. É essa exceção que as torna reconhecíveis entre os bits de dado.*

O **primeiro byte** depois do START carrega o endereço de **7 bits** (do MSB para o LSB) mais
o bit **R/W̄** (0 = escrita, 1 = leitura). Por isso o scanner envia `(a << 1) | W`:

![Dois bytes de endereço decompostos em bits: MPU-6050 em 0x68, que vira 0xD0 na escrita e 0xD1 na leitura, e OLED em 0x3C, que vira 0x78 e 0x79](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_i2c_byte_endereco.png)

*Figura 9-M — O primeiro byte da transação: 7 bits de endereço + R/W̄. Cuidado: muitos
datasheets citam o endereço **já deslocado** (0xD0, 0x78); o driver do ESP-IDF quer o
endereço de 7 bits (0x68, 0x3C).*

São 2⁷ = 128 endereços; a norma reserva **0x00–0x07** e **0x78–0x7F** (ex.: 0x00 é a "chamada
geral"), e o scanner convencional varre **0x03–0x77** (é o mesmo recorte do `i2cdetect` do
Linux). Depois de **cada byte** vem um 9º pulso de clock, o **ACK**: o *receptor* puxa a SDA
para 0 ("recebi"); SDA em 1 é **NACK** ("não recebi / chega"). Repare nos papéis: quem manda
o byte solta a SDA no 9º bit; quem recebe é quem a segura em 0.

### 4.2 Conversando com um sensor: o mapa de registradores

Sensores I2C "inteligentes" (MPU-6050, BME280, RTC DS3231…) expõem um **mapa de
registradores** — a mesma ideia dos periféricos mapeados da semana 2, agora do outro lado do
fio: cada função do sensor (identidade, configuração, dados) mora num endereço interno. O
protocolo de leitura tem duas fases: *escreve* o número do registrador desejado, *lê* os
bytes a partir dele (com um START repetido entre elas, Figura 9-L). É exatamente o par
`wr()`/`rd()` do firmware de hoje:

![Duas transações I2C: escrever o registrador 0x6B com valor 0x00, e ler 14 bytes a partir do registrador 0x3B com START repetido, ACK do mestre em cada byte e NACK no último](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_i2c_reg_rw.png)

*Figura 9-N — As duas transações que você usa em todo sensor I2C: **escrever** um registrador
(`wr`) e **ler** n bytes a partir dele (`rd`). O mestre dá ACK nos bytes que quer continuar
recebendo e NACK no último.*

```c
#define ADDR 0x68            // MPU-6050 com AD0=GND (0x69 com AD0=VCC)
#define REG_WHOAMI 0x75      // registrador de identidade: responde 0x68

static esp_err_t rd(uint8_t reg, uint8_t *dst, size_t n)
{   // fase 1: escreve o nº do registrador; fase 2: lê n bytes a partir dele
    return i2c_master_write_read_device(I2C_NUM_0, ADDR, &reg, 1, dst, n, pdMS_TO_TICKS(50));
}
```

Um fato que torna a leitura em rajada possível: o **ponteiro de registrador** do sensor
**auto-incrementa** a cada byte lido. Por isso os 14 bytes consecutivos a partir de 0x3B
(ax, ay, az, temperatura, gx, gy, gz) vêm num único pedido — e, bônus, **todos pertencem ao
mesmo instante** (o sensor "congela" o conjunto durante a leitura).

### 4.3 O lado elétrico: dimensionando o pull-up

Como a linha só *sobe* pela corrente do resistor, a subida de 0 para 1 é uma exponencial RC:
a resistência do pull-up (R) e a capacitância total do barramento (C — fios, pinos, trilhas;
dezenas a centenas de pF) definem **quanto tempo a linha demora a subir**. Quanto menor o R,
mais rápida a subida — mas mais corrente é gasta cada vez que um dispositivo puxa a linha
para 0.

![Três curvas de tensão em função do tempo para pull-ups de 1, 4,7 e 10 quilo-ohms com capacitância de 100 picofarads, com as marcações de 30 e 70 por cento de Vcc e as linhas de limite de 300 nanossegundos e 1000 nanossegundos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_i2c_subida_rc.png)

*Figura 9-O — A subida é uma exponencial RC. O padrão mede o tempo de subida entre 30 % e
70 % de Vcc: t_r = 0,8473·R·C. Os limites são 1000 ns (100 kHz) e **300 ns** (400 kHz).*

**Exemplo resolvido 9.4 (dimensionando o pull-up)** — Barramento com capacitância total de
100 pF, 3,3 V, e alvo de tempo de subida t_r ≤ 300 ns (exigência do modo 400 kHz). Qual a
faixa de R permitida? E os pull-ups de 4,7 kΩ dos nossos módulos servem?

*Solução.* **Limite superior** (velocidade): t_r = 0,8473·R·C ≤ 300 ns ⇒
R ≤ 300 ns/(0,8473 × 100 pF) = **3,54 kΩ**. **Limite inferior** (corrente): o dispositivo
precisa conseguir puxar a linha a V_OL ≤ 0,4 V sorvendo no máximo I_OL = 3 mA ⇒
R ≥ (3,3 − 0,4)/3 mA ≈ **970 Ω**. A janela é, portanto, **≈ 1 kΩ a 3,5 kΩ**; na prática,
2,2–3,3 kΩ. E o módulo de **4,7 kΩ**? Com 100 pF dá t_r = 0,8473 × 4,7 kΩ × 100 pF ≈ 398 ns
> 300 ns: **não** cumpre o 400 kHz para esse C — só cumpriria com C ≤ 75 pF (fios curtos), ou a
100 kHz (limite 1000 ns). Os módulos MPU-6050/OLED *já trazem pull-ups a bordo*; **dois
módulos no mesmo barramento = pull-ups em paralelo** (4,7 k ∥ 4,7 k = 2,35 kΩ), que aceita até
C ≈ 150 pF a 400 kHz — e ainda respeita o limite inferior de 970 Ω. Menor R = subida mais
rápida, porém mais corrente a cada nível 0 (3,3 V/2,35 kΩ ≈ 1,4 mA) — o compromisso da
questão 14 da Lista 3.

![Gráfico do pull-up em função da capacitância do barramento: duas curvas decrescentes para o R máximo a 400 kHz e a 100 kHz, uma reta horizontal para o R mínimo de cerca de 970 ohms e as marcações de 4,7 e 2,35 quilo-ohms](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_i2c_janela_pullup.png)

*Figura 9-P — A janela do pull-up: abaixo de ~970 Ω o dispositivo não consegue puxar a linha
até 0,4 V; acima da curva verde (400 kHz) ou azul (100 kHz) a subida é lenta demais.
Quanto maior a capacitância do barramento, mais estreita a janela.*

> 💡 **Pense aí**: o scanner do lab roda a 100 kHz e o firmware do MPU/OLED a 400 kHz.
> Se um barramento "funciona no scanner mas falha no 400 kHz", qual é a primeira suspeita?
> *A subida: capacitância alta (fios compridos, muitos módulos) ou pull-up grande demais.
> Reduza a velocidade para 100 kHz (limite de 1000 ns) ou diminua o R/os fios.*

### 4.4 Muitos dispositivos, um barramento

**Exemplo resolvido 9.3 (dois sensores idênticos, um barramento)** — O MPU-6050 responde em
0x68 (pino AD0 = GND) ou 0x69 (AD0 = V_cc): **dois** sensores idênticos convivem no mesmo
barramento escolhendo endereços diferentes por hardware — e o display OLED (0x3C) entra de
carona nos mesmos dois fios. É o superpoder do I2C: o barramento vira uma "rede local de
sensores" dentro da placa. (Mais de dois MPU-6050? Aí é multiplexador I2C ou SPI — pergunta
de projeto da Lista 3.)

![Barramento com três dispositivos: dois MPU-6050 com AD0 no GND (endereço 0x68) e no 3V3 (endereço 0x69), e um OLED em 0x3C, todos ligados às mesmas linhas SDA, SCL e 3V3](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_i2c_tres_dispositivos.png)

*Figura 9-Q — Exemplo 9.3 em um desenho: três dispositivos, três endereços, **zero fios novos**.*

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
endereço errado, nessa ordem). O Lab 9 mostra, passo a passo, a "cara" de cada falha.

### 4.5 Quando dá errado: clock stretching e barramento travado

Dois comportamentos do I2C que surpreendem quem só viu o caso feliz:

- **Clock stretching.** O escravo pode **segurar o SCL em 0** para pedir tempo (processando
  uma conversão, por exemplo). O mestre, depois de soltar o SCL, precisa **esperar a linha
  realmente subir** antes de seguir. Mestres que ignoram isso corrompem dados de sensores
  lentos. É mais uma consequência do dreno aberto: quem "puxa" ganha.
- **Barramento travado.** Se o mestre reinicia (um reset do ESP32, um watchdog, um cabo
  puxado) **no meio de um byte**, o escravo pode ficar esperando os pulsos que faltam, com a
  SDA presa em 0. Depois do reset, o mestre vê "barramento ocupado" e nada funciona — o
  sintoma é um scanner vazio mesmo com tudo ligado certo. A recuperação padrão é o mestre
  gerar **até 9 pulsos de clock manualmente** (configurando o SCL como GPIO) até o escravo
  largar a SDA, e então emitir um STOP. É um truque que vale ouro em produto de campo.

![Dois diagramas: acima, o escravo segura SCL em 0 enquanto prepara o dado e o mestre espera; abaixo, o barramento travado com SDA presa em 0, nove pulsos de clock manuais e a liberação da SDA seguida de STOP](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_i2c_stretch_travado.png)

*Figura 9-R — O escravo que pede tempo (clock stretching) e o barramento que trava (a
recuperação por até 9 pulsos de clock).*

### 4.6 Decodificando o MPU-6050

O firmware lê 14 bytes a partir de `REG_ACCEL (0x3B)`: ax, ay, az, temperatura, gx, gy, gz —
cada eixo em 16 bits **big-endian** (byte alto primeiro — a endianness da semana 3 cobrando
pedágio: por isso o `(b[0]<<8)|b[1]`). No fundo de escala padrão, ±2 g → 16 384 LSB/g e
±250 °/s → 131 LSB/(°/s). E a inclinação sai da gravidade por trigonometria:

```c
int16_t ax = (b[0]<<8)|b[1], az = (b[4]<<8)|b[5];       // big-endian → int16
float axg = ax/16384.f, azg = az/16384.f;               // LSB → g
float theta = atan2f(axg, azg) * 180.f / M_PI;          // inclinação no plano XZ
```

Os fundos de escala são programáveis, e **cada degrau mais largo troca resolução por faixa**
(o mesmo compromisso do ADC da semana 7):

| Acelerômetro (`ACCEL_CONFIG`, reg 0x1C) | Faixa | Sensibilidade |
|---|---|---|
| 0x00 | ±2 g | 16 384 LSB/g |
| 0x08 | ±4 g | 8 192 LSB/g |
| 0x10 | ±8 g | 4 096 LSB/g |
| 0x18 | ±16 g | 2 048 LSB/g |

| Giroscópio (`GYRO_CONFIG`, reg 0x1B) | Faixa | Sensibilidade |
|---|---|---|
| 0x00 | ±250 °/s | 131 LSB/(°/s) |
| 0x08 | ±500 °/s | 65,5 LSB/(°/s) |
| 0x10 | ±1000 °/s | 32,8 LSB/(°/s) |
| 0x18 | ±2000 °/s | 16,4 LSB/(°/s) |

(Mudou a faixa, **mude a divisão** — erro clássico: trocar para ±4 g e continuar dividindo
por 16 384 faz a placa parada marcar ~0,5 g. É um dos experimentos do Lab 9.)

Com a placa parada, o vetor aceleração ≈ gravidade (o acelerômetro “sente” o chão empurrando
a placa para cima a 1 g); `atan2` do par (ax, az) devolve o ângulo de inclinação — seu
primeiro "sensor de atitude", base de projetos de estabilização (robôs balanceadores, gimbals).

![À esquerda, o círculo unitário no plano az–ax com o vetor de gravidade formando o ângulo theta, com ax igual a seno de theta e az igual a cosseno de theta, e os quatro quadrantes; à direita, o ângulo estimado por asin e por atan2 e o erro devido a ruído em cada um](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_atan2.png)

*Figura 9-S — Da gravidade ao ângulo. O par (az, ax) é um vetor de módulo ≈ 1 g; θ é o seu
ângulo. O `atan2` lê o **quadrante** pelos sinais e cobre 360°; o `asin(ax)` só enxerga
−90°…+90° e vê o ruído amplificado por 1/cos θ perto de ±90°.*

> 💡 **Pense aí**: por que `atan2(ax, az)` e não `asin(ax)`? *O `atan2` usa os dois eixos e
> não explode quando o denominador passa por zero; `asin` perde resolução perto de ±90° e
> exige normalização. Em firmware de atitude, `atan2` é o padrão — e é grátis na math.h.*

**Limites desta medição (e a ponte para a fusão de sensores).** O acelerômetro não distingue
a gravidade de uma aceleração do movimento: sacudiu a placa, o ângulo calculado "mente".
Já o giroscópio mede a **taxa de rotação** — suave e imune à vibração —, mas ao integrá-la o
pequeno viés do sensor (graus por segundo) vira uma **deriva** que cresce com o tempo. Cada
sensor é bom justamente onde o outro é ruim. O **filtro complementar** costura os dois com
uma conta de uma linha: θ_f = α·(θ_f + ω·Δt) + (1 − α)·θ_acel, com α perto de 1 (0,98, por
exemplo) — "confie no giroscópio no curto prazo e no acelerômetro no longo prazo".

![Três gráficos de ângulo em função do tempo numa simulação: só o acelerômetro, ruidoso e enganado por vibração; só o giroscópio integrado, suave mas com deriva crescente; e o filtro complementar, que segue o ângulo verdadeiro](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_filtro_complementar.png)

*Figura 9-T — Fusão de sensores numa **simulação ilustrativa** (não é dado medido): erro RMS
de ≈ 4,0° só com o acelerômetro, ≈ 17° só com o giroscópio (deriva) e ≈ 0,8° com o filtro
complementar. No Lab 9 usamos só o acelerômetro, com média móvel; o complementar fica como
extensão.*

**O display OLED, do outro lado do mesmo barramento.** O SSD1306 (128×64) também é um escravo
I2C — endereço 0x3C — mas, em vez de registradores de *leitura*, tem uma memória de pixels
para *escrita*: cada transação começa com um **byte de controle** (0x00 = comando,
0x40 = dados de pixel) e a memória é organizada em **8 páginas × 128 colunas**, com cada byte
pintando 8 pixels de uma coluna (bit 0 no topo). Os detalhes — e o firmware de um inclinômetro
que usa MPU e OLED ao mesmo tempo — estão na Parte C do [Lab 9](lab-09.md).

## 5. A tabela de decisão

| | **UART** | **SPI** | **I2C** |
|---|---|---|---|
| Fios | 2 (TX/RX) | 3 + 1 CS por escravo | 2 (SDA/SCL) |
| Topologia | ponto a ponto | 1 mestre, N escravos | multiponto (127 end.) |
| Clock | não (baud combinado) | sim (mestre) | sim (mestre) |
| Velocidade típica | 9,6 k–3 Mbaud | 1–80 MHz | 100/400 kHz (1 MHz FM+) |
| Endereçamento | — | por fio (CS) | 7 bits no protocolo |
| Confirmação | — | — (sem ACK) | ACK a cada byte |
| Overhead | 25 % (8N1) | ~0 | ~11 % de ACK + endereço |
| Forte em | console, GPS, módulos | volume/velocidade | muitos sensores, 2 fios |
| Fraco em | multiponto; erros de baud | muitos escravos (fios) | velocidade; capacitância |
| No curso | monitor serial; RPi↔ESP32 | RC522 RFID, SD, MCP3008 | MPU-6050, OLED, LCD, RTC |

Casos-teste (questão 15 da Lista 3): GPS que "fala NMEA" ⇒ UART; display gráfico com refresh
alto ⇒ SPI; 5 sensores lentos na mesma placa ⇒ I2C. Se você acertou os três sem olhar a
tabela, a aula cumpriu a missão. O fluxograma da Figura 9-U organiza o raciocínio na ordem
em que um engenheiro o faz:

![Fluxograma de decisão com quatro perguntas: ponto a ponto de texto leva à UART; volume ou velocidade alta leva ao SPI; muitos dispositivos lentos com poucos pinos leva ao I2C; e longa distância ou ruído leva a RS-485 ou CAN](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/teoria09_decisao_barramento.png)

*Figura 9-U — Qual barramento? Pergunte nesta ordem: o primeiro "sim" decide, e a última
pergunta aponta para a semana 10 (CAN) — o "certo para este caso" muda quando a distância e o
ruído entram.*

**Exemplo resolvido 9.5 (o orçamento de um projeto real)** — O inclinômetro do Lab 9 lê o MPU
a 10 Hz e redesenha o OLED a 5 Hz, tudo no mesmo barramento a 400 kHz. O barramento aguenta?

*Solução.* **Leitura do MPU**: endereço+W, registrador, endereço+R e 14 dados = 17 bytes ×
9 bits / 400 kHz ≈ **0,38 ms**. **Quadro do OLED**: 8 transações de (endereço + controle + 128
dados) = 8 × 130 = 1 040 bytes, mais os comandos de janela (≈ 1 050 bytes no total) ⇒
1 050 × 9 / 400 kHz ≈ **24 ms**. Num período de 200 ms (o redesenho a 5 Hz) a ocupação é (0,38 × 2 + 24)/200 ≈
**12 %** — folga enorme. A 100 kHz, o mesmo quadro levaria **≈ 95 ms** (quase metade do período):
é por isso que o lab usa 400 kHz. O gargalo apareceria se o redesenho fosse a 30 Hz ou se
houvesse muitos sensores mais lentos disputando a mesma linha — hora de migrar o display
para SPI (Exemplo 9.2).

---

## Resumindo

- Serial venceu o paralelo (pinos, alcance, skew — Figura 9-A); a divisão viva é assíncrono
  (UART) × síncrono (SPI/I2C).
- UART 8N1: 10 bits no fio por byte ⇒ taxa útil = baud/10; LSB primeiro; TX↔RX cruzados; o
  erro de baud tolerável é ~2 % (Exemplos 9.1 e 9.1b); log demais mata temporização.
- SPI: clock do mestre, full-duplex, CS por escravo, modos CPOL/CPHA (0 e 3 são os comuns);
  o mais rápido (Exemplo 9.2: 6 µs × 200 µs do I2C).
- I2C: 2 fios, dreno aberto + pull-up (t_r = 0,8473·R·C; janela de ~1 a 3,5 kΩ a 400 kHz com
  100 pF — Exemplo 9.4), START/STOP como únicas mudanças da SDA com SCL alto, endereço de
  7 bits + R/W̄ + ACK; sensores com mapa de registradores lidos por "escreve reg, lê dados"
  (ponteiro auto-incrementa); dois MPU-6050 convivem via AD0 (Exemplo 9.3); o scanner é o
  ping do barramento; barramento travado se recupera com até 9 pulsos de clock.
- MPU-6050: 14 bytes big-endian; ±2 g = 16 384 LSB/g (muda com a faixa); inclinação por atan2
  — endianness e trigonometria pagando o almoço; acelerômetro + giroscópio = fusão.
- Escolha pelo caso: console⇒UART, volume⇒SPI, muitos sensores⇒I2C (Figura 9-U).

> 🔭 **Onde isto reaparece:** este mesmo MPU-6050 lido por I2C é a **fonte de dados** de um
> classificador de movimento embarcado. Na [trilha TinyML](../docs/trilha-tinyml.md) (opcional, para
> o projeto final), o `main.c` desta semana vira o coletor de amostras que alimenta o Edge Impulse —
> o acelerômetro que hoje mede inclinação passa a distinguir *parado / aceno / sobe-desce / círculo*.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| baud rate | bits por segundo na linha serial |
| 8N1 | frame: 8 dados, sem paridade, 1 stop |
| LSB first | o bit menos significativo é o primeiro a sair no fio (UART) |
| skew | desencontro de tempo entre fios de um barramento paralelo |
| full-duplex | transmissão simultânea nos dois sentidos |
| MOSI / MISO | fios de dados mestre→escravo / escravo→mestre |
| CS (chip select) | fio que acorda um escravo SPI |
| CPOL/CPHA | polaridade e fase do clock SPI (os 4 modos) |
| START / STOP / Sr | condições de enquadramento do I2C (Sr = START repetido) |
| ACK / NACK | confirmação (SDA em 0 no 9º bit) / ausência dela |
| dreno aberto | saída que só puxa para 0 (precisa de pull-up) |
| wired-AND | linha que vai a 1 só se todos soltarem (efeito do dreno aberto) |
| t_r | tempo de subida da linha (30 %→70 % de Vcc), t_r = 0,8473·R·C |
| clock stretching | escravo segurando SCL para pedir tempo |
| scanner I2C | varredura de endereços à procura de ACK |
| mapa de registradores | endereços internos de um sensor inteligente |
| filtro complementar | fusão de giroscópio (curto prazo) e acelerômetro (longo prazo) |

## 📖 Onde aprofundar (opcional)

- **Molloy**, *Exploring Raspberry Pi*, cap. 8 — barramentos no Linux (releitura na semana
  12).
- Datasheet + *Register Map* do **MPU-6050** (InvenSense, gratuito) — pratique achar o 0x75
  e o 0x3B você mesmo; saber ler mapa de registradores é metade da profissão.
- **NXP UM10204**, *I2C-bus specification and user manual* — a norma oficial do I2C (a
  fórmula do tempo de subida e os limites de pull-up da seção 4.3 vêm daqui).
- **ESP-IDF Guide**: *I2C Driver*, *SPI Master Driver*, *UART*.

## Indo além — UART não é só `printf`: o protocolo Modbus

O que trafega na UART pode ser texto solto (o nosso monitor) ou um **protocolo estruturado**.
O padrão industrial mais difundido é o **Modbus-RTU**: mestre-escravo, quadros com endereço,
função e **CRC-16**, sobre a mesma UART 9600 8N1 desta semana. Se o laboratório dispõe do
medidor de energia **PZEM-004T**, há um lab extra completo em `labs-extra/medidor-energia/`
que lê grandezas elétricas reais (V, I, potência ativa, kWh, fator de potência) por Modbus e
publica via MQTT — excelente aprofundamento desta aula e ótimo tema de projeto final.

## Exercícios

Lista 3, questões 11–15 (estilo dos Exemplos 9.1–9.4). Para fixar, tente também:

1. Quanto tempo leva para transmitir `"T=25.4\r\n"` a 9 600 baud 8N1? E qual o tempo de
   bit? (Exemplo 9.1.)
2. Um receptor UART tem o relógio 4 % mais rápido que o transmissor. O frame 8N1 ainda é
   recebido corretamente? Em que bit estaria a primeira amostra errada se o erro chegasse a 6 %?
3. Um sensor SPI tem o datasheet "clock ocioso baixo, dado capturado na borda de descida".
   Qual o modo? (Exemplo 9.2b.)
4. Com C = 200 pF e Rp = 2,2 kΩ, qual o t_r? Serve a 400 kHz? E a 100 kHz? (Exemplo 9.4.)
5. O scanner encontrou 0x3C e 0x68. Que byte o mestre envia no 1º byte de uma **leitura** do
   MPU-6050? (Figura 9-M.)

<details><summary>✅ Respostas</summary>

1. 8 caracteres × 10 bits = 80 bits; t = 80/9 600 ≈ **8,33 ms** (tempo de bit = 1/9 600 ≈
   104 µs).
2. 4 % < 5,26 %: **sim**, ainda funciona (mas sem margem para o erro do outro lado e para
   ruído — fora do limite prático de ~2 %). Com 6 % o primeiro erro é a amostra do **d7**
   (k = 8): cai em 8,5·0,94 = 7,99·T (receptor mais rápido) ou 8,5·1,06 = 9,01·T (mais
   lento) — já no bit vizinho; o stop (k = 9) falha em seguida (Figura 9-E).
3. Clock ocioso baixo ⇒ CPOL = 0. A 1ª borda é de subida; o dado é capturado na descida
   (2ª borda) ⇒ CPHA = 1 ⇒ **modo 1**.
4. t_r = 0,8473 × 2,2 kΩ × 200 pF ≈ **373 ns**: **não** serve a 400 kHz (> 300 ns), **serve** a
   100 kHz (< 1000 ns).
5. 0x68 << 1 | 1 = **0xD1** (o 0xD0 é o da escrita).

</details>
