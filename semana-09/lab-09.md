# Lab 9 — I2C na veia: scanner, MPU-6050 e display

> **Antes de começar**: leia a [teoria-09](teoria-09.md) — a Figura 9-C (transação I2C)
> explica o que o scanner provoca na linha, e a seção 4 detalha o firmware de hoje linha a
> linha.

**Objetivo**: montar seu primeiro barramento I2C com dois dispositivos; diagnosticar com o
scanner; ler o MPU-6050 por mapa de registradores; calcular inclinação e (extra) exibi-la
num display.

**Duração**: 2 aulas.
**Material**: ESP32, MPU-6050, display OLED SSD1306 **ou** LCD 16x2 com módulo I2C, jumpers.
Sem resistores extras: os módulos já trazem pull-ups (teoria, Exemplo 9.4).

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — O barramento e o scanner (30 min)

1. **Wokwi primeiro**: adicione `wokwi-mpu6050` e ligue **VCC→3V3, GND→GND, SDA→GPIO21,
   SCL→GPIO22**. Cole `~/sis-emb/semana-09/src/i2c_scanner/main.c` e rode:

```
varredura I2C...
  dispositivo em 0x68
fim.
```

2. No hardware, monte o mesmo e regrave. Apareceu o 0x68? Então o barramento está vivo.
3. **Diagnóstico proposital** (aprenda a "cara" de cada falha, como nos Labs 4 e 5):
   - troque SDA↔SCL e rode o scanner → **nenhum** dispositivo (anote);
   - desligue só o GND do módulo → nada, ou endereços fantasmas aleatórios (anote);
   - religue tudo certo e confirme o 0x68 de volta.
   90 % dos "meu I2C não funciona" do semestre morrem neste passo 3.
4. Adicione o **display** ao MESMO barramento (mesmos dois fios! SDA/SCL em paralelo) e rode
   o scanner de novo. Esperado: **dois** endereços — 0x68 e 0x3C (OLED) ou 0x27/0x3F (LCD
   com PCF8574). Anote os seus. Dois chips, dois fios: o superpoder do I2C (Exemplo 9.3).

> 🧠 **Por que o scanner funciona**: ele envia só START + endereço + W̄ e verifica se
> **alguém puxou SDA para 0 no 9º bit** (o ACK da Figura 9-C da teoria). Nenhum dado é
> lido — é um censo de "quem está em casa", endereço por endereço.

## Parte B — MPU-6050: identidade e dados (40 min)

5. Grave `~/sis-emb/semana-09/src/mpu6050/main.c` (as funções `wr`/`rd` e a decodificação
   estão detalhadas na teoria, seções 4.1–4.2 — leia antes). Primeira linha esperada:

```
WHO_AM_I = 0x68 (esperado 0x68)
```

   O WHO_AM_I é o "aperto de mão": se vier 0x00 ou lixo, volte ao passo 3.
6. Fluxo contínuo esperado (10 Hz):

```
a=(0.02,-0.01,0.99) g  gx=0.3 o/s  theta=1.2 o
```

7. **Verificações físicas** (preencha a tabela):

| posição da placa | (ax, ay, az) esperado | theta esperado | medido |
|---|---|---|---|
| deitada, chip p/ cima | (0, 0, +1) g | ~0° | |
| em pé, borda USB p/ baixo | (±1, 0, 0) g | ~±90° | |
| de cabeça p/ baixo | (0, 0, −1) g | ~180°/−180° | |

   A soma vetorial √(ax²+ay²+az²) dá ~1,00 g em repouso? (É o teste de sanidade de todo
   acelerômetro — a gravidade é a "referência grátis" que a natureza fornece.)
8. **Sacuda** a placa: os valores estouram ±2 g? É a **saturação do fundo de escala** — o
   registrador `ACCEL_CONFIG (0x1C)` permite ±4/8/16 g. Desafio curto: escreva
   `wr(0x1C, 0x08)` (±4 g, muda a escala para 8192 LSB/g), ajuste a conversão e comprove que
   o mesmo sacolejo não satura mais. Documente a mudança.
9. **Endianness ao vivo** (semana 3 cobrando ingresso): comente a linha do `<<8` e monte o
   int16 na ordem errada (`(b[1]<<8)|b[0]`). O que acontece com os valores em repouso?
   (Lixo com "saltos" — valores plausíveis às vezes, absurdos noutras.) Registre um print e
   desfaça.

## Parte C — Dois no barramento: inclinômetro com display (40 min)

10. **Caminho OLED (recomendado)**: instale o componente do display no projeto:

```bash
idf.py add-dependency "espressif/ssd1306"
```

    e, seguindo o exemplo do próprio componente, escreva `theta` em fonte grande no display
    a 5 Hz. **Caminho LCD**: use o endereço achado na Parte A.4 e um driver PCF8574 (há
    vários no registry) — mesmo objetivo.
11. Requisitos mínimos do inclinômetro: valor de theta com 1 casa, atualização fluida, e
    uma seta/letra indicando o lado da inclinação. Filtre com média móvel M=4 (Lab 7
    reciclado!) para o display não "tremer".
12. Pergunta de projeto para o relatório: MPU e display dividem o barramento a 400 kHz.
    Estime, com o método do Exemplo 9.2, o tempo de (i) ler 14 bytes do MPU e (ii) enviar
    ~200 bytes de quadro ao OLED. Sobra tempo de sobra num período de 200 ms? Em que
    cenário o barramento viraria gargalo?

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| Scanner vazio | SDA/SCL trocados ou GND | o passo 3 deste lab |
| WHO_AM_I = 0x00 | sensor não alimentado / endereço errado | AD0 define 0x68/0x69 |
| Valores congelados | sensor desconectou no meio | reconecte; cheque os jumpers |
| Display mudo | endereço errado (0x3C × 0x3D) | o scanner da Parte A.4 diz qual |

## Entrega (GitHub da bancada, `lab-09/relatorio.md`)

1. Prints do scanner: saudável (2 endereços) e das duas falhas provocadas (A.3), com uma
   linha de diagnóstico para cada.
2. Tabela de verificação física do MPU (B.7) + o teste √(ax²+ay²+az²).
3. Registro do experimento de fundo de escala (B.8) e do endianness invertido (B.9).
4. Foto/vídeo do inclinômetro funcionando + estimativas de tempo da C.12.
5. Duas linhas: por que os módulos já vêm com pull-up e o que acontece com a resistência
   equivalente quando você põe dois módulos no mesmo barramento? (Exemplo 9.4.)

## Desafio (opcional)

Nível de bolha digital: use ax **e** ay para desenhar no OLED uma "bolha" (círculo) que
desliza conforme a inclinação em 2D, como um nível de pedreiro. Bônus se a bolha tiver
inércia (filtro passa-baixas nas duas coordenadas).
