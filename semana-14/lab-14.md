# Lab 14 — A arquitetura completa: nó ESP32 publicando MQTT para o broker no RPi 3

> **Antes de começar**: leia a [teoria-14](teoria-14.md) — a Figura 14-A é o desenho do que
> você vai montar hoje, peça por peça. Tenha o Exemplo 14.2 (tópicos e QoS) em mente ao
> observar o tráfego.

**Objetivo**: montar a arquitetura de produto — broker Mosquitto no RPi, nó sensor ESP32 com
Wi-Fi + DHT11 + LWT, comando remoto de LED por tópico, e registro de telemetria na borda —
depurando cada elo com as ferramentas certas.

**Duração**: 2 aulas.
**Material**: ESP32 + DHT11 (GPIO4), RPi 3 (do Lab 11, com SSH), rede Wi-Fi do laboratório (ou
hotspot do celular), PC Ubuntu.
**Divisão sugerida**: um integrante assume o broker (RPi), o outro o nó (ESP32); na Parte C,
invertam.
**Checkpoint 2 do projeto final é nesta semana** (integração/comunicação funcionando).

---

## Parte 0 — Sincronize o repositório

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — O broker no RPi (30 min)

1. Via SSH no RPi:

```bash
sudo apt update && sudo apt install -y mosquitto mosquitto-clients
```

2. Libere o acesso pela rede local (Mosquitto ≥ 2.x nasce trancado — teoria, seção 6):

```bash
echo -e "listener 1883\nallow_anonymous true" | sudo tee /etc/mosquitto/conf.d/lab.conf
sudo systemctl restart mosquitto
systemctl is-active mosquitto      # esperado: active
```

> **Observação:** `allow_anonymous true` é postura de laboratório. No relatório, cite as duas
> providências mínimas de um produto (usuário/senha + TLS) — está na teoria.

3. **Teste local** (o broker conversando consigo mesmo) — dois terminais SSH:

```bash
# terminal 1 (o "painel"):
mosquitto_sub -t 'ifes/#' -v
# terminal 2:
mosquitto_pub -t ifes/teste -m ola
```

O terminal 1 deve exibir `ifes/teste ola`. Anote o IP do RPi: `hostname -I`. Este é o
"multímetro" da Figura 14-E da teoria, ao vivo — **sempre** valide o barramento antes de
culpar o firmware (a mesma disciplina do `i2cdetect` do Lab 12).

4. **Teste pela rede**: do PC Ubuntu (instale `mosquitto-clients` se preciso), publique com
   `-h <IP_do_RPi>` e veja chegar no terminal 1. Barramento validado **antes** de qualquer
   firmware.

## Parte B — O nó ESP32 (50 min)

5. Edite em `~/sis-emb/semana-14/src/no_mqtt/main.c`: `WIFI_SSID`, `WIFI_PASS`,
   `BROKER_URI` (`mqtt://<IP_do_RPi>`) e `BANCADA`. Antes de gravar, releia a teoria (seções
   2 e 4) com o arquivo aberto: você deve saber apontar onde vivem o event group, o LWT e o
   `vTaskDelayUntil`.
6. `idf.py build flash monitor` e acompanhe a novela dos eventos:

```
I (1834) no_mqtt: IP obtido
I (2101) no_mqtt: MQTT conectado
I (2115) no_mqtt: pub temp=27 umid=63
W (7211) no_mqtt: DHT11 falhou (-2)      ← ocasional: checksum; o nó segue (Lab 12!)
```

7. No RPi, o `mosquitto_sub -t 'ifes/#' -v` da Parte A agora mostra o nó vivo:

```
ifes/bancada3/status online
ifes/bancada3/temp 27
ifes/bancada3/umid 63
```

   Repare no `status online` que chegou **antes** de tudo: é o retained publicado no
   `MQTT_EVENT_CONNECTED`. Feche e reabra o `mosquitto_sub`: o `online` reaparece na hora,
   mesmo sem o nó republicar — **retain** em ação (anote para o relatório).
8. **Comando remoto** — do RPi, mande o LED acender e apagar:

```bash
mosquitto_pub -t ifes/bancada3/cmd/led -m 1
mosquitto_pub -t ifes/bancada3/cmd/led -m 0
```

   O LED azul da placa obedece: um tópico virou GPIO — a semana 3 atendendo chamado da
   semana 14. Meça "no olho" a latência (imperceptível — rede local).
9. **LWT, o teste da morte súbita**: com o `mosquitto_sub` aberto, **puxe o cabo USB do ESP32**.
   Cronometrar: em quantos segundos chega `ifes/bancada3/status offline`? (Esperado ~7–10 s:
   keepalive de 5 s + margens do broker.) Religue o nó e veja o `online` retomar. Explique no
   relatório **quem** publicou o `offline` — não foi o nó, obviamente; ele estava morto. (Era
   o broker, cumprindo o "testamento" registrado na conexão: teoria, seção 3.)
10. **Curingas**: assine só as temperaturas de todas as bancadas da sala
    (`mosquitto_sub -t 'ifes/+/temp' -v`) — se outras duplas já estão no ar no MESMO broker,
    você as verá. Uma linha de reflexão: o que precisou mudar nos nós das outras duplas para o
    seu painel enxergá-los? (Nada — o desacoplamento do Exemplo 14.2: é exatamente o mesmo
    princípio da arbitragem por ID do CAN, Lab 10.)

## Parte C — Registro na borda (30 min)

11. No RPi, instale a biblioteca e rode o logger:

```bash
sudo apt install -y python3-paho-mqtt     # (ou pip3 install paho-mqtt --break-system-packages)
python3 ~/lab14/logger_rpi.py localhost   # copie a pasta src via scp como no Lab 12
```

    Saída: cada mensagem de `ifes/#` impressa e gravada em `telemetria.csv` com timestamp.
12. Deixe rodando **≥ 10 min** enquanto aquecem o DHT11 com a mão em alguns momentos. Encerre,
    traga o CSV ao PC (`scp`) e plote a série temporal — reaproveite 3 linhas do notebook da
    semana 13 (pandas lê o CSV; matplotlib plota `temp` × tempo) ou o LibreOffice.
13. Contemple o que está rodando: sensor com timing de µs no MCU → RTOS → Wi-Fi → pub/sub →
    broker Linux → arquivo → gráfico. **Essa é a pilha da disciplina inteira em produção.**

## Parte D — QoS e retain no comando (15 min) + Checkpoint 2

14. Publique o comando do LED com retain (`-r`):

```bash
mosquitto_pub -t ifes/bancada3/cmd/led -m 1 -r
```

    Agora **resete o ESP32** (botão EN). Ao reconectar e reassinar, o nó recebe o retained e o
    LED **volta aceso sozinho** — restauração de estado sem persistência local. Relacione com o
    Exemplo 14.2 (por que comando retained faz sentido aqui, e quando NÃO faria — dica: um
    comando de *incremento* retained faria o nó recém-ligado executar um incremento velho).
15. Limpe o retained ao final (`mosquitto_pub -t ifes/bancada3/cmd/led -n -r`) e mostrem o
    **Checkpoint 2 do projeto** ao professor.

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| Nó não pega IP | SSID/senha errados (olhe o log WIFI_EVENT) | confira as constantes; rede 2,4 GHz? |
| IP ok, MQTT não conecta | broker inacessível da rede do nó | teste A.4 do PC pela MESMA rede |
| `mosquitto_sub` mudo | assinatura no tópico errado | `ifes/#` com aspas simples |
| Comando não chega | nó reassinou sem o bit MQTT_OK? | log `MQTT_EVENT_SUBSCRIBED` no monitor |
| "offline" nunca chega | desligou com o cabo de rede, não com reset brusco | puxe a **energia** do nó (teste real) |

## Entrega (GitHub da dupla, `lab-14/relatorio.md`)

1. Print do teste local e do teste pela rede (Parte A) + as duas providências de segurança de
   produto.
2. Print do monitor do nó (eventos Wi-Fi→IP→MQTT) e do `mosquitto_sub` com status/temp/umid
   (Parte B.7) + a observação do retain no `status`.
3. Cronometragem do LWT (B.9) + a explicação de quem publica o `offline` (2–3 linhas).
4. `telemetria.csv` (≥ 10 min) + gráfico da temperatura (Parte C).
5. Relato do experimento retain no comando (D.14) com o "quando não faria sentido".
6. Papel do event group na partida do nó (3 linhas, com os nomes dos bits — Exemplo 14.1).

## Desafio (opcional)

Malha fechada distribuída: faça o RPi mandar no LED por regra — um script Python (base:
`logger_rpi.py`) que assina `ifes/bancada3/temp` e publica `cmd/led 1` quando T ≥ 29 °C e `0`
abaixo de 28 °C (histerese!). Aqueça o DHT11 com a mão e veja o LED acender **por decisão da
borda**. Você acabou de implementar, em 15 linhas, o esqueleto de um termostato IoT — e o
argumento de venda da trilha full-stack (`docs/trilha-fullstack.md`) para o projeto final.
