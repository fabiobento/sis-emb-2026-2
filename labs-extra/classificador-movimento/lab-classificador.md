# Roteiro — Lab Extra: Classificador de movimento no ESP32 (Edge Impulse + MPU-6050)

Lab integrador **opcional/avançado** (~3 aulas). Você treina um classificador de movimento na
plataforma web Edge Impulse e o roda **na borda**, no ESP32, com o MPU-6050 da semana 9 — fechando
o ciclo **sensor → DSP → inferência → ação**. Ao final, o nó publica a classe detectada por MQTT
no broker do RPi (semana 14) e você mede, com honestidade, **quanto o sistema acerta de verdade**.

**Pré-requisitos:** semanas 5 (tarefas), 7 (amostragem/PDS), 9 (I²C/MPU-6050) e 14 (MQTT). Leia
antes a [teoria-classificador.md](teoria-classificador.md). O mapa geral do projeto, com rubrica, é
a [trilha TinyML](../../docs/trilha-tinyml.md). Quando algo der errado, abra o
[guia-problemas.md](guia-problemas.md). O modelo de relatório está em
[relatorio-modelo.md](relatorio-modelo.md).

> 🧪 **O que foi e o que não foi testado (leia):** a lógica de janela deslizante, o *debounce*, o
> classificador falso, o gravador CSV e o pipeline Python foram testados no PC. **Não foram
> testados** pelo autor deste material: o firmware no ESP32 real, a integração com a biblioteca
> exportada do Edge Impulse, a interface atual do Studio e a CLI. Os passos do Studio seguem a
> documentação oficial, mas **nomes de abas e botões mudam** — se algo divergir, vale o conceito
> (coletar → FFT → treinar → exportar → rodar) e a [documentação oficial](https://docs.edgeimpulse.com/).

## Mapa do lab

![Mapa do lab em seis etapas, da preparação ao painel ao vivo, cada uma com seu entregável](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_roteiro.png)
*Figura L-1 — As etapas deste lab seguem as da [trilha](../../docs/trilha-tinyml.md): 0 preparar,
1 coletar, 2 Impulse, 3 treinar, 4 deploy, 5 ao vivo + MQTT.*

| Parte | Aula | O que você faz | Entregável (vai para o relatório) |
|---|---|---|---|
| 0 | 1 | Ferramentas, conta, repositório, "teste sem hardware" | `pipeline_host.py` rodando com dados sintéticos |
| A | 1 | Ligação do MPU-6050 e firmware de coleta | Print: `WHO_AM_I=0x68` e ~100 linhas/s |
| B | 1 | Gravar e rotular os 4 gestos, com várias pessoas | Tabela de dados (minutos por classe e por pessoa) |
| C | 2 | Montar o *Impulse* e olhar o Feature explorer | Print do explorer + parâmetros |
| D | 2 | Treinar, ler a matriz, testar de verdade | Treino × teste, matriz, RAM/latência estimadas |
| E | 2–3 | Exportar a biblioteca C++ e **teste de sanidade** | Print: Studio × ESP32 batem |
| F | 3 | Sensor ao vivo + *debounce* + MQTT | Serial com `[janela]` e mensagens no `mosquitto_sub` |
| G | 3 | Teste de campo e demonstração | Matriz de confusão **ao vivo** (10 tentativas por gesto) |

**Rótulos das classes** (use exatamente estes, sem acento nem espaço — vão para JSON e para o
código): `parado`, `aceno`, `sobe_desce`, `circulo`. Você pode trocá-las (ex.: estados de vibração
de um motor), mas comece com estas quatro, que são bem separáveis.

## Parte 0 — Preparação (aula 1, ~20 min)

1. **Obtenha o repositório** e entre na pasta do lab:

   ```bash
   cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
   cd labs-extra/classificador-movimento
   ls            # README, teoria, lab, guia-problemas, relatorio-modelo, src/, tools/, tests/
   ```

2. **Ferramentas Python** (para o gravador CSV, o monitor MQTT e o pipeline local):

   ```bash
   python3 -m venv ~/venv-tinyml && source ~/venv-tinyml/bin/activate
   pip install numpy pyserial paho-mqtt
   ```

3. **Edge Impulse CLI.** Siga a [página oficial](https://docs.edgeimpulse.com/tools/clis/edge-impulse-cli)
   (requer Node.js; confira a versão exigida lá) e verifique:

   ```bash
   edge-impulse-data-forwarder --help | head -3
   ```

4. **Conta e projeto.** Entre em <https://studio.edgeimpulse.com>, crie um projeto (ex.:
   `sis-emb-2026-2-bancadaN`) e **anote o *project ID*** — ele vai no relatório. Confirme com o
   professor qual conta/organização usar.

5. **Teste sem hardware (10 min) — entenda o pipeline antes de gravar.** Gere dados *sintéticos*
   (não são reais!) e rode o pipeline local, que reimplementa "janela → FFT → classificador →
   matriz de confusão" em Python:

   ```bash
   python3 tools/gera_sinteticos.py /tmp/sint --pessoas 4
   python3 tools/pipeline_host.py /tmp/sint
   ```

   Saída esperada (formato): número de gravações e janelas, **acurácia treino × teste** e a matriz.
   Com dados sintéticos tão "limpos" o teste dá perto de 100% — **isso não vale nada** como
   resultado: serve para ver a máquina funcionando. Repita com `--split janela` e compare (é o
   vazamento da Figura C-5/C-6 da teoria).

   > ❓ **Pergunta 0:** por que o teste, nesse experimento, dá ~100% mesmo "por pessoa"? O que
   > falta nos dados sintéticos que existe nos gestos reais? (Dica: teoria, §6.)

## Parte A — Hardware e firmware de coleta (aula 1)

### A.1 Ligação

![Ligação ESP32 e MPU-6050: 3V3, GND, GPIO21 (SDA), GPIO22 (SCL), com AD0 ligado a GND para o endereço 0x68](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_fiacao.png)
*Figura L-2 — A mesma ligação do Lab 9. Se o seu módulo já funcionou no Lab 9, não mexa.*

Antes de seguir, confirme que o sensor responde: o `i2cdetect`/scanner da semana 9 deve achar
`0x68`. **Se o sensor não lê, nada à frente funciona.**

### A.2 O firmware de coleta

`src/coleta_mpu/main.c` lê o acelerômetro e imprime **uma linha `aX,aY,aZ` por amostra, em g**, a
100 Hz cravados. Mudanças em relação ao Lab 9 e por quê:

| Mudança | Por quê |
|---|---|
| `vTaskDelayUntil` (referência absoluta) | período uniforme → espectro limpo (teoria §3.1) |
| Filtro **DLPF** ligado (`0x1A = 3`) | anti-aliasing antes de amostrar a 100 Hz (teoria §3.2) |
| Só `aX,aY,aZ` em cada linha | formato que o *data forwarder* e o `gravar_csv.py` esperam |
| Linhas de diagnóstico começam com `#` | ignoradas pelo parser; mostram `WHO_AM_I`, fs, range |
| Em erro de I²C, repete a última amostra | mantém a taxa; o erro não vira buraco no dado |

<details>
<summary><code>src/coleta_mpu/main.c</code> (77 linhas) — clique para abrir</summary>

```c
// Lab Extra — Classificador de movimento (ETAPA DE COLETA)
// Deriva de semana-09/src/mpu6050. Le o acelerometro do MPU-6050 por I2C e imprime
// APENAS os 3 eixos (em g), uma linha "aX,aY,aZ" por amostra, com periodo CRAVADO
// (vTaskDelayUntil, semana 5). Esse e o formato que o edge-impulse-data-forwarder
// e o tools/gravar_csv.py esperam.
//
// REGRA DE OURO: estas configuracoes (taxa, fundo de escala, filtro DLPF, ordem e
// unidade dos eixos) tem de ser IDENTICAS no firmware de inferencia. Se mudar aqui,
// mude la — senao o modelo recebe "outra coisa" (erro n. 1 de integracao).
//
// Uso:
//   idf.py -p /dev/ttyUSB0 flash monitor     (confira ~100 linhas/s; sai com Ctrl+])
//   feche o monitor e rode:  edge-impulse-data-forwarder --frequency 100
//   nomes dos eixos: accX,accY,accZ
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include <stdio.h>

// ---------------- configuracao (manter igual ao firmware de inferencia) ----------------
#define SDA        21
#define SCL        22
#define ADDR       0x68      // AD0 = GND
#define FS_HZ      100       // Nyquist: > 2x a maior freq. util do gesto (~10-15 Hz)
#define MPU_RANGE  0         // 0:+-2g  1:+-4g  2:+-8g  3:+-16g  (se saturar em +-2,0 g, suba)
#define MPU_DLPF   3         // 3 => banda do acelerometro ~44 Hz (anti-aliasing p/ fs=100 Hz)
// ----------------------------------------------------------------------------------------
#define REG_DLPF    0x1A     // CONFIG: DLPF_CFG nos 3 bits baixos
#define REG_ACFG    0x1C     // ACCEL_CONFIG: AFS_SEL nos bits 4:3
#define REG_ACCEL   0x3B     // ax..az nos 6 primeiros bytes (big-endian)
#define REG_PWR     0x6B
#define REG_WHOAMI  0x75
#define LSB_POR_G   (16384.0f / (1 << MPU_RANGE))

static esp_err_t wr(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_write_to_device(I2C_NUM_0, ADDR, b, 2, pdMS_TO_TICKS(50));
}
static esp_err_t rd(uint8_t reg, uint8_t *dst, size_t n)
{
    return i2c_master_write_read_device(I2C_NUM_0, ADDR, &reg, 1, dst, n, pdMS_TO_TICKS(50));
}

void app_main(void)
{
    i2c_config_t cfg = { .mode = I2C_MODE_MASTER, .sda_io_num = SDA, .scl_io_num = SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000 };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    uint8_t who = 0;
    rd(REG_WHOAMI, &who, 1);
    // Linhas que NAO sao "num,num,num" sao ignoradas pelo forwarder; servem de diagnostico.
    printf("# WHO_AM_I=0x%02X (esperado 0x68) fs=%d Hz range=%d dlpf=%d\n", who, FS_HZ, MPU_RANGE, MPU_DLPF);

    wr(REG_PWR, 0x00);                          // sai do sleep (oscilador interno)
    wr(REG_DLPF, MPU_DLPF);                     // filtro passa-baixas interno (semana 7: anti-aliasing)
    wr(REG_ACFG, MPU_RANGE << 3);               // fundo de escala

    const TickType_t periodo = pdMS_TO_TICKS(1000 / FS_HZ);   // 10 ticks (CONFIG_FREERTOS_HZ=1000)
    TickType_t prox = xTaskGetTickCount();      // referencia absoluta: sem deriva acumulada
    float ult[3] = { 0, 0, 1 };                 // ultima amostra boa (repetida se o I2C falhar)

    while (1) {
        uint8_t b[6];
        if (rd(REG_ACCEL, b, 6) == ESP_OK) {
            ult[0] = (int16_t)((b[0] << 8) | b[1]) / LSB_POR_G;
            ult[1] = (int16_t)((b[2] << 8) | b[3]) / LSB_POR_G;
            ult[2] = (int16_t)((b[4] << 8) | b[5]) / LSB_POR_G;
        }                                       // se falhar: repete a anterior e mantem a taxa
        printf("%.4f,%.4f,%.4f\n", ult[0], ult[1], ult[2]);
        vTaskDelayUntil(&prox, periodo);
    }
}
```

</details>

### A.3 Grave e confira

```bash
cd src/coleta_mpu   # (ou copie main.c para um projeto ESP-IDF novo, como nos labs anteriores)
idf.py -p /dev/ttyUSB0 flash monitor      # sai com Ctrl+]
```

No monitor você deve ver `# WHO_AM_I=0x68 …` e depois linhas como `0.0123,-0.0456,0.9876`.
Parado sobre a mesa: `aZ ≈ 1,00` e `aX`, `aY` perto de 0. Incline a placa: a gravidade se
redistribui entre os eixos (semana 9).

> ✅ **Checkpoint A:** `WHO_AM_I=0x68`; `aZ ≈ 1 g` parado; os três eixos mudam ao mexer na placa.

### A.3b Justifique a taxa (vai no relatório)

Escreva, em 3–4 linhas, por que 100 Hz e não 10 Hz nem 1 kHz (Exemplo C.1) e por que
`vTaskDelayUntil` (§3.1 da teoria). **Feche o monitor** antes da Parte B: a porta
serial só aceita um programa por vez.

## Parte B — Coletar e rotular os dados (aula 1)

### B.1 Os gestos e a orientação do sensor

![Os quatro gestos desenhados em relação aos eixos X, Y e Z do sensor](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_gestos.png)
*Figura L-3 — Os quatro gestos e os eixos. **Segure o sensor sempre na mesma orientação** ao
gravar (por exemplo: face para cima, eixo X apontando para a direita), ou fixe-o numa pulseira /
caixinha. Orientação diferente = dado diferente.*

**Protocolo de gravação** (combine antes, anote no relatório):

| Item | Valor sugerido | Por quê |
|---|---|---|
| Duração de cada gravação | 10 s | dá ~41 janelas de 2 s (Exemplo C.4) |
| Por classe e por pessoa | ≥ 6 gravações (~1 min) | variedade > quantidade |
| Por classe, total | ≥ 3 min, de **≥ 3 pessoas** | generalização |
| Variações dentro da classe | ritmo e amplitude diferentes; mão esquerda/direita | evita decorar um jeito só |
| Pessoa reservada para **teste** | 1 pessoa, que **não** entra no treino | teste honesto (Figura C-5) |
| `parado` | sensor na mão, quieto; e também sobre a mesa | é a classe de "nada" — a mais usada na demo |

### B.2 Caminho 1 — *Data forwarder* (direto da serial para o Studio)

Com o firmware de coleta rodando e o monitor **fechado**:

```bash
edge-impulse-data-forwarder --frequency 100
```

Ele pede login, o projeto e os **nomes dos eixos**: informe `accX,accY,accZ`. O ESP32 passa a
aparecer em *Devices*. Em *Data acquisition* escolha o dispositivo, o **rótulo** (`aceno`…), o
*Sample length* (10000 ms) e clique em *Start sampling*; faça o gesto durante os 10 s. Se errar o
projeto ou os eixos, rode de novo com `--clean`.

### B.3 Caminho 2 — Gravar em CSV no PC e enviar (recomendado: você fica com cópia local)

```bash
python3 tools/gravar_csv.py --porta /dev/ttyUSB0 --classe aceno --pessoa p1 --n 6 --duracao 10
python3 tools/gravar_csv.py --porta /dev/ttyUSB0 --classe sobe_desce --pessoa p1 --n 6
#  … uma classe por vez; troque --pessoa p2, p3 quando mudar de integrante
```

O script mostra a contagem regressiva, grava `dados/<classe>.<pessoa>.<nn>.csv` e **confere a taxa
medida**: ele avisa se ficar fora de ±3% de 100 Hz (cabo/firmware) e se houver **amostras
saturadas** em ±2 g (suba `MPU_RANGE`, teoria §3.3).

> Rode `python3 tools/pipeline_host.py dados` a qualquer momento: ele treina um classificador
> simples com as **suas** gravações, reservando a última pessoa para teste. Se esse baseline já
> acerta pouco, o problema está nos **dados** (gestos parecidos, poucas pessoas), não no Studio.

**Enviar ao Studio.** Duas opções (confira o formato exigido na documentação do
[uploader](https://docs.edgeimpulse.com/tools/clis/edge-impulse-cli/uploader)):

```bash
# CLI: o rotulo vem de --label; a categoria separa treino e teste
edge-impulse-uploader --category training --label aceno dados/aceno.p[12].*.csv
edge-impulse-uploader --category testing  --label aceno dados/aceno.p3.*.csv   # pessoa p3 = teste
```

ou *Data acquisition → Upload data* na interface web. O Studio pode abrir o **CSV Wizard** na
primeira vez: indique a coluna `timestamp` (em ms) e as colunas `accX,accY,accZ`, e a frequência
de 100 Hz. Repita para as quatro classes. (⚠️ Os nomes de arquivo `classe.pessoa.nn.csv` seguem o
padrão `rótulo.resto.csv`; confirme na documentação do uploader se a sua versão infere o rótulo
do nome ou exige `--label`.)

### B.4 Confira o que está no Studio

Em *Data acquisition* verifique: **(1)** cada classe com minutos de dado e de **mais de uma
pessoa**; **(2)** o conjunto *Test* contém a pessoa reservada e **todas as quatro classes**;
**(3)** abra uma amostra: o gráfico mostra 3 eixos e a frequência é 100 Hz.

> ✅ **Checkpoint B:** tabela no relatório com minutos por classe e por pessoa; teste separado
> **por pessoa**. (Se o Studio dividir sozinho 80/20 por *amostra*, ainda é melhor do que por
> janela, mas **reservar uma pessoa inteira é o teste mais honesto** — Figura C-5.)

## Parte C — Montar o *Impulse* (aula 2)

![Mapa das abas do Edge Impulse Studio usadas no lab, da aquisição de dados ao deployment](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_studio.png)
*Figura L-4 — Onde fazer cada coisa no Studio (nomes podem mudar entre versões).*

1. Aba **Create impulse**. *Time series data*: **Window size = 2000 ms**, **Window increase =
   200 ms**, **Frequency = 100 Hz**. O Studio deve listar 3 eixos (`accX, accY, accZ`). Esses
   números têm de ser **os mesmos de `projeto_config.h`** (`JANELA_MS`, `FS_HZ`) — a janela de 2 s
   a 100 Hz são as 200 amostras do `static_assert` do firmware.
2. *Add a processing block* → **Spectral Features** (a FFT). *Add a learning block* →
   **Classification**. Salve (*Save Impulse*).
3. Aba **Spectral features**: comece com os **parâmetros padrão** e clique em *Generate features*.
   Dois parâmetros merecem entender, não só aceitar: o **comprimento da FFT** (resolução em
   frequência, Exemplo C.2) e o **filtro** (comece sem filtro; o DLPF do sensor já atuou).
4. Abra o **Feature explorer** e compare com a Figura C-8: as classes formam nuvens separadas?

> ✅ **Checkpoint C:** *Impulse* salvo com 2000 ms / 200 ms / 100 Hz; Feature explorer
> **com print no relatório** e uma frase sobre quais classes ficam próximas.

## Parte D — Treinar e avaliar com honestidade (aula 2)

1. Aba **Classifier**: treine com os parâmetros padrão. Leia: **acurácia de validação**, a **matriz
   de confusão** e a estimativa de **RAM / flash / latência** para o ESP32.
2. Aba **Model testing**: *Classify all* no conjunto de **teste** (a pessoa reservada). **Esta é a
   acurácia honesta.** Registre:

   | Medida | Valor |
   |---|---|
   | Acurácia de validação (treino) | |
   | Acurácia no *Model testing* | |
   | Classe com pior *recall* | |
   | Par de classes mais confundido | |
   | RAM / flash / latência estimadas (Studio) | |

3. Interprete pelos Exemplos C.5 (treino × teste) e C.6 (matriz → métricas).
4. **Iteração disciplinada.** Mude **uma coisa por vez** e anote num log:

   | # | O que mudei | Treino | Teste | Observação |
   |---|---|---|---|---|
   | 1 | padrão | | | |
   | 2 | + gravações da pessoa p4 | | | |

   Ideias: mais pessoas; gestos mais distintos; janela 1500 ou 3000 ms (e **atualizar o firmware**);
   menos épocas; classe `outro`.

> ⚠️ Treino ≈ 100% e teste ≪ treino = overfitting. Teste perto de 100% **sem** pessoa reservada =
> suspeito de vazamento. Compare com `pipeline_host.py dados`.

> ✅ **Checkpoint D:** tabela acima preenchida e log de iterações com ≥ 2 linhas.

## Parte E — Exportar e provar que o ESP32 vê o mesmo que o Studio (aula 2–3)

1. Aba **Deployment** → **C++ library** → *Build* → baixe o `.zip`. (Se o Studio oferecer
   otimização/quantização, anote qual escolheu e a latência/RAM que ele mostra.)
2. Clone o repositório-base do Edge Impulse e copie as pastas do `.zip` (`edge-impulse-sdk/`,
   `model-parameters/`, `tflite-model/`) para a **raiz** dele — [doc oficial](https://docs.edgeimpulse.com/hardware/deployments/run-cpp-espressif-esp32):

   ```bash
   git clone https://github.com/edgeimpulse/example-standalone-inferencing-espressif-esp32 ~/ei-movimento
   cp -r edge-impulse-sdk model-parameters tflite-model ~/ei-movimento/
   cd ~/ei-movimento && ls      # CMakeLists.txt  main/  sdkconfig ... (projeto ESP-IDF comum)
   ```

3. **Teste de sanidade com amostra fixa.** No Studio, aba *Live classification* (ou numa amostra do
   conjunto de teste), copie as **Raw features** de uma janela e cole no `static const float features[]`
   do arquivo principal do exemplo (`main/main.cpp` ou `main/ei_main.cpp`, conforme a versão do
   repositório). Então:

   ```bash
   idf.py set-target esp32      # só na primeira vez; confira o README do exemplo
   idf.py build flash monitor   # 115200 baud
   ```

   O ESP32 imprime algo como (formato aproximado):

   ```
   Predictions (DSP: 5 ms., Classification: 1 ms., Anomaly: 0 ms.):
       aceno: 0.93750
       circulo: 0.01172
       parado: 0.00000
       sobe_desce: 0.05078
   ```

   **A classe e as probabilidades devem bater com as do Studio**, salvo diferenças de arredondamento
   (da ordem de centésimos). Se **não** batem, a biblioteca não está integrada corretamente e
   **não adianta** ligar o sensor: veja o [guia de problemas](guia-problemas.md), seção "sanidade".

> ✅ **Checkpoint E:** print do Studio e do monitor do ESP32 lado a lado, mesma janela, mesma
> classe. Anote também os **tempos medidos** (`DSP`, `Classification`) × os estimados no Studio.

## Parte F — Sensor ao vivo + *debounce* + MQTT (aula 3)

![Arquitetura do firmware: amostra_task, buffer circular, inferencia_task, debounce e publicação MQTT](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_firmware.png)
*Figura L-5 — O firmware final. Leia a Figura C-10 e o §10 da teoria antes de mexer.*

### F.1 Os arquivos

Copie `src/inferencia_mqtt/main/*` para a pasta `main/` do seu projeto `~/ei-movimento`
(substituindo o arquivo principal do exemplo pelo `main.cpp` deste lab e **acrescentando** os
`.h`). Depois ajuste o `main/CMakeLists.txt` **do exemplo** (não o substitua; o arquivo
`src/inferencia_mqtt/main/CMakeLists.txt` é só o trecho de referência) para listar o novo
`main.cpp` e incluir os componentes `esp_wifi nvs_flash mqtt esp_event esp_netif driver esp_timer`.

Edite **`projeto_config.h`**: Wi-Fi, IP do broker (`hostname -I` no RPi) e `BANCADA`. Os
parâmetros de sensor e janela **têm de ser os mesmos da coleta e do Studio**; se não forem, o
`static_assert` do `classificador.h` **quebra a compilação** com uma mensagem clara (essa é a
função dele).

<details>
<summary><code>src/inferencia_mqtt/main/projeto_config.h</code> (37 linhas) — clique para abrir</summary>

```c
// projeto_config.h — TUDO que precisa casar com o Studio e com a rede fica aqui.
#ifndef PROJETO_CONFIG_H
#define PROJETO_CONFIG_H

// ---- Rede (como na semana 14) ----
#define WIFI_SSID   "MINHA_REDE"
#define WIFI_PASS   "MINHA_SENHA"
#define BROKER_URI  "mqtt://192.168.0.10"        // IP do RPi (hostname -I)
#define BANCADA     "bancada1"                   // troque pelo numero da sua bancada
#define T_STATUS    "ifes/" BANCADA "/status"    // LWT: online/offline (retido)
#define T_MOV       "ifes/" BANCADA "/movimento" // {"classe":"aceno","conf":0.93,...}
#define T_INFO      "ifes/" BANCADA "/movimento/info"  // latencias, a cada INFO_A_CADA

// ---- Sensor (IGUAL ao firmware de coleta) ----
#define PIN_SDA    21
#define PIN_SCL    22
#define MPU_ADDR   0x68
#define MPU_RANGE  0     // 0:+-2g 1:+-4g 2:+-8g 3:+-16g
#define MPU_DLPF   3

// ---- Janela (IGUAL ao Impulse no Studio) ----
#define FS_HZ        100          // Studio: Frequency (Hz)
#define JANELA_MS    2000         // Studio: Window size (ms)
#define JANELA_AMOSTRAS (FS_HZ * JANELA_MS / 1000)   // 200
#define STRIDE_MS    500          // so no firmware: de quanto em quanto tempo classificar
#define STRIDE_AMOSTRAS (FS_HZ * STRIDE_MS / 1000)   // 50

// ---- Decisao ----
#define N_CONFIRMA   3            // janelas seguidas iguais para mudar a saida
#define LIMIAR_CONF  0.80f        // confianca minima por janela
#define INFO_A_CADA  20           // publica /info a cada 20 janelas (~10 s)

// ---- Atuador local opcional (semana 8): LED liga quando a classe e CLASSE_ATUA ----
#define PINO_LED     2
#define CLASSE_ATUA  "circulo"

#endif
```

</details>

### F.2 Os blocos de lógica (puros, testáveis no PC)

O buffer circular e o *debounce* ficaram em arquivos C puros, **sem ESP-IDF**, de propósito: dá
para testá-los no PC antes de gravar no chip.

<details>
<summary><code>src/inferencia_mqtt/main/janela.h</code> (50 linhas) — clique para abrir</summary>

```c
// janela.h — janela deslizante (buffer circular) para o classificador de movimento.
// C puro, sem dependencia de ESP-IDF: testavel no PC (tests/test_logica.c).
//
//   push(amostra) ──▶ [ . . . . . . . . . . ]  buffer circular com 'cap' amostras
//                        ▲ head (proxima escrita)
//   a cada 'stride' amostras novas, com o buffer ja cheio, push() devolve 1:
//   "ha uma janela nova para classificar". janela_copia() entrega a janela em ORDEM
//   CRONOLOGICA (mais antiga primeiro), intercalada x,y,z,x,y,z,... — o layout que o
//   Edge Impulse espera em EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME = 3.
#ifndef JANELA_H
#define JANELA_H
#include <string.h>

#define EIXOS 3

typedef struct {
    float *dados;       // cap * EIXOS floats (fornecido pelo chamador)
    int    cap;         // amostras por janela (ex.: 200 = 2 s a 100 Hz)
    int    stride;      // a cada quantas amostras novas gera-se uma janela (ex.: 50 = 0,5 s)
    int    head;        // indice (em amostras) da proxima escrita
    long   total;       // amostras recebidas desde o inicio
    int    desde;       // amostras recebidas desde a ultima janela entregue
} janela_t;

static inline void janela_init(janela_t *j, float *mem, int cap, int stride)
{
    j->dados = mem; j->cap = cap; j->stride = stride;
    j->head = 0; j->total = 0; j->desde = 0;
    memset(mem, 0, sizeof(float) * cap * EIXOS);
}

// Retorna 1 quando uma nova janela esta pronta (buffer cheio E passou o stride).
static inline int janela_push(janela_t *j, const float a[EIXOS])
{
    memcpy(&j->dados[j->head * EIXOS], a, sizeof(float) * EIXOS);
    j->head = (j->head + 1) % j->cap;
    j->total++;
    j->desde++;
    if (j->total >= j->cap && j->desde >= j->stride) { j->desde = 0; return 1; }
    return 0;
}

// Copia a janela atual em ordem cronologica para dst[cap * EIXOS].
static inline void janela_copia(const janela_t *j, float *dst)
{
    int n1 = j->cap - j->head;                       // amostras do head ate o fim do vetor
    memcpy(dst, &j->dados[j->head * EIXOS], sizeof(float) * n1 * EIXOS);
    memcpy(dst + n1 * EIXOS, j->dados, sizeof(float) * j->head * EIXOS);
}
#endif
```

</details>

<details>
<summary><code>src/inferencia_mqtt/main/debounce.h</code> (27 linhas) — clique para abrir</summary>

```c
// debounce.h — "debounce" de decisoes de ML (semana 3, agora aplicado a classes).
// Regra: so muda o estado publicado quando N janelas CONSECUTIVAS escolhem a mesma classe
// com confianca >= limiar. Janela incerta (conf < limiar) zera a contagem mas NAO muda o estado.
// C puro, testavel no PC.
#ifndef DEBOUNCE_H
#define DEBOUNCE_H

typedef struct {
    int   estado;     // classe atualmente publicada (-1 = ainda indefinida)
    int   cand;       // classe candidata
    int   cont;       // quantas janelas seguidas confirmaram a candidata
    int   n;          // N exigido (ex.: 3)
    float limiar;     // confianca minima (ex.: 0,80)
} debounce_t;

static inline void debounce_init(debounce_t *d, int n, float limiar)
{ d->estado = -1; d->cand = -1; d->cont = 0; d->n = n; d->limiar = limiar; }

// Alimenta com o resultado de UMA janela. Retorna 1 se o estado publicado mudou.
static inline int debounce_update(debounce_t *d, int classe, float conf)
{
    if (conf < d->limiar) { d->cand = -1; d->cont = 0; return 0; }   // incerto: ignora
    if (classe == d->cand) d->cont++; else { d->cand = classe; d->cont = 1; }
    if (d->cont >= d->n && d->cand != d->estado) { d->estado = d->cand; return 1; }
    return 0;
}
#endif
```

</details>

Teste no PC (deve imprimir `OK`):

```bash
gcc -Wall -Wextra -o /tmp/test_logica tests/test_logica.c -I src/inferencia_mqtt/main && /tmp/test_logica
g++ -Wall -DUSAR_EI=0 -o /tmp/test_stub tests/test_stub.cpp -I src/inferencia_mqtt/main -lm && /tmp/test_stub
```

O segundo teste alimenta a cadeia janela → classificador falso → *debounce* com 10 s "parado" e 10 s
"sacudindo": deve haver **2 transições** (`parado` → `movendo`).

> ❓ **Pergunta F.1:** no `test_logica`, a primeira janela só aparece na amostra 8 (= `cap`). No
> firmware real, em que instante (em segundos) sai a primeira classificação? E por que a segunda
> sai 0,5 s depois? (Dica: teoria §2.1.)

### F.3 A ponte com o modelo e o firmware principal

<details>
<summary><code>src/inferencia_mqtt/main/classificador.h</code> (82 linhas) — clique para abrir</summary>

```c
// classificador.h — fronteira entre o seu firmware e o modelo.
//
//   USAR_EI = 1 : usa a biblioteca C++ exportada do Edge Impulse (run_classifier).
//   USAR_EI = 0 : usa um classificador FALSO por energia (so para testar o resto da cadeia:
//                 sensor -> janela -> debounce -> Wi-Fi/MQTT, antes de ter um modelo).
//
// Interface unica:   int clf_classifica(const float *janela, float *prob, float *ms_dsp, float *ms_clf)
//   janela : JANELA_AMOSTRAS*3 floats, x,y,z intercalados, ordem cronologica, em g
//   prob   : CLF_N_CLASSES floats (soma ~1)
//   retorna: indice da classe de maior probabilidade, ou -1 em caso de erro
#ifndef CLASSIFICADOR_H
#define CLASSIFICADOR_H
#include <math.h>
#include <stdio.h>
#include "projeto_config.h"

#ifndef USAR_EI
#define USAR_EI 1
#endif

#if USAR_EI
// ---------------------------------------------------------------------------------------
// Headers gerados pela biblioteca exportada (pastas edge-impulse-sdk/ e model-parameters/).
// Nomes conforme a documentacao do Edge Impulse; confira na SUA versao da biblioteca.
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "model-parameters/model_metadata.h"

// Trava de compilacao: o firmware tem de casar com o Impulse do Studio.
static_assert(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME == 3, "o Impulse deve ter 3 eixos (accX,accY,accZ)");
static_assert(EI_CLASSIFIER_RAW_SAMPLE_COUNT == JANELA_AMOSTRAS,
              "janela do Impulse diferente de JANELA_MS*FS_HZ em projeto_config.h");
static_assert((int)EI_CLASSIFIER_FREQUENCY == FS_HZ, "frequencia do Impulse diferente de FS_HZ");

#define CLF_N_CLASSES EI_CLASSIFIER_LABEL_COUNT

static inline const char *clf_nome(int k) { return ei_classifier_inferencing_categories[k]; }

static const float *g_janela_ei;
static int ei_le_dados(size_t off, size_t len, float *out)
{ memcpy(out, g_janela_ei + off, len * sizeof(float)); return 0; }

static inline int clf_classifica(const float *janela, float *prob, float *ms_dsp, float *ms_clf)
{
    g_janela_ei = janela;
    signal_t sinal;
    sinal.total_length = JANELA_AMOSTRAS * 3;
    sinal.get_data = &ei_le_dados;
    ei_impulse_result_t r = { 0 };
    EI_IMPULSE_ERROR e = run_classifier(&sinal, &r, false);
    if (e != EI_IMPULSE_OK) return -1;
    int best = 0;
    for (int k = 0; k < CLF_N_CLASSES; k++) {
        prob[k] = r.classification[k].value;
        if (prob[k] > prob[best]) best = k;
    }
    *ms_dsp = (float)r.timing.dsp;
    *ms_clf = (float)r.timing.classification;
    return best;
}

#else
// ---------------------------------------------------------------------------------------
// Classificador FALSO (stub): decide pela variancia total dos 3 eixos (energia AC do movimento).
// Nao e ML. Serve para validar a cadeia inteira antes do modelo existir.
#define CLF_N_CLASSES 2
static inline const char *clf_nome(int k) { return k == 0 ? "parado" : "movendo"; }

static inline int clf_classifica(const float *j, float *prob, float *ms_dsp, float *ms_clf)
{
    double s[3] = {0, 0, 0}, s2[3] = {0, 0, 0}; int n = JANELA_AMOSTRAS;
    for (int i = 0; i < n; i++)
        for (int e = 0; e < 3; e++) { double v = j[3*i + e]; s[e] += v; s2[e] += v * v; }
    double var = 0;
    for (int e = 0; e < 3; e++) var += s2[e] / n - (s[e] / n) * (s[e] / n);
    double sd = sqrt(var > 0 ? var : 0);            // desvio-padrao total (g)
    float pm = (float)(sd / (sd + 0.05));           // 0..1; 0,5 quando sd = 0,05 g
    prob[0] = 1.0f - pm; prob[1] = pm;
    *ms_dsp = 0; *ms_clf = 0;
    return pm > 0.5f ? 1 : 0;
}
#endif
#endif
```

</details>

<details>
<summary><code>src/inferencia_mqtt/main/main.cpp</code> (177 linhas) — clique para abrir</summary>

```cpp
// Lab Extra — Classificador de movimento no ESP32 (inferencia na borda + MQTT)
// ESP-IDF 5.x. Arquivo principal do projeto montado sobre o exemplo
//   github.com/edgeimpulse/example-standalone-inferencing-espressif-esp32
// (substitui o arquivo principal do exemplo; ver o lab, Parte D).
//
// Arquitetura (semanas 5, 6 e 14):
//
//   amostra_task (prio 6)              inferencia_task (prio 5)
//   vTaskDelayUntil, 100 Hz            espera notificacao
//   le MPU-6050 ──▶ janela_push        copia janela ──▶ clf_classifica ──▶ debounce
//   (mutex curto)    │ stride?  ───────▶ xTaskNotifyGive                      │ mudou?
//                                                                              ▼
//                                                       esp_mqtt_client_publish(ifes/<b>/movimento)
//
// Compilar SEM o Edge Impulse (testar sensor + Wi-Fi + MQTT com o classificador falso):
//   idf.py build -DCMAKE_CXX_FLAGS="-DUSAR_EI=0"      (ou coloque #define USAR_EI 0 em classificador.h)
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "driver/gpio.h"

#include "projeto_config.h"
#include "mpu.h"
#include "janela.h"
#include "debounce.h"
#include "classificador.h"

static const char *TAG = "movimento";

static float          g_mem[JANELA_AMOSTRAS * EIXOS];   // memoria do buffer circular
static janela_t       g_jan;
static SemaphoreHandle_t g_mutex;
static TaskHandle_t   g_task_inf;
static volatile long  g_perdidas = 0;                   // erros de I2C (amostra repetida)

static EventGroupHandle_t eg;
#define BIT_GOT_IP  BIT0
#define BIT_MQTT_OK BIT1
static esp_mqtt_client_handle_t cli;

/* ============================ Wi-Fi + MQTT (padrao da semana 14) ============================ */
static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(eg, BIT_GOT_IP); esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) xEventGroupSetBits(eg, BIT_GOT_IP);
}

static void wifi_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handler, NULL);
    wifi_config_t wc = {};
    strncpy((char *)wc.sta.ssid, WIFI_SSID, sizeof wc.sta.ssid);
    strncpy((char *)wc.sta.password, WIFI_PASS, sizeof wc.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void mqtt_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if ((esp_mqtt_event_id_t)id == MQTT_EVENT_CONNECTED) {
        esp_mqtt_client_publish(cli, T_STATUS, "online", 0, 1, 1);   // retido
        xEventGroupSetBits(eg, BIT_MQTT_OK);
    } else if ((esp_mqtt_event_id_t)id == MQTT_EVENT_DISCONNECTED) {
        xEventGroupClearBits(eg, BIT_MQTT_OK);
    }
}

static void mqtt_start(void)
{
    esp_mqtt_client_config_t mc = {};
    mc.broker.address.uri = BROKER_URI;
    mc.session.last_will.topic = T_STATUS;
    mc.session.last_will.msg = "offline";
    mc.session.last_will.msg_len = 7;
    mc.session.last_will.qos = 1;
    mc.session.last_will.retain = 1;
    mc.session.keepalive = 10;
    cli = esp_mqtt_client_init(&mc);
    esp_mqtt_client_register_event(cli, MQTT_EVENT_ANY, mqtt_handler, NULL);
    esp_mqtt_client_start(cli);
}

/* ============================ Tarefa 1: amostragem a 100 Hz ============================ */
static void amostra_task(void *arg)
{
    const TickType_t T = pdMS_TO_TICKS(1000 / FS_HZ);
    TickType_t prox = xTaskGetTickCount();
    float a[3] = { 0, 0, 1 };
    for (;;) {
        if (mpu_le_g(a) != ESP_OK) g_perdidas++;          // repete a ultima amostra boa
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        int pronta = janela_push(&g_jan, a);              // secao critica curtissima
        xSemaphoreGive(g_mutex);
        if (pronta) xTaskNotifyGive(g_task_inf);          // acorda a inferencia
        vTaskDelayUntil(&prox, T);                        // periodo cravado (semana 5)
    }
}

/* ============================ Tarefa 2: inferencia + decisao + MQTT ============================ */
static void inferencia_task(void *arg)
{
    static float janela[JANELA_AMOSTRAS * EIXOS];         // copia linearizada (cronologica)
    float prob[CLF_N_CLASSES];
    debounce_t deb; debounce_init(&deb, N_CONFIRMA, LIMIAR_CONF);
    int contador = 0;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);          // bloqueia ate haver janela nova
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        janela_copia(&g_jan, janela);
        xSemaphoreGive(g_mutex);

        float ms_dsp = 0, ms_clf = 0;
        int melhor = clf_classifica(janela, prob, &ms_dsp, &ms_clf);
        if (melhor < 0) { ESP_LOGE(TAG, "run_classifier falhou"); continue; }

        // diagnostico serial: uma linha por janela, com todas as probabilidades
        printf("[janela] ");
        for (int k = 0; k < CLF_N_CLASSES; k++) printf("%s=%.2f ", clf_nome(k), prob[k]);
        printf("| dsp=%.0f ms clf=%.0f ms perdidas_i2c=%ld\n", ms_dsp, ms_clf, g_perdidas);

        if (debounce_update(&deb, melhor, prob[melhor])) {
            const char *classe = clf_nome(deb.estado);
            char msg[96];
            snprintf(msg, sizeof msg, "{\"classe\":\"%s\",\"conf\":%.2f,\"t\":%lld}",
                     classe, prob[melhor], (long long)(esp_timer_get_time() / 1000));
            esp_mqtt_client_publish(cli, T_MOV, msg, 0, 0, 1);   // QoS 0, retido: painel novo ve o estado atual
            ESP_LOGI(TAG, "mudou -> %s", msg);
            gpio_set_level((gpio_num_t)PINO_LED, strcmp(classe, CLASSE_ATUA) == 0);  // acao local
        }
        if (++contador % INFO_A_CADA == 0) {
            char info[96];
            snprintf(info, sizeof info, "{\"dsp_ms\":%.0f,\"clf_ms\":%.0f,\"i2c_err\":%ld}",
                     ms_dsp, ms_clf, g_perdidas);
            esp_mqtt_client_publish(cli, T_INFO, info, 0, 0, 0);
        }
    }
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    eg = xEventGroupCreate();
    gpio_set_direction((gpio_num_t)PINO_LED, GPIO_MODE_OUTPUT);

    uint8_t who = mpu_init();
    ESP_LOGI(TAG, "MPU-6050 WHO_AM_I=0x%02X (esperado 0x68)", who);

    janela_init(&g_jan, g_mem, JANELA_AMOSTRAS, STRIDE_AMOSTRAS);
    g_mutex = xSemaphoreCreateMutex();

    wifi_start();
    xEventGroupWaitBits(eg, BIT_GOT_IP, pdFALSE, pdTRUE, portMAX_DELAY);
    mqtt_start();
    xEventGroupWaitBits(eg, BIT_MQTT_OK, pdFALSE, pdTRUE, portMAX_DELAY);

    // A inferencia precisa de pilha grande (o SDK do EI usa bastante); 16 kB e um ponto de partida.
    xTaskCreate(inferencia_task, "inferencia", 16384, NULL, 5, &g_task_inf);
    xTaskCreate(amostra_task,    "amostra",      4096, NULL, 6, NULL);
}
```

</details>

### F.4 Primeiro, a cadeia SEM o modelo (`USAR_EI=0`)

Compile primeiro com o classificador falso (`#define USAR_EI 0` no topo de `classificador.h`, ou
`-DUSAR_EI=0`). Isso isola problemas: se Wi-Fi + MQTT + *debounce* funcionam com o stub, qualquer
falha depois é do modelo. No monitor:

```
[janela] parado=0.93 movendo=0.07 | dsp=0 ms clf=0 ms perdidas_i2c=0
```

No RPi: `mosquitto_sub -t 'ifes/bancadaN/#' -v` — com o sensor parado ou sacudindo, o status
`online` e a mensagem `…/movimento {"classe":"movendo",…}` aparecem após ~3 s.

### F.5 Agora com o modelo (`USAR_EI=1`)

Compile, grave e observe uma linha `[janela]` por 0,5 s, com as probabilidades das **suas**
classes. Os valores `dsp=… clf=…` são os tempos **medidos** — confira que são ≪ 500 ms.

> ✅ **Checkpoint F:** o monitor mostra `[janela]` a cada 0,5 s; mexer a placa muda a classe
> publicada; `perdidas_i2c` fica em 0 (ou perto).

## Parte G — Painel, teste de campo e demonstração (aula 3)

![Fluxo MQTT: ESP32 publica status, movimento e info no broker do RPi, assinados pelo monitor, painel e mosquitto_sub](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_mqtt.png)
*Figura L-6 — Tópicos e payloads: `ifes/<bancada>/status`, `…/movimento`, `…/movimento/info`.*

1. No RPi (ou no PC com acesso ao broker), rode o monitor:

   ```bash
   python3 tools/monitor_movimento.py --broker localhost --bancada bancada1 --conta sobe_desce
   ```

   Ele mostra cada mudança de classe com a duração da anterior, conta repetições de
   `sobe_desce` e grava `movimento.csv`. (Se você fez a [trilha full-stack](../../docs/trilha-fullstack.md),
   adicione `ifes/<bancada>/movimento` ao painel.)

2. **Teste de campo — a matriz de confusão ao vivo.** Quem **não** gravou os dados de treino faz
   cada gesto **10 vezes** (~5 s cada, esperando a saída estabilizar) e você anota o que foi
   publicado:

   | gesto real \ publicado | parado | aceno | sobe_desce | circulo | nenhum |
   |---|---|---|---|---|---|
   | parado | | | | | |
   | aceno | | | | | |
   | sobe_desce | | | | | |
   | circulo | | | | | |

   Compare com o *Model testing* do Studio. Se o campo for muito pior, as causas típicas são:
   orientação/posição do sensor diferente, pessoa nova, ou **algo no firmware diferente do treino**
   (ver o fluxograma abaixo).

3. **Meça a latência de ponta a ponta** com um cronômetro (do início do gesto até a classe mudar no
   monitor) e compare com a estimativa do §8 da teoria.

![Fluxograma de diagnóstico: o problema aparece no Studio, no ESP32 ou na demonstração com outra pessoa](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_diagnostico.png)
*Figura L-7 — Onde olhar quando o resultado não é o esperado. Detalhes no [guia-problemas.md](guia-problemas.md).*

4. **Demonstração (vídeo curto, ≤ 2 min):** faça cada gesto e mostre a classe mudando ao vivo no
   painel/monitor, e o `status` ficando `offline` quando você desliga a placa (o LWT da semana 14).

> ✅ **Checkpoint G:** matriz ao vivo preenchida, latência medida, vídeo gravado.

## Entrega

No repositório da bancada, pasta `lab-classificador/`:

- `relatorio.md` — siga o [relatorio-modelo.md](relatorio-modelo.md): justificativa da taxa;
  protocolo de gravação e tabela de dados; print do Feature explorer; **treino × teste**, matriz
  e leitura; log de iterações; print de sanidade; tempos medidos × estimados; matriz de campo;
  latência ponta a ponta; e o **project ID**, as classes, `fs`, janela e a **acurácia de teste**
  (reprodutibilidade).
- `dados/` (CSVs que você gravou) e `projeto_config.h` final.
- O vídeo da demonstração.

A rubrica de avaliação está no [relatorio-modelo.md](relatorio-modelo.md).

## Desafios opcionais

- **Atuar localmente:** `circulo` liga um LED/relé por PWM (semana 8) — já há um LED em `PINO_LED`;
  troque `CLASSE_ATUA`. Meça: inferência → ação na borda leva ms, sem passar pela rede.
- **Classe `outro` / detecção de anomalia:** grave movimentos aleatórios como classe `outro`, ou
  ative a detecção de anomalia (K-means) no Impulse e use `result.anomaly` no firmware.
- **Contador de repetições:** use `--conta sobe_desce` e valide contra a contagem manual.
- **Monitor de vibração:** troque as classes por `normal` × `desbalanceado` num ventilador/motor
  (aqui o DLPF e o fundo de escala importam muito).
- **Experimento de aliasing:** ponha `MPU_DLPF 0` na coleta, grave um gesto perto de uma fonte de
  vibração e compare o espectro (Figura C-4). Lembre de usar o mesmo valor na inferência.
- **Stride e janela:** varie `STRIDE_MS`/`N_CONFIRMA`/janela e meça latência × estabilidade.
