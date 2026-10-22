# Teoria de apoio — Classificador de movimento na borda (TinyML)

Apoio conceitual do lab-extra que treina um classificador de movimento no **Edge Impulse** e o roda
no ESP32 com o **MPU-6050** da semana 9. Formato tutorial, com exemplos resolvidos **C.1–C.7**.
Nada aqui é "do zero": é a montagem, numa ordem nova, do que você já viu em amostragem e FFT
(semana 7), I²C/MPU-6050 (semana 9), tarefas FreeRTOS (semanas 5 e 6) e MQTT (semana 14).

> 📌 **Onde isto entra:** leia esta teoria *antes* do [roteiro](lab-classificador.md). A
> [trilha TinyML](../../docs/trilha-tinyml.md) é o mapa geral do projeto; aqui está o "porquê" de
> cada decisão que a trilha pede.
>
> 🧪 **Sobre as figuras:** as figuras desta teoria que mostram sinais, espectros, matrizes de
> confusão e gráficos de acurácia foram geradas com **dados sintéticos** (por
> `tools/gera_sinteticos.py` e `tools/pipeline_host.py`) para ilustrar o raciocínio. Elas **não são
> resultados do Edge Impulse** nem do seu sensor: o número que vale para o seu projeto é o do seu
> *Model testing* com gestos reais.

## 1. O que muda quando a decisão vem de um modelo

Até aqui, toda decisão do seu firmware foi um `if` escrito à mão: *se a inclinação passa de X, faça
Y*. Isso funciona quando você **conhece a regra**. Mas "isto é um aceno?" ou "este motor vibra de
forma anormal?" são perguntas cuja regra é difícil de escrever — dependem do **formato de um sinal
ao longo do tempo**, não de um limiar único.

Um classificador de **aprendizado de máquina** inverte o problema: em vez de você escrever a regra,
você mostra **exemplos rotulados** ("isto é um aceno", "isto é parado") e um algoritmo encontra a
regra sozinho. O resultado é uma função — pequena o bastante para caber no ESP32 — que recebe uma
janela do sinal e devolve a probabilidade de cada classe.

**TinyML** é rodar essa função de inferência num microcontrolador, sem nuvem. As vantagens são as
mesmas que motivam sistemas embarcados desde a semana 1: latência de milissegundos, funciona
offline, gasta pouca energia e não expõe o dado bruto na rede (só a conclusão, em poucos bytes).

| | `if` escrito à mão | classificador treinado |
|---|---|---|
| Quem escreve a regra | você | o algoritmo, a partir de exemplos |
| Quando usar | regra conhecida, limiar simples | regra difícil de escrever, depende de forma/frequência |
| Como testar | casos de teste | **dados de teste que o modelo nunca viu** |
| Como falha | erra onde você não pensou | erra onde faltaram exemplos |
| Custo no MCU | mínimo | DSP + rede: alguns ms e alguns kB |

## 2. O pipeline: por que ele é 90% processamento de sinal

![Pipeline ponta a ponta: fase de treino no Studio e fase de inferência no ESP32, com o contrato (taxa, janela, eixos, unidade) que precisa ser idêntico nas duas](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_pipeline.png)
*Figura C-1 — As duas fases do projeto. O modelo é treinado no Studio com dados coletados pelo
firmware de coleta e depois roda no ESP32 sobre dados que vêm do firmware de inferência. Se os
dois firmwares não produzirem o **mesmo tipo de dado**, o modelo recebe "outra coisa".*

Um classificador de movimento tem três blocos — e os dois primeiros são a semana 7:

```
 acelerômetro ──▶ [janela] ──▶ [DSP: FFT] ──▶ [rede neural pequena] ──▶ classe + confiança
   (semana 9)     (buffer)     (semana 7)        (o "ML" propriamente)
```

- **Janela.** Você não classifica uma amostra isolada, e sim um trecho — tipicamente 1–2 s. A cada
  passo (*stride*), uma janela de N amostras dos 3 eixos é o "quadro" a reconhecer.
- **DSP (*Digital Signal Processing*).** A janela crua tem centenas de números — demais para a
  rede. O bloco de DSP a resume em poucas **características** (*features*). Para movimento, o
  bloco padrão do Edge Impulse (*Spectral Features*) mede a energia do sinal por faixa de
  frequência com a **FFT**: um aceno lento e uma sacudida rápida diferem no **espectro**, mesmo
  com amplitude parecida.
- **Rede neural.** Recebe as características (não o sinal cru) e devolve a probabilidade de cada
  classe. Como o DSP já fez o trabalho pesado, a rede pode ser minúscula.

A lição central: **num bom classificador embarcado, o DSP carrega o piano.** Se as características
já separam as classes, quase qualquer rede pequena acerta; se não separam, nenhuma rede grande
salva. Por isso este lab é, no fundo, um lab de processamento de sinais.

### 2.1 A janela deslizante

![Sinal de um aceno começando em t = 3 s, e as janelas de 2 s deslocadas de 0,5 s em 0,5 s](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_janela.png)
*Figura C-2 — Janela de 2 s (200 amostras) com *stride* de 0,5 s (50 amostras). Janelas vizinhas
compartilham 75% das amostras. A primeira janela só existe depois de 2 s de coleta.*

Há **dois "passos" diferentes**, e é comum confundi-los:

| Parâmetro | Onde vive | Para que serve | Valor sugerido |
|---|---|---|---|
| *Window size* | Studio **e** firmware (têm de coincidir) | quanto sinal o modelo enxerga de uma vez | 2000 ms |
| *Window increase* | **só no Studio**, no treino | de quanto em quanto tempo fatiar as gravações em janelas de treino (mais janelas = mais exemplos) | 200 ms |
| *Stride* de inferência | **só no firmware** (`STRIDE_MS`) | de quanto em quanto tempo o ESP32 classifica | 500 ms |

### 2.2 O que o DSP enxerga: tempo × espectro

![Quatro gestos no tempo (em cima) e no espectro (embaixo): parado sem energia, aceno com pico em ~2,5 Hz no eixo X, sobe-desce em ~1 Hz no eixo Z, círculo em ~1 Hz em X e Y simultâneos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_espectro.png)
*Figura C-3 — Mesmo sensor, quatro gestos (dados sintéticos). No tempo, `sobe_desce` e `circulo`
parecem ondas lentas parecidas; no espectro, mudam **quais eixos** têm energia (Z × X e Y) e **em
que frequência**. Repare também: o `parado` tem só gravidade (≈ 1 g em Z, constante) e energia
quase nula em CA.*

Duas observações que valem ouro na hora de projetar os gestos:

1. **A média (gravidade/inclinação) não distingue gestos.** Ela só diz *como o sensor está
   segurado*. As características úteis são as de **CA**: energia por faixa, RMS, frequência
   dominante. (O *Spectral Features* já trabalha em cima do sinal sem a componente contínua.)
2. **O eixo importa.** Se o gesto muda o eixo dominante (X para o aceno, Z para o sobe-desce), isso
   ajuda a separar; mas só se o sensor estiver **sempre na mesma orientação** — veja a figura de
   gestos no [lab](lab-classificador.md).

## 3. Amostragem: Nyquist decide a taxa (revisão da semana 7)

Antes de coletar um único dado, você escolhe a **taxa de amostragem**. A regra é a de sempre:
`f_s > 2 · f_max`, onde `f_max` é a maior frequência **útil** do movimento.

**Exemplo resolvido C.1 (escolha da taxa).** Movimentos de mão/braço (acenar, girar, sacudir) têm
energia relevante até cerca de 10–15 Hz. Qual `f_s` usar?
*Solução.* Nyquist exige `f_s > 30 Hz`. Na prática usamos folga larga: **100 Hz** captura tudo com
sobra e dá uma janela de 2 s com 200 amostras. Taxas muito acima (1 kHz) só incham o dado — sem
sinal útil lá em cima — e **100 Hz é exatamente o que o `vTaskDelayUntil` com tick de 1 ms
sustenta** com período inteiro (10 ticks). ∎

### 3.1 Taxa uniforme: o jitter vira "ruído espectral"

A FFT **supõe** amostras igualmente espaçadas. Se o período oscila (jitter), o sinal é, na prática,
amostrado em instantes errados e o espectro sai "sujo". Por isso o coletor usa `vTaskDelayUntil`
(referência absoluta, sem deriva acumulada) e não `vTaskDelay` — o mesmo raciocínio do Exemplo 5.1,
agora com consequência direta na acurácia.

### 3.2 Anti-aliasing: o DLPF do MPU-6050

![Espectro com uma vibração de 70 Hz: sem filtro ela dobra para 30 Hz após amostrar a 100 Hz; com o DLPF fica atenuada](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_dlpf_alias.png)
*Figura C-4 — Aliasing no acelerômetro (esboço didático). Uma vibração de 70 Hz (por exemplo, de
um motor próximo) amostrada a 100 Hz aparece como 30 Hz — uma frequência que o gesto não tem — e
contamina as características. O filtro passa-baixas interno (DLPF) do MPU-6050 atenua isso antes de
amostrar.*

Na semana 7 você viu que o filtro **anti-aliasing** vem *antes* do ADC. O MPU-6050 tem um filtro
passa-baixas digital interno, configurado no registrador `CONFIG` (0x1A, campo `DLPF_CFG`). O
firmware do lab usa `DLPF_CFG = 3` (banda do acelerômetro da ordem de 44 Hz, conforme o *Register
Map* do MPU-6000/6050). Duas regras:

- é uma configuração do **sensor**, e por isso precisa ser **igual na coleta e na inferência**;
- ela não substitui a boa prática de **afastar o sensor** de fontes de vibração fora do gesto.

### 3.3 Fundo de escala e saturação

O firmware usa ±2 g (16384 LSB/g), como no Lab 9. Gestos vigorosos podem passar de 2 g e o sensor
**satura** (os valores ficam presos em ±2,0): o espectro vira uma onda quadrada, com harmônicos
falsos. Se `tools/gravar_csv.py` avisar de amostras saturadas, suba `MPU_RANGE` (±4 g) **nos dois
firmwares** — `LSB_POR_G` se ajusta sozinho — e regrave os dados.

## 4. Do espectro à decisão: resolução em frequência e tamanho da janela

**Exemplo resolvido C.2 (por que 2 s e não 0,5 s).** A FFT de uma janela de `N` amostras a `f_s`
tem resolução `Δf = f_s / N`. Compare janelas de 2 s e de 0,5 s a 100 Hz para separar um aceno
(~2,5 Hz) de um sobe-desce (~1,2 Hz).
*Solução.* Janela de 2 s: `N = 200`, `Δf = 100/200 = 0,5 Hz`; os dois gestos ficam a `(2,5 − 1,2)/0,5 ≈ 2,6`
bins um do outro — separáveis. Janela de 0,5 s: `N = 50`, `Δf = 2 Hz` — os dois picos caem em
bins vizinhos e se **misturam**; além disso, meio segundo contém só ~0,6 ciclo do sobe-desce (1,2 Hz × 0,5 s).
Janelas curtas dão resposta mais rápida, mas espectro grosseiro; janelas longas dão espectro fino,
mas demoram mais para "reagir" (seção 8). Por isso o ponto de partida é **2 s**. ∎

**Exemplo resolvido C.3 (quanto dado cabe no cano).** O firmware de coleta imprime
`aX,aY,aZ\n` com 4 casas decimais, até 24 caracteres por linha. A 100 Hz e 115200 baud
(≈ 11 520 bytes/s, pois cada byte usa 10 bits no UART), cabe?
*Solução.* `24 B × 100 Hz = 2 400 B/s`, que é `2 400 / 11 520 ≈ 21%` da capacidade da serial. Cabe
com folga. Já a 1 kHz seriam 24 000 B/s > 11 520 — **não cabe**, e nem precisamos. ∎

## 5. Dados: quantidade, variedade e divisão treino/teste

**Exemplo resolvido C.4 (quantas janelas saem de uma gravação).** Uma gravação de 10 s, janela de
2 s, *window increase* de 200 ms. Quantas janelas? E 3 minutos de uma classe, em gravações de 10 s?
*Solução.* `(10 − 2)/0,2 + 1 = 41` janelas por gravação; 3 min = 180 s = 18 gravações →
`18 × 41 = 738` janelas. **Mas** janelas vizinhas compartilham 90% das amostras: a quantidade de
informação *independente* é bem menor (da ordem de `180/2 = 90` janelas não sobrepostas). Mais
janelas por sobreposição **não substituem** mais gravações, de outras pessoas e de outros jeitos. ∎

![Duas formas de dividir treino e teste: sortear janelas (errado, vazamento) ou reservar pessoas inteiras (certo)](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_split.png)
*Figura C-5 — A divisão tem de ser por **gravação/pessoa**, não por janela. Se janelas vizinhas
(quase idênticas) caem uma no treino e outra no teste, o teste vira "prova com gabarito".*

![Acurácia no teste de dois modelos: com janelas sorteadas o modelo que decora chega a 100%; com uma pessoa reservada cai para 81%, enquanto o modelo em features espectrais mantém 99%](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_vazamento.png)
*Figura C-6 — O mesmo conjunto (sintético), duas divisões. Um modelo que **decora** a janela
crua (1-vizinho mais próximo) parece perfeito com janelas sorteadas e cai para ~81% quando uma
pessoa inteira é reservada. O modelo sobre **features espectrais** generaliza nas duas.*

## 6. Overfitting e como lê-lo

O perigo de todo classificador é **decorar** os exemplos de treino em vez de **aprender** o padrão.
A defesa é honestidade experimental: separe **treino** e **teste**, treine só com os de treino e
meça no de teste — dados que o modelo nunca viu.

**Exemplo resolvido C.5 (ler a diferença).** Um grupo obteve 99% de acurácia no treino e 71% no
teste. O que isso significa?
*Solução.* Diferença grande (99 → 71) é a assinatura clássica de **overfitting**: o modelo
decorou. Causas comuns: poucos exemplos, todos gravados do mesmo jeito (mesma pessoa, mesma posição
do sensor) ou **vazamento** (Figura C-5). Remédios: coletar mais e mais variado, reduzir a rede,
aumentar o passo da janela. O número que importa para o projeto é o **de teste** (71%). ∎

Duas pegadinhas inversas — *acurácia alta demais* também é suspeita:

- **Vazamento** (gravação em treino e teste);
- **Classes fáceis demais** (ex.: parado × sacudindo forte) — o número alto não prova que o modelo
  distinguirá gestos parecidos.

## 7. Matriz de confusão: onde o modelo erra

![Matriz de confusão 4×4 em percentuais por linha, com a explicação de como ler diagonal e fora da diagonal](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_confusao.png)
*Figura C-7 — Matriz de confusão (dados sintéticos, classificador 1-NN em janela crua, validação
deixando uma pessoa de fora). Cada **linha** é a classe real e soma 100%.*

**Exemplo resolvido C.6 (da matriz às métricas).** Com as contagens da Figura C-7 (cada classe
tem 1 230 janelas de teste agregadas):

| real \ previsto | parado | aceno | sobe_desce | circulo |
|---|---|---|---|---|
| parado | 1230 | 0 | 0 | 0 |
| aceno | 511 | 567 | 0 | 152 |
| sobe_desce | 141 | 0 | 1089 | 0 |
| circulo | 129 | 19 | 0 | 1082 |

Calcule acurácia, *recall* do `aceno` e precisão do `parado`.
*Solução.* Total = 4 920; diagonal = 1230 + 567 + 1089 + 1082 = 3 968 ⇒ **acurácia = 80,7%**.
*Recall* do aceno (dos acenos reais, quantos o modelo achou) = `567/1230 = 46,1%`. Precisão do
`parado` (das vezes que disse "parado", quantas eram) = `1230/(1230+511+141+129) = 1230/2011 = 61,2%`.
Leitura: o modelo **nunca erra o parado**, mas usa "parado" como refúgio quando não reconhece o
resto — por isso a precisão do `parado` é baixa e o *recall* do `aceno` é pior ainda. A acurácia
global (81%) esconde isso; a matriz mostra. ∎

| Métrica | Pergunta que responde | Fórmula |
|---|---|---|
| Acurácia | quanto acertou no total | acertos / total |
| *Recall* (sensibilidade) da classe | dos exemplos **reais** dela, quantos achei | diagonal / soma da **linha** |
| Precisão da classe | do que **chamei** dela, quanto era | diagonal / soma da **coluna** |
| F1 | média harmônica de precisão e *recall* | 2·P·R/(P+R) |

> 🎯 **Qual métrica importa?** Depende do uso. Detecção de queda: *recall* da classe `queda`
> (perder uma queda é grave). Contador de repetições: precisão (falso positivo conta repetição que
> não houve).

### 7.1 O Feature explorer: a matriz antes do treino

![PCA 2D de features espectrais com 4 nuvens separadas, ao lado de PCA só da média por eixo, que agrupa por pessoa e não por gesto](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_feature_explorer.png)
*Figura C-8 — O equivalente do *Feature explorer* (dados sintéticos). À esquerda, as features
espectrais formam grupos por classe; à direita, só a **média** por eixo (inclinação) agrupa por
**pessoa** — não por gesto. Se o seu explorer parecer com o da direita, o DSP não está ajudando.*

Se duas nuvens se sobrepõem no explorer, a rede não vai separá-las melhor que isso: volte aos
dados (gestos mais distintos, mais exemplos, outra janela).

## 8. Da classe à ação: janela, *debounce* e latência

O classificador devolve uma classe **a cada janela**, e janelas vizinhas são quase iguais, mas o
ruído faz a saída "piscar". Duas defesas:

- **limiar de confiança:** só aceitar a classe se `prob ≥ 0,80` (ajustável);
- ***debounce* de decisão:** só mudar a saída após **N janelas seguidas** concordarem (é o
  *debounce* da semana 3, aplicado a decisões de ML). Está em `src/inferencia_mqtt/main/debounce.h`.

**Exemplo resolvido C.7 (quanto o debounce ajuda).** Suponha que a cada janela o modelo erre
para uma classe específica com probabilidade `p = 5%`, e `N = 3`. Quantas vezes por hora ele
publicaria essa classe errada?
*Solução.* Se os erros fossem independentes, 3 seguidos ocorrem com `p³ = 0,05³ = 1,25·10⁻⁴`, ou
1 vez a cada 8 000 janelas; com uma janela a cada 0,5 s isso é 4 000 s ≈ **67 minutos** entre
publicações erradas — contra um erro a cada 10 s sem *debounce*. Cuidado: erros de janelas
vizinhas **não são independentes** (compartilham 75% das amostras), então o ganho real é menor;
o número serve de ordem de grandeza, e o teste de verdade é deixar o sistema rodando. ∎

![Linha do tempo da latência: janelas sucessivas se enchendo do gesto novo, até a terceira janela confiante seguida liberar a publicação em t = 2,5 s](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_latencia.png)
*Figura C-9 — De onde vem a latência (ilustrativo). A janela precisa se encher do gesto novo antes
de o modelo ficar confiante, e o *debounce* exige N janelas seguidas.*

A latência percebida é, em ordem de grandeza:

```
latência ≈ t_encher + (N − 1)·stride + t_inferência + t_rede
         ≈ 1,5 s    + 2 × 0,5 s     + ~0,01 s       + ~0,01 s  ≈ 2,5 s
```

`t_encher` é a fração da janela (aqui 75% de 2 s) que precisa conter o gesto novo para a
confiança passar do limiar — depende do seu modelo. O ESP32 inferir em poucos milissegundos é o
de menos; **o que domina é a física da janela**. Se 2,5 s é muito para a sua aplicação, reduza a
janela (pagando em resolução, seção 4) ou o `N`.

## 9. Orçamento de recursos (semanas 2 e 4)

**Memória** (ESP32 tem cerca de 520 kB de SRAM, dividida com Wi-Fi, FreeRTOS e pilhas):

| Item | Tamanho | Observação |
|---|---|---|
| Buffer circular | 200 × 3 × 4 B = **2 400 B** | `janela.h` |
| Cópia linear para o DSP | **2 400 B** | feita sob mutex, para não bloquear a amostragem |
| Pilha da `inferencia_task` | 16 kB (ponto de partida) | o SDK do EI usa bastante pilha; se travar, aumente |
| Modelo + arena | **o Studio informa** (aba *Classifier*, estimativa de RAM/flash por dispositivo) | confira antes de exportar |

**Tempo:** a inferência (DSP + rede) tem de terminar **bem antes do próximo *stride*** (500 ms),
senão as janelas se acumulam. O Studio estima a latência por dispositivo; o firmware imprime os
tempos medidos de verdade (`dsp=… clf=… ms`) e os publica em `…/movimento/info`. Compare **estimado
× medido** no relatório.

## 10. O firmware: duas tarefas, um mutex

![Arquitetura do firmware: amostra_task a 100 Hz escreve no buffer circular sob mutex e notifica inferencia_task, que copia a janela, classifica, aplica o debounce e publica por MQTT](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/tml_firmware.png)
*Figura C-10 — Arquitetura do firmware de inferência (`src/inferencia_mqtt/main/main.cpp`).*

Decisões e os porquês:

- **`amostra_task` com prioridade maior (6) que `inferencia_task` (5):** a amostragem cravada em
  100 Hz *preempta* a inferência. Sem isso, a janela de cálculo atrasaria as leituras e o espectro
  do dado ao vivo sairia diferente do espectro do treino.
- **Mutex só em volta do `memcpy`:** a seção crítica dura microssegundos; a inferência (ms) roda
  sobre a **cópia**.
- **`xTaskNotifyGive` em vez de *polling*:** a `inferencia_task` fica bloqueada (0% de CPU) até
  haver janela nova — mais simples e mais leve que o `cheio = 1` + `vTaskDelay` da versão anterior
  do lab.
- **I²C com erro:** repete-se a última amostra boa e conta-se o erro (`i2c_err` em
  `…/movimento/info`). Descartar a amostra mudaria o período; repeti-la o mantém.

## Resumindo

- TinyML = rodar a inferência de um classificador no próprio MCU: baixa latência, offline, privado.
- O pipeline é janela → DSP (FFT, *Spectral Features*) → rede pequena. O DSP é a semana 7.
- A taxa vem de Nyquist (100 Hz) e precisa ser uniforme (`vTaskDelayUntil`); ligue o **DLPF** do
  sensor e mantenha **toda a configuração idêntica** na coleta e na inferência.
- Divida treino/teste **por pessoa/gravação**; leia a **matriz de confusão**, não só a acurácia.
- *Debounce* + limiar tornam a saída estável; a latência é dominada pela janela, não pela CPU.
- Registre estimado × medido (latência/RAM) e o *project ID* para reprodutibilidade.

## 📖 Onde aprofundar (opcional)

- Edge Impulse — [Motion feature extraction](https://docs.edgeimpulse.com/knowledge/concepts/data-engineering/motion-feature-extraction)
  (o bloco *Spectral Features* em detalhe).
- Edge Impulse — [Getting started for embedded engineers](https://docs.edgeimpulse.com/knowledge/guides/getting-started-for-embedded-engineers).
- Edge Impulse — [Increasing model performance](https://docs.edgeimpulse.com/knowledge/guides/increasing-model-performance).
- InvenSense — *MPU-6000/MPU-6050 Register Map and Descriptions* (registradores 0x1A, 0x1C, 0x3B).
- A [trilha TinyML](../../docs/trilha-tinyml.md), para levar isto ao projeto final.
