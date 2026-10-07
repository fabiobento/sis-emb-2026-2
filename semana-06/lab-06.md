# Lab 6 — Vendo a corrida acontecer (e consertando com mutex, fila e semáforo)

> **Antes de começar**: leia a [teoria-06](teoria-06.md) — especialmente o Exemplo 6.1 (a
> tabela do interleaving), as Figuras 6-A a 6-D e o mapa mental de decisão do final. Hoje
> você reproduzirá em bancada o bug mais traiçoeiro do firmware — e o matará com as
> ferramentas certas: mutex, fila e semáforo.

**Objetivo**: **provocar e medir** uma condição de corrida real; consertá-la com mutex;
montar o padrão produtor–consumidor com fila; dimensionar a fila e vê-la estourar quando o
consumidor "apaga"; e substituir o polling do botão por um semáforo, medindo a latência.

**Duração**: 2 aulas.
**Material**: apenas o ESP32 para as Partes A e B (funcionam 100 % no Wokwi). Para a Parte C
você vai precisar do **mesmo circuito do Lab 4** (LED + botão em GPIO4, pull-up interno) —
se seu botão ainda está montado da Semana 4, não precisa remontar nada.

> ⚠️ **GPIO0 continua proibido para botão.** Como nas Semanas 3 e 4: GPIO0 é o pino de boot
> do ESP32, e um botão pendurado nele arrisca travar a placa em modo de gravação se for
> pressionado durante um reset. Na Parte C deste lab o botão vai no **GPIO4**, exatamente
> como no Lab 4 — se você montou com GPIO0 em alguma versão antiga deste material, é hora de
> corrigir o fio.

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — A corrida (35 min)

O firmware desta parte cria **duas tarefas idênticas** que incrementam o mesmo contador
global 1 000 000 de vezes cada (releia o Exemplo resolvido 6.1: o `g++` que são três
instruções — LOAD, ADD, STORE — e a Figura 6-A com o interleaving fatal). O resultado
*deveria* ser 2 000 000.

1. Abra o seu diretório de trabalho com o VS Code:
```bash
code ~/sis-emb
```

2. Dentro do VS Code, abra o terminal integrado, crie e acesse o diretório do lab 06:
```bash
mkdir ~/sis-emb/lab6
cd ~/sis-emb/lab6
idf.py create-project corrida
cd corrida
```

3. Ainda no terminal integrado, copie o firmware do repositório para o seu diretório de
   projeto:
```bash
cp ~/sis-emb-2026-2/semana-06/src/corrida_mutex/main.c ~/sis-emb/lab6/corrida/main/corrida.c
```

4. Antes de gravar, abra no VS Code o programa `~/sis-emb/lab6/corrida/main/corrida.c`
   **com a teoria do lado** (a [seção 2.1](https://github.com/fabiobento/sis-emb-2026-2/blob/main/semana-06/teoria-06.md#21-mutex-exclus%C3%A3o-m%C3%BAtua) detalha, linha a linha, o `take`/`give` do mutex).
   Código-fonte completo, para referência:

```c
// Semana 6A — condição de corrida e correção com mutex
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include "esp_random.h"
#include <stdio.h>

#define USAR_MUTEX 0
#define N 1000000

static volatile uint32_t g_contador = 0;
static SemaphoreHandle_t g_mutex;

// Em hardware real (240 MHz), a janela de risco entre o LOAD e o STORE de "g_contador++"
// dura poucos nanossegundos — curta demais para o escalonador "acertar" com frequência
// em 1 milhão de tentativas. Esta função alarga essa janela de propósito (só para tornar
// a corrida observável neste experimento), sem mudar a natureza do bug: continua sendo
// leitura-modificação-escrita não atômica, só que agora com tempo de sobra para colidir.
//
// O tamanho da pausa é sorteado a cada chamada com esp_random() (gerador de números
// aleatórios por hardware do ESP32). Sem isso, a pausa fixa colide sempre no MESMO ponto
// relativo entre T1 e T2 a cada reset — sem Wi-Fi/Bluetooth/botão rodando, não há nenhum
// evento assíncrono do mundo real para desalinhar o encontro, e o resultado final sai
// idêntico execução após execução (um determinismo "escondido" dentro do próprio
// hardware, não só no simulador). Sorteando a pausa, o ponto exato da colisão muda a cada
// execução de verdade.
static inline void amplia_janela_de_risco(void)
{
    int n = 10 + (esp_random() % 40);     // entre 10 e 49 iterações, sorteado agora
    for (volatile int k = 0; k < n; k++) { }
}

static void incrementador(void *arg)
{
    for (int i = 0; i < N; i++) {
#if USAR_MUTEX
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        uint32_t tmp = g_contador;      // LOAD
        amplia_janela_de_risco();
        g_contador = tmp + 1;           // STORE
        xSemaphoreGive(g_mutex);
#else
        uint32_t tmp = g_contador;      // LOAD
        amplia_janela_de_risco();       // leitura-modificação-escrita NÃO atômica
        g_contador = tmp + 1;           // STORE
#endif
    }
    printf("tarefa %s terminou; contador=%lu\n",
           (char *)arg, (unsigned long)g_contador);
    vTaskDelete(NULL);               // a tarefa termina sozinha ao sair do for
}

void app_main(void)
{
    // T1 e T2 ficam no mesmo núcleo, mesma prioridade, e (com USAR_MUTEX 1) fazem
    // 2 milhões de Take/Give SEM nunca ceder a CPU — a IDLE1 daquele núcleo fica sem
    // rodar tempo suficiente para alimentar o Task Watchdog (timeout padrão 5 s), e o
    // sistema reiniciaria antes de você ler o resultado. Provocar o watchdog não é o
    // assunto desta parte (isso já foi coberto nos Labs 4 e 5) — desligamos aqui de
    // propósito, só para esta medição.
    esp_task_wdt_deinit();

    g_mutex = xSemaphoreCreateMutex();
    // mesmo núcleo p/ maximizar preempções visíveis
    xTaskCreatePinnedToCore(incrementador, "T1", 2048, "T1", 3, NULL, 1);
    xTaskCreatePinnedToCore(incrementador, "T2", 2048, "T2", 3, NULL, 1);
}
```

   Repare quatro detalhes que fazem toda a diferença no experimento:
   - `#if USAR_MUTEX` **compila dois firmwares diferentes** a partir do mesmo arquivo — nada
     de comentar/descomentar código manualmente, só trocar o `0`/`1` no topo;
     `g_contador` é `volatile`, mas isso só garante que a leitura vá à memória (semana 3) —
     não protege o incremento de ser interrompido no meio, que é exatamente o bug que você
     vai ver. `amplia_janela_de_risco()` troca o que seria um único `g_contador++;` por um
     LOAD, um atraso artificial (de duração **sorteada** com `esp_random()`) e um STORE
     separados: em hardware real a janela de risco de um `++` puro dura poucos
     nanossegundos — curta demais para o escalonador (que troca de tarefa a cada 10 ms, por
     padrão) "acertar" o meio dela com frequência em 1 milhão de tentativas. Alargar a
     janela não inventa um bug novo: é a mesma leitura-modificação-escrita não atômica da
     teoria, só que com tempo de sobra para a colisão acontecer de forma confiável *neste*
     experimento. E por que sortear o tamanho em vez de usar um valor fixo? Porque sem
     Wi-Fi, Bluetooth ou botão rodando, não existe nenhum evento assíncrono "do mundo real"
     para desalinhar o encontro entre T1 e T2 — com uma pausa fixa, a colisão cai sempre no
     mesmo ponto relativo a cada reset, e o resultado final sairia **idêntico a cada
     execução** (o mesmo determinismo que você já viu no Wokwi, só que agora escondido
     dentro do próprio hardware). Sorteando a pausa, o ponto exato da colisão muda a cada
     execução de verdade, e os valores finais voltam a variar como a teoria prevê;
   - as duas tarefas são **pinadas no mesmo núcleo** (`xTaskCreatePinnedToCore(..., 1)`) de
     propósito: queremos maximizar preempções entre elas. Com uma em cada núcleo o problema
     seria *pior* ainda, mas de outra natureza — aí as escritas aconteceriam
     **simultaneamente de verdade**, não só intercaladas, um passo de cada vez;
   - cada tarefa chama `vTaskDelete(NULL)` ao terminar o `for` — sem isso, a função retornaria
     e a tarefa ficaria num estado indefinido (lembre da semana 5: uma tarefa **nunca
     retorna**, ou termina num laço infinito, ou se autodeleta);
   - `esp_task_wdt_deinit()` é chamado **antes** de criar as tarefas, e só existe por causa do
     custo do mutex: 2 milhões de `Take`/`Give` a ~1–2 µs cada passam dos 5 s de timeout
     padrão do watchdog, e como T1/T2 nunca cedem a CPU nesse laço, a `IDLE1` do núcleo onde
     elas rodam fica sem chance de "alimentar" o watchdog. Sem essa linha, o firmware com
     `USAR_MUTEX 1` reiniciaria sozinho antes de você conseguir ler o resultado — não é um bug
     do seu código, é esperado dado o volume de operações deste experimento específico.

5. Compile, grave o firmware na placa e abra o monitor serial em um único comando:
```bash
cd ~/sis-emb/lab6/corrida
idf.py -p /dev/ttyUSB0 flash monitor
```
   Não tem o ESP32 em mãos agora? Sem problema — esta parte roda **100 % em simulação**:
   cole o conteúdo de `corrida.c` num novo projeto ESP32 (ESP-IDF) no
   [wokwi.com](https://wokwi.com) (veja `docs/instalacao.md`, seção 3) e rode a simulação em
   vez do `flash monitor` acima — o resto do roteiro funciona igual, só o tempo de execução
   muda um pouco (simulação costuma ser mais lenta que hardware real).

6. Confirme no topo do arquivo: `#define USAR_MUTEX 0` (proteção desligada) — é o estado em
   que você acabou de copiar o firmware, então só confira.

7. Grave e anote o valor final impresso pela última tarefa a terminar. Rode **5 vezes**
   (basta resetar a placa com o botão EN) e preencha:

| execução | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| contador final | | | | | |

8. O que você deve observar: valores **diferentes a cada execução**, todos < 2 000 000 —
   incrementos evaporaram, e a quantidade evaporada depende de *quando* o escalonador
   preemptou. Este é o bug intermitente da teoria (seção 1.2), reproduzido em bancada.

> 🖥️ **Rodando no Wokwi em vez de hardware real?** É esperado ver o **mesmo valor se
> repetindo** em todas as 5 execuções, em vez de variar. O simulador reproduz o escalonamento
> de forma determinística (sem o jitter elétrico de um chip físico), então a mesma disputa
> entre T1 e T2 tende a se resolver sempre da mesma forma. O valor abaixo de 2 000 000
> continua provando a corrida (os incrementos somem do mesmo jeito); só a *variação* entre
> execuções é um fenômeno mais fácil de observar em hardware real do que em simulação — se
> tiver um ESP32 físico disponível, vale repetir o teste lá para ver os números mudarem.

> **Observação:** se por acaso uma execução der exatamente 2 000 000, rode de novo —
> corrida é probabilística. É exatamente por isso que ela passa nos testes e explode em
> campo.

9. Agora `#define USAR_MUTEX 1`, regrave e repita as 5 execuções. Esperado: **2 000 000
   cravados, sempre**. Anote também o tempo total (compare o carimbo de tempo do monitor):
   quanto o mutex custou em desempenho? (Take/give ~1–2 µs × 2 000 000 = alguns segundos a
   mais — proteção não é grátis; por isso a seção crítica deve ser curta. E note: o custo
   é *previsível*, ao contrário do bug, que era *aleatório*. Engenharia prefere custo
   conhecido a risco desconhecido.)

![Linha do tempo mostrando duas tarefas nunca executando a seção crítica ao mesmo tempo, graças ao mutex](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mutex_serializacao.png)

*Figura — Compare o que você acabou de medir com a Figura 6-C da teoria: com
`USAR_MUTEX 1`, T2 fica bloqueada (0 % de CPU) sempre que T1 está dentro da seção crítica —
é essa não-sobreposição, e não "sorte", que garante os 2 000 000 cravados do item 9.*

## Parte B — Produtor–consumidor com fila (40 min)

10. No terminal integrado, crie e acesse o diretório deste experimento:
```bash
mkdir ~/sis-emb/lab6/fila
cd ~/sis-emb/lab6/fila
idf.py create-project fila
cd fila
```

11. Copie o firmware do repositório:
```bash
cp ~/sis-emb-2026-2/semana-06/src/fila_prod_cons/main.c ~/sis-emb/lab6/fila/fila/main/fila.c
```

12. Abra o arquivo copiado **com a teoria do lado** (seção 2.3 — produtor, consumidor e o
    dimensionamento do Exemplo 6.2). Código-fonte completo:

```c
// Semana 6B — produtor-consumidor com fila (dimensionamento do Exemplo 6.2)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>

static QueueHandle_t g_fila;

static void produtor(void *arg)          // 100 Hz
{
    TickType_t prox = xTaskGetTickCount();
    uint32_t amostra = 0;
    while (1) {
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(10));
        if (xQueueSend(g_fila, &amostra, 0) != pdTRUE)
            printf("FILA CHEIA! amostra %lu perdida\n", (unsigned long)amostra);
        amostra++;
    }
}

static void consumidor(void *arg)        // processa em rajadas (dorme 300 ms)
{
    uint32_t v; UBaseType_t max_ocup = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(300));  // simula ficar "ocupado" em outra coisa
        UBaseType_t ocup = uxQueueMessagesWaiting(g_fila);
        if (ocup > max_ocup) max_ocup = ocup;
        while (xQueueReceive(g_fila, &v, 0) == pdTRUE) { /* processa v */ }
        printf("rajada consumida; ocupacao antes=%u (max=%u)\n",
               (unsigned)ocup, (unsigned)max_ocup);
    }
}

void app_main(void)
{
    g_fila = xQueueCreate(64, sizeof(uint32_t));   // 64 itens (Exemplo 6.2)
    xTaskCreate(produtor,   "prod", 2048, NULL, 4, NULL);
    xTaskCreate(consumidor, "cons", 2048, NULL, 3, NULL);
}
```

    Repare: o produtor usa `vTaskDelayUntil` (semana 5) para cravar os 100 Hz, não
    `vTaskDelay` — se a taxa de produção derivasse, toda a conta de dimensionamento abaixo
    ficaria errada. `xQueueSend(..., 0)` com timeout **0** é a escolha certa aqui: o produtor
    nunca deve esperar a fila abrir espaço (desacoplamento temporal, teoria seção 2.3); se
    não coube, a perda é **detectada e logada**, nunca engolida em silêncio.

13. Compile, grave e abra o monitor:
```bash
cd ~/sis-emb/lab6/fila/fila
idf.py -p /dev/ttyUSB0 flash monitor
```
    Também roda 100 % no Wokwi, do mesmo jeito da Parte A (nenhum componente externo é
    necessário — é só lógica interna da ESP32).

14. Observe o monitor por ~30 s e responda com números:
    - Quantos itens o consumidor drena por rajada, tipicamente? (Esperado ≈ 100 Hz × 0,3 s =
      **30**.)
    - Alguma mensagem `FILA CHEIA!` apareceu? (Não deveria: 30 < 64.)

![Nível da fila ao longo do tempo em forma de serrote, com painel mostrando o apagão de 300 ms dentro da capacidade e o de 700 ms estourando](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fila_nivel_tempo.png)

*Figura — O que você está medindo no item 14 é o painel esquerdo desta figura (apagão de
300 ms): o nível sobe em rampa e zera antes de chegar perto da capacidade de 64. No próximo
item você vai reproduzir o painel direito.*

15. **Verifique o Exemplo 6.2 na prática**: no código, troque `vTaskDelay(pdMS_TO_TICKS(300))`
    por `vTaskDelay(pdMS_TO_TICKS(700))` (aumente o "apagão" do consumidor) e regrave. Agora
    a produção por apagão (70) supera a capacidade (64): o monitor deve mostrar perdas
    detectadas:

```
FILA CHEIA! amostra 4471 perdida
```

    Conte quantas perdas por rajada (esperado ≈ 70 − 64 = 6, variando ±1). A conta da teoria
    bateu com a bancada?

16. Corrija **sem** mudar o consumidor: qual capacidade de fila suporta o apagão de 700 ms
    com a folga de 2× da regra do Exemplo 6.2? (70 × 2 = 140 → potência de 2 seguinte:
    **256** itens.) Ajuste `xQueueCreate(64, ...)` para `xQueueCreate(256, ...)`, regrave e
    comprove o fim das perdas. Registre a conta no relatório.

## Parte C — ISR→tarefa com semáforo binário (30 min)

Hora de aposentar de vez o polling do botão: o padrão profissional do Exemplo resolvido 6.3.

17. **Circuito**: é o mesmo do Lab 4 — botão entre **GPIO4** e GND, pull-up interno cuidando
    do repouso em nível alto (veja a figura do circuito em `semana-04/lab-04.md`, se precisar
    remontar). Nenhum fio novo é necessário se seu protoboard ainda está montado da Semana 4.

18. No terminal integrado, crie e acesse o diretório deste experimento:
```bash
mkdir ~/sis-emb/lab6/isr_sem
cd ~/sis-emb/lab6/isr_sem
idf.py create-project isr_sem
cd isr_sem
```

19. Copie o firmware do repositório:
```bash
cp ~/sis-emb-2026-2/semana-06/src/isr_sem/main.c ~/sis-emb/lab6/isr_sem/isr_sem/main/isr_sem.c
```

20. Abra o arquivo copiado **com a teoria do lado** (seção 2.4 — o padrão ISR→tarefa linha a
    linha). Código-fonte completo:

```c
// Semana 6C — ISR -> tarefa com semaforo (binario ou contador)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>

#define BTN GPIO_NUM_4     // NUNCA GPIO0 com botão (pino de boot - ver Semana 3, Parte D)
#define DEBOUNCE_US 20000  // 20 ms - mesmo valor validado no Lab 3, Parte C

// item 23 do roteiro: troque para 1 e regrave para comparar binário x contador
#define USAR_CONTADOR 0

static SemaphoreHandle_t sem;
static volatile int64_t s_t_isr = 0;          // carimbo da borda, lido pela tarefa
static int64_t s_ultimo_evento_us = 0;        // debounce, dentro da própria ISR

static void IRAM_ATTR isr_botao(void *arg)
{
    int64_t agora = esp_timer_get_time();
    if (agora - s_ultimo_evento_us <= DEBOUNCE_US) {
        return;                               // bounce: ignora, não conta, não dá
    }
    s_ultimo_evento_us = agora;
    s_t_isr = agora;

    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(sem, &woken);        // ~1 µs: só sinaliza "aconteceu"
    portYIELD_FROM_ISR(woken);                 // troca de contexto JÁ, se preciso
}

static void tarefa_botao(void *arg)
{
    uint32_t eventos = 0;
    while (1) {
        xSemaphoreTake(sem, portMAX_DELAY);    // dorme sem gastar CPU até o give
        int64_t latencia_us = esp_timer_get_time() - s_t_isr;
        printf("evento #%lu | latencia ISR->tarefa: %lld us\n",
               (unsigned long)(++eventos), latencia_us);
    }
}

void app_main(void)
{
    // O semáforo tem que existir ANTES de a ISR poder disparar: gpio_isr_handler_add() já
    // arma a interrupção na hora. Se o pino "piscar" um único ciclo enquanto o pull-up ainda
    // está assentando (comum — não é preciso apertar o botão), a ISR roda imediatamente e
    // chama xSemaphoreGiveFromISR(sem, ...) com sem == NULL — configASSERT do FreeRTOS,
    // sempre na mesma linha, sempre no boot. Por isso a ordem abaixo é obrigatória.
#if USAR_CONTADOR
    sem = xSemaphoreCreateCounting(10, 0);        // até 10 eventos pendentes, começa em 0
#else
    sem = xSemaphoreCreateBinary();               // satura em 1: rajada rápida "come" eventos
#endif

    gpio_reset_pin(BTN);
    gpio_set_direction(BTN, GPIO_MODE_INPUT);
    gpio_pullup_en(BTN);
    gpio_set_intr_type(BTN, GPIO_INTR_NEGEDGE);   // borda de descida (ativo-baixo)
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN, isr_botao, NULL);   // a partir daqui a ISR pode disparar

    xTaskCreate(tarefa_botao, "botao", 2048, NULL, 4, NULL);
}
```
> **Observe a ordem**:
> 
> o semáforo é criado **antes** de instalar a ISR. Se fosse o contrário, a
ISR poderia disparar (um simples ruído no pino enquanto o pull-up assenta já basta) e
chamar `xSemaphoreGiveFromISR` sobre um `sem` ainda `NULL` — é exatamente o tipo de bug
que gera um `assert failed` reproduzível em todo boot, sem precisar apertar nada. E note
o debounce: ele acontece **dentro da própria ISR**, como no desafio da Semana 4 —
só aritmética de carimbo de tempo (`esp_timer_get_time()`), nada de `vTaskDelay`. É o
único "trabalho" que a ISR faz além do `give`; tudo o mais (imprimir, contar eventos)
fica na tarefa, que pode gastar o tempo que precisar porque não está mais em contexto de
interrupção.

21. Compile, grave e abra o monitor:
```bash
cd ~/sis-emb/lab6/isr_sem/isr_sem
idf.py -p /dev/ttyUSB0 flash monitor
```

Precisa do botão físico para esta parte (ou simule com um `wokwi-pushbutton` ligado ao
GPIO4 num novo projeto ESP32 no Wokwi — mesma configuração de pino do circuito do Lab 4).

22. Pressione o botão algumas vezes e observe a latência impressa. Compare com a do Lab 4
    (onde a tarefa fazia polling da flag a cada `vTaskDelay`): o semáforo deve derrubá-la de
    "até o período do laço" para **dezenas de µs** — a tarefa acorda *no ato*, cortesia do
    `portYIELD_FROM_ISR`.

![Gráfico de barras comparando a latência típica e máxima entre polling no Lab 4 e semáforo no Lab 6, em escala logarítmica](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab06_latencia_isr_semaforo.png)

*Figura — Ordem de grandeza esperada: no Lab 4 a tarefa só percebe o evento no próximo
`vTaskDelay(10 ms)`, então a latência de pior caso é o próprio período do laço; aqui a
tarefa acorda assim que a ISR dá o semáforo. Preencha a tabela abaixo com os valores que
**você mediu**, não com os desta figura — eles são só uma referência de ordem de grandeza.*

| método | latência média | latência máx |
|---|---|---|
| Lab 4: flag + polling da tarefa | | |
| Lab 6: semáforo + take bloqueante | | |

23. **Experimento do contador**: pressione o botão 5× *muito* rápido (< 20 ms entre bordas
    — difícil, o bouncing ajuda!). Com `USAR_CONTADOR 0` (semáforo **binário**), gives em
    rajada saturam em 1: alguns eventos "somem" — a tarefa só vê um `xSemaphoreTake` acordar
    por vez, mesmo que a ISR tenha dado várias vezes seguidas. Troque para
    `#define USAR_CONTADOR 1`, regrave e repita: agora cada `give` é contado
    (`xSemaphoreCreateCounting(10, 0)`), e os cinco eventos aparecem. Explique a diferença em
    2 linhas (teoria, seção 2.4, último parágrafo).

> **Onde esse padrão reaparece**: na ISR do ADC com DMA, na recepção de CAN (semana 10)
> e no callback de dados MQTT (semana 14) — sempre "interrupção sinaliza, tarefa processa".
> Você acabou de aprender a estrutura de todo driver profissional.

---

## Entrega (GitHub da bancada, `lab-06/relatorio.md`)

1. Tabela da Parte A (5 execuções sem mutex + 5 com, itens 7 e 9) + 3 linhas: por que os
   valores sem mutex variam e por que com mutex não.
2. Números da Parte B (itens por rajada do item 14, perdas com 700 ms do item 15) + a conta
   e o novo tamanho da fila do item 16.
3. Tabela de latências do item 22 + código da sua ISR e da tarefa (só os dois blocos).
4. Resposta do item 23 (binário × contador).
5. Uma frase honesta: qual primitiva você usaria para proteger o barramento I2C que duas
   tarefas compartilharão na semana 9 — e por que não um semáforo binário? (Dica: Mars
   Pathfinder.)

## Desafio (opcional)

Deadlock didático: crie os mutexes `mA` e `mB` e duas tarefas — T1 toma `mA`, dorme 100 ms,
toma `mB`; T2 toma `mB`, dorme 100 ms, toma `mA`. Rode, observe o congelamento (e o
task_wdt eventual), e então conserte **apenas reordenando** as aquisições. Relate o
antes/depois — você acabou de demonstrar a regra da ordem global de aquisição.
