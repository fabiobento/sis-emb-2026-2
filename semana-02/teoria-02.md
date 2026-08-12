# Aula 2 — Arquitetura de Microcontroladores: o ESP32 e o Raspberry Pi por Dentro (U1/U2)

> **Pré-requisito**: Aula 1 (restrições, MCU × SoC, o blink no ESP-IDF).
> **Como usar**: texto autossuficiente — todos os conceitos são explicados do zero. Refaça os
> quatro exemplos resolvidos com calculadora; eles são o modelo exato das questões 6–10 da
> Lista 1.

Na semana passada você piscou um LED chamando `gpio_set_level()`. Mas o que essa função
*faz*, fisicamente? Como uma linha de C vira um pino de silício mudando de 0 V para 3,3 V?
Nessa aula vamos conversar desde a linguagem à arquitetura — CPU, barramentos, memórias
e registradores — usando nossas duas plataformas como espécimes de dissecação. No
laboratório, você vai gravar o firmware no ESP32 **físico** pela primeira vez e piscar o LED
escrevendo **diretamente num endereço de memória**, sem driver nenhum.

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) diferenciar as arquiteturas Von Neumann e Harvard e explicar por que MCUs usam Harvard;
- (b) descrever a organização interna do ESP32 e do BCM2837 (núcleos, memórias, periféricos);
- (c) explicar a hierarquia de memórias de um MCU e onde cada parte do seu programa "mora"
  (`.text`, `.rodata`, `.data`, `.bss`, pilha, heap);
- (d) explicar o que é um periférico mapeado em memória e usar um registrador diretamente,
  com `volatile`;
- (e) ler o “extrato” de memória que o `idf.py build` imprime ao final da compilação.

---

## 1. O modelo básico: CPU, memória e E/S conversando por barramentos

Todo computador — do ATmega ao datacenter — se reduz a três blocos ligados por
**barramentos** (feixes de fios compartilhados por onde trafegam bits): a **CPU** busca
instruções da memória, executa-as e lê/escreve dados na memória e nos periféricos de E/S.

```
        barramento de endereços (CPU diz ONDE)
   ┌───────────┬──────────────┬──────────────┐
   │           │              │              │
 ┌─┴──┐   ┌────┴────┐   ┌─────┴─────┐   ┌────┴───────┐
 │CPU │   │ MEMÓRIA │   │ MEMÓRIA   │   │ PERIFÉRICOS│
 │    │   │ (código)│   │ (dados)   │   │(GPIO, ADC…)│
 └─┬──┘   └────┬────┘   └─────┬─────┘   └────┬───────┘
   │           │              │              │
   └───────────┴──────────────┴──────────────┘
        barramento de dados (o QUE trafega)
```

Três barramentos trabalham juntos (o terceiro, de controle, não aparece no projeto):

- **Barramento de endereços**: a CPU coloca nele *onde* quer ler/escrever — um número, o
  **endereço**. A largura deste barramento define o “espaço de endereçamento”: 32 bits de
  endereço ⇒ 2³² posições = 4 GB endereçáveis. Por isso sistemas de 32 bits “enxergam” no
  máximo 4 GB de memória — número que aparece de novo quando comparamos MCU (KB de RAM) e
  SoC (GB de RAM).
- **Barramento de dados**: por onde os bits lidos/escritos efetivamente trafegam. Largura
  típica: 32 bits nos nossos dois processadores — daí “processador de 32 bits”.
- **Barramento de controle**: sinais que dizem *o que está acontecendo* — leitura ou escrita,
  memória ou E/S, interrupções.

> 💡 **Analogia para fixar**: pense num prédio de apartamentos (a memória). O barramento de
> endereços é a lista de números dos apartamentos; o de dados é o elevador que leva e traz
> encomendas; o de controle é o interfone que avisa “é entrega” (escrita) ou “é retirada”
> (leitura). Sem número do apartamento, o elevador não sabe onde parar.

### 1.1 Von Neumann × Harvard

Há duas formas clássicas de organizar esse desenho:

![Arquiteturas Von Neumann (barramento único) e Harvard (barramentos separados)](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/von_neumann_harvard.png)

*Figura 2-A — À esquerda, Von Neumann: um corredor só para tudo. À direita, Harvard: dois
corredores paralelos — buscar a próxima instrução não atrapalha o acesso a dados.*

- **Von Neumann**: instruções e dados compartilham **o mesmo** barramento e memória. Simples
  e flexível (o programa é só mais um dado — por isso seu PC carrega programas do disco para
  a RAM), mas cria o "gargalo de Von Neumann": a CPU não pode buscar a próxima instrução
  *enquanto* transfere um dado — é um corredor único para tudo.
- **Harvard**: barramentos e memórias **separados** para instruções e dados. A busca da
  próxima instrução acontece em paralelo com o acesso a dados — dois corredores. É a escolha
  natural de MCUs, que já têm memórias fisicamente diferentes para código (flash) e dados
  (SRAM), e que valorizam vazão previsível acima de flexibilidade.

MCUs modernos (AVR, Xtensa do ESP32, ARM Cortex-M) usam **Harvard modificada**: espaços
separados, mas com "pontes" que permitem, por exemplo, ler constantes da flash como dados ou
executar código da RAM (é o que a `IRAM_ATTR` da semana 4 explora: mover uma ISR da flash —
lenta e às vezes indisponível durante gravações — para a SRAM, sempre disponível).

> 💡 **Pense aí**: por que o seu notebook usa Von Neumann e não Harvard? *Dica: como ele
> instala programas novos?* Resposta: porque no PC o programa é dado (vem do disco para a
> RAM) — separar memória de programa de memória de dados impediria carregar software. O
> embarcado não instala nada: o programa nasce na flash de fábrica — Harvard não custa
> flexibilidade alguma e entrega desempenho e previsibilidade.

### 1.2 RISC × CISC — e a conta que quantifica a diferença entre nossas placas

**RISC** (*Reduced Instruction Set Computer*): instruções simples, de tamanho regular,
executadas em ~1 ciclo cada — a complexidade fica no compilador. **CISC** (*Complex…*):
instruções complexas de tamanho variável que fazem muito por instrução (x86 é o sobrevivente
famoso). Todo o nosso hardware é RISC: o Xtensa LX6 do ESP32 e o ARM Cortex-A53 do RPi 3.
A consequência prática para embarcados é que dá para *estimar* tempo de execução contando
instruções — habilidade que usaremos em rotinas de tempo real:

**Exemplo resolvido 2.1 (estimativa por contagem de instruções)** — Um processador executa em
média 1 instrução/ciclo (RISC). A 240 MHz (ESP32), um laço de leitura de sensor com 1 200
instruções roda em quanto tempo? E num ATmega328P a 16 MHz?

*Solução.* Tempo = instruções ÷ (instruções/ciclo × ciclos/segundo):

- ESP32: 1 200 / (1 × 240·10⁶) = **5 µs**
- ATmega328P: 1 200 / (1 × 16·10⁶) = **75 µs** — 15× mais lento.

É por isso que o Arduino Uno terá papel *limitado* na disciplina: serve para eletrônica
básica, mas não para as cargas (Wi-Fi, DSP, RTOS) que exploraremos. Note que a diferença
vem **só do clock** — mesmo IPC (instruções por ciclo). Na seção 3.1 veremos por que, na
prática, processadores como o A53 entregam ainda mais do que a razão de clocks promete.

> 📐 **Método**: em qualquer conta de desempenho, separe os três fatores:
> **tempo = (nº de instruções) × (ciclos/instrução) × (segundos/ciclo)**. Otimizar código é
> atacar o primeiro fator; arquitetura (pipeline, cache) ataca o segundo; clock ataca o
> terceiro. Saber qual fator domina o seu gargalo é metade da engenharia de desempenho.

---

## 2. O SoC ESP32 por dentro

Se abríssemos o envólucro metálico do módulo ESP-WROOM-32 da sua bancada, veríamos que dentro há um chip
(o SoC ESP32) e uma memória flash SPI ao lado. No chip:

- **CPU**: 2× Xtensa LX6 de 32 bits até 240 MHz. Os núcleos têm apelidos: **PRO_CPU** (core
  0, onde o ESP-IDF roda Wi-Fi/Bluetooth por padrão) e **APP_CPU** (core 1, "seu"). Semana 5:
  fixaremos tarefas em núcleos específicos e veremos o que acontece quando duas tarefas
  disputam o mesmo núcleo.
- **Memória**: 520 KB de SRAM interna (dados, pilhas, heap); 448 KB de ROM (bootloader e
  funções de fábrica — o chip “sabe” dar boot antes de qualquer código seu existir); flash
  SPI **externa ao chip, interna ao módulo** (4 MB: seu código, constantes, NVS — o
  “armazenamento de configurações” do ESP-IDF); memória RTC *slow/fast* (8+8 KB) que
  **sobrevive ao deep sleep** (semana 14 — é onde o nó sensor guarda o que não pode esquecer
  enquanto dorme).
- **Periféricos**: 34 GPIOs, ADC de 12 bits (18 canais), 2× DAC de 8 bits, 16 canais de PWM
  (LEDC), 3× UART, 2× I2C, 4× SPI, **TWAI (CAN!)**, I2S, sensores touch — veremos nas semanas 7–10 em silício. Cada um deles é um bloco de hardware independente que
  a CPU configura e comanda… escrevendo em endereços de memória (seção 4.2!).
- **Rádios** Wi-Fi 802.11 b/g/n e Bluetooth + aceleradores criptográficos (AES, SHA, RSA em
  hardware — TLS não sufoca a CPU) — a razão do domínio do ESP32 em IoT.

> **Observação — módulo × chip × placa.** *Chip* ESP32 (o silício) ⊂ *módulo* ESP-WROOM-32
> (chip + flash + antena + blindagem, certificado pela Anatel) ⊂ *placa* DevKit (módulo +
> conversor USB-serial + regulador 3,3 V + botões EN/BOOT). Vocês compram placas; produtos
> integram módulos (ninguém certifica rádio do zero — usa o módulo já certificado); e quase
> ninguém usa o chip pelado. Essa hierarquia chip→módulo→placa existe por uma razão
> regulatória e econômica: o desenho de RF (antena, casamento de impedância) é a parte mais
> difícil e mais cara de certificar.

## 3. O Raspberry Pi 3 por dentro

O RPi 3 é outra espécie: SoC Broadcom **BCM2837** com 4× ARM Cortex-A53 @ 1,2 GHz e GPU
VideoCore IV; **1 GB de RAM LPDDR2 num chip separado** (a diferença física mais visível para
o MCU: memória externa, muito maior e mais lenta por acesso); armazenamento em cartão
microSD; Ethernet/Wi-Fi/BT; e o headerde **40 pinos GPIO** que usaremos no Bloco 2.

Volte à Figura 1-C da Aula 1 (o layout do RPi 3B) e identifique: o SoC quadrado no centro;
o slot microSD na parte de baixo (o “disco rígido” do Pi — sem ele, o Pi não liga, pois…);
o headerGPIO de 40 pinos no topo; e os conectores USB/Ethernet à direita. Duas ausências
são decisões de projeto com consequências didáticas enormes:

- **Não há ADC nativo** — o RPi não lê tensões analógicas diretamente. Diferença crucial que
  voltará na semana 12: para ler nosso LDR no Pi, usaremos um truque de tempo de carga de
  capacitor ou um ADC externo I2C (ADS1115).
- **Não há flash de código: tudo vem do cartão SD** — bootloader, kernel, sistema de
  arquivos, seus programas. Isso exige um processo de boot em etapas (semana 11) e explica
  por que “cartão corrompido = Pi morto”.

### 3.1 Por que o Cortex-A53 é tão mais rápido? Pipeline, superescalar e cache

O A53 do RPi não vence o LX6 só por clock (1,2 GHz × 240 MHz = 5×; o desempenho real difere
bem mais). Três técnicas de arquitetura explicam — e caem em prova de concurso e entrevista:

1. **Pipeline** — a linha de montagem de instruções. Executar uma instrução envolve etapas:
   **buscar** da memória, **decodificar**, **executar**, **acessar memória**, **escrever
   resultado**. Sem pipeline, a instrução seguinte espera todas as etapas da anterior: cada
   instrução, 5 ciclos. Com pipeline, as etapas se sobrepõem como numa linha de montagem:
   enquanto a instrução A executa, a B é decodificada e a C é buscada. Em regime, ~1
   instrução **concluída** por ciclo, mesmo que cada uma leve 5+ ciclos de ponta a ponta.

![Diagrama temporal de um pipeline de 5 estágios com 5 instruções](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/pipeline.png)

*Figura 2-B — O pipeline de 5 estágios: cada instrução ainda leva 5 ciclos, mas a vazão é de
~1 instrução por ciclo. O preço: desvios (if/else) podem esvaziar a linha de montagem —
ciclos desperdiçados chamados “bolhas”.*

2. **Superescalar** — unidades de execução duplicadas permitem **despachar 2+ instruções no
   mesmo ciclo**, quando não há dependência entre elas (o A53 é *dual-issue*: despacha até
   duas). `a = b + c` e `d = e + f` não dependem uma da outra → rodam juntas; já `a = b + c`
   seguida de `d = a * 2` força espera (a segunda precisa do resultado da primeira).
3. **Cache** — uma SRAM pequena e rápida junto à CPU guarda **linhas** (blocos de 32–64 B)
   da DRAM lenta. Acerto (*hit*): ~1–3 ciclos; falta (*miss*): dezenas de ciclos indo até a
   LPDDR2. O princípio que a faz funcionar é a **localidade**: programas tendem a reutilizar
   dados próximos no tempo (localidade temporal — a variável do laço) e no espaço
   (localidade espacial — o elemento seguinte do array). Programas com laços compactos e
   dados **contíguos** "moram" na cache — um dos motivos para preferir *arrays* a listas
   encadeadas em código de tempo real (o outro é o determinismo: miss de cache é jitter,
   variação imprevisível de tempo — veneno para tempo real).

**Exemplo resolvido 2.2 (efeito da cache)** — Um laço processa 10 000 amostras. Compare os
acessos com dados contíguos (array) e com dados espalhados (lista encadeada), supondo 50 %
de miss na lista e custo de miss de 40 ciclos.

*Solução passo a passo.*

- Array contíguo: quase todos os acessos são *hits* de ~2 ciclos (a linha de cache já trouxe
  os vizinhos): 10 000 × 2 = **20 000 ciclos**.
- Lista encadeada: 5 000 hits × 2 + 5 000 misses × 40 = 10 000 + 200 000 = **210 000
  ciclos**.

Mais de 10× pior, com o *mesmo* algoritmo O(n) — a notação de complexidade não vê a
constante, mas o pino do seu atuador, vê. Em MCU sem cache (ESP32 na SRAM interna) o efeito
não existe; no RPi, domina. Layout de dados **é** desempenho — e determinismo.

---

## 4. Memórias e registradores (U2)

### 4.1 A hierarquia — e onde seu programa "mora"

Do mais rápido ao mais lento num MCU: **registradores** da CPU (acesso em 1 ciclo, ~16–32
deles) → **SRAM** (dados, pilha, heap) → **flash** (código e constantes) → **NVS/EEPROM**
(configuração persistente). Mais capacidade = mais lento e mais barato por bit; menos
capacidade = mais rápido e mais caro. Não existe “memória boa para tudo” — existe a
hierarquia certa:

![Pirâmide da hierarquia de memória com exemplos do ESP32](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/hierarquia_memoria.png)

*Figura 2-C — A hierarquia de memória: perto da CPU, pouco e rápido; longe, muito e lento.
O compilador e o linker decidem onde cada parte do seu programa mora — e você pode (e às
vezes deve) interferir.*

O linker distribui seu programa em **seções**:

| Seção | O que contém | Onde fica |
|---|---|---|
| `.text` | o código de máquina | flash |
| `.rodata` | constantes (`const`, strings) | flash |
| `.data` | globais **inicializadas** (`int x = 5;`) | valores na flash, **copiados** para a SRAM no boot |
| `.bss` | globais não inicializadas (zeradas) | SRAM |
| pilha/heap | locais, retorno de funções / `malloc` | SRAM |

Leia a linha da `.data` duas vezes — ela esconde um truque elegante. Uma global inicializada
precisa do seu valor inicial guardado em algum lugar não-volátil (a flash), mas precisa ser
*alterável* em tempo de execução (logo, precisa morar na RAM). Solução do código de startup:
antes de `app_main` rodar, ele **copia** os valores iniciais da flash para a SRAM e zera a
`.bss`. Essa cópia invisível explica um mistério clássico de prova: por que globais
inicializadas "gastam" flash **e** RAM, enquanto as não inicializadas gastam só RAM — é
exatamente uma questão da Lista 1.

> 💡 **Pense aí**: `static int contador = 42;` ocupa quantos bytes de flash e quantos de
> RAM? *Resposta: 4 de flash (o valor 42) + 4 de RAM (a variável viva). E se fosse
> `= 0`? Só 4 de RAM — o compilador a manda para a `.bss`, zerada de graça no boot.*

### 4.2 Periféricos mapeados em memória: a grande sacada

Como a CPU conversa com o GPIO, o ADC, a UART? Nos processadores modernos, **não há
instruções especiais de E/S**: cada periférico expõe seus **registradores** de
controle/estado/dados em **endereços fixos** do mapa de memória. Escrever naquele endereço
*é* mexer no periférico. A mesma instrução `store` que grava uma variável liga um pino — só
muda o endereço. É a ideia mais importante desta aula: **para a CPU, hardware é memória**.

![Mapa de memória do ESP32 mostrando a faixa de periféricos e o endereço do GPIO_OUT_REG](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mapa_memoria_esp32.png)

*Figura 2-D — Mapa de memória do ESP32 (simplificado): periféricos ocupam uma faixa de
endereços como qualquer memória. Escrever em `0x3FF44004` não grava um dado — liga pinos.*

**Exemplo resolvido 2.3 (mapa de memória)** — No ESP32, o registrador `GPIO_OUT_REG` fica em
`0x3FF44004`; cada bit dele comanda um pino. Fazer

```c
*(volatile uint32_t*)0x3FF44004 |= (1 << 2);
```

liga o GPIO 2 *sem usar driver nenhum* — é exatamente o que `gpio_set_level()` faz por baixo
(com validações e conveniências). Disseque a linha: `(volatile uint32_t*)0x3FF44004` converte
o número literal num ponteiro para inteiro de 32 bits naquele endereço; `*(...)` acessa o
conteúdo (o registrador de hardware); `|= (1 << 2)` seta o bit 2 preservando os demais. O
qualificador **`volatile`** é obrigatório aqui: ele avisa ao compilador que aquele endereço
pode mudar “sozinho” (o hardware escreve nele!) e que cada acesso seu tem efeito físico —
sem `volatile`, otimizações legítimas (eliminar leituras “redundantes”, guardar em
registrador) quebrariam o programa. A semana 3 destrincha isso.

Vamos ler o firmware do laboratório de hoje (`src/blink_registrador/main.c`), que pisca o LED
**duas vezes por segundo escrevendo direto nos registradores**:

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_reg.h"      // define GPIO_OUT_W1TS_REG etc. (evita números mágicos)
#include "driver/gpio.h"

#define BIT_LED (1u << 2)      // GPIO 2 → bit 2 dos registradores de GPIO 0–31

void app_main(void)
{
    gpio_reset_pin(GPIO_NUM_2);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);   // direção ainda via driver (didática)

    volatile uint32_t *w1ts = (volatile uint32_t *)GPIO_OUT_W1TS_REG; // "write 1 to SET"
    volatile uint32_t *w1tc = (volatile uint32_t *)GPIO_OUT_W1TC_REG; // "write 1 to CLEAR"

    while (1) {
        *w1ts = BIT_LED;                    // liga: escreve 1 SÓ no bit 2 (atômico!)
        vTaskDelay(pdMS_TO_TICKS(250));
        *w1tc = BIT_LED;                    // desliga: idem
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
```

Repare no par **W1TS/W1TC** (*write-1-to-set / write-1-to-clear*): em vez de ler o
registrador, alterar um bit e escrever de volta (3 passos, com risco de outra tarefa/ISR
mexer no meio — a "corrida" da semana 6), você escreve 1 apenas nos bits desejados e o
hardware seta/limpa **só eles**, atomicamente (uma única escrita, impossível de interromper
no meio). É um padrão de projeto de hardware que você reencontrará em todo MCU — e a
primeira vacina contra corridas que esta disciplina oferece, três semanas antes de
formalizarmos o problema.

**Exemplo resolvido 2.4 (dimensionamento de memória)** — Um buffer de aquisição de 2 s de
áudio a 16 kHz, 16 bits cabe no ESP32? E 60 s?

*Solução passo a passo.* Bytes = segundos × amostras/segundo × bytes/amostra:

- 2 s: 2 × 16 000 × 2 = **64 000 bytes** ≈ 62,5 KB. Cabe na SRAM do ESP32 (520 KB), com
  folga — não cabe num ATmega328P (2 KB).
- 60 s: 60 × 16 000 × 2 = **1 920 000 bytes** ≈ 1,9 MB. Não cabe na SRAM de nenhum MCU da
  nossa bancada. A saída não é “comprar um chip maior”: é *streaming* — processar ou enviar
  em blocos (double buffering, semana 7) ou gravar em armazenamento (semana de *storage*).
  Este tipo de conta orienta a *seleção de plataforma* (semana 1) antes de qualquer linha
  de código.

---

## 5. Toolchain: o caminho do C ao silício

O que acontece quando você digita `idf.py build`?

```
main.c ──▶ COMPILADOR CRUZADO ──▶ main.o ──▶ LINKER ──▶ app.elf ──▶ app.bin ──▶ FLASH
           (xtensa-esp32-elf-gcc)  (objeto)   (junta objetos,       (imagem     (gravada via
            roda no seu x86,                   aloca .text/.data     binária)    UART pelo
            gera código Xtensa                 nos endereços               idf.py flash)
```

- **Compilador cruzado** (*cross-compiler*): roda no seu PC x86, mas gera código de máquina
  **Xtensa**. “Cruzado” porque a máquina que compila ≠ a máquina que executa. O firmware não
  roda no seu PC — só no alvo (ou no simulador). Por isso não adianta “testar o .bin com
  duplo clique”: ele é idioma estrangeiro para o seu processador.
- **Linker**: junta todos os objetos e bibliotecas e atribui **endereços** conforme o *linker
  script*: `.text` na flash (a partir de 0x10000 no layout padrão — você verá esse número na
  saída do `idf.py flash` hoje), `.data`/`.bss` na SRAM. É o linker que materializa a tabela
  de seções da seção 4.1.
- `idf.py` orquestra tudo (CMake + ninja) e ainda grava (`flash`) e monitora a serial
  (`monitor`).

No fim do build, o ESP-IDF imprime um resumo precioso — aprenda a lê-lo hoje:

```
Total sizes:
Used static IRAM:   58432 bytes
Used static DRAM:   14720 bytes (.data + .bss)
Used Flash size :  148231 bytes (.text + .rodata)
```

É o "extrato bancário" das suas memórias: quanto de RAM (IRAM = RAM de instruções, DRAM =
RAM de dados) e de flash o firmware consome. Estourou? O linker recusa com um erro de
“overflow” — e as contas do Exemplo 2.4 viram diagnóstico: o que está grande? Código (corte
features, use `-Os`), constantes (mova tabelas para NVS) ou dados (buffers grandes demais —
streaming!)?

---

## Resumindo

- Computador = CPU + memória + E/S sobre barramentos (endereço: ONDE; dados: O QUÊ; controle:
  COMO). 32 bits de endereço ⇒ 4 GB de espaço.
- **Harvard** separa código de dados e é o padrão em MCUs (o programa não é “dado
  carregável”); **RISC** permite estimar tempo contando instruções (Exemplo 2.1: 15× entre
  ESP32 e Uno só pelo clock).
- ESP32: 2 núcleos LX6, 520 KB SRAM, flash externa no módulo, periféricos = nosso cronograma.
  RPi 3: 4× A53 + 1 GB de DRAM externa + GPU; **sem ADC**; código no cartão SD.
- Pipeline, superescalar e **cache** explicam o desempenho do A53 — e cache torna o *layout
  dos dados* uma decisão de desempenho e de determinismo (Exemplo 2.2: 10× só de mudar a
  estrutura de dados).
- O linker distribui seu programa em seções: `.data` inicializada mora na flash **e** na RAM
  (copiada no boot); `.bss` só na RAM. `idf.py build` mostra o extrato de cada memória.
- Periférico **mapeado em memória**: escrever num endereço é mexer no hardware; `volatile`
  obrigatório; registradores W1TS/W1TC dão atomicidade de graça (Exemplo 2.3).

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| barramento | feixe de fios compartilhado (endereços / dados / controle) |
| pipeline | sobreposição das etapas de execução das instruções |
| superescalar | despachar 2+ instruções por ciclo |
| cache | SRAM rápida que espelha linhas da memória lenta |
| hit / miss | acerto / falta na cache (miss custa dezenas de ciclos) |
| localidade | tendência de reutilizar dados próximos no tempo/espaço |
| MMU | unidade de gerenciamento de memória (habilita SO com processos) |
| seção (.text/.data/.bss) | fatias do programa alocadas pelo linker |
| volatile | qualificador C: “este endereço muda sozinho; não otimize” |
| mapeado em memória | periférico acessado por endereços comuns |

## 📖 Onde aprofundar (opcional)

- **Upton & Duntemann**, *Learning Computer Architecture with Raspberry Pi*, caps. 3–4 — O
  livro para esta semana: memória, cache, pipeline e superescalar explicados sobre o próprio
  RPi, escritos por um dos fundadores da Raspberry Pi.
- **Molloy**, *Exploring Raspberry Pi*, cap. 1 — subsistemas e alimentação do RPi.
- **ESP-IDF Programming Guide** + *ESP32 Technical Reference Manual*, cap. "System and
  Memory" — o mapa de memória oficial, bit a bit (referência para o Lab 2).
- ***IoT Programming Projects*** (Packt), cap. 1 — as famílias RPi e ESP32 em fotos.

## Exercícios

Lista 1, questões 6–10 — todas no estilo dos Exemplos 2.1–2.4. Refaça os exemplos com a
calculadora antes; confira sempre as unidades no final de cada conta.
