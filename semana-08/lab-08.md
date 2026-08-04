# Lab 8 — PWM na prática: dimmer, servo e motor DC com ponte H

> **Antes de começar**: leia a [teoria-08](teoria-08.md) — os Exemplos 8.1–8.3 são as contas
> que você validará com multímetro e servo hoje, e a Figura 8-E (ponte H) é o mapa da Parte
> C. O checklist anti-fumaça da seção 3.2 da teoria é **obrigatório** antes de energizar o
> motor.

**Objetivo**: configurar o LEDC nos três "dialetos" do PWM — brilho (5 kHz/12 bits),
protocolo de servo (50 Hz/14 bits) e velocidade de motor (1 kHz/10 bits via L298N) —
medindo e validando as contas dos Exemplos 8.1–8.3.

**Duração**: 2 aulas (o segundo encontro da semana é a **P1** — este lab é o primeiro
encontro; a Parte C pode transbordar para a semana 9 se necessário).
**Material**: ESP32, LED + R 220 Ω; servo SG90; módulo L298N + motor TT + fonte externa
(fonte ajustável de protoboard em 6–7 V ou pack de pilhas); multímetro.

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — Dimmer: vendo o valor médio (30 min)

1. **Wokwi primeiro**: LED + R 220 Ω no GPIO2; cole
   `~/sis-emb/semana-08/src/ledc_dimmer/main.c` (dissecado na teoria, seção 1.1). Rode: o
   LED deve subir de brilho em rampa suave contínua.
2. No hardware, grave e **meça com o multímetro (VDC)** o GPIO2 durante a rampa: a tensão
   lida sobe suavemente de ~0 a ~3,3 V. O multímetro é lento — ele mostra exatamente o
   **valor médio** D·3,3 V da teoria (Figura 8-A). Anote 3 pontos (início/meio/fim).
3. **Experimento de cintilação** (fecha a questão 10 da Lista 3): mude `freq_hz` para
   **30** e regrave. A olho: pisca? Filme com a câmera do celular: aparecem
   faixas/flicker? Volte para 5000 e confirme que ambos somem. Registre as observações.
4. **Experimento resolução × frequência**: tente `freq_hz = 5000` com
   `LEDC_TIMER_15_BIT`. O `ledc_timer_config` deve **falhar** (cheque o retorno/erro no
   monitor): 80 MHz/2¹⁵ = 2,44 kHz < 5 kHz — o hardware não fecha a conta do Exemplo 8.1.
   Copie a mensagem de erro para o relatório.

## Parte B — Servo: PWM como protocolo (35 min)

5. **Wokwi**: adicione `wokwi-servo` (sinal no GPIO18, V+ no 5V, GND comum) e cole
   `~/sis-emb/semana-08/src/servo/main.c`. O braço deve varrer −90°→+90° em passos de 5°.
6. No hardware: servo SG90 — fio **laranja** = sinal (GPIO18), **vermelho** = 5 V (pino
   VIN/5V da placa serve para UM servo pequeno sem carga; mais que isso, fonte externa),
   **marrom** = GND.

![O idioma do servo: largura de pulso codifica o ângulo](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/servo_pulsos.png)

*Figura L8-A — Tenha esta figura ao lado durante a calibração: os extremos do *seu* servo
podem não ser os 0,5/2,4 ms nominais. Fonte: Practical Python Programming for IoT (Packt),
cap. 10, Fig. 10.4.*

7. **Calibração fina** (todo servo é único): encontre o `PULSO_MIN`/`PULSO_MAX` do *seu*
   servo. Método: fixe o laço num ângulo só (−90) e reduza `PULSO_MIN` de 50 em 50 µs até o
   servo "roncar" no batente — recue 50. Idem no outro extremo. Anote os valores achados e
   compare com os 500/2400 do código.
8. **Validando o Exemplo 8.2**: imprima o duty calculado para −90/0/+90 com seus pulsos
   calibrados (adicione um `printf` em `angulo_para_duty`). Os valores batem com a conta
   pulso·16384/20000? Preencha:

| ângulo | pulso (µs) | duty calculado | duty teórico (Ex. 8.2, pulsos padrão) |
|---|---|---|---|
| −90° | | | 819 (p/ 1000 µs) |
| 0° | | | 1229 (p/ 1500 µs) |
| +90° | | | 1638 (p/ 2000 µs) |

> **Observação:** se o servo "treme" parado, a causa clássica é alimentação fraca (queda no
> USB) ou GND ruim — não firmware. Multímetro no 5 V durante o movimento: caiu abaixo de
> 4,75 V? Fonte externa nele.

## Parte C — Motor DC + L298N (45 min)

**Antes de energizar, o ritual de segurança** (a teoria, seção 3.2, explica cada item):

- [ ] Fonte do motor (6–7 V) **separada** do USB do ESP32;
- [ ] **GND da fonte ↔ GND do L298N ↔ GND do ESP32** unidos (o fio mais importante da
      bancada);
- [ ] Jumper "5V-EN" do L298N conforme o módulo (regulador interno ligado p/ 7,4 V de
      entrada);
- [ ] Motor nos bornes OUT1/OUT2; nada encostando nas hélices/rodas.

![Esquemático de driver de motor com duas pontes H e alimentações separadas](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/l293d_esquematico.png)

*Figura L8-B — O esquemático de um driver da mesma família (L293D): identifique as duas
alimentações (lógica × motores), os pares de saída (cada um é uma ponte H) e os enables —
onde entra o nosso PWM. Fonte: Practical Python Programming for IoT (Packt), cap. 10,
Fig. 10.5.*

![Dois motores DC ligados aos dois pares de saída do driver](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/l293d_motor.png)

*Figura L8-C — O destino de cada par de saídas: o motor A vai nos bornes 1Y/2Y (nossos
OUT1/OUT2 do L298N) e o motor B nos 3Y/4Y — cada par é uma ponte H independente, com seu
próprio enable. No nosso lab usamos um motor só; o segundo par é o que você usaria no
projeto final para um robô de duas rodas. Fonte: Practical Python Programming for IoT
(Packt), cap. 10.*

9. Ligações lógicas: **ENA=GPIO16 (PWM), IN1=GPIO17, IN2=GPIO18**. Grave
   `~/sis-emb/semana-08/src/motor_l298n/main.c`: o ciclo demonstra aceleração 0→100 %,
   freio, ré a 60 %.
10. **Validando o Exemplo 8.3**: com o motor a 100 %, meça com o multímetro (i) a tensão da
    fonte na entrada do L298N e (ii) a tensão **nos bornes do motor**. A diferença é a
    queda da ponte. Deu perto dos ~1,4 V da teoria (às vezes ~2 V com as duas junções)?
    Registre.
11. **Zona morta**: reduza `p` de 10 em 10 e encontre o menor duty em que o motor ainda
    **gira partindo do repouso** (típico: 30–50 % — atrito + característica da ponte).
    Anote. Esse número reaparecerá na semana 13 como a razão de o PID precisar de termo
    integral (o P sozinho não vence a zona morta: perto do alvo, o erro pequeno gera duty
    abaixo do mínimo de partida — e o erro estaciona; o I acumula até vencer).
12. Troque o `sentido()` durante a rotação plena e observe (e ouça) o tranco. Boa prática
    de produto: parar (duty 0, pequena pausa) antes de inverter — implemente e comente por
    quê (dica: pico de corrente ≈ motor travado + fcem somadas — inverter em plena rotação
    é quase um curto para a ponte).

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| Servo treme/ronca parado | 5 V fraco (USB) | fonte externa; GND comum |
| Motor não gira | GND comum faltando (90 % dos casos) | o fio sagrado da checklist |
| Motor gira fraco | bateria descarregada / queda do L298N | meça na entrada e no motor (C.10) |
| `ledc_timer_config` retorna erro | conta f × resolução não fecha (A.4) | menos bits ou menos Hz |
| LED apagado no dimmer | duty invertido no código? | `ledc_update_duty` esquecido |

## Entrega (GitHub da bancada, `lab-08/relatorio.md`)

1. Três medições do valor médio (A.2) + observações de 30 Hz a olho e em vídeo (A.3) + a
   mensagem de erro de A.4 com a conta que a explica.
2. `PULSO_MIN/MAX` calibrados do seu servo + tabela de dutys da B.8.
3. Medições da queda do L298N (C.10) confrontadas com o Exemplo 8.3; duty mínimo de partida
   (C.11).
4. Foto da montagem do motor com o **fio de GND comum destacado** (círculo na foto).
5. Duas linhas: por que o servo exige 50 Hz mas o LED aceita "qualquer" f ≥ 200 Hz?
   (protocolo × filtragem — teoria, seções 1.2 e 2.)

## Desafio (opcional)

Radar de bancada: monte servo + (na semana 12 vocês terão o HC-SR04; por ora simule a
distância com o LDR do Lab 7) e faça o servo varrer ±60° imprimindo `angulo, leitura` em
CSV. Guarde o código: com o ultrassônico no lugar do LDR, isso vira um radar de verdade —
e um começo de projeto final.
