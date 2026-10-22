# Lab Extra — Classificador de Movimento na Borda (Edge Impulse + MPU-6050)

Lab integrador **opcional/avançado** que coloca **inteligência na borda**: treina um classificador
de movimento na plataforma web [Edge Impulse](https://edgeimpulse.com) e o roda **no próprio
ESP32**, com o MPU-6050 da semana 9. Une amostragem e FFT (PDS, semana 7), I²C e o acelerômetro
(semana 9), tarefas e janela deslizante (semanas 5 e 6) e IoT/MQTT (semana 14) num sistema que
**reconhece o movimento localmente** e publica só a conclusão na rede.

> Sem alta tensão, sem risco elétrico — ao contrário do lab de energia, este é seguro para os
> alunos montarem sozinhos. O "perigo" aqui é epistemológico: confiar num modelo que decorou os
> dados. A `teoria-classificador.md` mostra como não cair nessa.

![Pipeline do projeto: fase de treino (PC e Studio) e fase de inferência (ESP32), com o contrato de parâmetros que precisa ser idêntico nas duas](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_pipeline.png)

## Por onde começar

1. Leia o mapa geral e a rubrica: [trilha TinyML](../../docs/trilha-tinyml.md).
2. Leia a [teoria](teoria-classificador.md) (≈ 40 min).
3. Siga o [roteiro](lab-classificador.md), Parte 0 → G.
4. Documente no [modelo de relatório](relatorio-modelo.md); se travar, o [guia de problemas](guia-problemas.md).

**Atalho de 10 minutos, sem hardware** (para ver o pipeline funcionando):

```bash
python3 -m venv ~/venv-tinyml && source ~/venv-tinyml/bin/activate && pip install numpy pyserial paho-mqtt
python3 tools/gera_sinteticos.py /tmp/sint
python3 tools/pipeline_host.py /tmp/sint
```

## Conteúdo

```
classificador-movimento/
├── README-classificador.md     este arquivo
├── teoria-classificador.md     TinyML, janela, FFT, Nyquist/DLPF, overfitting, matriz, latência (C.1–C.7)
├── lab-classificador.md        roteiro guiado, Partes 0, A–G, com todo o código embutido
├── guia-problemas.md           sintoma → causa → teste → correção
├── relatorio-modelo.md         modelo de relatório + rubrica (100 pontos)
├── src/
│   ├── coleta_mpu/main.c       firmware de COLETA (100 Hz, DLPF, linha "aX,aY,aZ")
│   └── inferencia_mqtt/main/   firmware de INFERÊNCIA (ESP-IDF, C++)
│       ├── main.cpp            2 tarefas FreeRTOS + Wi-Fi + MQTT
│       ├── classificador.h     ponte com a biblioteca do Edge Impulse (ou classificador falso)
│       ├── janela.h            buffer circular + stride          (C puro, testado no PC)
│       ├── debounce.h          N janelas seguidas + limiar       (C puro, testado no PC)
│       ├── mpu.h               leitura do MPU-6050 (idêntica à coleta)
│       ├── projeto_config.h    rede, sensor, janela, decisão — TUDO que tem de casar com o Studio
│       └── CMakeLists.txt      trecho de referência (não substitui o do exemplo do EI)
├── tools/
│   ├── gera_sinteticos.py      gravações SINTÉTICAS no formato do Edge Impulse (só para testar)
│   ├── gravar_csv.py           grava gestos da serial em CSV, mede a taxa e avisa de saturação
│   ├── pipeline_host.py        janela → FFT → classificador → matriz, no PC (baseline local)
│   └── monitor_movimento.py    assina o MQTT, mostra mudanças, conta repetições, grava CSV
└── tests/
    ├── test_logica.c           testa janela.h e debounce.h
    └── test_stub.cpp           testa a cadeia janela → classificador falso → debounce
```

Figuras usadas (em `assets/figuras/`, prefixo `tml_`): `pipeline`, `roteiro`, `fiacao`, `gestos`,
`janela`, `espectro`, `dlpf_alias`, `split`, `vazamento`, `confusao`, `feature_explorer`,
`firmware`, `latencia`, `mqtt`, `studio`, `diagnostico`. As que mostram dados foram geradas com
**dados sintéticos** e dizem isso na própria figura.

## Hardware

- ESP32 + **MPU-6050** por I²C (SDA = GPIO21, SCL = GPIO22, endereço 0x68) — exatamente a montagem
  do Lab 9. Nada além disso.
- Opcional (desafio): LED/relé num pino para o nó **atuar** localmente conforme a classe.
- RPi 3 com Mosquitto (semana 14) para o fecho por MQTT.

## Pré-requisitos e onde encaixa

Melhor depois da semana 14 (usa Wi-Fi/MQTT no fecho), mas a coleta e o treino podem ser feitos já
após a semana 9. Serve como **lab extra**, como **aprofundamento de PDS/sensores** (ver as notas
no fim de `../../semana-07/teoria-07.md` e `../../semana-09/teoria-09.md`) e como **kit de projeto
final** — é a versão "roteiro fechado" da [trilha TinyML](../../docs/trilha-tinyml.md), que é o
estudo dirigido mais aberto para o projeto.

## O que foi testado

| Item | Situação |
|---|---|
| `janela.h`, `debounce.h`, cadeia com classificador falso | **testado no PC** (`tests/`) |
| `gera_sinteticos.py`, `pipeline_host.py` | **testado no PC** |
| `gravar_csv.py` | testado no PC com uma porta serial **simulada** |
| `monitor_movimento.py` | escrito conforme o padrão do `logger_rpi.py`; **não executado contra um broker real** |
| Firmware de coleta e de inferência no ESP32 | **não gravado em hardware** pelo autor — pode exigir ajustes |
| Integração com a biblioteca do Edge Impulse (`USAR_EI=1`) | **não compilada** (o ambiente do autor não alcança o site do Edge Impulse); os nomes da API seguem a documentação oficial |
| Telas/menus do Studio e comandos da CLI | descritos pela documentação; **podem ter mudado** |

## Convenções

- **Tópicos MQTT** (padrão da semana 14, prefixo `ifes/`): `ifes/<bancada>/movimento`,
  `ifes/<bancada>/movimento/info`, `ifes/<bancada>/status`.
- **Rótulos das classes** sem acento e sem espaço: `parado`, `aceno`, `sobe_desce`, `circulo`.
- **Unidade e ordem dos dados:** aceleração em **g**, eixos `accX, accY, accZ`, intercalados.

## Reprodutibilidade

Registre no repositório da bancada o **project ID** do Edge Impulse, a lista de classes, a taxa de
amostragem, a janela, o `projeto_config.h` e a **acurácia de teste** (não a de treino). Sem esses
dados, o classificador não é reproduzível — e reprodutibilidade conta como documentação no
[projeto final](../../projeto-final/README-proj-final.md).
