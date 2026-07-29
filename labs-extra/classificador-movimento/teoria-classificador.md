# Teoria de apoio — Classificador de movimento na borda (TinyML)

Apoio conceitual do lab-extra opcional que treina um classificador de movimento no **Edge Impulse**
e o roda no ESP32 com o **MPU-6050** da semana 9. Formato tutorial, com exemplos resolvidos
C.1–C.3. Nada aqui é novo "do zero": é a montagem, numa ordem nova, do que você já viu em
amostragem (semana 7), I2C/MPU-6050 (semana 9) e tarefas/MQTT (semanas 5, 6 e 14).

## 1. O que muda quando a decisão vem de um modelo

Até aqui, toda decisão do seu firmware foi um `if` escrito à mão: *se a inclinação passa de X, faça
Y*. Isso funciona quando você **conhece a regra**. Mas "isto é um aceno?" ou "este motor está
vibrando de forma anormal?" são perguntas cuja regra é difícil de escrever — dependem do formato de
um sinal ao longo do tempo, não de um limiar único.

Um classificador de **aprendizado de máquina** inverte o problema: em vez de você escrever a regra,
você mostra **exemplos rotulados** ("isto é um aceno", "isto é parado") e um algoritmo encontra a
regra sozinho. O resultado é uma função — pequena o bastante para caber no ESP32 — que recebe uma
janela do sinal e devolve a probabilidade de cada classe.

**TinyML** é justamente isso: rodar essa função de inferência num microcontrolador, sem nuvem. As
vantagens são as mesmas que motivam sistemas embarcados desde a semana 1 — latência de
milissegundos, funciona offline, gasta pouca energia e não expõe dados brutos na rede.

## 2. O pipeline: por que ele é 90% processamento de sinal

Um classificador de movimento tem três blocos. Repare que os dois primeiros são a semana 7:

```
 acelerômetro ──▶ [janela] ──▶ [DSP: FFT] ──▶ [rede neural pequena] ──▶ classe + confiança
   (semana 9)     (buffer)     (semana 7)        (o "ML" propriamente)
```

- **Janela**: você não classifica uma amostra isolada, e sim um trecho — tipicamente 1–2 s. A cada
  passo, uma janela de N amostras dos 3 eixos é o "quadro" a ser reconhecido.
- **DSP (Digital Signal Processing)**: a janela crua tem centenas de números por eixo — demais para
  a rede. O bloco de DSP a resume em poucas **características**. Para movimento, o bloco padrão do
  Edge Impulse (*Spectral Features*) aplica a **FFT** e mede a energia por faixa de frequência. É a
  análise em frequência da semana 7: um aceno lento e um tremor rápido diferem no **espectro**,
  mesmo com amplitude parecida.
- **Rede neural**: recebe as características (não o sinal cru) e produz a probabilidade de cada
  classe. Como o DSP já fez o trabalho pesado, a rede pode ser minúscula — poucos kB — e ainda
  assim precisa.

A lição central: **num bom classificador embarcado, o DSP carrega o piano.** Se as
características já separam as classes, quase qualquer rede pequena acerta. Se não separam, nenhuma
rede grande salva. Por isso este lab é, no fundo, um lab de processamento de sinais.

## 3. Amostragem: Nyquist decide a taxa (revisão da semana 7)

Antes de coletar um único dado, você escolhe a **taxa de amostragem**. A regra é a mesma de sempre:
`f_s > 2 · f_max`, onde `f_max` é a maior frequência **útil** do movimento.

**Exemplo resolvido C.1 (escolha da taxa).** Movimentos de mão/braço (acenar, girar, sacudir) têm
energia relevante até cerca de 10–15 Hz. Qual `f_s` usar?
*Solução.* Nyquist exige `f_s > 30 Hz`. Na prática, usamos folga larga: **100 Hz** captura tudo com
sobra e ainda dá uma janela de 2 s com 200 amostras — bastante para a FFT resolver as faixas de
frequência. Taxas muito acima (ex.: 1 kHz) só incham o dado sem ganho, pois não há sinal útil lá em
cima. ∎

E — como na semana 5 — a amostragem tem de ser **uniforme**. Jitter no período embaralha o
espectro (a FFT assume espaçamento constante), então o coletor usa `vTaskDelayUntil`, não
`vTaskDelay`. Período torto = espectro sujo = classificador pior. O mesmo raciocínio do
Exemplo 5.1, agora com consequência direta na acurácia.

## 4. Overfitting: o erro nº 1 (e como a métrica o denuncia)

O perigo de todo classificador é **decorar** os exemplos de treino em vez de **aprender** o padrão.
Um modelo que decorou acerta 100% no treino e erra feio no mundo real.

A defesa é honestidade experimental: separe os dados em **treino** e **teste**, treine só com os de
treino e meça no de teste — dados que o modelo nunca viu. A diferença entre as duas acurácias é a
medida de generalização.

**Exemplo resolvido C.2 (ler a diferença).** Um grupo obteve 99% de acurácia no treino e 71% no
teste. O que isso significa?
*Solução.* Uma diferença grande (99 → 71) é a assinatura clássica de **overfitting**: o modelo
decorou. Causas comuns: poucos exemplos, todos gravados do mesmo jeito (mesma pessoa, mesma posição
do sensor). Remédios: coletar mais e mais variado, reduzir o tamanho da rede, aumentar o passo da
janela. O número que importa para o projeto é o **de teste** (71%), nunca o de treino. ∎

**Exemplo resolvido C.3 (ler a matriz de confusão).** A matriz mostra que `parado` e `aceno` quase
nunca se confundem, mas `sobe-desce` é classificado como `círculo` em 30% dos casos. O que fazer?
*Solução.* O modelo separa bem o que é espectralmente distinto (parado × aceno) e erra onde os
movimentos se parecem (sobe-desce × círculo têm frequências próximas). Caminhos: coletar mais
exemplos desses dois, torná-los mais distintos na hora de gravar, ou aceitar que essas duas classes
talvez devam virar uma só. A matriz de confusão diz **onde** o modelo erra — muito mais útil que a
acurácia global. ∎

## Resumindo

- TinyML = rodar a inferência de um classificador no próprio MCU: baixa latência, offline, privado.
- O pipeline é janela → DSP (FFT, *Spectral Features*) → rede pequena. O DSP é a semana 7; ele faz
  o trabalho pesado, e por isso a rede cabe no ESP32.
- A taxa de amostragem sai de Nyquist e precisa ser uniforme (`vTaskDelayUntil`), senão o espectro —
  e a acurácia — degrada.
- Overfitting é o erro central; treino × teste e a matriz de confusão são as ferramentas para
  detectá-lo. O número que vale é o de **teste**.

## 📖 Onde aprofundar (opcional)

- Edge Impulse — [Motion feature extraction](https://docs.edgeimpulse.com/knowledge/concepts/data-engineering/motion-feature-extraction.md)
  (o bloco Spectral Features em detalhe).
- Edge Impulse — [Getting started for embedded engineers](https://docs.edgeimpulse.com/knowledge/guides/getting-started-for-embedded-engineers.md).
- Edge Impulse — [Increasing model performance](https://docs.edgeimpulse.com/knowledge/guides/increasing-model-performance.md).
- A [trilha TinyML](../../docs/trilha-tinyml.md) do repositório, para levar isto ao projeto final.
