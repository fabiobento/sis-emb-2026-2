# Lab 13 — Malha fechada de luminosidade: identificar, controlar, sintonizar, analisar

> **Antes de começar**: leia a [teoria-13](teoria-13.md) — a Figura 13-A é o diagrama do que
> você vai montar, o Exemplo 13.3 (PID na mão) deve estar **feito no papel** antes da aula, e
> o Exemplo 13.4 é o roteiro de sintonia que você seguirá na Parte C.

**Objetivo**: percorrer o fluxo completo de uma malha de controle embarcada — identificação em
malha aberta, fechamento com PID, sintonia empírica documentada, sabotagem do anti-windup e
análise quantitativa em Jupyter.

**Duração**: 2 aulas.
**Material**: ESP32, LDR + R 10 kΩ (divisor no GPIO34, como no Lab 7), LED de alto brilho +
R 100 Ω (GPIO2), copo/caixa opaca (o "ambiente"), lanterna do celular (a perturbação).
**Wokwi**: circuito completo disponível — o slider de lux do fotorresistor faz o papel da
lanterna; ótimo para ensaiar a sintonia em casa.

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb && git fetch && git reset --hard origin/main
```

## Parte A — Montagem do "ambiente" (15 min)

1. LED (via R 100 Ω no GPIO2) e LDR (divisor do Lab 7 no GPIO34) **frente a frente, a ~3 cm**,
   dentro do copo opaco emborcado. O copo isola a malha da iluminação da sala — sem ele, o
   professor apagar a luz vira perturbação não solicitada (…o que, aliás, usaremos a favor na
   Parte D).

> 💡 **A anatomia da planta**: cada peça da montagem é um bloco da Figura 13-A da teoria. O
> LED + driver LEDC = atuador; LED→luz→LDR dentro do copo = processo; LDR + divisor + ADC +
> média móvel = sensor; seu firmware = somador + controlador. Se algum dia faltar um bloco
> no seu raciocínio de depuração, volte ao diagrama e pergunte: qual bloco está mentindo?

![Protoboard com divisor LDR → pino de ADC, montagem de referência](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/luxmetro_esquema.png)

*Figura L13-A — Referência visual do front-end de medição: o divisor LDR + resistor fixo
com o nó central indo ao pino do ADC (a foto é de uma montagem equivalente num Raspberry
Pi Pico — o princípio é idêntico ao do nosso ESP32: LDR para o 3,3 V, resistor de 10 kΩ
para o GND, nó do meio no GPIO34). Fonte: Raspberry Pi 4 OR 5 AND Pico — Cool Projects
for Test, Measurement, and Control (Elektor).*

2. Grave `~/sis-emb/semana-13/src/pid_luz/main.c` como está e abra o monitor: o firmware
   imprime CSV contínuo:

```
t_ms,ref,y,u
20,2000,412.0,1270.4
40,2000,509.2,2405.7
```

## Parte B — Identificação em malha aberta (25 min)

Antes de controlar, meça a planta (teoria, seção 5 — "engenharia na ordem certa").

3. Mude `#define MODO_PID 0` e regrave: o firmware agora aplica degraus de duty pré-programados
   (800 → 2400 → 800 → 3600) e só registra a resposta do LDR.
4. Capture ~30 s de monitor para um arquivo no PC (dica: `idf.py monitor | tee aberta.csv`, e
   limpe as linhas não-CSV depois — ou copie/cole do terminal).
5. No gráfico (Parte E) ou a olho nos números: após um degrau de duty, quantos ms o `y` leva
   para percorrer ~63 % da variação total? Essa é a **constante de tempo** da planta
   (LED + LDR + copo). Deu na casa dos ~100 ms que o Exemplo 13.1 assumiu? Registre o valor —
   ele é a justificativa *medida* dos seus T_s = 20 ms.

> 🧠 **Por que 63 %?** Para um sistema de 1ª ordem, a resposta ao degrau é
> y(t) = Δ·(1 − e^(−t/τ)) — em t = τ, a variação percorrida é 1 − e^(−1) ≈ 0,63. Achar o
> instante dos 63 % **é** achar τ, sem ajuste de curva nenhum. (E se a resposta tiver forma
> de "S" suave em vez de exponencial, há mais dinâmica escondida — comente se for o seu caso.)

## Parte C — Fechando a malha e sintonizando (40 min)

6. `MODO_PID 1` de volta. A sintonia inicial (K_p = 0,8; K_i = 2,0; K_d = 0,01) deve segurar
   `y` em torno de `REF 2000` — mas não otimamente. Sua missão é seguir o **Exemplo 13.4** e
   documentar cada passo:

| tentativa | Kp | Ki | Kd | comportamento observado (1 linha) |
|---|---|---|---|---|
| 1 (inicial) | 0,8 | 2,0 | 0,01 | |
| 2 (só P: Ki=Kd=0, suba Kp) | | 0 | 0 | |
| 3 (P no limite da oscilação) | | 0 | 0 | |
| 4 (+I até zerar regime) | | | 0 | |
| 5 (+D para amortecer) | | | | |

7. Para **cada** tentativa: regrave, aplique um degrau de referência (mude `REF` de 1500→2500,
   ou implemente a alternância automática a cada 5 s — 4 linhas de código, vale o esforço) e
   capture ~15 s de CSV. Nomeie os arquivos `sint1.csv`…`sint5.csv`.
8. Sinais clínicos para a coluna de observações (teoria, seção 3 — e a Figura 13-B é a sua
   radiografia de referência): P alto demais = LED "respirando" (oscilação sustentada); sem I
   = `y` estaciona *perto* mas nunca *em* 2000 (erro de regime — e note o papel da zona morta
   do LED, que você mediu na semana 8, C.11); K_d alto = `u` chiando com o ruído do LDR.

## Parte D — Perturbação e a sabotagem do anti-windup (20 min)

9. Com a melhor sintonia: **lanterna do celular** encostada no copo. Observe `u` despencar (o
   PID cede espaço à luz intrusa) e `y` voltar ao alvo em < 1 s. Retire a lanterna: `u` sobe de
   volta. Capture o episódio em CSV (`perturbacao.csv`) — é o gráfico mais bonito do relatório:
   a malha **rejeitando um distúrbio** sozinha, que é a razão de existir da realimentação.
10. **Windup ao vivo**: mude `ANTI_WINDUP 0`, regrave, e **abra o copo** com `REF 3500`
    (referência inalcançável com a luz da sala: o LED satura e a integral dispara — o
    "acelerador atolado na lama" da teoria, seção 3.1). Depois de ~10 s, **feche o copo** e
    cronometre quanto tempo a malha leva para se recompor (sobressinal longo). Repita com
    `ANTI_WINDUP 1`: recuperação quase imediata. Registre os dois tempos — é a questão 5 da
    Lista 5 respondida com cronômetro.

## Parte E — Análise no Jupyter (20 min)

11. No PC:

```bash
pip install jupyter pandas matplotlib
jupyter notebook ~/sis-emb/semana-13/src/analise_pid.ipynb
```

12. Aponte o notebook para seus CSVs e execute: ele plota `ref/y/u` no tempo e calcula
    **sobressinal (%)** e **tempo de acomodação (±5 %)** de cada degrau. Monte a tabela final
    comparando as sintonias 3, 4 e 5 — os números devem contar a mesma história das suas
    observações a olho.

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| `y` não responde ao LED | LDR/LED fora do copo ou trocados | confira a montagem frente a frente |
| Malha "corre pro lado errado" (sobe o alvo, LED apaga) | lógica do divisor invertida | teoria, seção do Ex. 7.2: qual a convenção do seu divisor? |
| `u` sempre no teto | referência inalcançável / saturação | confira REF com a faixa do sensor (Parte A.4 do Lab 7) |
| Oscilação que não para | K_p alto demais (ou K_d zero) | volte ao passo 2 da tabela |

## Entrega (GitHub da dupla, `lab-13/relatorio.md`)

1. Constante de tempo identificada em malha aberta (Parte B) + o trecho de dados que a sustenta.
2. Tabela de sintonia completa (Parte C) + os 5 CSVs no repositório.
3. Gráfico da perturbação com a lanterna (Parte D.9) exportado do notebook.
4. Os dois tempos de recuperação do experimento de windup (D.10) + 3 linhas explicando o
   mecanismo com o vocabulário da teoria (integral acumulada, saturação, clamping).
5. Notebook executado (`analise_pid.ipynb` com saídas salvas) + tabela sobressinal × t_acomodação
   das 3 melhores sintonias.

## Desafio (opcional)

Escalone a referência: faça `REF` seguir uma onda quadrada 1500↔2500 a 0,2 Hz e sobreponha, no
notebook, as respostas das sintonias 3/4/5 no mesmo gráfico. Bônus conceitual: troque a derivada
do erro pela derivada da medida (teoria, seção 3.1, cuidado 2) e mostre nos dados o fim do
"chute" no instante do degrau.
