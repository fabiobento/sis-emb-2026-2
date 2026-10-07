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

> **Observe**:
>
> o produtor usa `vTaskDelayUntil` (semana 5) para cravar os 100 Hz, não
`vTaskDelay` — se a taxa de produção derivasse, toda a conta de dimensionamento abaixo
ficaria errada. `xQueueSend(..., 0)` com timeout **0** é a escolha certa aqui: o produtor
nunca deve esperar a fila abrir espaço (desacoplamento temporal, teoria seção 2.3); se
não coube, a perda é **detectada e logada**.

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
    do repouso em nível alto. Nenhum fio novo é necessário se seu protoboard ainda está
    montado da Semana 4 (o LED da figura abaixo não é usado nesta parte — só o botão).

![Circuito do Lab 4 reaproveitado na Parte C: botão entre GPIO4 e GND com pull-up interno; o LED e o resistor da imagem pertencem ao Lab 4 e ficam de fora desta parte](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab-04.png)

   Não tem o botão físico em mãos agora? Monte este mesmo circuito em simulação: crie um
   projeto ESP32 (ESP-IDF) em [wokwi.com](https://wokwi.com) e substitua o `diagram.json`
   do projeto por este:

```json
{
  "version": 1,
  "author": "sua-bancada",
  "editor": "wokwi",
  "parts": [
    {
      "type": "board-esp32-devkit-c-v4",
      "id": "esp",
      "top": -38.4,
      "left": -33.56,
      "attrs": { "builder": "esp-idf" }
    },
    {
      "type": "wokwi-pushbutton-6mm",
      "id": "btn1",
      "top": 30.6,
      "left": 131.2,
      "rotate": 270,
      "attrs": { "color": "blue", "xray": "1" }
    }
  ],
  "connections": [
    [ "esp:TX", "$serialMonitor:RX", "", [] ],
    [ "esp:RX", "$serialMonitor:TX", "", [] ],
    [ "btn1:2.l", "esp:GND.2", "green", [ "h38.8", "v-67.2" ] ],
    [ "btn1:1.l", "esp:4", "green", [ "h-48", "v57.6", "h-19.2" ] ]
  ],
  "dependencies": {}
}
```

   Mesma lógica do botão da Semana 3 (Parte B) — só troca de pino: aqui vai no **GPIO4**,
   não no GPIO0/BOOT (regra da Semana 3/4). Não precisa de resistor: o código habilita o
   pull-up interno.

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

![Linha do tempo do que acontece entre a borda no pino e a primeira linha da sua ISR: evento, sincronização do sinal, salvamento de contexto, despacho para o vetor de interrupção, seu código, restauração de contexto e retomada do programa](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/latencia_interrupcao.png)

*Figura — Antes mesmo da primeira linha de `isr_botao` executar, o hardware já gastou um
tempo fixo (sincronização do sinal, salvamento de registradores, despacho para o vetor).
É por isso que a regra de ouro é manter **sua parte** (o bloco azul) a menor possível — foi
exatamente o que você fez ao tirar todo o trabalho pesado da ISR e deixar só o `give`.*

21. Compile, grave e abra o monitor:
```bash
cd ~/sis-emb/lab6/isr_sem/isr_sem
idf.py -p /dev/ttyUSB0 flash monitor
```

   Sem o botão físico em mãos? Use o mesmo projeto Wokwi montado no item 17.

22. Pressione o botão algumas vezes e observe a latência impressa. Compare com a do Lab 4
    (onde a tarefa fazia polling da flag a cada `vTaskDelay`): o semáforo deve derrubá-la de
    "até o período do laço" para **dezenas de µs** — a tarefa acorda *no ato*, cortesia do
    `portYIELD_FROM_ISR`.

![Comparação temporal entre polling e interrupção: no polling a tarefa só percebe o evento na próxima verificação periódica, enquanto a interrupção atende no mesmo instante em que o evento ocorre](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/polling_vs_interrupcao.png)

*Figura — É exatamente esta diferença que você acabou de medir na prática: no Lab 4
(polling) o evento espera a próxima verificação do laço; aqui (interrupção) a ISR já
dispara no mesmo instante do evento, e a tarefa acorda em seguida, assim que o `give`
acontece — sem esperar sua "vez" num laço de `vTaskDelay`.*

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

1. (CANCELADO)
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
