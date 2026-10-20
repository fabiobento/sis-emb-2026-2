# Aula 7 — Interfaceamento Analógico: ADC, DAC e a Porta de Entrada do PDS (U4)

> **Pré-requisito**: Aulas 3 (divisor, GPIO) e 5 (`vTaskDelayUntil` para período exato).
> **Como usar**: texto autossuficiente. Os Exemplos 7.1–7.4 são o modelo das questões 1–5 da
> Lista 3. Esta aula é onde a disciplina encontra a sua pré-requisita de Sinais e Sistemas —
> aproveite para revisar amostragem.

Até aqui seu firmware vive num mundo binário confortável: pino em 0 ou 1, botão apertado ou
solto. Mas o mundo físico é **analógico** — a luz da sala, a temperatura, a tensão de um
sensor variam continuamente, assumindo infinitos valores intermediários. Esta aula constrói a
ponte nos dois sentidos: o **ADC** (conversor analógico→digital), que transforma tensões em
números, e o **DAC** (digital→analógico), que faz o caminho inverso. E, no momento em que
você amostra um sinal no tempo, entra em cena o **processamento digital de sinais** (item
explícito da ementa): quantização, teorema de Nyquist, aliasing e o primeiro filtro digital
da disciplina — a média móvel. No laboratório, um LDR vira seu primeiro sensor de verdade.

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) calcular resolução, LSB e erro de quantização de um ADC;
- (b) projetar um divisor de tensão para um sensor resistivo;
- (c) enunciar e aplicar o teorema de Nyquist e prever a frequência de um alias;
- (d) implementar uma média móvel O(1) e conhecer seus limites (e quando a mediana é melhor);
- (e) gerar formas de onda com DAC por tabela e calcular sua frequência.

---

## 1. O ADC: transformando tensão em número

### 1.1 Quantização e resolução

Um ADC de **N bits** divide sua faixa de entrada (0 a $V_{ref}$) em **$2^N$ níveis** e responde à
pergunta "em qual degrau está a tensão?". O tamanho do degrau é o **LSB** (*least significant
bit* — o valor de um passo):

$$ LSB = \frac{V_{ref}}{2^N}  $$

Tudo que cair dentro do mesmo degrau vira o **mesmo número** — essa perda irreversível é o
**erro de quantização**, no máximo $±\frac{LSB}{2}$. Mais bits ⇒ degraus menores ⇒ mais fidelidade (e
mais custo, e mais sensibilidade a ruído: degrau menor que o ruído do circuito é bit
desperdiçado).

Na primeira figura abaixo temos a função de
transferência de um ADC ideal de 3 bits (8 degraus — repare que toda uma faixa de tensões
vira o mesmo código); Na segunda é o erro de quantização correspondente, o “dente de serra”
confinado entre $±\frac{LSB}{2}$.

![Função de transferência de um ADC ideal de 3 bits](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/adc_3bits.png)

*Figura 7-A — ADC ideal de 3 bits: a tensão contínua entra, o código sai em degraus. Fonte:
Hands-On Industrial Internet of Things (Packt), cap. 3, Fig. 3.5.*

![Erro de quantização confinado entre mais e menos meio LSB](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/erro_quantizacao.png)

*Figura 7-B — O erro de quantização: um “dente de serra” entre −½ LSB e +½ LSB. É
irrecuperável — nenhum filtro posterior devolve a informação perdida dentro do degrau.
Fonte: Hands-On Industrial Internet of Things (Packt), cap. 3, Fig. 3.6.*

E o efeito de aumentar N, lado a lado:

![Comparação das funções de transferência de ADCs de 4, 8 e 12 bits](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/adc_transferencia.png)

*Figura 7-C — Esquerda: mais bits, escada mais fina (4, 8 e 12 bits sobre a mesma faixa). Em
0–3,3 V inteiros as escadas de 8 e 12 bits quase coincidem a olho nu — por isso o retângulo
de zoom amplia uma fatia estreita (1,0–1,3 V), onde as três ficam visivelmente distintas.
Direita: a quantização aplicada a uma senoide — o erro colore o sinal com “textura” digital.*

**Exemplo resolvido 7.1 (resolução e conversão raw→volts)** — ESP32: N = 12, atenuação
11 dB ⇒ faixa útil ≈ 0–3,1 V (não são os 3,3 V completos — detalhe de projeto do chip). Uma
leitura `raw = 2418` corresponde a que tensão?

*Solução passo a passo.*

1. LSB = 3,1 / 4096 ≈ **0,757 mV**; erro de quantização máximo ±0,38 mV (irrelevante diante
   do ruído real do chip, que é de alguns mV — por isso filtramos, seção 4).
2. Conversão: V ≈ 2418 × 3,1/4095 ≈ **1,83 V**. (Divide-se por 4095 e não 4096 porque o
   código máximo, 4095, corresponde ao topo da escala.)

Essa conversão `raw → volts` é a linha `filtrado * 3.1 / 4095.0` do firmware de hoje — cada
constante da fórmula agora tem nome e sobrenome.

> **Observação — o ADC do ESP32 é honesto, não excelente.** Ele é do tipo **SAR**
> (*successive approximation* — aproximações sucessivas: uma "busca binária" em hardware,
> comparando a entrada com um DAC interno bit a bit, N comparações por conversão) e sofre de
> não linearidade e ruído conhecidos. Para a nossa bancada e para controle, serve bem — com
> filtro. Para instrumentação de precisão, a indústria usa ADCs externos (ADS1115 I2C,
> MCP3008 SPI — que reaparecerão na semana 12, quando o RPi, *sem* ADC, precisar de um).

![Esquerda: diagrama de blocos do ADC SAR — comparador, registrador SAR e DAC interno em laço fechado. Direita: a busca binária convergindo para Vin em 5 comparações, uma por bit, do MSB ao LSB, com o registrador SAR sendo preenchido a cada passo](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/adc_sar_busca_binaria.png)

*Figura 7-D — Como o ADC SAR chega ao código final: o registrador testa um bit por vez,
do mais significativo ao menos significativo, comparando a tensão que o DAC interno produz
para aquele "chute" com $V_{in}$. Se o chute ficou **abaixo** de $V_{in}$, o bit fica marcado
como 1 e o registrador segue para o próximo bit somando esse peso; se ficou **acima**, o bit
vira 0 e aquele peso é descartado. N bits ⇒ exatamente N comparações — nem mais, nem menos —
o que também explica por que resolução maior tem um custo direto em tempo de conversão.*

### 1.2 A atenuação do ESP32

O ADC interno mede nativamente ~0–1,1 V; um atenuador programável estica a faixa (0 dB,
2,5 dB, 6 dB, 11 dB). Usamos sempre `ADC_ATTEN_DB_11` (faixa ~0–3,1 V) — compatível com
divisores alimentados em 3,3 V. Tensão acima da faixa não quebra o pino (até 3,3 V), mas
**satura** a leitura em 4095: o conversor “grita” o valor máximo e você não sabe se está em
3,2 V ou em 5 V — saturação também é perda de informação, e diagnóstico comum de “sensor que
não muda”.

**O que "atenuar X dB" significa matematicamente.** O decibel compara duas tensões pela
razão entre elas. Atenuar o sinal em $A$ dB quer dizer que a tensão que chega ao núcleo do
conversor é menor que a do pino por um fator $10^{A/20}$ (usa-se 20·log₁₀ porque a razão é
de *tensões*, ou seja, de amplitudes):

$$ A_{dB} = 20\log_{10}\left(\frac{V_{pino}}{V_{ADC}}\right) \quad\Rightarrow\quad V_{ADC} = V_{pino}\cdot 10^{-A_{dB}/20} $$

O núcleo SAR só enxerga ~0–1,1 V. Se o sinal é dividido por $10^{A/20}$ *antes* de chegar
lá, o fundo de escala **no pino** é multiplicado pelo mesmo fator:

$$ V_{FS}(A) \approx 1{,}1\ \text{V}\cdot 10^{A/20} $$

| Atenuação | Fator de tensão $10^{A/20}$ | $V_{FS}$ nominal no pino |
|---|---|---|
| 0 dB | 1,00 | 1,1 V |
| 2,5 dB | 1,33 | ≈ 1,5 V |
| 6 dB | 2,00 | ≈ 2,2 V |
| 11 dB | 3,55 | ≈ 3,9 V (nominal) → usamos ≈ 3,1 V |

Resumo: **6 dB ≈ ×2 em tensão** (e 20 dB = ×10). Quanto ao 11 dB, a conta nominal dá
3,9 V, mas perto do topo a curva do ESP32 deixa de ser linear e o fundo de escala varia de
chip para chip; por isso a disciplina adota ≈ 3,1 V como faixa **útil** (e é esse o valor
que entra no LSB e na conversão `raw → volts` do Exemplo 7.1).

**O preço é resolução.** O ADC continua com 4096 degraus, só que agora eles cobrem uma
faixa maior: $LSB = V_{FS}/4096$ cresce pelo mesmo fator da atenuação. Com 0 dB o degrau é
≈ 0,27 mV; com 11 dB (faixa de 3,1 V) é ≈ 0,76 mV. Escolher a atenuação é, portanto, uma
troca: *faixa maior* (cabe um sinal de até 3,1 V sem saturar) contra *degrau mais grosso*
(menos detalhe fino). Em linguagem de projeto: use a **menor** atenuação cuja faixa ainda
comporta o seu sinal inteiro; na nossa bancada, com divisores alimentados em 3,3 V, isso
significa 11 dB.

![Esquerda: curvas de transferência do ADC do ESP32 para as atenuações de 0, 2,5, 6 e 11 dB, cada uma saturando em 4095 no seu fundo de escala (≈1,1; 1,5; 2,2 e 3,1 V). Direita: um sensor variando entre 1,3 V e 2,8 V lido com 0 dB fica travado em 4095, enquanto com 11 dB a leitura acompanha o sinal](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/adc_atenuacao_saturacao.png)

*Figura 7-E — Esquerda: cada atenuação estica a faixa do conversor, mas todas terminam num
"teto" em 4095; com `ADC_ATTEN_DB_11` esse teto está em ≈3,1 V, então 3,2 V e 3,3 V
(ainda seguros para o pino) viram o mesmo número. Direita: o sintoma no firmware — um
sensor que varia de 1,3 V a 2,8 V lido com atenuação de 0 dB (teto em 1,1 V) devolve 4095 o
tempo todo, e parece "morto"; trocando para 11 dB a leitura volta a acompanhar o sinal.*

## 2. Condicionamento: o divisor de tensão

O ADC lê **tensão**; muitos sensores baratos variam **resistência** (LDR: luz; NTC:
temperatura; sensor de solo resistivo: umidade). A ponte entre os dois mundos é o circuito
mais importante da disciplina — o **divisor de tensão**:

```
   3V3 ──[ R_sensor ]──┬──[ R_fixo ]── GND
                       │
                      ADC (GPIO34)          V_out = 3,3 · R_fixo / (R_sensor + R_fixo)
```

A fórmula sai da lei de Ohm em duas linhas:
- a mesma corrente atravessa os dois resistores,
$$I = \frac{3,3}{(R_{sensor} + R_{fixo})} $$
- $V_{out}$ é a queda de tensão $I\times R_{fixo}$.

A foto mostra o componente real: um LDR (*light-dependent resistor* — resistor dependente de
luz) e seus símbolos esquemáticos. A resistência cai quando a luz bate no material
semicondutor da superfície serpentada — fotóns liberam portadores de carga.

![Fotografia de um LDR e seus símbolos esquemáticos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/ldr_componente.png)

*Figura 7-F — O LDR: componente físico e símbolos. Fonte: Practical Python Programming for
IoT (Packt), cap. 9, Fig. 9.4.*

**Exemplo resolvido 7.2 (LDR + divisor)** — LDR varia de 8 kΩ (claro) a 120 kΩ (escuro), em
série com $R_{fixo}$ = 10 kΩ (LDR em cima, como no desenho). Calcule a excursão de tensão.

*Solução passo a passo.*

- Claro: V = 3,3 × 10/(8+10) = **1,83 V**
- Escuro: V = 3,3 × 10/(120+10) = **0,25 V**
- Excursão: 1,83 − 0,25 ≈ **1,58 V** — mais da metade da faixa do ADC, ótimo aproveitamento.

E o valor de $R_{fixo}$? A regra do **máximo contraste** é $R_{fixo}$ ≈ √(R_claro × R_escuro) = √(8k × 120k) ≈ 31 kΩ; nosso 10 kΩ (o que há aos montes no inventário) entrega um pouco menos
de excursão e mais corrente — compromisso consciente, típico de projeto real (“usa o que
tem” é uma restrição legítima). Repare na lógica: mais luz ⇒ menos resistência ⇒ **mais**
tensão. Se quiser a lógica invertida (mais luz, menos tensão), troque sensor e resistor de
lugar — e a semana 13 precisará saber qual é a sua convenção antes de fechar a malha!

> 💡 **Pense aí**: por que não ligar o LDR direto entre 3,3 V e o ADC, sem resistor? *Porque
> resistor não “gera” tensão sozinho: a variação de resistência só vira variação de tensão
> quando uma corrente a atravessa — e é o $R_{fixo}$ quem fecha o caminho da corrente e cria o
> ponto de comparação. Sem ele, o pino veria 3,3 V sempre (até a corrente de fuga mudar
> isso de forma imprevisível).*

## 3. Amostragem: o teorema que não perdoa

Até aqui falamos de **quanto** o ADC mede (resolução: eixo vertical). Agora falamos de
**quando** ele mede (taxa: eixo horizontal). As duas decisões são independentes — e o erro
em qualquer uma delas é irreversível.

### 3.1 O que é amostrar

O ADC não enxerga o sinal contínuo: ele tira **fotografias** a cada $T_s$ segundos, ou seja,
com **taxa de amostragem** $f_s = \frac{1}{T_s}$. Entre duas fotografias, o conversor não sabe
nada sobre o sinal; o que sobra para o firmware é uma **lista de números** igualmente
espaçados no tempo. (Por dentro, um circuito *sample-and-hold* captura a tensão no instante
da amostra e a "segura" estável enquanto o conversor trabalha, como no SAR da seção 1.1.)

![Em cima, sinal contínuo com as amostras marcadas a cada Ts. Embaixo, a sequência de números resultante, vista como degraus segurados entre as amostras](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/amostragem_sample_hold.png)

*Figura 7-G — Amostragem em duas etapas. Em cima: o mundo real é contínuo, mas o ADC só olha
nos instantes marcados (a cada $`T_s`$ = 40 ms, ou $`f_s`$ = 25 Hz). Embaixo: o que o firmware
recebe é uma sequência de números; o que acontece **entre** as amostras é informação que não
chegou ao programa.*

A pergunta natural é: *quão rápido preciso fotografar para não perder nada importante?*

### 3.2 O teorema de Nyquist–Shannon

O **teorema de Nyquist–Shannon** responde:

> Para representar fielmente um sinal cujas componentes vão até $f_{max}$, é preciso amostrar a **$f_s > 2·f_{max}$**.

Chamamos $f_s/2$ de **frequência de Nyquist**: é o teto do que o sistema consegue "enxergar".
Três observações para não errar a leitura do teorema:

- **$f_{max}$ é a maior frequência *presente* no sinal, não a que você quer medir.** Ruído,
  harmônicos (uma onda quadrada tem harmônicos muito além da fundamental) e interferências
  (rede elétrica, luz de LED) contam — se existem no pino, o ADC as amostra.
- **A desigualdade é estrita.** Com $f_s = 2f$ exatos, tudo depende de *onde* as amostras
  caem no ciclo: no pior caso, caem sobre os zeros e o sinal desaparece (painel de baixo à
  esquerda da figura abaixo).
- **"Fiel" supõe reconstrução ideal.** Com $f_s$ um pouco acima de $2f$ a informação *existe*
  nas amostras, mas recuperá-la exige o interpolador ideal de Shannon (somas de funções
  $\text{sinc}$), que não conseguimos construir. Na prática, usa-se bem mais que o mínimo
  (regra de bolso: **5–10× a banda de interesse**).

![Quatro painéis com uma senoide de 5 Hz amostrada a 50, 12, 10 e 6 Hz](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/amostragem_taxas.png)

*Figura 7-H — A mesma senoide de 5 Hz amostrada com quatro taxas diferentes. Com 50 Hz
(10×) e 12 Hz (2,4×) a curva reconstruída (tracejada) coincide com o original. Com 10 Hz
(exatamente 2×), as amostras caem nos zeros e o sinal some. Com 6 Hz (1,2×), as mesmas
poucas amostras passam a descrever, perfeitamente bem, uma senoide **lenta de 1 Hz** — que
não existe no mundo real.*

### 3.3 Aliasing: quando o sinal rápido se disfarça de lento

Violou Nyquist? O sinal rápido demais não desaparece — ele se **disfarça** de um sinal lento,
fenômeno chamado **aliasing** (*alias* = pseudônimo). É o efeito "roda de carroça girando para
trás" do cinema: a câmera amostra a roda 24× por segundo; se ela gira quase nessa taxa, o filme
a mostra girando devagar... e, se gira um pouco *menos* que uma volta por quadro, para trás.

O impostor aparece na frequência

$$ f_{alias} = \left| f_{sinal} - k\,f_s \right|, \qquad k = \text{inteiro mais próximo de } f_{sinal}/f_s $$

que, para $f_s/2 < f_{sinal} < f_s$ (o caso mais comum), simplifica para
$f_{alias} = f_s - f_{sinal}$. Note que o alias é **sempre** uma frequência entre $0$ e $f_s/2$:
qualquer frequência acima do teto de Nyquist é "dobrada" de volta para dentro da faixa.

![Sinal de 9 Hz amostrado a 10 Hz produzindo um alias de 1 Hz](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/aliasing_nyquist.png)

*Figura 7-I — Aliasing: as amostras (bolas vermelhas) do sinal de 9 Hz colhidas a 10 Hz são
**idênticas** às de um sinal de 1 Hz. Repare que o impostor tem a **fase invertida** (a
senoide verde começa descendo): é a "roda girando para trás". Depois de amostrado, não há
como distinguir o verdadeiro do impostor.*

Para visualizar *para onde* cada frequência vai, imagine o eixo de frequências dobrado como um
leque (ou como uma fita colada em ziguezague): a cada múltiplo de $f_s/2$ a fita reflete.

![Esquerda: frequência aparente em função da frequência real, uma onda triangular que sobe até fs/2, desce até fs e se repete. Direita: o exemplo de 600 Hz amostrado a 1 kHz refletindo em 500 Hz e aparecendo em 400 Hz](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/aliasing_dobra.png)

*Figura 7-J — Esquerda: a "dobra" (com $f_s$ = 10 Hz). Só até $f_s/2$ a frequência vista é a
real; acima disso, a curva volta a descer — 6 Hz aparece como 4 Hz, 9 Hz como 1 Hz, 11 Hz também
como 1 Hz, e assim indefinidamente. Direita: o Exemplo 7.3 no eixo de frequências — o
espelho fica em $f_s/2$, e o impostor surge à mesma distância dele, do lado de dentro.*

**Exemplo resolvido 7.3 (aliasing)** — Senoide de 600 Hz amostrada a $f_s = 1$ kHz. O que o
sistema registra?

*Solução.* A taxa exigida seria $f_s > 2 × 600 = 1{,}2$ kHz — violada (a frequência de Nyquist
é 500 Hz, e 600 Hz está acima). As amostras são **indistinguíveis** das de uma senoide de
|1000 − 600| = **400 Hz**. Pior: nenhum processamento posterior desfaz o disfarce — depois de
amostrado, o alias é matematicamente idêntico a um sinal legítimo de 400 Hz.

Uma tabelinha ajuda a "sentir" a dobra neste mesmo $f_s = 1$ kHz:

| $f_{sinal}$ | 100 Hz | 400 Hz | 500 Hz | 600 Hz | 900 Hz | 1000 Hz | 1100 Hz |
|---|---|---|---|---|---|---|---|
| $f_{alias}$ visto | 100 | 400 | *limite* | **400** | **100** | **0** (CC!) | **100** |

Note o caso de 1000 Hz: uma senoide com frequência igual a $f_s$ é amostrada sempre no mesmo
ponto do ciclo e vira um **valor constante** — um "nível CC" que não existe.

**Exemplo resolvido 7.3b (a luz da sala no seu LDR)** — No Lab 7 o LDR é lido a $f_s = 20$ Hz,
e lâmpadas de LED/fluorescentes eletrônicas piscam a 100 ou 120 Hz (o dobro da frequência da
rede). O que o ESP32 enxerga?

*Solução.* A frequência de Nyquist é 20/2 = 10 Hz; 120 Hz está *muito* acima. Como
120 = 6 × 20 e 100 = 5 × 20 são múltiplos exatos de $f_s$, o alias cai em
|120 − 6·20| = |100 − 5·20| = **0 Hz**: o cintilar rápido vira um **nível constante** somado à
leitura. Se a luz piscar um pouquinho fora do múltiplo (por exemplo, 120,3 Hz), o alias será
0,3 Hz: uma "respiração" lenta e fantasma na leitura — que ninguém gerou, e que nenhuma média
móvel de poucas amostras consegue remover (é lenta demais). É exatamente a linha da tabela de
problemas do roteiro: *"desvio alto com LDR parado — luz fluorescente/LED da sala"*.

### 3.4 A defesa: filtro anti-aliasing

Como o alias é indistinguível de um sinal verdadeiro, **a defesa precisa vir antes do ADC**,
no mundo analógico: um filtro **passa-baixas** que derruba tudo acima de $f_s/2$ *antes* que
a amostragem aconteça. O mais simples é um RC (um resistor em série e um capacitor para o
terra), com frequência de corte

$$ f_c = \frac{1}{2\pi R C} $$

![Em cima, diagrama de blocos sensor, filtro RC, ADC e código. Embaixo, resposta em frequência do RC com as atenuações em fs/2 para fs de 1 kHz e de 5 kHz](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/antialiasing_filtro_rc.png)

*Figura 7-K — Em cima: o filtro fica entre o sensor e o pino do ADC. Embaixo: um RC de
1ª ordem (R = 10 kΩ, C = 100 nF, $f_c$ ≈ 159 Hz) deixa passar quase intacto o sinal útil
(−1,4 dB em 100 Hz), mas atenua só −10,4 dB em 500 Hz e −23,9 dB em 2,5 kHz: o filtro não é
uma "parede", é uma **rampa** de −20 dB por década.*

O detalhe que muita gente esquece: o RC **não** é uma parede. Seu ganho cai suavemente
($-20$ dB por década) e, em $f_s/2$, ainda deixa passar uma fração relevante do que deveria
ter barrado. Por isso a regra prática tem *duas* partes:

1. um filtro analógico simples antes do pino; **e**
2. $f_s$ com folga (5–10× a banda de interesse), para empurrar $f_s/2$ para longe de $f_c$,
   onde o filtro já atenua bastante. Cada vez que você dobra $f_s$, ganha ~6 dB a mais de
   barreira no mesmo RC.

**Exemplo resolvido 7.3c (dimensionando o RC)** — Sensor com informação útil até 100 Hz,
$f_s = 1$ kHz, e uma interferência de 600 Hz no pino. Com R = 10 kΩ e C = 100 nF, quanto da
interferência chega ao ADC?

*Solução.* $f_c = 1/(2\pi \cdot 10^4 \cdot 10^{-7}) \approx 159$ Hz. O ganho de um RC de 1ª
ordem é $|H| = 1/\sqrt{1 + (f/f_c)^2}$:

- em 100 Hz (sinal útil): $|H| = 0{,}85$ → **−1,4 dB** (perda de ~15% na amplitude, aceitável);
- em 600 Hz (interferência): $|H| = 1/\sqrt{1 + 3{,}77^2} \approx 0{,}26$ → **−11,8 dB**.

Ou seja, a interferência ainda chega com **26%** da amplitude original e aparecerá como um
alias de |1000 − 600| = 400 Hz, só que menor. Para barrá-la de verdade: subir $f_s$ (com
$f_s$ = 5 kHz, 600 Hz fica *abaixo* de Nyquist e deixa de ser alias — vira um sinal legítimo,
que a média móvel ou outro filtro digital da seção 4 pode tratar) e/ou usar um filtro de
ordem maior.

> **Detalhe de bancada.** O pino do ESP32 tem um capacitor de amostragem interno que precisa
> recarregar a cada leitura; resistores muito grandes na entrada (dezenas de kΩ) deixam essa
> recarga lenta e causam erro de leitura. Ficar na faixa de alguns kΩ, com o capacitor de filtro
> ao lado do pino (que funciona como reservatório de carga), é uma boa partida.

### 3.5 A condição escondida: amostras igualmente espaçadas

O teorema tem uma condição tácita: as fotografias devem ser **igualmente espaçadas**. Se o
instante real de cada amostra erra por $\Delta t$ (**jitter**), o programa acredita ter lido o
sinal em $kT_s$, mas leu em $kT_s + \Delta t$: o erro de amplitude é aproximadamente a
inclinação do sinal vezes $\Delta t$, no máximo

$$ \varepsilon_{max} \approx 2\pi\,f\,A\,\Delta t $$

![Em cima, senoide de 25 Hz com as amostras ideais e as amostras com jitter de ±1,5 ms ligadas por segmentos. Embaixo, erro de cada amostra limitado por duas linhas tracejadas em ±0,24 A](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/amostragem_jitter.png)

*Figura 7-L — Jitter distorce o sinal mesmo com $f_s$ bem acima de Nyquist (200 Hz para um
sinal de 25 Hz). Cada amostra vermelha foi lida um pouco antes ou depois do instante
previsto (círculo vazio); o erro (embaixo) é maior onde o sinal é mais inclinado e fica
dentro do limite 2π·f·A·Δt.*

Quanto mais rápido o sinal e maior o desvio, pior: um sinal de 50 Hz amostrado com jitter de
2 ms (típico de uma tarefa em Linux disputando CPU) já erra até 2π·50·0,002 ≈ **63% da
amplitude**. Já uma tarefa FreeRTOS de prioridade adequada e disparada por `vTaskDelayUntil`
tem jitter bem menor que 1 ms (tipicamente dezenas de µs). É *por isso* que o firmware de hoje
amostra com `vTaskDelayUntil` (Exemplo 5.1), e é por isso que a malha PID da semana 13 rodará
no ESP32 e não no RPi (o Linux do Pi tem jitter de milissegundos; isso afeta a malha de
controle).

## 4. O primeiro filtro digital: média móvel

Sensores reais tremem: ruído térmico, 60 Hz da rede, spikes de EMI do motor da bancada ao
lado. O filtro mais simples e mais usado do firmware é a **média móvel**: a saída é a média
das últimas M amostras. Ela suaviza o ruído (atenua por ~√M o ruído branco, porque os erros
aleatórios se cancelam parcialmente na soma) ao custo de **atraso** — a saída "olha para o
passado" em (M−1)/2 amostras. Em controle (semana 13), atraso é veneno: ele desestabiliza a
malha. Todo filtro é um compromisso, não um presente.

Implementação ingênua: somar M valores a cada amostra — O(M). A implementação profissional
é **O(1)** com soma corrente (custo constante por amostra, qualquer que seja M), exatamente
como no firmware de hoje:

```c
soma += raw - buf[idx];      // entra o novo, sai o mais velho: UMA soma e UMA subtração
buf[idx] = raw;
idx = (idx + 1) % M;         // buffer circular
int filtrado = soma / M;
```

O **buffer circular** é o detalhe elegante: em vez de deslocar o array inteiro, o índice
anda em círculo (`% M`), e a posição mais antiga é simplesmente sobrescrita pela nova. Memória
constante, tempo constante.

**Exemplo resolvido 7.4 (média × mediana)** — Amostras do LDR: 512, 530, 498, **2900 (spike
de ruído)**, 505. Compare o efeito da média móvel M = 4.

*Solução passo a passo.* A partir da 4ª amostra: (512+530+498+2900)/4 = **1110**;
(530+498+2900+505)/4 = **1108**. O spike de 2900 foi atenuado ~4× (para ~+600 sobre a base
de ~510) mas **espalhado** por 4 saídas consecutivas — durante quatro leituras, o sistema
“acredita” em valores falsamente altos. Para ruído **impulsivo** (spikes raros e grandes), o
filtro certo é a **mediana** (ordena a janela e pega o valor do meio: o spike, sendo
extremo, é simplesmente descartado — a mediana de {498, 505, 512, 530, 2900} é 512 ✔); a
média brilha para ruído **contínuo** (gaussiano). Conhecer o ruído antes de escolher o
filtro é a primeira lição de PDS aplicado.

## 5. O caminho de volta: DAC

O **DAC** converte número→tensão: você escreve um código, o pino assume a tensão
correspondente (é o ADC de trás para frente — compare a figura abaixo com a 7-A).

![Função de transferência de um DAC de 3 bits](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/dac_3bits.png)

*Figura 7-M — DAC de 3 bits: a cada código, uma tensão degrau. Fonte: Hands-On Industrial
Internet of Things (Packt), cap. 3, Fig. 3.4.*

O ESP32 tem 2 canais de **8 bits** (GPIO25/26): 256 degraus de 3,3/256 ≈ 13 mV. Serve para
gerar formas de onda, áudio simples e tensões de referência. O firmware
`src/dac_senoide/main.c` gera uma senoide por **tabela pré-computada** — padrão clássico que
troca memória por CPU (32 bytes guardados valem uma chamada de `sin()` por amostra
economizada):

```c
for (int i = 0; i < NPTS; i++)                       // 32 pontos de um ciclo, calculados UMA vez
    s_tab[i] = (uint8_t)(127.5 + 127.5 * sin(2 * M_PI * i / NPTS));

static void tick_cb(void *arg)                       // esp_timer a NPTS·F = 3200 Hz
{
    dac_oneshot_output_voltage(s_dac, s_tab[s_i]);   // só consulta a tabela: barato
    s_i = (s_i + 1) % NPTS;
}
```

A aritmética da forma de onda: f_senoide = taxa de atualização ÷ pontos por ciclo =
3200/32 = **100 Hz**. Quer 400 Hz na mesma taxa? Sobram 8 pontos por ciclo — uma "senoide"
visivelmente escadeada, cheia de harmônicos (é a questão 5 da Lista 3; um RC na saída
suaviza — o *filtro de reconstrução*, o irmão gêmeo do anti-aliasing, agora na saída: um
impede fantasmas na entrada, o outro limpa degraus na saída).

---

## Resumindo

- ADC de N bits: $2^N$ níveis
- $LSB = \frac{V_{ref}}{2^N}$;
- erro de quantização $±\frac{LSB}{2}$ — irreversível.
- ESP32: 12 bits, ~0–3,1 V com atenuação 11 dB (Exemplo 7.1).
- Sensor resistivo → divisor de tensão; excursão máxima com R_fixo ≈ média geométrica dos
  extremos (Exemplo 7.2); sem R_fixo, resistência não vira tensão.
- Nyquist: f_s > 2·f_max, **com espaçamento uniforme**; violou ⇒ alias em |f_s − f| que
  nenhum filtro digital remove depois (Exemplos 7.3, 7.3b) — anti-aliasing é analógico e vem
  antes, e um RC sozinho é uma rampa (−20 dB/década), não uma parede (Exemplo 7.3c).
- Média móvel O(1) com soma corrente + buffer circular: atenua ruído contínuo, atrasa
  (M−1)/2 amostras; spikes pedem mediana (Exemplo 7.4). Filtro = compromisso com atraso.
- DAC 8 bits por tabela: f = taxa/pontos; poucos pontos por ciclo = escada + harmônicos;
  filtro RC de reconstrução na saída.

> 🔭 **Onde isto reaparece:** amostragem uniforme e análise em frequência (FFT) são exatamente o
> **pré-processamento** de um classificador de TinyML. Quando um grupo treina um modelo de movimento
> no Edge Impulse (bloco *Spectral Features*), é o teorema de Nyquist desta seção que decide a taxa
> de amostragem, e é a FFT que vira a "entrada" da rede. Ver a
> [trilha TinyML](../docs/trilha-tinyml.md), opcional, para o projeto final.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| ADC / DAC | conversor analógico→digital / digital→analógico |
| LSB | valor de um degrau do conversor (V_ref/2^N) |
| erro de quantização | perda dentro do degrau (±½ LSB) |
| SAR | ADC de aproximações sucessivas (busca binária em hardware) |
| saturação | leitura travada no fim de escala |
| divisor de tensão | dois resistores em série que fatiam a fonte |
| frequência de Nyquist | $f_s/2$: teto do que o sistema enxerga |
| aliasing | disfarce de frequência por subamostragem |
| anti-aliasing | filtro analógico antes do ADC |
| média móvel | média das últimas M amostras |
| buffer circular | array usado em anel (índice % M) |
| jitter de amostragem | irregularidade no espaçamento das amostras |

## 📖 Onde aprofundar (opcional)

- **Molloy**, *Exploring Raspberry Pi*, cap. 9 — aquisição de dados no RPi (gancho da
  semana 12).
- ***Hacking Electronics*** (Monk), cap. 3 — divisores, LDR, montagem com fotos.
- **ESP-IDF Guide**: *ADC Oneshot Mode Driver* e *DAC* — as APIs exatas do firmware de hoje.
- Revisão de **Análise de Sinais e Sistemas**: amostragem e espectro.

## Exercícios

Lista 3, questões 1–5 (estilo dos Exemplos 7.1–7.4; a 5 é a senoide escadeada do DAC).
