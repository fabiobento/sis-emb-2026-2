# Lab 3 — GPIO de entrada e saída + o experimento do bouncing

> **Antes de começar**: leia a [teoria-03](teoria-03.md) — a seção 2.5 detalha o firmware de
> hoje linha a linha, e as Figuras 3-C/3-D mostram exatamente os circuitos que você montará.

**Objetivo**: montar o circuito LED + botão; entender o firmware de polling com debounce;
**medir o bouncing do seu botão** variando a janela de debounce e concluir o valor ideal.

**Duração**: 2 aulas.
**Material**: ESP32, protoboard, LED + resistor 220 Ω, botão táctil, resistor 10 kΩ (parte
D), jumpers.

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb && git fetch && git reset --hard origin/main
```

## Parte A — Primeiro no Wokwi (25 min)

Como sempre no Bloco 1: **simule antes de gravar**.

1. Novo projeto ESP32/ESP-IDF no Wokwi; cole `~/sis-emb/semana-03/src/botao_led/main.c`
   (detalhado na seção 2.4 da teoria — leia antes!).

```c
// Semana 3 — botão com pull-up interno + debounce por software (polling)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>

#define LED   GPIO_NUM_2
#define BTN   GPIO_NUM_0
#define DEBOUNCE_MS 20

void app_main(void)
{
    gpio_reset_pin(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    gpio_reset_pin(BTN);
    gpio_set_direction(BTN, GPIO_MODE_INPUT);
    gpio_pullup_en(BTN);              // repouso = 1; pressionado = 0

    int  led = 0, eventos = 0;
    int  nivel_ant = 1;
    int64_t t_ok = 0;                 // instante a partir do qual aceitamos nova borda

    while (1) {
        int nivel = gpio_get_level(BTN);
        int64_t agora = esp_timer_get_time() / 1000;      // ms
        if (nivel_ant == 1 && nivel == 0 && agora >= t_ok) {  // borda de descida válida
            led = !led;
            gpio_set_level(LED, led);
            printf("evento #%d\n", ++eventos);
            t_ok = agora + DEBOUNCE_MS;
        }
        nivel_ant = nivel;
        // Garantir pelo menos 1 tick no padrão de 100Hz do ESP-IDF
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

```   
2. Circuito: LED no **D2** via resistor de 220 Ω (como no Lab 1) + `wokwi-pushbutton` entre
   **D0** e **GND**. Não adicione resistor no botão: o código habilita o **pull-up interno**.

![](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab-03.png)

- Se quiser pode montar o circuito com o diagrama.json abaixo ou acessar o projeto Wokwi: [Lab-03](https://wokwi.com/projects/473268920512424961).
```json
{
  "version": 1,
  "author": "Fabio Bento",
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
      "type": "wokwi-led",
      "id": "led1",
      "top": 82.8,
      "left": 90.6,
      "attrs": { "color": "red", "flip": "1" }
    },
    {
      "type": "wokwi-resistor",
      "id": "r1",
      "top": 119.45,
      "left": 133,
      "rotate": 180,
      "attrs": { "value": "220" }
    },
    {
      "type": "wokwi-pushbutton-6mm",
      "id": "btn1",
      "top": 30.6,
      "left": 131.2,
      "rotate": 270,
      "attrs": { "color": "green", "xray": "1" }
    }
  ],
  "connections": [
    [ "esp:TX", "$serialMonitor:RX", "", [] ],
    [ "esp:RX", "$serialMonitor:TX", "", [] ],
    [ "led1:A", "esp:2", "green", [ "v0" ] ],
    [ "led1:C", "r1:2", "green", [ "v0" ] ],
    [ "r1:1", "esp:GND.2", "green", [ "v0" ] ],
    [ "btn1:2.l", "esp:GND.2", "green", [ "h38.8", "v-67.2" ] ],
    [ "btn1:1.l", "esp:0", "green", [ "h-48", "v57.6", "h-19.2" ] ]
  ],
  "dependencies": {}
}
```

3. Rode a simulação: cada clique deve alternar o LED e imprimir `evento #N` no monitor.
4. **Verifique**: no Wokwi, clique e *segure* o botão. Por que o LED não fica
   alternando enquanto seguro? Localize no código a linha responsável e anote (é a detecção
   de borda — relatório, questão 1).

> 🧠 **A resposta (não leia antes de pensar!)**: a condição
> `nivel_ant == 1 && nivel == 0` só é verdadeira **no instante da transição**. Com o botão
> seguro, `nivel` fica 0 e `nivel_ant` também — a condição nunca mais se cumpre. Borda é
> evento; nível é estado. É o mesmo princípio que diferencia "a porta abriu" de "a porta
> está aberta".

> OBSERVAÇÃO: o Wokwi simula o pull-up interno do ESP32, então não há resistor externo no botão.
![Diagrama do botão com pull-up interno do ESP32: LED no GPIO2 e botão entre GPIO0 e GND](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/botao_pullup-esp32.png)

*Figura L3-A — A montagem do lab no ESP32: o LED (com R220) no GPIO2 e o botão entre GPIO0 e GND.
O pull-up é o **interno** do chip (`gpio_pullup_en`), então não há resistor externo — em repouso o
GPIO0 lê 1 e, pressionado, lê 0.*

> **Observação:** o GPIO 0 é também o pino de *boot* da placa (o botão "BOOT" embutido está
> ligado nele!). Vantagem didática: você pode testar o firmware **sem botão externo**,
> usando o BOOT da própria placa. Cuidado colateral: se o botão estiver pressionado durante
> um reset, a placa entra em modo de gravação — solte e resete de novo.

## Parte B — Hardware (25 min)

5. Monte o mesmo circuito na protoboard: LED→R220→GPIO2; botão entre **GPIO0 e GND**.

![](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab-03-hard.png)

6. **Crie o projeto via CLI**: no terminal, com o ambiente do ESP-IDF ativado, navegue até a pasta de trabalho desejada e inicie um novo projeto chamado `botao_led`:
```bash
cd ~/sis-emb/lab3
idf.py create-project botao_led
```


7. Entre na pasta criada (`cd botao_led`):
```bash
cd botao_led
```
Em seguida abra o arquivo `main/botao_led.c` no seu editor de código e **substitua todo o conteúdo** pelo código em C que você utilizou e simulou na Parte A.

8. Compile, grave o firmware na placa e abra o monitor serial em um único comando. Confirme se o comportamento físico é o mesmo do simulador:
```bash
idf.py -p /dev/ttyUSB0 flash monitor 

```

## Parte C — O experimento do bouncing (40 min)

Este é o coração do lab: **quantificar** o fenômeno do Exemplo resolvido 3.3 no *seu*
botão. Lembre da figura da teoria: os contatos metálicos quicam por 1–10 ms antes de
assentar, gerando uma rajada de bordas. Hoje vocês medirão a rajada.

7. No topo do código, mude `DEBOUNCE_MS` para **0** e regrave. Agora o firmware conta
   **todas** as bordas, sem filtro.
8. Pressione o botão **10 vezes**, com firmeza normal, e anote o total de `evento #N`
   impressos. Se deu mais que 10, você acabou de *ver* o bouncing.
9. Repita o procedimento para as janelas **5, 20 e 50 ms**, preenchendo:

| DEBOUNCE_MS | eventos por 10 pressionadas | eventos "fantasma" |
|---|---|---|
| 0 | | |
| 5 | | |
| 20 | | |
| 50 | | |

10. **Teste de clique rápido**: com 50 ms, tente clicar o mais rápido que conseguir (duas
    pressionadas em sequência imediata). Alguma pressionada legítima foi "engolida"? É o
    outro lado do compromisso (janela grande demais).
11. Conclua no relatório: qual janela seu grupo adotaria e por quê, citando os dois limites
    (duração do bouncing medida × percepção humana ~50 ms).

> **Observação:** botões diferentes "quicam" diferente — inclusive entre unidades do mesmo
> lote. Compare sua tabela com a da bancada vizinha. Em produto real, dimensiona-se pela
> *pior* unidade (e mede-se com osciloscópio — *Molloy*, Fig. 4-21, mostra um bouncing real
> capturado: uma serra de ~5 ms antes do nível estabilizar).

## Parte D — Invertendo a lógica: pull-down externo (30 min)

12. Desmonte o botão e remonte com **pull-down externo**: GPIO0 → botão → **3V3**, e um
    resistor de **10 kΩ** do GPIO0 ao **GND** (segure o desenho da Figura 3-C da teoria ao
    lado).
13. Ajuste o firmware (3 mudanças): desabilite o pull-up interno (`gpio_pullup_dis`),
    habilite `gpio_pulldown_dis` também (queremos só o externo) e **inverta a detecção de
    borda** — agora o repouso é 0 e o acionamento é a borda **0→1**
    (`nivel_ant == 0 && nivel == 1`).
14. Valide: mesmo comportamento externo, lógica interna invertida ("ativo-alto").

> 💡 **Por que este exercício existe**: para você sentir que pull-up e pull-down são
> **escolhas**, não leis da física — e que a escolha muda três coisas acopladas: o circuito,
> o nível de repouso e a borda do evento. Confundir essa trindade é a origem do clássico
> "meu botão funciona ao contrário".

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| LED acende sozinho / conta sem clique | entrada flutuando (pull-up não habilitado) | confira `gpio_pullup_en(BTN)` |
| Conta 2× por clique mesmo com 20 ms | janela medida errada / código antigo | regrave após editar; confira `DEBOUNCE_MS` |
| Placa entra em modo de gravação | GPIO0 pressionado no reset | solte o botão e resete |
| Nada acontece no clique | botão em fileira errada da protoboard | botão táctil tem os pares ligados em cruz — gire 90° |

## Entrega (GitHub da dupla, `lab-03/relatorio.md`)

1. Resposta da Parte A.4: qual condição do código impede repetição com o botão seguro?
2. Tabela do experimento C completa + conclusão sobre a janela ideal (Parte C.11).
3. *Diff* das três mudanças da Parte D (pode ser print do código com as linhas destacadas).
4. Foto da montagem final (pull-down) com o resistor de 10 kΩ visível.

## Desafio (opcional)

Clique simples × clique duplo: modifique o firmware para distinguir 1 clique (alterna o LED)
de 2 cliques em até 400 ms (pisca o LED 3× rápido). Dica: ao detectar um clique, em vez de
agir na hora, aguarde 400 ms observando se vem o segundo — uma máquina de estados com dois
estados resolve.
