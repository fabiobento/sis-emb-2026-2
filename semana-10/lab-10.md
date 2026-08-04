# Lab 10 — Uma rede CAN de verdade com dois ESP32 + TWAI

> **Antes de começar**: leia a [teoria-10](teoria-10.md) — a arbitragem do Exemplo 10.1
> acontecerá na sua rede hoje, e a Figura 10-B é o mapa da montagem física.

**Objetivo**: validar o protocolo em **selftest** (uma placa, sem fios); montar a rede
física com transceptores SN65HVD230 e terminação; trocar mensagens sensor→atuador; observar
a prioridade por ID e o comportamento de erro do CAN.

**Duração**: 2 aulas.
**Material (por bancada de bancadas — juntem-se!):** 2× ESP32, 2× transceptor SN65HVD230,
2× resistor 120 Ω, par de fios trançado (~50 cm; trancem jumpers!), protoboards.
**Semana de proposta**: hoje também é o prazo da **proposta do projeto final**
(`projeto-final/README.md`) — reservem os últimos 20 min.

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — Selftest: o protocolo sem a rede (25 min)

O CAN pune nós solitários (frame sem ACK = erro — teoria, seção 6). O modo selftest contorna
isso para testarmos **uma** placa, sem transceptor nenhum:

1. Confirme em `~/sis-emb/semana-10/src/twai_no/main.c`: `#define MODO_SELFTEST 1`. Grave.
2. Saída esperada (a placa recebe a própria publicação):

```
TWAI iniciado @500 kbit/s
RX ID=0x0A0 T=25.5 C  (tx=1 rx=1)
RX ID=0x0A0 T=26.0 C  (tx=2 rx=2)
```

3. Confira no código **o contrato do payload**: temperatura em décimos de °C, 2 bytes
   little-endian (`data[0] = t10 & 0xFF; data[1] = t10 >> 8`). Anote: este contrato
   explícito é o que a semana 3 (endianness) mandou fazer.
4. **Experimento do nó solitário**: mude para `MODO_SELFTEST 0` (modo NORMAL) **sem montar
   rede nenhuma** e regrave. O que acontece? (Esperado: transmissões falham/param — sem
   ninguém para dar ACK, o controlador acumula erros e se retira: o *confinamento*
   protegendo o barramento.) Copie o comportamento observado e volte para selftest até a
   Parte B estar montada.

## Parte B — Montando a rede física (35 min)

Agora em bancada de bancadas (2 placas):

5. Em **cada** lado: ESP32 **TX=GPIO21 → pino D** do SN65HVD230; **RX=GPIO22 ← pino R**;
   **3V3 → VCC; GND → GND**. (O SN65HVD230 é de 3,3 V — casa direto com o ESP32, sem
   conversor.)
6. Interligue os dois transceptores: **CANH↔CANH, CANL↔CANL** pelo par trançado, com **120 Ω
   entre CANH e CANL em CADA extremidade** (duas terminações — multímetro em ohms entre H e
   L com tudo desenergizado deve ler ~60 Ω: as duas em paralelo; use isso como teste de
   montagem!).

![Topologia do barramento CAN com terminação de 120 ohms nas extremidades](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/can_diferencial_topologia.png)

*Figura L10-A — O painel direito é a sua montagem: um barramento único, um resistor de 120 Ω
em cada ponta, nós pendurados no meio. O teste dos ~60 Ω verifica as duas terminações de uma
vez.*

7. **GND comum entre as duas bancadas** (sim, ele de novo — duas fontes USB diferentes
   precisam de referência comum para os transceptores conversarem).

## Parte C — Sensor e atuador conversando (30 min)

8. Placa 1: `MODO_SELFTEST 0`, `PAPEL_SENSOR 1` → publica T no **ID 0x0A0** a 10 Hz.
   Placa 2: `MODO_SELFTEST 0`, `PAPEL_SENSOR 0` → assina; T > 30 °C ⇒ publica comando de
   ventilador no **ID 0x120**.
9. Grave cada firmware na sua placa e abra **dois monitores** (um por bancada). Esperado,
   na placa 2:

```
RX ID=0x0A0 T=29.5 C  (tx=0 rx=9)
RX ID=0x0A0 T=30.5 C  (tx=0 rx=11)
```

   e, na placa 1, quando T cruza 30 °C:

```
RX comando ventilador=1
```

   A rede fechou o ciclo sensor→decisão→comando. Parabéns: vocês acabaram de reproduzir, em
   miniatura, o que acontece milhares de vezes por segundo em qualquer carro moderno.
10. **Teste de robustez** (só o CAN aguenta isso): com tudo rodando, **desconecte CANH por
    3 s** e reconecte. Observe: os nós registram erros, param... e **se recuperam sozinhos**
    quando o fio volta (o driver TWAI pode exigir `twai_initiate_recovery()`/restart
    dependendo do estado — se travar em BUS_OFF, resetem a placa e anotem: acabaram de ver o
    confinamento em ação).
11. **A arbitragem do Exemplo 10.1, ao vivo**: faça a placa 2 publicar o comando (0x120)
    **continuamente** (remova o `if (t > 30)`) enquanto a placa 1 publica 0x0A0 a 10 Hz. As
    temperaturas continuam chegando pontualmente? (Devem: 0x0A0 vence toda disputa.) Agora
    inverta os IDs nos dois firmwares (temperatura em 0x120, comando em 0x0A0) e observe se
    algo muda perceptivelmente nesta carga baixa (não deve — carga de ~2 % não estressa;
    registre a reflexão: prioridade importa quando a carga aperta, Exemplos 10.2/10.3).

## Parte D — Proposta do projeto final (20 min)

12. Entreguem a proposta de 1 página conforme `projeto-final/README.md`: problema, diagrama
    de blocos, componentes do inventário, cronograma interno. O professor aprova (ou ajusta)
    ainda hoje.

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| Selftest não recebe nada | `MODO_SELFTEST 0` esquecido | volte o define para 1 |
| Rede muda após montar | terminação ausente (~60 Ω teste) | 120 Ω em CADA ponta |
| Um lado recebe, o outro não | TX/RX trocados num lado | D←TX(21), R→RX(22) |
| BUS_OFF / nó calado | fio solto por tempo demais | reset; revise a fiação |

## Entrega (GitHub da bancada, `lab-10/relatorio.md`)

1. Print do selftest + comportamento do nó solitário em modo NORMAL (A.4) com explicação de
   1–2 linhas (ACK/confinamento).
2. Foto da rede montada com **as duas terminações visíveis** + a medição dos ~60 Ω (B.6).
3. Prints dos dois monitores mostrando o ciclo sensor→comando (C.9).
4. Relato do teste de robustez (C.10): mensagens de erro observadas e como a rede voltou.
5. Reflexão da C.11 (≤ 4 linhas): quando a escolha de IDs passa a importar de verdade?
6. Confirmação da proposta de projeto entregue (link no repositório do grupo).

## Desafio (opcional)

Temperatura de verdade no barramento: substitua a rampa sintética pelo DHT11 (código da
semana 14, função `dht11_ler`, já está no repositório — pode importar) e publique leituras
reais. Bônus: acrescente um 3º nó "painel" (outra bancada!) que só escuta e imprime tudo —
três nós, um par de fios, zero mudanças nos outros firmwares. Sinta o multiponto.
