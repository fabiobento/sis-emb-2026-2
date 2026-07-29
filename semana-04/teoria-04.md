# Aula 4 — Interrupções e Temporizadores (U2)

> **Pré-requisito**: Aula 3 (GPIO, debounce por software, polling).
> **Como usar**: texto autossuficiente. Os Exemplos 4.1–4.3 são o modelo das questões 1–5 da
> Lista 2. No laboratório você provocará **de propósito** os dois crashes clássicos desta
> aula (printf na ISR; watchdog) — errar em ambiente controlado é a melhor vacina.

No Lab 3 seu firmware perguntava 500 vezes por segundo: "o botão mudou?". Funcionou — mas
imagine perguntar isso para dez periféricos, alguns exigindo resposta em microssegundos,
enquanto o laço principal também precisa trabalhar. Esta aula apresenta o mecanismo que
inverte a lógica: em vez de a CPU perguntar, **o hardware avisa**. Interrupções são o coração
de todo firmware sério — e também a origem dos bugs mais traiçoeiros, então vêm com um código
de conduta rígido. Na segunda parte, conheceremos os **temporizadores de hardware** (o
metrônomo do sistema) e o **watchdog** (o vigia que reinicia tudo quando seu código trava).

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) comparar polling e interrupção **com números** (latência e custo de CPU);
- (b) escrever ISRs corretas (curtas, sem bloqueio, em IRAM, com APIs `FromISR`);
- (c) projetar um timer de hardware (prescaler + comparação) para um período pedido;
- (d) explicar o papel do watchdog e reconhecer seu disparo no monitor serial.

---

## 1. Polling × Interrupção

**Polling**: a CPU verifica o periférico repetidamente num laço. Simples de escrever,
previsível de raciocinar — mas o custo de CPU é pago **mesmo sem eventos** (500 leituras por
segundo para descobrir que nada aconteceu 499 vezes), e a latência de pior caso é o período
de varredura: se o evento nasce logo depois de uma verificação, ele espera o ciclo inteiro.

**Interrupção**: o periférico sinaliza a CPU no instante do evento. A CPU **suspende** o
fluxo atual (salvando o contexto: contador de programa e registradores), salta para a
**ISR** (*Interrupt Service Routine*) registrada para aquele evento, executa-a e **retorna**
ao ponto exato onde estava — o código interrompido nem fica sabendo. Latência típica: poucos
µs. Custo de CPU: zero quando não há eventos — e é isso que permite à CPU **dormir** entre
eventos (releia o Exemplo 1.1: sem interrupção, o deep sleep da semana 14 seria impossível).

![Comparação temporal entre polling e interrupção](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/polling_vs_interrupcao.png)

*Figura 4-A — Polling: a CPU interrompe o trabalho útil para perguntar, e o evento espera a
próxima pergunta. Interrupção: o hardware chama, a ISR trata, o programa retoma de onde
parou.*

O que acontece, em câmera lenta, entre a borda no pino e a primeira linha da sua ISR:

![Linha do tempo da latência de interrupção: sincronização, salvamento de contexto, despacho](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/latencia_interrupcao.png)

*Figura 4-B — Anatomia da latência de interrupção. A parte da CPU (salvar/restaurar
contexto) explica por que a ISR deve ser curta: tudo nessa figura é tempo em que o resto do
sistema está congelado.*

```
 fluxo principal ──────────■ (borda no pino!) ┌────────────┐
                            \────────────────▶│    ISR     │
                             salvamento de     │ (curta!)   │
                             contexto          └─────┬──────┘
                            /◀───────────────────────┘
 fluxo principal ──────────■  restaura contexto e segue
```

**Exemplo resolvido 4.1 (latência de polling — e quando ela condena o projeto)** — Varredura
a cada 2 ms (Lab 3): latência de pior caso = 2 ms + tempo de tratamento. Um encoder de motor
a 3 000 RPM com 20 pulsos/volta sobrevive a esse polling?

*Solução passo a passo.*

1. Frequência dos pulsos: f = (3 000/60) voltas/s × 20 pulsos/volta = **1 000 pulsos/s**.
2. Período de um pulso: T = 1 ms **< 2 ms** (período da varredura).
3. Conclusão: o pulso nasce e morre entre duas varreduras → **perderíamos pulsos**, e a
   contagem de rotação ficaria errada de forma silenciosa. Com interrupção, cada borda é
   capturada com latência de poucos µs — 100× mais rápido que o evento.

Critério geral: **eventos rápidos (período < ~10× a varredura) ou raros (custo de polling
desperdiçado) ⇒ interrupção; sinais lentos e constantes ⇒ polling ainda é honesto** — e mais
simples de depurar. Engenharia é escolher a ferramenta mais simples que atende ao requisito.

> **Observação — quem decide qual ISR roda?** O controlador de interrupções: cada fonte
> (pino, timer, UART…) tem uma entrada numa **tabela de vetores** — um array de endereços de
> ISRs que a CPU consulta por hardware, sem gastar um ciclo de software para descobrir quem
> chamou. No ARM ela tem nome próprio (vetor de exceções); no ESP-IDF, a matriz de
> interrupções faz o roteamento e `gpio_isr_handler_add()` pendura sua função lá.

## 2. Regras de ouro para ISRs

Uma ISR executa num contexto especial: outras interrupções da mesma prioridade esperam, e o
sistema inteiro fica "prendendo a respiração" — incluindo o escalonador. Daí o código de
conduta:

1. **Curtíssima**: capture o evento (leia o dado, marque um flag, carimbe o tempo), sinalize
   uma tarefa e **saia**. Processamento pesado é trabalho da tarefa (a semana 6 formaliza o
   padrão ISR→tarefa com semáforo/fila). Regra de bolso: se a ISR passa de poucos
   microssegundos, algo está fora do lugar.
2. **Nunca bloqueie**: `vTaskDelay`, `printf`, `malloc`, mutex — proibidos. Bloquear numa
   ISR é contraditório (bloquear = esperar o escalonador te acordar; mas o escalonador está
   congelado esperando você sair). Além de travarem, muitos deles simplesmente não funcionam
   nesse contexto — o `printf` na ISR é o crash que provocamos no laboratório.
3. **`IRAM_ATTR`**: no ESP32, marque a ISR com `IRAM_ATTR` para que o código resida **na
   RAM**. Motivo Harvard-modificada (semana 2): a ISR pode disparar enquanto a flash está
   ocupada (ex.: gravação de NVS, que desabilita o cache de instruções) — código em flash
   nesse instante = crash.
4. **APIs `...FromISR()`**: dentro de ISR, use apenas as variantes `xQueueSendFromISR`,
   `xSemaphoreGiveFromISR` etc., com a variável `xHigherPriorityTaskWoken` +
   `portYIELD_FROM_ISR()` para trocar de contexto imediatamente se uma tarefa de alta
   prioridade acordou. Sem isso, a tarefa acordada só roda no próximo tick — latência
   desnecessária de até 10 ms.
5. **Dados compartilhados**: variáveis tocadas pela ISR e pelo resto do código são
   `volatile` (semana 3) e, se a atualização não for atômica, protegidas por seção crítica.

Veja as regras encarnadas no firmware do laboratório (`src/isr_timer/main.c`) — a ISR
completa:

```c
static volatile uint32_t s_eventos = 0;     // regra 5: volatile (ISR escreve, tarefa lê)
static volatile int64_t  s_t_isr   = 0;

static void IRAM_ATTR btn_isr(void *arg)    // regra 3: IRAM_ATTR
{
    int64_t agora = esp_timer_get_time();
    if (agora - s_ultimo_evento_us > DEBOUNCE_US) {   // debounce DENTRO da ISR: só aritmética
        s_ultimo_evento_us = agora;
        s_eventos++;                        // regra 1: só captura e conta
        s_t_isr = agora;                    // carimbo p/ medir latência na tarefa
    }
}                                           // regra 2: nada de printf/delay aqui!
```

E quem imprime? A **tarefa** principal, ao notar `s_eventos != vistos` — o processamento
saiu da ISR. Repare que o debounce virou comparação de *timestamps* (Exemplo 3.3, agora em
µs), porque ISR não pode esperar: em vez de “descarte bordas por 20 ms”, temos “aceite
bordas separadas por mais de 20 ms” — a mesma ideia, escrita sem nenhuma espera.

A instalação, no `app_main`:

```c
gpio_set_intr_type(BTN, GPIO_INTR_NEGEDGE);   // dispare na borda de DESCIDA (ativo-baixo)
gpio_install_isr_service(0);                  // serviço de despacho de ISRs de GPIO
gpio_isr_handler_add(BTN, btn_isr, NULL);     // pendura btn_isr no vetor do pino
```

Por que borda de descida? Porque o botão usa pull-up e é ativo-baixo (semana 3): apertar =
o pino cai de 1 para 0. Se o botão fosse ativo-alto, seria `GPIO_INTR_POSEDGE`; para medir
largura de pulso (seção 3, HC-SR04), usa-se `GPIO_INTR_ANYEDGE` — qualquer borda chama.

## 3. Temporizadores de hardware

Como fazer algo "a cada X ms" com precisão? `vTaskDelay` tem resolução de tick (10 ms) e
depende do escalonador (se uma tarefa mais prioritária estiver ocupada, seu atraso estica —
jitter). A resposta de precisão é o **timer de hardware**: um contador incrementado pelo
clock, dividido por um **prescaler**, que gera uma interrupção ao atingir o valor de
**comparação** (alarme) — e pode recarregar sozinho (modo periódico), sem a CPU no caminho:

```
clock 80 MHz ──▶ [÷ prescaler] ──▶ contador ──▶ (== alarme?) ──▶ IRQ + recarrega
```

**Exemplo resolvido 4.2 (projeto de timer)** — Projete um timer para interromper a cada
250 ms, com clock base de 80 MHz.

*Solução passo a passo.*

1. Escolha do prescaler: prescaler 80 → tick = 80 MHz / 80 = 1 MHz ⇒ **1 µs por contagem**
   (um número redondo e conveniente — é o “metro” do nosso sistema de tempo).
2. Alarme: 250 ms em contagens de 1 µs = 250 000.
3. Verificação de alcance: o contador do ESP32 tem 64 bits ⇒ estouro em 2⁶⁴ µs ≈ 584 mil
   anos — na prática, ilimitado.
4. Alternativa: prescaler 8000 (tick de 100 µs, alarme 2 500) daria o mesmo período — mas
   com resolução 100× pior. Preferimos o tick de 1 µs pela resolução e pelo alinhamento com
   `esp_timer_get_time()`, que também conta em µs.

Método geral (decore o caminho, não os números): **período_desejado = prescaler × alarme ÷
clock**. Escolha o prescaler para um tick “redondo” (1 µs, 1 ms) e o alarme cai de presente.

No ESP-IDF, a API de alto nível **`esp_timer`** entrega exatamente isso sem tocar
registradores — e é o "heartbeat" do firmware desta semana:

```c
static void heartbeat_cb(void *arg)          // callback: roda em contexto de TAREFA
{                                            // (uma tarefa dedicada do esp_timer)
    static int nivel = 0;
    gpio_set_level(LED, nivel ^= 1);         // XOR alterna (semana 3!)
}

const esp_timer_create_args_t targs = { .callback = heartbeat_cb, .name = "hb" };
esp_timer_handle_t timer;
esp_timer_create(&targs, &timer);
esp_timer_start_periodic(timer, 500000);     // período em µs: 500 ms
```

> **Observação:** por padrão o callback do `esp_timer` roda numa *tarefa* de alta
> prioridade, não numa ISR — por isso pode usar APIs normais (inclusive `gpio_set_level`).
> Ainda assim, mantenha-o curto: ele compartilha a fila com todos os outros timers do
> sistema — um callback guloso atrasa os demais.

**Exemplo resolvido 4.3 (medição de largura de pulso — o futuro HC-SR04)** — Para medir o
eco do sensor ultrassônico (semana 12 no RPi): o sensor responde a um disparo com um pulso
cuja **largura** é proporcional à distância. Como medi-la?

*Solução.* Capturar o instante da borda de subida (t₁) e da descida (t₂) por interrupção
(uma ISR em `GPIO_INTR_ANYEDGE` carimbando `esp_timer_get_time()` a cada chamada), e
calcular: distância = (t₂ − t₁) × velocidade_do_som ÷ 2 (o som vai **e volta** — daí o ÷ 2).
Com v_som = 340 m/s e um eco de 1 166 µs:

d = 1 166 × 10⁻⁶ s × 340 m/s ÷ 2 ≈ **0,198 m** (19,8 cm)

A resolução de 1 µs do relógio ⇒ resolução de distância de 340 m/s × 1 µs ÷ 2 = 0,17 mm —
sobra em relação à precisão do sensor (±3 mm); o erro real virá do sensor e da temperatura do
ar (o som viaja mais rápido no ar quente: ~0,6 m/s por °C).

## 4. Watchdog Timer (WDT): o vigia do firmware

E se, apesar de tudo, o firmware travar em campo — um laço infinito por bug, um deadlock? Não
há ninguém para apertar reset num medidor no poste, num satélite, num controlador enterrado
numa plantação. O **watchdog** é um contador de hardware que **reinicia o sistema** se o
software não o "alimentar" (resetar) dentro do prazo. Firmware saudável alimenta o WDT no seu
laço principal; firmware travado deixa o prazo estourar → reset → sistema volta ao ar
sozinho. É a última linha de defesa, presente em todo produto sério — e um dos motivos pelos
quais seu medidor de energia raramente “trava” de vez.

O ESP-IDF já ativa por padrão o **Task WDT** vigiando a tarefa IDLE de cada núcleo (a IDLE é
a tarefa que roda quando ninguém mais quer a CPU — se ela não roda, alguém está
monopolizando): se alguma tarefa monopolizar a CPU com um laço sem bloqueio (a violação da
semana 5), a IDLE nunca roda e você verá no monitor:

```
E (15324) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
E (15324) task_wdt:  - IDLE0 (CPU 0)
```

Vamos **provocar isso de propósito** no laboratório (`#define PROVOCAR_WDT 1` liga um
`while(1){}` nu) — você reconhecerá essa mensagem no futuro como um médico reconhece um
sintoma: “alguém segurou a CPU e nunca bloqueou”.

> **Observação — anti-padrão clássico**: alimentar o WDT dentro de uma ISR de timer. A ISR
> continua viva mesmo com o laço principal travado ⇒ o WDT nunca dispara e perde a razão de
> existir — o vigia foi subornado. Alimente o watchdog **no fluxo que você quer vigiar**.
> (Questão da Lista 2.)

> 💡 **Pense aí — por que watchdog e não “código sem bugs”?** Porque produtos vivem 10–20
> anos em campo, sob radiação, surtos de tensão, memória que envelhece e condições que o
> laboratório nunca viu. O watchdog não substitui qualidade — é a admissão humilde de que
> *algum* caminho de falha sempre escapa. Confiabilidade em camadas: código bom + watchdog +
> (em sistemas críticos) redundância.

---

## Resumindo

- Polling paga CPU sempre e tem latência = período de varredura; interrupção custa ~zero em
  repouso e responde em µs (o Exemplo 4.1 dá o critério com números: período do evento vs.
  período da varredura).
- Interrupção é o que permite à CPU **dormir** entre eventos — a base do baixo consumo.
- ISR: curta, sem bloqueio, `IRAM_ATTR`, APIs `FromISR`, dados compartilhados `volatile` —
  processamento pesado vai para a tarefa (padrão que a semana 6 completa com semáforo/fila).
- Timer de hardware = clock ÷ prescaler → contador → alarme; **período = prescaler × alarme ÷
  clock**; prescaler 80 dá o tick canônico de 1 µs no ESP32 (Exemplo 4.2); `esp_timer`
  entrega callbacks periódicos prontos.
- Medição de pulso por duas bordas + carimbos de tempo: d = Δt × 340 / 2 (Exemplo 4.3) —
  será nosso HC-SR04.
- Watchdog reinicia firmware travado; o Task WDT do ESP-IDF denuncia tarefas que monopolizam
  a CPU; nunca alimente o WDT de uma ISR de timer.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| ISR | rotina de tratamento de interrupção |
| tabela de vetores | array de endereços de ISRs consultado por hardware |
| contexto | PC + registradores salvos na troca de fluxo |
| IRAM_ATTR | atributo que coloca o código na SRAM (ISR segura) |
| prescaler | divisor de clock à frente do contador do timer |
| alarme (comparação) | valor que dispara a IRQ do timer |
| jitter | variação de tempo de resposta/periodicidade |
| watchdog | timer que reinicia o sistema se não for alimentado |
| Task WDT | watchdog do ESP-IDF que vigia as tarefas IDLE |

## 📖 Onde aprofundar (opcional)

- **ESP-IDF Programming Guide**: *GPIO ISR*, *esp_timer*, *Watchdogs* — a documentação
  oficial das três APIs desta aula.
- **Molloy**, *Exploring Raspberry Pi*, cap. 6 — resposta a eventos e desempenho em Linux
  (o contraste µs×ms que abriremos na semana 11).
- **Upton & Duntemann**, cap. 4 — exceções e interrupções no ARM.

## Exercícios

Lista 2, questões 1–5 (estilo dos Exemplos 4.1–4.3; o anti-padrão do watchdog aparece na
última).
