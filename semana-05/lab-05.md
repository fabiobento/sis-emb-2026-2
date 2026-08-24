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

1. Grave `~/sis-emb/semana-05/src/tarefas/main.c` (detalhado na seção 2.3 da teoria). Três
   instâncias da **mesma** função viram as tarefas A (prio 5), B (prio 3) e C (prio 1),
   cada uma imprimindo seu período real medido:

```
[A] core=0  periodo=500.0 ms
[B] core=0  periodo=500.1 ms
[C] core=0  periodo=499.9 ms
```

2. Observe por ~30 s: as três rodam "juntas" mesmo com prioridades diferentes. Por quê?
   (Resposta esperada: cada uma dorme 499 ms a cada 500 — a CPU está ociosa ~99,9 % do
   tempo; prioridade só decide **disputas**, e quase não há disputa. É como três pessoas
   num corredor de 10 metros de largura: a "prioridade de passagem" só importa quando duas
   chegam à porta juntas.) Anote — o contraste com a Parte B é o ponto do lab.

## Parte B — Starvation ao vivo (25 min)

3. Descomente a função `cpu_bound` e a linha que a cria — mas **troque** o núcleo para 0 e
   a prioridade para 6:

```c
xTaskCreatePinnedToCore(cpu_bound, "HOG", 2048, NULL, 6, NULL, 0);
```

4. Regrave e observe o monitor por 20 s. O que acontece com A, B e C? E que mensagem
   aparece (~5 s depois)? Registre a saída — você deve reconhecer o `task_wdt`/IDLE0 do
   Lab 4, agora com nome de crime: **starvation** por uma tarefa CPU-bound de prioridade
   máxima (teoria, seção 2.1, "regra de convivência"). Como o HOG tem prioridade 6 e nunca
   bloqueia, ele **sempre** é a tarefa pronta mais prioritária do core 0 — A, B, C e a IDLE
   simplesmente nunca rodam. É a Figura 5-A da teoria com um vilão permanente.
5. Abaixe a prioridade do HOG para **1** (igual à de C) e regrave. A, B voltam ao normal?
   E C — roda sempre, às vezes, nunca? (Dica: mesma prioridade ⇒ *time slicing* por tick —
   o escalonador reveza C e HOG a cada 10 ms, então C roda "na metade do tempo" e com
   período dobrado. Explique com a teoria em ≤ 3 linhas.)

## Parte C — Deriva de período: Exemplo 5.1 ao vivo (30 min)

6. Crie uma 4ª tarefa `D` (prio 4) com **corpo lento e `vTaskDelay`** — a receita da
   deriva:

```c
static void tarefa_d(void *arg)
{
    int64_t t_ant = esp_timer_get_time();
    while (1) {
        int64_t fim = esp_timer_get_time() + 50000;      // "trabalho" de 50 ms
        while (esp_timer_get_time() < fim) { }           // (ocupado de propósito)
        vTaskDelay(pdMS_TO_TICKS(200));                  // dorme 200 ms A PARTIR DE AGORA
        int64_t t = esp_timer_get_time();
        printf("[D] periodo=%.1f ms\n", (t - t_ant) / 1000.0);
        t_ant = t;
    }
}
```

7. Meça 10 períodos de D. Valor esperado: ~**250 ms**, não 200 (200 de sono + 50 de corpo)
   — a deriva do Exemplo 5.1 com outros números. Calcule: em 1 minuto, quantas ativações D
   perde em relação às 300 ideais? (60 000/250 = 240 ativações → **60 perdidas**, 20 % da
   taxa!)
8. Troque o `vTaskDelay` por `vTaskDelayUntil` (copie o padrão da tarefa A: variável
   `proximo` + chamada no **início** do laço) e meça de novo. Esperado: ~**200,0 ms**
   cravados, com o corpo de 50 ms "absorvido" dentro do período. Preencha:

| Configuração | período médio (ms) | período máx (ms) |
|---|---|---|
| vTaskDelay + corpo 50 ms | | |
| vTaskDelayUntil + corpo 50 ms | | |

> 🧠 **Por que isso é sério e não pedantismo**: na semana 7 você amostrará um sinal
> esperando taxa constante, e na semana 13 o PID calculará `K_d·(e−e_ant)/T_s` assumindo
> T_s exato. Uma deriva de 20 % no período vira 20 % de erro na derivada — invisível no
> código, devastador no resultado.

## Parte D — Pilha: medindo o high water mark (20 min)

9. Na tarefa A, imprima a folga de pilha a cada ciclo:

```c
printf("[A] pilha livre: %u palavras\n", (unsigned)uxTaskGetStackHighWaterMark(NULL));
```

10. Anote o valor estabilizado. Agora **provoque**: declare na tarefa um
    `char buf[1500];` e use-o (`snprintf(buf, sizeof buf, "x"); printf("%s", buf);`). O
    valor caiu quanto? Com pilha de 2048, sobrou margem? Relacione com a receita do
    Exemplo 5.3 (uso + 50 %) e proponha o tamanho certo para esta tarefa.

> **Observação:** se exagerar no buffer (tente 3000!) você verá o crash de *stack overflow*
> com o nome da tarefa culpada — mais uma "cara de erro" para a sua coleção (junto do
> task_wdt do Lab 4 e do backtrace da ISR).

## Parte E — Dual-core (20 min)

11. Restaure o HOG com prioridade 6, mas agora **no core 1**:

```c
xTaskCreatePinnedToCore(cpu_bound, "HOG", 2048, NULL, 6, NULL, 1);
```

12. Regrave: A, B e C (core 0) devem voltar a rodar em dia **mesmo com o HOG vivo** — os
    núcleos trabalham em paralelo de verdade. Confirme pelos logs que as tarefas imprimem
    `core=0` e pelo desaparecimento do task_wdt. (Se quiser ver o HOG confessar o núcleo,
    dê um printf nele com `xPortGetCoreID()` + um `vTaskDelay(1000)` só para não afogar o
    monitor.)

> 💡 **A lição de arquitetura**: no ESP32, o core 0 já carrega Wi-Fi/BT e serviços do
> sistema. Cargas pesadas da sua aplicação → core 1. É a divisão de trabalho que os
> projetos finais saudáveis usam — e a resposta da questão 5 da entrega.

---

## Entrega (GitHub da bancada, `lab-05/relatorio.md`)

1. Explicação da Parte A.2 (por que prioridades diferentes convivem em CPU ociosa), ≤ 3
   linhas.
2. Saída da Parte B.4 (starvation + task_wdt) e resposta da B.5 (time slicing).
3. Tabela da Parte C preenchida + a conta de ativações perdidas (C.7).
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
