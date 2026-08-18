# Aula 3 — C para Embarcados e GPIO em Profundidade (U2/U3/U4)

> **Pré-requisito**: Aula 2 (mapa de memória, registradores, `volatile` introduzido).
> **Como usar**: texto autossuficiente. Tenha papel à mão: os Exemplos 3.1–3.3 são o modelo
> exato das questões 11–15 da Lista 1. Todos os trechos de C rodam no Wokwi — experimente.

Você já sabe *o que* é um registrador (semana 2). Esta aula ensina a **linguagem** para "falar"
com eles — o subconjunto de C que todo firmware usa: tipos de largura fixa, operações bit a
bit, `volatile`, ponteiros para endereços físicos. Na segunda metade, aplicamos tudo ao
periférico fundamental, o **GPIO**, agora pelo lado da eletrônica: quanta corrente um pino
aguenta, por que uma entrada solta "enlouquece", e por que um botão apertado uma vez pode ser
contado dez.

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) usar corretamente `stdint.h`, `volatile`, ponteiros e operações bit a bit;
- (b) explicar endianness e quando ela corrompe dados;
- (c) dimensionar o resistor de um LED e escolher entre pull-up e pull-down;
- (d) explicar e tratar o *bouncing* de chaves por software;
- (e) distinguir saída *push-pull* de *open-drain* (base para o I2C da semana 9).

---

## 1. C que o embarcado exige

C domina o firmware por um motivo: é uma linguagem de alto nível **que não esconde a
máquina** — você controla exatamente onde cada byte vive e quantas instruções cada linha
custa. Não há coletor de lixo, não há exceções escondidas, não há objetos surgindo do nada:
o que você escreve é o que a CPU executa. Mas o C "de faculdade" precisa de quatro ajustes
de mentalidade.

### 1.1 Tipos de largura fixa

Qual o tamanho de um `int`? **Depende da plataforma**: 16 bits no AVR do Arduino, 32 bits no
ESP32 e no RPi, e o padrão C só garante limites *mínimos* — o compilador escolhe o resto. Um
código que assume 32 bits quebra silenciosamente ao migrar (um contador que cabia vira
estouro; um campo de protocolo muda de lugar). Em registradores, protocolos e formatos de
dados, a largura **é parte da especificação do hardware** — então nós a declaramos
explicitamente com `#include <stdint.h>`:

```c
uint8_t  flags;      // exatamente 8 bits, sem sinal (registrador de 8 bits, byte de protocolo)
uint16_t adc_bruto;  // 16 bits (amostra de ADC de 12 bits cabe aqui, com folga)
uint32_t reg_gpio;   // 32 bits (registradores do ESP32)
int32_t  temp_mC;    // com sinal (temperatura em mili-°C pode ser negativa)
```

A regra prática de escolha: **sem sinal por padrão** (medidas, contagens, registradores não
são negativos); com sinal só quando o valor realmente pode ser negativo; e a menor largura
que representa a grandeza com folga (economia de RAM e de banda em protocolos).

Regra da disciplina: **em código de driver e protocolo, `int` é proibido**; use
`uintN_t/intN_t`.

### 1.2 Operações bit a bit

Um registrador de 32 bits são 32 interruptores independentes. Para mexer em **um** sem tocar
nos outros, usamos máscaras. O kit completo:

```c
reg |=  (1u << n);   // SETA o bit n       (OR com máscara)
reg &= ~(1u << n);   // LIMPA o bit n      (AND com máscara invertida)
reg ^=  (1u << n);   // INVERTE o bit n    (XOR)
if (reg & (1u << n)) // TESTA o bit n
```

Por que funciona? `1u << 3` desloca o 1 três posições: `0b00001000`. O OR liga só onde a
máscara tem 1 (onde tem 0, o bit original sobrevive); o AND com `~máscara` (`0b11110111`)
zera só ali; o XOR alterna (0⊕1=1, 1⊕1=0). O sufixo `u` evita surpresas de sinal ao deslocar
o bit 31 (deslocar 1 com sinal para o bit de sinal é comportamento indefinido em C).

![As três operações fundamentais sobre bits de um registrador: SET, CLEAR e TOGGLE](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/bitwise_ops.png)

*Figura 3-A — SET liga só o bit da máscara; CLEAR apaga só ele; TOGGLE inverte. Em vermelho,
o bit alvo; em verde, o que mudou. Tudo o mais passa intacto.*

Campos de **vários** bits pedem o padrão *clear-then-set* — primeiro limpe o campo, depois
escreva o valor novo:

**Exemplo resolvido 3.1 (clear-then-set)** — Configurar em um registrador de 8 bits os bits
7–6 = `10` (modo), preservando o resto: `reg = (reg & ~0xC0) | 0x80;`.

*Passo a passo.* A máscara do campo é `0xC0 = 0b11000000`. (1) `reg & ~0xC0` zera **só** os
bits 7–6 (`~0xC0 = 0x3F = 0b00111111`); (2) `| 0x80` escreve `10` neles
(`0x80 = 0b10000000`). Se pulássemos o passo (1) e fizéssemos só o OR, um valor antigo `11`
viraria `11 | 10 = 11` — errado, o modo não mudou. Este padrão aparece em **todo** driver
que você ler, do ESP-IDF ao Linux: é assim que se edita um campo sem estragar os vizinhos.

> 💡 **Pense aí**: qual a diferença entre `&` e `&&`? *Resposta: `&` opera bit a bit sobre
> inteiros (0b1100 & 0b1010 = 0b1000); `&&` opera sobre verdadeiro/falso e tem curto-circuito
> (não avalia o segundo operando se o primeiro já decide). Trocar um pelo outro compila — e
> gera bugs lógicos silenciosos. Em drivers, você quase sempre quer `&`.*

### 1.3 `volatile`: dizendo a verdade ao compilador

O compilador otimiza assumindo que só *o seu código* altera as variáveis. Para um endereço de
hardware — ou uma variável compartilhada com uma ISR — isso é falso. `volatile` avisa: "este
valor muda por conta própria; **não** elimine leituras, **não** guarde em registrador da CPU,
**não** reordene os acessos".

O bug clássico, que provocaremos no laboratório da semana 4:

```c
uint32_t pronto = 0;                  // setada por uma ISR

void espera(void) {
    while (!pronto) { }               // com -O2, o compilador lê 'pronto' UMA vez,
}                                     // conclui que nunca muda... laço infinito!
```

Do ponto de vista do compilador, o raciocínio é perfeitamente lógico: dentro do laço ninguém
escreve em `pronto`, logo relê-la é desperdício — ele a carrega uma vez num registrador da
CPU e testa para sempre. Com `volatile uint32_t pronto;`, cada iteração relê a memória e o
laço termina quando a ISR escrever. Repare no `src/isr_timer/main.c` da semana 4:
`s_eventos` e `s_t_isr` são `volatile` exatamente por isso. **Atenção**: `volatile` garante
releitura, **não** atomicidade — proteger `x++` compartilhado é assunto de mutex (semana 6).
`volatile` responde “enxergue a mudança”; mutex responde “não misture as mudanças”.

### 1.4 Ponteiros: a ponte entre C e o mapa de memória

Um ponteiro é uma variável que guarda um **endereço**. Em PC, você os usou para estruturas de
dados; em embarcados, eles têm um segundo emprego: **apontar para endereços de hardware**.
Quando a semana 2 escreveu

```c
volatile uint32_t *w1ts = (volatile uint32_t *)GPIO_OUT_W1TS_REG;
*w1ts = BIT_LED;
```

cada peça tem um papel: `GPIO_OUT_W1TS_REG` é um número (um endereço, ex.: `0x3FF44008`); o
*cast* `(volatile uint32_t *)` diz “trate esse número como ponteiro para inteiro de 32 bits
volátil”; `*w1ts = ...` escreve *no endereço apontado* — e o hardware que mora ali obedece.
`&` faz o caminho inverso (o endereço de uma variável), e `p[i]` é uma forma amigável para
`*(p + i)` — é por isso que arrays e ponteiros se confundem em C, e por isso que o teste de
endianness abaixo funciona.

### 1.5 Endianness: a ordem dos bytes na memória

Um `uint32_t` com valor `0x11223344` ocupa 4 bytes — mas em que ordem? **Little-endian**
(ESP32, ARM do RPi por padrão, x86): byte **menos** significativo primeiro → memória contém
`44 33 22 11`. **Big-endian** ("ordem de rede", usada em TCP/IP e em vários protocolos de
sensores/CAN): mais significativo primeiro → `11 22 33 44`.

```
 valor: 0x11223344
 little-endian (ESP32, ARM, x86):  endereço+0  +1  +2  +3
                                   [  44  ][ 33 ][ 22 ][ 11 ]   “pequeno primeiro”
 big-endian (rede, Modbus, CAN):   [  11  ][ 22 ][ 33 ][ 44 ]   “grande primeiro”
```

Quando isso dá problema? Ao **serializar** dados entre sistemas (semanas 9–14): se o ESP32 despeja
uma struct crua na UART e o receptor assume a outra ordem, `0x11223344` vira `0x44332211` —
valores corrompidos **sem nenhum erro de transmissão**, o pior tipo de bug. Por isso existem `htons()/htonl()` (*host-to-network short/long*) e a
regra de ouro: protocolo define a ordem dos bytes **explicitamente**, campo a campo. O
Modbus do Lab Extra, por exemplo, é big-endian — e o enunciado do lab cobra isso de você.

Teste de um minuto que você pode rodar hoje (vale no Wokwi):

```c
uint32_t v = 0x11223344;
uint8_t *p = (uint8_t *)&v;
printf("%02X %02X %02X %02X\n", p[0], p[1], p[2], p[3]);  // ESP32: 44 33 22 11
```

O truque: `&v` é o endereço do inteiro; o cast para `uint8_t*` deixa lermos esse endereço
**byte a byte** — revelando a ordem em que a CPU arrumou os 4 bytes.

---

## 2. GPIO(U4)

GPIO = *General Purpose Input/Output*. Cada pino pode ser saída (a CPU impõe 0 V ou 3,3 V)
ou entrada (a CPU lê a tensão como 0 ou 1). Parece trivial — até você ligar o primeiro
componente de verdade e descobrir que o mundo analógico "cobra pedágio". As próximas quatro
seções são os quatro pedágios: corrente, flutuação, quique e tipo de driver de saída.

Antes, um detalhe que evita 90 % dos “não funciona” de bancada: **onde 0 vira 1?** A entrada
compara a tensão do pino com limiares: abaixo de ~0,8 V lê 0 com certeza; acima de ~2,0 V lê
1 com certeza; entre os dois é a **zona proibida** — leitura imprevisível. A figura abaixo
(do nosso livro de referência de Python/IoT) mostra essas faixas para o GPIO de 3,3 V:

![Faixas de tensão de entrada digital: baixo, flutuante/proibido e alto](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/niveis_logicos.png)

*Figura 3-B — Níveis de entrada digital: leitura garantida de 0 abaixo de ~0,8 V, de 1 acima
de ~2,0 V, e a zona intermediária proibida, onde a leitura é imprevisível. Fonte: Practical
Python Programming for IoT (Packt), cap. 6, Fig. 6.4.*

### 2.1 Saída: acendendo um LED com segurança

Um LED **não** é uma lâmpada: é um diodo, com queda de tensão quase fixa $V_F$ (vermelho
≈ 2,0 V; azul/branco ≈ 3,0 V) e corrente nominal $I_F$ (10–20 mA típicos). Diodo conduz
“de repente”: acima de $V_F$, pequenos aumentos de tensão disparam a corrente. Ligado direto
na fonte, a corrente explode e ele queima — em frações de segundo. O **resistor série**
assume o papel de definir a corrente, absorvendo a diferença de tensão:

```
        R = (V_fonte − V_F) / I_desejada
GPIO ──[R]──▶|── GND        (▶| = LED: anodo no lado do GPIO)
```

**Exemplo resolvido 3.2 (resistor do LED)** — GPIO de 3,3 V, LED vermelho ($V_F = 2,0 V$),
alvo I = 6 mA.

*Solução passo a passo.*

1. Tensão sobre o resistor: a fonte entrega 3,3 V; o LED “toma” 2,0 V; sobram 1,3 V para o
   resistor (é a soma das tensões na malha: 3,3 = 2,0 + V_R).
2. Lei de Ohm: R = V_R / I = 1,3 / 0,006 = 217 Ω → **220 Ω comercial** (série E12).
3. Verificação da corrente real: I = 1,3/220 = 5,9 mA ✔ — dentro do que o GPIO do ESP32
   fornece com folga (limite prático ~12 mA por pino; no RPi é menor: ~8 mA, e há um
   orçamento total de ~50 mA para todos os pinos somados).
4. Verificação da potência no resistor: P = I²·R = 0,0059² × 220 ≈ 7,6 mW ≪ 250 mW do
   resistor de 1/4 W ✔.

O passo 4 parece pedante, mas é o hábito que separa protótipo de produto: **toda conta de
circuito termina com duas verificações** — o componente aguenta a corrente? e a potência?

E quando a carga pede mais que ~10 mA (relé, fita de LED, motor)? O GPIO vira **sinal de
comando** para uma chave eletrônica — transistor BJT ou MOSFET — que conduz a corrente
pesada de outra fonte: o pino entrega mA na base/gate; o transistor entrega amperes à carga.
A semana 8 pratica isso com motores; a conta é a Lista 1, Q13.

### 2.2 Entrada: entradas flutuantes e pull-up/pull-down

Experimento mental: configure um pino como entrada e **não ligue nada**. O que ele lê?
Resposta: **lixo** — 0 e 1 aleatórios, pois o pino de altíssima impedância de entrada
(praticamente não drena corrente) vira uma antena para ruído: a luz do laboratório, seu dedo
se aproximando, a rede elétrica a 60 Hz. Entrada **nunca** fica flutuando.

A solução é um resistor que define o nível de repouso — fraco o suficiente para o botão
vencê-lo, forte o suficiente para dominar o ruído:

![Circuitos com resistor de pull-up, pull-down e entrada flutuante](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/pullup_pulldown.png)

*Figura 3-C — Pull-up: o resistor segura o pino em 1; o botão drena para GND (ativo-baixo).
Pull-down: o oposto. Sem resistor: flutuação — o pino lê ruído.*

```
 PULL-UP (repouso = 1)                PULL-DOWN (repouso = 0)
     3V3                                  GPIO ────┬──── botão ─── 3V3
      │                                            │
     [R ~10k–50k]                                 [R]
      │                                            │
 GPIO ┴──── botão ─── GND                         GND
 (aperta → lê 0: "ativo-baixo")           (aperta → lê 1: "ativo-alto")
```

Por que o botão “vence” o resistor? Com o botão aberto, quase nenhuma corrente flui — a
queda no resistor é ~0 e o pino vê quase 3,3 V (ou quase 0 V, no pull-down). Com o botão
fechado, o pino é curto-circuitado ao outro trilho — o resistor só drena uma corrente
pequenina (3,3 V / 10 kΩ = 0,33 mA) que o botão ignora solenemente.

O ESP32 e o RPi têm resistores **internos** configuráveis (~45 kΩ e ~50 kΩ) — é o que o nosso
código usa: `gpio_pullup_en(BTN)`, dispensando o componente externo. Convenção dominante na
indústria: **pull-up + ativo-baixo** (herança de eletrônica: transistores drenam corrente
para GND melhor do que a fornecem — e o GND é o “rio” comum a todos os chips).

A foto abaixo mostra a montagem real de um botão com resistor de pull-up em protoboard (aqui
com resistor externo de 50–65 kΩ; no nosso laboratório usaremos o pull-up **interno**):

![Botão com resistor de pull-up em protoboard ligado ao GPIO 21](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/botao_pullup.png)

*Figura 3-D — Botão com pull-up: repare no resistor do trilho de 3,3 V ao pino e no botão do
pino ao GND. Fonte: Practical Python Programming for IoT (Packt), cap. 6, Fig. 6.6.*

### 2.3 Bouncing: quando um clique vale dez

Um botão é mecânico: ao fechar/abrir, os contatos metálicos **quicam** — como uma bola
quicando, em escala microscópica — por 1–10 ms, gerando uma rajada de bordas antes de
estabilizar:

```
 tensão no pino ──┐   ┌┐┌┐ ┌┐
  (soltando)      └───┘└┘└─┘└────────  ← quiques de ~1–10 ms antes de estabilizar
                  ↑ apertou   ↑ estabilizou de vez
```

A CPU, que enxerga em microssegundos, conta cada quique como um acionamento: um clique vale
dez. Tratamentos: (i) **software** — ignorar novas transições por uma janela de ~20 ms após a
primeira borda; (ii) **hardware** — filtro RC + Schmitt trigger (custa componentes e espaço;
em produto de consumo, quase sempre se resolve em software, que é grátis).

**Exemplo resolvido 3.3 (debounce por software)** — Botão gera 8 bordas em 4 ms ao ser solto.
Projete o tratamento.

*Solução.* Com janela de 20 ms: a primeira borda dispara a ação e arma
`t_bloqueio = agora + 20 ms`; as 7 bordas seguintes chegam com `agora < t_bloqueio` e são
descartadas → conta-se **1** acionamento ✔. Dimensionamento da janela: **maior** que a
duração do bouncing com folga (20 ms cobre botões tácteis comuns, cujos quiques raramente
passam de 10 ms) e **menor** que a percepção humana (~50–100 ms) para não "engolir" cliques
rápidos legítimos. Margem apertada? Meça seu botão no osciloscópio (ou no Lab 3, com o
próprio ESP32 contando bordas — spoiler do laboratório).

### 2.4 Push-pull × open-drain: como o pino “dirige” a saída

Último pedágio, e o mais sutil: por dentro, a saída digital tem transistores — e há duas
arquiteturas:

![Estágio de saída push-pull versus open-drain](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/pushpull_opendrain.png)

*Figura 3-E — Push-pull: dois transistores, o pino “empurra” 1 e “puxa” 0 com força.
Open-drain: só existe o transistor de baixo; o 1 vem do resistor de pull-up — vários chips
podem dividir o mesmo fio.*

- **Push-pull** (padrão do GPIO): um transistor liga o pino ao VDD (1), outro ao GND (0).
  Saída forte nos dois sentidos — ótimo para LEDs e sinais rápidos. Perigo: **nunca** ligue
  duas saídas push-pull juntas — se uma impõe 1 e a outra 0, criou-se um curto VDD→GND
  através dos dois chips.
- **Open-drain**: só existe o transistor de baixo; o pino ou puxa para 0 ou **solta**
  (alta impedância). O nível 1 vem de um pull-up externo. Parece pior — mas é exatamente o
  que permite o **wired-AND**: vários dispositivos no mesmo fio, qualquer um puxa para 0
  sem curto. É a base elétrica do I2C (semana 9) e a razão dos pull-ups obrigatórios do
  Lab 9. Anote; daqui a seis semanas isso vira circuito na sua bancada.

### 2.5 O firmware da semana, linha a linha

Junte tudo e você lê `src/botao_led/main.c` sem tropeçar (este é o código do laboratório):

```c
#define LED   GPIO_NUM_2
#define BTN   GPIO_NUM_0
#define DEBOUNCE_MS 20

void app_main(void)
{
    gpio_reset_pin(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);      // seção 2.1: LED com R de 220 Ω

    gpio_reset_pin(BTN);
    gpio_set_direction(BTN, GPIO_MODE_INPUT);
    gpio_pullup_en(BTN);                            // seção 2.2: repouso=1, apertado=0

    int  led = 0, eventos = 0;
    int  nivel_ant = 1;                             // memória do nível anterior
    int64_t t_ok = 0;                               // fim da janela de debounce

    while (1) {
        int nivel = gpio_get_level(BTN);
        int64_t agora = esp_timer_get_time() / 1000;          // relógio em ms
        if (nivel_ant == 1 && nivel == 0 && agora >= t_ok) {  // borda 1→0 VÁLIDA
            led = !led;
            gpio_set_level(LED, led);
            printf("evento #%d\n", ++eventos);
            t_ok = agora + DEBOUNCE_MS;             // Exemplo 3.3: arma a janela
        }
        nivel_ant = nivel;
        vTaskDelay(pdMS_TO_TICKS(2));               // varredura a ~500 Hz (POLLING)
    }
}
```

Três detalhes de projeto para discutir em aula:

- **Detecção de borda por software**: comparamos `nivel_ant` com `nivel` — só a *transição*
  1→0 interessa, não o nível (senão o LED alternaria 500×/s enquanto o botão estivesse
  pressionado). Borda = evento; nível = estado. Sistemas reagem a eventos.
- **A janela de debounce é uma comparação de tempo, não um delay**: o laço nunca para;
  apenas descarta bordas prematuras. Bloquear com `vTaskDelay(20)` dentro do `if` também
  funcionaria aqui, mas congelaria tudo o mais que o laço fizesse — má prática que a
  semana 5 substituirá por tarefas.
- **Isto é *polling* a 500 Hz** — a CPU pergunta 500×/s "mudou?". Funciona, mas tem custo e
  latência de até 2 ms. A pergunta "e se o evento durar menos de 2 ms?" abre a semana 4
  (interrupções) — guarde-a.

E a montagem correspondente, para quem quiser reproduzi-la também no Raspberry Pi (Lab 12):

![Circuito completo de botão e LED em protoboard ligado ao Raspberry Pi](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/circuito_botao_led.png)

*Figura 3-F — Botão + LED + resistores em protoboard: o mesmo circuito que o
`botao_led/main.c` comanda, aqui fotografado num Raspberry Pi. Fonte: Practical Python
Programming for IoT (Packt), cap. 2, Fig. 2.7.*

---

## Resumindo

- Firmware usa **tipos de largura fixa** (`uintN_t`); `int` fica proibido em
  drivers/protocolos.
- Máscaras: `|=` seta, `&= ~` limpa, `^=` inverte, `&` testa; campos multi-bit usam
  *clear-then-set* (Exemplo 3.1).
- `volatile` = "muda por conta própria": obrigatório em endereços de hardware e variáveis de
  ISR; **não** dá atomicidade.
- Ponteiro + cast para endereço físico = como se fala com registradores; `&` e byte-a-byte
  revelam a **endianness** (ESP32: little; rede/Modbus: big).
- Entrada digital tem zona proibida (~0,8–2,0 V): garanta níveis fora dela.
- LED exige resistor série: R = (V_fonte − V_F)/I (Exemplo 3.2) + verificação de corrente
  no pino e potência no resistor; cargas > ~10 mA exigem transistor.
- Entrada nunca flutua: pull-up (padrão, ativo-baixo) ou pull-down; internos disponíveis.
- Botões "quicam" 1–10 ms: janela de debounce ~20 ms (Exemplo 3.3), por comparação de tempo,
  sem bloquear.
- Saída push-pull é forte mas exclusiva; open-drain + pull-up permite barramentos
  compartilhados (I2C, semana 9).

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| máscara | valor com 1s onde se quer atuar num registrador |
| clear-then-set | zerar o campo antes de escrever o valor novo |
| endianness | ordem dos bytes de um inteiro na memória |
| V_F | queda de tensão direta do LED/diodo |
| pull-up / pull-down | resistor que define o nível de repouso da entrada |
| ativo-baixo | lógica em que “acionado” = nível 0 |
| bouncing | quique mecânico dos contatos de uma chave |
| debounce | tratamento (soft/hard) do bouncing |
| push-pull / open-drain | arquiteturas do estágio de saída digital |
| polling | verificar o periférico repetidamente em laço |

## 📖 Onde aprofundar (opcional)

- **Molloy**, *Exploring Raspberry Pi*, cap. 4 (eletrônica de interfaceamento — LEDs,
  transistores, botões, bouncing com oscilografias reais) e cap. 5 (C embarcado).
- ***Hacking Electronics*** (Monk), caps. 2–3 — componentes e protoboard.
- ***Guia Completo de C para Sistemas Embarcados*** (Dias & Moody) — manipulação de bits,
  `stdint.h` e `volatile` em português.
- **Upton & Duntemann**, cap. 5 — compilação, linguagens e endianness no ARM.

## Exercícios

Lista 1, questões 11–15 (inclui os cálculos de LED e debounce — os Exemplos 3.2 e 3.3 são o
gabarito do método; a Q13 é a conta do transistor).
