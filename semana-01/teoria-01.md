# Aula 1 — Introdução aos Sistemas Embarcados (Unidade I do PPC)

> **Como usar esta apostila.** Este texto foi escrito para ser **autossuficiente**: você não
> precisa de nenhum dos livros da bibliografia para acompanhar a disciplina — todos os
> conceitos são explicados do zero, com exemplos resolvidos passo a passo, figuras e
> vocabulário técnico traduzido. Os livros citados ao final de cada aula (seção
> *“Onde aprofundar”*) são apenas para quem quiser ir além. Leia na ordem, refaça os
> exemplos resolvidos com papel e lápis e só então parta para os exercícios da lista.

Olhe ao seu redor agora. Quantos computadores você consegue contar? Provavelmente você pensou
no notebook e no celular. Mas se você estiver numa sala comum, a resposta certa passa fácil
de dez: há um computador no ar-condicionado, um no micro-ondas, vários no seu carro (um carro
popular moderno tem **dezenas** deles), um no roteador Wi-Fi, um no relógio de pulso, um
dentro do carregador do notebook. Nenhum deles se parece com um computador: não têm teclado,
nem tela, nem Windows. São **sistemas embarcados** — e projetar esses sistemas é o assunto
desta disciplina.

Nesta primeira semana você vai:

- entender o que define um sistema embarcado e quais restrições governam seu projeto;
- conhecer as tecnologias disponíveis (MCU, MPU/SoC, FPGA) e quando usar cada uma;
- conhecer as **duas plataformas** que usaremos o semestre inteiro — o **ESP32** e o
  **Raspberry Pi 3** — e entender por que precisamos das duas;
- montar seu ambiente de trabalho e **simular seu primeiro firmware** no Wokwi, sem hardware.

Ao final da disciplina, você terá construído — com as próprias mãos — desde um pisca-LED
escrito direto em registradores até uma rede de sensores sem fio com controle PID e painel
web. Tudo começa aqui.

---

## 1. O que é um sistema embarcado?

### 1.1 Definição — e por que ela importa

Um **sistema embarcado** (do inglês *embedded system*, “sistema embutido”) é um sistema
computacional **dedicado**, embutido dentro de um produto maior, projetado para executar uma
função específica (ou um conjunto pequeno de funções) sob **restrições** de custo, consumo de
energia, memória, tamanho físico, confiabilidade e, muitas vezes, **tempo real**.

Vamos "desmontar" a definição em três partes, porque cada uma delas tem consequências de engenharia:

1. **Computacional**: tem um processador executando um programa. Isso o distingue de um
   circuito puramente analógico ou digital fixo. O comportamento do produto está no
   *software* — e pode ser alterado, atualizado, corrigido (ou estragado) sem trocar o
   hardware. A indústria chama o software gravado no produto de **firmware**, porque ele fica
   “firme” no aparelho, entre o software (que o usuário troca) e o hardware (que ninguém
   troca).
2. **Dedicado e embutido**: ele faz *aquela* função e o usuário nem percebe que há um
   computador ali. O dono do micro-ondas não “roda programas”; ele esquenta comida. Isso
   muda tudo no projeto: a interface é um botão e um bip, não um teclado; o sistema operacional
   (quando existe) não é visível; e o produto precisa funcionar **sempre**, sem “reinicie e
   tente de novo”.
3. **Sob restrições**: diferente do PC, onde a resposta para quase todo problema é “compre
   mais memória”, no embarcado cada recurso é caro. Essa é a essência da engenharia desta
   disciplina — e a seção 1.3 inteira é dedicada a ela.

Compare com o computador da sua casa:

| | Computador de propósito geral | Sistema embarcado |
|---|---|---|
| Função | qualquer software que o usuário instalar | uma função fixa, definida no projeto |
| Usuário percebe? | é "o computador" | é invisível — o usuário vê "o micro-ondas" |
| Restrição dominante | desempenho/conforto | custo, energia, prazo de resposta |
| Falha | reinicia, paciência | pode ser inaceitável (freio ABS!) |
| Atualização de software | frequente, pelo usuário | rara, controlada pelo fabricante |
| Ciclo de vida | troca em ~5 anos | 10–20 anos em campo, sem manutenção |
| Volume de produção | milhares | milhões (cada centavo multiplica) |

**Exemplos concretos** (procure os processadores “invisíveis” em cada um): controlador de
injeção eletrônica, marca-passo, medidor inteligente de energia, drone, inversor de
frequência, roteador Wi-Fi, forno de micro-ondas, catraca de ônibus, balança de supermercado,
controlador de irrigação, módulo do ABS, elevador, ar-condicionado inverter, estação
meteorológica, máquina de cartão.

> 💡 **Pense aí — sem consultar.** Pegue o aparelho mais próximo de você. Qual é a função
> *única* dele? O que acontece se o “computador” dele travar por 2 segundos? A resposta a essa
> segunda pergunta é o que separa um produto banal de um produto crítico — e define quanto de
> engenharia de confiabilidade o projeto exige. Voltaremos a isso na seção de tempo real.

### 1.2 A anatomia de todo sistema embarcado

Independentemente do produto — marca-passo ou drone — o diagrama de blocos é sempre uma
variação do mesmo tema. O sistema existe para **medir algo** no mundo físico, **decidir algo**
com essa medida e, quase sempre, **agir de volta** sobre o mundo:

```
   mundo físico                    sistema embarcado                     mundo físico
 ┌─────────────┐   ┌────────────────────────────────────────────┐   ┌──────────────┐
 │  grandezas  │──▶│ SENSORES ─▶ CONDICIONAMENTO ─▶ PROCESSADOR │──▶│  ATUADORES   │
 │ (T, luz, …) │   │                 ▲                   │      │   │ (motor, LED, │
 └─────────────┘   │                 │              COMUNICAÇÃO │   │  relé, …)    │
                   │              ENERGIA          (UART, Wi-Fi,│   └──────────────┘
                   │           (bateria/fonte)      I2C, CAN…)  │
                   └────────────────────────────────────────────┘
```

Bloco a bloco:

- **Sensores**: convertem uma grandeza física (temperatura, luz, pressão, distância,
  aceleração) num sinal elétrico. O LDR que você usará na semana 7 muda de resistência com a
  luz; o MPU-6050 da semana 9 mede aceleração e rotação; o HC-SR04 da semana 12 mede distância
  com ultrassom.
- **Condicionamento de sinal**: o sinal do sensor raramente está pronto — pode ser fraco
  (mV), ruidoso, ou numa faixa errada. Divisores de tensão, amplificadores e filtros
  preparam o sinal antes da conversão. É a parte “analógica” que faz a ponte com a Eletrônica
  que você já estudou.
- **Processador**: o cérebro. Lê os sensores (via ADC, GPIO, barramentos), executa o
  algoritmo (seu firmware!) e comanda os atuadores. A seção 2 desta aula compara as três
  famílias de processadores para embarcados.
- **Atuadores**: o caminho de volta ao mundo físico — LEDs, relés, motores (com ponte H,
  semana 8), servos, buzzers, displays.
- **Comunicação**: quase nenhum produto moderno é uma ilha. UART, I2C e SPI conversam com
  chips vizinhos na placa (semana 9); CAN conecta módulos de um carro (semana 10); Wi-Fi e
  MQTT conectam o produto ao mundo (semana 14).
- **Energia**: atravessa todos os blocos. Fonte, reguladores, bateria — e o firmware, que
  decide quanto tempo cada bloco fica ligado. Veremos no Exemplo 1.1 que **software define
  consumo**.

A figura abaixo, retirada de um livro de IoT industrial, mostra a mesma cadeia com os nomes
que a indústria usa: sensor → conversão A/D → algoritmo de controle → conversão D/A → atuador,
com o processo realimentado. É o nosso diagrama de blocos vestido de terno:

![Cadeia de medição e atuação: do sensor ao atuador, fechando a malha sobre o processo](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/cadeia_medicao_atuacao.png)

*Figura 1-A — A cadeia medição → decisão → atuação, com a malha de realimentação. Fonte:
Hands-On Industrial Internet of Things (Packt), cap. 3, Fig. 3.2.*

Guarde este desenho: **cada bloco dele é uma unidade do nosso cronograma**. Sensores e ADC na
semana 7; atuadores e PWM na semana 8; comunicação nas semanas 9, 10 e 14; o processador e seu
software em todas as outras.

### 1.3 Problemas fundamentais: as restrições de projeto

O que torna a engenharia de embarcados *diferente* de programar um PC não é a programação em
si — é projetar **sob restrição**. As seis clássicas:

| Restrição | Pergunta de projeto | Exemplo numérico |
|---|---|---|
| Custo | Qual o custo por unidade em produção? | MCU de R$ 8 × SoC de R$ 90 |
| Energia | Bateria dura quanto? | nó sensor: meses com 2×AA |
| Memória | Código + dados cabem? | ESP32: 520 KB SRAM, 4 MB flash |
| Tempo real | Qual o pior tempo de resposta aceitável? | airbag: < 10 ms |
| Confiabilidade | Pode travar? Watchdog? | ECU automotiva: não pode |
| Ambiente | Faixa térmica, vibração, umidade | −40 °C a +125 °C (automotivo) |

Num produto real, uma dessas restrições **domina** e dita a arquitetura inteira. No
micro-ondas, o custo (milhões de unidades ⇒ cada centavo importa — trocar um MCU de R$ 12 por
um de R$ 8 “economiza” R$ 4 milhões num milhão de unidades). No marca-passo, a confiabilidade
e a energia (trocar a bateria exige cirurgia!). No airbag, o tempo real. Identificar a
restrição dominante é o primeiro passo de todo projeto — e é a primeira pergunta da Lista 1.

Duas das seis restrições merecem tratamento quantitativo já nesta aula, porque as contas
reaparecerão o semestre inteiro: **energia** e **tempo real**.

#### Restrição de energia: a conta que todo nó sensor exige

Um nó sensor alimentado por bateria passa a vida alternando dois estados: **ativo** (mede,
processa, transmite — consome muito) e **dormindo** (quase tudo desligado — consome quase
nada). A autonomia do produto sai desta conta:

**Exemplo resolvido 1.1 (energia)** — Um nó sensor consome 80 mA transmitindo por 2 s a cada
10 min e 10 µA dormindo. Qual a corrente média e quanto dura uma bateria de 2000 mAh?

*Solução passo a passo.* O ciclo completo dura 600 s (10 min). Em cada ciclo, o consumo tem
duas parcelas, medidas em mA·s (corrente × tempo = carga):

1. Ativo: 80 mA × 2 s = **160 mA·s**
2. Dormindo: 0,01 mA × 598 s = **5,98 mA·s**

A corrente média é a carga total do ciclo dividida pelo período do ciclo:

I̅ = (160 + 5,98) / 600 ≈ **0,277 mA**

Autonomia: capacidade da bateria ÷ corrente média:

t = 2000 mAh / 0,277 mA ≈ 7 220 h ≈ **10 meses**

Agora o cenário de bug — e aqui mora a lição de engenharia: se o firmware esquecer o rádio
ligado (80 mA contínuos), a mesma bateria dura 2000/80 = **25 h**. De 10 meses para 1 dia —
*sem trocar um único componente*. Moral: **software define o consumo**. Nenhum hardware de
baixo consumo salva um firmware mal comportado. Esse tema reaparecerá no FreeRTOS (semana 5:
tarefa bloqueada não gasta CPU) e no *deep sleep* do ESP32 (semana 14).

> 📐 **Método — como atacar qualquer problema de autonomia.**
> 1. Desenhe o ciclo do produto no tempo (quando está ativo? dormindo?).
> 2. Calcule a carga de cada fase em mA·s (ou mA·h).
> 3. Some as cargas, divida pelo período → corrente média.
> 4. Autonomia = capacidade da bateria ÷ corrente média. **Confira as unidades!**
>    (mAh ÷ mA = h. É a conferência mais rápida contra erros de fator 1000.)

#### Restrição de tempo real: “certo, mas tarde” é errado

Dizemos que um sistema tem requisito de **tempo real** quando a *correção* da resposta depende
não só do valor calculado, mas **do instante** em que ele é entregue. Um freio ABS que calcula
a pressão correta e a aplica 2 s depois calculou certo — e falhou. A classificação:

- **Tempo real rígido (*hard real-time*)**: perder o prazo é **falha do sistema** — o
  resultado atrasado é inútil ou catastrófico. Ex.: airbag, ABS, controle de injeção,
  marca-passo.
- **Tempo real brando (*soft real-time*)**: perder o prazo **degrada a qualidade**, mas o
  sistema continua útil. Ex.: atualização de display, streaming de áudio (um engasgo irrita,
  não mata).

**Exemplo resolvido 1.2 (tempo real rígido × brando)** — Classifique: (a) o disparo do airbag;
(b) a atualização do display de um multímetro.

*Solução.* (a) O airbag deve inflar completamente em ~30 ms após a colisão; o comando de
disparo tem prazo da ordem de **10 ms**. Perder esse prazo = falha catastrófica ⇒ **tempo real
rígido**: o prazo é parte da especificação de correção. (b) Se o display atualizar em 120 ms
em vez de 100 ms, ninguém percebe; a qualidade degrada suavemente ⇒ **tempo real brando**. A
distinção importa porque sistemas *hard* exigem análise de pior caso (WCET — *worst-case
execution time*, o tempo máximo que um trecho de código pode levar — semana 5) e normalmente
**proíbem** um SO de propósito geral: um Linux convencional pode decidir, no pior momento,
que outro processo é mais importante que o seu (semana 11 discute isso em detalhe).

---

## 2. Tecnologias: MCU × MPU/SoC × FPGA

Existem três grandes famílias de "cérebros" para embarcados. Uma analogia para fixar: o
**microcontrolador** é um canivete suíço — tudo integrado, pequeno, barato, resolve 90 % dos
problemas do dia a dia; o **microprocessador/SoC** é uma oficina completa — muito mais
poderosa, mas precisa de instalação (memória externa, SO, armazenamento); o **FPGA** é uma
impressora 3D de ferramentas — você *constrói* o hardware exato de que precisa.

### 2.1 As três famílias, uma a uma

- **Microcontrolador (MCU)**: CPU + memória (RAM e flash) + periféricos (GPIO, ADC, UART,
  timers…) **num único chip**. Roda *bare-metal* (seu programa direto no hardware) ou um
  RTOS pequeno; liga em milissegundos; consome de µA a dezenas de mA. É o chip do
  micro-ondas, da catraca, do medidor de energia. Ex.: ATmega328P (Arduino Uno), **ESP32**,
  STM32, PIC.
- **Microprocessador / SoC de aplicação (MPU)**: CPU potente (com **MMU** — unidade de
  gerenciamento de memória, o hardware que torna possível um SO com processos isolados;
  semana 11), memória RAM **externa** de centenas de MB ou GB, roda um SO completo
  (Linux); liga em segundos; consome watts. É o chip do roteador, da smart TV, do
  caixa eletrônico. Ex.: **BCM2837** do Raspberry Pi 3 (4× ARM Cortex-A53 @ 1,2 GHz).
- **FPGA** (*Field-Programmable Gate Array*): em vez de executar instruções, o chip contém
  uma matriz de blocos lógicos que você **conecta** como quiser — você descreve *hardware*,
  não software. Paralelismo massivo e latência determinística de nanossegundos. Usado em
  processamento de sinais de altíssima taxa (radar, telecom) e interfaces muito específicas.
  **Fora do escopo** desta disciplina (aparece em Sistemas Digitais e disciplinas de
  projeto), mas você precisa saber que existe para completar a tabela de decisão.

### 2.2 A tabela de decisão — e a armadilha do “mais forte é melhor”

Critério rápido de escolha (que a Lista 1 cobra):

| Requisito do produto | Aponta para |
|---|---|
| Tempo real rígido, resposta em µs, consumo em µA, custo mínimo | **MCU** |
| Pilhas complexas: câmera, banco de dados, interface web, multiusuário | **MPU/SoC + Linux** |
| Processamento paralelo massivo/protocolo proprietário em hardware | **FPGA** |

A armadilha clássica do iniciante: “o SoC é mais potente, então é melhor para tudo”. Não é.
Pergunte a um SoC Linux que acorde a cada 10 min, meça e durma: ele gasta **segundos** para
dar boot e watts para existir — o nó sensor de bateria morre em dias. Pergunte a um MCU que
rode um servidor web com banco de dados e câmera: ele simplesmente não tem RAM nem pilha de
software para isso. **Potência errada é defeito, não virtude**: você paga em custo, energia,
complexidade e tempo de boot por capacidade que não usa.

A disciplina usa **um representante de cada paradigma dominante**: o **ESP32** (MCU, Bloco 1,
semanas 1–10) e o **Raspberry Pi 3** (Linux embarcado, Bloco 2, semanas 11–14). Na semana 14
os dois trabalharão **juntos** — que é como aparecem nos produtos reais: o MCU cuida do tempo
real e do baixo consumo; o SoC cuida da inteligência e da conectividade pesada. O seu
celular, aliás, faz exatamente isso: um SoC de aplicação roda o Android enquanto vários MCUs
invisíveis cuidam da tela sensível ao toque, do carregamento da bateria e dos sensores.

### 2.3 Apresentando as duas plataformas da disciplina

**O ESP32** (Espressif): o MCU que dominou a IoT na última década por integrar, num chip de
alguns dólares, Wi-Fi e Bluetooth completos, dois núcleos de 32 bits a 240 MHz, 520 KB de RAM
e um ecossistema de periféricos (ADC, DAC, PWM, UART, I2C, SPI, CAN, sensores de toque). A
foto abaixo mostra um integrante da família com display OLED e rádio LoRa — repare no módulo
metálico (a “lata”): dentro dela ficam o chip ESP32, a memória flash e a antena. Na semana 2
abriremos essa lata (mentalmente) e na semana 3 programaremos cada periférico.

![Módulo ESP32 com display OLED, antena LoRa e conectores](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/esp32_oled_lora.png)

*Figura 1-B — Um ESP32 com OLED e LoRa: MCU + rádio + periféricos numa placa menor que um
cartão de crédito. Fonte: Internet of Things Programming Projects, 2. ed. (Packt), cap. 1,
Fig. 1.8.*

**O Raspberry Pi** (Raspberry Pi Foundation): um computador Linux completo — CPU ARM de 4
núcleos, 1 GB de RAM, vídeo, USB, rede — do tamanho de um cartão de crédito, com um conector *header*
de 40 pinos que expõe GPIO, I2C, SPI e UART para o mundo físico. O layout abaixo apresenta o
modelo 3B, o nosso padrão de laboratório: identifique o SoC (quadrado central), os conectores
USB e Ethernet (direita), o slot microSD (embaixo) e o conector *header* GPIO de 40 pinos (topo) —
todos eles aparecerão nos laboratórios do Bloco 2.

![Layout do Raspberry Pi 3B com seus conectores e subsistemas identificados](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/rpi3b_layout.png)

*Figura 1-C — Raspberry Pi 3 Model B: um PC Linux completo com conector *header* GPIO de 40 pinos.
Fonte: Internet of Things Programming Projects, 2. ed. (Packt), cap. 1, Fig. 1.2.*

E a família é grande — a tabela abaixo (do mesmo livro) compara os modelos e é uma excelente
lição de **escala de recursos**: repare como, dentro da mesma “marca”, convivem produtos de
1 GHz com 512 MB (Zero) e quad-core de 2,4 GHz com 8 GB (Pi 5). Escolher plataforma é escolher
*quanto* de cada recurso o seu produto precisa — nem mais, nem menos.

![Tabela comparativa de modelos de Raspberry Pi](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/rpi_models_table.png)

*Figura 1-D — Tabela de modelos Raspberry Pi: a mesma decisão “quanto de CPU/RAM/E/S o
produto precisa?” que você fará em todo projeto. Fonte: Internet of Things Programming
Projects, 2. ed. (Packt), cap. 1, Fig. 1.7.*

> **Observação — e o Arduino?** O Arduino Uno (ATmega328P: 8 bits, 16 MHz, 2 KB de RAM) é uma
> ferramenta didática excelente para eletrônica básica, e temos no laboratório. Mas ele não
> tem recursos para o que faremos: Wi-Fi, RTOS, DSP, CAN. Por isso seu papel na disciplina é
> **limitado** a demonstrações pontuais e comparações de arquitetura. O Exemplo resolvido 2.1
> (semana 2) quantifica essa diferença: o mesmo laço de código roda **15× mais devagar** no
> Uno do que no ESP32.

---

## 3. Fluxo de projeto de sistemas embarcados

Todo produto embarcado percorre, com variações, este caminho:

```
Requisitos ─▶ Particionamento HW/SW ─▶ Seleção de plataforma ─▶ Protótipo
                                                                  │
Produção ◀─ Certificação/Testes ◀─ Validação ◀─ Firmware ◀────────┘
                                              (protoboard + SIMULAÇÃO)
```

1. **Requisitos**: o que o sistema faz, com que prazos, a que custo, por quanto tempo de
   bateria. Requisito mal escrito é a causa nº 1 de projeto fracassado: “responder rápido” não
   é requisito; “responder em ≤ 10 ms, no pior caso” é.
2. **Particionamento hardware/software**: o que vira circuito e o que vira código? Regra
   prática: hardware dedicado é mais rápido e consome menos por operação, mas é caro de
   projetar e impossível de atualizar; software é flexível e barato de mudar, mas limitado
   pela velocidade do processador. O particionamento correto é o cerne da profissão — e
   produtos modernos deslocam quase tudo para o software, porque firmware se atualiza por
   Wi-Fi e silício, não.
3. **Seleção de plataforma**: as contas de memória/CPU/energia (Exemplos 1.1, 2.1, 2.3)
   decidem entre MCU, SoC ou ambos.
4. **Prototipagem**: protoboard + **simulação**. Nesta disciplina o simulador é o **Wokwi**
   (wokwi.com): circuitos ESP32 completos no navegador, com Wi-Fi simulado, sem risco de
   queimar componente e com compartilhamento por link. **Todo laboratório do Bloco 1 começa
   no Wokwi** e só depois vai ao hardware — é o mesmo fluxo "simule antes de gravar" da
   indústria (onde simular custa centavos e refazer uma placa custa semanas).
5. **Firmware, validação e produção**: iteração com testes; em produto real, certificação
   (Anatel para rádios, CE na Europa, ISO 26262 para automotivo…).

**Exemplo resolvido 1.3 (particionamento)** — Termostato: leitura de temperatura a 1 Hz
(software, trivial), acionamento do relé (hardware de potência + GPIO), interface local (LCD
I2C) e remota (Wi-Fi/MQTT). Um MCU com Wi-Fi (ESP32) elimina a necessidade de dois chips →
menor custo. Se houvesse processamento de vídeo, o particionamento mudaria para um SoC Linux
(RPi) — ou para o par MCU + SoC: o ESP32 medindo e atuando em tempo real, o RPi servindo a
interface web. Guardem esse desenho: ele **é** a arquitetura do Lab 14 e do projeto final.

### 3.1 Seu primeiro firmware: anatomia de um "blink" no ESP-IDF

Você vai rodar este código hoje no laboratório (arquivo `src/blink_wokwi/main.c`). Vamos lê-lo
juntos, linha a linha — mesmo sem ter visto C embarcado ainda, você entenderá a estrutura, e
nas próximas semanas cada linha ganhará profundidade:

```c
#include <stdio.h>                 // printf para o monitor serial
#include "freertos/FreeRTOS.h"     // o RTOS que vive dentro do ESP-IDF (semana 5)
#include "freertos/task.h"         // vTaskDelay e criação de tarefas
#include "driver/gpio.h"           // driver de GPIO do ESP-IDF (semana 3)

#define PINO_LED GPIO_NUM_2        // GPIO 2: LED azul embutido na maioria dos DevKits

void app_main(void)                // ponto de entrada do ESP-IDF (não é main()!)
{
    gpio_reset_pin(PINO_LED);                          // devolve o pino ao estado padrão
    gpio_set_direction(PINO_LED, GPIO_MODE_OUTPUT);    // configura como SAÍDA

    int nivel = 0;
    while (1) {                                        // firmware nunca "termina"
        nivel = !nivel;                                // alterna 0 ↔ 1
        gpio_set_level(PINO_LED, nivel);               // escreve no pino
        printf("LED = %d\n", nivel);                   // log no monitor serial
        vTaskDelay(pdMS_TO_TICKS(500));                // dorme 500 ms SEM gastar CPU
    }
}
```

Pontos que merecem sua atenção já hoje:

- **`app_main` em vez de `main`**: quando seu código começa, o ESP-IDF já inicializou o
  FreeRTOS e criou uma tarefa para você. Seu firmware *nasce dentro de um RTOS* — na semana 5
  vamos abrir essa caixa.
- **O laço infinito é proposital**: um firmware não tem "fim"; se `app_main` retornar, a
  tarefa morre e nada mais acontece. Pense no micro-ondas: o programa dele não “termina” —
  ele fica esperando você apertar botões pelo resto da vida do aparelho.
- **`vTaskDelay` ≠ atraso ocupado**: um laço `for` vazio manteria a CPU a 100 % queimando
  energia (releia o Exemplo 1.1!). `vTaskDelay` coloca a tarefa no estado *bloqueado*: a CPU
  fica livre (para outras tarefas ou para dormir). Essa pergunta está na entrega do Lab 1.
- **`pdMS_TO_TICKS(500)`**: o FreeRTOS conta tempo em *ticks* (batidas do relógio do sistema;
  10 ms cada, por padrão no ESP-IDF); essa macro converte milissegundos em ticks, para o seu
  código não depender da configuração do tick. Detalhes na semana 5.
- **`printf` sai pela UART**: no ESP32, o `printf` vai para a porta serial USB — é o nosso
  “olho” dentro do chip durante o desenvolvimento. Você o lerá no monitor serial do VS Code
  (ou na aba de monitor do Wokwi).

Saída esperada no monitor serial (ou no Wokwi):

```
LED = 1
LED = 0
LED = 1
...
```

> **Observação — "por que tanta cerimônia para piscar um LED?"** Porque a *estrutura* deste
> programa (configurar periférico → laço com temporização) é a mesma de um firmware de
> produto com 50 mil linhas. O pisca-LED é o “Olá, mundo!” do embarcado: prova que o
> toolchain compila, grava, que o chip executa e que você enxerga a saída. Dominar a
> estrutura no caso trivial é o que permite escalar depois.

---

## 4. Mercado e carreira

Automotivo (dezenas de MCUs por veículo, redes CAN — semana 10), industrial (CLPs,
inversores, IIoT), consumo (IoT doméstica, wearables), médico, agronegócio (sensores de solo
e estações meteorológicas — temos os sensores no inventário e são temas de projeto final!). O
Brasil importa a maior parte dos chips, mas o **projeto** de firmware e de produto é
intensivo em engenharia local — integradores, startups de agritech, tração elétrica, energia.
É onde vocês entram: a disciplina termina com um **projeto integrador** exatamente no formato
de um miniproduto (regulamento em `projeto-final/README.md`; leiam desde já).

Uma visão de onde chegam essas tecnologias: a figura abaixo mostra a arquitetura típica de um
sistema de controle industrial moderno — dispositivos de campo (sensores/atuadores) sob
aplicações de supervisão e controle. É o mesmo diagrama de blocos da seção 1.2, escalado para
uma fábrica — e é o mercado que contrata quem domina o conteúdo desta disciplina.

![Modelo de sistema de controle e medição industrial: dispositivos, aplicações e processo controlado](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/sistema_controle_medicao.png)

*Figura 1-E — Dispositivos de campo alimentando aplicações de controle sobre um processo —
a versão industrial da nossa anatomia. Fonte: Hands-On Industrial Internet of Things
(Packt), cap. 2, Fig. 2.4.*

---

## Resumindo

- Sistema embarcado = computador **dedicado e invisível**, projetado sob **restrições**; a
  restrição dominante dita a arquitetura.
- A anatomia é sempre: sensores → condicionamento → processamento → atuação/comunicação, com
  energia atravessando tudo.
- Autonomia de bateria sai da conta de carga por ciclo ÷ período; **software define consumo**
  (Exemplo 1.1: 10 meses × 25 horas com o MESMO hardware).
- Tempo real rígido: prazo perdido = falha. Brando: prazo perdido = degradação. A classe
  define se cabe um SO de propósito geral.
- **MCU** (ESP32) para tempo real e baixo consumo; **SoC + Linux** (RPi 3) para pilhas
  complexas; produtos reais combinam os dois — e a disciplina também.
- O fluxo de projeto passa por particionamento, seleção de plataforma e **simulação antes do
  hardware** (Wokwi, sempre).
- `vTaskDelay` bloqueia sem gastar CPU; laço vazio desperdiça energia.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| firmware | software gravado no produto, entre software e hardware |
| MCU | microcontrolador: CPU+RAM+flash+periféricos num chip só |
| SoC/MPU | processador de aplicação: CPU potente + MMU, RAM externa, roda Linux |
| RTOS | sistema operacional de tempo real (o nosso: FreeRTOS) |
| bare-metal | programa que roda direto no hardware, sem SO |
| hard/soft real-time | prazo como critério de correção / prazo como qualidade |
| WCET | tempo de execução de pior caso de um trecho de código |
| deep sleep | modo do ESP32 com quase tudo desligado (µA) — semana 14 |

## 📖 Onde aprofundar (opcional — a apostila já é suficiente)

- **Molloy**, *Exploring Raspberry Pi*, cap. 1, seção "Embedded Linux Devices" — o que
  caracteriza um dispositivo embarcado, contado por um professor de embarcados.
- ***Internet of Things from Scratch*** (Packt), caps. 1–2 — história e anatomia de objetos
  inteligentes.
- **Upton & Duntemann**, *Learning Computer Architecture with Raspberry Pi*, caps. 1–2 — a
  história do RPi contada por quem o criou.
- ***Hacking Electronics*** (Monk), cap. 1 — montagem em protoboard (leitura recomendada a
  quem nunca usou uma).

## Exercícios

Lista 1, questões 1–5 (`listas/lista-01.md`). As questões seguem o estilo dos Exemplos
resolvidos 1.1–1.3 — refaça-os com papel e lápis antes de tentar a lista.
