# Aula 13 — PDS Embarcado e Algoritmos de Controle: Fechando a Malha (U4 / ementa)

> **Pré-requisito**: semanas 5 (`vTaskDelayUntil`), 7 (ADC, filtro) e 8 (PWM).
> **Como usar**: texto autossuficiente. Faça a iteração do Exemplo 13.3 na mão antes do lab —
> a P2 cobra exatamente essa mecânica. Os Exemplos 13.1–13.4 são o modelo das questões 1–6 da
> Lista 5.

Esta é a semana em que a disciplina fecha o círculo — literalmente. Você vai juntar o ADC e
o filtro da semana 7, o PWM da semana 8, o `vTaskDelayUntil` da semana 5 e a lição de jitter
da semana 11 num único firmware que faz o que a engenharia de controle promete desde o
primeiro período: **medir, comparar com o desejado e corrigir, dezenas de vezes por
segundo**. O controlador é o PID — o algoritmo que roda em >90 % das malhas industriais do
planeta — e a planta é honesta e didática: um LED iluminando um LDR dentro de um copo.
Referências mudando, lanterna perturbando, e o firmware segurando a luminosidade no alvo.
"Microprocessamento de algoritmos de controle", diz a ementa; hoje ela sai do papel.

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) escolher a taxa de amostragem de uma malha e implementá-la sem deriva;
- (b) projetar um FIR simples posicionando zeros;
- (c) discretizar o PID e implementá-lo com anti-windup e derivada bem-comportada;
- (d) sintonizar empiricamente uma malha real e analisar sua resposta.

---

## 0. A malha, antes de tudo

Todo controle é um ciclo: compare o medido com o desejado e aja sobre a diferença. O diagrama
abaixo é a aula inteira num desenho — guarde-o, porque cada bloco vira uma seção:

![Diagrama de blocos de uma malha fechada de controle com realimentação](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/malha_fechada.png)

*Figura 13-A — A malha fechada: o somador calcula o erro (alvo − medição), o controlador
decide, o atuador age, o processo responde, o sensor mede e fecha o ciclo. Distúrbios (a
lanterna do lab!) entram pelo processo — e a malha os rejeita sozinha.*

## 1. Amostragem periódica de verdade

Todo o PDS e todo o controle digital assumem uma premissa silenciosa: T_s **constante**. As
fórmulas de integral e derivada discretas (seção 3) têm T_s no meio — se o período real
flutua, a matemática mente. Por isso:

- **No FreeRTOS**: tarefa de controle com `vTaskDelayUntil` (o Exemplo 5.1 existiu para este
  momento) em prioridade **alta** — jitter de µs.
- **No Linux**: jitter de ms (Exemplo 11.1) ⇒ malhas rápidas **não** moram lá; o RPi
  supervisiona, o MCU controla (semana 14 consagra a divisão).

E qual T_s? Regra prática de projeto: **f_s ≥ 10–20× a banda da malha** — rápido o suficiente
para o controlador "enxergar" a dinâmica, sem desperdiçar CPU nem amplificar ruído na
derivada.

**Exemplo resolvido 13.1 (escolha de f_s)** — Malha de luminosidade com constante de tempo
~100 ms ⇒ banda ≈ 1/(2π·0,1) ≈ 1,6 Hz ⇒ f_s = 50 Hz (T_s = 20 ms) dá 31× a banda: folgado e
barato em CPU. É o `TS_MS 20` no topo do firmware. (Amostrar a 10 kHz aqui não melhoraria
nada e pioraria a derivada — questão 1 da Lista 5. Amostragem exagerada é como óculos de
grau errado: mais não é melhor.)

## 2. Filtros digitais FIR: a média móvel cresce

O filtro **FIR** (*Finite Impulse Response*) generaliza a média móvel: a saída é uma
combinação linear das últimas N entradas, y[n] = Σ b_k·x[n−k]. A média móvel é o FIR de
coeficientes iguais (b_k = 1/M). Duas propriedades que fazem do FIR o queridinho do
embarcado: é **sempre estável** (não tem realimentação interna para explodir — a saída é
soma ponderada de entradas, ponto final) e pode ter **fase linear** (todas as frequências
atrasam igual — o sinal não "entorta").

E dá para *projetar* com a média móvel, escolhendo onde ela zera: |H(f)| da média de M
pontos tem **zeros em k·f_s/M** — escolha M para cravar um zero em cima do ruído:

**Exemplo resolvido 13.2 (projeto rápido de FIR)** — Ruído de 60 Hz (rede elétrica!) sobre
sinal lento, f_s = 500 Hz. Média móvel com M = f_s/60 ≈ 8 coloca um **zero** em 500/8 =
62,5 Hz ≈ 60 Hz: atenuação profunda do ruído da rede ao custo de 8 somas por amostra.
Verificação pela resposta em frequência: |H(f)| = |sin(πfM/f_s)/(M·sin(πf/f_s))|; em
f = 62,5 Hz o numerador é sin(π) = 0 ✔. (Quer o zero *exatamente* em 60? f_s = 480 Hz com
M = 8 — bônus da Lista 5.)

No firmware de hoje, um FIR modesto (média móvel M = 4) filtra a **medida** antes do PID — a
seção 3 explica por que a derivada implora por isso.

## 3. PID discreto: da fórmula da apostila ao C

O contínuo que você conhece: u(t) = K_p·e + K_i·∫e dt + K_d·de/dt. Discretizando com Euler
(período T_s), cada termo vira uma linha de C:

```
e[n]  = ref − y[n]                     erro
I[n]  = I[n−1] + K_i·T_s·e[n]          integral = acumulador (a "memória" da malha)
D[n]  = K_d·(e[n] − e[n−1])/T_s        derivada = diferença dividida pelo período
u[n]  = K_p·e[n] + I[n] + D[n]         saída → duty do PWM
```

Intuição de cada ganho na *nossa* malha: **P** reage ao erro presente (sobe = resposta
rápida; demais = oscila); **I** acumula o passado e é o único que **zera o erro em regime** —
é ele que compensa a luz ambiente constante e a zona morta do atuador (lembra o duty mínimo
de partida do Lab 8?); **D** antecipa o futuro amortecendo variações (demais = amplifica
ruído do LDR). A figura abaixo mostra as três personalidades respondendo ao mesmo degrau:

![Respostas ao degrau de controladores P, PI e PID comparadas](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/pid_respostas.png)

*Figura 13-B — P é rápido mas deixa erro permanente; I zera o erro mas oscila e demora; D
amortece o sobressinal. O PID bem sintonizado pega o melhor dos três — e é o que você vai
caçar no laboratório.*

### 3.1 Os dois cuidados que separam a apostila do produto

**1. Saturação + anti-windup.** Nosso u é um duty de 0–4095: satura. Se u satura e a integral
continua acumulando ("windup"), ela cresce sem efeito nenhum — e quando a referência voltar
ao alcançável, esse acumulador gigante mantém a saída no teto por segundos, causando
sobressinal enorme e recuperação lenta. É como um motorista que, com o carro atolado na lama,
continua afundando o acelerador: quando a roda finalmente pega tração, o carro dispara.
Solução *clamping*, direto do firmware:

```c
int sat = (u > 4095) ? 1 : (u < 0 ? -1 : 0);
#if ANTI_WINDUP
if (!(sat == 1 && e > 0) && !(sat == -1 && e < 0))   // só integra se NÃO estiver
    I += Ki * Ts * e;                                 // saturado no sentido do erro
#endif
```

Traduzindo: "se a saída já bateu no teto e o erro pede mais teto, a integral congela". O lab
desliga o `ANTI_WINDUP` de propósito para você **ver** o estrago (referência impossível com
o copo aberto → fecha o copo → a malha dispara).

**2. Derivada bem-comportada.** Dois refinamentos de praxe: derivar **a medida** em vez do
erro (−K_d·(y[n]−y[n−1])/T_s) elimina o "chute derivativo" quando a referência muda em degrau
(o degrau de ref tem derivada infinita; o de y, não — derivar a ref faria a saída dar um
pulo violento a cada mudança de alvo); e **filtrar y antes de derivar** — a diferença
(y[n]−y[n−1]) amplifica exatamente o ruído de quantização do ADC (÷T_s pequeno!), e a média
móvel M = 4 do firmware existe para domá-lo.

### 3.2 Uma iteração na mão (faça antes do lab)

**Exemplo resolvido 13.3 (PID na mão)** — K_p = 0,8; K_i = 2,0; K_d = 0,01; T_s = 0,02 s;
ref = 2000; y = 1500; I anterior = 50; e anterior = 400.

*Passo a passo.*

1. e = 2000 − 1500 = 500 → P = 0,8·500 = **400**
2. I = 50 + 2,0·0,02·500 = 50 + 20 = **70**
3. D = 0,01·(500−400)/0,02 = **50**
4. u = 400 + 70 + 50 = **520**

Com atuador limitado em 4095: sem saturação, integral atualizada normalmente. Se o duty
máximo fosse 500: u = 500 (saturado) e a integral **não** seria atualizada nesta iteração —
o clamping em ação. (A P2 cobra exatamente esta mecânica; a Lista 5, questão 4, é a irmã
gêmea deste exemplo.)

## 4. Sintonia empírica: o método do laboratório

Sem modelo da planta (nosso caso), sintonia manual disciplinada funciona — e ensina:

**Exemplo resolvido 13.4 (sintonia empírica — o que faremos no lab)** — (1) Parta de
K_i = K_d = 0; (2) aumente K_p até resposta rápida com **leve** oscilação (K_p demais: o
LED "respira" sem parar — a malha ganhou tanta força que passa do alvo e volta, passa e
volta); (3) adicione K_i até o erro de regime zerar em tempo razoável (K_i demais: oscilação
lenta e windup fácil); (4) pitada de K_d para amortecer o sobressinal (K_d demais: a saída
"chia" com o ruído do LDR). A cada ajuste, registre um **degrau de referência** e compare
sobressinal e tempo de acomodação — o notebook `analise_pid.ipynb` calcula ambos do CSV.

(Existe sintonia sistemática — o método de Ziegler-Nichols: leve K_p até a oscilação
sustentada, anote o ganho crítico e o período, e aplique fórmulas tabeladas. É a alternativa
da questão 5 da Lista 5; no lab, o método manual ensina mais, porque você *sente* cada
ganho.)

## 5. A malha do laboratório, de ponta a ponta

```
        ┌────────────── ESP32, tarefa a 50 Hz (vTaskDelayUntil) ──────────────┐
 ref ──▶(+)── e ──▶ [ PID + anti-windup ] ── u (0–4095) ──▶ LEDC 5 kHz ──▶ LED
        (−)                                                                  │ luz
         ▲                                                                   ▼
         y ◀── média móvel M=4 ◀── ADC GPIO34 ◀── divisor 10 kΩ ◀── LDR ◀── (copo)
```

Cada bloco é uma semana antiga: divisor e ADC (7), filtro (7), PWM (8), período cravado (5).
O firmware imprime `t_ms,ref,y,u` em CSV pelo monitor — a ponte para a análise no **notebook
Jupyter** (`analise_pid.ipynb`): gráficos de resposta, sobressinal e tempo de acomodação
calculados com pandas/matplotlib. Um detalhe de método experimental: o modo `MODO_PID 0`
roda a malha **aberta** (degraus de duty pré-programados) — é assim que se *identifica* a
planta (constante de tempo ~100 ms que justificou o Exemplo 13.1) antes de fechar a malha.
Medir antes de controlar: engenharia na ordem certa.

> 💡 **Pense aí — por que um copo?** A planta LED→LDR é rápida (constante ~100 ms), segura,
> barata e **reproduzível** — cada bancada tem uma igual. O copo isola a luz ambiente para o
> distúrbio ser controlado (a lanterna, não o sol da janela). É o "motor de bancada" da
> engenharia de controle: simples o bastante para entender, real o bastante para doer.

## Resumindo

- Controle digital exige T_s constante: `vTaskDelayUntil` + prioridade alta no MCU; Linux
  supervisiona, não controla rápido; f_s ≈ 10–20× a banda (Exemplo 13.1: 50 Hz).
- FIR: sempre estável, fase linear; média móvel de M pontos zera em k·f_s/M — dá para
  "mirar" o zero no 60 Hz (Exemplo 13.2).
- PID discreto: P = presente, I = memória que zera regime (e vence zona morta), D =
  amortecedor ruidoso; uma iteração na mão é obrigatória (Exemplo 13.3).
- Produto exige: **anti-windup** (clamping: não integra saturado no sentido do erro) e
  derivada da medida filtrada.
- Sintonia empírica na ordem P → I → D com degraus registrados (Exemplo 13.4); identifique a
  planta em malha aberta antes.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| malha fechada | controle com realimentação da medida |
| planta | o processo controlado (LED→LDR no lab) |
| erro | ref − medida: a entrada do controlador |
| FIR | filtro de resposta finita ao impulso |
| zero do filtro | frequência de atenuação máxima |
| windup | integral acumulando com saída saturada |
| anti-windup (clamping) | congelar a integral quando saturado |
| sobressinal | quanto a resposta passa do alvo |
| tempo de acomodação | tempo para entrar e ficar na faixa do alvo |
| sintonia | escolha de K_p, K_i, K_d |

## 📖 Onde aprofundar (opcional)

- Revisão de Análise de Sinais e Sistemas (amostragem, resposta em frequência).
- Åström & Murray, *Feedback Systems* (gratuito na web) — o capítulo de PID mais bem escrito
  que existe.
- **Molloy, cap. 9** (aquisição) — base experimental desta semana.

## Exercícios

Lista 5, questões 1–6 (estilo dos Exemplos 13.1–13.4).
