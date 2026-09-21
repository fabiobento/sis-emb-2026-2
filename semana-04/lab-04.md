# Lab 4 — Botão por interrupção, timer periódico e o *watchdog*

> **Antes de começar**: leia a [teoria-04](teoria-04.md) — as Figuras 4-A e 4-B explicam o
> que você vai medir hoje, e a seção 2 lista as regras de ISR que você vai **violar de
> propósito** na Parte C. Errar em ambiente controlado é a oportunidade de observar para "se vacinar" contra problemas em sistemas embarcados.

**Objetivos**:
- substituir o polling do Lab 3 por **interrupção**;
- usar o `esp_timer` como heartbeat;
- **medir a latência** ISR→tarefa e;
- provocar (de propósito!) o Task Watchdog para aprender a reconhecer seu sintoma.

**Duração**: 2 aulas.

**Material**: ESP32, LED + R 220 Ω, botão, 1 jumper extra (só para a Parte D — veja lá).

> ⚠️ **Não use o botão BOOT da placa neste lab.** O firmware usa **GPIO4** para o botão (não
> mais GPIO0 como em versões antigas deste material) — GPIO0 é o pino de boot do ESP32, e um
> botão externo pendurado nele corre o risco de derrubar a placa em modo de gravação se for
> pressionado durante um reset (mesmo problema que resolvemos na Semana 3, Parte D). Monte um
> botão externo real entre **GPIO4** e **GND**, com o pull-up interno cuidando do resto.

**Circuito**: o circuito é parecido com o do Lab. 03, parte B, mas o firmware mudou (e o botão agora vai no **GPIO4**, não no GPIO0/BOOT). Se quiser, você pode conferir a simulação [nesse link do Wokwi](https://wokwi.com/projects/475146701299637249).

Monte o circuito da imagem abaixo (LED com R220 + botão com pull-up interno).
![circuito do lab 04](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab-04.png)

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — Rodando e entendendo (30 min)

1. Abra o seu diretório de trabalho com o VS Code:
```bash
code ~/sis-emb
```

2. Dentro do VS Code, abra o terminal integrado, crie e acesse o diretório do lab 04:
```bash
mkdir ~/sis-emb/lab4
cd ~/sis-emb/lab4
idf.py create-project botao_led_interrupt
cd botao_led_interrupt 
```  

3. Ainda no terminal integrado, copie o firmware do repositório do lab 04 para o seu diretório de projeto:
```bash
cp ~/sis-emb-2026-2/semana-04/src/isr_timer/main.c ~/sis-emb/lab4/botao_led_interrupt/main/botao_led_interrupt.c 
```

4. Antes de gravar, lembre-se:
   - Abra no VSCode o programa `~/sis-emb/lab4/botao_led_interrupt/main/botao_led_interrupt.c` **com a teoria do lado** (as seções 2 e 3 detalham a ISR e o timer linha a linha). 
   - quem  pisca o LED? quem conta os eventos?
   - quem imprime?
      (o callback do `esp_timer`, a ISR do botão e a tarefa principal — cada um com seu contexto  e suas restrições)
   

5. Compile, grave o firmware na placa e abra o monitor serial em um único comando. Confirme se o comportamento físico é o mesmo do simulador:
```bash
cd ~/sis-emb/lab4/botao_led_interrupt
idf.py -p /dev/ttyUSB0 flash monitor 
```

6. O comportamento esperado é o seguinte: LED piscando a 1 Hz (heartbeat de 500 ms via
   `esp_timer`) **independentemente** do botão; a cada pressionada:

```
evento #1 | latencia ate a tarefa: 812 us
evento #2 | latencia ate a tarefa: 1204 us
```

7. Anote 10 valores de latência. Essa latência **não** é a da ISR (que respondeu em ~µs):
   é o tempo até a *tarefa* notar o flag — inclui o `vTaskDelay` do laço dela. Guarde essa
   distinção para o relatório: **capturar** (ISR, µs, garantido pelo hardware) ×
   **processar** (tarefa, quando o escalonador deixar).

![Linha do tempo: do evento no pino até a execução da ISR](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/latencia_interrupcao.png)

*Figura L4-A — A latência que o hardware garante (até a ISR) é a parte esquerda desta
figura; o que você mede no lab é ela **mais** o tempo de a tarefa acordar — que depende do
laço dela, não do hardware.*

## Parte B — Polling × interrupção, na prática (25 min)

8. Faça uma "metralhadora de cliques": pressione o botão o mais rápido que conseguir por
   10 segundos e anote o total contado. Repita com o firmware do **Lab 3** (polling a
   2 ms). Compare os totais. Houve diferença? Com este botão e estas taxas, provavelmente
   pouca — então **quando a diferença importaria?** Responda com o Exemplo resolvido 4.1
   (encoder a 1 kHz: pulsos de 1 ms contra varredura de 2 ms — o polling perderia metade
   deles).
9. **Experimento de estresse do laço principal**: no firmware de hoje, aumente o
   `vTaskDelay` do laço da tarefa para 500 ms. Os eventos ainda são todos contados? (Sim —
   a ISR não depende do laço!) E a latência impressa? (Explode para até ~500 ms.) Registre
   os novos valores e explique a diferença entre *capturar* o evento e *processá-lo*.

## Parte C — Quebrando as regras (30 min)

Hora de errar em ambiente controlado.

10. **printf na ISR**: adicione um `printf("isr!\n");` dentro de `btn_isr` e regrave.
   Pressione o botão. Você verá um **`panic_abort`** com backtrace no monitor — dê um print para o relatório. Remova o printf. (Regra 2 da
   teoria: violada e comprovada. O `printf` usa mutex e buffers da newlib — recursos que
   assumem um contexto de tarefa numa ISR! E eis que o chão some, rsrs)
11. **Task WDT**: mude `#define PROVOCAR_WDT 0` para `1` e regrave. O `while(1){}` nu
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

## Parte D — Medindo a largura de um pulso (25 min) — revise o  com o Exemplo 4.3 da [teoria da Aula 4](https://github.com/fabiobento/sis-emb-2026-2/blob/main/semana-04/teoria-04.md)

Nesta parte o botão sai de cena. Em vez de medir uma pressionada manual (sujeita ao bounce
que já te deu trabalho antes), o **próprio ESP32 gera o pulso**: uma tarefa (`gerador_task`)
alterna a saída do **GPIO18** entre alto e baixo com larguras exatas e conhecidas por você —
e você mede essa largura pela ISR do **GPIO4**. Como o sinal vem de uma saída digital do
próprio chip (push-pull, sem contato mecânico), **não existe bounce para filtrar**: o
foco fica 100% na técnica de medição (`t1`/`t2`, `GPIO_INTR_ANYEDGE`), sem o ruído de "meu
botão quica diferente do seu".

> 💡 **Comece pelo Wokwi, não pelo hardware.** Como o GPIO18 e o GPIO4 estão no mesmo chip,
> dá para montar e depurar essa parte inteira em simulação antes de tocar na protoboard —
> inclusive **vendo o pulso de verdade** com o `Logic Analyzer` do Wokwi, em vez de confiar
> só no que o firmware imprime. Passo a passo:
>
> 1. Abra um novo projeto Wokwi (ESP-IDF) e adicione um [`Logic Analyzer (8 channels)`]
>    ([`wokwi-logic-analyzer`](https://docs.wokwi.com/guides/logic-analyzer)) ao circuito — clique no botão azul **+** e procure por
>    "Logic Analyzer".
> 2. Utilize como template de sua solução o projeto do Wokwi disponível nesse link: [lab-04-D](https://wokwi.com/projects/475239458696647681).
>
>    Repare que `esp:18` e `esp:4` estão ligados **direto um no outro** — é a simulação do
>    jumper físico que você vai fazer depois na protoboard.
>
> ![lab-04-parte-d](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab-04-parte-d.png)
>
> 3. **Trigger (opcional, mas recomendado)**: o Wokwi não tem um painel de propriedades
>    para isso — só dá para configurar editando o `diagram.json` direto. Na barra lateral de
>    arquivos do projeto, clique na aba **`diagram.json`**, ache a entrada do Logic Analyzer
>    e preencha o `"attrs"` dela:
>    ```json
>    "attrs": {
>      "triggerMode": "edge",
>      "triggerPin": "D0",
>      "triggerLevel": "low"
>    }
>    ```
>    **Não quer mexer no JSON?** Sem problema — deixe `"attrs": {}` (trigger desligado) e o
>    analisador grava a simulação inteira desde o "play". O buffer padrão aguenta 1 milhão
>    de amostras, sobra bastante para os poucos segundos deste lab; só rode, espere uns 2-3
>    pulsos passarem, e pare a simulação.
> 4. **Antes de escrever a ISR**, implemente só a `gerador_task` (item 15) e rode a
>    simulação por uns 5 segundos. Pare a simulação — o Wokwi baixa um `wokwi-logic.vcd`.
> 5. Abra esse arquivo no **PulseView** (instalado no seu PC — veja
>    [`docs/instalacao.md`](https://github.com/fabiobento/sis-emb-2026-2/blob/main/docs/instalacao.md), seção 6) e confirme visualmente: os pulsos
>    realmente duram ~150 ms e ~1200 ms? Use os cursores para medir direto na tela, sem
>    depender de nenhum código de medição ainda.
> 6. Só depois de confirmar visualmente que o gerador está correto, implemente a ISR
>    (itens 13-14) e compare a leitura impressa pelo firmware com o que você já mediu "no
>    olho" no PulseView. Se baterem, migre para o hardware real com confiança.
>
> Essa ordem — gerar, ver na tela, só então medir por código — é a mesma lógica de
> depuração que profissionais usam com osciloscópio/analisador de verdade: **nunca confie
> cegamente numa medição de firmware sem uma segunda fonte independente para conferir**.
>
> **Tem um analisador lógico USB físico na bancada?** Depois de validar em simulação, vale
> repetir a mesma verificação em **hardware real**, sem precisar do Wokwi: ligue um canal do
> analisador ao GPIO4 (ou ao GPIO18, já que são o mesmo sinal) e o `GND` dele ao `GND` do
> ESP32, e capture com o PulseView. Passo a passo completo — driver, pacote de firmware
> extra, e como configurar o trigger direto na interface (mais fácil que no Wokwi) — em
> [`docs/instalacao.md`](https://github.com/fabiobento/sis-emb-2026-2/blob/main/docs/instalacao.md), seção 6.3. A vantagem: a defasagem que você vai medir aqui é a
> latência **real** do chip, não uma aproximação do simulador — útil de novo no Desafio
> (opcional), mais abaixo.

12. **Religue o circuito**: desconecte o botão do GPIO4 (ele não é mais usado nesta parte) e
    ligue um **jumper físico** diretamente do **GPIO18** ao **GPIO4** — é o único fio novo.
13. Configure `GPIO18` como saída (`GPIO_MODE_OUTPUT`) e `GPIO4` como entrada com
    `gpio_set_intr_type(BTN, GPIO_INTR_ANYEDGE)` (ambas as bordas). Sem pull-up: quem define
    o nível do pino agora é sempre o `GPIO18`, ativamente — não há mais nível flutuante para
    proteger.
14. Na ISR, ao detectar borda de **descida** guarde `t1 = esp_timer_get_time()`; na de
    **subida**, calcule `s_duracao_us = agora - t1` e incremente o contador. Na tarefa,
    imprima a duração.

![Diagrama da Parte D: o próprio ESP32 gera o pulso pelo GPIO18, ligado por jumper ao GPIO4, com t1 na borda de descida e t2 na borda de subida, o que cada ramo da ISR faz em cada borda, e os dois pulsos de largura conhecida gerados pelo firmware](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab04_parte_d.png)

*O sinal chega ao GPIO4 pelo mesmo fio que o GPIO18 comanda — desce quando o `gerador_task`
zera o pino (t₁) e sobe quando ele volta a 1 (t₂). A mesma ISR de `GPIO_INTR_ANYEDGE` é
chamada nas duas bordas; é o `gpio_get_level(IN_PIN)` lido *dentro* dela que decide qual dos
dois ramos (`if`/`else`) executar — sem debounce nenhum, porque não há contato mecânico para
quicar.*

15. Implemente `gerador_task`: uma tarefa em loop que alterna dois pulsos de largura
    **conhecida** — por exemplo, 150 ms ("curto") e 1200 ms ("longo") — com um intervalo de
    repouso entre eles (`vTaskDelay`). É o seu próprio "gabarito": você sabe exatamente o
    valor certo antes mesmo de rodar.
16. **Compare**: os valores impressos pela ISR batem com os 150 ms / 1200 ms programados?
    A diferença esperada é só o *jitter* do `vTaskDelay` do gerador (alguns ms, resolução de
    tick) — não deve sobrar bounce nenhum. Você acabou de implementar o esqueleto da medição
    do HC-SR04 (semana 12) — só muda a escala: lá os pulsos terão centenas de **µs** (gerados
    por um sensor de verdade, não por outra tarefa sua) e o resultado vira distância via
    d = Δt × 340/2.

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| `Guru Meditation` ao apertar o botão | algo ilegal na ISR (printf, delay) | reveja as 5 regras da teoria §2 |
| Eventos param de contar | `gpio_isr_handler_add` esquecido | confira a instalação no `app_main` |
| LED heartbeat não pisca | `esp_timer_start_periodic` faltando | confira a Parte A do firmware |
| Latências sempre ~10 ms | está medindo o tick, não o evento | lembre: `vTaskDelay` tem resolução de 10 ms |
| Parte D: duração sempre 0 ou nunca imprime | jumper GPIO18→GPIO4 não ligado (ou ligado no pino errado) | confira o fio; sem ele, IN_PIN nunca muda de nível |
| Parte D: valores muito diferentes de 150/1200 ms | `larguras_ms[]` ou `vTaskDelay` do gerador com typo | confira `gerador_task`; o "gabarito" é o próprio código |

## Entrega (GitHub da bancada, `lab-04/relatorio.md`)

1. Tabela com as 10 latências da Parte A + média e máximo; e os valores do item 9 com a
   explicação capturar × processar (≤ 5 linhas).
2. Resposta do item 8: cenário numérico em que o polling do Lab 3 perderia eventos.
3. Prints das duas falhas da Parte C (erro do printf-na-ISR e mensagem do task_wdt) + a
   explicação da cadeia do WDT.
4. Código da Parte D (a ISR modificada e a `gerador_task`) + três leituras de duração,
   comparadas aos valores programados (150 ms / 1200 ms).
5. Parágrafo final: por que `printf` dentro da ISR é proibido e **como** o firmware
   contorna (flag `volatile` lida pela tarefa)?

## Desafio (opcional)

Latência real da ISR: em vez de medir até a tarefa, meça da borda até a **primeira linha da
ISR**. Como não dá para carimbar "antes" da ISR, use um truque de bancada: configure um
segundo GPIO como saída, faça a ISR **setá-lo imediatamente**, e ligue os dois pinos ao componente
`Logic Analyzer` do Wokwi! Compare a defasagem entre a borda do
botão e a borda da saída. Reporte o valor em µs.
