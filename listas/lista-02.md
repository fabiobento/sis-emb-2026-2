# Lista de Exercícios 02 — Semanas 4 a 6
**Temas:** interrupções e temporizadores; FreeRTOS (tarefas, escalonamento); comunicação e
sincronização (filas, semáforos, mutex).
**Entrega:** individual, em PDF ou markdown no GitHub, até a aula teórica da semana 7.

## 📚 Como estudar para esta lista

1. Refaça os **exemplos resolvidos 4.1–6.3** sem olhar — em especial a **tabela do Exemplo 6.1**
   (o interleaving da corrida). As figuras de apoio de cada questão aparecem junto do enunciado.
2. Questões "explique com evento concreto do Lab" ganham nota cheia só se citarem **nomes reais**
   do código (`xSemaphoreTake`, `btn_isr`, `cpu_bound`…) — volte aos fontes dos labs.

## Parte A — Interrupções e timers (semana 4)
**Q1.** Compare polling e interrupção quanto a: latência de pior caso, uso de CPU e
complexidade. Em que situação o polling é a escolha **certa**?

![Linha do tempo da latência de interrupção: sincronização, salvamento de contexto, despacho](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/latencia_interrupcao.png)

**Q2.** *(estilo Exemplo 4.1)* Um encoder gera pulsos de 400 µs a até 1 kHz. Mostre por que a
varredura de 2 ms do Lab 3 perde pulsos e dimensione o período de polling que não perderia.
Conclua se ainda é razoável ou se a ISR se impõe.
> 💡 *Critério da teoria-04: período de varredura ≤ período do evento ÷ 10. Faça as contas dos
> dois lados antes de concluir.*

**Q3.** Enumere as três regras de ouro de uma ISR vistas em aula (curta; sem chamadas
bloqueantes; APIs `FromISR` + `IRAM_ATTR` quando aplicável) e explique o que dá errado ao
violar cada uma.
> 💡 *Os experimentos do Lab 4, Parte C, são as provas empíricas: cite o que vocês viram no
> monitor (o crash do printf e o task_wdt).*

**Q4.** *(estilo Exemplo 4.2)* Com clock base de 80 MHz, projete prescaler e valor de comparação
para interrupções a cada 250 ms. Dê duas combinações válidas e diga qual prefere e por quê.

**Q5.** O que é um watchdog timer, que classe de falha ele mitiga e por que "alimentar" o WDT
dentro de uma ISR de timer é um **anti-padrão**?

## Parte B — FreeRTOS: tarefas (semana 5)
**Q6.** Desenhe o diagrama de estados de uma tarefa (pronta, executando, bloqueada, suspensa) e
dê um evento concreto do nosso Lab 5 que causa cada transição.
> 💡 *Ex.: A→bloqueada: `vTaskDelayUntil`; bloqueada→pronta: fim do delay; pronta→executando:
> escalonador; executando→pronta: preempção pelo HOG…*

<details><summary>🖼️ Confira seu desenho (Fig. 5-B da teoria-05)</summary>

![Diagrama de estados de uma tarefa FreeRTOS: pronta, executando, bloqueada, suspensa](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/estados_tarefa.png)

</details>

**Q7.** *(estilo Exemplo 5.1)* Tarefa de 100 ms usa `vTaskDelay(100 ms)` e seu corpo demora
7 ms. Calcule o período efetivo e a deriva acumulada em 1 min; corrija com `vTaskDelayUntil` e
explique a diferença de semântica.

![Comparação entre vTaskDelay (período relativo, com deriva) e vTaskDelayUntil (período absoluto)](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/vtaskdelay_vs_until.png)

**Q8.** *(estilo Exemplo 5.2)* Tarefas: A (3 ms de CPU a cada 10 ms), B (10 ms a cada 100 ms),
C (log, quando der). Atribua prioridades, calcule a utilização total e verifique se C ainda roda.
O que acontece com C se B virar 60 ms a cada 100 ms?

**Q9.** Explique *starvation* e como o escalonador do FreeRTOS com *time slicing* trata tarefas
de **mesma** prioridade. Por que um `while(1){}` sem bloqueio numa tarefa de prioridade alta
congela as demais (e como o WDT de idle nos avisa)?
> 💡 *Vocês provocaram isso duas vezes: Lab 4 (PROVOCAR_WDT) e Lab 5 (HOG). Cite os logs.*

**Q10.** No ESP32 (2 núcleos), o que muda ao fixar (`xTaskCreatePinnedToCore`) a tarefa de
controle no core 1 e deixar Wi-Fi/sistema no core 0? Cite um benefício e um cuidado.
> 💡 *Benefício: jitter — teoria-05, seção 3. Cuidado: dados compartilhados entre núcleos viram
> concorrência REAL (simultânea), não só intercalada — a semana 6 inteira se aplica dobrada.*

## Parte C — IPC e sincronização (semana 6)
**Q11.** *(estilo Exemplo 6.1)* Duas tarefas fazem `g++` (lê–incrementa–escreve) 1000× cada.
Explique o entrelaçamento que perde incrementos e dê o intervalo possível do valor final.
Corrija com mutex e com alternativa sem bloqueio (variável por tarefa + agregação).
> 💡 *A alternativa sem bloqueio é a "solução do jardim de infância": cada criança com seu
> brinquedo, soma no final. Quando ela NÃO serve? (Quando o dado precisa de leitura
> instantânea consistente, não só de soma final.)*

![Interleaving de instruções de duas tarefas mostrando a perda de um incremento](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/corrida_interleaving.png)

**Q12.** Monte uma tabela comparando **fila, semáforo binário, semáforo de contagem, mutex e
event group**: o que sinalizam/carregam, caso de uso típico no curso, e se podem ser usados de ISR.
> 💡 *A tabela da teoria-06, seção 2, é o ponto de partida — complete a coluna "caso de uso no
> curso" com os labs 6, 10 e 14.*

**Q13.** *(estilo Exemplo 6.2)* ISR produz amostras de 4 B a 2 kHz; a tarefa consumidora acorda
a cada 40 ms. Dimensione a fila (itens e bytes) com folga de 2× e explique o que ocorre sem a
folga quando o Wi-Fi rouba CPU por 60 ms.

**Q14.** O que é inversão de prioridade? Descreva o caso Mars Pathfinder em 3 frases e explique
como a **herança de prioridade** do mutex do FreeRTOS resolve (e por que semáforo binário não).

![Linha do tempo da inversão de prioridade](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/inversao_prioridade.png)

**Q15.** Escreva (pseudocódigo ou C) o padrão ISR→tarefa com semáforo binário para o botão da
semana 4, incluindo `xSemaphoreGiveFromISR` e `portYIELD_FROM_ISR`, e justifique por que o
processamento pesado fica na tarefa.
