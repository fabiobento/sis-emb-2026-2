# Lab Extra — Medição de Energia Elétrica com PZEM-004T: CA, RMS, Fator de Potência e Modbus

> **Pré-requisitos**: semanas 9 (UART), 14 (MQTT) e, idealmente, 7 (amostragem/RMS) e 11–12
> (RPi). Este lab **integra tudo** — é um ótimo trampolim para o projeto final.
>
> ⚠️ **AVISO DE SEGURANÇA — LEIA ANTES DE QUALQUER COISA.** Este módulo se conecta à **rede
> elétrica de 220 V**, que **mata**. A parte de corrente alternada (ligação de L/N e posição da
> garra CT no fio fase) é montada, isolada e energizada **exclusivamente pelo professor**, com a
> montagem em caixa ou sobre base isolante, disjuntor a montante e a bancada desenergizada
> durante qualquer manuseio. **O aluno trabalha somente no lado TTL de baixa tensão** (os 4
> fios 5V/GND/RX/TX que vão ao ESP32 ou ao RPi). Nunca abra, toque ou remonte o lado CA com a
> rede ligada. Em caso de dúvida, pergunte — eletricidade não perdoa improviso.

## Por que este lab é especial

Na semana 1, o Exemplo 1.1 calculou a autonomia de um sensor a bateria a partir de correntes
médias. Agora invertemos a lente: em vez de estimar consumo de miliamperes, vamos **medir o
consumo real de kilowatts** de aparelhos de verdade — uma lâmpada, um ventilador, um
carregador de notebook — e descobrir, com números, coisas que surpreendem (por que o
ventilador "consome" mais VA do que W? por que a lâmpada de LED tem fator de potência
ruim?).

O **PZEM-004T v3.0** que você adquiriu é um medidor de energia CA completo num módulo do
tamanho de um isqueiro. Ele mede tensão, corrente, potência ativa, energia acumulada (kWh),
frequência e fator de potência, e entrega tudo por **UART/Modbus-RTU** — o mesmo barramento
serial da semana 9, agora falando um **protocolo industrial de verdade**. O modelo que você
tem é o **100 A**, que usa o **transformador de corrente (CT) de núcleo partido** — a garra
preta que abraça o fio sem tocá-lo eletricamente. Este lab cobre, de uma vez: sinais CA e
valor RMS, potência ativa × aparente × reativa e fator de potência (o "PDS aplicado" da
ementa), o protocolo Modbus-RTU completo com CRC, e a arquitetura ESP32↔RPi via MQTT das
semanas 11–14.

## 1. Corrente alternada: por que tudo muda

Até agora nossos sinais eram CC (uma tensão que fica parada) ou lentos. A tomada entrega
**corrente alternada**: uma senoide de ~60 Hz (no Brasil) que troca de polaridade 120 vezes
por segundo:

```
 v(t) = V_pico · sin(2π·60·t)      V_pico ≈ 311 V para os "220 V" nominais!
```

Pausa para o susto: os "220 V" da tomada **não** são o pico da senoide — o pico é ~311 V. Os
220 V são o **valor RMS** (*Root Mean Square*, valor eficaz), definido para que uma senoide
CA entregue a **mesma potência média** que uma tensão CC de mesmo valor. Para uma senoide:

V_RMS = V_pico / √2  ⇒  311/√2 ≈ 220 V ✓

O RMS é literalmente a "raiz da média dos quadrados" do sinal — e aqui a semana 7 volta com
tudo: para medir RMS, o PZEM **amostra** a senoide muitas vezes por ciclo (Nyquist!), eleva
ao quadrado, tira a média e a raiz. Se amostrasse devagar demais, teria aliasing e mediria
errado. O que você fez com o LDR a 20 Hz, o PZEM faz com a rede a milhares de Hz,
internamente.

![Formas de onda de tensão e corrente em CA, potência instantânea e triângulo de potências](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/rms_potencia.png)

*Figura E-A — Os três painéis da potência em CA: (1) tensão e corrente senoidais com
defasagem φ e a linha do valor RMS; (2) a potência instantânea p(t) = v·i, cuja média é a
potência ativa e cujos trechos negativos são energia que volta; (3) o triângulo S² = P² + Q²
com o fator de potência como cosseno.*

**Exemplo resolvido E.1 (RMS de um sinal amostrado)** — Um chip de medição amostra a
corrente 16 vezes num ciclo e obtém (em A): 0, 3.8, 7.1, 9.2, 10, 9.2, 7.1, 3.8, 0, −3.8,
−7.1, −9.2, −10, −9.2, −7.1, −3.8.

*Solução passo a passo.* A média aritmética é ~0 (é CA — os semiciclos se cancelam!) — por
isso **não** se usa a média para medir CA. Já a média dos **quadrados**:

(0² + 3,8² + 7,1² + 9,2² + 10² + 9,2² + 7,1² + 3,8² + 0² + 3,8² + 7,1² + 9,2² + 10² + 9,2² +
7,1² + 3,8²)/16 ≈ (0+14,4+50,4+84,6+100+84,6+50,4+14,4+0+14,4+50,4+84,6+100+84,6+50,4+
14,4)/16 ≈ 50 A²

e a raiz ⇒ I_RMS ≈ **7,07 A** — coerente com o pico: 10/√2 = 7,07 ✓. Moral: o valor que
"importa" em CA é sempre o RMS, e obtê-lo exige amostrar rápido e uniformemente — PDS puro.

## 2. Potência em CA: ativa, aparente, reativa e o fator de potência

Em CC, potência é P = V·I, fim. Em CA, a corrente pode estar **defasada** da tensão (cargas
indutivas como motores atrasam a corrente; capacitivas adiantam), e isso parte a potência em
três:

- **Potência ativa P (watts, W)**: a que **realmente vira trabalho/calor** — a que a
  concessionária cobra. P = V_RMS · I_RMS · cos φ, onde φ é a defasagem.
- **Potência aparente S (volt-ampères, VA)**: o produto "ingênuo" S = V_RMS · I_RMS — o que
  a fiação e os transformadores precisam **suportar**, defasagem ou não.
- **Potência reativa Q (volt-ampères reativos, var)**: a que "vai e volta" entre a fonte e
  os campos magnéticos/elétricos da carga sem virar trabalho. Q = V_RMS · I_RMS · sin φ.

As três formam um triângulo retângulo: **S² = P² + Q²**. E o **fator de potência**:

FP = P/S = cos φ

FP = 1 é o ideal (carga resistiva pura: aquecedor, lâmpada incandescente). FP baixo
significa que você "ocupa" a rede com corrente que não faz trabalho — por isso a indústria
paga multa por FP baixo e usa bancos de capacitores de correção.

**Exemplo resolvido E.2 (o ventilador enganador)** — Um ventilador lê no PZEM: V = 220 V,
I = 0,50 A, P = 88 W, FP = 0,80.

*Conferência passo a passo.* S = 220·0,50 = **110 VA**; P esperado = S·FP = 110·0,80 =
**88 W** ✓ (bate com o medido); Q = √(110² − 88²) = √(12100 − 7744) = √4356 = **66 var**.
Interpretação: a fiação carrega 110 VA (0,5 A), mas só 88 W viram vento; 66 var ficam
"circulando" por causa do motor indutivo. Se você medisse só corrente (0,5 A) e
multiplicasse por 220, superestimaria o consumo real em 25 %. **É por isso que precisamos de
um medidor de potência ativa, não de um amperímetro** — e é o experimento central do lab.

## 3. O transformador de corrente (CT) de núcleo partido

Como medir 100 A sem cortar o fio nem se eletrocutar? Com um **transformador de corrente**.
O fio da rede é o "primário" (1 volta); a garra contém o "secundário" (muitas voltas) em
torno de um núcleo de ferrite que você **fecha em volta do fio** (por isso "núcleo partido"
— abre e fecha como um alicate). O campo magnético alternado da corrente do fio induz uma
corrente proporcional, porém **muito menor e isolada**, no secundário.

O seu CT é marcado **0–100 A (1000:1), 10 Ω** (leia na etiqueta preta). A relação 1000:1
significa:

**Exemplo resolvido E.3 (o CT)** — Passam 30 A no fio da carga. No secundário do CT
(relação 1000:1): I_sec = 30/1000 = **30 mA**. Sobre o resistor de carga (*burden*) de 10 Ω,
isso gera V = 0,030·10 = **0,30 V** — uma tensão pequena e segura que o PZEM digitaliza. É a
**mesma ideia do divisor de tensão** da semana 7 (transformar uma grandeza física numa
tensão pequena que o conversor entende), agora por indução magnética e para corrente.
Vantagem enorme: **isolação galvânica** — o seu circuito de medição nunca toca os 220 V.
Por isso o CT é a parte *segura* do conjunto; o perigo está na medição de **tensão** (que
exige contato com a rede — e é por isso que ela é feita pelo professor).

## 4. Modbus-RTU: um protocolo industrial de verdade sobre a UART

Na semana 9, a UART carregava texto solto (o `printf`). O PZEM usa a UART para falar
**Modbus-RTU**, o protocolo serial mais difundido da automação industrial — o mesmo de CLPs,
inversores e medidores do mundo todo. Vale ouro conhecê-lo.

**Camada física** (datasheet, seção 2.1): UART **9600 8N1** — exatamente o frame da semana
9.

**Modelo mestre-escravo**: o mestre (seu ESP32/RPi) pergunta, o escravo (PZEM, endereço
`0xF8` por padrão) responde. Ninguém fala sem ser perguntado — determinístico e simples.

![Estrutura do quadro Modbus RTU de requisição de leitura](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/modbus_frame.png)

*Figura E-B — O quadro Modbus RTU: endereço do escravo, código da função, registrador
inicial, quantidade e CRC-16. Sem bytes de início/fim — o silêncio de 3,5 caracteres
delimita as mensagens.*

**Estrutura de um quadro de leitura** (função 0x04, *Read Input Registers*):

```
 mestre → escravo:  [addr][0x04][reg_ini_H][reg_ini_L][n_regs_H][n_regs_L][CRC_L][CRC_H]
 escravo → mestre:  [addr][0x04][n_bytes][dado0_H][dado0_L]...[CRC_L][CRC_H]
```

O **CRC-16** (seção 2.7, polinômio 0xA001) fecha cada quadro: o receptor recalcula e
compara — se não bater, descarta (é o mesmo espírito do checksum do DHT11 e do CRC do CAN:
detecção de erro em fio ruidoso). Diferente do texto do `printf`, aqui **cada byte é
verificado**.

**O mapa de registradores de medição** (datasheet, seção 2.3 — a "tabela de tesouro" deste
lab):

| Registrador | Grandeza | Resolução | Observação |
|---|---|---|---|
| 0x0000 | Tensão | 0,1 V | 16 bits |
| 0x0001–0x0002 | Corrente | 0,001 A | 32 bits (low + high) |
| 0x0003–0x0004 | Potência ativa | 0,1 W | 32 bits |
| 0x0005–0x0006 | Energia | 1 Wh | 32 bits (acumula!) |
| 0x0007 | Frequência | 0,1 Hz | 16 bits |
| 0x0008 | Fator de potência | 0,01 | 16 bits |
| 0x0009 | Alarme | — | 0xFFFF = acima do limiar |

É o **mesmo conceito** de mapa de registradores do MPU-6050 (semana 9) e dos periféricos
mapeados em memória (semana 2) — só que agora do outro lado de um fio serial, com um
protocolo padronizado por cima. Ler o PZEM é: "escreva o pedido dos 10 registradores a
partir de 0x0000, leia 25 bytes de resposta, valide o CRC, desempacote".

**Exemplo resolvido E.4 (decodificando uma resposta real)** — A datasheet dá este exemplo
de resposta (tensão): bytes `0x08 0x98` no registrador 0x0000. Juntando: 0x0898 = 2200
decimal; × 0,1 V = **220,0 V**. A corrente vem em **dois** registradores (32 bits) porque
pode ter muitos dígitos: `0x0000` (high) e `0x03E8` (low) → 0x000003E8 = 1000 × 0,001 A =
**1,000 A**. Repare na ordem low-depois-high dos pares de 16 bits — a **endianness** da
semana 3 cobrando ingresso de novo, agora dentro de um protocolo industrial. O código do
lab (`pzem_ler` no ESP32, `struct.unpack(">10H", ...)` no RPi) faz exatamente esse
desempacotamento.

## 5. A arquitetura do lab: os dois mundos, de novo

O lab fecha com a arquitetura da semana 14, agora medindo o mundo físico "grande":

```
  220 V  ──┬── carga (lâmpada/ventilador) ──┐
           │         [ CT abraça o fio ]     │
       (rede)                │  isolação      │
                             ▼                │
                     ┌──────────────┐         │
                     │  PZEM-004T   │◀── L/N (medição de tensão, lado do professor)
                     │  (Modbus/TTL)│
                     └──────┬───────┘
              UART 9600 8N1 │ (4 fios: 5V/GND/RX/TX)
                     ┌──────▼───────┐   Wi-Fi/MQTT   ┌──────────────┐
                     │    ESP32     │───────────────▶│  RPi 3       │
                     │ (lê Modbus,  │  energia/#     │  Mosquitto + │
                     │  publica)    │                │  painel+CSV  │
                     └──────────────┘                └──────────────┘
```

E há **dois caminhos** de leitura, ambos no repositório — use o que a bancada permitir:

- **ESP32** (`src/pzem_esp32/`): lê o Modbus por UART e publica no MQTT — o nó sensor
  clássico.
- **RPi direto** (`src/rpi/pzem_rpi.py`): lê o PZEM por adaptador USB-TTL e publica — útil
  quando o RPi é o gateway e não há ESP32 sobrando.

Nos dois, o `painel_energia.py` no RPi assina `energia/#`, grava CSV e **calcula o custo em
R$** a partir da tarifa local — transformando bytes de um registrador em "quanto custa
deixar isso ligado", que é o argumento de venda de qualquer projeto de eficiência
energética.

## Resumindo

- CA: os "220 V" são **RMS** (pico ≈ 311 V); RMS = raiz da média dos quadrados, obtido por
  amostragem rápida — PDS da semana 7 na rede elétrica (Exemplo E.1).
- Potência ativa (W, cobrada) × aparente (VA, dimensiona a fiação) × reativa (var); FP =
  P/S = cos φ; S² = P² + Q². Medir corrente não basta (Exemplo E.2).
- CT de núcleo partido: mede corrente por indução, **isolado** da rede; relação 1000:1 +
  burden de 10 Ω transforma amperes em uma tensão pequena e segura (Exemplo E.3) — o divisor
  da semana 7 em versão magnética.
- Modbus-RTU sobre UART 9600 8N1: mestre-escravo, função 0x04, mapa de registradores,
  CRC-16 — protocolo industrial real; desempacotar exige cuidar da endianness (Exemplo E.4).
- Arquitetura ESP32/RPi + MQTT das semanas 11–14, agora medindo consumo real e convertendo
  em R$.

### 📌 Vocabulário do lab

| Termo | Significado |
|---|---|
| RMS (valor eficaz) | raiz da média dos quadrados; o “V” da tomada |
| potência ativa / aparente / reativa | W (trabalho) / VA (produto V·I) / var (oscilante) |
| fator de potência | FP = P/S = cos φ |
| CT | transformador de corrente (medição isolada) |
| burden | resistor de carga do secundário do CT |
| Modbus-RTU | protocolo industrial mestre-escravo sobre UART |
| CRC-16 | verificação de integridade do quadro |
| função 0x04 | Modbus: ler registradores de entrada |

## 📖 Onde aprofundar (opcional)

- **Datasheet oficial PZEM-004T v3.0** (Peacefair) — seções 1 (grandezas) e 2 (protocolo
  Modbus, registradores, CRC). Leitura obrigatória; é curta e completa.
- Revisão de **Circuitos Elétricos / Análise de Sinais**: valor eficaz, fasores, potência em
  CA (pré-requisitos em ação).
- Biblioteca de referência (para comparar com o nosso código "na mão"):
  `github.com/mandulaj/PZEM-004T-v30` (Arduino) e `pymodbus` (Python).

## Exercícios sugeridos

1. Deduza V_pico dos 127 V RMS (outra tensão comum no Brasil). *(resp.: ~180 V)*
2. Uma carga lê V=220, I=2,0 A, FP=0,65. Calcule P, S e Q. *(resp.: S=440 VA, P=286 W,
   Q≈334 var)*
3. No CT 1000:1 com burden 10 Ω, que tensão aparece no secundário para 75 A no fio?
   *(resp.: 0,75 V)*
4. Monte o quadro Modbus (bytes) para ler os 10 registradores do PZEM no endereço 0x01
   (sem o CRC). *(resp.: 01 04 00 00 00 0A)*
5. Por que a corrente ocupa dois registradores de 16 bits e a tensão só um? *(resp.:
   faixa/resolução — 100 A a 0,001 A = 100000 passos > 65535)*
