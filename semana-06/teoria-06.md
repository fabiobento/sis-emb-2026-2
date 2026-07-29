# Aula 6 — FreeRTOS II: Comunicação e Sincronização entre Tarefas (U3)

> **Pré-requisito**: Aula 5 (tarefas, preempção, estados).
> **Como usar**: texto autossuficiente. A tabela do Exemplo 6.1 merece ser redesenhada por
> você no caderno — ela é a mãe de todos os bugs de concorrência. Os Exemplos 6.1–6.3 são o
> modelo das questões 11–15 da Lista 2.

Na semana passada você dividiu o firmware em tarefas independentes — e criou um problema
novo. Tarefas independentes **precisam conversar**: a ISR do ADC produz amostras que outra
tarefa processa; duas tarefas querem usar a mesma UART; a lógica só pode começar quando
"Wi-Fi conectou **E** sensor calibrou". Coordenar tarefas com variáveis globais soltas parece
funcionar nos testes... e falha em campo, uma vez por semana, sem padrão reproduzível. Esta
aula apresenta o vilão (**condição de corrida**) e o arsenal civilizado do FreeRTOS: filas,
semáforos, mutex e event groups. No laboratório você vai **ver a corrida acontecer** — um
contador que deveria chegar a 2 000 000 e não chega — e consertá-la.

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) explicar o que é uma condição de corrida e por que ela é intermitente;
- (b) escolher a primitiva certa (fila, semáforo binário/contador, mutex, event group) para
  cada situação;
- (c) implementar os padrões produtor–consumidor e ISR→tarefa;
- (d) dimensionar uma fila;
- (e) contar a história do Mars Pathfinder sem consultar nada.

---

## 1. O problema: concorrência e a condição de corrida

### 1.1 `g++` não é uma operação — são três

Em C, `g_contador++` parece indivisível. Em assembly (qualquer RISC — semana 2), são
**três** instruções:

```
LOAD  r0, [g_contador]    ; 1. lê da memória para um registrador
ADD   r0, r0, 1           ; 2. soma 1 no registrador
STORE r0, [g_contador]    ; 3. escreve de volta na memória
```

O escalonador preemptivo (semana 5) pode trocar de tarefa **entre quaisquer duas
instruções**. E aí:

**Exemplo resolvido 6.1 (a corrida, passo a passo)** — Tarefas A e B incrementam `g` (valor
atual: 10). Siga o fio:

| passo | Tarefa A | Tarefa B | g na memória |
|---|---|---|---|
| 1 | LOAD r0 ← 10 | | 10 |
| 2 | *— preempção! —* | LOAD r0 ← 10 | 10 |
| 3 | | ADD → 11 | 10 |
| 4 | | STORE → **11** | 11 |
| 5 | ADD → 11 | | 11 |
| 6 | STORE → **11** | | **11** |

Dois incrementos, resultado 11 (deveria ser 12): A leu 10 e guardou 10 no seu registrador;
enquanto A dormia, B incrementou para 11; A acordou e somou sobre **o seu valor velho** — a
escrita de A **sobrescreveu** a de B, e um incremento evaporou. Com 1 000 000 de incrementos
de cada lado, o resultado final fica imprevisível entre ~1 000 000 e 2 000 000, **variando a
cada execução**. É o que você medirá no Lab 6, Parte A.

![Interleaving de instruções de duas tarefas mostrando a perda de um incremento](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/corrida_interleaving.png)

*Figura 6-A — A corrida em diagrama: o interleaving fatal acontece quando a troca de contexto
cai entre o LOAD e o STORE de uma das tarefas.*

### 1.2 Por que é o pior tipo de bug

- **Intermitente**: depende do instante exato da preempção — roda 999 vezes certo, falha na
  milésima. Teste de bancada quase nunca pega.
- **Sensível à observação**: adicionar um `printf` para depurar muda os tempos e... o bug
  some (o apelido de guerra é *Heisenbug*, em homenagem ao princípio de incerteza).
- **Invisível no código**: cada tarefa, lida sozinha, está "certa". O defeito não está em
  nenhuma linha — está na *relação temporal* entre elas.

A única defesa é **disciplina de projeto**: todo dado compartilhado entre tarefas (ou entre
tarefa e ISR) passa por uma primitiva de sincronização. Sempre. E lembre da semana 3:
`volatile` **não resolve isso** — ele garante que a leitura vá à memória, não que o
ler-modificar-escrever seja indivisível. `volatile` trata de *visibilidade*; as primitivas
desta aula tratam de *atomicidade*.

## 2. O arsenal: as cinco primitivas

| Primitiva | O que transporta/sinaliza | Uso típico no curso | Pode na ISR? |
|---|---|---|---|
| **Fila (queue)** | **cópias de dados** (N itens) | amostras ADC → tarefa (Lab 6B) | `xQueueSendFromISR` |
| **Semáforo binário** | 1 evento ("aconteceu!") | botão acorda tarefa (Lab 6C) | give sim |
| **Semáforo contador** | N eventos/recursos | rajadas de eventos; pool de buffers | give sim |
| **Mutex** | **posse** de um recurso | proteger contador, UART, barramento I2C | **NÃO** |
| **Event group** | máscara de bits | "Wi-Fi conectado **E** MQTT ok" (semana 14!) | set com ressalvas |

Vamos a cada uma, com o código que você usará hoje.

### 2.1 Mutex: exclusão mútua

O **mutex** (*mutual exclusion*) é um cadeado: a tarefa "toma" antes de mexer no recurso e
"devolve" depois; quem chegar com o cadeado tomado **bloqueia** (o estado nobre da semana 5
— sem gastar CPU) até a devolução. O trecho protegido chama-se **seção crítica**.

O conserto do Exemplo 6.1, exatamente como está em `src/corrida_mutex/main.c`:

```c
static SemaphoreHandle_t g_mutex;              // criado com xSemaphoreCreateMutex()

// dentro da tarefa:
xSemaphoreTake(g_mutex, portMAX_DELAY);        // toma (bloqueia se ocupado)
g_contador++;                                  // seção crítica: agora é indivisível
xSemaphoreGive(g_mutex);                       // devolve — SEMPRE, o quanto antes
```

Com o cadeado, a sequência do Exemplo 6.1 muda de destino: quando B toma o mutex, A **não
preempta para dentro** da seção crítica — ela bloqueia no `Take` e só entra quando B devolve.
Os três passos (LOAD-ADD-STORE) voltam a ser indivisíveis *na prática*, e o contador chega a
2 000 000 cravados.

Regras de bom uso: seção crítica **curta** (só o acesso ao dado, nunca I/O lento lá dentro —
cada microssegundo dentro do cadeado é tempo de espera imposto a todos os outros usuários do
recurso); quem toma, devolve (em todo caminho de saída, inclusive nos de erro); e nunca tomar
dois mutexes em ordens diferentes em tarefas diferentes — receita do **deadlock**, o abraço
mortal em que A segura o mutex 1 e espera o 2, enquanto B segura o 2 e espera o 1, para
sempre. Antídoto: ordem global de aquisição (todo mundo toma na mesma sequência).

### 2.2 Mutex ≠ semáforo binário: a herança de prioridade e o Mars Pathfinder

Por fora parecem iguais (take/give). A diferença decisiva: o mutex tem **dono** e implementa
**herança de prioridade**. O problema que ela resolve:

**Inversão de prioridade** — Tarefa **L** (baixa) toma o mutex. Tarefa **H** (alta) fica
pronta, preempta L, tenta tomar o mutex e bloqueia (normal, seria breve — é só L terminar a
seção crítica). Mas aí a tarefa **M** (média, que nem usa o recurso) fica pronta e preempta
L — e H, a mais prioritária do sistema, fica esperando **M** terminar. A prioridade se
inverteu: a média manda na alta.

![Linha do tempo da inversão de prioridade](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/inversao_prioridade.png)

*Figura 6-B — A inversão de prioridade: H (alta) bloqueada no mutex de L (baixa), enquanto M
(média) passeia pela CPU. Com herança de prioridade, L “empresta” a prioridade de H enquanto
segura o cadeado — e M não passa.*

Aconteceu em Marte: na sonda **Mars Pathfinder (1997)**, a tarefa de barramento (alta)
esperava um mutex tomado pela tarefa meteorológica (baixa), enquanto a de comunicações
(média) rodava por cima; o watchdog (semana 4!) via o sistema "travado" e reiniciava a sonda
— repetidamente, em outro planeta. O conserto foi enviado da Terra: **ativar a herança de
prioridade** do mutex. Com herança, enquanto L segura o mutex que H espera, L é *promovida*
à prioridade de H, termina rápido e devolve — M não consegue atrapalhar. Semáforo binário
não tem dono ⇒ não há quem promover — por isso **recurso compartilhado = mutex, evento =
semáforo**, nunca trocados.

### 2.3 Fila: transportando dados (produtor–consumidor)

A **fila** é um tubo FIFO de tamanho fixo que **copia** os itens (o produtor pode reutilizar
sua variável — o que entra na fila é uma fotocópia). Produtor insere com `xQueueSend`;
consumidor retira com `xQueueReceive` — e pode **bloquear esperando item chegar**, o que
elimina qualquer polling:

```
 produtor (100 Hz) ──▶ [ ▢▢▢▢▢▢ 64 itens ] ──▶ consumidor (em rajadas)
   xQueueSend                                     xQueueReceive
```

Note o que o padrão compra: o produtor **nunca espera** o consumidor (desacoplamento
temporal) e o consumidor **nunca perde** dados enquanto a fila tiver espaço (desacoplamento
de taxa). É a correia transportadora entre duas velocidades diferentes.

O firmware `src/fila_prod_cons/main.c`, por partes:

```c
g_fila = xQueueCreate(64, sizeof(uint32_t));   // 64 itens de 4 bytes (dimensionado abaixo!)

// PRODUTOR — 100 Hz cravados (vTaskDelayUntil, semana 5):
vTaskDelayUntil(&prox, pdMS_TO_TICKS(10));
if (xQueueSend(g_fila, &amostra, 0) != pdTRUE)        // timeout 0: não espera
    printf("FILA CHEIA! amostra %lu perdida\n", ...); // perda DETECTADA, nunca silenciosa

// CONSUMIDOR — dorme 300 ms (simulando estar ocupado) e drena tudo de uma vez:
UBaseType_t ocup = uxQueueMessagesWaiting(g_fila);    // instantâneo p/ estatística
while (xQueueReceive(g_fila, &v, 0) == pdTRUE) { /* processa v */ }
```

De onde saíram os 64 itens?

**Exemplo resolvido 6.2 (dimensionando fila)** — ISR do ADC produz uma amostra (4 bytes) a
1 kHz; a tarefa consumidora processa em rajadas, podendo ficar 50 ms sem rodar.

*Solução passo a passo.* Acúmulo no pior caso: 1 kHz × 50 ms = **50 itens** → fila de **64**
(folga + potência de 2, convenção que facilita aritmética de índices). RAM: 64 × 4 = 256 B ✔.
Regra geral: **capacidade ≥ taxa de produção × maior "apagão" do consumidor**, com folga —
porque o apagão real sempre supera o estimado. No Lab 6 você medirá o *high-water* da fila e
comparará com a conta; e verá a fila estourar quando o apagão dobrar — com a perda
**detectada** no log, jamais silenciosa. (Dado perdido sem aviso é como instrumento sem
alarme: o sistema mente com cara de saudável.)

### 2.4 Semáforos: sinalizando eventos

O **semáforo binário** transporta um único fato: "aconteceu". Perfeito para o padrão mais
importante do firmware — **ISR → tarefa**:

**Exemplo resolvido 6.3 (ISR→tarefa)** — Reescreva o botão da semana 4 na forma civilizada.

*Solução.*

```c
static SemaphoreHandle_t sem;                       // xSemaphoreCreateBinary()

static void IRAM_ATTR isr_botao(void *arg)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(sem, &woken);             // ~1 µs: só sinaliza
    portYIELD_FROM_ISR(woken);                      // troca de contexto JÁ, se preciso
}

static void tarefa_botao(void *arg)
{
    while (1) {
        xSemaphoreTake(sem, portMAX_DELAY);         // dorme sem gastar CPU
        // trabalho pesado AQUI: debounce fino, printf, I2C...
    }
}
```

A ISR cumpre as regras da semana 4 (curtíssima, sem bloqueio — só o give); a tarefa herda
todo o conforto (APIs normais, prioridade ajustável, preemptável). O `portYIELD_FROM_ISR`
garante latência mínima: se a tarefa acordada tem prioridade maior que a interrompida, a
troca acontece **na saída da ISR**, não no próximo tick. Este padrão — *ISR sinaliza, tarefa
trabalha* — é a espinha dorsal de todo driver bem escrito; você o verá no ADC (semana 7), no
CAN (semana 10) e no MQTT (semana 14).

O **semáforo contador** generaliza: cada give incrementa, cada take decrementa — eventos em
rajada não se perdem (o binário "satura" em 1: dez gives seguidos = um único take). Use-o
quando *contar* importa: quantos buffers livres restam, quantos pulsos de encoder chegaram.

### 2.5 Event group: esperando combinações

O **event group** é um conjunto de bits de evento; uma tarefa pode bloquear esperando
"bit X **e** bit Y" ou "X **ou** Y". É a ferramenta para partidas coordenadas — e é
exatamente como o firmware da semana 14 espera `BIT_GOT_IP` e depois `BIT_MQTT_OK` antes de
publicar:

```c
xEventGroupWaitBits(eg, BIT_GOT_IP, pdFALSE, pdTRUE, portMAX_DELAY);  // dorme até o Wi-Fi
// ... e no handler de evento, em outro contexto:
xEventGroupSetBits(eg, BIT_GOT_IP);
```

Os dois booleanos do meio respondem às perguntas naturais: “limpo os bits ao consumir?” e
“espero TODOS ou QUALQUER UM?”. Guarde a assinatura: você a reencontrará pronta no
`no_mqtt/main.c`.

## 3. Seções críticas de emergência

E quando o dado é compartilhado **com uma ISR** e a operação é minúscula (mutex não pode em
ISR!)? `portENTER_CRITICAL(&mux)` / `portEXIT_CRITICAL(&mux)`: desliga interrupções no núcleo
por algumas instruções. É a marreta — eficaz e perigosa: cada nanossegundo lá dentro é
latência adicionada a **todas** as interrupções do sistema. Use para 2–3 instruções
(ler-e-zerar um contador da ISR), jamais em torno de I/O. Se a proteção precisa durar mais
que isso, o design está errado: use fila.

> 💡 **Mapa mental de decisão** (cole na bancada): preciso passar **dados**? → fila.
> Sinalizar **um evento**? → semáforo binário. **Contar** eventos/recursos? → semáforo
> contador. Proteger **um recurso** de acesso concorrente? → mutex. Esperar **combinação** de
> condições? → event group. Mexer com **ISR** em operação de 2 instruções? → seção crítica.
> Variável global solta? → **errado**, sempre.

---

## Resumindo

- `g++` são três instruções; preempção no meio **perde** atualizações — corrida:
  intermitente, sensível à observação, invisível no código (Exemplo 6.1).
- `volatile` dá visibilidade, não atomicidade; dado compartilhado exige primitiva, sempre.
- **Mutex** para recursos (tem dono + herança de prioridade — Mars Pathfinder);
  **semáforo** para eventos; **fila** para dados (produtor–consumidor; capacidade = taxa ×
  apagão + folga, Exemplo 6.2); **event group** para combinações ("Wi-Fi E MQTT", semana 14).
- Padrão ISR→tarefa: `GiveFromISR` + `portYIELD_FROM_ISR` na ISR; take bloqueante + trabalho
  pesado na tarefa (Exemplo 6.3).
- Seção crítica com desligamento de interrupções: só para instruções contadas nos dedos.
- Deadlock: dois mutexes em ordens opostas — padronize a ordem de aquisição.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| condição de corrida | resultado depende do interleaving temporal |
| seção crítica | trecho com acesso exclusivo ao recurso |
| mutex | cadeado com dono e herança de prioridade |
| semáforo binário/contador | sinaliza 1 / N eventos |
| fila | FIFO que copia dados entre tarefas |
| event group | conjunto de bits para esperas combinadas |
| inversão de prioridade | alta espera média por causa do mutex da baixa |
| herança de prioridade | dono do mutex herda a prioridade de quem espera |
| deadlock | espera circular de mutexes |
| Heisenbug | bug que muda/some ao ser observado |

## 📖 Onde aprofundar (opcional)

- *Mastering the FreeRTOS Kernel* (gratuito em freertos.org), caps. 5–7 — filas, semáforos e
  mutex pelo autor do kernel, com diagramas de tempo excelentes.
- **Molloy**, cap. 6 — threads POSIX: o mesmo problema no Linux do Bloco 2.
- Sobre o Mars Pathfinder: relato "What really happened on Mars" (Glenn Reeves, JPL) —
  gratuito na web; leitura de 10 min que vale por um capítulo.

## Exercícios

Lista 2, questões 11–15 (estilo dos Exemplos 6.1–6.3).
