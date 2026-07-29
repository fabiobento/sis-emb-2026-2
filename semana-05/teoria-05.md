# Aula 5 — FreeRTOS I: Tarefas e Escalonamento (U3)

> **Pré-requisito**: Aula 4 (interrupções, `vTaskDelay` já usado nos labs).
> **Como usar**: texto autossuficiente. Os Exemplos 5.1–5.3 são o modelo das questões 6–10 da
> Lista 2. Esta aula muda a forma como você pensa firmware — releia a seção 1 até a dor do
> super-loop ficar óbvia.

Seu firmware até agora é um laço só. Funciona para um LED e um botão — mas um produto real
faz **várias coisas ao mesmo tempo**: amostra sensores a 100 Hz, atualiza um display a 5 Hz,
atende botões "na hora", mantém Wi-Fi vivo. Espremer tudo num único `while(1)` transforma o
firmware num castelo de cartas onde qualquer função lenta atrasa todas as outras. A solução
da indústria é o **RTOS** (*Real-Time Operating System*) — e você já está usando um sem
saber: todo programa ESP-IDF **nasce dentro do FreeRTOS**, o RTOS mais usado do mundo. Esta
semana abrimos essa caixa.

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) justificar o uso de um RTOS a partir das dores do super-loop;
- (b) descrever os estados de uma tarefa e o escalonamento preemptivo por prioridade;
- (c) criar tarefas no ESP-IDF com pilha e prioridade dimensionadas;
- (d) obter período exato com `vTaskDelayUntil` (e provar com números por que `vTaskDelay`
  não basta);
- (e) usar os dois núcleos do ESP32.

---

## 1. Do super-loop ao RTOS

### 1.1 A dor

O **super-loop** é a arquitetura bare-metal clássica — tudo que o firmware faz, num laço só,
na ordem em que foi escrito:

```c
while (1) {
    le_sensor();          // 1 ms
    atualiza_display();   // 80 ms  ← o vilão
    trata_botao();        // precisa responder "na hora"
}
```

Problema: os tempos ficam **acoplados**. O botão só é examinado a cada ~81 ms (latência
perceptível — interfaces que respondem em mais de ~100 ms “parecem travadas”); se amanhã o
display ganhar animação de 200 ms, *tudo* piora junto, inclusive o sensor que não tem nada a
ver com displays. Dá para remendar com **máquinas de estado** que fatiam o display em
pedacinhos de 5 ms intercalados com as outras funções — e firmwares reais dos anos 90 eram
exatamente isso: remendos engenhosos e ilegíveis, em que cada função nova exigia refatorar o
fatiamento de todas as outras. O custo de manutenção explode com o número de funcionalidades.

### 1.2 A solução

Um **RTOS** divide o firmware em **tarefas** independentes — cada uma com seu próprio laço,
sua prioridade e sua pilha — e um **escalonador** decide, a cada instante, qual tarefa usa a
CPU. A regra do FreeRTOS é **preempção por prioridade**: *a tarefa pronta de maior prioridade
roda, sempre*. Se uma tarefa de prioridade maior fica pronta (chegou um dado, venceu um
prazo), ela **interrompe imediatamente** a atual — como uma interrupção, mas entre tarefas.

O firmware acima vira três tarefas: `sensor` (prioridade alta, período 10 ms), `display`
(baixa, 200 ms), `botao` (alta, acordada por evento). O display lento **não atrasa mais
ninguém** — quando o sensor precisa rodar, ele simplesmente preempta o display no meio da
animação, faz seu 1 ms de trabalho, e o display continua de onde parou, sem saber de nada.
Cada tarefa é escrita **como se fosse a única no mundo** — a ilusão de CPU exclusiva é o
grande presente que o RTOS dá ao programador.

![Linha do tempo do escalonamento preemptivo por prioridade](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/escalonamento_preemptivo.png)

*Figura 5-A — Preempção por prioridade: a tarefa de maior prioridade toma a CPU no instante
em que fica pronta; as de menor prioridade usam as sobras. Leia da esquerda para a direita
acompanhando quem está “dentro” da CPU.*

**Espectro de software embarcado** (item 3.5 do PPC): bare-metal/super-loop → **RTOS**
(FreeRTOS, Zephyr; latências de µs, footprint de KB) → **Linux embarcado** (Bloco 2;
latências de ms, footprint de centenas de MB — a semana 11 discute quando cada um). O RTOS
**não** é um "Linux pequeno": é uma **biblioteca** linkada junto com seu código, sem
separação usuário/kernel, sem processos — só tarefas e o escalonador. Seu binário final
contém o kernel inteiro, e uma tarefa mal-educada pode corromper a memória de outra (não há
MMU protegendo ninguém — daí a disciplina desta e da próxima semana).

## 2. Conceitos do FreeRTOS

### 2.1 Tarefa: anatomia e criação

Uma tarefa é uma função com uma assinatura fixa, que **nunca retorna** (se retorna, ela deixa
de existir — quase sempre um bug):

```c
void minha_tarefa(void *arg)     // arg: ponteiro genérico passado na criação
{
    // inicialização própria da tarefa
    while (1) {
        // trabalho + ALGUMA chamada bloqueante (delay, fila, semáforo...)
    }
}

xTaskCreate(minha_tarefa,   // função
            "nome",         // nome para depuração (aparece nos logs de crash!)
            2048,           // pilha em bytes (ver Exemplo 5.3)
            NULL,           // arg
            5,              // prioridade: 0 (idle) a 24; MAIOR número = MAIOR prioridade
            NULL);          // handle (opcional, p/ suspender/deletar depois)
```

Cada tarefa tem **pilha própria** (suas variáveis locais vivem lá — por isso o tamanho
importa) e o escalonador salva/restaura os registradores da CPU a cada troca (**troca de
contexto**, alguns µs no ESP32 — é o preço da ilusão de CPU exclusiva, e é barato).

> **Observação — a regra de convivência**: toda tarefa deve **bloquear** em algum ponto do
> laço (delay, espera de fila/semáforo). Uma tarefa que nunca bloqueia monopoliza a CPU no
> seu nível de prioridade — as de prioridade menor **nunca** rodam (*starvation*,
> inanição), e o Task WDT da semana 4 denuncia (`IDLE0 not reset`). Provocaremos isso de
> novo hoje, agora de propósito e sabendo o nome do crime.

### 2.2 Estados de tarefa

Uma tarefa, a cada instante, está em exatamente um destes quatro estados:

![Diagrama de estados de uma tarefa FreeRTOS: pronta, executando, bloqueada, suspensa](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/estados_tarefa.png)

*Figura 5-B — O ciclo de vida da tarefa. Repare que “bloqueada” e “suspensa” **não** são a
mesma coisa: bloqueada acorda sozinha (tempo/evento); suspensa só acorda quando outra tarefa
manda.*

```
                 criada
                   │
                   ▼        escolhida pelo escalonador
              ┌─ PRONTA ─────────────────────▶ EXECUTANDO ─┐
   evento/    │    ▲                                │      │ vTaskSuspend()
   timeout    │    │  preemptada por prioridade     │      ▼
   chegou     │    └────────────────────────────────┤   SUSPENSA
              │                                     │ vTaskDelay / xQueueReceive…
              └─────────────── BLOQUEADA ◀──────────┘
```

- **Pronta**: quer a CPU, mas alguém de prioridade ≥ a sua está usando. Está na fila,
  “de braço levantado”.
- **Executando**: com a CPU na mão (uma por núcleo — no ESP32, no máximo duas).
- **Bloqueada**: o estado nobre — a tarefa espera tempo (`vTaskDelay`) ou evento (fila,
  semáforo) **sem gastar um ciclo de CPU** (compare com o polling da semana 3!). É o que
  torna o RTOS eficiente em energia — a ponte com o Exemplo 1.1: CPU sem tarefas prontas
  pode dormir.
- **Suspensa**: congelada por `vTaskSuspend()`, só volta com `vTaskResume()` — uso raro
  (depuração, modos especiais).

### 2.3 O tick e os delays

O escalonador acorda numa interrupção periódica chamada **tick** — no ESP-IDF, **100 Hz** por
padrão (você conferiu no menuconfig do Lab 2) ⇒ resolução de tempo de **10 ms** para as APIs
de tick (`pdMS_TO_TICKS(5)` arredonda para zero ou um — cuidado!). Precisão sub-tick é
trabalho do `esp_timer` (semana 4).

Dois delays, uma diferença crucial:

![Comparação entre vTaskDelay (período relativo, com deriva) e vTaskDelayUntil (período absoluto)](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/vtaskdelay_vs_until.png)

*Figura 5-C — `vTaskDelay` mede “100 ms depois de terminar” → o período inclui o trabalho e
deriva; `vTaskDelayUntil` agenda despertares absolutos → período cravado.*

- `vTaskDelay(n)`: dorme n ticks **a partir de agora** — o período real = n + duração do
  corpo.
- `vTaskDelayUntil(&ref, n)`: dorme **até o instante absoluto** ref + n e atualiza ref —
  período cravado, sem acúmulo, mesmo que o corpo demore (desde que caiba no período).

**Exemplo resolvido 5.1 (deriva de período)** — Tarefa deve rodar a cada 100 ms; o corpo
demora 7 ms. Compare as duas APIs ao longo de 1 minuto.

*Solução passo a passo.* Com `vTaskDelay(100 ms)`: período real = 100 + 7 = 107 ms → em 1
min acumula 60 × 7 = **420 ms de atraso** e a taxa efetiva cai para 1000/107 ≈ **9,35 Hz**
em vez de 10 Hz (erro de 6,5 %!). Com `vTaskDelayUntil`: período = 100,000 ms cravados ✔.
Para amostragem (semana 7) e controle (semana 13), **só** o segundo serve — período
irregular = espectro distorcido na análise de sinais e parcela derivada errada no PID.

No firmware de hoje (`src/tarefas/main.c`), o padrão canônico:

```c
static void tarefa(void *arg)
{
    const char *nome = (const char *)arg;
    TickType_t proximo = xTaskGetTickCount();      // referência absoluta inicial
    int64_t t_ant = esp_timer_get_time();
    while (1) {
        vTaskDelayUntil(&proximo, pdMS_TO_TICKS(500));   // período EXATO de 500 ms
        int64_t t = esp_timer_get_time();
        printf("[%s] core=%d  periodo=%.1f ms\n",
               nome, xPortGetCoreID(), (t - t_ant) / 1000.0);
        t_ant = t;                                  // medimos o período REAL e imprimimos
    }
}

void app_main(void)
{
    xTaskCreate(tarefa, "A", 2048, "A", 5, NULL);   // mesma função, três instâncias!
    xTaskCreate(tarefa, "B", 2048, "B", 3, NULL);   // (cada uma com SUA pilha e arg)
    xTaskCreate(tarefa, "C", 2048, "C", 1, NULL);
}
```

Saída esperada (períodos ~500,0 ms com jitter de décimos):

```
[A] core=0  periodo=500.0 ms
[B] core=0  periodo=500.1 ms
[C] core=0  periodo=499.9 ms
```

Note a elegância: **uma** função, **três** tarefas — o `arg` diferencia (cada tarefa recebe
seu próprio valor de `arg`, guardado junto à sua pilha). E `app_main` pode até retornar: as
tarefas criadas continuam vivas (quem morre é só a tarefa que rodava `app_main`).

### 2.4 Prioridades: quem merece a CPU?

Regra prática (base do *rate-monotonic scheduling*, o teorema clássico de tempo real):
**quanto menor o período/prazo, maior a prioridade**. E verifique a **utilização** total — a
fração da CPU consumida:

U = Σ (tempo de CPU por ativação / período)

**Exemplo resolvido 5.2 (prioridades e utilização)** — Tarefas: controle (2 ms de CPU a cada
10 ms), display (30 ms a cada 200 ms), log (roda quando der). Atribua prioridades e avalie a
folga.

*Solução.* Menor período → maior prioridade: controle = 5 > display = 3 > log = 1.
Utilização: U = 2/10 + 30/200 = 0,20 + 0,15 = **35 %** — folga confortável; o log vive dos
65 % restantes. (O rate-monotonic garante escalonabilidade até U ≈ 69 % para muitas tarefas;
acima disso, só análise mais fina.) Se o display tivesse prioridade **maior** que o controle,
o laço de controle sofreria jitter de até 30 ms (o corpo inteiro do display) — inaceitável
para a malha da semana 13. Moral: prioridade não é "importância para o usuário" — é
**urgência temporal**. O display é mais visível que o controle; e ainda assim manda menos.

### 2.5 Pilha: quanto dar a cada tarefa?

Cada tarefa carrega sua pilha (variáveis locais + quadros de chamada + contexto salvo).
Pouca pilha = *stack overflow* (crash com mensagem explícita, se a checagem estiver ativa);
pilha demais = SRAM desperdiçada (o Exemplo 2.3 da semana 2 lembra que ela é finita: 520 KB
para tudo).

**Exemplo resolvido 5.3 (dimensionamento de pilha)** — Tarefa usa buffer local de 1 KB +
chamadas aninhadas ≈ 512 B. Quanto declarar?

*Solução.* Soma: 1 536 B. Margem de 50 % (regra prática para absorver caminhos de código
mais profundos que o medido): ≥ 2,3 KB → usar **3072 bytes** em `xTaskCreate`. Verificação
empírica — a que manda: `uxTaskGetStackHighWaterMark(NULL)` devolve quantas palavras
**sobraram** no pior momento desde o boot. Meça no lab rodando o pior cenário, ajuste com
margem, documente o número no código.

> **Observação:** `printf` é faminto de pilha (~1 KB, por causa das rotinas de formatação).
> Tarefa que imprime com 1024 bytes de pilha é crash marcado. Nossos 2048 do firmware de
> hoje são o mínimo confortável para tarefas que imprimem pouco — e é por isso que o
> parâmetro de pilha do `xTaskCreate` do Lab 5 foi escolhido assim, não por chute.

## 3. Dual-core no ESP32

O ESP32 tem dois núcleos, e o FreeRTOS do ESP-IDF escalona nos dois (SMP — *symmetric
multiprocessing*). Você pode deixar o sistema decidir (`xTaskCreate`) ou **fixar** a tarefa
num núcleo:

```c
xTaskCreatePinnedToCore(f, "nome", pilha, arg, prio, &h, 1);   // core 1 (APP_CPU)
```

Boa prática do ecossistema: o **core 0** (PRO_CPU) carrega Wi-Fi/BT e serviços do sistema;
cargas pesadas ou sensíveis a jitter da **sua** aplicação vão para o **core 1**. No
laboratório, uma tarefa CPU-bound no core 1 deixará A, B e C (core 0) rodando em paz —
**paralelismo real**, não revezamento: duas instruções executam literalmente ao mesmo tempo.
`xPortGetCoreID()` dentro da tarefa revela onde ela está.

> 💡 **Pense aí**: dois núcleos dobram o desempenho? *Resposta: só se o trabalho for
> divisível em tarefas independentes que não disputem os mesmos dados. Duas tarefas que
> vivem tomando o mesmo mutex rodam quase tão devagar quanto num núcleo só — a semana 6 dá
> o motivo (a seção crítica só admite um de cada vez), e o Lab 5 mede o caso feliz.*

---

## Resumindo

- Super-loop acopla todos os tempos; RTOS desacopla em **tarefas** com **preempção por
  prioridade** — a pronta de maior prioridade roda, sempre.
- Tarefa: função que nunca retorna, pilha própria, e **deve bloquear** no laço (senão:
  starvation + task_wdt).
- Estados: pronta ↔ executando ↔ bloqueada (o estado eficiente — zero CPU!) ↔ suspensa; o
  **tick** (100 Hz ⇒ resolução 10 ms) move o relógio do escalonador.
- `vTaskDelay` deriva (Exemplo 5.1: 9,35 Hz em vez de 10 Hz); `vTaskDelayUntil` crava o
  período — obrigatório para amostragem e controle.
- Prioridade = urgência temporal (menor período → maior prioridade); some as utilizações e
  deixe folga (Exemplo 5.2: U = 35 %).
- Pilha: estime uso + 50 % e **meça** com `uxTaskGetStackHighWaterMark` (Exemplo 5.3);
  `printf` custa ~1 KB.
- Dual-core: sistema no core 0, cargas pesadas suas no core 1 (`xTaskCreatePinnedToCore`);
  paralelismo real só existe sem disputa de dados.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| RTOS | sistema operacional de tempo real |
| tarefa | fluxo de execução com pilha e prioridade próprias |
| escalonador | quem decide qual tarefa usa a CPU |
| preempção | interrupção de uma tarefa por outra mais prioritária |
| troca de contexto | salvar/restaurar registradores na troca de tarefa |
| tick | batida periódica do relógio do RTOS (10 ms) |
| starvation | tarefa que nunca roda por falta de CPU |
| utilização (U) | fração da CPU consumida: Σ CPU/período |
| rate-monotonic | menor período ⇒ maior prioridade |
| high water mark | máximo de pilha já usado (medido) |
| SMP | multiprocessamento simétrico (2 núcleos) |

## 📖 Onde aprofundar (opcional)

- *Mastering the FreeRTOS Real Time Kernel* (gratuito em freertos.org), caps. 3–4 — a fonte
  definitiva, escrita pelo autor do kernel.
- **Molloy**, *Exploring Raspberry Pi*, cap. 6 — threads e desempenho no Linux (o contraste
  com o RTOS que abre o Bloco 2).

## Exercícios

Lista 2, questões 6–10 (estilo dos Exemplos 5.1–5.3).
