# Trilha opcional — TinyML no ESP32 com Edge Impulse (para projetos finais)

Guia de estudo dirigido para grupos que queiram embarcar um **classificador de movimento** no
próprio ESP32, usando o acelerômetro MPU-6050 da semana 9 e a plataforma web gratuita
[Edge Impulse](https://docs.edgeimpulse.com/) (o campus tem acesso). Pré-requisitos: semana 7
(ADC, amostragem e fundamentos de PDS), semana 9 (I2C e MPU-6050) e semana 14 (MQTT) para quem
quiser publicar a inferência no painel do RPi. Todo o fluxo cabe no plano gratuito (Developer).

## O que você vai construir

Um nó ESP32 que **reconhece o próprio movimento** — por exemplo, distinguir *parado*, *aceno*,
*sobe-desce* e *círculo* — rodando a inferência **localmente**, sem nuvem, e publicando a classe
detectada via MQTT no broker do RPi (a mesma arquitetura da Fig. 14-A da teoria-14). É o fecho do
ciclo que o curso inteiro monta: **sensor → processamento de sinal → decisão → ação**, agora com
a decisão vinda de um modelo treinado em vez de um `if` escrito à mão.

![Arquitetura ESP32 + RPi: a inferência roda no nó e a classe detectada vai pelo MQTT ao painel](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/arquitetura_esp32_rpi.png)
*Figura T-1 — A arquitetura da semana 14, reaproveitada: o ESP32 classifica o movimento e
publica a etiqueta; o RPi concentra e mostra. Fonte: diagrama da apostila (teoria-14, Fig. 14-A).*

> 🧭 **Como usar esta trilha:** as etapas são cumulativas — não pule. Reserve ~2 sessões de
> laboratório por etapa. Ao final de cada uma você deve ter algo **funcionando e demonstrável**,
> não apenas lido. O checklist de "chegamos!" de cada etapa é o que vale na rubrica do projeto.

## Por que isto conecta com o que você já aprendeu

O Edge Impulse não é mágica nova: é a formalização do que a apostila já ensinou. A tabela abaixo
amarra cada peça da plataforma a uma semana do curso.

| Peça no Edge Impulse | O que é | Onde você já viu |
|---|---|---|
| Aquisição a uma taxa fixa (ex.: 62,5 ou 100 Hz) | amostrar o acelerômetro em período cravado | semana 5 (`vTaskDelayUntil`) e semana 7 (Nyquist) |
| Janela + *Spectral Features* (FFT) | o "DSP block": extrai energia por frequência de cada janela | semana 7 (ADC/PDS, FFT) — a própria doc do EI chama isso de "DSP / pré-processamento" |
| MPU-6050 via I2C | fonte dos dados (ax, ay, az) | semana 9 (I2C, scanner, mapa de registradores) |
| *Learning block* (classificação) | a rede que aprende a separar as classes | novo — é o assunto desta trilha |
| Deploy como biblioteca C++ (ESP-IDF) | o modelo vira código que compila com `idf.py` | semanas 1–10 (ESP-IDF/FreeRTOS) |
| Publicar a classe por MQTT | levar a inferência ao painel | semana 14 (MQTT) |

A lição de fundo: **um modelo de ML embarcado é um bloco de PDS seguido de um classificador.** Se
você entendeu a FFT da semana 7, você já entende metade do pipeline.

## Etapa 0 — Conta, projeto e o pré-requisito de hardware

**O quê:** criar a conta gratuita, um projeto vazio e confirmar que o MPU-6050 lê no ESP32.

- Crie a conta Developer em <https://edgeimpulse.com/signup> e um projeto novo no
  [Studio](https://docs.edgeimpulse.com/studio) (a interface web onde tudo acontece).
- Confirme, no ESP32, que o Lab 9 ainda roda: o `i2cdetect`/scanner acha o MPU-6050 no endereço
  `0x68` e você consegue ler aceleração nos três eixos. **Esse é o pré-requisito real** — se o
  sensor não lê, nada à frente funciona.

> ✅ **Chegamos:** projeto criado no Studio e leitura de (ax, ay, az) saindo no monitor serial.

## Etapa 1 — Coletar e rotular os dados de movimento

**O quê:** gravar exemplos de cada gesto, já rotulados, dentro do projeto.

- Defina **3 a 4 classes** de movimento distintas e fáceis de repetir. Uma boa escolha inicial,
  espelhando o dataset público de referência do Edge Impulse (idle / wave / updown / snake):
  **parado, aceno, sobe-desce, círculo**.
- Escolha a **taxa de amostragem** e fixe-a com `vTaskDelayUntil` (semana 5), não `vTaskDelay` —
  período irregular estraga o espectro (é o Exemplo 5.1 em ação). 62,5 Hz é o valor do dataset de
  referência; 100 Hz também funciona bem para gestos de mão. Lembre de Nyquist (semana 7): a essa
  taxa você enxerga componentes até ~31–50 Hz, de sobra para movimento humano.
- Envie os dados para o Studio por um destes caminhos (todos documentados): o
  **Data forwarder / uploader** da [Edge Impulse CLI](https://docs.edgeimpulse.com/tools/clis/edge-impulse-cli),
  o **CSV Wizard** (se você gravar em CSV pelo próprio firmware — formato que você já domina desde
  os labs de logging), ou a **Ingestion API**. Para um atalho de estudo, dá para **clonar o
  dataset público** "Continuous motion recognition" e treinar sem coletar nada — útil para provar
  o fluxo antes de gravar os seus próprios gestos.
- Grave **vários exemplos por classe e por pessoa diferente** (mãos e ritmos variam). Separe
  treino/teste (~80/20) — o Studio ajuda nisso.

> ⚠️ **Erro clássico:** treinar só com os seus próprios gestos e o modelo falhar com o colega. É o
> equivalente de ML do "funcionou na minha bancada". Colete diversidade de propósito.

> ✅ **Chegamos:** cada classe com um punhado de amostras rotuladas e um conjunto de teste
> separado, visíveis na aba *Data acquisition*.

## Etapa 2 — Montar o *Impulse*: janela + FFT + classificador

**O quê:** desenhar o pipeline que transforma janelas de sinal em uma classe.

- Em *Create impulse*, defina o **tamanho da janela** (ex.: 1–2 s) e o **passo** entre janelas.
  Pense nisso como o "quadro" que o classificador enxerga de cada vez.
- Adicione o bloco de processamento **Spectral Features** — é o **DSP block**. Ele aplica a
  **FFT** em cada eixo e resume a janela pela energia nas faixas de frequência. **Isto é a
  semana 7 na prática:** o mesmo motivo pelo qual a FFT revela a frequência de um sinal é o que
  faz "aceno lento" e "círculo rápido" caírem em regiões diferentes do espaço de features.
- Adicione o **Learning block** de **Classification** (uma rede neural pequena). Opcionalmente,
  acrescente **Anomaly Detection (K-means)** para pegar movimentos que não são de nenhuma classe.
- Abra *Feature explorer* depois de gerar as features e **olhe se as classes se separam**. Se
  duas classes se misturam no gráfico, elas vão se confundir no ESP32 também — volte à Etapa 1 e
  colete gestos mais distintos ou mais exemplos.

> ✅ **Chegamos:** *Impulse* com Spectral Features + Classification configurado e o Feature
> explorer mostrando agrupamentos visíveis por classe.

## Etapa 3 — Treinar e ler as métricas com honestidade

**O quê:** treinar o classificador e interpretar o resultado sem se enganar.

- Treine (poucos minutos no plano gratuito). Leia a **matriz de confusão** e a acurácia do
  conjunto de validação. **Uma acurácia alta demais é suspeita** — costuma significar vazamento
  (mesma gravação em treino e teste) ou classes fáceis demais.
- Use *Model testing* no conjunto separado da Etapa 1 — esse número é o honesto.
- Veja também a estimativa de **latência e memória** que o Studio mostra por dispositivo: é o
  vínculo direto com a semana 2 (RAM/flash) e a semana 4 (tempo real). Um modelo que não cabe na
  SRAM ou demora demais por janela não serve, por melhor que seja a acurácia.

> ⚠️ **Sensível ao overfitting:** se a acurácia de treino é ótima e a de teste é ruim, o modelo
> "decorou". Menos épocas, mais dados, ou classes mais separáveis.

> ✅ **Chegamos:** matriz de confusão entendida, número honesto de *Model testing* anotado, e
> latência/RAM dentro do que o ESP32 aguenta.

## Etapa 4 — Exportar e rodar no ESP32 (ESP-IDF)

**O quê:** transformar o modelo em biblioteca C++ e compilar no ambiente que você já usa.

Fluxo oficial (documentado em
[Run C++ library on ESP32](https://docs.edgeimpulse.com/hardware/deployments/run-cpp-espressif-esp32)):

1. Na aba **Deployment**, escolha **C++ library** e clique **Build**. Baixe o `.zip`.
2. Clone o repositório de exemplo do próprio Edge Impulse:
   ```bash
   git clone https://github.com/edgeimpulse/example-standalone-inferencing-espressif-esp32
   ```
3. Descompacte a biblioteca e copie **as pastas** (`edge-impulse-sdk/`, `model-parameters/`,
   `tflite-model/`) para a raiz desse repositório. A estrutura final tem `CMakeLists.txt`,
   `main/`, `partitions.csv`, `sdkconfig` etc. — projeto ESP-IDF comum, igual aos das semanas 1–10.
4. Para um primeiro teste **sem sensor**, cole um vetor de *Raw features* (copiado do Studio, em
   *Live classification*) dentro de `static const float features[]` no `ei_main.cpp`, e:
   ```bash
   idf.py build
   idf.py flash
   idf.py monitor        # 115200 baud
   ```
   A classificação impressa no ESP32 deve **bater** com a que o Studio mostrou para a mesma
   amostra. Esse é o teste de sanidade: mesma entrada, mesma saída, dentro e fora do navegador.

> ✅ **Chegamos:** o ESP32 imprime as probabilidades por classe e elas coincidem com o Studio.

## Etapa 5 — Ligar no sensor de verdade e publicar por MQTT

**O quê:** substituir o vetor fixo pela leitura ao vivo do MPU-6050 e mandar a classe ao painel.

- Troque o `features[]` estático por um **buffer circular** preenchido pela sua tarefa de
  amostragem (a mesma da Etapa 1, `vTaskDelayUntil`). A cada janela cheia, chame o classificador.
  Atenção à **ordem e à escala dos eixos**: têm de ser idênticas às da coleta, ou o modelo recebe
  "outra coisa". Este é o erro nº 1 de integração.
- Publique a classe de maior probabilidade (acima de um limiar) via **MQTT** (semana 14) num
  tópico tipo `sis-emb/<dupla>/movimento`. No painel do RPi, mostre a etiqueta em tempo real — se
  você fez a trilha full-stack, é só mais um tópico no dashboard.
- Dica de robustez: exija que **N janelas seguidas** concordem antes de publicar, para não piscar
  a saída a cada janela ruidosa (é o "debounce" da semana 3, agora aplicado a decisões de ML).

> ✅ **Chegamos:** mover a placa muda a etiqueta no painel em tempo real, de forma estável.

## Ideias de projeto final com esta trilha

- **Controle por gesto:** aceno liga/desliga um atuador (relé/LED) via MQTT — junta esta trilha
  com a ponte H da semana 8.
- **Contador de repetições de exercício:** conta "sobe-desce" (agachamento, rosca) e publica a
  contagem — vira um wearable simples.
- **Monitor de vibração de máquina:** troca gestos por estados de uma bancada (parada / normal /
  desbalanceada) — o mesmo pipeline FFT+classificação, tema industrial, conversa direto com o
  lab-extra de energia.
- **Detecção de queda:** classe "queda" dispara um alerta MQTT — clássico de saúde/assistência.

## Aprofundamento (opcional)

- Fundamentos de ML e edge AI (curso gratuito do próprio Edge Impulse):
  [Introduction to Embedded Machine Learning](https://docs.edgeimpulse.com/knowledge/courses/introduction-embedded-ml)
  e [Edge AI Fundamentals](https://docs.edgeimpulse.com/knowledge/courses/edge-ai-fundamentals).
- Por que a FFT é o pré-processamento certo para movimento:
  [Motion feature extraction](https://docs.edgeimpulse.com/knowledge/concepts/data-engineering/motion-feature-extraction).
- Dataset público de referência (clonável) e tutorial ponta-a-ponta:
  [Continuous motion recognition](https://docs.edgeimpulse.com/datasets/time-series/continuous-motion-recognition).
- Livro do acervo: há material de deep learning em Jetson/RPi citado na teoria-14 para quem
  quiser ir além do TinyML no microcontrolador.

> **Nota de reprodutibilidade:** o Edge Impulse evolui rápido; nomes de menu e telas podem mudar.
> Se algo estiver diferente do descrito aqui, os passos conceituais (coletar → FFT → treinar →
> exportar C++ → rodar no ESP-IDF → publicar) seguem valendo; confira a
> [documentação oficial](https://docs.edgeimpulse.com/) para o detalhe da interface.
