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
cd ~/sis-emb && git fetch && git reset --hard origin/main
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

Saída esperada: `/dev/ttyUSB0`. Se nada aparecer: (i) o cabo é só de carga? troque —
cabos USB "de carga" não têm os fios de dados e são o vilão nº 1 desta etapa; (ii)
permissão — rode `sudo usermod -aG dialout $USER` e relogue (já feito nos PCs do lab, mas
anote para o seu notebook).

**A.5** Grave e monitore (o par de comandos mais usado do semestre):

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Na avalanche de mensagens do flash, **cace estas linhas** (endereços do layout de flash —
teoria, seção 5):

```
Bootloader binary ... 0x1000
Partition table   ... 0x8000
App binary        ... 0x10000
```

Cada um tem um papel: em `0x1000` vai o bootloader (o "preparador de terreno" que roda
antes do seu código); em `0x8000`, a tabela de partições (o mapa das fatias da flash:
app, NVS, etc.); em `0x10000`, **o seu aplicativo**. O LED azul da placa deve piscar. Para
sair do monitor: **`Ctrl + ]`**.

> **Observação:** se a gravação falhar com "Failed to connect", segure o botão **BOOT** da
> placa enquanto o `idf.py` tenta conectar e solte quando começar. Algumas placas exigem
> isso (o botão BOOT força o modo de gravação no reset).

## Parte B — Pisca por registrador (40 min)

**B.1** Substitua o conteúdo de `main/blink_example_main.c` pelo nosso
`~/sis-emb/semana-02/src/blink_registrador/main.c` (dissecado na seção 4.2 da teoria —
releia o bloco W1TS/W1TC antes de gravar).

**B.2** `idf.py flash monitor`. O efeito visível é o mesmo do driver — e essa é a lição:
o driver é só uma casca conveniente sobre os registradores. Você acabou de comprovar a
frase central da semana: **para a CPU, hardware é memória** — escrever 1 no bit 2 do
endereço `0x3FF44008` liga o pino, exatamente como escrever numa variável.

![Mapa de memória do ESP32 com a faixa de periféricos e o registrador de GPIO](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mapa_memoria_esp32.png)

*Figura L2-A — Reveja o mapa da teoria: o registrador que seu código escreveu hoje mora na
faixa de periféricos. Nenhuma mágica — um endereço como outro qualquer, só que com pinos
atrás.*

**B.3 Experimento — atomicidade**: no código, troque as duas escritas por uma versão
lê-modifica-escreve usando `GPIO_OUT_REG` (`*out |= BIT_LED;` / `*out &= ~BIT_LED;`).
Funciona igual? Sim — *por enquanto*. Guarde no relatório a resposta a: "que vantagem o
W1TS/W1TC oferece quando houver uma ISR mexendo em OUTRO pino do mesmo registrador?"
(semana 6 confirmará: a versão `|=` lê, modifica e escreve em três passos — e uma
interrupção no meio pode perder a escrita; o W1TS faz tudo numa única escrita indivisível).

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

## Entrega (GitHub da dupla, `lab-02/relatorio.md`)

1. Foto da montagem + print do monitor serial.
2. Tabela `Total sizes` (DRAM/IRAM/Flash) do seu build e os **três endereços** de gravação
   (bootloader/partições/app) — com uma frase explicando o que é gravado em `0x10000`.
3. Tabela de medições da Parte C preenchida + a corrente calculada.
4. Resposta da Parte B.3 (atomicidade W1TS/W1TC), 3–5 linhas.

## Desafio (opcional)

Sem usar `gpio_set_direction`, configure a direção do GPIO 2 escrevendo no registrador
`GPIO_ENABLE_W1TS_REG` (dica: `soc/gpio_reg.h`). Confirme no monitor que o pisca continua e
explique no relatório o paralelo com o par OUT/ENABLE.
