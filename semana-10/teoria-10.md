# Aula 10 — Barramento CAN e o Controlador TWAI do ESP32 (U4 / ementa)

> **Pré-requisito**: Aula 9 (barramentos seriais: UART, SPI, I2C).
> **Como usar**: texto autossuficiente. Os Exemplos 10.1–10.3 são o modelo das questões 16–20
> da Lista 3. Refaça a arbitragem do Exemplo 10.1 bit a bit, com papel e lápis — ela *cai*.

Os barramentos da semana passada vivem *dentro* de uma placa: centímetros de trilha, ambiente
elétrico civilizado. Agora imagine interligar 30 módulos espalhados por um carro — chicote de
metros, ao lado de bobinas de ignição e motores, com mensagens de freio que **não podem**
esperar as de vidro elétrico. I2C não sobrevive a isso. A resposta da indústria (Bosch, anos
80) é o **CAN** (*Controller Area Network*): item explícito da nossa ementa, idioma nativo de
todo veículo (o conector OBD-II do seu carro fala CAN) e de boa parte da automação industrial
e agrícola. E há um presente escondido no nosso hardware: o ESP32 traz um controlador CAN
completo — a Espressif o chama de **TWAI** — de fábrica. No laboratório, dois ESP32 formarão
uma rede CAN de verdade.

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) explicar a camada física diferencial e os níveis dominante/recessivo;
- (b) descrever o frame CAN e a arbitragem por prioridade;
- (c) calcular tempo de frame, carga de barramento e latência;
- (d) montar e programar nós CAN com o driver TWAI.

---

## 1. Por que CAN? O problema que ele resolve

Requisitos do ambiente veicular/industrial que nenhum barramento da semana 9 atende junto:

1. **Multiponto real**: dezenas de nós no mesmo par de fios, qualquer um podendo transmitir —
   sem mestre central para ser ponto único de falha.
2. **Robustez elétrica**: metros de cabo em ambiente ruidoso ⇒ sinalização **diferencial**.
3. **Prioridade sem colisão**: mensagens críticas passam na frente, sem destruir as demais.
4. **Detecção de erro paranoica**: CRC15 + ACK + monitoramento de linha + *bit stuffing* +
   confinamento automático de nós defeituosos (um nó "louco" se auto-silencia em vez de
   derrubar a rede).

O CAN não é "I2C mais forte" — é uma filosofia diferente de rede, nascida para ambientes
onde falhar é inaceitável. Para situá-lo no mapa maior: a figura abaixo mostra como a
indústria organiza suas redes em camadas, do barramento de campo (onde mora o CAN) até o
sistema de informação da empresa (onde morarão o MQTT e o TCP/IP da semana 14):

![Camadas de redes industriais: barramento de campo, controle e informação](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fieldbus_camadas.png)

*Figura 10-A — A pirâmide da automação: na base, os *fieldbuses* (CAN, Modbus, Profibus)
ligando sensores e atuadores; acima, controle e informação. O nosso lab extra de Modbus e a
semana 14 de MQTT sobem um degrau dessa escada. Fonte: Hands-On Industrial Internet of
Things (Packt), cap. 3, Fig. 3.11.*

## 2. Camada física: dominante × recessivo

Dois fios trançados, **CAN_H** e **CAN_L**, com resistores de terminação de **120 Ω em cada
extremidade** (casamento de impedância — sem eles, o sinal reflete nas pontas do cabo como
eco num cano, corrompendo bits; em cabos curtos de bancada às vezes “funciona sem”, mas é
sorte, não engenharia). Os dois estados:

```
 bit RECESSIVO (1): CAN_H ≈ CAN_L ≈ 2,5 V   (linhas "soltas", juntas pela terminação)
 bit DOMINANTE (0): CAN_H ≈ 3,5 V, CAN_L ≈ 1,5 V  (transceptores FORÇAM a diferença)

 Se um nó transmite 0 e outro transmite 1 ao mesmo tempo → o barramento fica em 0.
 O dominante VENCE eletricamente (um "E" cabeado) — sem curto, sem dano, sem colisão.
```

![Sinalização diferencial CAN_H/CAN_L e topologia de barramento com terminação](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/can_diferencial_topologia.png)

*Figura 10-B — Esquerda: o par diferencial — o receptor lê a *diferença* CAN_H − CAN_L, e o
ruído, que afeta os dois fios igualmente, se cancela (é o mesmo superpoder do cabo de
microfone balanceado). Direita: a topologia — um barramento único, terminado nas duas pontas
com 120 Ω, onde qualquer nó pode transmitir.*

Por que diferencial? O receptor mede a **diferença** entre os fios. Uma descarga de ignição
perturba os dois fios *juntos* — e a diferença continua intacta. É como dois amigos no
elevador: se o elevador (o ruído) sobe ou desce, a diferença de altura entre eles não muda.

Essa assimetria dominante/recessivo é a pedra fundamental: ela permite que um transmissor
**compare** o que enviou com o que o barramento mostra — e descubra, bit a bit, se alguém
"mais dominante" está falando junto. (Guarde; a seção 4 colhe o fruto.)

Quem gera esses níveis não é o ESP32: o controlador (**TWAI**, dentro do SoC) fala TX/RX em
lógica 3,3 V; o **transceptor** (nosso SN65HVD230, um CI de 8 pinos alimentado em 3,3 V)
converte para o par diferencial e de volta. Controlador = protocolo; transceptor = músculo
elétrico. Um sem o outro não faz rede — questão 20 da Lista 3.

## 3. O frame CAN (base, CAN 2.0A)

![Campos do quadro CAN clássico: SOF, identificador, controle, dados, CRC, ACK](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/can_frame.png)

*Figura 10-C — O frame CAN: curto, verificado e confirmado. O campo de identificador é a
alma do protocolo — ele carrega a função da mensagem e a sua prioridade.*

```
SOF │ ID (11 bits) │ RTR │ IDE+r0 │ DLC (4b) │ DADOS (0–8 bytes) │ CRC15 │ ACK │ EOF
 ▲        ▲                             ▲                             ▲      ▲
início  identifica a MENSAGEM       quantos bytes                 checagem  QUALQUER receptor
        (não o nó!) e define                                       forte    que validou crava
        a prioridade                                                        um dominante aqui
```

Duas ideias contraintuitivas e centrais:

- O **ID identifica a mensagem, não o remetente**: 0x0A0 pode ser "temperatura do motor",
  publicada por quem a tiver. É *publish–subscribe* em hardware (a semana 14 mostrará o
  mesmo padrão em software: MQTT) — os nós filtram por ID o que lhes interessa, e nenhum nó
  precisa saber da existência dos outros. Desacoplamento total.
- **Menor ID = maior prioridade**, consequência direta da arbitragem a seguir — e por isso o
  projeto de uma rede CAN começa atribuindo IDs: os mais críticos recebem os números mais
  baixos.

Payload máximo de 8 bytes parece pouco? É proposital: frames curtos ⇒ latência baixa e
barramento devolvido rápido às mensagens urgentes. (Existe o CAN FD, com até 64 bytes e taxa
variável, mas o clássico segue onipresente.)

## 4. Arbitragem: a disputa que ninguém perde feio

Todo nó pode transmitir quando o barramento estiver livre. Se dois começam **juntos**,
transmitem o ID bit a bit **enquanto escutam**:

**Exemplo resolvido 10.1 (arbitragem)** — Nós A (ID 0x120) e B (ID 0x0A0) iniciam juntos.
0x120 = 00100100000₂; 0x0A0 = 00010100000₂. Bit a bit (MSB primeiro): os dois primeiros
bits coincidem; no **3º bit**, A transmite 1 (recessivo) e B transmite 0 (dominante) ⇒ o
barramento fica em 0; A *lê* 0 ≠ 1 que enviou ⇒ A percebe que perdeu, **cala-se** e re-tenta
automaticamente no próximo intervalo. B segue transmitindo **sem ter destruído um único
bit** — ao contrário do CSMA-CD da Ethernet clássica (onde dois transmissores destruíam os
quadros um do outro e ambos recomeçavam), não há colisão destrutiva nem tempo perdido.

Menor ID venceu: 0x0A0 é mais prioritário que 0x120 — e é por isso que, no firmware do lab,
a temperatura (crítica) usa 0x0A0 e o comando de ventilador usa 0x120. No seu carro, “ABS”
tem ID baixíssimo; “ligar o vidro do passageiro” tem ID alto. A arbitragem é o mecanismo que
torna o CAN **determinístico**: o pior caso de espera é calculável (próxima seção) — algo
que uma Ethernet comum não promete.

## 5. As contas de engenharia de rede

**Exemplo resolvido 10.2 (tempo de frame e carga de barramento)** — A 500 kbit/s, um frame
com 8 bytes tem ~111 bits (47 de overhead + 64 de dados + *stuffing* médio — o protocolo
insere um bit extra a cada 5 bits iguais seguidos, para manter a sincronia dos receptores)
⇒ t ≈ 111/500 000 = **222 µs**. Dez mensagens periódicas de 10 ms cada: carga = 10 · 222 µs/10
ms = **22 %**. Regra prática da indústria: manter **< 50 %** para garantir a latência das
mensagens de baixa prioridade (acima disso, as "plebeias" começam a morar na fila).

**Exemplo resolvido 10.3 (latência de pior caso)** — A mensagem de MAIOR prioridade pode
esperar, no pior caso, **1 frame já em curso** (arbitragem só ocorre entre frames): ~222 µs
a 500 k. Mensagens de menor prioridade esperam **todas** as superiores pendentes — por isso o
mapeamento ID↔função é decisão de engenharia de sistema, documentada em tabela (a "matriz de
mensagens" de um carro tem milhares de linhas e dono formal — alterá-la exige aprovação,
porque mexe na temporização de todo o veículo).

## 6. TWAI no ESP-IDF: o driver em 5 chamadas

O driver `driver/twai.h` esconde os registradores e entrega uma API de fila:

```c
twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
twai_timing_config_t  t = TWAI_TIMING_CONFIG_500KBITS();   // bit timing pronto p/ 500 k
twai_filter_config_t  f = TWAI_FILTER_CONFIG_ACCEPT_ALL(); // sem filtro: recebe tudo
twai_driver_install(&g, &t, &f);
twai_start();

twai_message_t m = { .identifier = 0x0A0, .data_length_code = 4 };
m.data[0] = ...;                       // payload (até 8 bytes)
twai_transmit(&m, pdMS_TO_TICKS(100)); // bloqueia até enfileirar (timeout!)
twai_receive(&r, 0);                   // 0 = não bloqueia; portMAX_DELAY = espera
```

Dois modos que o firmware do lab usa com sabedoria didática:

- **`TWAI_MODE_NO_ACK` + `m.self = 1` (selftest)**: uma placa **sozinha**, sem transceptor,
  transmite e recebe a própria mensagem — o protocolo inteiro testável antes de existir a
  rede (lembra o "simule antes do hardware"? mesmo espírito). Sem NO_ACK, um nó solitário
  entra em erro: ninguém dá ACK ao frame dele — e essa é uma *feature* do CAN, não um bug:
  a mensagem não confirmada é retransmitida, e o contador de erros do nó cresce até ele se
  isolar (o confinamento da seção 1).
- **`TWAI_MODE_NORMAL` (rede real)**: com transceptores e terminação, dois papéis no nosso
  firmware — `PAPEL_SENSOR 1` publica a temperatura (ID 0x0A0, 2 bytes em décimos de °C,
  little-endian: `t10 & 0xFF`, `t10 >> 8` — a endianness da semana 3 de novo, agora **por
  contrato explícito** do nosso protocolo); `PAPEL_SENSOR 0` assina, e acima de 30 °C
  publica o comando de ventilador (ID 0x120 — de propósito **menos** prioritário que a
  temperatura: telemetria crítica fura fila de comando de conforto, Exemplo 10.1 virando
  decisão de projeto).

> 💡 **Pense aí — CAN × MQTT.** O CAN publica mensagens identificadas por número e qualquer
> nó assina; o MQTT publica mensagens identificadas por *tópico* e qualquer cliente assina
> (semana 14). É o mesmo padrão publish–subscribe, um em hardware de 2 fios e outro sobre
> TCP/IP. Quando chegar lá, lembre que você já conhece a ideia — só mudou o transporte.

## Resumindo

- CAN: multiponto diferencial com terminação 120 Ω; **dominante (0) vence recessivo (1)**
  eletricamente — a base de tudo; ruído comum aos dois fios se cancela.
- Frame: ID de 11 bits identifica a **mensagem** e define prioridade (menor ID ganha); dados
  0–8 bytes; CRC15 + ACK + stuffing + confinamento de nós defeituosos.
- Arbitragem bit a bit sem colisão destrutiva (Exemplo 10.1); frame de 8 B ≈ 111 bits ≈ 222 µs
  a 500 k; carga < 50 % (Exemplo 10.2); latência de pior caso = função da prioridade
  (Exemplo 10.3).
- Controlador (TWAI, no SoC) ≠ transceptor (SN65HVD230, no fio): os dois são necessários.
- Driver TWAI: install/start/transmit/receive; NO_ACK + self permite testar com uma placa só.
- CAN é publish–subscribe em hardware — o avô do MQTT da semana 14.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| CAN / TWAI | barramento veicular / o controlador CAN do ESP32 |
| CAN_H / CAN_L | par diferencial do barramento |
| dominante / recessivo | 0 forçado (vence) / 1 solto (cede) |
| transceptor | CI que converte lógica ↔ par diferencial |
| terminação | resistores de 120 Ω nas duas pontas do barramento |
| arbitragem | disputa bit a bit pelo barramento |
| bit stuffing | bit extra a cada 5 iguais (sincronismo) |
| confinamento | nó com muitos erros se auto-silencia |
| matriz de mensagens | tabela ID↔função de uma rede CAN |
| publish–subscribe | publicar por identificador, assinar por interesse |

## 📖 Onde aprofundar (opcional)

- **ESP-IDF Programming Guide**, seção *TWAI* (inclui o diagrama de frame oficial).
- Bosch, *CAN Specification 2.0* (o documento original, gratuito na web — leiam ao menos a
  seção de arbitragem: 3 páginas que valem a semana).
- Datasheet do **SN65HVD230** (TI): pinagem e o resistor de *slope control*.

## Exercícios

Lista 3, questões 16–20 (estilo dos Exemplos 10.1–10.3).
