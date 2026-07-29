# Sistemas Embarcados — Engenharia Elétrica (Campus Guarapari) (2026/2)

Repositório da disciplina Sistemas Embarcados (5º período, 60 h = 30 h teoria + 30 h laboratório, 4 aulas/semana). Pré-requisito: Sistemas Digitais.

Este repositório está sendo construído como uma apostila: cada semana traz a teoria em formato tutorial (conceitos do zero, para quem não tem os livros), o roteiro de laboratório guiado e os códigos-fonte completos. Slides acompanham todas as aulas.

**Curso:** Engenharia Elétrica — 5º período — Ifes — campus Guarapari

**Carga horária:** 60 h (30 h teoria + 30 h laboratório) — 4 aulas/semana, 15 semanas

**Pré-requisito:** Sistemas Digitais

**Plataformas:** ESP32 (ESP-IDF + FreeRTOS) no Bloco 1; Raspberry Pi 3 (Linux embarcado) no Bloco 2

**Simulador:** [Wokwi](https://wokwi.com) para o ESP32 (validação sem hardware)

## Objetivos

**Geral (PPC):** estudar o funcionamento e a aplicação dos microcontroladores na implementação de soluções de engenharia.

**Específicos (PPC):** projetos baseados em microcontroladores; estudo de processadores; estudo de memória e periféricos.

Na prática, vamos buscar:

1. Compreender a arquitetura de microcontroladores e de sistemas Linux embarcados e escolher entre eles conforme os requisitos;
2. Programar em C e Python para hardware, do acesso a registradores aos sistemas operacionais de tempo real (FreeRTOS);
3. Interfacear sensores e atuadores, dimensionando corretamente os aspectos elétricos;
4. Estudar os principais barramentos de comunicação (UART, SPI, I2C, CAN, MQTT);
5. Implementar aquisição de sinais, filtragem e controle em malha fechada (PID) embarcados;
6. Integrar microcontrolador e Linux numa arquitetura de produto IoT (ESP32 + Raspberry Pi + MQTT).

## Ementa (PPC)

Arquitetura de microcontroladores. Linguagens de programação aplicadas a microcontroladores.
Interfaces de comunicação serial e paralela. Protocolos de comunicação: I2C e CAN. Processamento
digital de sinais. Geração PWM. Microprocessamento de algoritmos de controle. Projetos de
aplicação.

As quatro unidades do PPC e sua carga horária:

| Unidade | Tema | CH |
|---|---|---|
| I | Introdução (histórico, problemas fundamentais, aplicações, tecnologias e arquitetura, projeto, mercado) | 12 h |
| II | Microcontroladores (arquitetura e organização, memórias e registradores, contadores/temporizadores, interrupções) | 12 h |
| III | Software para sistemas embarcados (alto e baixo nível, ambiente de desenvolvimento, simulação, sistemas operacionais) | 22 h |
| IV | Interfaceamento analógico e digital (E/S, conversão A/D e D/A, sensores, atuadores, condicionamento de sinal, comunicação) | 14 h |

> **Nota sobre as plataformas.** O PPC não fixa fabricante; este curso adota **ESP32** (ESP-IDF +
> FreeRTOS, com o simulador Wokwi) e **Raspberry Pi 3** (Linux embarcado) no lugar do
> PIC/MPLAB/Proteus historicamente usados, mantendo todos os conteúdos da ementa e acrescentando
> Linux embarcado e a arquitetura IoT (MQTT), alinhando a disciplina à prática atual do mercado.


## Plataformas

| Bloco | Semanas | Plataforma | Toolchain |
|---|---|---|---|
| 1 — Microcontrolador | 1–10 | **ESP32** (ESP-WROOM-32) | ESP-IDF + FreeRTOS, VS Code, simulador **Wokwi** |
| 2 — Linux embarcado | 11–14 | **Raspberry Pi 3 Model B** | Raspberry Pi OS, Python/gpiozero, C/libgpiod |

> O Raspberry Pi Zero 2 W compartilha o mesmo SoC (BCM2837) e roda todo o material do Bloco 2;
> a semana 14 o menciona como alternativa de baixo custo para os nós.

## Estrutura

```
sis-emb-2026-2/
├── README.md                     ← este arquivo (plano de ensino)
├── docs/                         ← instalação, trilha full-stack e trilha TinyML
├── semana-01/ … semana-14/       ← teoria + laboratório + código de cada semana
│   ├── teoria-XX.md
│   ├── lab-XX.md
│   └── src/                      ← projetos-fonte (ESP-IDF, Python)
├── semana-15/                    ← encerramento: prova P2 e apresentações
├── listas/                       ← listas de exercícios (lista-0X)
├── slides/                       ← 31 apresentações com roteiro falado (ver LEIA-ME.md)
├── projeto-final/                ← regulamento e rubrica do projeto integrador
├── labs-extra/                   ← laboratórios avançados (ex.: medidor de energia PZEM-004T)
└── assets/figuras/               ← figuras da apostila
```

Detalhando cada item:

- `docs/instalacao.md` — preparação do ambiente (Ubuntu 22.04, ESP-IDF, Wokwi, RPi)
- `docs/trilha-fullstack.md` — trilha opcional de dashboard web para o projeto final
- `docs/trilha-tinyml.md` — trilha opcional de TinyML (classificador de movimento no ESP32 com Edge Impulse)
- `semana-XX/teoria-XX.md` — **aula completa em formato tutorial**: conceitos explicados do zero
  (para quem não tem os livros), códigos dissecados linha a linha, saídas de terminal esperadas,
  exemplos resolvidos numerados e seção "Resumindo"
- `semana-XX/lab-XX.md` — roteiro prático guiado: experimentos com tabelas de medição, erros
  provocados de propósito (com a mensagem esperada), entrega itemizada e desafio opcional
- `semana-XX/src/` — códigos-fonte completos (os mesmos dissecados nas teorias)
- `semana-15/` — semana de encerramento: prova P2 e apresentação dos projetos (`LEIA-ME-15.md`)
- `slides/` — **31 apresentações** (14 teorias + 14 laboratórios + semana 15 + lab extra + projeto
  final), com roteiro falado nas notas do apresentador; ver `slides/LEIA-ME.md`
- `listas/` — listas de exercícios (`lista-0X.md`) e gabaritos do professor (`gabarito-0X.md`)
- `projeto-final/` — regulamento e rubrica do projeto integrador (`README-proj-final.md`) + deck
- `labs-extra/` — laboratórios opcionais/avançados (ex.: medidor de energia PZEM-004T com Modbus)
- `assets/figuras/` — figuras da apostila (ver "Sobre as figuras", ao final)

## Como usar nas aulas práticas

Todo lab começa sincronizando este repositório com a versão oficial da semana:

```bash
cd ~/sis-emb && git fetch && git reset --hard origin/main
```

(Atenção: isso **sobrescreve** alterações locais — preserve seus experimentos no repositório da
dupla, criado no Lab 1.)

## Entrega das duplas no GitHub (ao longo de todo o semestre)

A avaliação de laboratório **não** é uma prova única no fim: ela é o **fluxo contínuo de
entregas** que cada dupla acumula no próprio repositório GitHub, semana após semana. Esse
histórico é parte central da nota — e começa já no Lab 1.

**Como funciona:**

- **No Lab 1**, cada dupla cria um repositório **privado** `sis-emb-dupla-XX` (XX = número da
  bancada) e adiciona o colega **e o professor** como colaboradores. A dupla vale para o semestre
  inteiro.
- A estrutura é uma pasta por laboratório (`lab-01/`, `lab-02/`, …), cada uma com um
  `relatorio.md` e os arquivos pedidos na seção **Entrega** do roteiro daquela semana.
- Cada laboratório tem prazo **até a próxima aula prática** e é entregue **por commit** no
  repositório da dupla — não por e-mail nem por outro canal.

**Por que isso importa (e pesa na nota):**

- O **histórico de commits distribuído ao longo do semestre** é critério explícito de avaliação:
  vale **20 % (documentação)** na rubrica do projeto final, e a nota individual pode ser ajustada
  em **±20 %** conforme a participação demonstrada nos commits e na arguição — ver
  [`projeto-final/README-proj-final.md`](projeto-final/README-proj-final.md).
- Commits **pequenos, frequentes e com mensagens descritivas** desde a primeira semana valem mais
  do que um único commit na véspera (que é penalizado e facilmente detectável). Exemplo de boa
  mensagem: `lab01: experimento 2 — LED assimétrico 900/100 ms`; exemplo ruim: `update`.
- O mesmo repositório e a mesma disciplina de versionamento evoluem naturalmente para o **projeto
  final**, que exige o GitHub desde a proposta e culmina no congelamento com a tag `v1.0` na
  semana 15.

> Em resumo: o repositório da dupla é o seu **caderno de laboratório versionado**. Mantê-lo vivo,
> semana a semana, é a forma de entrega da disciplina e uma fração relevante da avaliação — não
> deixe para o fim.

## Roteiro de leitura

**Bloco 1 — ESP32, ESP-IDF e FreeRTOS (semanas 1–10)**

| Semana | Teoria | Laboratório |
|---|---|---|
| 1 | [Arquitetura de computadores e sistemas embarcados](semana-01/teoria-01.md) | [Ambiente ESP-IDF e primeiro blink](semana-01/lab-01.md) |
| 2 | [C para embarcados e registradores](semana-02/teoria-02.md) | [GPIO direto nos registradores](semana-02/lab-02.md) |
| 3 | [Eletrônica básica de bancada](semana-03/teoria-03.md) | [Botões, pull-ups e debounce](semana-03/lab-03.md) |
| 4 | [Interrupções e latência](semana-04/teoria-04.md) | [ISR × polling na prática](semana-04/lab-04.md) |
| 5 | [FreeRTOS: tarefas e tempo](semana-05/teoria-05.md) | [Multitarefa e vTaskDelayUntil](semana-05/lab-05.md) |
| 6 | [Comunicação entre tarefas](semana-06/teoria-06.md) | [Filas, semáforos e mutex](semana-06/lab-06.md) |
| 7 | [ADC, DAC e fundamentos de PDS](semana-07/teoria-07.md) | [LDR: o primeiro sensor de verdade](semana-07/lab-07.md) |
| 8 | [PWM, pontes H e atuadores](semana-08/teoria-08.md) | [Servo e motor DC com L298N](semana-08/lab-08.md) — **Prova P1** no 2º encontro |
| 9 | [Barramentos: UART, SPI e I2C](semana-09/teoria-09.md) | [Scanner I2C e MPU-6050](semana-09/lab-09.md) |
| 10 | [Barramentos industriais e CAN](semana-10/teoria-10.md) | [Rede CAN/TWAI entre placas](semana-10/lab-10.md) |

> **Prova P1 — semana 8** (segundo encontro): avaliação escrita individual cobrindo as
> semanas 1–6. Preparação: Listas 1 e 2 e os Exemplos resolvidos 1.1 a 6.3.

**Bloco 2 — Raspberry Pi 3 e Linux embarcado (semanas 11–14)**

| Semana | Teoria | Laboratório |
|---|---|---|
| 11 | [Linux embarcado e boot do RPi](semana-11/teoria-11.md) | [RPi headless: SSH e terminal](semana-11/lab-11.md) |
| 12 | [GPIO, sensores e barramentos no RPi](semana-12/teoria-12.md) | [LED, botão, DHT11, HC-SR04 e I2C](semana-12/lab-12.md) |
| 13 | [PDS embarcado e controle PID](semana-13/teoria-13.md) | [Malha fechada de luminosidade](semana-13/lab-13.md) |
| 14 | [Wi-Fi, MQTT e arquitetura IoT](semana-14/teoria-14.md) | [ESP32 + RPi + broker MQTT](semana-14/lab-14.md) |

**Semana 15 — Encerramento**

- [Prova P2 e apresentação dos projetos finais](semana-15/LEIA-ME-15.md) — a P2 cobre as semanas
  7–14; na sequência, a demonstração dos projetos e o congelamento do repositório (tag `v1.0`).

**Complementos**

- [Laboratório extra — medidor de energia PZEM-004T (Modbus-RTU)](labs-extra/medidor-energia/lab-extra.md)
  e sua [teoria de apoio (RMS, potência, Modbus, CRC-16)](labs-extra/medidor-energia/teoria-extra.md)
  — **requer supervisão do professor na parte de 220 V**
- [Projeto Final Integrador — regulamento e rubrica](projeto-final/README-proj-final.md)
- [Trilha opcional full-stack — dashboard web no RPi](docs/trilha-fullstack.md)
- [Trilha opcional TinyML — classificador de movimento no ESP32 com Edge Impulse](docs/trilha-tinyml.md)

**Listas de exercícios** (individuais — ver [Avaliação](#avaliação-da-aprendizagem))

| Lista | O quê estudar | Enunciado | Entregar até |
|---|---|---|---|
| 01 | Semanas 1–3 | [lista-01.md](listas/lista-01.md) | aula teórica da semana 4 |
| 02 | Semanas 4–6 | [lista-02.md](listas/lista-02.md) | aula teórica da semana 7 |
| 03 | Semanas 7–10 | [lista-03.md](listas/lista-03.md) | aula teórica da semana 11 |
| 04 | Semanas 11–12 | [lista-04.md](listas/lista-04.md) | aula teórica da semana 13 |
| 05 | Semanas 13–14 | [lista-05.md](listas/lista-05.md) | 1º encontro da semana 15 |

## Slides

A pasta `slides/` traz **31 apresentações**, uma por aula (teoria e laboratório),
mais a semana 15, o lab extra de energia e o projeto final.

## Cronograma (15 semanas)

As unidades (U1–U4) seguem o PPC da disciplina. A **P1** ocorre no 2º encontro da semana 8; a
**P2**, no 1º encontro da semana 15 (as apresentações dos projetos ficam no 2º encontro).

| Semana | Unidade | Atividade |
|---|---|---|
| 1 | U1 | Introdução aos sistemas embarcados; ambiente ESP-IDF e primeiro blink |
| 2 | U1/U2 | Arquitetura do ESP32 e do RPi; GPIO direto nos registradores |
| 3 | U2 | C para embarcados e eletrônica de bancada; botões, pull-ups e debounce |
| 4 | U2 | Interrupções, temporizadores e latência; ISR × polling |
| 5 | U3 | FreeRTOS I: tarefas e escalonamento; multitarefa e `vTaskDelayUntil` |
| 6 | U3 | FreeRTOS II: comunicação entre tarefas; filas, semáforos e mutex |
| 7 | U4 | ADC, DAC e fundamentos de PDS; LDR, o primeiro sensor |
| 8 | U4 | PWM e atuadores; servo e motor com L298N — **Prova P1** (2º encontro) |
| 9 | U4 | Barramentos UART, SPI e I2C; scanner I2C e MPU-6050 |
| 10 | U4 | Barramento industrial CAN/TWAI; rede entre placas |
| 11 | U3 | Linux embarcado e boot do RPi; RPi headless por SSH — início do Bloco 2 |
| 12 | U4 | Interfaceamento físico no RPi; LED, botão, DHT11, HC-SR04 e I2C |
| 13 | U4 | PDS embarcado e controle PID; malha fechada de luminosidade |
| 14 | U4 | Wi-Fi, MQTT e arquitetura IoT; ESP32 + RPi + broker |
| 15 | — | **Prova P2** (1º encontro) e apresentação dos projetos finais — repositório em `v1.0` |

## Estratégias de aprendizagem

Conforme o PPC: aula expositiva; exercícios de análise e síntese; roteiros de laboratório; estudo
de caso; trabalhos em grupo; resolução de situações-problema.

Cada encontro segue o ciclo: **(i)** exposição teórica pelo deck da semana (com exemplos
resolvidos em destaque); **(ii)** demonstração guiada no hardware ou no Wokwi; **(iii)** prática
do estudante no roteiro de laboratório, com experimentos e erros provocados de propósito;
**(iv)** discussão dos resultados e entrega. Muitos laboratórios trazem, ainda, um **desafio
opcional** para quem terminar antes.

## Avaliação da aprendizagem

Instrumentos previstos no PPC: avaliação escrita (testes e provas); exercícios; elaboração e
apresentação de trabalhos. Neste curso, tomam a forma:

| Instrumento | Cobre | Como é entregue |
|---|---|---|
| Prova P1 | Semanas 1–6 | Escrita individual, semana 8 (2º encontro) |
| Prova P2 | Semanas 7–14 | Escrita individual, semana 15 (1º encontro) |
| Relatórios de laboratório | Todas as semanas de lab | **Em dupla**, por commit no repositório GitHub da dupla, até a aula prática seguinte — ver [Entrega das duplas no GitHub](#entrega-das-duplas-no-github-ao-longo-de-todo-o-semestre) |
| Listas de exercícios | Blocos de semanas (ver abaixo) | **Individuais**, em PDF ou markdown no GitHub, no prazo de cada lista |
| Projeto final (sistema integrado) | Unidades I–IV | **Em grupo**; rubrica própria em [`projeto-final/README-proj-final.md`](projeto-final/README-proj-final.md) |

### Como funcionam as listas de exercícios

São **cinco listas individuais**, uma por bloco de conteúdo, com questões no mesmo estilo dos
**Exemplos resolvidos** das teorias (conta feita, unidade correta, conclusão de projeto). Cada
lista é entregue **individualmente** — diferente dos relatórios de laboratório, que são em dupla —
**em PDF ou markdown, no GitHub**, até a aula indicada no seu cabeçalho:

| Lista | Cobre | Prazo | Função |
|---|---|---|---|
| 01 | Semanas 1–3 | aula teórica da semana 4 | preparação para a **P1** (semanas 1–6) |
| 02 | Semanas 4–6 | aula teórica da semana 7 | preparação para a **P1** (semanas 1–6) |
| 03 | Semanas 7–10 | aula teórica da semana 11 | preparação para a **P2** (semanas 7–14) |
| 04 | Semanas 11–12 | aula teórica da semana 13 | preparação para a **P2** |
| 05 | Semanas 13–14 | 1º encontro da semana 15 | preparação direta para a **P2** |

Os prazos caem sempre na aula que **abre o bloco seguinte**, para que a lista sirva de revisão e
consolidação antes de o conteúdo novo começar — e, nas semanas de prova, de preparação direta.

### Pesos dos instrumentos

Os critérios priorizados pelo PPC são: a articulação entre o saber estudado e a solução de
problemas reais; a capacidade de análise crítica; a iniciativa e a criatividade nos trabalhos; a
interação e o trabalho em grupo; a organização e a clareza na expressão dos conceitos.

A composição da nota semestral (os pesos abaixo) segue o plano de ensino da turma:

| Instrumento | Peso |
|---|---|
| Prova P1 | _a definir_ |
| Prova P2 | _a definir_ |
| Relatórios de laboratório (duplas) | _a definir_ |
| Listas de exercícios (individuais) | _a definir_ |
| Projeto final | _a definir_ |
| **Total** | **100 %** |

> **Preencher os pesos com os valores do plano de ensino desta turma.** O material não fixa essa
> composição; os números acima são o único ponto do plano que depende exclusivamente do professor.

Dentro do **projeto final**, a distribuição já está definida em sua rubrica: funcionamento 40 %,
qualidade técnica 25 %, documentação 20 % (inclui o histórico de commits do repositório da dupla)
e apresentação 15 %, com a nota individual ajustável em ±20 % pela arguição — ver o regulamento.

## Convenções usadas no texto

- **Exemplo resolvido N.M** — contas e raciocínios completos, no mesmo estilo das questões das
  listas e das provas. Refazer sem olhar é a melhor preparação.
- **Pense aí** — pergunta de verificação rápida no meio do texto; tente responder antes de
  continuar.
- **Observação** — detalhe de implementação ou armadilha clássica de bancada.
- 💡 — dica contextual (nos labs e listas); 🛠️ — tabela de problemas comuns e remédios;
  📌 — vocabulário para revisão; 📖 — referências para aprofundamento (opcionais: a apostila
  já é autossuficiente).
- Figuras numeradas por arquivo: **Figura N-X** na teoria da semana N, **Figura LN-X** no lab
  da semana N. As figuras extraídas dos livros citam a fonte na legenda.

## Bibliografia

**Referências deste curso** (ESP32 + Raspberry Pi):

- MOLLOY, D. *Exploring Raspberry Pi: Interfacing to the Real World with Embedded Linux*. Wiley, 2016. **(referência principal do Bloco 2)**
- MAIR. *ESP32 Course* — vídeos da disciplina + repositório <https://github.com/Mair/esp32-course> **(referência principal do Bloco 1)**
- Espressif. *ESP-IDF Programming Guide* — <https://docs.espressif.com>
- UPTON, E.; DUNTEMANN, J. *Learning Computer Architecture with Raspberry Pi*. Wiley. **(aprofundamento de arquitetura — semanas 2, 3 e 11)**
- SMART, G. *Practical Python Programming for IoT*. Packt. **(eletrônica no RPi e REST/WebSockets/MQTT — semanas 8, 12 e 14)**
- DALMARIS, P. *Raspberry Pi Full Stack*. **(trilha full-stack para projetos — ver `docs/trilha-fullstack.md`)**
- Acervo complementar disponibilizado à turma: ver `docs/instalacao.md#acervo`.

> Cada teoria semanal termina com uma seção **"Onde estudar"** que indica os capítulos e figuras
> específicos de cada livro para aquele tema — este é o mapa de leituras detalhado, mantido junto
> ao conteúdo.

**Bibliografia básica (PPC):**

- CARRO, L. *Projeto e prototipação de sistemas digitais*. Porto Alegre: UFRGS, 2001.
- SHAW, A. C. *Sistemas e software de tempo real*. Bookman, 2003.
- OLIVEIRA, A. S.; ANDRADE, F. S. *Sistemas embarcados: hardware e firmware na prática*. São Paulo: Érica, 2006.

**Bibliografia complementar (PPC):**

- TOSCANI, S. S.; OLIVEIRA, R. S. de; CARISSIMI, A. S. *Sistemas operacionais e programação concorrente*. Sagra Luzzatto, 2004.
- PEREIRA, F. *Tecnologia ARM*. São Paulo: Érica, 2007.
- BUTTAZZO, G. *Hard Real-Time Computing Systems: Predictable Scheduling Algorithms and Applications*. Springer, 2010.
- CARRO, L.; RECH, F. *Sistemas computacionais embarcados*. Campinas: SBC-JAI, 2003.


## Sobre as figuras

O diretório `assets/figuras/` reúne as **Figuras extraídas dos livros de referência** do acervo da disciplina (Packt, Wiley,
   Elektor e Raspberry Pi Press), usadas para fins didáticos e sempre com a fonte citada na
   legenda — fotos de montagens, esquemáticos, capturas de tela de ferramentas e diagramas
   dos capítulos indicados no plano de ensino.