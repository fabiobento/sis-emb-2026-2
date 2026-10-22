# Trilha opcional — TinyML no ESP32 com Edge Impulse (para projetos finais)

Guia de estudo dirigido para grupos que queiram embarcar um **classificador de movimento** no
próprio ESP32, usando o acelerômetro MPU-6050 da semana 9 e a plataforma web gratuita
[Edge Impulse](https://docs.edgeimpulse.com/) (o campus tem acesso). Pré-requisitos: semana 7
(ADC, amostragem e fundamentos de PDS), semana 9 (I²C e MPU-6050) e semana 14 (MQTT) para quem
quiser publicar a inferência no painel do RPi. Todo o fluxo cabe no plano gratuito (Developer).

> 🎯 **Este documento é o ponto de partida.** Ele dá o **mapa**, as **decisões** e os
> **critérios de "pronto"** de cada etapa. Os passos de mão na massa, com **todo o código**, estão
> no [lab-extra do classificador](../labs-extra/classificador-movimento/README-classificador.md);
> o "porquê" de cada decisão está na [teoria de apoio](../labs-extra/classificador-movimento/teoria-classificador.md).
> Quem seguir a trilha **etapa por etapa** chega a um sistema funcionando.

## O que você vai construir

Um nó ESP32 que **reconhece o próprio movimento** — por exemplo, distinguir *parado*, *aceno*,
*sobe-desce* e *círculo* — rodando a inferência **localmente**, sem nuvem, e publicando a classe
detectada via MQTT no broker do RPi (a mesma arquitetura da Fig. 14-A da teoria-14). É o fecho do
ciclo que o curso inteiro monta: **sensor → processamento de sinal → decisão → ação**, agora com
a decisão vinda de um modelo treinado em vez de um `if` escrito à mão.

![Arquitetura ESP32 + RPi: a inferência roda no nó e a classe detectada vai pelo MQTT ao painel](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/arquitetura_esp32_rpi.png)
*Figura T-1 — A arquitetura da semana 14, reaproveitada: o ESP32 classifica o movimento e
publica a etiqueta; o RPi concentra e mostra. Fonte: diagrama da apostila (teoria-14, Fig. 14-A).*

Por dentro, o projeto tem **duas fases** que precisam "falar a mesma língua":

![Pipeline em duas fases: treino (MPU-6050, firmware de coleta, serial/CSV, Studio, Impulse, métricas, deployment) e inferência (amostra_task, buffer circular, biblioteca EI, debounce, MQTT, RPi), com o contrato de parâmetros em destaque](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_pipeline.png)
*Figura T-2 — Fase 1 (azul): coletar dados rotulados e treinar o modelo no Studio. Fase 2
(vermelho): o modelo vira código dentro do firmware e classifica dados ao vivo. O quadro amarelo é
o **contrato**: taxa, janela, ordem dos eixos, unidade, fundo de escala e filtro têm de ser
idênticos nas duas fases — o erro nº 1 de integração é violá-lo sem perceber.*

## Como usar esta trilha

![Seis etapas, de 0 a 5, cada uma com seu entregável e as semanas do curso que a sustentam](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_roteiro.png)
*Figura T-3 — O roteiro. Cada etapa termina em algo que você **demonstra**.*

> 🧭 As etapas são **cumulativas** — não pule. Reserve ~2 sessões de laboratório por etapa. Ao
> final de cada uma você deve ter algo **funcionando e demonstrável**, não apenas lido. O checklist
> "✅ Chegamos" de cada etapa é o que vale na rubrica do projeto.

**Onde está cada coisa** (tudo no repositório):

| Preciso de… | Arquivo |
|---|---|
| passo a passo completo com código | [`lab-classificador.md`](../labs-extra/classificador-movimento/lab-classificador.md) |
| entender janela, FFT, Nyquist, overfitting, matriz | [`teoria-classificador.md`](../labs-extra/classificador-movimento/teoria-classificador.md) |
| firmware de coleta (ESP32) | [`src/coleta_mpu/main.c`](../labs-extra/classificador-movimento/src/coleta_mpu/main.c) |
| firmware de inferência + MQTT (ESP32) | [`src/inferencia_mqtt/main/`](../labs-extra/classificador-movimento/src/inferencia_mqtt/main/) |
| gravar CSV, baseline no PC, monitor MQTT | [`tools/`](../labs-extra/classificador-movimento/tools/) |
| algo deu errado | [`guia-problemas.md`](../labs-extra/classificador-movimento/guia-problemas.md) |
| modelo de relatório e rubrica | [`relatorio-modelo.md`](../labs-extra/classificador-movimento/relatorio-modelo.md) |

**Divisão de papéis sugerida** (grupo de 3–4): *dados* (coleta e rótulos), *modelo* (Studio e
métricas), *firmware* (coleta, inferência, tarefas), *rede/painel* (MQTT, RPi, demo). Todos
gravam gestos e todos entendem o contrato.

> 🧪 **Status do material:** a lógica de janela/*debounce* e as ferramentas Python foram testadas
> no PC. O firmware **no ESP32**, a integração com a **biblioteca exportada** do Edge Impulse e as
> telas do Studio **não foram verificados** pelo autor — nomes de menu mudam e pequenos ajustes
> podem ser necessários. Relate o que divergir ao professor: faz parte do trabalho de engenharia.

## Por que isto conecta com o que você já aprendeu

O Edge Impulse não é mágica nova: é a formalização do que a apostila já ensinou. A tabela amarra
cada peça da plataforma a uma semana do curso.

| Peça no Edge Impulse / no projeto | O que é | Onde você já viu |
|---|---|---|
| Aquisição a uma taxa fixa (100 Hz) | amostrar o acelerômetro em período cravado | semana 5 (`vTaskDelayUntil`) e semana 7 (Nyquist) |
| Filtro DLPF do MPU-6050 | anti-aliasing antes de amostrar | semana 7 (aliasing) |
| Janela + *Spectral Features* (FFT) | o "DSP block": energia por frequência de cada janela | semana 7 (ADC/PDS, FFT) |
| MPU-6050 via I²C | fonte dos dados (ax, ay, az) | semana 9 (I²C, scanner, mapa de registradores) |
| *Learning block* (classificação) | a rede que aprende a separar as classes | **novo** — assunto desta trilha |
| Deploy como biblioteca C++ (ESP-IDF) | o modelo vira código que compila com `idf.py` | semanas 1–10 (ESP-IDF/FreeRTOS) |
| Amostragem + inferência em tarefas | duas tarefas, mutex, notificação | semanas 5 e 6 (tarefas, sincronização) |
| *Debounce* de N janelas | estabilizar a decisão | semana 3 (debounce) |
| Latência/RAM estimadas | caber e responder a tempo | semana 2 (memória) e semana 4 (tempo real) |
| Publicar a classe por MQTT | levar a inferência ao painel | semana 14 (MQTT, LWT) |

A lição de fundo: **um modelo de ML embarcado é um bloco de PDS seguido de um classificador.** Se
você entendeu a FFT da semana 7, você já entende metade do pipeline.

## Glossário rápido

| Termo | Significado neste projeto |
|---|---|
| **Amostra (de dado)** | uma gravação rotulada (ex.: 10 s de `aceno`) — no Edge Impulse, "sample" |
| **Rótulo / classe** | o nome do gesto: `parado`, `aceno`, `sobe_desce`, `circulo` |
| **Janela** | trecho de sinal que o modelo enxerga de uma vez (2 s = 200 amostras × 3 eixos) |
| ***Stride* / *window increase*** | de quanto em quanto tempo uma nova janela é fatiada (treino: 200 ms; firmware: 500 ms) |
| **DSP / *feature*** | resumo da janela por FFT (energia por faixa de frequência) que entra na rede |
| ***Impulse*** | o pipeline completo no Studio: janela → DSP → classificador |
| **Treino / validação / teste** | dados para ajustar / para acompanhar durante o treino / **nunca vistos**, para medir de verdade |
| ***Overfitting*** | o modelo decorou o treino e falha no mundo real |
| **Matriz de confusão** | tabela real × previsto; mostra **quem** se confunde com **quem** |
| **Inferência** | usar o modelo treinado (no ESP32) sobre dados novos |
| ***Debounce* de decisão** | só mudar a saída após N janelas seguidas concordarem |
| **Latência** | tempo do início do gesto até a classe aparecer no painel |

## Antes de começar: decisões do grupo

Preencham em conjunto (vai para o relatório). Os valores sugeridos funcionam; mudar exige **mudar
nos dois lugares** (Studio e firmware).

| Decisão | Sugestão | Se mudar, lembre de… |
|---|---|---|
| Aplicação e classes (3–4) | `parado`, `aceno`, `sobe_desce`, `circulo` | rótulos sem acento/espaço |
| Taxa de amostragem `fs` | 100 Hz (62,5 Hz é o do dataset público do EI) | `FS_HZ`; manter ≥ 2× a maior frequência útil |
| Janela | 2000 ms (200 amostras) | `JANELA_MS` e Studio; janela menor = espectro mais grosseiro |
| Eixos, unidade | `accX, accY, accZ`, em **g** | nunca misturar g e m/s² |
| Fundo de escala | ±2 g (`MPU_RANGE 0`) | se saturar, ±4 g nos **dois** firmwares |
| Filtro DLPF | `3` (banda ~44 Hz) | idem nos dois firmwares |
| Tópico MQTT | `ifes/<bancada>/movimento` | padrão da semana 14 (`ifes/#`) |
| Quem é a **pessoa de teste** | 1 integrante/colega que **não** entra no treino | defina agora |

## Etapa 0 — Conta, projeto e o pré-requisito de hardware

**Objetivo:** ter o ambiente pronto e confirmar que o MPU-6050 lê no ESP32.

![Ligação do ESP32 ao MPU-6050: 3V3, GND, SDA no GPIO21, SCL no GPIO22 e AD0 em GND](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_fiacao.png)
*Figura T-4 — A mesma ligação do Lab 9 (4 fios + jumper AD0).*

**Passo a passo**

1. Crie a conta Developer em <https://edgeimpulse.com/signup> e um projeto novo no
   [Studio](https://docs.edgeimpulse.com/studio). **Anote o *project ID*.**
2. Instale a [Edge Impulse CLI](https://docs.edgeimpulse.com/tools/clis/edge-impulse-cli) e as
   ferramentas Python do lab:

   ```bash
   python3 -m venv ~/venv-tinyml && source ~/venv-tinyml/bin/activate
   pip install numpy pyserial paho-mqtt
   ```

3. Confirme, no ESP32, que o Lab 9 ainda roda: o scanner acha o MPU-6050 em `0x68` e você lê
   aceleração nos três eixos. **Esse é o pré-requisito real** — se o sensor não lê, nada à frente
   funciona.
4. **Sem hardware ainda?** Rode o atalho de 10 min (dados sintéticos + pipeline local) do
   [README do lab](../labs-extra/classificador-movimento/README-classificador.md) para ver o
   pipeline funcionando antes de gravar.

> ✅ **Chegamos:** projeto criado no Studio (com *project ID* anotado) e leitura de (ax, ay, az)
> saindo no monitor serial, com `aZ ≈ 1 g` com a placa parada.

## Etapa 1 — Coletar e rotular os dados de movimento

**Objetivo:** gravar exemplos de cada gesto, já rotulados, de **várias pessoas**, e separar um
teste honesto.

![Os quatro gestos desenhados em relação aos eixos X, Y e Z do sensor](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_gestos.png)
*Figura T-5 — Os gestos e os eixos do sensor. Fixe a orientação do sensor (ou use uma pulseira/caixinha):
orientação diferente é dado diferente.*

**Passo a passo**

1. Defina **3 a 4 classes** distintas e fáceis de repetir (espelhando o dataset público do Edge
   Impulse: idle / wave / updown / snake → `parado, aceno, sobe_desce, circulo`).
2. Grave o **firmware de coleta** ([`coleta_mpu/main.c`](../labs-extra/classificador-movimento/src/coleta_mpu/main.c)):
   100 Hz com `vTaskDelayUntil` (semana 5 — período irregular estraga o espectro), DLPF ligado
   (semana 7 — anti-aliasing) e linha `aX,aY,aZ` em g.

   ![Uma vibração de 70 Hz dobra para 30 Hz ao amostrar a 100 Hz, a menos que o DLPF a atenue antes](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_dlpf_alias.png)
   *Figura T-6 — Por que o DLPF: sem filtro antes de amostrar, uma vibração rápida "finge" ser um
   movimento lento (esboço didático; ver teoria §3.2).*

3. Envie os dados ao Studio por **um** destes caminhos (detalhes no lab, Parte B): o ***Data
   forwarder*** da CLI (`edge-impulse-data-forwarder --frequency 100`, eixos `accX,accY,accZ`); o
   **`tools/gravar_csv.py`** (grava CSV local, **mede a taxa**, avisa de saturação) seguido de
   `edge-impulse-uploader`/*Upload data* (CSV Wizard); ou a **Ingestion API**. Para provar o fluxo
   antes de gravar, dá para **clonar o dataset público** "Continuous motion recognition".
4. **Quanto gravar:** ≥ 6 gravações de 10 s por classe e por pessoa, de **≥ 3 pessoas**
   (≥ 3 min por classe). Varie ritmo, amplitude e mão.
5. **Separe o teste por pessoa/gravação — não por janela:**

   ![Divisão treino/teste: sortear janelas vaza informação; reservar uma pessoa inteira é o teste honesto](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_split.png)
   *Figura T-7 — Janelas vizinhas são quase idênticas: sorteá-las entre treino e teste faz o
   modelo "colar". Reserve uma pessoa inteira (categoria *Test* no Studio).*

> ⚠️ **Erro clássico:** treinar só com os seus próprios gestos e o modelo falhar com o colega. É o
> equivalente de ML do "funcionou na minha bancada". Colete diversidade de propósito.

> ✅ **Chegamos:** cada classe com minutos de dado de **≥ 3 pessoas**, taxa medida ≈ 100 Hz, e um
> conjunto de teste **de uma pessoa reservada**, com as 4 classes, visível na aba *Data
> acquisition*.

## Etapa 2 — Montar o *Impulse*: janela + FFT + classificador

**Objetivo:** desenhar o pipeline que transforma janelas de sinal em uma classe.

![Sinal de um aceno com as janelas de 2 s deslocadas de 0,5 s em 0,5 s](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_janela.png)
*Figura T-8 — A janela deslizante. No Studio você define *Window size* e *Window increase*
(treino); no firmware, o *stride* de inferência.*

**Passo a passo** (aba *Create impulse*)

1. *Time series data*: **Window size = 2000 ms**, **Window increase = 200 ms**, **Frequency =
   100 Hz**; 3 eixos.
2. Adicione o bloco **Spectral Features** — é o **DSP block**. Ele aplica a **FFT** em cada eixo e
   resume a janela pela energia nas faixas de frequência. **Isto é a semana 7 na prática:** o mesmo
   motivo pelo qual a FFT revela a frequência de um sinal faz "aceno rápido" e "círculo lento"
   caírem em regiões diferentes do espaço de features.

   ![Quatro gestos no tempo e no espectro: cada gesto tem eixos e frequências dominantes diferentes](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_espectro.png)
   *Figura T-9 — O que o DSP enxerga (dados sintéticos): no espectro, os gestos se separam por
   **frequência** e por **eixo**.*

3. Adicione o **Learning block** de **Classification** (rede neural pequena). Opcionalmente,
   acrescente **Anomaly Detection (K-means)** para pegar movimentos de nenhuma classe.
4. Clique *Generate features* e abra o **Feature explorer**:

   ![Feature explorer ilustrativo: à esquerda features espectrais com classes separadas; à direita só a média por eixo, que agrupa por pessoa](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_feature_explorer.png)
   *Figura T-10 — O que procurar (dados sintéticos): grupos por **classe** (esquerda). Se duas
   classes se misturam aqui, elas vão se confundir no ESP32 também — volte à Etapa 1.*

> ✅ **Chegamos:** *Impulse* com Spectral Features + Classification (2000 ms / 200 ms / 100 Hz)
> configurado e o Feature explorer mostrando agrupamentos visíveis por classe (print no relatório).

## Etapa 3 — Treinar e ler as métricas com honestidade

**Objetivo:** treinar o classificador e interpretar o resultado sem se enganar.

**Passo a passo**

1. Treine (poucos minutos no plano gratuito). Leia a **matriz de confusão** e a acurácia de
   validação. **Uma acurácia alta demais é suspeita** — costuma significar vazamento ou classes
   fáceis demais.
2. Rode *Model testing* no conjunto separado da Etapa 1 — **esse número é o honesto**.

   ![Matriz de confusão 4×4 com explicação de como ler diagonal e fora da diagonal](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_confusao.png)
   *Figura T-11 — Lendo uma matriz (dados sintéticos): a acurácia global (81%) esconde que o
   `aceno` só é reconhecido em 46% das vezes. A matriz diz **onde** o modelo erra.*

3. Veja a estimativa de **latência e memória** por dispositivo: é o vínculo direto com a semana 2
   (RAM/flash) e a semana 4 (tempo real). Um modelo que não cabe na SRAM ou demora demais por
   janela não serve, por melhor que seja a acurácia.
4. **Itere com disciplina:** mude **uma coisa por vez** (mais pessoas, gestos mais distintos,
   janela, épocas) e registre treino × teste num log.

![Acurácia de teste com janelas sorteadas (vazamento) × uma pessoa reservada, para um modelo que decora e outro sobre features espectrais](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_vazamento.png)
*Figura T-12 — O vazamento (dados sintéticos): o modelo que decora vai de 100% (teste "viciado")
para ~81% (pessoa nova).*

> ⚠️ **Sensível ao overfitting:** acurácia de treino ótima e de teste ruim = o modelo "decorou".
> Mais dados e pessoas, menos épocas, ou classes mais separáveis.

> ✅ **Chegamos:** matriz de confusão entendida (com *recall* por classe), número honesto de
> *Model testing* anotado, e latência/RAM estimadas dentro do que o ESP32 aguenta.

## Etapa 4 — Exportar e rodar no ESP32 (ESP-IDF)

**Objetivo:** transformar o modelo em biblioteca C++ e compilar no ambiente que você já usa — e
**provar** que o ESP32 enxerga o mesmo que o Studio.

![Mapa das abas do Studio: Data acquisition, Create impulse, Spectral features, Classifier, Model testing, Live classification, Deployment](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_studio.png)
*Figura T-13 — Onde fazer cada coisa no Studio (nomes podem mudar).*

Fluxo oficial (documentado em
[Run C++ library on ESP32](https://docs.edgeimpulse.com/hardware/deployments/run-cpp-espressif-esp32)):

1. Na aba **Deployment**, escolha **C++ library** e clique **Build**. Baixe o `.zip`.
2. Clone o repositório de exemplo do próprio Edge Impulse:

   ```bash
   git clone https://github.com/edgeimpulse/example-standalone-inferencing-espressif-esp32 ~/ei-movimento
   ```

3. Descompacte a biblioteca e copie **as pastas** (`edge-impulse-sdk/`, `model-parameters/`,
   `tflite-model/`) para a raiz desse repositório. A estrutura final é um projeto ESP-IDF comum,
   igual aos das semanas 1–10:

   ```
   ~/ei-movimento/
   ├── CMakeLists.txt
   ├── sdkconfig
   ├── main/                 ← aqui entram os arquivos do lab (Etapa 5)
   ├── edge-impulse-sdk/     ← copiado do .zip  (o "motor": DSP + inferência)
   ├── model-parameters/     ← copiado do .zip  (descreve o SEU impulse: janela, eixos, rótulos)
   └── tflite-model/         ← copiado do .zip  (os pesos da SUA rede)
   ```

4. **Teste de sanidade sem sensor:** no Studio, em *Live classification*, copie as **Raw features**
   de uma janela e cole em `static const float features[]` do arquivo principal do exemplo; então:

   ```bash
   idf.py build flash monitor      # 115200 baud
   ```

   A classificação impressa no ESP32 deve **bater** com a que o Studio mostrou para a mesma
   amostra. Mesma entrada, mesma saída, dentro e fora do navegador.

> ✅ **Chegamos:** o ESP32 imprime as probabilidades por classe e elas **coincidem** com o Studio
> (print dos dois lado a lado). Tempos **medidos** (`DSP`, `Classification`) anotados.

## Etapa 5 — Ligar no sensor de verdade e publicar por MQTT

**Objetivo:** substituir o vetor fixo pela leitura ao vivo do MPU-6050 e mandar a classe ao painel,
de forma estável.

![Arquitetura do firmware: amostra_task a 100 Hz escreve no buffer circular sob mutex e notifica inferencia_task, que copia a janela, classifica, aplica debounce e publica por MQTT](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_firmware.png)
*Figura T-14 — O firmware final (semanas 5, 6 e 14 juntas): duas tarefas, um mutex, uma
notificação.*

**Passo a passo** (código completo no [lab, Parte F](../labs-extra/classificador-movimento/lab-classificador.md))

1. Troque o `features[]` estático por um **buffer circular** preenchido pela tarefa de amostragem
   (a mesma da Etapa 1, `vTaskDelayUntil`). A cada *stride* (500 ms), com a janela cheia, chame o
   classificador. Atenção à **ordem e à escala dos eixos**: têm de ser idênticas às da coleta, ou
   o modelo recebe "outra coisa" — o `static_assert` do `classificador.h` ajuda a travar isso.
2. **Primeiro sem o modelo** (`USAR_EI=0`, classificador falso): valide sensor → janela →
   *debounce* → Wi-Fi/MQTT. Só então ligue o modelo.
3. **Estabilize a saída:** limiar de confiança (0,80) **e** *debounce* — exigir **N = 3 janelas
   seguidas** antes de publicar. É o *debounce* da semana 3 aplicado a decisões de ML.
4. Publique em **`ifes/<bancada>/movimento`** (`{"classe":"aceno","conf":0.93,"t":…}`), com o
   `status` online/offline por LWT, como na semana 14:

   ![Fluxo MQTT: o ESP32 publica status, movimento e info; o broker do RPi entrega ao monitor, ao painel e ao mosquitto_sub](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_mqtt.png)
   *Figura T-15 — Tópicos e payloads. No painel do RPi mostre a etiqueta em tempo real; se você fez
   a [trilha full-stack](trilha-fullstack.md), é só mais um tópico no dashboard.*

5. **Meça a latência** (do início do gesto à classe no painel) e compare com a estimativa:

   ![Linha do tempo da latência: a janela se enche do gesto novo e três janelas confiantes seguidas liberam a publicação em t = 2,5 s](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_latencia.png)
   *Figura T-16 — A latência é dominada pela física da janela e pelo *debounce*, não pela CPU
   (ilustrativo; veja a fórmula na teoria §8).*

6. **Teste de campo:** quem **não** gravou o treino faz cada gesto 10 vezes; anote a matriz de
   confusão **ao vivo** e compare com o *Model testing*.

> ✅ **Chegamos:** mover a placa muda a etiqueta no painel em tempo real, de forma estável;
> matriz de campo e latência medidas; vídeo curto da demonstração.

## Quando algo dá errado

![Fluxograma de diagnóstico em três colunas: problema no Studio, Studio bom mas ESP32 ruim, ESP32 bom mas demo com outra pessoa ruim](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_diagnostico.png)
*Figura T-17 — Descubra **em qual elo** está o problema antes de mexer. Detalhes e correções no
[guia de problemas](../labs-extra/classificador-movimento/guia-problemas.md).*

## Cronograma sugerido

| Sessão | Etapa | Entrega ao final |
|---|---|---|
| 1 | 0 + início da 1 | sensor lendo; firmware de coleta; primeiras gravações |
| 2 | 1 | dados de ≥ 3 pessoas, teste separado |
| 3 | 2 | Impulse e Feature explorer |
| 4 | 3 | treino, *Model testing*, log de iterações |
| 5 | 4 | biblioteca no ESP-IDF, sanidade OK |
| 6 | 5 | sensor ao vivo + MQTT, teste de campo |
| 7 | — | polimento, relatório, demonstração |

## Ideias de projeto final com esta trilha

Cada ideia reaproveita o mesmo pipeline; o que muda são as classes, a **fixação do sensor** e a
métrica que importa.

| Ideia | Classes | Fixação / cuidado | Ação / saída | Métrica-chave |
|---|---|---|---|---|
| **Controle por gesto** | `parado`, `aceno`, `circulo` | pulseira, orientação fixa | aceno liga/desliga relé/LED por MQTT (ponte H da semana 8) | precisão (falso positivo aciona sem querer) |
| **Contador de repetições** | `parado`, `sobe_desce` | sensor no pulso/barra | contagem publicada (`monitor_movimento.py --conta`) | erro da contagem vs. manual |
| **Monitor de vibração** | `parado`, `normal`, `desbalanceada` | sensor **colado** na carcaça; rever DLPF e fundo de escala | alerta MQTT se `desbalanceada` | *recall* da classe anômala |
| **Detecção de queda** | `parado`, `andando`, `queda` | cintura/peito; ±8 g provavelmente necessário | alerta MQTT | *recall* de `queda` (perder queda é grave) |

> ⚖️ Quem escolher **queda** ou qualquer uso de saúde/segurança: documente as limitações e
> **nunca** apresente o sistema como confiável sem teste extenso — é um bom exercício de ética de
> engenharia.

## Rubrica resumida

A rubrica completa (100 pontos) está no
[modelo de relatório](../labs-extra/classificador-movimento/relatorio-modelo.md). Em resumo: dados
variados e teste por pessoa; justificativas de PDS (Nyquist, `vTaskDelayUntil`, DLPF); *Impulse*
coerente com o firmware; **avaliação honesta** (treino × teste, matriz); sanidade Studio × ESP32;
sistema ao vivo estável com MQTT; teste de campo; reprodutibilidade (*project ID*, classes, `fs`,
janela, acurácia de **teste**).

## Perguntas frequentes

- **Preciso saber ML?** Não. O pipeline é 90% processamento de sinal — a semana 7 — e o Studio
  cuida da rede. Mas você precisa **ler métricas com honestidade**.
- **Posso usar 62,5 Hz como no dataset do Edge Impulse?** Pode; atualize `FS_HZ`, o Studio e use
  um período inteiro de ticks (16 ms ≈ 62,5 Hz).
- **Posso treinar sem hardware?** Sim, com o dataset público; mas a demonstração final exige o
  sensor no ESP32.
- **O modelo funciona comigo e não com o colega.** Falta diversidade nos dados — Etapa 1.
- **Quero outra janela/taxa.** Mude no Studio **e** em `projeto_config.h`; o `static_assert`
  avisa se esquecer um dos dois.
- **Posso usar outro sensor?** Sim, se o **contrato** (Figura T-2) for respeitado: mesma taxa,
  eixos, unidade e fundo de escala na coleta e na inferência.

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
> As figuras que mostram sinais, espectros, matrizes e acurácias foram geradas com **dados
> sintéticos** apenas para ilustrar o raciocínio; os resultados do **seu** projeto são os que
> contam.
