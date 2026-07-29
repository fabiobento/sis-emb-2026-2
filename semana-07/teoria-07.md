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

Um ADC de **N bits** divide sua faixa de entrada (0 a V_ref) em **2^N níveis** e responde à
pergunta "em qual degrau está a tensão?". O tamanho do degrau é o **LSB** (*least significant
bit* — o valor de um passo):

LSB = V_ref / 2^N

Tudo que cair dentro do mesmo degrau vira o **mesmo número** — essa perda irreversível é o
**erro de quantização**, no máximo ±LSB/2. Mais bits ⇒ degraus menores ⇒ mais fidelidade (e
mais custo, e mais sensibilidade a ruído: degrau menor que o ruído do circuito é bit
desperdiçado).

As figuras abaixo mostram a matemática ganhando corpo: a primeira é a função de
transferência de um ADC ideal de 3 bits (8 degraus — repare que toda uma faixa de tensões
vira o mesmo código); a segunda é o erro de quantização correspondente, o “dente de serra”
confinado entre ±½ LSB.

![Função de transferência de um ADC ideal de 3 bits](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/adc_3bits.png)

*Figura 7-A — ADC ideal de 3 bits: a tensão contínua entra, o código sai em degraus. Fonte:
Hands-On Industrial Internet of Things (Packt), cap. 3, Fig. 3.5.*

![Erro de quantização confinado entre mais e menos meio LSB](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/erro_quantizacao.png)

*Figura 7-B — O erro de quantização: um “dente de serra” entre −½ LSB e +½ LSB. É
irrecuperável — nenhum filtro posterior devolve a informação perdida dentro do degrau.
Fonte: Hands-On Industrial Internet of Things (Packt), cap. 3, Fig. 3.6.*

E o efeito de aumentar N, lado a lado:

![Comparação das funções de transferência de ADCs de 4, 8 e 12 bits](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/adc_transferencia.png)

*Figura 7-C — Esquerda: mais bits, escada mais fina (4, 8 e 12 bits sobre a mesma faixa).
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

### 1.2 A atenuação do ESP32

O ADC interno mede nativamente ~0–1,1 V; um atenuador programável estica a faixa (0 dB,
2,5 dB, 6 dB, 11 dB). Usamos sempre `ADC_ATTEN_DB_11` (faixa ~0–3,1 V) — compatível com
divisores alimentados em 3,3 V. Tensão acima da faixa não quebra o pino (até 3,3 V), mas
**satura** a leitura em 4095: o conversor “grita” o valor máximo e você não sabe se está em
3,2 V ou em 5 V — saturação também é perda de informação, e diagnóstico comum de “sensor que
não muda”.

## 2. Condicionamento: o divisor de tensão

O ADC lê **tensão**; muitos sensores baratos variam **resistência** (LDR: luz; NTC:
temperatura; sensor de solo resistivo: umidade). A ponte entre os dois mundos é o circuito
mais importante da disciplina — o **divisor de tensão**:

```
   3V3 ──[ R_sensor ]──┬──[ R_fixo ]── GND
                       │
                      ADC (GPIO34)          V_out = 3,3 · R_fixo / (R_sensor + R_fixo)
```

A fórmula sai da lei de Ohm em duas linhas: a mesma corrente atravessa os dois resistores,
I = 3,3 / (R_sensor + R_fixo); e V_out é a queda sobre R_fixo, I × R_fixo. Substitua e pronto.

A foto mostra o componente real: um LDR (*light-dependent resistor* — resistor dependente de
luz) e seus símbolos esquemáticos. A resistência cai quando a luz bate no material
semicondutor da superfície serpentada — fotóns liberam portadores de carga.

![Fotografia de um LDR e seus símbolos esquemáticos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/ldr_componente.png)

*Figura 7-D — O LDR: componente físico e símbolos. Fonte: Practical Python Programming for
IoT (Packt), cap. 9, Fig. 9.4.*

**Exemplo resolvido 7.2 (LDR + divisor)** — LDR varia de 8 kΩ (claro) a 120 kΩ (escuro), em
série com R_fixo = 10 kΩ (LDR em cima, como no desenho). Calcule a excursão de tensão.

*Solução passo a passo.*

- Claro: V = 3,3 × 10/(8+10) = **1,83 V**
- Escuro: V = 3,3 × 10/(120+10) = **0,25 V**
- Excursão: 1,83 − 0,25 ≈ **1,58 V** — mais da metade da faixa do ADC, ótimo aproveitamento.

E o valor de R_fixo? A regra do **máximo contraste** é R_fixo ≈ √(R_claro × R_escuro) =
√(8k × 120k) ≈ 31 kΩ; nosso 10 kΩ (o que há aos montes no inventário) entrega um pouco menos
de excursão e mais corrente — compromisso consciente, típico de projeto real (“usa o que
tem” é uma restrição legítima). Repare na lógica: mais luz ⇒ menos resistência ⇒ **mais**
tensão. Se quiser a lógica invertida (mais luz, menos tensão), troque sensor e resistor de
lugar — e a semana 13 precisará saber qual é a sua convenção antes de fechar a malha!

> 💡 **Pense aí**: por que não ligar o LDR direto entre 3,3 V e o ADC, sem resistor? *Porque
> resistor não “gera” tensão sozinho: a variação de resistência só vira variação de tensão
> quando uma corrente a atravessa — e é o R_fixo quem fecha o caminho da corrente e cria o
> ponto de comparação. Sem ele, o pino veria 3,3 V sempre (até a corrente de fuga mudar
> isso de forma imprevisível).*

## 3. Amostragem: o teorema que não perdoa

O ADC não enxerga o sinal contínuo: tira **fotografias** a cada T_s segundos (taxa
f_s = 1/T_s). O **teorema de Nyquist–Shannon** diz o preço da discrição:

> Para representar fielmente um sinal com componentes até f_max, é preciso amostrar a
> **f_s > 2·f_max**.

Violou? O sinal rápido demais não desaparece — ele se **disfarça** de um sinal lento,
fenômeno chamado **aliasing** (o efeito "roda de carroça girando para trás" do cinema: a
câmera amostra a roda 24× por segundo; se ela gira quase nessa taxa, o filme a mostra
girando devagar... para trás). A frequência do impostor: f_alias = |f_s − f_sinal| (para
f_sinal entre f_s/2 e f_s).

![Sinal de 9 Hz amostrado a 10 Hz produzindo um alias de 1 Hz](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/aliasing_nyquist.png)

*Figura 7-E — Aliasing: as amostras (bolas vermelhas) do sinal de 9 Hz colhidas a 10 Hz são
**idênticas** às de um sinal de 1 Hz. Depois de amostrado, não há como distinguir o
verdadeiro do impostor.*

**Exemplo resolvido 7.3 (aliasing)** — Senoide de 600 Hz amostrada a f_s = 1 kHz. O que o
sistema registra?

*Solução.* A taxa exigida seria f_s > 2 × 600 = 1,2 kHz — violada. As amostras são
**indistinguíveis** das de uma senoide de |1000 − 600| = **400 Hz**. Pior: nenhum
processamento posterior desfaz o disfarce — depois de amostrado, o alias é matematicamente
idêntico a um sinal legítimo de 400 Hz. Por isso a defesa é **antes** do ADC: filtro
*anti-aliasing* analógico (um RC passa-baixas já ajuda) e/ou f_s com folga (regra prática:
5–10× a banda de interesse, não o mínimo teórico — o teorema assume filtros ideais que não
existem).

E uma condição escondida no teorema: as fotografias devem ser **igualmente espaçadas**.
Período irregular (jitter) distorce o espectro tanto quanto f_s baixa — é *por isso* que o
firmware de hoje amostra com `vTaskDelayUntil` (Exemplo 5.1) e é por isso que a malha PID da
semana 13 rodará no ESP32 e não no RPi (o Linux do Pi tem jitter de milissegundos; a malha
de controle não perdoa).

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

*Figura 7-F — DAC de 3 bits: a cada código, uma tensão degrau. Fonte: Hands-On Industrial
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

- ADC de N bits: 2^N níveis; LSB = V_ref/2^N; erro de quantização ±LSB/2 — irreversível.
  ESP32: 12 bits, ~0–3,1 V com atenuação 11 dB (Exemplo 7.1).
- Sensor resistivo → divisor de tensão; excursão máxima com R_fixo ≈ média geométrica dos
  extremos (Exemplo 7.2); sem R_fixo, resistência não vira tensão.
- Nyquist: f_s > 2·f_max, **com espaçamento uniforme**; violou ⇒ alias em |f_s − f| que
  nenhum filtro digital remove depois (Exemplo 7.3) — anti-aliasing é analógico e vem antes.
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
