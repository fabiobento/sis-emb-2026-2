# Roteiro — Lab Extra: Medidor de Energia PZEM-004T (ESP32 + RPi + MQTT)

> ⚠️ **SEGURANÇA (releia o topo da `teoria-extra.md`).** O lado de **220 V** (ligação L/N e a
> garra CT no fio fase da carga) é montado, isolado e energizado **pelo professor**, com a
> bancada desligada durante o manuseio. **Alunos conectam apenas os 4 fios TTL**
> (5V/GND/RX/TX). Nunca toque no lado CA com a rede ligada.
>
> **Antes de começar**: leia a [teoria-extra.md](teoria-extra.md) — os Exemplos E.1–E.4 são o
> gabarito conceitual deste roteiro, e as Figuras E-A/E-B devem estar na bancada.

**Objetivo**: ler o PZEM-004T por Modbus-RTU (no ESP32 e no RPi), medir grandezas reais de uma
carga de bancada, comprovar na prática a diferença entre potência ativa e aparente, e publicar o
consumo (com custo em R$) via MQTT.

**Duração**: 2–3 aulas (ou trilha de projeto final).
**Material**: PZEM-004T v3.0 + CT de núcleo partido (já em caixa isolada, montado pelo professor);
ESP32; RPi 3 com broker (Lab 14); adaptador USB-TTL (para o caminho RPi direto); cargas de
teste — **lâmpada incandescente**, **lâmpada de LED** e **ventilador** (o trio que prova o
Exemplo E.2); jumpers fêmea.

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — Reconhecendo o protocolo antes de codar (20 min)

Antes do firmware, entenda o quadro Modbus na unha (teoria, seção 4, Figura E-B):

![Estrutura do quadro Modbus RTU de requisição](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/modbus_frame.png)

*Figura LE-A — O quadro que você montará no papel: endereço, função, registrador inicial,
quantidade e CRC-16.*

1. Monte, no papel, o quadro que **pede os 10 registradores de medição** ao PZEM de endereço
   `0xF8`, começando em `0x0000` (função 0x04). Confira contra o código: é o array `req[]` em
   `pzem_ler()` (ESP32) e o `bytes([...])` em `ler()` (RPi). Bateu? (Se montou direito, os seis
   primeiros bytes são: `F8 04 00 00 00 0A` — o CRC você calcula com a tabela/rotina do código.)
2. Localize no código de **ambos** os caminhos onde o CRC-16 é calculado e comparado. Por que o
   CRC é indispensável num fio que corre ao lado de 220 V? (Relacione com o checksum do DHT11 e o
   CRC do CAN — relatório. Rascunho da resposta: interferência eletromagnética de chaveamento e
   surtos da rede elétrica corrompem bits; o CRC-16 detecta ~99,99 % das corrupções e o firmware
   simplesmente descarta e repete — sem ele, um byte corrompido viraria "potência = 6553,5 W"
   num painel, e decisões erradas sairiam de dados mentirosos.)
3. Na tabela de registradores (teoria, seção 4), identifique **por que a corrente e a potência
   ocupam 2 registradores** e a tensão só 1. (É a resposta do exercício 5 da teoria: faixa ×
   resolução — 100 A a 0,001 A são 100 000 passos, que não cabem em 65 535; tensão 0–260 V a
   0,1 V cabe com folga.)

## Parte B — Caminho ESP32 (50 min)

**Conexões TTL (com a bancada do professor DESENERGIZADA):**

- [ ] PZEM **5V** → ESP32 **5V/VIN**  (alimenta os optoacopladores — os 4 fios são obrigatórios!)
- [ ] PZEM **GND** → ESP32 **GND**
- [ ] PZEM **TX** → ESP32 **GPIO16** (RX)   *(cruzado!)*
- [ ] PZEM **RX** → ESP32 **GPIO17** (TX)

> **Observação:** a datasheet (seção 5.1) é enfática: o TTL é **passivo** — sem os 4 fios
> (inclusive o 5V, que alimenta os optoacopladores de isolação), **não há comunicação**. Se o
> monitor mostrar só timeouts e o LED TX do PZEM não piscar, o primeiro suspeito é o 5V ou o
> par RX/TX trocado (é o "SDA/SCL invertido" da semana 9 em nova roupa).

4. Edite `src/pzem_esp32/main.c`: `WIFI_SSID`, `WIFI_PASS`, `BROKER_URI` (IP do RPi), `BANCADA`.
5. `idf.py build flash monitor`. **Com o professor energizando a carga**, você deve ver:

```
I (2101) pzem: medidor no ar
V=220.3 I=0.412 P=88.1W E=3Wh f=60.0Hz FP=0.79
V=220.1 I=0.410 P=87.6W E=3Wh f=60.0Hz FP=0.80
```

Se aparecer `resposta invalida` ou `CRC nao confere` de vez em quando: é tolerável (como o DHT11
da semana 12) — o código simplesmente tenta de novo. Se for **sempre**, volte às conexões TTL.

6. No RPi, assine e veja o nó publicando:

```bash
mosquitto_sub -t 'energia/#' -v
```

## Parte C — O experimento que surpreende (40 min) — Exemplo E.2 ao vivo

Meça as **três cargas** e preencha a tabela. Compare potência **aparente** (V·I) com **ativa**
(o que a conta cobra):

| Carga | V (V) | I (A) | P ativa (W) | S = V·I (VA) | FP | P/S confere? |
|---|---|---|---|---|---|---|
| Lâmpada incandescente | | | | | | |
| Lâmpada de LED | | | | | | |
| Ventilador | | | | | | |

7. Discussão obrigatória no relatório (a "aha!" do lab):
   - A **incandescente** é resistiva pura: FP ≈ 1, S ≈ P. É a referência.
   - O **ventilador** (motor indutivo): FP < 1, S > P — a diferença é a potência reativa
     (Exemplo E.2). Calcule Q = √(S²−P²) para ele.
   - A **lâmpada de LED** costuma ter **FP baixo** (0,5–0,7) por causa da fonte chaveada barata —
     surpresa! Um aparelho "econômico" pode maltratar a rede. Comente.
8. Se medir só a corrente (amperímetro) e multiplicar por 220, qual carga você **mais**
   superestimaria? Por quê? (É o argumento de existir um medidor de potência ativa: a de menor
   FP — tipicamente a lâmpada LED — seria superestimada em 1/FP = até 2×, porque parte da
   corrente "vai e volta" sem virar luz.)

![Tensão, corrente e triângulo de potências em CA](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/rms_potencia.png)

*Figura LE-B — Tenha o triângulo ao lado ao calcular Q: S² = P² + Q², FP = cos φ.*

## Parte D — Custo em reais na borda (30 min)

9. No RPi, rode o painel que assina tudo, grava CSV e calcula custo (ajuste `TARIFA_KWH` para a
   tarifa local — EDP ES):

```bash
pip3 install paho-mqtt --break-system-packages
python3 ~/lab-energia/painel_energia.py localhost      # copie src/rpi via scp (como no Lab 12)
```

Saída esperada:

```
P=88.1W  FP=0.79  E=0.003kWh  custo=R$0.00
P=1450.0W  FP=0.99  E=0.012kWh  custo=R$0.01     ← ligou o chuveiro? (só com o professor!)
```

10. Deixe o ventilador rodando **≥ 15 min** e acompanhe a energia (Wh) subir. Extrapole:
    quanto custaria deixá-lo ligado o mês inteiro? (kWh/mês × tarifa — a conta que todo mundo
    deveria saber fazer. Modelo de cálculo: P = 0,088 kW × 24 h × 30 dias = 63,4 kWh/mês;
    a R$ 0,90/kWh ≈ **R$ 57** — um ventilador deixado ligado o mês todo custa mais que um
    almoço por dia. Meçam com os números de vocês.)

## Parte E — Caminho RPi direto (opcional, 30 min)

Quando não há ESP32 sobrando, o RPi lê o PZEM diretamente por um adaptador **USB-TTL**:

11. Ligue o PZEM ao adaptador USB-TTL (5V/GND/RX/TX cruzados) e o adaptador a uma USB do RPi.
    Descubra a porta: `ls /dev/ttyUSB*`.
12. Rode:

```bash
pip3 install pyserial paho-mqtt --break-system-packages
python3 ~/lab-energia/pzem_rpi.py /dev/ttyUSB0 localhost
```

Mesmas leituras, agora sem o ESP32 no caminho. Compare no relatório: quando faz mais sentido cada
arquitetura (nó ESP32 remoto por Wi-Fi × RPi lendo direto)? (Dica: distância, número de pontos
de medição, custo, e a lição das semanas 11–14 sobre MCU × Linux — um medidor por cômodo com
ESP32 em cada um, *versus* um painel único de vários PZEM no mesmo barramento Modbus.)

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| Só timeouts no monitor | 5 V não ligado (optoacopladores apagados) ou RX/TX trocados | os 4 fios são obrigatórios; cruze TX/RX |
| CRC não confere sempre | fiação longa/ruído / paridade errada | UART **9600 8N1**; encurte os fios |
| Leituras zeradas com carga ligada | CT aberto ou abraçando o fio errado | garra no **fase** da carga (professor!) |
| `ttyUSB0` não aparece (Parte E) | adaptador não reconhecido | outro adaptador; `dmesg \| tail` |

## Entrega (GitHub da dupla, `lab-energia/relatorio.md`)

1. O quadro Modbus da Parte A.1 (bytes) + explicação do papel do CRC (A.2).
2. Print do monitor ESP32 medindo uma carga real (Parte B.5).
3. **Tabela das três cargas** preenchida (Parte C) + o cálculo de Q do ventilador + a discussão
   do FP da lâmpada de LED + a resposta de C.8.
4. `energia.csv` do painel (≥ 15 min) + a extrapolação de custo mensal do ventilador (D.10).
5. Comparação das duas arquiteturas (E.12), se fez a Parte E.

## Ideias de projeto final a partir daqui

- **Monitor de energia residencial com dashboard web**: PZEM → ESP32 → MQTT → RPi com a trilha
  full-stack (`docs/trilha-fullstack.md`) mostrando gráficos e custo em tempo real.
- **Alerta de sobrecarga**: usa o registrador de alarme do PZEM (0x0009) + a semana 4
  (interrupções) para cortar um relé quando P ultrapassa um limiar.
- **Comparador de eficiência**: banca de tomadas medindo vários aparelhos, ranqueando por FP e
  consumo — com apelo de sustentabilidade.
- **Submedição multiponto**: vários PZEM com endereços Modbus distintos no mesmo barramento
  (a datasheet suporta 0x01–0xF7), lidos por um RPi — introduz endereçamento Modbus de verdade.
