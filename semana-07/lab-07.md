# Lab 7 — Lendo o mundo: LDR no ADC, filtro em ação e senoide no DAC

> **Antes de começar**: leia a [teoria-07](teoria-07.md) — o Exemplo 7.2 (divisor com LDR) é
> a conta que você calibrará na Parte A, as Figuras 7-A/7-B mostram o que o ADC faz com a
> sua tensão, a seção 1.2 explica a saturação da Parte C e a seção 3 (Nyquist e aliasing) é
> o que você verá acontecer, ao vivo, na Parte E.

**Objetivo**: montar o divisor com LDR e **calibrá-lo** contra as contas do Exemplo 7.2;
medir o ruído do ADC com e sem média móvel; provocar a saturação; gerar e "ouvir" a escada
do DAC; e (opcional) **ver o aliasing acontecer** ligando o DAC ao ADC na mesma placa.

**Duração**: 2 aulas (a Parte E é opcional e pode ficar para casa).
**Material**: ESP32, LDR (temos 10), resistor 10 kΩ, protoboard, jumpers; multímetro;
lanterna do celular; (opcional, Parte D) buzzer passivo ou fone velho + resistor 220 Ω.
As Partes A–C têm um passo no Wokwi (com um potenciômetro no lugar do divisor) para validar o código antes do hardware; a Parte D (DAC) e a
Parte E (loopback) pedem a placa real.

> ⚠️ **Os pinos do ESP32 toleram no máximo 3,3 V.** Nunca ligue 5 V (pino VIN/5V da placa
> ou saída de módulo de 5 V) em um GPIO — nem "só por um instante". Neste lab o único ponto
> de alimentação que encosta em GPIO é o **3V3**. E lembre: **GPIO34 só é entrada** (os
> GPIOs 34–39 não têm saída nem pull-up interno), por isso é ótimo para o ADC e inútil para
> acender LED.

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — Divisor com LDR: montagem e calibração (35 min)

1. **Wokwi primeiro**: o Wokwi não tem o LDR "solto" (só o módulo de LDR com saída
   digital, que **não** serve ao ADC), então vamos simular o **nó do divisor** com um
   potenciômetro: girar o potenciômetro equivale a "iluminar/escurecer" o LDR, e o GPIO34
   recebe uma tensão de 0 a 3,3 V exatamente como receberá do divisor real. Crie um projeto
   ESP32 (ESP-IDF) em [wokwi.com](https://wokwi.com) e substitua o `diagram.json` por este
   (saída no **GPIO34**, canal 6 do ADC1):

```json
{
  "version": 1,
  "author": "sua-bancada",
  "editor": "wokwi",
  "parts": [
    {
      "type": "board-esp32-devkit-c-v4",
      "id": "esp",
      "top": 0,
      "left": 0,
      "attrs": { "builder": "esp-idf" }
    },
    {
      "type": "wokwi-potentiometer",
      "id": "pot1",
      "top": -20,
      "left": -170,
      "attrs": {}
    }
  ],
  "connections": [
    [ "esp:TX", "$serialMonitor:RX", "", [] ],
    [ "esp:RX", "$serialMonitor:TX", "", [] ],
    [ "pot1:VCC", "esp:3V3", "red", [] ],
    [ "pot1:GND", "esp:GND.1", "black", [] ],
    [ "pot1:SIG", "esp:34", "orange", [] ]
  ],
  "dependencies": {}
}
```

   Isso valida o **firmware** (ADC, média móvel, estatística) dos itens 3 a 7 antes do
   hardware, mas não o sensor: o Wokwi não reproduz a resistência do LDR nem a sua
   calibração (item 8), que é com o LDR real. Se algum fio ficar estranho no desenho, é só
   arrastá-lo: a lógica é VCC→3V3, GND→GND, SIG→GPIO34.

   > Na bancada, use o **LDR solto** (aquele de duas perninhas) com o resistor de 10 kΩ,
   > como no item 2. O **módulo de LDR azul** com LM393, trimpot e pinos VCC/GND/**DO** tem
   > só saída **digital** (claro/escuro, com limiar no trimpot): serve para o desafio de
   > "acender com a luz", mas **não** para este lab — o ADC precisa da tensão analógica do
   > divisor. (Se quiser, o LDR do módulo pode ser dessoldado e usado como o LDR solto.)

2. Na protoboard: **3V3 → LDR → nó do GPIO34 → 10 kΩ → GND** (LDR em cima, como no Exemplo
   7.2 e no desenho da teoria, seção 2). Antes de ligar, **meça com o multímetro** a
   resistência do LDR fora do circuito: com a luz da sala, com a lanterna encostada e
   coberto com a mão — você vai usar esses três valores no item 8.

![Esquema do divisor da bancada: 3V3, LDR, nó ligado ao GPIO34, resistor de 10 kΩ e GND, com a fórmula do divisor](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab07_divisor_ldr.png)

*Figura L7-A — O circuito que você vai montar. O GPIO34 lê o **nó do meio**: quanto mais
luz, menor a resistência do LDR e **maior** a tensão no nó (por isso o LDR fica em cima, e
não embaixo — com os dois trocados a leitura sobe no escuro e muita gente acha que o
sensor "está invertido").*

![O LDR: componente físico e símbolos esquemáticos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/ldr_componente.png)

*Figura L7-B — O componente que você tem na mão: a superfície serpentada é o material
fotossensível; a resistência cai com a luz. Fonte: Practical Python Programming for IoT
(Packt), cap. 9, Fig. 9.4.*

3. No terminal integrado, crie e acesse o diretório deste experimento:
```bash
mkdir -p ~/sis-emb/lab7/adc_ldr
cd ~/sis-emb/lab7/adc_ldr
idf.py create-project adc_ldr
cd adc_ldr
```

4. Copie o firmware do repositório:
```bash
cp ~/sis-emb-2026-2/semana-07/src/adc_ldr/main.c ~/sis-emb/lab7/adc_ldr/adc_ldr/main/adc_ldr.c
```

5. Abra o arquivo copiado **com a teoria do lado** (seção 1 — ADC e atenuação — e seção 4 —
   média móvel). Código-fonte completo:

```c
// Semana 7 — ADC (LDR em divisor) + media movel O(1) + estatistica simples
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include <stdio.h>
#include <math.h>

#define M 8                      // janela da media movel (1 = sem filtro)
#define FS_HZ 20                 // taxa de amostragem

void app_main(void)
{
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&ucfg, &adc);
    adc_oneshot_chan_cfg_t ccfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_11,          // faixa ~0-3.1 V
    };
    adc_oneshot_config_channel(adc, ADC_CHANNEL_6, &ccfg);   // GPIO34

    int buf[M] = {0}; int idx = 0; long soma = 0;
    double s1 = 0, s2 = 0; int n = 0;
    TickType_t prox = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(1000 / FS_HZ)); // periodo cravado
        int raw;
        adc_oneshot_read(adc, ADC_CHANNEL_6, &raw);

        soma += raw - buf[idx];              // media movel O(1): soma corrente
        buf[idx] = raw;
        idx = (idx + 1) % M;
        int filtrado = soma / M;

        s1 += filtrado; s2 += (double)filtrado * filtrado; n++;
        if (n == FS_HZ) {                    // estatistica a cada 1 s
            double media = s1 / n;
            double dp = sqrt(s2 / n - media * media);
            printf("raw=%4d  filt=%4d  V=%.3f  media_1s=%.1f  desvio=%.1f\n",
                   raw, filtrado, filtrado * 3.1 / 4095.0, media, dp);
            s1 = s2 = 0; n = 0;
        }
    }
}
```

> **Observe**:
>
> três decisões que você já viu na teoria, agora no código. (1) `vTaskDelayUntil` crava os
> 20 Hz: se o período derivasse (jitter, teoria seção 3.5), toda a estatística abaixo
> estaria sobre amostras desigualmente espaçadas. (2) `ADC_ATTEN_DB_11` é a escolha que dá a
> faixa de ~0–3,1 V; é também o que justifica o `3.1 / 4095.0` na conversão para volts (o
> Exemplo 7.1 faz a mesma conta). (3) A média móvel é **O(1)**: em vez de somar M números a
> cada amostra, mantém uma soma corrente — entra o valor novo, sai o mais antigo, e o
> índice anda em círculo (`% M`), o buffer circular da teoria. Note também que o buffer
> **começa zerado**: nas primeiras M amostras `filt` sobe "de baixo" até alcançar o valor
> real, e por isso a **primeira linha de estatística** (e, com M grande, a segunda) vem
> contaminada — descarte-a nas medições da Parte B.

6. Compile, grave e abra o monitor:
```bash
cd ~/sis-emb/lab7/adc_ldr/adc_ldr
idf.py -p /dev/ttyUSB0 flash monitor
```
   No Wokwi, basta colar o código no `main.c` do projeto e iniciar a simulação.

7. Observe a linha impressa a cada 1 s e identifique cada campo:

```
raw=2417  filt=2410  V=1.824  media_1s=2408.3  desvio=3.1
```

| campo | o que é |
|---|---|
| `raw` | código bruto do ADC (0–4095) da **última** amostra do segundo |
| `filt` | saída da média móvel de M amostras — o que o resto do firmware usaria |
| `V` | `filt` convertido em volts: `filt · 3,1 / 4095` |
| `media_1s`, `desvio` | média e desvio-padrão de `filt` nas últimas 20 amostras — o "ruidômetro" |

8. **Calibração contra a teoria** — preencha, cobrindo/iluminando o LDR. A coluna "V
   esperado" usa a sua resistência medida no item 2 e a fórmula da Figura L7-A:
   $V = 3{,}3 \cdot \frac{10\,\text{k}\Omega}{R_{LDR} + 10\,\text{k}\Omega}$.

| condição | R do LDR (multímetro) | V esperado (fórmula) | V medido (`V=`) | raw medido |
|---|---|---|---|---|
| luz da sala (≈ "claro") | | (Ex. 7.2: ~1,8 V) | | |
| lanterna encostada | | (> 2,5 V) | | |
| coberto com a mão (≈ "escuro") | | (Ex. 7.2: ~0,3–0,6 V) | | |

   Calcule também o desvio entre "esperado" e "medido" em cada linha. Ele vem de três
   fontes: tolerância do resistor de 10 kΩ (5 %), não linearidade do ADC perto das pontas
   da faixa (teoria, seção 1.2) e o fato de `V=` usar 3,1 V como fundo de escala. Qual
   dessas fontes pesa mais no seu resultado?

9. O seu LDR não é o do exemplo (8k/120k eram valores nominais); comente no relatório o
   quanto a física real se afastou — e por que isso **não** é um problema para um sistema
   de controle (dica: a semana 13 controla em malha fechada, que tolera sensor
   descalibrado — ela corrige o erro seja qual for a escala exata do sensor, desde que a
   resposta seja monotônica).

## Parte B — Medindo o ruído e o efeito do filtro (30 min)

O firmware imprime, a cada 1 s, média e **desvio-padrão** do sinal filtrado — nosso
"ruidômetro".

10. Com iluminação **constante** (LDR imóvel, sem sombras), anote o `desvio` por ~20 s para
    cada janela `M` (edite o `#define M` do `adc_ldr.c`, regrave) — **descartando a(s)
    primeira(s) linha(s)**, por causa da partida a frio do buffer (item 5):

| M (janela) | desvio típico | atraso teórico (M−1)/2 amostras @ 20 Hz |
|---|---|---|
| 1 (sem filtro) | | 0 ms |
| 4 | | 75 ms |
| 8 | | 175 ms |
| 32 | | 775 ms |

11. O desvio caiu com M na razão ~√M prevista na teoria? (De M = 1 para M = 4, o desvio
    deveria cair à metade; de M = 1 para M = 32, a menos de 20 % — e, para M grande, até
    *mais* que 1/√M, porque o firmware calcula o desvio sobre só 20 amostras e a saída da
    média móvel varia devagar dentro dessa janela de 1 s.) Se não caiu, pense: o
    ruído do seu LDR é mesmo "branco" (independente de amostra para amostra) ou há uma
    oscilação lenta da luz da sala, que a média não consegue remover?

12. E o preço: com M = 32, passe a mão rapidamente sobre o LDR — a resposta ficou
    visivelmente "lenta"? Você acabou de sentir o compromisso ruído × atraso que a malha
    PID da semana 13 terá de negociar.

![Esquerda: degrau de luminosidade filtrado com M de 1, 4 e 32, mostrando que a janela maior atrasa a resposta em cerca de 775 ms. Direita: desvio-padrão caindo com raiz de M e atraso crescendo linearmente com M](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab07_ruido_vs_janela.png)

*Figura L7-C — O compromisso que você mediu nos itens 10–12, numa **simulação**
(ruído branco de ~14 contagens, taxa de 20 Hz): à direita, o desvio cai como 1/√M enquanto o
atraso cresce **linearmente** com M; à esquerda, o mesmo degrau de luz (a "mão passando")
visto com M = 1, 4 e 32 — o M = 32 é limpíssimo, mas só "percebe" a mudança ~775 ms depois
(e leva ~1,6 s para assentar). Note ainda a rampa inicial do M = 32 a partir de zero: é a
partida a frio do buffer. Seus números reais serão diferentes; o **formato** das curvas, não.*

13. **Spike sintético** (Exemplo 7.4 ao vivo): com M = 4, dê um "flash" de lanterna o mais
    curto que conseguir. O `filt` mostra o spike atenuado e espalhado por ~4 amostras? Cole
    um trecho do log no relatório. (Pergunta de bônus: se o flash fosse um spike **isolado
    e enorme** — uma única amostra a 4095 —, a média ou a mediana lidaria melhor? Por quê?)

## Parte C — Saturação e limites da faixa (15 min)

14. Tire o resistor de 10 kΩ e ligue o GPIO34 **direto no 3V3** por um instante (3V3, e só
    ele — veja o aviso no topo). Leitura esperada: `raw=4095` cravado — **saturação**
    (teoria, seção 1.2). Tensões entre ≈ 3,1 e 3,3 V "desaparecem" no teto. Anote e devolva
    o circuito ao normal.

![Curvas de transferência do ADC do ESP32 para diferentes atenuações saturando em 4095, e um sensor travado em 0 dB mas acompanhado com 11 dB](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/adc_atenuacao_saturacao.png)

*Figura L7-D — O que você acabou de provocar: cada atenuação satura no seu fundo de escala
(à esquerda). Usamos 11 dB, cujo teto é ~3,1 V; com 0 dB, o teto seria ~1,1 V e o mesmo
sensor ficaria "travado" em 4095 (à direita) — o diagnóstico clássico de "sensor que não
muda".*

15. Pergunta de projeto para o relatório: se o seu sensor entregasse 0–5 V, o que fazer
    antes do ADC? (Duas respostas válidas: divisor de tensão — a semana 3 — ou ADC externo
    de faixa maior; você reverá ambas no HC-SR04 da semana 12.) Se escolher o divisor,
    calcule **dois resistores iguais** (por exemplo, 10 kΩ + 10 kΩ): quanto sobra na faixa
    do ADC quando o sensor está em 5 V? E o que isso faz com o LSB efetivo, em volts do
    sensor?

## Parte D — DAC: a senoide e sua escada (30 min)

> 🔌 **Esta parte precisa da placa real:** o DAC do ESP32 é um recurso de hardware (GPIO25
> e GPIO26) e o Wokwi não reproduz fielmente a tensão analógica de saída.

16. No terminal integrado, crie e acesse o diretório deste experimento:
```bash
mkdir -p ~/sis-emb/lab7/dac_senoide
cd ~/sis-emb/lab7/dac_senoide
idf.py create-project dac_senoide
cd dac_senoide
```

17. Copie o firmware do repositório:
```bash
cp ~/sis-emb-2026-2/semana-07/src/dac_senoide/main.c ~/sis-emb/lab7/dac_senoide/dac_senoide/main/dac_senoide.c
```

18. Abra o arquivo copiado **com a teoria do lado** (seção 5 — DAC por tabela). Código-fonte
    completo:

```c
// Semana 7 — DAC: senoide de 100 Hz com 32 pontos/ciclo (GPIO25)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_timer.h"
#include <math.h>

#define NPTS 32
#define F_HZ 100

static dac_oneshot_handle_t s_dac;
static uint8_t s_tab[NPTS];
static volatile int s_i = 0;

static void tick_cb(void *arg)               // chamado a NPTS*F_HZ = 3200 Hz
{
    dac_oneshot_output_voltage(s_dac, s_tab[s_i]);
    s_i = (s_i + 1) % NPTS;
}

void app_main(void)
{
    for (int i = 0; i < NPTS; i++)           // tabela pre-computada (economiza CPU)
        s_tab[i] = (uint8_t)(127.5 + 127.5 * sin(2 * M_PI * i / NPTS));

    dac_oneshot_config_t cfg = { .chan_id = DAC_CHAN_0 };   // GPIO25
    dac_oneshot_new_channel(&cfg, &s_dac);

    const esp_timer_create_args_t targs = { .callback = tick_cb, .name = "dac" };
    esp_timer_handle_t t;
    esp_timer_create(&targs, &t);
    esp_timer_start_periodic(t, 1000000 / (NPTS * F_HZ));   // ~312 us
}
```

> **Observe**:
>
> o `sin()` roda **32 vezes, uma só vez**, no início; dentro do callback do timer só há uma
> consulta à tabela e um incremento circular de índice — barato o bastante para rodar a
> 3200 Hz sem pesar na CPU. O `app_main` retorna logo depois de armar o timer: quem toca a
> senoide dali em diante é o `esp_timer`, que chama o callback periodicamente. E repare na conta
> da frequência de saída: **f = taxa de atualização ÷ pontos por ciclo = 3200 / 32 =
> 100 Hz**. Para mudar a frequência você mexe em `F_HZ`; para mudar a *qualidade* da
> senoide, em `NPTS`.

19. Compile, grave e abra o monitor (a senoide sai pelo GPIO25; o monitor não imprimirá
    nada — é normal):
```bash
cd ~/sis-emb/lab7/dac_senoide/dac_senoide
idf.py -p /dev/ttyUSB0 flash monitor
```

![Esquerda: dois ciclos da senoide de 100 Hz do DAC, mostrando os 32 degraus por ciclo de 312 microssegundos cada, com eixo de código 0 a 255 e de tensão 0 a 3,3 V. Direita: a senoide com nível CC de 1,65 V e amplitude A, explicando que o valor eficaz em VAC é A dividido por raiz de 2](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab07_dac_escada.png)

*Figura L7-E — O que sai do GPIO25. À esquerda: a "senoide" é uma escada de 32 degraus por
ciclo, cada um com ~312 µs (é a Figura 7-M da teoria no tempo). À direita: a senoide fica
em torno de 1,65 V (nível CC) com amplitude A = 1,65 V — por isso o multímetro em VDC lê
~1,65 V e, em VAC, só enxerga a parte alternada: V_rms = A/√2.*

20. **Vendo**: se a bancada tiver osciloscópio, observe a escada de 32 degraus. Sem
    osciloscópio, o multímetro em VAC lê o valor eficaz (~1,1–1,2 V para senoide de
    amplitude 1,65 V — senoide: V_rms = A/√2 = 1,65/1,41 ≈ 1,17 V; anote). Em VDC, o que
    ele deveria ler? (Figura L7-E, painel direito.)

21. **Ouvindo** (opcional): buzzer passivo (ou fone) em série com 220 Ω entre GPIO25 e GND.
    100 Hz é um grave; mude `F_HZ` para 440 (o lá musical) e ouça. Agora reduza `NPTS` para
    8 mantendo 440 Hz: o timbre fica áspero (harmônicos da escada — questão 5 da Lista 3
    respondida pelo ouvido). Anote o período do `esp_timer` nos dois casos: a 440 Hz com 8
    pontos o callback roda a quantos kHz?

## Parte E — Loopback DAC → ADC: o aliasing ao vivo (15 min, opcional)

Antes você precisaria de duas placas para ligar um DAC a um ADC. Não precisa: o ESP32 tem os
dois, e o DAC (GPIO25) pode alimentar o ADC (GPIO34) **na mesma placa**, com um único
jumper. Assim o DAC gera uma senoide de frequência conhecida e o ADC a amostra a
$f_s$ = 20 Hz (frequência de Nyquist = 10 Hz) — é a seção 3 da teoria, em bancada.

22. Ligue um jumper **GPIO25 → GPIO34** (tire o circuito do LDR do GPIO34 para não
    competir com o DAC). Crie o projeto e copie o firmware:
```bash
mkdir -p ~/sis-emb/lab7/loopback
cd ~/sis-emb/lab7/loopback
idf.py create-project loopback
cd loopback
cp ~/sis-emb-2026-2/semana-07/src/loopback_alias/main.c ~/sis-emb/lab7/loopback/loopback/main/loopback.c
```

23. Código-fonte completo (junta o DAC da Parte D com o ADC da Parte A):

```c
// Semana 7E — loopback DAC -> ADC na MESMA placa: ver o aliasing acontecer
// Jumper: GPIO25 (saida do DAC) ligado ao GPIO34 (entrada do ADC).
// O DAC gera uma senoide de F_HZ; o ADC a amostra a FS_HZ. Mude F_HZ e compare
// o que o firmware PREVE com o que o ADC realmente enxerga (teoria, secao 3).
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include <math.h>
#include <stdio.h>

#define NPTS  32
#define F_HZ  2          // frequencia gerada pelo DAC (tente 2, 8, 95 e 100)
#define FS_HZ 20         // taxa de amostragem do ADC (frequencia de Nyquist = 10 Hz)

static dac_oneshot_handle_t s_dac;
static uint8_t s_tab[NPTS];
static volatile int s_i = 0;

static void tick_cb(void *arg)               // atualiza o DAC a NPTS*F_HZ
{
    dac_oneshot_output_voltage(s_dac, s_tab[s_i]);
    s_i = (s_i + 1) % NPTS;
}

void app_main(void)
{
    for (int i = 0; i < NPTS; i++)           // amplitude 100 (e nao 127,5): fica em ~0,35-2,9 V,
        s_tab[i] = (uint8_t)(127.5 + 100.0 * sin(2 * M_PI * i / NPTS));   // abaixo do teto do ADC

    dac_oneshot_config_t dcfg = { .chan_id = DAC_CHAN_0 };      // GPIO25
    dac_oneshot_new_channel(&dcfg, &s_dac);

    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&ucfg, &adc);
    adc_oneshot_chan_cfg_t ccfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_11 };
    adc_oneshot_config_channel(adc, ADC_CHANNEL_6, &ccfg);      // GPIO34

    // o periodo do timer e inteiro em us, entao a frequencia REAL difere um pouco da nominal
    int per_us = 1000000 / (NPTS * F_HZ);
    double f_real = 1e6 / per_us / NPTS;
    double k = round(f_real / FS_HZ);
    double f_alias = fabs(f_real - k * FS_HZ);               // formula da teoria, secao 3.3
    printf("DAC gera %.2f Hz | ADC amostra a %d Hz (Nyquist = %.1f Hz) | alias previsto = %.2f Hz\n",
           f_real, FS_HZ, FS_HZ / 2.0, f_alias);

    const esp_timer_create_args_t targs = { .callback = tick_cb, .name = "dac" };
    esp_timer_handle_t t;
    esp_timer_create(&targs, &t);
    esp_timer_start_periodic(t, per_us);

    TickType_t prox = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(1000 / FS_HZ));   // periodo cravado (teoria, secao 3.5)
        int raw;
        adc_oneshot_read(adc, ADC_CHANNEL_6, &raw);
        printf("%d\n", raw);                                   // um numero por linha: serve ao Serial Plotter
    }
}
```

> **Observe**:
>
> a senoide do DAC usa amplitude 100 (não 127,5) para ficar em ~0,35–2,9 V, **abaixo do
> teto de ~3,1 V** do ADC com 11 dB: caso contrário o topo da senoide saturaria
> (teoria 1.2) e você confundiria saturação com aliasing. E o firmware **calcula a
> previsão antes de o experimento rodar** — a fórmula
> $f_{alias} = |f - k\,f_s|$ da seção 3.3 está literalmente no código (`f_alias`). Sua
> tarefa é conferir se o ADC concorda. O programa usa a frequência *real* do DAC
> (`f_real`), porque o período do timer é inteiro em microssegundos e a frequência
> efetiva escapa um pouquinho da nominal (95 Hz vira ≈ 95,27 Hz).

24. Grave com `F_HZ` = 2, 8, 95 e 100 (um de cada vez; regrave a cada mudança). Para cada
    um, leia a **primeira linha** do monitor (a previsão) e depois observe a sequência de
    números — o mais fácil é abrir o **Serial Plotter** (VS Code: extensão de plotter
    serial, ou `idf.py monitor` e copiar ~100 linhas para uma planilha) e medir o período
    do que aparece. Preencha:

| `F_HZ` | frequência real do DAC | amostras por ciclo ($f_s/f$) | alias previsto | o que você vê no gráfico |
|---|---|---|---|---|
| 2 | 2,00 Hz | 10 | — (abaixo de Nyquist) | uma senoide lenta e fiel |
| 8 | 8,00 Hz | 2,5 | — (abaixo, mas perto do limite) | |
| 95 | ≈ 95,27 Hz | 0,21 | ≈ 4,73 Hz | |
| 100 | ≈ 100,16 Hz | 0,20 | ≈ 0,16 Hz | |

   Resultados esperados: com 2 Hz a senoide aparece bonita (10 amostras por ciclo). Com
   8 Hz ela ainda é **legítima** (abaixo de 10 Hz), mas com só 2,5 amostras por ciclo o
   desenho parece irregular — teorema satisfeito, "fidelidade visual" nem tanto (é o que a
   regra de bolso de 5–10× previne). Com 95 Hz surge uma senoide **lenta de ~5 Hz** que
   *não existe* no sinal: o impostor. E com 100 Hz — um múltiplo exato de $f_s$ — o ADC
   vê quase uma **reta**, que só se mexe devagarzinho (0,16 Hz) porque os dois relógios não
   são exatamente comensuráveis. É o mesmo mecanismo do Exemplo 7.3b (a luz da sala no LDR).

![A dobra de frequências: esquerda, frequência aparente em função da frequência real; direita, exemplo de 600 Hz a 1 kHz aparecendo em 400 Hz](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/aliasing_dobra.png)

*Figura L7-F — O mapa que prevê o que você anotou na tabela (a figura usa $f_s$ = 10 Hz; no
seu experimento $f_s$ = 20 Hz, então o "espelho" fica em 10 Hz). Em qualquer $f_s$, tudo
acima de $f_s/2$ é dobrado de volta para a faixa 0 … $f_s/2$.*

25. Pergunta para o relatório: o que você faria, **no hardware**, para que o ADC nunca visse
    esse impostor de 5 Hz ao medir um sinal que pode ter componentes de 95 Hz? (Pense na
    seção 3.4: filtro RC passa-baixas antes do pino; qual $f_c$ você escolheria para
    $f_s$ = 20 Hz e por que um RC sozinho não seria suficiente?)

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| `raw` sempre 0 ou 4095 | fio no GPIO errado / divisor aberto | confira o nó do divisor no GPIO34 |
| Leitura não muda com luz | LDR e resistor trocados de lugar | LDR em cima (3V3), 10k embaixo |
| Leitura **sobe** no escuro | idem (inversão do divisor) | troque as posições do LDR e do resistor |
| Desvio altíssimo só na 1ª linha | partida a frio do buffer da média móvel (item 5) | descarte a(s) primeira(s) linha(s) |
| Desvio alto com LDR parado | luz fluorescente/LED da sala (100/120 Hz!) | cubra o sensor; discuta no relatório (aliasing — Exemplo 7.3b da teoria) |
| Desvio não cai como √M | ruído não é branco (oscilação lenta da luz, sombra, mão) | meça com o LDR coberto e a sala estável |
| DAC sem saída | GPIO errado (só 25/26 têm DAC) | confira o pino |
| Loopback: gráfico "serrilhado"/cortado no topo | saturação do ADC (sinal passa de ~3,1 V) | confira a amplitude da tabela (100, não 127,5) |
| Loopback: ADC lê ~0 ou valores aleatórios | jumper ausente ou GPIO34 ainda ligado ao LDR | só o jumper GPIO25→GPIO34 deve estar no pino |
| `idf.py` não acha `esp_adc/adc_oneshot.h` | versão antiga do ESP-IDF | use ESP-IDF 5.x (o mesmo dos labs anteriores) |

## Entrega (GitHub da bancada, `lab-07/relatorio.md`)

1. Tabela de calibração (Parte A, item 8, com R medido e desvio calculado) + comentário
   sobre o desvio da teoria (item 9).
2. Tabela de ruído × M (Parte B) + o trecho de log do spike + 3 linhas sobre o compromisso
   ruído/atraso.
3. Registro da saturação (item 14) e resposta de projeto (item 15, com a conta do divisor).
4. Valor eficaz medido (ou descrição do áudio) na Parte D + o valor que o multímetro
   mostrou em VDC.
5. (Opcional, Parte E) A tabela do item 24 preenchida, com o gráfico de pelo menos um dos
   casos (95 ou 100 Hz), e a resposta do item 25.

## Desafio (opcional)

Luxímetro tosco porém honesto: usando dois pontos de calibração (lanterna encostada =
"100 %", coberto = "0 %"), converta a leitura filtrada em percentual de luminosidade e
imprima uma barra ASCII (`[#########.......] 62 %`) atualizada a 5 Hz. Capriche: a barra
não pode "tremer" com o LDR parado (escolha M com critério e justifique).
