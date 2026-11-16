# Gabarito do professor — projeto guiado de movimentos contínuos

> ⚠️ **Professor:** esta pasta contém as soluções dos TODOs e as respostas de referência. Se
> preferir que os alunos não a vejam, mova-a para o repositório privado (`sis-emb-2026-2-privado`)
> antes do encontro 3.

## Scripts

| Arquivo | TODOs resolvidos |
|---|---|
| `02_extrai_features.py` | `split(",")` e as três conversões `float(...)` |
| `03_janela_deslizante.py` | `process_line`, `data_window.extend(features)`, `len(data_window) >= WINDOW_SIZE * EIXOS`, `data_window[STRIDE * EIXOS:]` |
| `04_classifica.py` | `runner.classify(window_data)`, `max(scores, key=scores.get)`, `mp["input_features_count"] // eixos`, `extend`, deslizamento |

Teste automático (sem hardware e sem conta no Edge Impulse):

```bash
cd projeto-guiado-movimento
python3 -m venv /tmp/v && . /tmp/v/bin/activate && pip install pyserial numpy six edge_impulse_linux
python3 tests/testa_scripts.py        # simulador + .eim falso; esperado: TODOS OS TESTES PASSARAM
```

## Respostas de referência

**Etapa 1, Q1.** Uma amostra é um instante: pode ser idêntica em classes diferentes (o instante
de parada no topo de um `lift` é igual a `idle`). Movimento é variação no tempo; o modelo precisa
de um trecho. `maritime` tem energia em ~0,3–0,6 Hz (período de 2–3 s): uma janela de 0,2 s não
contém nem um décimo de ciclo, e a resolução da FFT (`Δf ≈ fs/N`) seria de ~5 Hz — impossível
separar 0,5 Hz de 0 Hz.

**Etapa 1, Q3.** `idle` acerta primeiro porque suas *features* (RMS≈0, espectro plano) ficam
isoladas no espaço de *features*; as demais exigem pesos mais ajustados. Confusão típica:
`maritime` × `terrestrial` (ambas com energia horizontal) — gravar `maritime` mais lento e com
inclinação nos dois sentidos.

**Etapa 2, Q1.** O *callback* roda na tarefa do `esp_timer`, compartilhada e de alta prioridade:
bloqueá-la com I²C (~0,3 ms + *timeouts*) e `printf` (~2,2 ms de UART) atrasa todos os outros
*timers* e gera *jitter*. Regra das ISRs: sinalizar e sair; o trabalho lento vai para uma tarefa.

**Etapa 2, Q2.** Taxa ≈ 62,5 linhas/s. (1) Z ≈ −9,8. (2) gravidade em X ou Y. (3) ≈ 100 linhas/s.
(4) patamares em ≈ ±19,6 m/s² (±2 g).

**Etapa 2, Q4.** A classe muda depois de ~1–2 s: a janela de 2 s precisa estar majoritariamente
preenchida pelo novo movimento. Em pé, `lift` passa a aparecer em X/Y: acurácia cai (contrato de
orientação).

**Etapa 3, Q2.** Intervalo ≈ 0,19 s com `STRIDE=12`; ≈ 0,99 s com 62; 2,0 s com 125.
`STRIDE = WINDOW_SIZE` ⇒ janelas **sem sobreposição** (janela "tombada", *tumbling window*).

**Etapa 3, Q4.** Domina o preenchimento da janela (~1 s para a maioria das 125 amostras ser do
novo movimento) + até 0,19 s de espera do próximo *stride*; inferência (~1 ms no PC) e serial
(~2 ms) são desprezíveis. Reduzir a latência: janela menor (perde resolução em baixa frequência,
pior para `maritime`) e/ou *stride* menor (mais CPU).

**Etapa 4, Q2.** Esperado: tempo por inferência de poucos ms no RPi 3 (algumas vezes o do PC),
muito abaixo dos 192 ms entre janelas ⇒ folga para tempo real; CPU livre para MQTT, registro em
banco, painel web (trilha full-stack) etc.
