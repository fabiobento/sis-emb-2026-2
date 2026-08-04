# Lab 4 — Botão por interrupção, timer periódico e o watchdog em ação

> **Antes de começar**: leia a [teoria-04](teoria-04.md) — as Figuras 4-A e 4-B explicam o
> que você vai medir hoje, e a seção 2 lista as regras de ISR que você vai **violar de
> propósito** na Parte C. Errar em ambiente controlado é a melhor vacina.

**Objetivo**: substituir o polling do Lab 3 por **interrupção**; usar o `esp_timer` como
heartbeat; **medir a latência** ISR→tarefa; e provocar (de propósito!) o Task Watchdog para
aprender a reconhecer seu sintoma.

**Duração**: 2 aulas.
**Material**: ESP32, LED + R 220 Ω, botão (ou o BOOT da placa). **Wokwi**: circuito idêntico
ao Lab 3 — valide lá primeiro.

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — Rodando e entendendo (30 min)

1. Abra `~/sis-emb/semana-04/src/isr_timer/main.c` **com a teoria do lado** (seções 2 e 3
   dissecam a ISR e o timer linha a linha). Antes de gravar, responda mentalmente: quem
   pisca o LED? quem conta os eventos? quem imprime? (três "personagens" diferentes: o
   callback do `esp_timer`, a ISR do botão e a tarefa principal — cada um com seu contexto
   e suas restrições).
2. Grave e monitore. Comportamento esperado: LED piscando a 1 Hz (heartbeat de 500 ms via
   `esp_timer`) **independentemente** do botão; a cada pressionada:

```
evento #1 | latencia ate a tarefa: 812 us
evento #2 | latencia ate a tarefa: 1204 us
```

3. Anote 10 valores de latência. Essa latência **não** é a da ISR (que respondeu em ~µs):
   é o tempo até a *tarefa* notar o flag — inclui o `vTaskDelay` do laço dela. Guarde essa
   distinção para o relatório: **capturar** (ISR, µs, garantido pelo hardware) ×
   **processar** (tarefa, quando o escalonador deixar).

![Linha do tempo: do evento no pino até a execução da ISR](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/latencia_interrupcao.png)

*Figura L4-A — A latência que o hardware garante (até a ISR) é a parte esquerda desta
figura; o que você mede no lab é ela **mais** o tempo de a tarefa acordar — que depende do
laço dela, não do hardware.*

## Parte B — Polling × interrupção, na prática (25 min)

4. Faça uma "metralhadora de cliques": pressione o botão o mais rápido que conseguir por
   10 segundos e anote o total contado. Repita com o firmware do **Lab 3** (polling a
   2 ms). Compare os totais. Houve diferença? Com este botão e estas taxas, provavelmente
   pouca — então **quando a diferença importaria?** Responda com o Exemplo resolvido 4.1
   (encoder a 1 kHz: pulsos de 1 ms contra varredura de 2 ms — o polling perderia metade
   deles).
5. **Experimento de estresse do laço principal**: no firmware de hoje, aumente o
   `vTaskDelay` do laço da tarefa para 500 ms. Os eventos ainda são todos contados? (Sim —
   a ISR não depende do laço!) E a latência impressa? (Explode para até ~500 ms.) Registre
   os novos valores e explique a diferença entre *capturar* o evento e *processá-lo*.

## Parte C — Quebrando as regras (30 min)

Hora de errar em ambiente controlado.

6. **printf na ISR**: adicione um `printf("isr!\n");` dentro de `btn_isr` e regrave.
   Pressione o botão. Dependendo da sorte, você verá um **abort/Guru Meditation** com
   backtrace no monitor — fotografe a primeira linha do erro. Remova o printf. (Regra 2 da
   teoria: violada e comprovada. O `printf` usa mutex e buffers da newlib — recursos que
   assumem um contexto de tarefa; numa ISR, o chão some.)
7. **Task WDT**: mude `#define PROVOCAR_WDT 0` para `1` e regrave. O `while(1){}` nu
   monopoliza a CPU; em ~5 s o monitor mostra:

```
E (xxxxx) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
E (xxxxx) task_wdt:  - IDLE0 (CPU 0)
```

   Copie a mensagem para o relatório e explique **com suas palavras** a cadeia: laço sem
   bloqueio → IDLE nunca roda → IDLE não alimenta o WDT → aviso. Volte `PROVOCAR_WDT` para
   0.

> **Observação:** memorize a "cara" dessas duas falhas (backtrace de ISR ilegal e
> task_wdt). Nas próximas semanas, quando aparecerem sem convite, você diagnosticará em
> segundos em vez de horas. O task_wdt, em particular, é a mensagem mais comum em projetos
> de fim de semestre — e quase sempre denuncia um `while` esperando flag que outra tarefa
> deveria setar (solução: `vTaskDelay` de 1 tick dentro do laço, ou melhor, semáforo da
> semana 6).

## Parte D — Medindo a largura de um pulso (25 min) — ponte com o Exemplo 4.3

8. Reconfigure a interrupção do botão para **ambas as bordas**
   (`gpio_set_intr_type(BTN, GPIO_INTR_ANYEDGE)`).
9. Na ISR, ao detectar borda de **descida** guarde `t1 = esp_timer_get_time()`; na de
   **subida**, calcule `s_duracao_us = agora - t1` e incremente o contador. Na tarefa,
   imprima a duração.
10. Meça: quanto dura **sua** pressionada "curta"? E uma "longa" proposital? (Valores
    típicos: 80–300 ms e >1 s.) Você acabou de implementar o esqueleto da medição do
    HC-SR04 (semana 12) — só muda a escala: lá os pulsos terão centenas de **µs** e o
    resultado vira distância via d = Δt × 340/2.

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| `Guru Meditation` ao apertar o botão | algo ilegal na ISR (printf, delay) | reveja as 5 regras da teoria §2 |
| Eventos param de contar | `gpio_isr_handler_add` esquecido | confira a instalação no `app_main` |
| LED heartbeat não pisca | `esp_timer_start_periodic` faltando | confira a Parte A do firmware |
| Latências sempre ~10 ms | está medindo o tick, não o evento | lembre: `vTaskDelay` tem resolução de 10 ms |

## Entrega (GitHub da bancada, `lab-04/relatorio.md`)

1. Tabela com as 10 latências da Parte A + média e máximo; e os valores da Parte B.5 com a
   explicação capturar × processar (≤ 5 linhas).
2. Resposta da Parte B.4: cenário numérico em que o polling do Lab 3 perderia eventos.
3. Prints das duas falhas da Parte C (erro do printf-na-ISR e mensagem do task_wdt) + a
   explicação da cadeia do WDT.
4. Código da Parte D (só a ISR modificada) + três medições de largura de pulso.
5. Parágrafo final: por que `printf` dentro da ISR é proibido e **como** o firmware
   contorna (flag `volatile` lida pela tarefa)?

## Desafio (opcional)

Latência real da ISR: em vez de medir até a tarefa, meça da borda até a **primeira linha da
ISR**. Como não dá para carimbar "antes" da ISR, use um truque de bancada: configure um
segundo GPIO como saída, faça a ISR **setá-lo imediatamente**, e ligue os dois pinos ao...
Wokwi Logic Analyzer (peça `wokwi-logic-analyzer`)! Compare a defasagem entre a borda do
botão e a borda da saída. Reporte o valor em µs.
