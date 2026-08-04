# Lab Extra — Classificador de Movimento na Borda (Edge Impulse + MPU-6050)

Lab integrador **opcional/avançado** que coloca **inteligência na borda**: treina um classificador
de movimento na plataforma web [Edge Impulse](https://edgeimpulse.com) e o roda **no próprio ESP32**,
com o MPU-6050 da semana 9. Une amostragem e FFT (PDS, semana 7), I2C e o acelerômetro (semana 9),
tarefas e janela deslizante (semanas 5 e 6) e IoT/MQTT (semana 14) num sistema que **reconhece o
movimento localmente** e publica só a conclusão na rede.

> Sem alta tensão, sem risco elétrico — ao contrário do lab de energia, este é seguro para os
> alunos montarem sozinhos. O "perigo" aqui é epistemológico: confiar num modelo que decorou os
> dados. A `teoria-classificador.md` mostra como não cair nessa.

## Conteúdo

- `teoria-classificador.md` — o que é TinyML e por que o pipeline é 90% processamento de sinal;
  o bloco DSP (FFT / *Spectral Features*), a escolha da taxa por Nyquist, e overfitting × matriz de
  confusão. Formato tutorial, com exemplos resolvidos C.1–C.3.
- `lab-classificador.md` — roteiro guiado de ~2 aulas: coleta pela CLI (data forwarder), montagem do
  Impulse, treino e leitura honesta das métricas, deploy como biblioteca C++ no ESP-IDF, e o fecho
  com inferência ao vivo publicada por MQTT.
- `src/coleta_mpu/main.c` — firmware ESP-IDF que lê o MPU-6050 e imprime `aX,aY,aZ` a 100 Hz
  cravados (para o `edge-impulse-data-forwarder`).
- `src/inferencia_mqtt/main.c` — esqueleto do produto final: duas tarefas (amostragem + inferência
  em janela deslizante) e publicação da classe em `movimento/#`. A parte de ML vem da biblioteca C++
  que você exporta do Edge Impulse.

## Hardware

- ESP32 + **MPU-6050** por I2C (SDA=21, SCL=22) — exatamente a montagem do Lab 9. Nada além disso.
- Opcional (desafio): LED/relé num pino PWM para o nó **atuar** localmente conforme a classe.

## Pré-requisitos e onde encaixa

Melhor depois da semana 14 (usa Wi-Fi/MQTT no fecho), mas a coleta e o treino podem ser feitos já
após a semana 9. Serve como **lab extra**, como **aprofundamento de PDS/sensores** (ver as notas no
fim de `../../semana-07/teoria-07.md` e `../../semana-09/teoria-09.md`) e como **kit de projeto
final** — é a versão "roteiro fechado" da [trilha TinyML](../../docs/trilha-tinyml.md), que é o
estudo dirigido mais aberto para o projeto.

## Fluxo em uma figura

```
MPU-6050 ──I2C──▶ ESP32 (coleta 100 Hz) ──serial──▶ Edge Impulse Studio
                                                       │  (janela + FFT + treino)
                        biblioteca C++  ◀──────────────┘
                              │ (Deployment)
                              ▼
ESP32 (inferência na borda) ──MQTT "movimento/#"──▶ painel no RPi (semana 14)
```

## Reprodutibilidade

Registre no repositório da bancada o **project ID** do Edge Impulse, a lista de classes, a taxa de
amostragem e a **acurácia de teste** (não a de treino). Sem esses dados, o classificador não é
reproduzível — e reprodutibilidade conta como documentação no
[projeto final](../../projeto-final/README-proj-final.md).
