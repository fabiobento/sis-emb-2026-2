# Lab 5 — Tarefas, prioridades, deriva de período e dual-core

> **Antes de começar**: leia a [teoria-05](teoria-05.md). As Figuras 5-A (preempção) e 5-C
> (deriva) são exatamente os dois fenômenos que você vai medir hoje — com números, não com
> fé.

**Objetivo**: criar múltiplas tarefas e **medir** o comportamento do escalonador:
intercalação por prioridade, starvation, deriva `vTaskDelay` × `vTaskDelayUntil`, pilha e
paralelismo real nos dois núcleos.

**Duração**: 2 aulas.
**Material**: apenas o ESP32 — este lab funciona **100 % no Wokwi** (ótimo para terminar em
casa).

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — Três tarefas, um escalonador (25 min)

1. Abra o seu diretório de trabalho com o VS Code:
```bash
code ~/sis-emb
```

2. Dentro do VS Code, abra o terminal integrado, crie e acesse o diretório do lab 05:
```bash
mkdir ~/sis-emb/lab5
cd ~/sis-emb/lab5
idf.py create-project tarefas
cd tarefas
```

3. Ainda no terminal integrado, copie o firmware do repositório do lab 05 para o seu
   diretório de projeto:
```bash
cp ~/sis-emb-2026-2/semana-05/src/tarefas/main.c ~/sis-emb/lab5/tarefas/main/tarefas.c
```

4. Antes de gravar, abra no VS Code o programa
   `~/sis-emb/lab5/tarefas/main/tarefas.c` **com a teoria do lado** (a seção 2.3 detalha,
   linha a linha, como as três instâncias da mesma função viram tarefas independentes).
   Repare: as tarefas A (prio 5), B (prio 3) e C (prio 1) rodam o mesmo código — só o que
   muda é o parâmetro de criação. O que, exatamente, faz cada uma se comportar diferente?

5. Compile, grave o firmware na placa e abra o monitor serial em um único comando:
```bash
cd ~/sis-emb/lab5/tarefas
idf.py -p /dev/ttyUSB0 flash monitor
```
   Não tem o ESP32 em mãos agora? Sem problema — como diz o material deste lab, ele roda
   **100 % em simulação**: cole o conteúdo de `tarefas.c` num novo projeto ESP32 (ESP-IDF)
   no [wokwi.com](https://wokwi.com) (veja `docs/instalacao.md`, seção 3), ou abra [esse projeto de referência](https://wokwi.com/projects/475867733789639681), e rode a
   simulação em vez do `flash monitor` acima — o resto do roteiro funciona igual.

6. O comportamento esperado é cada tarefa imprimindo seu período real medido:

```
[A] core=0  periodo=500.0 ms
[B] core=0  periodo=500.1 ms
[C] core=0  periodo=499.9 ms
```

7. Observe por ~30 s: as três rodam "juntas" mesmo com prioridades diferentes. Por quê?
   (Resposta esperada: cada uma dorme 499 ms a cada 500 — a CPU está ociosa ~99,9 % do
   tempo; prioridade só decide **disputas**, e quase não há disputa. É como três pessoas
   num corredor de 10 metros de largura: a "prioridade de passagem" só importa quando duas
   chegam à porta juntas.) Anote — o contraste com a Parte B é o ponto do lab.

## Parte B — Starvation(25 min)

8. Descomente a função `cpu_bound` e a linha que a cria — mas **troque** o núcleo para 0 e
   a prioridade para 6. Código-fonte completo (as tarefas A/B/C são as mesmas da Parte A;
   `cpu_bound` e a linha extra em `app_main` são novas):

```c
// Semana 5 — Parte B, item 8: starvation ao vivo (HOG monopoliza o core 0)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdio.h>

static void tarefa(void *arg)
{
    const char *nome = (const char *)arg;
    TickType_t proximo = xTaskGetTickCount();
    int64_t t_ant = esp_timer_get_time();
    while (1) {
        vTaskDelayUntil(&proximo, pdMS_TO_TICKS(500));
        int64_t t = esp_timer_get_time();
        printf("[%s] core=%d  periodo=%.1f ms\n",
               nome, xPortGetCoreID(), (t - t_ant) / 1000.0);
        t_ant = t;
    }
}

// item 8 — tarefa gulosa: nunca bloqueia, nunca cede a CPU de propósito
static void cpu_bound(void *arg)
{
    volatile uint32_t x = 0;
    while (1) { x++; }        // sem delay: monopoliza o núcleo
}

void app_main(void)
{
    xTaskCreate(tarefa, "A", 2048, "A", 5, NULL);
    xTaskCreate(tarefa, "B", 2048, "B", 3, NULL);
    xTaskCreate(tarefa, "C", 2048, "C", 1, NULL);

    xTaskCreatePinnedToCore(cpu_bound, "HOG", 2048, NULL, 6, NULL, 0);   // item 8
}
```

9. Regrave e observe o monitor por 20 s. O que acontece com A, B e C? E que mensagem
   aparece (~5 s depois)? Registre a saída — você deve reconhecer o `task_wdt`/IDLE0 do
   Lab 4, agora com nome de crime: **starvation** por uma tarefa CPU-bound de prioridade
   máxima (teoria, seção 2.1, "regra de convivência"). Como o HOG tem prioridade 6 e nunca
   bloqueia, ele **sempre** é a tarefa pronta mais prioritária do core 0 — A, B, C e a IDLE
   simplesmente nunca rodam. É a Figura 5-A da teoria com um vilão permanente.

![Linha do tempo do escalonamento preemptivo por prioridade](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/escalonamento_preemptivo.png)

*Figura 5-A — Preempção por prioridade: a tarefa de maior prioridade toma a CPU no instante
em que fica pronta; as de menor prioridade usam as sobras. Leia da esquerda para a direita
acompanhando quem está “dentro” da CPU.*

10. Abaixe a prioridade do HOG para **1** (igual à de C) e regrave. Única mudança no código
    acima é o parâmetro de prioridade:
```c
xTaskCreatePinnedToCore(cpu_bound, "HOG", 2048, NULL, 1, NULL, 0);   // item 10
```
A, B voltam ao normal? E C — roda sempre, às vezes, nunca? (Dica: mesma prioridade ⇒
*time slicing* por tick — o escalonador reveza C e HOG a cada 10 ms, então C roda "na
metade do tempo" e com período dobrado. Explique com a teoria em ≤ 3 linhas.)

## Parte C — Deriva de período: Exemplo 5.1 ao vivo (30 min)

11. Crie uma 4ª tarefa `D` (prio 4) com **corpo lento e `vTaskDelay`** — a receita da
   deriva. Código-fonte completo (as tarefas A/B/C são as mesmas da Parte A; só `tarefa_d`
   e a linha extra em `app_main` são novas):

```c
// Semana 5 — Parte C, item 11: deriva de período (versão com vTaskDelay)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdio.h>

static void tarefa(void *arg)
{
    const char *nome = (const char *)arg;
    TickType_t proximo = xTaskGetTickCount();
    int64_t t_ant = esp_timer_get_time();
    while (1) {
        vTaskDelayUntil(&proximo, pdMS_TO_TICKS(500));
        int64_t t = esp_timer_get_time();
        printf("[%s] core=%d  periodo=%.1f ms\n",
               nome, xPortGetCoreID(), (t - t_ant) / 1000.0);
        t_ant = t;
    }
}

// item 11 — corpo "lento" (50 ms de trabalho ocupado) + vTaskDelay (sono RELATIVO
// ao instante em que é chamado) — a receita da deriva.
static void tarefa_d(void *arg)
{
    int64_t t_ant = esp_timer_get_time();
    while (1) {
        int64_t fim = esp_timer_get_time() + 50000;      // "trabalho" de 50 ms
        while (esp_timer_get_time() < fim) { }            // (ocupado de propósito)
        vTaskDelay(pdMS_TO_TICKS(200));                    // dorme 200 ms A PARTIR DE AGORA
        int64_t t = esp_timer_get_time();
        printf("[D] periodo=%.1f ms\n", (t - t_ant) / 1000.0);
        t_ant = t;
    }
}

void app_main(void)
{
    xTaskCreate(tarefa, "A", 2048, "A", 5, NULL);
    xTaskCreate(tarefa, "B", 2048, "B", 3, NULL);
    xTaskCreate(tarefa, "C", 2048, "C", 1, NULL);

    xTaskCreate(tarefa_d, "D", 2048, NULL, 4, NULL);
}
```

12. Meça 10 períodos de D. Valor esperado: ~**250 ms**, não 200 (200 de sono + 50 de corpo)
   — a deriva do Exemplo 5.1 com outros números. Calcule: em 1 minuto, quantas ativações D
   perde em relação às 300 ideais? (60 000/250 = 240 ativações → **60 perdidas**, 20 % da
   taxa!)
13. Troque o `vTaskDelay` por `vTaskDelayUntil` (copie o padrão da tarefa A: variável
   `proximo` + chamada no **início** do laço) e meça de novo. Esperado: ~**200,0 ms**
   cravados, com o corpo de 50 ms "absorvido" dentro do período. Única mudança em relação
   ao código acima é dentro de `tarefa_d`:

```c
// item 13 — mesma tarefa D, agora com vTaskDelayUntil: o corpo de 50 ms fica
// "absorvido" dentro do período de 200 ms, em vez de somado a ele.
static void tarefa_d(void *arg)
{
    TickType_t proximo = xTaskGetTickCount();
    int64_t t_ant = esp_timer_get_time();
    while (1) {
        int64_t fim = esp_timer_get_time() + 50000;      // "trabalho" de 50 ms
        while (esp_timer_get_time() < fim) { }            // (ocupado de propósito)
        vTaskDelayUntil(&proximo, pdMS_TO_TICKS(200));     // dorme até o PRÓXIMO alvo absoluto
        int64_t t = esp_timer_get_time();
        printf("[D] periodo=%.1f ms\n", (t - t_ant) / 1000.0);
        t_ant = t;
    }
}
```

Preencha:

| Configuração | período médio (ms) | período máx (ms) |
|---|---|---|
| vTaskDelay + corpo 50 ms | | |
| vTaskDelayUntil + corpo 50 ms | | |

> **Por que isso é importante?**: na semana 7 você amostrará um sinal
> esperando taxa constante, e na semana 13 o PID calculará `K_d·(e−e_ant)/T_s` assumindo
> T_s exato. Uma deriva de 20 % no período vira 20 % de erro na derivada — invisível no
> código, devastador no resultado.

## Parte D — Pilha: medindo o high water mark (20 min)

**O que é "high water mark"**: é a maior profundidade que a pilha (stack) de uma tarefa já
atingiu desde que ela começou a rodar — não "quanto está em uso agora", e sim o **pior caso
já registrado**. É a mesma ideia da marca d'água que fica na parede de um rio depois de uma
enchente: mostra até onde a água chegou no pico, mesmo que o nível já tenha baixado. Com a
pilha é igual — ela sobe e desce o tempo todo conforme a tarefa entra e sai de funções, mas o
FreeRTOS guarda o ponto mais fundo que ela já alcançou.

A função `uxTaskGetStackHighWaterMark()` **inverte** essa lógica: em vez de devolver o quanto
já foi usado no pior momento, devolve o quanto **sobrou** de pilha nesse pior momento (a folga
que nunca chegou a ser tocada). Número alto = pilha generosa (talvez generosa demais,
desperdiçando SRAM); número baixo, perto de zero = a tarefa já encostou quase na borda da
própria pilha alguma vez — sinal de perigo, a próxima chamada um pouco mais funda pode
estourar (*stack overflow*).

> ⚠️ **Pegadinha de unidade**: o valor retornado é em **palavras**, não em bytes. No ESP32
> (32 bits), 1 palavra = 4 bytes — se o print mostrar `384`, isso são `384 × 4 = 1536` bytes
> de folga, não 384 bytes. Confira sempre antes de comparar com o tamanho que você passou em
> `xTaskCreate` (que é em bytes).

14. Na tarefa A, imprima a folga de pilha a cada ciclo. Código-fonte completo (a única
    linha nova está marcada — pode remover a tarefa D da Parte C se não quiser rodar as
    quatro juntas, o roteiro abaixo assume só A/B/C):

```c
// Semana 5 — Parte D, item 14: medindo o high water mark de pilha
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

static void tarefa(void *arg)
{
    const char *nome = (const char *)arg;
    TickType_t proximo = xTaskGetTickCount();
    int64_t t_ant = esp_timer_get_time();
    while (1) {
        vTaskDelayUntil(&proximo, pdMS_TO_TICKS(500));
        int64_t t = esp_timer_get_time();

        if (strcmp(nome, "A") == 0) {                       // item 14 — só a tarefa A
            printf("[A] pilha livre: %u palavras\n",
                   (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }

        printf("[%s] core=%d  periodo=%.1f ms\n",
               nome, xPortGetCoreID(), (t - t_ant) / 1000.0);
        t_ant = t;
    }
}

void app_main(void)
{
    xTaskCreate(tarefa, "A", 2048, "A", 5, NULL);
    xTaskCreate(tarefa, "B", 2048, "B", 3, NULL);
    xTaskCreate(tarefa, "C", 2048, "C", 1, NULL);
}
```

15. Anote o valor estabilizado. Agora **provoque**: declare na tarefa um
    `char buf[1500];` e use-o (`snprintf(buf, sizeof buf, "x"); printf("%s", buf);`). O
    valor caiu quanto? Com pilha de 2048, sobrou margem? Relacione com a receita do
    Exemplo 5.3 (uso + 50 %) e proponha o tamanho certo para esta tarefa.

> **Observação:** se exagerar no buffer (tente 3000!) você verá o crash de *stack overflow*
> com o nome da tarefa culpada — mais uma "cara de erro" para a sua coleção (junto do
> task_wdt do Lab 4 e do backtrace da ISR).

## Parte E — Dual-core (20 min)

16. Restaure o HOG com prioridade 6, mas agora **no core 1**. Código-fonte completo (as
    tarefas A/B/C são as mesmas da Parte A; `cpu_bound` volta, mas agora pinada no core 1):

```c
// Semana 5 — Parte E, item 16: dual-core (HOG isolado no core 1)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdio.h>

static void tarefa(void *arg)
{
    const char *nome = (const char *)arg;
    TickType_t proximo = xTaskGetTickCount();
    int64_t t_ant = esp_timer_get_time();
    while (1) {
        vTaskDelayUntil(&proximo, pdMS_TO_TICKS(500));
        int64_t t = esp_timer_get_time();
        printf("[%s] core=%d  periodo=%.1f ms\n",
               nome, xPortGetCoreID(), (t - t_ant) / 1000.0);
        t_ant = t;
    }
}

// item 16 — tarefa gulosa, agora isolada no core 1: não compete mais com A/B/C (core 0)
static void cpu_bound(void *arg)
{
    volatile uint32_t x = 0;
    int64_t proximo_print = esp_timer_get_time();
    while (1) {
        x++;
        // Descomente para o HOG "confessar" o núcleo sem afogar o monitor:
        // if (esp_timer_get_time() >= proximo_print) {
        //     printf("[HOG] core=%d\n", xPortGetCoreID());
        //     proximo_print = esp_timer_get_time() + 1000000;   // a cada ~1 s
        // }
    }
}

void app_main(void)
{
    xTaskCreate(tarefa, "A", 2048, "A", 5, NULL);
    xTaskCreate(tarefa, "B", 2048, "B", 3, NULL);
    xTaskCreate(tarefa, "C", 2048, "C", 1, NULL);

    xTaskCreatePinnedToCore(cpu_bound, "HOG", 2048, NULL, 6, NULL, 1);   // item 16
}
```

17. Regrave: A, B e C (core 0) devem voltar a rodar em dia **mesmo com o HOG vivo** — os
    núcleos trabalham em paralelo de verdade. Confirme pelos logs que as tarefas imprimem
    `core=0` e pelo desaparecimento do task_wdt. (Descomente o bloco de `printf` dentro de
    `cpu_bound` acima se quiser ver o HOG confessar o núcleo — ele já vem com um limitador
    de ~1 s entre prints, sem usar `vTaskDelay` dentro de um laço que não pode bloquear.)

> 💡 **A lição de arquitetura**: no ESP32, o core 0 já carrega Wi-Fi/BT e serviços do
> sistema. Cargas pesadas da sua aplicação → core 1. É a divisão de trabalho que os
> projetos finais saudáveis usam — e a resposta da questão 5 da entrega.

---

## Entrega (GitHub da bancada, `lab-05/relatorio.md`)

1. Explicação da Parte A.7 (por que prioridades diferentes convivem em CPU ociosa), ≤ 3
   linhas.
2. Saída da Parte B.9 (starvation + task_wdt) e resposta da B.10 (time slicing).
3. Tabela da Parte C preenchida + a conta de ativações perdidas (C.12).
4. Medições de pilha da Parte D e o tamanho de pilha que vocês recomendariam, com
   justificativa pelo Exemplo 5.3.
5. Evidência da Parte E (log mostrando A/B/C saudáveis com HOG no core 1) + 2 linhas: por
   que fixar cargas pesadas no core 1 é a boa prática no ESP32?

## Desafio (opcional)

Semáforo humano: crie as tarefas `verde`, `amarelo` e `vermelho` controlando três LEDs
(GPIOs 2, 4, 5) com o ciclo 5 s / 1 s / 4 s. Restrição: **sem** variáveis globais de
coordenação — cada tarefa usa apenas `vTaskDelayUntil` com offsets iniciais calculados para
nunca colidirem. (Na semana 6 você refará isso com semáforos de verdade e comparará as duas
soluções.)
