# Lab 12 — GPIO, DHT11 e HC-SR04 no Raspberry Pi

> **Antes de começar**: leia a [teoria-12](teoria-12.md) — as Figuras 12-A/12-B (pinout) são
> obrigatórias na bancada, e o Exemplo 12.2 é a conta do divisor que você montará na Parte C.

**Objetivo**: repetir sob Linux o interfaceamento que você domina no MCU — botão+LED nas três
camadas, DHT11 com log CSV tolerante a falhas e HC-SR04 com divisor — **medindo** o efeito do
jitter do SO com estatística.

**Duração**: 2 aulas.
**Material**: RPi 3 do Lab 11 (via SSH), LED + R 220 Ω, botão, DHT11, HC-SR04, resistores
1 kΩ + 2 kΩ (divisor!), MPU-6050, jumpers **fêmea-macho** (o header do RPi é de pinos).
**Checkpoint 1 do projeto final é nesta semana**: hardware montado + leitura de sensores —
reservem os últimos 15 min para mostrar ao professor.

---

## Parte 0 — Sincronize e transfira

```bash
# no PC:
cd ~/sis-emb && git fetch && git reset --hard origin/main
scp -r ~/sis-emb/semana-12/src aluno@bancadaN.local:~/lab12
# no RPi:
ssh aluno@bancadaN.local && cd ~/lab12
```

Tenha o mapa do cabeçalho à mão durante todo o lab:

![Mapa do cabeçalho de 40 pinos do Raspberry Pi com numeração física e BCM](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/rpi_poster_gpio.png)

*Figura L12-A — O mapa do tesouro: número físico, nome BCM e funções alternativas. Errar um
pino aqui custa componentes. Fonte: pôster oficial de Exploring Raspberry Pi (Wiley).*

## Parte A — Botão + LED nas três linguagens da casa (35 min)

**A.1 — gpiozero (3 linhas úteis).** Monte: LED + R 220 Ω no **BCM 17** (pino físico 11) →
GND; botão entre **BCM 27** (físico 13) e GND. Rode:

```bash
python3 led_botao.py
```

Cada aperto alterna o LED. Agora abra o arquivo (dissecado na teoria, seção 2) e responda
para o relatório: **o que o `Button(27, bounce_time=0.02)` está escondendo de você?** Liste
pelo menos três coisas que você fez à mão nas semanas 3–4 (pull-up, detecção de borda,
debounce, o "laço"/evento). O circuito que você montou, fotografado num RPi idêntico:

![Circuito de botão e LED em protoboard no Raspberry Pi](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/circuito_botao_led.png)

*Figura L12-B — Confira sua montagem contra esta referência: LED com resistor série no GPIO
e botão entre pino e GND (o pull-up é interno). Fonte: Practical Python Programming for IoT
(Packt), cap. 2, Fig. 2.7.*

**A.2 — libgpiod em C.** Compile e rode o pisca de 10 ciclos:

```bash
sudo apt install -y libgpiod-dev
gcc led_gpiod.c -lgpiod -o led && ./led
```

Note no código o `gpiod_line_request_output(led, "lab12", 0)`: rode em outro terminal
`gpioinfo | grep -A1 "line  17"` **enquanto** o programa pisca — o kernel mostra a linha 17
ocupada por `"lab12"`. Dois processos não brigam por um pino sem o kernel saber: compare com
o ESP32, onde nada impedia (e por isso duas tarefas podiam escrever no mesmo pino —
lembra o mutex da semana 6? Aqui o "mutex" é do kernel).

**A.3 — sysfs, o legado (só para conhecer).** Tente o caminho antigo:

```bash
ls /sys/class/gpio 2>/dev/null || echo "sysfs de GPIO ausente neste kernel"
```

Em kernels recentes ele nem existe mais — registre o resultado e a lição (teoria, seção 2:
código antigo na internet usará isso; agora você reconhece e sabe migrar).

## Parte B — DHT11: log tolerante a falhas (30 min)

1. Monte o DHT11: VCC→3,3 V (o módulo aceita), DADOS→**BCM 4** (físico 7), GND→GND.

![Circuito do DHT11 no Raspberry Pi com pull-up no pino de dados](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/dht_circuito.png)

*Figura L12-C — O DHT11 ligado: repare no resistor de pull-up no pino DATA (o módulo com
PCB já o traz; o sensor pelado, não). Fonte: Practical Python Programming for IoT (Packt),
cap. 9, Fig. 9.3.*

2. Instale a biblioteca e rode o logger (o try/except está explicado na teoria, seção 5):

```bash
pip3 install adafruit-circuitpython-dht --break-system-packages
sudo apt install -y libgpiod2
python3 dht11_log.py
```

Saída esperada (com falhas ocasionais — **conte-as!**):

```
T=27 C  UR=62 %
T=27 C  UR=62 %
leitura falhou: Checksum did not validate. Try again.
T=28 C  UR=61 %
```

3. Deixe rodando **≥ 10 min** (aqueçam o sensor com a mão em algum momento) e então encerre
   com Ctrl+C. Estatística da confiabilidade: conte linhas boas × falhas
   (`wc -l dht11_log.csv` e as mensagens de erro no terminal) e calcule a taxa de falha (%).
   Relacione com o Exemplo 11.1: **de onde vêm essas falhas?** (Não é o sensor: é o SO
   perdendo bits de 26–70 µs enquanto atendia outra coisa.)
4. Traga o CSV ao PC (`scp`) — ele entra na entrega e será reaproveitado na semana 14.

## Parte C — HC-SR04: o divisor e a estatística do jitter (35 min)

**Antes de energizar** (a pegadinha dos 5 V — teoria, seção 4):

- [ ] VCC do sensor no **5 V** (físico 2), GND comum;
- [ ] TRIG direto no **BCM 23** (físico 16);
- [ ] ECHO → **1 kΩ** → nó do **BCM 24** (físico 18) → **2 kΩ** → GND (Exemplo 12.2: 3,33 V).
- [ ] Conferência com multímetro (sensor alimentado, sem medir): tensão no nó do divisor com
      ECHO em repouso ≈ 0 V; ninguém liga ECHO direto no GPIO nesta bancada.

![Montagem do HC-SR04 com o divisor de tensão no pino ECHO](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/hcsr04_circuito.png)

*Figura L12-D — O circuito completo: TRIG direto, ECHO passando pelo divisor antes de tocar
o GPIO de 3,3 V. Fonte: Practical Python Programming for IoT (Packt), cap. 11, Fig. 11.6.*

5. Alvo fixo (livro em pé) a **20 cm** da face do sensor, medidos com régua. Rode:

```bash
python3 distancia.py
```

Saída esperada:

```
coletando 100 amostras...
media = 20.31 cm | desvio = 0.42 cm | min = 19.6 | max = 22.1
```

6. Registre média/desvio/min/max para **três distâncias** (10, 20, 40 cm) numa tabela. O
   desvio de ~0,3–1 cm é o jitter do escalonador carimbando as bordas do ECHO (Exemplo
   12.3) — compare com a resolução de 0,2 mm que o timer de 1 µs do ESP32 permitia (Exemplo
   4.3). Mesma física, incerteza 50× maior: escreva a conclusão de projeto em 2 linhas.
7. **Melhoria barata**: edite o script para descartar outliers (mediana das 100 em vez da
   média — `statistics.median`). O `max` absurdo some? É o Exemplo 7.4 (mediana × spikes)
   pagando dividendos no Bloco 2.

## Parte D — I2C de terno novo (15 min)

8. MPU-6050 no barramento: VCC→3,3 V, SDA→**BCM 2** (físico 3), SCL→**BCM 3** (físico 5),
   GND.
9. Os velhos conhecidos:

```bash
i2cdetect -y 1          # esperado: 68 na tabela
i2cget -y 1 0x68 0x75   # esperado: 0x68  (o WHO_AM_I da semana 9!)
```

Print dos dois para o relatório — e uma linha: qual ferramenta da semana 9 cada comando
substitui?

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| `gpiozero` reclama de pino | BCM × físico confundidos | Figura L12-A na bancada |
| DHT11 falha 100 % das vezes | pino errado ou módulo sem pull-up | BCM 4, módulo com PCB |
| Distâncias absurdas/timeout | ECHO sem divisor (ou divisor errado) | refaça a checklist da Parte C |
| `i2cdetect` mostra tudo ocupado | I2C não habilitado | Lab 11, Parte D.11 |

## Entrega (GitHub da dupla, `lab-12/relatorio.md`)

1. Resposta da A.1 (o que o gpiozero esconde — mínimo 3 itens) + print do `gpioinfo`
   mostrando a linha ocupada (A.2) + resultado do sysfs (A.3).
2. `dht11_log.csv` (≥ 10 min) + taxa de falha calculada + explicação da causa (B.3).
3. Tabela de 3 distâncias do HC-SR04 (média/desvio/min/max) + comparação MCU × Linux (C.6) +
   efeito da mediana (C.7).
4. Prints do `i2cdetect`/`i2cget` com a correspondência semana 9 ↔ Linux (D.9).
5. Foto da montagem com o **divisor do ECHO em destaque**.
6. Checkpoint 1 do projeto: uma foto + 3 linhas de status no repositório do grupo.

## Desafio (opcional)

Trena falante: combine `distancia.py` com `espeak-ng` (`sudo apt install espeak-ng`) para o
RPi **falar** a distância a cada 2 s ("vinte e um centímetros"). Inútil? Talvez. Mas você
terá integrado sensor + processamento + síntese de voz em 20 linhas — tente fazer isso num
MCU e aprecie o Bloco 2.
