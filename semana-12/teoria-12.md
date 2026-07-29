# Aula 12 — Interfaceamento Físico no Linux Embarcado (U4)

> **Pré-requisito**: Aula 11 (Linux, kernel/usuário, comandos) e os circuitos do Bloco 1
> (semanas 3 e 7: botão, divisor, ADC).
> **Como usar**: texto autossuficiente. Os Exemplos 12.1–12.3 são o modelo das questões 7–12
> da Lista 4. Imprima o pinout da seção 1 e cole na bancada — de verdade.

Semana passada você dominou o *sistema*; hoje o RPi encosta no mundo físico. A pergunta que
organiza a aula: **tudo que fizemos no ESP32 (GPIO, botão, sensores, barramentos) — como fica
sob um sistema operacional?** A resposta tem três camadas de software para o mesmo pino, um
periférico que simplesmente *não existe* no RPi (o ADC!) e uma pegadinha elétrica que já
queimou muito GPIO por aí (o ECHO de 5 V do HC-SR04). No laboratório, o mesmo botão da semana
3 reaparece — em **3 linhas de Python** — e você medirá com estatística o preço do jitter do
SO.

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) usar o header de 40 pinos com a numeração BCM, sem fritar nada;
- (b) escolher entre gpiozero, libgpiod e sysfs conforme o caso;
- (c) resolver a ausência de ADC por três caminhos;
- (d) interfacear HC-SR04 (com divisor!) e DHT11;
- (e) usar as ferramentas I2C do Linux.

---

## 1. O cabeçalho de 40 pinos: leia antes de ligar

As regras de ouro, na ordem dos acidentes mais comuns:

1. **Lógica de 3,3 V — 5 V queima o SoC.** Diferente do ESP32 (que ao menos tem diodos mais
   tolerantes), o BCM2837 não perdoa: um único fio de 5 V num GPIO pode aposentar a placa.
   Todo sinal de 5 V entra por divisor ou conversor de nível (seção 4).
2. **Duas numerações**: a **física** (1–40, o zigue-zague do conector, contado a partir do
   pino quadrado) e a **BCM** (o número do pino *no chip* Broadcom). As bibliotecas usam
   **BCM** — quando o roteiro diz "BCM 17", é o pino físico 11. A figura abaixo mostra os
   dois esquemas lado a lado — estude-a até a confusão desaparecer:

![Esquemas de numeração dos pinos GPIO: físico (BOARD) versus BCM](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/gpio_numeracao.png)

*Figura 12-A — Os dois esquemas de numeração: físico (a posição no conector) × BCM (o nome do
pino no chip). As bibliotecas falam BCM. Fonte: Practical Python Programming for IoT (Packt),
cap. 5, Fig. 5.1.*

3. **Pinos com segundo emprego**: BCM 2/3 são o I2C1 (e já têm pull-ups de 1,8 kΩ **na
   placa** — não adicione outros; dois módulos com pull-ups próprios, ok, resistores
   externos novos, não); BCM 14/15 são a UART do console; SPI0 em BCM 9–11. Evite-os para
   GPIO genérico.
4. **Orçamento de corrente**: ~8 mA confortáveis por pino, ~50 mA no total dos GPIOs. LED
   com resistor ok; qualquer coisa maior, transistor (a semana 8 vale aqui também — elétrons
   não sabem em que sistema operacional estão).

O pôster abaixo é o mapa completo do cabeçalho (vale imprimir em A3 e pendurar no
laboratório): cada pino com seu número físico, nome BCM e funções alternativas.

![Pôster de referência do cabeçalho GPIO do Raspberry Pi](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/rpi_poster_gpio.png)

*Figura 12-B — O mapa completo do cabeçalho de 40 pinos: numeração física, nomes BCM,
alimentações (3V3, 5V, GND) e funções alternativas (I2C, SPI, UART, PWM). Fonte: pôster
oficial do livro Exploring Raspberry Pi (Wiley).*

> 💡 **Regra de sobrevivência**: antes de ligar qualquer fio, responda em voz alta: “que
> número físico é este? que BCM é? qual a tensão máxima que esse sinal pode assumir?” Três
> segundos de pergunta valem uma placa de 300 reais.

## 2. Três camadas para o mesmo pino

No ESP32 havia um caminho (o driver do ESP-IDF sobre os registradores). No Linux, o kernel é
o dono do hardware (semana 11!) e oferece **interfaces** — três gerações delas:

![Pilha de software entre o código Python e o pino físico no Linux](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/stack_gpio_linux.png)

*Figura 12-C — A pilha completa: seu `led.on()` desce por gpiozero → libgpiod →
`/dev/gpiochip0` → driver do SoC → registradores. Cada camada para baixo é mais rápida e
menos confortável.*

1. **sysfs** (`/sys/class/gpio`) — o "tudo é arquivo" aplicado a GPIO. Didático (você já
   brincou com o LED assim na semana 11), porém **legado**: lento e removido dos kernels
   novos. Conhecer para ler código antigo; não usar em projeto novo.
2. **Character device** `/dev/gpiochip0` + **libgpiod** — o caminho moderno oficial: API em
   C (e ferramentas de linha de comando: `gpioset`, `gpioget`, `gpioinfo`), rápida *para
   Linux*, com dono declarado por linha (o kernel sabe *quem* está usando o pino — adeus
   conflitos silenciosos).
3. **gpiozero** (Python) — a camada de conforto sobre a anterior: objetos prontos (`LED`,
   `Button`, `DistanceSensor`, `MotionSensor`…) com debounce, eventos e boas práticas
   embutidos.

Compare o custo cognitivo — o botão-alterna-LED da semana 3, que exigiu ~35 linhas de C e
uma discussão de bordas e debounce, vira isto em gpiozero (o `led_botao.py` do lab,
completo):

```python
from gpiozero import LED, Button
from signal import pause

led = LED(17)                          # BCM 17 (pino físico 11)
btn = Button(27, bounce_time=0.02)     # BCM 27, pull-up interno + debounce de 20 ms PRONTOS
btn.when_pressed = led.toggle          # programação por EVENTO: sem laço, sem polling
pause()                                # dorme para sempre; os eventos fazem o resto
```

Três linhas úteis. O `Button` já configurou pull-up, detecção de borda e a janela de 20 ms
que você *deduziu e mediu* no Lab 3 — e é exatamente por ter feito na unha que você sabe o
que a biblioteca esconde (pergunta de relatório!). O modelo `when_pressed` é orientado a
eventos: o análogo em espírito da nossa ISR, só que com o kernel e a biblioteca no meio do
caminho — e com a latência de ambos.

E qual camada usar? A régua do curso:

**Exemplo resolvido 12.1 (velocidade das camadas)** — Toggle máximo típico no RPi 3:
Python/gpiozero ≈ dezenas de kHz; C/libgpiod ≈ centenas de kHz; acesso a registrador mapeado
≈ dezenas de MHz. Mas atenção à letra miúda: **nenhum** deles é determinístico (Exemplo
11.1) — o jitter de ms pode ocorrer em qualquer camada, pois o problema é o escalonador do
SO, não a linguagem. Logo: protótipo/aula ⇒ gpiozero; daemon em produção ⇒ libgpiod;
"preciso de µs garantidos" ⇒ não é caso de RPi, é caso de MCU (ou PREEMPT_RT, com
ressalvas).

## 3. O periférico que falta: sem ADC nativo

O BCM2837 foi projetado para multimídia — **não tem ADC**. O LDR da semana 7, ligado direto,
não tem onde entrar. Três saídas, em ordem de preferência prática:

1. **Sensor já digital**: DHT11 (temperatura/umidade), DS18B20, MPU-6050 — o sensor embute o
   ADC e entrega números por protocolo. É o caminho do lab de hoje.
2. **ADC externo no barramento**: MCP3008 (SPI, 8 canais, 10 bits) ou ADS1115 (I2C, 16
   bits) — o padrão "periférico que falta → CI externo" que resolve qualquer sensor
   analógico. O módulo abaixo é o ADS1115: quatro canais, 16 bits, I2C — note os quatro
   pinos à direita (VDD, GND, SDA, SCL) e os quatro canais analógicos à esquerda:

![Módulo ADS1115: ADC de 16 bits com interface I2C](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/ads1115.png)

*Figura 12-D — O ADS1115: o ADC que o RPi não tem, servido via I2C. Fonte: Practical Python
Programming for IoT (Packt), cap. 5, Fig. 5.3.*

E o LDR da semana 7 ligado nele — o mesmo divisor de tensão, agora medido com resolução de
16 bits:

![Esquemático do LDR ligado ao ADS1115 no Raspberry Pi](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/ldr_ads1115_esquema.png)

*Figura 12-E — LDR + ADS1115 no RPi: o divisor da semana 7, o ADC externo e o barramento
I2C — três semanas de conteúdo num esquemático só. Fonte: Practical Python Programming for
IoT (Packt), cap. 9, Fig. 9.5.*

3. **Delegar a um MCU**: o ESP32 amostra (bem, com tempo real) e envia ao RPi por
   serial/MQTT — spoiler da semana 14, e a arquitetura de produto de verdade.

## 4. HC-SR04: distância por ultrassom (e a armadilha dos 5 V)

O HC-SR04 mede distância cronometrando um eco: pulso de 10 µs em TRIG → o sensor emite 8
ciclos de 40 kHz → **ECHO fica em nível alto pelo tempo de voo** do som (ida e volta):

d = v_som · t_ECHO / 2,  com v_som ≈ 343 m/s

![Módulo HC-SR04: transmissor e receptor ultrassônicos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/hcsr04_modulo.png)

*Figura 12-F — O HC-SR04: os dois "olhos" são o transmissor (T) e o receptor (R) de 40 kHz;
os quatro pinos são VCC (5 V), TRIG, ECHO e GND. Fonte: Practical Python Programming for
IoT (Packt), cap. 11, Fig. 11.4.*

![Diagrama temporal do funcionamento do HC-SR04: disparo, emissão e eco](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/ultrassom_funcionamento.png)

*Figura 12-G — O princípio de operação: o pulso de TRIG dispara a rajada ultrassônica; a
largura do ECHO mede o tempo de voo de ida e volta. Fonte: Practical Python Programming
for IoT (Packt), cap. 11, Fig. 11.5.*

A armadilha: o sensor é alimentado em **5 V** e o ECHO responde **em 5 V** — direto no GPIO
do RPi, é roleta-russa (regra 1 da seção 1). O divisor de tensão da semana 7 volta como
**conversor de nível**:

```
 ECHO(5V) ──[ 1 kΩ ]──┬──▶ GPIO (BCM 24)
                      │
                    [ 2 kΩ ]
                      │
                     GND
```

**Exemplo resolvido 12.2 (divisor do ECHO)** — V_out = 5·2k/(1k+2k) = **3,33 V** ✔ (no
limite superior confortável — abaixo de 3,3 V + tolerância). Corrente drenada do pino ECHO:
5/3k ≈ **1,7 mA** — desprezível para o driver do sensor. (TRIG não precisa de nada: os 3,3 V
do GPIO são "alto" suficiente para a entrada do sensor, cujo limiar típico de V_IH é ~2 V.)

A montagem completa — repare no divisor escondido entre o fio do ECHO e o GPIO:

![Montagem do HC-SR04 em protoboard com divisor de tensão no ECHO](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/hcsr04_circuito.png)

*Figura 12-H — O circuito do laboratório: TRIG direto no GPIO, ECHO passando pelo divisor
1k/2k antes de tocar o RPi. Fonte: Practical Python Programming for IoT (Packt), cap. 11,
Fig. 11.6.*

**Exemplo resolvido 12.3 (distância e incerteza)** — O `DistanceSensor` do gpiozero
cronometra o ECHO por você. Pulso de 2,33 ms ⇒ d = 343·2,33·10⁻³/2 ≈ **0,40 m**. E a
incerteza? No MCU, a resolução de 1 µs do timer dava ±0,2 mm (Exemplo 4.3); no RPi, quem
carimba as bordas é um processo de usuário sujeito ao escalonador ⇒ jitter de ± dezenas de
µs ⇒ **± ~1 cm** de espalhamento — que você medirá com 100 amostras e desvio-padrão no lab.
Mesma física, plataformas diferentes, incertezas diferentes: eis o Bloco 2 resumido num
sensor.

## 5. DHT11 no Linux: quando o SO atrapalha

O DHT11 fala um protocolo proprietário de **1 fio com bits de 26–70 µs** — exatamente a
escala de tempo em que o Linux não dá garantias (Exemplo 11.1). Consequência prática:
leituras falham de vez em quando (checksum inválido) e **isso é normal**; a biblioteca (e o
nosso `dht11_log.py`) simplesmente captura a exceção e tenta de novo no próximo ciclo:

```python
try:
    t, h = SENSOR.temperature, SENSOR.humidity
    w.writerow([int(time.time()), t, h]); f.flush()
except RuntimeError as e:      # falha de leitura: esperada no Linux — registre e siga
    print("leitura falhou:", e)
```

![Sensores DHT11 e DHT22: pinagem e aparência](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/dht_sensores.png)

*Figura 12-I — A família DHT: o 11 (azul, ±2 °C, ±5 %UR) e o 22 (branco, mais preciso).
Pinos: VCC, DATA (com pull-up!), NC, GND. Fonte: Practical Python Programming for IoT
(Packt), cap. 9, Fig. 9.1.*

![Circuito do DHT11 ligado ao Raspberry Pi com resistor de pull-up no pino de dados](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/dht_circuito.png)

*Figura 12-J — A montagem: o pino DATA pede pull-up de ~10 kΩ (dreno aberto — a semana 3
aparecendo de novo). Fonte: Practical Python Programming for IoT (Packt), cap. 9,
Fig. 9.3.*

Guarde o padrão: **em Linux, driver de sensor com timing crítico = tolerância a falha + nova
tentativa** (ou, melhor ainda, sensores I2C sem timing crítico — BME280 — ou delegação ao
MCU). E para quem nunca montou circuito em protoboard, o mapa dela:

![Anatomia de uma protoboard: trilhos de alimentação e fileiras conectadas](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/protoboard.png)

*Figura 12-K — A protoboard por dentro: as colunas laterais são trilhos contínuos (+/−); as
fileiras centrais conectam 5 furos em linha, separadas pela canaleta do meio. Fonte:
Practical Python Programming for IoT (Packt), cap. 2, Fig. 2.1.*

## 6. I2C sob Linux: os velhos conhecidos de terno novo

Habilitado o I2C (Lab 11), o barramento da semana 9 reaparece como... arquivos e utilitários:

```bash
i2cdetect -y 1        # a varredura 0x03–0x77 — o NOSSO scanner, versão de fábrica
i2cget -y 1 0x68 0x75 # lê o WHO_AM_I do MPU-6050: responde 0x68!
```

`i2cdetect` desenha a tabela de endereços vivos; `i2cget/i2cset` leem/escrevem registradores
avulsos — perfeitos para reconhecimento antes de escrever código. Em Python, `smbus2` faz o
papel do nosso par `wr()/rd()`. Mesmo protocolo, mesmos endereços, mesma lógica de mapa de
registradores: a semana 9 inteira se transfere — mais uma prova de que **conceito aprendido
no metal não se perde, só troca de roupa**.

E se sobrar um OLED I2C (0x3C) no kit, a montagem é idêntica à do MPU-6050 (dois fios de
barramento, dois de alimentação):

![Circuito do display OLED I2C ligado ao Raspberry Pi](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/oled_i2c_circuito.png)

*Figura 12-L — Display OLED SSD1306 no barramento I2C: SDA/SCL compartilhados com qualquer
outro dispositivo, cada um com seu endereço. Fonte: Practical Python Programming for IoT
(Packt), cap. 8, Fig. 8.7.*

## Resumindo

- Header 40 pinos: **3,3 V inegociáveis**, numeração BCM nas bibliotecas, BCM 2/3 com
  pull-ups de fábrica, ~8 mA por pino.
- Camadas: sysfs (legado) → libgpiod (produção em C) → gpiozero (protótipo/ensino);
  velocidade cresce nessa ordem inversa, determinismo **nenhuma** tem (Exemplo 12.1).
- Sem ADC: sensor digital, ADC externo (MCP3008/ADS1115) ou MCU delegado — o trio da
  questão 8 da Lista 4.
- HC-SR04: divisor 1k/2k no ECHO (3,33 V — Exemplo 12.2); d = 343·t/2; jitter do SO ⇒ ±~1 cm
  (Exemplo 12.3).
- DHT11 sob Linux falha às vezes por natureza: try/except + repetição é o padrão, não
  gambiarra.
- I2C: `i2cdetect/i2cget` = scanner e `rd()` da semana 9 em versão linha de comando.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| numeração BCM / física | nome do pino no chip / posição no conector |
| gpiozero | biblioteca Python de GPIO orientada a eventos |
| libgpiod | biblioteca C oficial para /dev/gpiochip |
| sysfs | interface legada de GPIO por arquivos |
| conversor de nível | divisor/circuito que adapta 5 V ↔ 3,3 V |
| HC-SR04 | sensor ultrassônico de distância (TRIG/ECHO) |
| DHT11 | sensor digital de temperatura/umidade, 1 fio |
| i2cdetect | varredura de endereços I2C no Linux |
| venv | ambiente Python isolado por projeto |

## 📖 Onde aprofundar (opcional)

- ***Simple Electronics with GPIO Zero*** (King, 2. ed.) — receitas gpiozero passo a passo;
  excelente para quem tem pouca base de Python (entre os PDFs da disciplina).
- **Smart**, *Practical Python Programming for IoT*: cap. 2 (protoboard ilustrada), cap. 5
  (GPIO em Python — inclui as distorções do PWM por software: a prova visual do jitter do
  SO!), cap. 6 (eletrônica de interfaceamento) e caps. 9–11 (DHT, LDR+ADS1115, HC-SR04 —
  origem de várias figuras desta aula; entre os PDFs).
- **Molloy**, *Exploring Raspberry Pi*, caps. 6, 8 e 11 — a referência clássica (o pôster
  da Figura 12-B é dele).

## Exercícios

Lista 4, questões 7–12 (estilo dos Exemplos 12.1–12.3).
