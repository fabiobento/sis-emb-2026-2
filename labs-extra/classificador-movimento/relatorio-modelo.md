# Modelo de relatório — Classificador de movimento

Copie este arquivo para `lab-classificador/relatorio.md` no repositório da bancada e preencha. Todo
número deve vir **de uma execução sua**, com a fonte indicada (print, arquivo, comando).

## 0. Identificação

- Bancada / integrantes / data:
- **Edge Impulse — project ID:**
- Classes (rótulos exatos):
- `fs` = ___ Hz; janela = ___ ms (___ amostras); *window increase* = ___ ms; `STRIDE_MS` = ___; `N_CONFIRMA` = ___; `LIMIAR_CONF` = ___
- Sensor: `MPU_RANGE` = ___, `MPU_DLPF` = ___, orientação física do sensor (descreva ou foto):

## 1. Taxa de amostragem (Parte A)

Justifique 100 Hz (ou o valor que usou) por Nyquist, a escolha de `vTaskDelayUntil` e o uso do DLPF.
Anexe o print do monitor com `WHO_AM_I` e a taxa medida pelo `gravar_csv.py`.

## 2. Dados (Parte B)

| Classe | Pessoa | Nº de gravações | Minutos | Treino/Teste |
|---|---|---|---|---|
| | | | | |

Descreva o protocolo de gravação (como segurou o sensor, variações introduzidas) e **como separou
o teste** (por pessoa? por gravação?).

## 3. Impulse e Feature explorer (Parte C)

Print do *Feature explorer*. Quais classes ficam próximas? Parâmetros do *Spectral Features*
alterados (se algum) e por quê.

## 4. Treino e avaliação (Parte D)

| Medida | Valor |
|---|---|
| Acurácia treino/validação | |
| **Acurácia no *Model testing*** | |
| *Recall* por classe | |
| Par mais confundido | |
| RAM / flash / latência **estimadas** (Studio) | |

Cole a matriz de confusão e **interprete** (Exemplos C.5 e C.6). Log de iterações (≥ 2) com o que
mudou e o efeito. Compare com o baseline local (`pipeline_host.py dados`), se rodou.

## 5. Integração (Parte E)

Print Studio × ESP32 (mesma janela, mesma classe). Tempos **medidos** (`DSP`, `Classification`) ×
estimados.

## 6. Sistema ao vivo (Parte F e G)

- Print do monitor com linhas `[janela]` e do `mosquitto_sub`.
- **Matriz de confusão de campo** (10 tentativas por gesto, por quem não gravou o treino):

| gesto real \ publicado | parado | aceno | sobe_desce | circulo | nenhum |
|---|---|---|---|---|---|

- Acurácia de campo = ___ %. Compare com o *Model testing* e explique a diferença.
- Latência de ponta a ponta medida (cronômetro) × estimativa (teoria §8):
- Link/arquivo do vídeo da demonstração.

## 7. Limitações e próximos passos

O que o sistema **não** faz bem? Em que condições falha? O que faria com mais uma semana?

---

## Rubrica (100 pontos)

| Critério | Pontos | O que se espera |
|---|---|---|
| Coleta e dados | 15 | ≥ 3 pessoas, ≥ 3 min/classe, protocolo descrito, taxa medida, teste **separado por pessoa/gravação** |
| Justificativas de PDS | 10 | taxa (Nyquist), `vTaskDelayUntil`, DLPF, janela (Exemplo C.2) |
| Impulse e explorer | 10 | parâmetros corretos e coerentes com o firmware; explorer lido com critério |
| Avaliação honesta | 20 | treino × teste, matriz interpretada, *recall*/precisão, log de iterações, sem vazamento |
| Integração no ESP32 | 15 | teste de sanidade com print; `static_assert` satisfeito; tempos medidos × estimados |
| Sistema ao vivo | 15 | buffer circular, *debounce*, MQTT no tópico padrão, LWT, saída estável |
| Teste de campo e demonstração | 10 | matriz de campo, latência medida, vídeo |
| Reprodutibilidade | 5 | project ID, classes, `fs`, janela, acurácia de **teste**, `projeto_config.h` |

**Penalidades:** apresentar acurácia de treino como resultado final (−10); dados sintéticos
apresentados como reais (−20, e é questão de integridade); relatório sem prints de execução (nota
de itens que dependem do print = 0).
