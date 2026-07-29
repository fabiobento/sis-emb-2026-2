# Lab 6 — Vendo a corrida acontecer (e consertando com mutex e fila)

> **Antes de começar**: leia a [teoria-06](teoria-06.md) — especialmente o Exemplo 6.1 (a
> tabela do interleaving) e o mapa mental de decisão do final. Hoje você reproduzirá em
> bancada o bug mais traiçoeiro do firmware — e o matará com as ferramentas certas.

**Objetivo**: **provocar e medir** uma condição de corrida real; consertá-la com mutex;
montar o padrão produtor–consumidor com fila; dimensionar a fila e vê-la estourar quando o
consumidor "apaga".

**Duração**: 2 aulas.
**Material**: apenas o ESP32 (funciona 100 % no Wokwi). Botão/LED opcionais na Parte C.

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb && git fetch && git reset --hard origin/main
```

## Parte A — A corrida (35 min)

O firmware `~/sis-emb/semana-06/src/corrida_mutex/main.c` cria **duas tarefas idênticas**
que incrementam o mesmo contador global 1 000 000 de vezes cada (releia o Exemplo resolvido
6.1: o `g++` que são três instruções — LOAD, ADD, STORE — e a Figura 6-A com o
interleaving fatal). O resultado *deveria* ser 2 000 000.

1. Confirme no topo do arquivo: `#define USAR_MUTEX 0` (proteção desligada). Repare também
   no `xTaskCreatePinnedToCore(..., 1)`: as duas tarefas vão para o **mesmo núcleo**, de
   propósito — queremos maximizar preempções entre elas (com uma em cada núcleo o problema
   seria *pior* ainda, mas de outra natureza: aí as escritas aconteceriam
   **simultaneamente de verdade**, não só intercaladas; um passo de cada vez).
2. Grave e anote o valor final impresso pela última tarefa a terminar. Rode **5 vezes**
   (basta resetar a placa com o botão EN) e preencha:

| execução | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| contador final | | | | | |

3. O que você deve observar: valores **diferentes a cada execução**, todos < 2 000 000 —
   incrementos evaporaram, e a quantidade evaporada depende de *quando* o escalonador
   preemptou. Este é o bug intermitente da teoria (seção 1.2), reproduzido em bancada.

> **Observação:** se por acaso uma execução der exatamente 2 000 000, rode de novo —
> corrida é probabilística. É exatamente por isso que ela passa nos testes e explode em
> campo.

4. Agora `#define USAR_MUTEX 1`, regrave e repita as 5 execuções. Esperado: **2 000 000
   cravados, sempre**. Anote também o tempo total (compare o carimbo de tempo do monitor):
   quanto o mutex custou em desempenho? (Take/give ~1–2 µs × 2 000 000 = alguns segundos a
   mais — proteção não é grátis; por isso a seção crítica deve ser curta. E note: o custo
   é *previsível*, ao contrário do bug, que era *aleatório*. Engenharia prefere custo
   conhecido a risco desconhecido.)

## Parte B — Produtor–consumidor com fila (40 min)

5. Grave `~/sis-emb/semana-06/src/fila_prod_cons/main.c` (teoria, seção 2.3): produtor a
   100 Hz cravados (`vTaskDelayUntil` — semana 5 em ação) enche a fila de 64 itens; o
   consumidor "dorme" 300 ms simulando estar ocupado e drena tudo em rajada.
6. Observe o monitor por ~30 s e responda com números:
   - Quantos itens o consumidor drena por rajada, tipicamente? (Esperado ≈ 100 Hz × 0,3 s =
     **30**.)
   - Alguma mensagem `FILA CHEIA!` apareceu? (Não deveria: 30 < 64.)
7. **Verifique o Exemplo 6.2 na prática**: aumente o "apagão" do consumidor para **700 ms**
   e regrave. Agora a produção por apagão (70) supera a capacidade (64): o monitor deve
   mostrar perdas detectadas:

```
FILA CHEIA! amostra 4471 perdida
```

   Conte quantas perdas por rajada (esperado ≈ 70 − 64 = 6, variando ±1). A conta da teoria
   bateu com a bancada?
8. Corrija **sem** mudar o consumidor: qual capacidade de fila suporta o apagão de 700 ms
   com a folga de 2× da regra do Exemplo 6.2? (70 × 2 = 140 → potência de 2 seguinte:
   **256** itens.) Calcule, ajuste o `xQueueCreate`, regrave e comprove o fim das perdas.
   Registre a conta no relatório.

## Parte C — ISR→tarefa com semáforo binário (30 min)

Hora de aposentar de vez o polling do botão: o padrão profissional do Exemplo resolvido
6.3.

9. Crie um projeto novo (ou uma cópia do Lab 4) e implemente:
   - ISR do botão (GPIO0, borda de descida, `IRAM_ATTR`) que **só** faz
     `xSemaphoreGiveFromISR(sem, &woken)` + `portYIELD_FROM_ISR(woken)` — copie da teoria,
     seção 2.4, entendendo cada linha;
   - tarefa `botao` (prioridade 4) que bloqueia em `xSemaphoreTake(sem, portMAX_DELAY)`,
     faz o debounce por carimbo de tempo (20 ms) e imprime o evento com a **latência**
     ISR→tarefa (carimbe `esp_timer_get_time()` na ISR numa variável `volatile`, como no
     Lab 4).
10. Compare a latência medida com a do Lab 4 (onde a tarefa fazia polling da flag a cada
    `vTaskDelay`): o semáforo deve derrubá-la de "até o período do laço" para **dezenas de
    µs** — a tarefa acorda *no ato*, cortesia do `portYIELD_FROM_ISR`.

| método | latência média | latência máx |
|---|---|---|
| Lab 4: flag + polling da tarefa | | |
| Lab 6: semáforo + take bloqueante | | |

11. **Experimento do contador**: pressione o botão 5× *muito* rápido (< 20 ms entre bordas
    — difícil, o bouncing ajuda!). Com semáforo **binário**, gives em rajada saturam em 1:
    alguns eventos "somem". Troque por `xSemaphoreCreateCounting(10, 0)` e repita: agora
    cada give é contado. Explique a diferença em 2 linhas (teoria, seção 2.4, último
    parágrafo).

> 🧠 **Onde esse padrão reaparece**: na ISR do ADC com DMA, na recepção de CAN (semana 10)
> e no callback de dados MQTT (semana 14) — sempre "interrupção sinaliza, tarefa processa".
> Você acabou de aprender a estrutura de todo driver profissional.

---

## Entrega (GitHub da dupla, `lab-06/relatorio.md`)

1. Tabela da Parte A (5 execuções sem mutex + 5 com) + 3 linhas: por que os valores sem
   mutex variam e por que com mutex não.
2. Números da Parte B (itens por rajada, perdas com 700 ms) + a conta e o novo tamanho da
   fila da B.8.
3. Tabela de latências da Parte C.10 + código da sua ISR e da tarefa (só os dois blocos).
4. Resposta da C.11 (binário × contador).
5. Uma frase honesta: qual primitiva você usaria para proteger o barramento I2C que duas
   tarefas compartilharão na semana 9 — e por que não um semáforo binário? (Dica: Mars
   Pathfinder.)

## Desafio (opcional)

Deadlock didático: crie os mutexes `mA` e `mB` e duas tarefas — T1 toma `mA`, dorme 100 ms,
toma `mB`; T2 toma `mB`, dorme 100 ms, toma `mA`. Rode, observe o congelamento (e o
task_wdt eventual), e então conserte **apenas reordenando** as aquisições. Relate o
antes/depois — você acabou de demonstrar a regra da ordem global de aquisição.
