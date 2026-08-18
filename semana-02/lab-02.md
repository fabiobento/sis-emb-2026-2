# Lab 2 — ESP-IDF no hardware: build, flash, monitor e registradores

> **Antes de começar**: leia a [teoria-02](teoria-02.md) — principalmente as seções 4.2
> (periféricos mapeados em memória) e 5 (toolchain). Hoje você verá, com os próprios olhos,
> os dois conceitos: o endereço `0x3FF44004` ligando um LED e o linker distribuindo seu
> programa pelas memórias.

**Objetivo**: compilar, gravar e monitorar seu primeiro firmware no **ESP32 físico**;
interpretar a saída do build (consumo de memórias e endereços de gravação); reproduzir o
pisca-LED escrevendo direto nos registradores (Exemplo 2.3 da teoria).

**Duração**: 2 aulas.
**Material**: 1× ESP32 DevKit na placa breakout com bornes, cabo micro-USB **de dados**,
LED + resistor 220 Ω, protoboard, multímetro da bancada.

---

## Parte 0 — Sincronize o repositório (sempre!)

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — Do exemplo oficial ao seu primeiro flash (45 min)

**A.1** Copie o exemplo *blink* do ESP-IDF para uma pasta de trabalho:

```bash
get_idf
cp -r $IDF_PATH/examples/get-started/blink ~/lab2 && cd ~/lab2
idf.py set-target esp32
```

> 💡 **O que é `$IDF_PATH`?** Uma variável de ambiente que o `get_idf` criou apontando para
> a pasta de instalação do ESP-IDF — onde moram os drivers, o FreeRTOS e centenas de
> exemplos oficiais. Explore-a quando sobrar tempo: `$IDF_PATH/examples` é uma biblioteca
> de firmware profissional comentado.

**A.2** Configure o projeto com o `menuconfig` (a central de configuração do ESP-IDF):

```bash
idf.py menuconfig
```

Navegue com as setas: *Example Configuration* → **Blink GPIO number = 2** (o LED azul
embutido). Antes de sair, faça um passeio: *Component config → FreeRTOS → Kernel* e
localize **configTICK_RATE_HZ = 100**. Anote esse número — na semana 5 ele explicará por que
a resolução do `vTaskDelay` é 10 ms. Salve (`S`) e saia (`Q`).

**A.3** Compile e **leia** o resumo final:

```bash
idf.py build
```

Ao final, procure o bloco `Total sizes:` e anote no relatório: bytes de **DRAM**, de
**IRAM** e de **Flash** usados. (Teoria, seção 5: é o extrato das memórias — DRAM guarda
`.data`+`.bss`+pilhas, IRAM guarda código marcado para RAM como as ISRs da semana 4, e a
flash guarda `.text`+`.rodata`, o seu programa propriamente dito.)

**A.4** Conecte o ESP32 e descubra a porta serial:

```bash
ls /dev/ttyUSB*
```

Saída esperada: `/dev/ttyUSB0`. Se nada aparecer então: (i) o cabo é só de carga? troque —
cabos USB "de carga" não têm os fios de dados e são o que mais dá erro nesta etapa; (ii)
permissão — rode `sudo usermod -aG dialout $USER` e relogue (já feito nos PCs do lab, mas
anote para o seu notebook).

**A.5** Grave e então monitore (o par de comandos mais usado do semestre):

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Na avalanche de mensagens do processo de gravação (*flash*), **cace as linhas que confirmam a escrita dos arquivos** e o log inicial de boot (endereços do layout de flash — teoria, seção 5):

```bash
Wrote 26704 bytes (...) at 0x00001000...
Wrote 184592 bytes (...) at 0x00010000...
Wrote 3072 bytes (...) at 0x00008000...
...
I (180) boot: Loaded app from partition at offset 0x10000
```


> **Observação:** se a gravação falhar com "Failed to connect", segure o botão **BOOT** da
> placa enquanto o `idf.py` tenta conectar e solte quando começar. Algumas placas exigem
> isso (o botão BOOT força o modo de gravação no reset).

## Parte B — Pisca por registrador (40 min)

**B.1** Substitua o conteúdo de `main/blink_example_main.c` pelo nosso
`~/sis-emb/semana-02/src/blink_registrador/main.c` (detalhado na seção 4.2 da teoria —
releia o bloco W1TS/W1TC antes de gravar), e possui uma abordagem "lê-modifica-escreve" .
```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_reg.h"      
#include "driver/gpio.h"

#define BIT_LED (1u << 2)      // GPIO 2 → bit 2 dos registradores de GPIO 0–31

void app_main(void)
{
    gpio_reset_pin(GPIO_NUM_2);
    
    // Substitui a direção via driver pelo acesso direto ao registrador ENABLE
    *(volatile uint32_t *)GPIO_ENABLE_REG |= BIT_LED;

    // Aponta para o registrador de saída geral (onde moram todos os 32 pinos)
    volatile uint32_t *out = (volatile uint32_t *)GPIO_OUT_REG; 

    while (1) {
        *out |= BIT_LED;                    // liga: lê, modifica e escreve (NÃO atômico!)
        vTaskDelay(pdMS_TO_TICKS(250));
        
        *out &= ~BIT_LED;                   // desliga: lê, modifica e escreve (NÃO atômico!)
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
```


**B.2** `idf.py flash monitor`. O efeito visível é o mesmo do driver — e essa é a lição:
o driver é só uma casca conveniente sobre os registradores. Você acabou de comprovar a
frase central da semana: **para a CPU, hardware é memória** — escrever 1 no bit 2 do
endereço `0x3FF44008` liga o pino, exatamente como escrever numa variável.

![Mapa de memória do ESP32 com a faixa de periféricos e o registrador de GPIO](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mapa_memoria_esp32.png)

*Figura L2-A — Reveja o mapa da teoria: o registrador que seu código escreveu hoje mora na
faixa de periféricos. É um endereço como outro qualquer, só que com pinos
vinculados.*

**B.3 Experimento — atomicidade**: no código, troque o código do item **B.1** por uma versão com maior atomicidade usando o seguinte código: 
```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_reg.h"      // define GPIO_OUT_W1TS_REG etc. (evita números mágicos)
#include "driver/gpio.h"

#define BIT_LED (1u << 2)      // GPIO 2 → bit 2 dos registradores de GPIO 0–31

void app_main(void)
{
    gpio_reset_pin(GPIO_NUM_2);
    
    // Substitui a direção via driver pelo acesso direto ao registrador ENABLE
    *(volatile uint32_t *)GPIO_ENABLE_REG |= BIT_LED;

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

**B.4 A questão do "Lê-Modifica-Escreve" e a vantagem da Atomicidade**

Você deve ter percebido que, visualmente, o LED pisca da mesma forma nas duas abordagens. No entanto, no nível de hardware , a diferença estrutural entre elas é o que separa um sistema embarcado instável e sujeito a falhas intermitentes de um sistema robusto e profissional.

* **A armadilha do código B.1 (Lê-Modifica-Escreve):** Ao utilizarmos os operadores bit a bit (`|=` e `&= ~`) diretamente no registrador geral `GPIO_OUT_REG`, obrigamos o processador a executar três etapas distintas: ele **lê** o estado de todos os 32 pinos, **modifica** o bit desejado na sua memória interna e **escreve** o pacote de 32 bits de volta no registrador. Se uma Rotina de Serviço de Interrupção (ISR) for acionada no meio dessas etapas e alterar um pino diferente no mesmo registrador, o passo final de "escrita" do seu loop principal devolverá a "foto antiga" dos pinos, apagando silenciosamente a alteração feita pela ISR. Isso é o que chamamos de Condição de Corrida (*Race Condition*).
* **A segurança do código B.3 (Atomicidade via Hardware):** Para resolver esse problema, a arquitetura do ESP32 nos oferece os registradores `W1TS` (Write 1 To Set) e `W1TC` (Write 1 To Clear). Eles dispensam a leitura prévia. Basta executar uma única instrução de escrita informando quais bits devem ir para `1`, e o próprio circuito lógico do microcontrolador altera exclusivamente aqueles pinos, ignorando os que receberam `0`. Por ocorrer em um único ciclo indivisível de instrução (uma operação **atômica**), é impossível que uma interrupção atropele o processo.

> **OBSERVAÇÃO:** Sempre que o hardware do microcontrolador oferecer registradores dedicados do tipo *Set/Clear* atômicos, o uso deles deve ser a sua escolha padrão em projetos de sistemas embarcados.

> *(Nota para a Semana 6: a operação `|=` executa a leitura, modificação e escrita em três passos distintos, abrindo brecha para perda de dados em caso de interrupção. O uso de W1TS garante a atomicidade, realizando a operação em uma única instrução em nível de hardware).*


## Parte C — Eletrônica de bancada (30 min)

**C.1** Monte na protoboard: **GPIO 2 → resistor 220 Ω → anodo do LED → catodo → GND**
(mesma topologia do Wokwi do Lab 1, agora com elétrons de verdade). O LED externo deve
piscar em sincronia com o azul da placa.

> 🔌 **Protoboard — o mapa**: as duas colunas das bordas são trilhos contínuos de
> alimentação (+ e −); as fileiras centrais conectam 5 furos em linha, separadas pela
> canaleta do meio. Se o circuito não fecha, 80 % das vezes é fio em fileira vizinha ou
> trilho de alimentação não conectado dos dois lados.

**C.2 Medições com o multímetro** (boa prática de *Molloy, Exploring Raspberry Pi*):
meça **sempre com o circuito energizado e o firmware rodando**, ponta preta no GND,
vermelha no ponto de interesse, escala DC 20 V:

| Medição | Onde | Valor esperado | Medido |
|---|---|---|---|
| Alimentação do módulo | pino 5V ↔ GND | 4,75–5,25 V | |
| Nível alto no GPIO | GPIO 2 ↔ GND (LED aceso) | ~3,3 V | |
| Queda no LED | anodo ↔ catodo (aceso) | ~1,8–2,1 V | |

**C.3** Com as medições, **verifique o Exemplo 3.2** por antecipação:
I = (V_GPIO − V_LED)/220. O valor bate com a faixa de 5–6 mA? Se a queda no LED medir ~2,0 V
e o GPIO ~3,3 V: I = (3,3 − 2,0)/220 ≈ 5,9 mA — a conta da semana 3 confirmada por um
instrumento. Anotem os três números: eles são a primeira ponte entre a apostila e a
bancada.

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| `/dev/ttyUSB0` não existe | cabo só de carga | troque por cabo de dados |
| "Failed to connect" no flash | auto-reset falhou | segure BOOT ao gravar |
| "Permission denied" na porta | usuário fora do grupo dialout | `sudo usermod -aG dialout $USER`, relogue |
| flash ok, LED azul pisca, externo não | fiação/polaridade | anodo no resistor, catodo no GND |
| build falha em `set-target` | rodou fora da pasta do projeto | `cd ~/lab2` e repita |

## Entrega (GitHub da bancada, `lab-02/relatorio.md`)

1. Foto da montagem + print do monitor serial.
2. Tabela `Total sizes` (DRAM/IRAM/Flash) do seu build e os **três endereços** de gravação
   (bootloader/partições/app) — com uma frase explicando o que é gravado em `0x10000`.
3. Tabela de medições da Parte C preenchida + a corrente calculada.
4. Resposta da Parte B.3 (atomicidade W1TS/W1TC), 3–5 linhas.

## Desafio (opcional)

Sem usar `gpio_set_direction`, configure a direção do GPIO 2 escrevendo no registrador
`GPIO_ENABLE_W1TS_REG` (dica: `soc/gpio_reg.h`). Confirme no monitor que o pisca continua e
explique no relatório o paralelo com o par OUT/ENABLE.
