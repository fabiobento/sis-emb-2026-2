# Aula 8 — Geração PWM e Acionamento de Atuadores (U4) • Semana da P1

> **Pré-requisito**: Aulas 3 (transistor, GPIO) e 7 (DAC — o caminho "analógico de verdade").
> **Como usar**: texto autossuficiente. Os Exemplos 8.1–8.3 são o modelo das questões 6–10 da
> Lista 3. Segundo encontro da semana: **Prova P1** (semanas 1–6).

O DAC da semana passada entrega tensões analógicas — mas com 8 bits, pouca corrente e só 2
pinos. Quando o objetivo é **entregar potência de forma ajustável** (dimerizar um LED,
controlar a velocidade de um motor, posicionar um servo), a indústria usa outro truque, mais
robusto e mais eficiente: mentir para a carga **muito rápido**. É o **PWM** (*Pulse Width
Modulation*), item explícito da ementa e a espinha dorsal de toda a eletrônica de potência
que vocês verão no curso. Na segunda parte, os elétrons engrossam: transistores, ponte H e
as regras para não soltar fumaça. Fechamos a semana com a **Prova P1** (semanas 1–6).

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) explicar PWM (frequência, duty, resolução) e a troca resolução × frequência do LEDC;
- (b) gerar o sinal de servo e converter ângulo→duty;
- (c) explicar por que motor não liga em GPIO e o papel da ponte H, do diodo de roda-livre e
  do GND comum;
- (d) escolher a frequência de PWM para não cintilar nem apitar.

---

## 1. PWM: o analógico dos apressados

A ideia: chavear a saída entre 0 e 3,3 V num período fixo T, controlando a **fração do tempo
em nível alto** — o *duty cycle* D:

```
 D = 25 %:  ▔▔____________▔▔____________      valor médio = 0,25 · 3,3 = 0,825 V
 D = 75 %:  ▔▔▔▔▔▔▔▔▔▔____▔▔▔▔▔▔▔▔▔▔____     valor médio = 0,75 · 3,3 = 2,475 V
            |←──── T ────→|
```

![Formas de onda de PWM com diferentes duty cycles e suas tensões médias](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/pwm_tensao_media.png)

*Figura 8-A — Quatro duty cycles, quatro “tensões analógicas” diferentes (linhas
tracejadas): a carga lenta responde à média D × 3,3 V.*

Se a carga for **lenta** comparada a T (o olho humano, a inércia do motor, um filtro RC), ela
responde ao **valor médio** — e enxerga um "analógico" de amplitude D·V. As vantagens sobre
um DAC de potência: o transistor de saída trabalha só **totalmente ligado ou totalmente
desligado** (dissipação mínima — um transistor ligado tem V≈0 sobre si, um desligado tem
I≈0; nos dois casos P = V·I ≈ 0. É por isso que fontes chaveadas e inversores dominam o
mundo), e o duty é um número digital imune a ruído — a informação está no *tempo*, não na
amplitude.

A mesma física, agora medida no mundo real — formas de onda PWM de fato capturadas em
bancada, com duty de 50 %, 75 % e 25 %:

![Oscilogramas de PWM com duty cycles de 50, 75 e 25 por cento](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/pwm_duty_cycles.png)

*Figura 8-B — Duty cycles reais no osciloscópio: 50 %, 75 % e 25 %. Fonte: Practical Python
Programming for IoT (Packt), cap. 5, Fig. 5.2.*

### 1.1 O periférico LEDC do ESP32: resolução × frequência

O ESP32 tem 16 canais de PWM no periférico **LEDC**. Um timer conta de 0 a 2^N − 1 com clock
de 80 MHz; a saída fica em 1 enquanto contagem < duty. Daí a **troca fundamental**:

f_PWM = 80 MHz / 2^N  (máxima, para N bits de resolução)

Mais resolução (N grande) ⇒ contagem mais longa ⇒ frequência máxima menor. Você escolhe
**dois** dos três (clock é fixo); o terceiro é consequência. (É a mesma matemática do timer
da semana 4: período = prescaler × contagens ÷ clock — só que aqui o “prescaler” é a
resolução.)

**Exemplo resolvido 8.1 (dimmer de LED)** — Projete o PWM para um dimmer sem cintilação
visível.

*Solução passo a passo.*

1. Frequência mínima: o olho funde cintilações acima de ~90 Hz; câmeras de celular (30/60
   fps) captam faixas com PWM lento. **f ≥ 200 Hz** basta; escolhemos **5 kHz** — fora
   também da faixa audível de assobios de indutor (abaixo de ~20 kHz, bobinas “cantam”).
2. Resolução: queremos rampa suave → **12 bits** (4096 degraus de brilho).
3. Verificação da troca: 80 MHz/2¹² = 19,5 kHz ≥ 5 kHz ✔ — cabe.
4. Duty de 25 %: 0,25 × 4096 = **1024 contagens**; passo de ajuste de brilho: 1/4096 ≈
   0,024 % — imperceptível, rampa suave garantida.

É exatamente a configuração de `src/ledc_dimmer/main.c`:

```c
ledc_timer_config_t t = {
    .speed_mode = LEDC_LOW_SPEED_MODE, .timer_num = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_12_BIT,   // N = 12
    .freq_hz = 5000,                        // Exemplo 8.1
    .clk_cfg = LEDC_AUTO_CLK };
ledc_timer_config(&t);
ledc_channel_config_t c = {
    .gpio_num = PIN, .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0, .duty = 0 };
ledc_channel_config(&c);
// no laço: rampa de brilho
ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, d);   // 0..4095
ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);   // aplica (dupla escrita = sem glitch)
```

Note a arquitetura **timer + canal**: um timer define f e N; vários canais podem
compartilhá-lo com dutys independentes (três LEDs RGB = 1 timer + 3 canais).

> **Observação — por que o par set/update?** `ledc_set_duty` escreve num registrador-sombra;
> `ledc_update_duty` transfere no início do próximo período. Sem isso, mudar o duty no meio
> do período criaria um pulso "quebrado" (glitch) — o mesmo cuidado dos registradores W1TS
> da semana 2, agora contra glitches temporais.

### 1.2 A frequência certa para cada carga

| Carga | f recomendada | motivo |
|---|---|---|
| LED (olho humano) | ≥ 200 Hz (usamos 5 kHz) | fusão visual ~90 Hz; câmeras batem em 30/60 fps |
| Motor DC pequeno | 1–20 kHz | abaixo de ~1 kHz vibra/assobia; muito alto = perdas de chaveamento |
| Servo-modelismo | **exatamente 50 Hz** | é protocolo, não filtragem (ver seção 2) |

A questão 10 da Lista 3 pede o diagnóstico do PWM de 30 Hz filmado por celular — você já tem
todos os elementos (spoiler: o obturador da câmera amostra a luz a 30/60 Hz; o PWM de 30 Hz
está na fronteira — e o aliasing da semana 7 aparece na tela como faixas escuras móveis).

## 2. Servomotor: PWM como protocolo

No servo de modelismo (nosso SG90), o PWM não transporta potência — transporta
**informação**. O servo espera um pulso a cada 20 ms (50 Hz) e lê a **largura** do pulso como
posição: ~0,5 ms = −90°, ~1,5 ms = centro, ~2,4 ms = +90° (faixas variam por unidade; por
isso o firmware tem `PULSO_MIN/MAX` ajustáveis). A eletrônica interna fecha a malha de
posição sozinha — você manda o alvo, ele se vira. É um sistema de controle completo dentro
de uma caixinha de 9 g.

![Mapeamento entre largura de pulso e ângulo do servo](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/servo_pulsos.png)

*Figura 8-C — O idioma do servo: pulsos de ~1 ms a ~2 ms a cada 20 ms codificam −90° a +90°.
Fonte: Practical Python Programming for IoT (Packt), cap. 10, Fig. 10.4.*

![Servos de modelismo: corpo, braços e conector de três fios](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/servos.png)

*Figura 8-D — Servos reais: marrom = GND, vermelho = VCC (5–6 V — **não** alimente do 3,3 V
do ESP32!), laranja/amarelo = sinal. Fonte: Practical Python Programming for IoT (Packt),
cap. 10, Fig. 10.1.*

**Exemplo resolvido 8.2 (servo: de ângulo a duty)** — Servos padrão (SG90): pulso de 1–2 ms
a cada 20 ms (50 Hz). Calcule os dutys para 14 bits e a resolução angular.

*Solução passo a passo.*

1. Viabilidade: LEDC a 50 Hz e **14 bits** — 80 M/2¹⁴ = 4,88 kHz ≥ 50 Hz ✔ (resolução máxima
   disponível para essa frequência).
2. Cada contagem vale 20 000 µs/16 384 ≈ 1,22 µs.
3. Convertendo pulsos em contagens: duty = pulso_µs × 16 384/20 000 → 1 ms → **819**;
   1,5 ms → **1229**; 2 ms → **1638**.
4. Resolução angular: a faixa de 180° cabe em 1638 − 819 = 819 contagens ⇒ 180/819 ≈
   **0,22°/LSB** — mais fina que a folga mecânica do SG90 (o servo é o elo fraco, não o
   sinal).

A função `angulo_para_duty()` do firmware é essa conta empacotada:

```c
static uint32_t angulo_para_duty(int ang)   // ang em [-90, +90]
{
    uint32_t pulso = PULSO_MIN + (uint32_t)(ang + 90) * (PULSO_MAX - PULSO_MIN) / 180;
    return (uint32_t)((uint64_t)pulso * 16384 / PERIODO_US);   // µs → contagens de 14 bits
}
```

(O cast para `uint64_t` documenta o cuidado com overflow: em aritmética de duty, os
produtos intermediários crescem rápido — 2400 × 16384 ≈ 39 milhões. Cabe em 32 bits? Cabe —
mas o hábito de checar overflow em aritmética intermediária é o que separa firmware de
susto. Regra: some as ordens de grandeza **antes** de escolher o tipo.)

## 3. Motores DC: quando os elétrons engrossam

### 3.1 Por que não ligar direto no GPIO — três motivos

1. **Corrente**: nosso motor TT drena ~200 mA nominais (e >1 A no arranque/travado — motor
   parado é quase um curto); o GPIO fornece ~12 mA. Ligar direto afunda a tensão e pode
   danificar o pino.
2. **Tensão**: motor de 6 V não anda direito com 3,3 V.
3. **Indutância**: motor é bobina. Ao desligar a corrente, `V = −L·di/dt` gera um **pico
   reverso** de dezenas de volts que fulmina o transistor/pino (a bobina “se vinga” de quem
   a interrompe — a energia guardada no campo magnético precisa ir para algum lugar).
   Antídoto: **diodo de roda-livre** em antiparalelo, dando caminho para a corrente morrer
   em paz, circulando pela própria bobina até dissipar.

A solução mínima é um transistor (BJT ou MOSFET) como **chave**: o GPIO comanda; a corrente
pesada flui da fonte do motor pela chave. Para também **inverter o sentido**, quatro chaves
em **ponte H**:

![Os três estados de uma ponte H: frente, ré e o proibido shoot-through](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/ponte_h.png)

*Figura 8-E — A ponte H: fechar Q1+Q4 gira para um lado; Q2+Q3, para o outro; Q1+Q3 (ou
Q2+Q4) juntos = curto direto na fonte (*shoot-through*) — a combinação proibida que o
driver comercial impede por hardware.*

O **L298N** do nosso inventário embala duas pontes H prontas (com diodos), entradas lógicas
IN1/IN2 (sentido) e ENA (habilita — onde entra o **PWM de velocidade**). O esquemático
abaixo mostra um driver da mesma família (L293D) ligado a dois motores — repare nas duas
alimentações separadas (lógica e motores) e nos quatro GPIOs de comando por motor:

![Esquemático de driver de motor L293D com dois motores](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/l293d_esquematico.png)

*Figura 8-F — Driver L293D (irmão menor do L298N): VCC1 alimenta a lógica, VCC2 alimenta os
motores; cada par de saídas é uma ponte H. Fonte: Practical Python Programming for IoT
(Packt), cap. 10, Fig. 10.5.*

O firmware `src/motor_l298n/main.c` resume o contrato:

```c
static void sentido(int frente) { gpio_set_level(IN1, frente); gpio_set_level(IN2, !frente); }
static void velocidade(int pct) {   // ENA no canal 2 do LEDC, 10 bits @ 1 kHz
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, pct * 1023 / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}
```

### 3.2 As contas de potência — e a regra sagrada do GND comum

**Exemplo resolvido 8.3 (orçamento de tensão e potência)** — Motor TT a 5 V consome 200 mA
nominal; o L298N tem queda típica ~1,4 V por caminho (é um CI antigo, de transistores
bipolares Darlington). Alimentando a ponte com bateria 2S (7,4 V), qual a tensão no motor e
a dissipação na ponte?

*Solução passo a passo.*

1. V no motor ≈ 7,4 − 1,4 = **6,0 V** — perfeito para um motor de 6 V (eis por que o L298N
   "gosta" de baterias 2S: a queda dele vira desconto útil).
2. Potência torrada na ponte: P ≈ 1,4 V × 0,2 A = **0,28 W** por motor (dissipador morno).
3. Comparação com drivers MOSFET modernos (TB6612, DRV8833): R_DS(on) de dezenas de mΩ →
   P = I²R ≈ 0,04 × 0,05 = **2 mW** — cem vezes menos; a queda some e o motor recebe a
   tensão quase cheia. O L298N segue nos kits por ser robusto, didático — e por já estar no
   nosso armário.

**Regra sagrada**: fonte do motor e fonte do ESP32 são separadas, mas os **GNDs se unem**
num ponto. Sem referência comum, os níveis de IN1/IN2/ENA ficam "flutuando" entre os dois
circuitos — o motor faz o que quer (o “0” do ESP32 não é “0” para o L298N). É o erro nº 1 da
bancada; o roteiro do lab começa por esse fio.

> 💡 **Checklist anti-fumaça da bancada** (use em todo acionamento):
> 1. GND comum conectado **primeiro**?
> 2. Motor na saída do driver, nunca no GPIO?
> 3. Diodos de roda-livre presentes (ou dentro do CI)?
> 4. Fonte do motor com corrente de sobra (arranque = 3–5× nominal)?
> 5. Lógica IN1/IN2 nunca em (1,1) ou (0,0) confundida com freio — confira o datasheet do
>    seu driver.

## 4. Prova P1 (segundo encontro da semana)

Cobre as semanas 1–6: restrições e plataformas; arquitetura, memórias e registradores; C
embarcado e GPIO; interrupções e timers; FreeRTOS (tarefas e IPC). **Estude pelas
Listas 1 e 2 e refazendo os Exemplos resolvidos 1.1–6.3** — as questões seguem exatamente
esse estilo. Consulta: 1 folha A4 manuscrita (frente e verso). Calculadora liberada.

---

## Resumindo

- PWM = valor médio por chaveamento: eficiente (chave só liga/desliga ⇒ P ≈ 0) e digitalmente
  preciso; LEDC troca resolução por frequência: f_max = 80 MHz/2^N (Exemplo 8.1).
- Frequência por carga: LED ≥ 200 Hz (usamos 5 kHz), motor 1–20 kHz, servo **50 Hz por
  protocolo** — largura do pulso = posição; 14 bits dão 0,22°/LSB (Exemplo 8.2).
- Motor no GPIO: nunca (corrente, tensão, indutância). Transistor como chave; ponte H para
  inverter; diodo de roda-livre sempre; **GND comum** entre fontes, religiosamente.
- L298N: robusto, mas queima 1,4 V — conta de projeto do Exemplo 8.3; MOSFETs modernos
  quase não dissipam.
- P1: Listas 1–2 + Exemplos 1.1–6.3 = o mapa do tesouro.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| duty cycle | fração do período em nível alto |
| LEDC | periférico PWM do ESP32 (16 canais) |
| resolução (N bits) | nº de degraus do duty (2^N) |
| glitch | pulso quebrado por troca de duty no meio do período |
| servo | atuador com malha de posição interna (pulso 0,5–2,4 ms/20 ms) |
| ponte H | 4 chaves que invertem a corrente no motor |
| diodo de roda-livre | caminho para a corrente indutiva morrer |
| shoot-through | curto na fonte por chaves do mesmo lado ligadas |
| GND comum | referência compartilhada entre circuitos |

## 📖 Onde aprofundar (opcional)

- **Molloy**, *Exploring Raspberry Pi*, caps. 4 e 10 — transistores, cargas indutivas,
  motores DC e ponte H.
- ***Hacking Electronics*** (Monk), cap. 3 — transistores e MOSFETs com fotos.
- **Smart**, *Practical Python Programming for IoT*, caps. 7 e 10 — os mesmos circuitos em
  Python/RPi (útil no Bloco 2 e no projeto final).

## Exercícios

Lista 3, questões 6–10 (estilo dos Exemplos 8.1–8.3) — e revisão geral para a P1.
