# Lab 7 — Lendo o mundo: LDR no ADC, filtro em ação e senoide no DAC

> **Antes de começar**: leia a [teoria-07](teoria-07.md) — o Exemplo 7.2 (divisor com LDR) é
> a conta que você calibrará na Parte A, e as Figuras 7-A/7-B mostram o que o ADC faz com a
> sua tensão.

**Objetivo**: montar o divisor com LDR e **calibrá-lo** contra as contas do Exemplo 7.2;
medir o ruído do ADC com e sem média móvel; provocar a saturação; gerar e "ouvir" a escada
do DAC.

**Duração**: 2 aulas.
**Material**: ESP32, LDR (temos 10), resistor 10 kΩ, protoboard, jumpers; lanterna do
celular; (opcional, Parte D) buzzer passivo ou fone velho + resistor 220 Ω.

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — Divisor com LDR: montagem e calibração (35 min)

1. **Wokwi primeiro**: monte o divisor com `wokwi-photoresistor-sensor` + resistor de 10 kΩ,
   saída no **GPIO34** (canal 6 do ADC1 — entrada *apenas*: os GPIOs 34–39 não têm saída),
   e cole `~/sis-emb/semana-07/src/adc_ldr/main.c`. O slider de lux do sensor no Wokwi
   permite "iluminar" com precisão — valide o comportamento antes do hardware.
2. Na protoboard: **3V3 → LDR → nó do GPIO34 → 10 kΩ → GND** (LDR em cima, como no Exemplo
   7.2 e no desenho da teoria, seção 2).

![O LDR: componente físico e símbolos esquemáticos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/ldr_componente.png)

*Figura L7-A — O componente que você tem na mão: a superfície serpentada é o material
fotossensível; a resistência cai com a luz. Fonte: Practical Python Programming for IoT
(Packt), cap. 9, Fig. 9.4.*

3. Grave e observe a linha impressa a cada amostra:

```
raw=2417  filt=2410  V=1.824  media_1s=2408.3  desvio=3.1
```

4. **Calibração contra a teoria** — preencha, cobrindo/iluminando o LDR:

| condição | V esperado (Ex. 7.2) | V medido | raw medido |
|---|---|---|---|
| luz da sala (≈ "claro") | ~1,8 V | | |
| lanterna encostada | > 2,5 V | | |
| coberto com a mão (≈ "escuro") | ~0,3–0,6 V | | |

   O seu LDR não é o do exemplo (8k/120k eram valores nominais); comente no relatório o
   quanto a física real se afastou — e por que isso **não** é um problema para um sistema
   de controle (dica: a semana 13 controla em malha fechada, que tolera sensor
   descalibrado — ela corrige o erro seja qual for a escala exata do sensor, desde que a
   resposta seja monotônica).

## Parte B — Medindo o ruído e o efeito do filtro (30 min)

O firmware imprime, a cada 1 s, média e **desvio-padrão** do sinal filtrado — nosso
"ruidômetro".

5. Com iluminação **constante** (LDR imóvel, sem sombras), anote o `desvio` por ~20 s para
   cada janela `M` (edite o `#define M`, regrave):

| M (janela) | desvio típico | atraso teórico (M−1)/2 amostras @ 20 Hz |
|---|---|---|
| 1 (sem filtro) | | 0 ms |
| 4 | | 75 ms |
| 8 | | 175 ms |
| 32 | | 775 ms |

6. O desvio caiu com M na razão ~√M prevista na teoria? E o preço: com M = 32, passe a mão
   rapidamente sobre o LDR — a resposta ficou visivelmente "lenta"? Você acabou de sentir o
   compromisso ruído × atraso que a malha PID da semana 13 terá de negociar.
7. **Spike sintético** (Exemplo 7.4 ao vivo): com M = 4, dê um "flash" de lanterna o mais
   curto que conseguir. O `filt` mostra o spike atenuado e espalhado por ~4 amostras? Cole
   um trecho do log no relatório.

## Parte C — Saturação e limites da faixa (15 min)

8. Tire o resistor de 10 kΩ e ligue o GPIO34 **direto no 3V3** por um instante. Leitura
   esperada: `raw=4095` cravado — **saturação** (teoria, seção 1.2). Tensões entre 3,1 e
   3,3 V "desaparecem" no teto. Anote e devolva o circuito ao normal.
9. Pergunta de projeto para o relatório: se o seu sensor entregasse 0–5 V, o que fazer
   antes do ADC? (Duas respostas válidas: divisor de tensão — a semana 3 — ou ADC externo
   de faixa maior; você reverá ambas no HC-SR04 da semana 12.)

## Parte D — DAC: a senoide e sua escada (30 min)

10. Grave `~/sis-emb/semana-07/src/dac_senoide/main.c`. O GPIO25 agora emite uma senoide de
    100 Hz com 32 pontos/ciclo (teoria, seção 5).
11. **Vendo**: se a bancada tiver osciloscópio, observe a escada de 32 degraus; sem
    osciloscópio, o multímetro em VAC lê o valor eficaz (~1,1–1,2 V para senoide de
    amplitude 1,65 V — senoide: V_rms = A/√2 = 1,65/1,41 ≈ 1,17 V; anote).
12. **Ouvindo** (opcional): buzzer passivo (ou fone) em série com 220 Ω entre GPIO25 e GND.
    100 Hz é um grave; mude `F_HZ` para 440 (o lá musical) e ouça. Agora reduza `NPTS` para
    8 mantendo 440 Hz: o timbre fica áspero (harmônicos da escada — questão 5 da Lista 3
    respondida pelo ouvido).
13. Extra de 5 min: ligue o GPIO25 (DAC) **no GPIO34 (ADC)** com um jumper e rode o
    firmware do ADC noutra placa... não temos duas? Então apenas descreva no relatório o
    experimento de *loopback* que faria e o que esperaria ver com f_s = 20 Hz amostrando
    100 Hz (dica: Exemplo 7.3 — o alias apareceria em |20 − 100·k|... para f_s=20 Hz e
    f=100 Hz: 100 = 5×20, exatamente no limite — as amostras cairiam sempre no mesmo ponto
    do ciclo: uma reta! E com 90 Hz? |100−90| = 10 Hz de alias).

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| `raw` sempre 0 ou 4095 | fio no GPIO errado / divisor aberto | confira o nó do divisor no GPIO34 |
| Leitura não muda com luz | LDR e resistor trocados de lugar | LDR em cima (3V3), 10k embaixo |
| Desvio alto com LDR parado | luz fluorescente/LED da sala (100/120 Hz!) | cubra o sensor; discuta no relatório |
| DAC sem saída | GPIO errado (só 25/26 têm DAC) | confira o pino |

## Entrega (GitHub da dupla, `lab-07/relatorio.md`)

1. Tabela de calibração (Parte A) + comentário sobre o desvio da teoria.
2. Tabela de ruído × M (Parte B) + o trecho de log do spike + 3 linhas sobre o compromisso
   ruído/atraso.
3. Registro da saturação (C.8) e resposta de projeto (C.9).
4. Valor eficaz medido (ou descrição do áudio) na Parte D + resposta do loopback (D.13).

## Desafio (opcional)

Luxímetro tosco porém honesto: usando dois pontos de calibração (lanterna encostada =
"100 %", coberto = "0 %"), converta a leitura filtrada em percentual de luminosidade e
imprima uma barra ASCII (`[#########.......] 62 %`) atualizada a 5 Hz. Capriche: a barra
não pode "tremer" com o LDR parado (escolha M com critério e justifique).
