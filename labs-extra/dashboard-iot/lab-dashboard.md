# Roteiro — Lab Extra: Dashboard web profissional no Raspberry Pi (Flask + Socket.IO + MQTT + SQLite)

Lab integrador **opcional/avançado** (~6 aulas). Você constrói, etapa por etapa, uma aplicação web
que lê os nós ESP32 pelo broker MQTT do RPi, mostra tudo **ao vivo**, guarda o histórico, permite
**comandar** os atuadores e roda como **serviço de produção** que sobrevive a reboot. Todas as
etapas têm código de referência testado neste repositório — mas o **objetivo é você escrever e
entender**, não copiar.

**Pré-requisitos:** semanas 11–12 (Linux, venv, systemd, GPIO) e 14 (MQTT, LWT, retain, QoS). Leia
antes a [teoria-dashboard.md](teoria-dashboard.md). O mapa geral do projeto, com livros e rubrica,
é a [trilha full-stack](../../docs/trilha-fullstack.md). Quando algo der errado, abra o
[guia-problemas.md](guia-problemas.md). O modelo de relatório está em
[relatorio-modelo.md](relatorio-modelo.md).

> 🧪 **O que foi e o que não foi testado (leia):** o código Python tem **42 testes automáticos**
> que passam (incluindo Mosquitto real, comando de ida e volta e *last will*), e a pilha nginx +
> gunicorn foi testada no PC com WebSocket e token. **Não foram testados** pelo autor deste
> material: o `systemd` e o `instalar.sh` em um Raspberry Pi real, nós ESP32 reais (usou-se o
> simulador `tools/simulador_no.py`), o PWM do LED por `gpiozero` no hardware e o caminho com
> uWSGI. As capturas de tela e os históricos dos gráficos usam **dados simulados/sintéticos**.
> Se algo divergir no seu RPi, vale o conceito — e vale abrir um *issue* para corrigirmos.

## Mapa do lab

![Seis etapas cumulativas, cada uma com um teste de aceitação](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_roadmap.png)
*Figura L-1 — As partes deste lab seguem as etapas da [trilha](../../docs/trilha-fullstack.md).*

| Parte | Aula | O que você faz | Entregável (vai para o relatório) |
|---|---|---|---|
| 0 | 1 | venv, broker, simulador, testes | print de `mosquitto_sub` e de `pytest` verdes |
| A | 2 | API REST de um LED (Etapa 1) | `curl` válido e inválido (200 × 400) |
| B | 2 | WebSocket "ao vivo" (Etapa 2) | duas abas sincronizadas + print do Network |
| C | 3 | Ponte MQTT ↔ Web (Etapa 3) | `mosquitto_pub` aparece na tela; botão aparece no `mosquitto_sub` |
| D | 4–5 | Dashboard completo (Etapa 4) | testes passando, histórico, CSV, token 401, LWT offline |
| E | 5 | Medições: tráfego, disco, downsampling | tabela com **seus** números |
| F | 6 | Produção (Etapa 5) | `systemctl status`, reboot, `nginx -t` |
| G | 6 | Demonstração e relatório | roteiro de 5 min + relatório |

**Portas usadas** (para não colidirem): Parte A → 5000 · B → 5001 · C → 5002 · D/E → 5003 ·
F → 80 (nginx) e 8000 (gunicorn, só interno) · broker → 1883.

## Parte 0 — Preparação (aula 1, ~40 min)

1. **Obtenha o repositório** e entre na pasta do lab:

   ```bash
   cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
   cd labs-extra/dashboard-iot
   ls            # README, teoria, lab, guia-problemas, relatorio-modelo, src/
   ```

2. **Crie a venv e instale as dependências** (nunca `sudo pip`):

   ```bash
   python3 -m venv venv
   source venv/bin/activate
   pip install -r src/requirements.txt
   ```

3. **Broker.** Se ainda não tem o Mosquitto da semana 14:

   ```bash
   sudo apt install -y mosquitto mosquitto-clients
   sudo systemctl enable --now mosquitto
   ```

   *Mosquitto 2.x só aceita conexões locais por padrão. Para os nós ESP32 (outra máquina) usarem,
   copie `src/deploy/mosquitto-lab.conf` para `/etc/mosquitto/conf.d/lab.conf` e reinicie —
   laboratório isolado apenas; veja a Parte F para a versão com senha.*

4. **Nós simulados** (terminal 2; deixe rodando durante todo o lab):

   ```bash
   cd ~/sis-emb-2026-2/labs-extra/dashboard-iot && source venv/bin/activate && cd src
   python3 tools/simulador_no.py --bancadas bancada1 bancada2 --periodo 1 --movimento
   ```

5. **Confirme que há dados no barramento** (terminal 3):

   ```bash
   mosquitto_sub -h localhost -t 'ifes/#' -v
   ```

   Você deve ver `ifes/bancada1/temp 26.7`, `ifes/bancada1/umid 62`, `ifes/bancada1/status online`
   e, de vez em quando, `ifes/bancada1/movimento {"classe":"aceno","conf":0.93,"t":…}`.

6. **Rode os testes** — eles provam que o seu ambiente está são antes de você mexer em qualquer coisa:

   ```bash
   cd src && python3 -m pytest tests -q
   ```

   Esperado: `42 passed` (o teste ponta a ponta sobe seu próprio Mosquitto numa porta alta; sem o
   comando `mosquitto` no PATH ele é pulado e você verá `skipped`).

✅ **Entregável da Parte 0:** print do `mosquitto_sub` com mensagens e do `pytest` verde.

## Parte A — API REST de um LED (Etapa 1, aula 2)

**Objetivo:** controlar o brilho de um LED por HTTP/JSON, com **validação de entrada**.
Livro: **Smart, cap. 3, seção RESTful APIs (p. 60–85)**.

**Faça você primeiro** (15 min): sem olhar o código abaixo, escreva um `app.py` com `GET /led` e
`POST /led` usando Flask. Só depois compare.

<details>
<summary><code>src/etapa1_rest/app.py</code> (51 linhas) — clique para abrir</summary>

```python
"""Etapa 1 — API REST no RPi (Flask): GET /led, POST /led {"level": 0..100}.

Se o gpiozero e um LED real estiverem disponíveis (RPi), o brilho é aplicado por PWM no GPIO18;
caso contrário o estado fica só em memória (dá para testar no PC).

Rodar:   python3 app.py            (escuta em 0.0.0.0:5000)
Testar:  curl http://localhost:5000/led
         curl -X POST -H "Content-Type: application/json" -d '{"level": 50}' http://localhost:5000/led
"""
from flask import Flask, jsonify, request

app = Flask(__name__)
estado = {"level": 0}

try:                                  # no RPi: LED com PWM (semana 12). No PC: ignora.
    from gpiozero import PWMLED
    _led = PWMLED(18)
except Exception:                     # ImportError, ou GPIO indisponível
    _led = None


def aplica(level):
    estado["level"] = level
    if _led is not None:
        _led.value = level / 100.0


@app.get("/led")
def ler_led():
    return jsonify(estado)


@app.post("/led")
def escrever_led():
    dados = request.get_json(silent=True) or {}
    level = dados.get("level")
    # validar a entrada: REST sem validação é o começo de todo problema
    if not isinstance(level, (int, float)) or isinstance(level, bool) or not 0 <= level <= 100:
        return jsonify(erro="'level' deve ser um número entre 0 e 100"), 400
    aplica(level)
    return jsonify(estado)


@app.get("/leitura")                  # usado na Etapa 2 para medir o custo do polling
def leitura():
    import random, time
    return jsonify(valor=round(25 + random.uniform(-1, 1), 2), ts=time.time())


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)   # 0.0.0.0 = aceita conexões de outros PCs da rede
```

</details>

**Rodar e testar:**

```bash
cd src/etapa1_rest && python3 app.py            # terminal 4 (porta 5000)
```
```bash
curl -s localhost:5000/led
curl -s -X POST -H "Content-Type: application/json" -d '{"level": 50}'  localhost:5000/led
curl -s -X POST -H "Content-Type: application/json" -d '{"level": 500}' localhost:5000/led   # 400
curl -s -X POST -H "Content-Type: application/json" -d '{"level": "ab"}' localhost:5000/led  # 400
curl -i -X POST localhost:5000/led                                                           # corpo vazio → 400
```

**Com hardware:** ligue um LED (resistor de 330 Ω) ao **GPIO 18** do RPi — o código usa
`gpiozero.PWMLED(18)` automaticamente se encontrar a biblioteca; sem ela, só imprime o nível.
*(PWM em hardware real não foi testado pelo autor.)*

**Perguntas para o relatório:**
1. Por que `POST /led` devolve **400** e não 200 para `level = 500`? O que aconteceria sem a validação?
2. Por que o servidor escuta em `0.0.0.0` e não em `127.0.0.1`? Qual é o risco?
3. Faça um polling a 2 Hz (`watch -n 0.5 curl -s localhost:5000/leitura`) e abra a aba **Network**
   do navegador numa página que faça o mesmo: quantos bytes vão e voltam por requisição?

✅ **Chegamos!** — comando válido muda o estado; inválido devolve 400 e **não** muda nada.

## Parte B — "Ao vivo" por WebSocket (Etapa 2, aula 2)

**Objetivo:** ver o servidor **empurrar** dados e comparar com o polling. Livro: **Smart, cap. 3,
seção Web Sockets (p. 86–100)**.

<details>
<summary><code>src/etapa2_websocket/app.py</code> (45 linhas) — clique para abrir</summary>

```python
"""Etapa 2 — Dashboard "ao vivo" por WebSocket (Flask-SocketIO).

Uma thread simula um sensor (2 leituras/s). O servidor EMPURRA cada leitura aos navegadores
(evento "leitura"). A rota /leitura (REST) existe só para você comparar o custo do polling.

Rodar:  python3 app.py     e abra  http://localhost:5001/        (push)
                                    http://localhost:5001/?modo=poll   (polling a 2 Hz)
"""
import random
import time

from flask import Flask, jsonify, render_template
from flask_socketio import SocketIO

app = Flask(__name__)
socketio = SocketIO(app, async_mode="threading", cors_allowed_origins="*")
PERIODO = 0.5   # s  (2 Hz)


@app.get("/")
def index():
    return render_template("index.html")


@app.get("/leitura")
def leitura():
    return jsonify(valor=_ultimo["valor"], ts=_ultimo["ts"])


_ultimo = {"valor": 25.0, "ts": time.time()}


def sensor_simulado():
    v = 25.0
    while True:
        v += random.uniform(-0.3, 0.3)
        _ultimo.update(valor=round(v, 2), ts=time.time())
        socketio.emit("leitura", _ultimo)         # push para TODOS os navegadores conectados
        socketio.sleep(PERIODO)


socketio.start_background_task(sensor_simulado)

if __name__ == "__main__":
    socketio.run(app, host="0.0.0.0", port=5001, allow_unsafe_werkzeug=True)
```

</details>

<details>
<summary><code>src/etapa2_websocket/templates/index.html</code> (28 linhas) — clique para abrir</summary>

```html
<!doctype html>
<html lang="pt-br">
<head>
<meta charset="utf-8"><title>Etapa 2 — valor ao vivo</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>body{font-family:system-ui,sans-serif;max-width:32rem;margin:3rem auto;padding:0 1rem}
#v{font-size:4rem;font-weight:700}small{color:#666}</style>
</head>
<body>
<h1>Valor ao vivo</h1>
<div id="v">--</div>
<p><small id="modo"></small></p>
<script src="https://cdn.socket.io/4.7.5/socket.io.min.js"></script>
<script>
// baixe uma cópia local (curl -O https://cdn.socket.io/4.7.5/socket.io.min.js) se a rede do lab bloquear CDNs
const modo = new URLSearchParams(location.search).get("modo") || "ws";
const v = document.getElementById("v"), info = document.getElementById("modo");
function mostra(d){ v.textContent = d.valor.toFixed(2) + " °C"; }
if (modo === "poll") {
  info.textContent = "modo: polling a 2 Hz (fetch /leitura a cada 500 ms)";
  setInterval(() => fetch("/leitura").then(r => r.json()).then(mostra), 500);
} else {
  info.textContent = "modo: WebSocket (o servidor empurra; sem polling)";
  io().on("leitura", mostra);
}
</script>
</body>
</html>
```

</details>

**Rodar:**

```bash
cd src/etapa2_websocket && python3 app.py       # porta 5001
```

1. Abra `http://<ip-do-rpi>:5001/` em **duas abas**: os números mudam **juntos** — o servidor
   empurra para todos.
2. Abra `http://<ip-do-rpi>:5001/?modo=poll`: a mesma tela, por polling a 2 Hz.
3. Em ambas, abra as ferramentas de desenvolvedor (F12) → **Network**. No modo push você verá **uma**
   conexão `socket.io` com status **101** (aba *WS* → *Messages*); no polling, uma linha nova a
   cada 0,5 s.

![Polling pergunta sempre; WebSocket é avisado quando há dado novo](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_sequencia.png)
*Figura L-2 — O que você deve estar enxergando na aba Network.*

**Perguntas para o relatório:** (a) qual a latência média esperada do polling a 2 Hz? (*T/2 =
0,25 s*) (b) por que o botão F5 deixa de ser necessário? (c) o que acontece se você derrubar o
servidor e subir de novo — a página volta sozinha? (*o Socket.IO reconecta*).

✅ **Chegamos!** — duas abas sincronizadas e o print do Network mostrando o 101.

## Parte C — Ponte MQTT ↔ Web (Etapa 3, aula 3)

**Objetivo:** o servidor vira cliente MQTT: **dados** do broker para o navegador, **comandos** do
navegador para o broker. Livro: **Smart, cap. 4 (p. 101–130)**; código-irmão: `semana-14/src/logger_rpi.py`.

<details>
<summary><code>src/etapa3_mqtt_web/app.py</code> (57 linhas) — clique para abrir</summary>

```python
"""Etapa 3 — Ponte MQTT <-> Web.

O servidor Flask é um cliente MQTT a mais do broker local (Mosquitto do Lab 14):
  - assina  ifes/#  e repassa cada mensagem aos navegadores por WebSocket (evento "leitura");
  - recebe do navegador o evento "led" e publica  ifes/<bancada>/cmd/led  (QoS 1).
O ESP32 continua falando só MQTT: ele nem sabe que existe web (desacoplamento do pub/sub).

Rodar:   MQTT_HOST=localhost python3 app.py       e abra  http://localhost:5002/
Teste sem ESP32:  mosquitto_pub -t ifes/bancada1/temp -m 27.5
"""
import os

import paho.mqtt.client as mqtt
from flask import Flask, render_template
from flask_socketio import SocketIO

MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
BASE = os.getenv("TOPICO_BASE", "ifes")

app = Flask(__name__)
socketio = SocketIO(app, async_mode="threading", cors_allowed_origins="*")

cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)


def on_connect(client, userdata, flags, reason_code, properties=None):
    print(f"MQTT conectado a {MQTT_HOST} (rc={reason_code}); assinando {BASE}/#")
    client.subscribe(f"{BASE}/#")                 # (re)assina a cada (re)conexão


def on_message(client, userdata, msg):
    # callback roda na thread do paho: só repassa — nada de trabalho pesado aqui
    socketio.emit("leitura", {"topico": msg.topic, "valor": msg.payload.decode(errors="replace")})


cli.on_connect, cli.on_message = on_connect, on_message
cli.reconnect_delay_set(min_delay=1, max_delay=30)


@app.get("/")
def index():
    return render_template("index.html", base=BASE)


@socketio.on("led")
def comando_led(dados):
    bancada, valor = str(dados.get("bancada", "")), str(dados.get("valor", ""))
    if not bancada.isalnum() or valor not in ("0", "1"):      # nunca publique entrada não validada
        return {"ok": False}
    cli.publish(f"{BASE}/{bancada}/cmd/led", valor, qos=1)
    return {"ok": True}


if __name__ == "__main__":
    cli.connect_async(MQTT_HOST, 1883, keepalive=30)
    cli.loop_start()
    socketio.run(app, host="0.0.0.0", port=5002, allow_unsafe_werkzeug=True)
```

</details>

<details>
<summary><code>src/etapa3_mqtt_web/templates/index.html</code> (23 linhas) — clique para abrir</summary>

```html
<!doctype html>
<html lang="pt-br">
<head>
<meta charset="utf-8"><title>Etapa 3 — MQTT na web</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>body{font-family:system-ui,sans-serif;max-width:40rem;margin:2rem auto;padding:0 1rem}
pre{background:#111;color:#9f9;padding:1rem;height:16rem;overflow:auto;border-radius:.5rem}
button{font-size:1rem;padding:.5rem 1rem;margin-right:.5rem}</style>
</head>
<body>
<h1>Barramento MQTT no navegador</h1>
<p>Bancada: <input id="b" value="bancada1" size="10">
<button onclick="led(1)">LED ligado</button><button onclick="led(0)">LED desligado</button></p>
<pre id="log">aguardando mensagens de {{ base }}/# …</pre>
<script src="https://cdn.socket.io/4.7.5/socket.io.min.js"></script>
<script>
const sock = io(), log = document.getElementById("log");
sock.on("leitura", d => { log.textContent += `${d.topico} ${d.valor}\n`; log.scrollTop = log.scrollHeight; });
function led(v){ sock.emit("led", {bancada: document.getElementById("b").value, valor: v},
                           r => log.textContent += r.ok ? "→ comando enviado\n" : "→ REJEITADO\n"); }
</script>
</body>
</html>
```

</details>

**Rodar e testar** (com o simulador da Parte 0 ainda ligado):

```bash
cd src/etapa3_mqtt_web && MQTT_HOST=localhost python3 app.py       # porta 5002
```

1. Abra `http://<ip-do-rpi>:5002/` — a lista mostra as mensagens do simulador chegando.
2. Publique à mão: `mosquitto_pub -t ifes/bancada1/temp -m 31.5` — **aparece na tela?**
3. Em outro terminal: `mosquitto_sub -t 'ifes/+/cmd/#' -v`; clique **LED on** no navegador — a
   mensagem `ifes/bancada1/cmd/led 1` deve aparecer no terminal. É o caminho **inverso**.

**Experimento de robustez (obrigatório, vale ponto):** com a página aberta, rode
`sudo systemctl restart mosquitto`. O que acontece com os dados? (*o paho reconecta e, como a
assinatura está em `on_connect`, volta a receber*). Agora **mova** a linha `subscribe` para fora do
`on_connect` e repita — o que muda? Explique usando o conceito de *clean session*.

**Perguntas para o relatório:** por que o navegador **não** assina o MQTT diretamente? Por que o
comando usa QoS 1? O que impede um `bancada` malicioso (`x/../`) de ser publicado?

✅ **Chegamos!** — um `mosquitto_pub` move a tela e um clique na tela aparece no `mosquitto_sub`.

## Parte D — O dashboard completo (Etapa 4, aulas 4–5)

![Arquitetura do painel, do sensor ao navegador, com o caminho do comando de volta](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_arquitetura.png)
*Figura L-3 — As peças da Parte D. Cada arquivo abaixo é um retângulo desta figura.*

As etapas anteriores eram um único arquivo cada. Um painel de verdade precisa de **organização**:
um pacote Python com responsabilidades separadas. Leia os arquivos **nesta ordem** — da folha
(sem dependências) para a raiz:

| # | Arquivo | Responsabilidade | Depende de |
|---|---|---|---|
| 1 | `dashboard/config.py` | configuração por variáveis de ambiente | — |
| 2 | `dashboard/topicos.py` | **interpretar** um tópico/payload (função pura) | — |
| 3 | `dashboard/db.py` | SQLite: gravar, consultar, purgar, reamostrar | — |
| 4 | `dashboard/mqtt_bridge.py` | cliente MQTT robusto; publica comandos | `topicos` |
| 5 | `dashboard/app.py` | rotas REST, eventos Socket.IO, token | tudo acima |
| 6 | `dashboard/wsgi.py` | ponto de entrada de produção | `app` |
| 7 | `templates/` + `static/` | páginas, gráficos, estilo | — |

### D.1 Configuração: nada de senha no código

<details>
<summary><code>src/dashboard/config.py</code> (25 linhas) — clique para abrir</summary>

```python
"""Configuração por variáveis de ambiente (12-factor): nada de senha dentro do código.

Em produção as variáveis vêm de /etc/default/dashboard (lido pelo systemd; ver deploy/).
"""
import os


def _int(nome, padrao):
    try:
        return int(os.getenv(nome, padrao))
    except ValueError:
        return padrao


class Config:
    MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
    MQTT_PORT = _int("MQTT_PORT", 1883)
    MQTT_USER = os.getenv("MQTT_USER", "")
    MQTT_PASS = os.getenv("MQTT_PASS", "")
    TOPICO_BASE = os.getenv("TOPICO_BASE", "ifes")           # ifes/<bancada>/<grandeza>
    DB_PATH = os.getenv("DB_PATH", "dashboard.db")
    DASH_TOKEN = os.getenv("DASH_TOKEN", "")                 # vazio = sem token (laboratório)
    RETENCAO_DIAS = _int("RETENCAO_DIAS", 7)                 # leituras mais antigas são apagadas
    PONTOS_INICIAIS = _int("PONTOS_INICIAIS", 120)           # histórico enviado ao abrir a página
    PURGA_A_CADA = _int("PURGA_A_CADA", 500)                 # inserções entre duas purgas
```

</details>

**Por quê:** a mesma aplicação roda no seu notebook, no RPi do laboratório e em produção; só mudam
as variáveis. Senhas **nunca** vão para o Git.

### D.2 A função pura `interpreta` (aprendendo por testes)

<details>
<summary><code>src/dashboard/topicos.py</code> (62 linhas) — clique para abrir</summary>

```python
"""Interpretação dos tópicos MQTT — função PURA (sem rede, sem banco), por isso fácil de testar.

Árvore de tópicos adotada (padrão da semana 14, prefixo `ifes`):

    ifes/<bancada>/<grandeza>            número em texto  -> leitura (temp, umid, dist, ...)
    ifes/<bancada>/status                online | offline -> estado do nó (LWT)
    ifes/<bancada>/movimento             JSON {"classe":..,"conf":..}  (trilha TinyML)
    ifes/<bancada>/movimento/info        ignorado (diagnóstico do nó)
    ifes/<bancada>/cmd/<atuador>         comandos (o dashboard os PUBLICA; não os grava)
"""
import json
import math
import re

RE_BANCADA = re.compile(r"^[a-z0-9_-]{1,32}$")
RE_GRANDEZA = re.compile(r"^[a-z0-9_]{1,32}$")
RE_CLASSE = re.compile(r"^[A-Za-z0-9_]{1,32}$")
RESERVADOS = {"status", "movimento", "cmd"}


def interpreta(base, topico, payload, agora):
    """Devolve um dict descrevendo o dado, ou None se a mensagem deve ser ignorada.

    base: prefixo (ex. "ifes"); topico: str; payload: bytes|str; agora: timestamp (s).
    O ESP32 não tem relógio de parede: o carimbo de tempo é o da CHEGADA ao servidor.
    """
    partes = topico.split("/")
    if len(partes) < 3 or partes[0] != base:
        return None
    bancada, resto = partes[1], partes[2:]
    if not RE_BANCADA.match(bancada):
        return None
    if isinstance(payload, bytes):
        payload = payload.decode("utf-8", errors="replace")
    texto = payload.strip()

    if resto == ["status"]:
        if texto not in ("online", "offline"):
            return None
        return {"tipo": "status", "bancada": bancada, "texto": texto, "ts": agora}

    if resto == ["movimento"]:
        try:
            d = json.loads(texto)
            classe, conf = d["classe"], float(d["conf"])
        except (ValueError, KeyError, TypeError):
            return None
        if not isinstance(classe, str) or not RE_CLASSE.match(classe) or not 0.0 <= conf <= 1.0:
            return None
        return {"tipo": "movimento", "bancada": bancada, "classe": classe,
                "conf": round(conf, 3), "ts": agora}

    if len(resto) == 1 and resto[0] not in RESERVADOS and RE_GRANDEZA.match(resto[0]):
        try:
            valor = float(texto)
        except ValueError:
            return None
        if not math.isfinite(valor):                 # "nan" e "inf" passam em float(): barre aqui
            return None
        return {"tipo": "leitura", "bancada": bancada, "grandeza": resto[0],
                "valor": valor, "ts": agora}
    return None
```

</details>

**Exercício guiado (TDD, 30 min) — o mais importante da Parte D.** Copie o arquivo e **apague o
corpo** de `interpreta`, deixando só `return None`. Rode:

```bash
python3 -m pytest tests/test_topicos.py -q       # 4 falham, 21 "passam" — é o esperado (veja a nota)
```

> 🔎 **Por que 21 testes passam com um corpo vazio?** Porque eles verificam que o lixo é **ignorado**
> (`None`) — e `return None` ignora tudo. Moral: *um teste que só verifica "não quebra" é fraco*; os 4
> que falham são os que exigem **comportamento**. Pense nisso ao escrever os seus.

Agora implemente aos poucos, um teste por vez, até ficar verde. Cada teste vira uma regra de
negócio (`nan` é lixo, `conf` fora de 0–1 é lixo, tópico fora do contrato é ignorado…). Depois
**acrescente o seu teste**: o tópico `ifes/bancada1/dist` com `abc` deve ser ignorado. *Mostre
no relatório um commit com o teste novo.*

### D.3 O banco de dados

<details>
<summary><code>src/dashboard/db.py</code> (137 linhas) — clique para abrir</summary>

```python
"""Camada de persistência — SQLite (um arquivo, sem servidor).

Duas tabelas:
  leituras(ts, bancada, grandeza, valor)   séries numéricas (temp, umid, ...)
  eventos(ts, bancada, tipo, texto, extra) status online/offline, classe de movimento, comandos

Modo WAL: o escritor (thread do MQTT) e os leitores (requisições web) não se bloqueiam.
Uma conexão curta por operação: simples e seguro com threads (sqlite3 não compartilha
conexões entre threads por padrão).
"""
import sqlite3
import time
from contextlib import contextmanager

ESQUEMA = """
CREATE TABLE IF NOT EXISTS leituras (
    id       INTEGER PRIMARY KEY,
    ts       REAL NOT NULL,
    bancada  TEXT NOT NULL,
    grandeza TEXT NOT NULL,
    valor    REAL NOT NULL
);
CREATE INDEX IF NOT EXISTS ix_leituras ON leituras (bancada, grandeza, ts);
CREATE TABLE IF NOT EXISTS eventos (
    id      INTEGER PRIMARY KEY,
    ts      REAL NOT NULL,
    bancada TEXT NOT NULL,
    tipo    TEXT NOT NULL,
    texto   TEXT NOT NULL,
    extra   REAL
);
CREATE INDEX IF NOT EXISTS ix_eventos ON eventos (bancada, tipo, ts);
"""


class Banco:
    def __init__(self, caminho):
        self.caminho = caminho

    @contextmanager
    def _con(self):
        con = sqlite3.connect(self.caminho, timeout=5)
        con.row_factory = sqlite3.Row
        try:
            yield con
            con.commit()
        finally:
            con.close()

    def iniciar(self):
        with self._con() as con:
            con.execute("PRAGMA journal_mode=WAL")
            con.executescript(ESQUEMA)

    # ---------------- escrita ----------------
    def inserir_leitura(self, ts, bancada, grandeza, valor):
        with self._con() as con:
            con.execute("INSERT INTO leituras (ts,bancada,grandeza,valor) VALUES (?,?,?,?)",
                        (ts, bancada, grandeza, valor))

    def inserir_evento(self, ts, bancada, tipo, texto, extra=None):
        with self._con() as con:
            con.execute("INSERT INTO eventos (ts,bancada,tipo,texto,extra) VALUES (?,?,?,?,?)",
                        (ts, bancada, tipo, texto, extra))

    def purgar(self, dias, agora=None):
        limite = (agora or time.time()) - dias * 86400
        with self._con() as con:
            a = con.execute("DELETE FROM leituras WHERE ts < ?", (limite,)).rowcount
            b = con.execute("DELETE FROM eventos WHERE ts < ?", (limite,)).rowcount
        return a + b

    # ---------------- leitura ----------------
    def ultimas(self, bancada, grandeza, n):
        """Últimas n leituras, em ordem cronológica: [[ts, valor], ...]."""
        with self._con() as con:
            linhas = con.execute(
                "SELECT ts, valor FROM leituras WHERE bancada=? AND grandeza=? "
                "ORDER BY ts DESC LIMIT ?", (bancada, grandeza, n)).fetchall()
        return [[r["ts"], r["valor"]] for r in reversed(linhas)]

    def historico(self, bancada, grandeza, desde, ate, bucket_s):
        """Série reamostrada: média/mín/máx por balde de bucket_s segundos.

        Reduzir no BANCO (GROUP BY) e não no navegador: 24 h a 0,2 Hz são ~17 mil pontos;
        o gráfico só precisa de algumas centenas.
        """
        with self._con() as con:
            linhas = con.execute(
                "SELECT CAST(ts / :b AS INTEGER) * :b AS t, AVG(valor) AS media, "
                "MIN(valor) AS minimo, MAX(valor) AS maximo, COUNT(*) AS n "
                "FROM leituras WHERE bancada=:ban AND grandeza=:g AND ts BETWEEN :d AND :a "
                "GROUP BY 1 ORDER BY 1",
                {"b": bucket_s, "ban": bancada, "g": grandeza, "d": desde, "a": ate}).fetchall()
        return [{"t": r["t"], "media": r["media"], "min": r["minimo"], "max": r["maximo"],
                 "n": r["n"]} for r in linhas]

    def grandezas(self, bancada):
        with self._con() as con:
            return [r[0] for r in con.execute(
                "SELECT DISTINCT grandeza FROM leituras WHERE bancada=? ORDER BY 1", (bancada,))]

    def eventos_recentes(self, n=30):
        with self._con() as con:
            linhas = con.execute("SELECT ts,bancada,tipo,texto,extra FROM eventos "
                                 "ORDER BY ts DESC, id DESC LIMIT ?", (n,)).fetchall()
        return [dict(r) for r in linhas]

    def nos(self):
        """Estado atual de cada nó: último status, último valor de cada grandeza, último movimento."""
        with self._con() as con:
            bancadas = sorted({r[0] for r in con.execute(
                "SELECT bancada FROM leituras UNION SELECT bancada FROM eventos")})
            saida = {}
            for b in bancadas:
                ultimo = {}
                for r in con.execute(
                        "SELECT grandeza, valor, MAX(ts) AS ts FROM leituras "
                        "WHERE bancada=? GROUP BY grandeza", (b,)):
                    ultimo[r["grandeza"]] = {"valor": r["valor"], "ts": r["ts"]}
                st = con.execute("SELECT texto, ts FROM eventos WHERE bancada=? AND tipo='status' "
                                 "ORDER BY ts DESC, id DESC LIMIT 1", (b,)).fetchone()
                mv = con.execute("SELECT texto, extra, ts FROM eventos WHERE bancada=? AND "
                                 "tipo='movimento' ORDER BY ts DESC, id DESC LIMIT 1", (b,)).fetchone()
                saida[b] = {
                    "status": st["texto"] if st else "desconhecido",
                    "status_ts": st["ts"] if st else None,
                    "ultimo": ultimo,
                    "movimento": ({"classe": mv["texto"], "conf": mv["extra"], "ts": mv["ts"]}
                                  if mv else None),
                }
        return saida

    def contagens(self):
        with self._con() as con:
            return {"leituras": con.execute("SELECT COUNT(*) FROM leituras").fetchone()[0],
                    "eventos": con.execute("SELECT COUNT(*) FROM eventos").fetchone()[0]}
```

</details>

**Leia procurando:** (1) o `PRAGMA journal_mode=WAL`; (2) todos os `?` e `:nome` (SQL
parametrizado); (3) o `GROUP BY` do `historico` (downsampling); (4) por que `_con()` abre e fecha
uma conexão por operação. Abra o banco para olhar com os olhos:

```bash
sqlite3 dashboard.db ".schema" "select count(*) from leituras;" \
  "select datetime(ts,'unixepoch','localtime'), bancada, grandeza, valor from leituras order by ts desc limit 5;"
```

*(Sem `sqlite3`: `sudo apt install sqlite3`.)*

### D.4 A ponte MQTT

<details>
<summary><code>src/dashboard/mqtt_bridge.py</code> (68 linhas) — clique para abrir</summary>

```python
"""Ponte MQTT: o dashboard como mais um cliente do broker (padrão do logger_rpi.py, semana 14).

- assina  <base>/#  em CADA (re)conexão (a assinatura some com a sessão limpa);
- entrega cada mensagem interpretada ao callback `ao_dado` (que grava e empurra aos navegadores);
- publica comandos (QoS 1) em  <base>/<bancada>/cmd/<atuador>.
"""
import logging
import re
import socket
import time

import paho.mqtt.client as mqtt

from .topicos import interpreta

log = logging.getLogger("dashboard.mqtt")
RE_BANCADA = re.compile(r"^[a-z0-9_-]{1,32}$")


class Ponte:
    def __init__(self, cfg, ao_dado):
        self.cfg, self.ao_dado = cfg, ao_dado
        self.conectado = False
        # client_id ÚNICO: dois clientes com o mesmo id derrubam um ao outro em loop
        self.cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                               client_id=f"dashboard-{socket.gethostname()}-{int(time.time())}")
        if cfg.MQTT_USER:
            self.cli.username_pw_set(cfg.MQTT_USER, cfg.MQTT_PASS)
        self.cli.reconnect_delay_set(min_delay=1, max_delay=30)
        self.cli.on_connect = self._on_connect
        self.cli.on_disconnect = self._on_disconnect
        self.cli.on_message = self._on_message

    # ---- ciclo de vida ----
    def iniciar(self):
        self.cli.connect_async(self.cfg.MQTT_HOST, self.cfg.MQTT_PORT, keepalive=30)
        self.cli.loop_start()          # thread própria do paho (rede + reconexão automática)

    def parar(self):
        self.cli.loop_stop()
        self.cli.disconnect()

    # ---- callbacks (thread do paho: mantenha-os curtos e sem exceções não tratadas) ----
    def _on_connect(self, cli, userdata, flags, reason_code, properties=None):
        ok = not reason_code.is_failure
        self.conectado = ok
        log.info("MQTT %s (%s)", "conectado" if ok else "RECUSADO", reason_code)
        if ok:
            cli.subscribe(f"{self.cfg.TOPICO_BASE}/#", qos=0)

    def _on_disconnect(self, cli, userdata, flags, reason_code, properties=None):
        self.conectado = False
        log.warning("MQTT desconectado (%s); o paho tentará reconectar", reason_code)

    def _on_message(self, cli, userdata, msg):
        try:
            dado = interpreta(self.cfg.TOPICO_BASE, msg.topic, msg.payload, time.time())
            if dado is not None:
                self.ao_dado(dado)
        except Exception:                      # uma mensagem ruim não pode derrubar a ponte
            log.exception("erro ao processar %s", msg.topic)

    # ---- comandos ----
    def publica_led(self, bancada, valor):
        if not RE_BANCADA.match(bancada) or valor not in (0, 1):
            return False
        info = self.cli.publish(f"{self.cfg.TOPICO_BASE}/{bancada}/cmd/led", str(valor), qos=1)
        return info.rc == mqtt.MQTT_ERR_SUCCESS
```

</details>

**Compare com a Parte C** e liste, no relatório, **cinco** melhorias de robustez que a versão da
Etapa 3 ainda não tinha (*client_id único, reconexão com espera, `try/except` no callback,
validação via `interpreta`, comando validado e com retorno de sucesso*).

### D.5 A aplicação

<details>
<summary><code>src/dashboard/app.py</code> (163 linhas) — clique para abrir</summary>

```python
"""Aplicação Flask + Socket.IO: páginas, API REST e tempo real.

Fluxo dos dados (ver Figura F-? do lab):
  ESP32 --MQTT--> broker --> Ponte (thread paho) --> ao_dado():  grava no SQLite
                                                                + socketio.emit() aos navegadores
  navegador --POST /api/cmd/led--> ponte.publica_led() --MQTT QoS 1--> broker --> ESP32
"""
import csv
import io
import math
import re
import time

from flask import Flask, Response, jsonify, render_template, request
from flask_socketio import SocketIO, emit

from . import __version__
from .config import Config
from .db import Banco
from .mqtt_bridge import Ponte

socketio = SocketIO()
RE_NOME = re.compile(r"^[a-z0-9_-]{1,32}$")


class Estado:
    """Agrupa as peças ligadas à aplicação (guardado em app.extensions['dash'])."""

    def __init__(self, cfg, iniciar_mqtt):
        self.cfg = cfg
        self.t0 = time.time()
        self.banco = Banco(cfg.DB_PATH)
        self.banco.iniciar()
        self._n = 0
        self.ponte = Ponte(cfg, self.ao_dado)
        self.iniciar_mqtt = iniciar_mqtt

    def ao_dado(self, d):
        """Chamado pela thread do MQTT para cada mensagem válida: grava e empurra."""
        if d["tipo"] == "leitura":
            self.banco.inserir_leitura(d["ts"], d["bancada"], d["grandeza"], d["valor"])
        elif d["tipo"] == "status":
            self.banco.inserir_evento(d["ts"], d["bancada"], "status", d["texto"])
        elif d["tipo"] == "movimento":
            self.banco.inserir_evento(d["ts"], d["bancada"], "movimento", d["classe"], d["conf"])
        socketio.emit(d["tipo"], d)                          # push a todos os navegadores
        self._n += 1
        if self._n % self.cfg.PURGA_A_CADA == 0:             # retenção: o disco do RPi é finito
            self.banco.purgar(self.cfg.RETENCAO_DIAS)

    def snapshot(self):
        nos = self.banco.nos()
        series = {}
        for b in nos:
            for g in self.banco.grandezas(b):
                series.setdefault(g, {})[b] = self.banco.ultimas(b, g, self.cfg.PONTOS_INICIAIS)
        return {"nos": nos, "series": series, "eventos": self.banco.eventos_recentes(20),
                "mqtt": self.ponte.conectado}


def create_app(cfg=None, iniciar_mqtt=True):
    cfg = cfg or Config
    app = Flask(__name__)
    est = Estado(cfg, iniciar_mqtt)
    app.extensions["dash"] = est
    socketio.init_app(app, async_mode="threading")

    # ---------------- autenticação por token (opcional) ----------------
    def exige_token():
        if not cfg.DASH_TOKEN:
            return None
        if request.headers.get("Authorization", "") != f"Bearer {cfg.DASH_TOKEN}":
            return jsonify(erro="token ausente ou inválido"), 401
        return None

    # ---------------- páginas ----------------
    @app.get("/")
    def pagina_inicio():
        return render_template("index.html", versao=__version__, base=cfg.TOPICO_BASE,
                               com_token=bool(cfg.DASH_TOKEN))

    @app.get("/historico")
    def pagina_historico():
        return render_template("historico.html", versao=__version__)

    # ---------------- API REST ----------------
    @app.get("/api/saude")
    def saude():
        return jsonify(status="ok", versao=__version__, mqtt=est.ponte.conectado,
                       uptime_s=round(time.time() - est.t0), **est.banco.contagens())

    @app.get("/api/nos")
    def api_nos():
        nos = est.banco.nos()
        for b, n in nos.items():
            n["grandezas"] = sorted(n["ultimo"])
        return jsonify(nos)

    @app.get("/api/leituras")
    def api_leituras():
        b, g = request.args.get("bancada", ""), request.args.get("grandeza", "")
        n = min(max(request.args.get("n", 100, type=int), 1), 5000)
        if not RE_NOME.match(b) or not RE_NOME.match(g):
            return jsonify(erro="bancada/grandeza inválidas"), 400
        return jsonify(est.banco.ultimas(b, g, n))

    def _historico_params():
        b, g = request.args.get("bancada", ""), request.args.get("grandeza", "")
        horas = request.args.get("horas", 1.0, type=float)
        pontos = min(max(request.args.get("pontos", 300, type=int), 10), 2000)
        if not RE_NOME.match(b) or not RE_NOME.match(g) or not 0 < horas <= 24 * 30:
            return None
        ate = time.time()
        desde = ate - horas * 3600
        bucket = max(1, math.ceil(horas * 3600 / pontos))
        return b, g, desde, ate, bucket

    @app.get("/api/historico")
    def api_historico():
        p = _historico_params()
        if p is None:
            return jsonify(erro="parâmetros inválidos"), 400
        b, g, desde, ate, bucket = p
        return jsonify(bancada=b, grandeza=g, desde=desde, ate=ate, bucket_s=bucket,
                       pontos=est.banco.historico(b, g, desde, ate, bucket))

    @app.get("/api/historico.csv")
    def api_historico_csv():
        p = _historico_params()
        if p is None:
            return jsonify(erro="parâmetros inválidos"), 400
        b, g, desde, ate, bucket = p
        buf = io.StringIO()
        w = csv.writer(buf)
        w.writerow(["timestamp", "media", "minimo", "maximo", "n"])
        for r in est.banco.historico(b, g, desde, ate, bucket):
            w.writerow([int(r["t"]), round(r["media"], 3), r["min"], r["max"], r["n"]])
        return Response(buf.getvalue(), mimetype="text/csv",
                        headers={"Content-Disposition": f"attachment; filename={b}_{g}.csv"})

    @app.post("/api/cmd/led")
    def api_cmd_led():
        negado = exige_token()
        if negado:
            return negado
        d = request.get_json(silent=True) or {}
        b, v = str(d.get("bancada", "")), d.get("valor")
        if not RE_NOME.match(b) or v not in (0, 1) or isinstance(v, bool):
            return jsonify(erro="use {'bancada': 'bancada1', 'valor': 0 ou 1}"), 400
        if not est.ponte.publica_led(b, v):
            return jsonify(erro="broker indisponível"), 503
        est.banco.inserir_evento(time.time(), b, "comando", f"led={v}")
        socketio.emit("comando", {"bancada": b, "texto": f"led={v}", "ts": time.time()})
        return jsonify(ok=True)

    # ---------------- tempo real ----------------
    @socketio.on("connect")
    def ao_conectar():
        emit("estado", est.snapshot())          # só para quem acabou de conectar

    if iniciar_mqtt:
        est.ponte.iniciar()
    return app
```

</details>

**Roteiro de leitura:** `Estado.ao_dado` é o **coração** — cada mensagem válida é (1) gravada,
(2) empurrada aos navegadores e (3) de vez em quando dispara a purga. `snapshot()` é o que o
navegador recém-conectado recebe. As rotas `/api/*` são o REST. `create_app(cfg, iniciar_mqtt=False)`
existe para os **testes** criarem a aplicação sem broker.

| Rota | Método | Para quê | Exemplo |
|---|---|---|---|
| `/` · `/historico` | GET | as duas páginas | — |
| `/api/saude` | GET | liveness: versão, MQTT conectado, contagens | `curl localhost:5003/api/saude` |
| `/api/nos` | GET | estado de cada bancada | `curl localhost:5003/api/nos` |
| `/api/leituras` | GET | últimas N leituras de uma série | `?bancada=bancada1&grandeza=temp&n=50` |
| `/api/historico` | GET | série reamostrada (média/mín/máx) | `?bancada=bancada1&grandeza=temp&horas=24&pontos=300` |
| `/api/historico.csv` | GET | o mesmo em CSV | idem |
| `/api/cmd/led` | POST | comando (token opcional) | `{"bancada":"bancada1","valor":1}` |
| `socket.io: estado` | servidor→cliente | snapshot ao conectar | — |
| `socket.io: leitura/status/movimento/comando` | servidor→cliente | push ao vivo | — |

### D.6 O front-end

<details>
<summary><code>src/dashboard/templates/base.html</code> (25 linhas) — clique para abrir</summary>

```html
<!doctype html>
<html lang="pt-br">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{% block titulo %}Painel IoT{% endblock %}</title>
<link rel="stylesheet" href="{{ url_for('static', filename='style.css') }}">
</head>
<body>
<header class="topo">
  <div class="marca">📡 <strong>Painel IoT</strong> <span class="sub">Sistemas Embarcados · Ifes Guarapari</span></div>
  <nav>
    <a href="{{ url_for('pagina_inicio') }}" class="{% block nav_inicio %}{% endblock %}">Ao vivo</a>
    <a href="{{ url_for('pagina_historico') }}" class="{% block nav_hist %}{% endblock %}">Histórico</a>
  </nav>
  <div class="selos">
    <span id="selo-mqtt" class="selo off" title="Conexão do servidor com o broker MQTT">MQTT</span>
    <span id="selo-ws" class="selo off" title="Conexão deste navegador com o servidor (WebSocket)">WebSocket</span>
  </div>
</header>
<main>{% block conteudo %}{% endblock %}</main>
<footer class="rodape">v{{ versao }} · dados: broker MQTT → SQLite → este painel</footer>
{% block scripts %}{% endblock %}
</body>
</html>
```

</details>

<details>
<summary><code>src/dashboard/templates/index.html</code> (36 linhas) — clique para abrir</summary>

```html
{% extends "base.html" %}
{% block titulo %}Painel IoT — ao vivo{% endblock %}
{% block nav_inicio %}ativo{% endblock %}
{% block conteudo %}
<section>
  <h2>Nós</h2>
  <div id="nos" class="grade"><p class="vazio">Aguardando dados de <code>{{ base }}/&lt;bancada&gt;/…</code> no broker.</p></div>
</section>

<section>
  <div class="linha-titulo">
    <h2>Tempo real</h2>
    <label>Grandeza <select id="sel-grandeza"></select></label>
  </div>
  <div class="caixa-grafico"><canvas id="grafico"></canvas></div>
</section>

<section>
  <h2>Eventos recentes</h2>
  <ul id="eventos" class="eventos"></ul>
</section>

<dialog id="dlg-token">
  <form method="dialog">
    <p>Este painel exige um <strong>token</strong> para enviar comandos.</p>
    <input id="in-token" type="password" placeholder="token" autocomplete="off">
    <menu><button value="cancel">Cancelar</button><button id="ok-token" value="ok">Salvar</button></menu>
  </form>
</dialog>
<div id="toast" class="toast" role="status"></div>
{% endblock %}
{% block scripts %}
<script src="{{ url_for('static', filename='vendor/socket.io.min.js') }}"></script>
<script src="{{ url_for('static', filename='vendor/chart.umd.js') }}"></script>
<script src="{{ url_for('static', filename='app.js') }}"></script>
{% endblock %}
```

</details>

<details>
<summary><code>src/dashboard/static/app.js</code> (150 linhas) — clique para abrir</summary>

```javascript
/* Painel ao vivo: Socket.IO + Chart.js, sem build, sem framework. */
"use strict";
const UNIDADES = {temp: "°C", umid: "%", dist: "cm", tensao: "V", corrente: "A", potencia: "W", luz: "lux"};
const CORES = ["#1f6fb2", "#d2691e", "#2e7d32", "#b03030", "#6a3d9a", "#00838f"];
const MAX_PONTOS = 120, VELHO_S = 30;

const estado = {nos: {}, series: {}, grandeza: null};
const $ = (id) => document.getElementById(id);
const fmtHora = (ts) => new Date(ts * 1000).toLocaleTimeString("pt-BR");

/* ---------------- tempo real ---------------- */
const sock = io();            // mesma origem: passa pelo nginx em produção
sock.on("connect", () => selo("selo-ws", true));
sock.on("disconnect", () => selo("selo-ws", false));
function selo(id, ok) { $(id).className = "selo " + (ok ? "on" : "off"); }

sock.on("estado", (s) => {    // fotografia inicial, enviada só a este navegador
  estado.nos = s.nos; estado.series = s.series;
  selo("selo-mqtt", s.mqtt);
  desenhaNos(); desenhaEventos(s.eventos); preencheGrandezas(); montaGrafico();
});
sock.on("leitura", (d) => {
  const no = garanteNo(d.bancada);
  no.ultimo[d.grandeza] = {valor: d.valor, ts: d.ts};
  const por = (estado.series[d.grandeza] ||= {});
  const s = (por[d.bancada] ||= []);
  s.push([d.ts, d.valor]); if (s.length > MAX_PONTOS) s.shift();
  selo("selo-mqtt", true);
  desenhaNos(); preencheGrandezas();
  if (d.grandeza === estado.grandeza) atualizaGrafico();
});
sock.on("status", (d) => {
  const no = garanteNo(d.bancada); no.status = d.texto; no.status_ts = d.ts;
  desenhaNos(); eventoNaLista({ts: d.ts, bancada: d.bancada, tipo: "status", texto: d.texto});
});
sock.on("movimento", (d) => {
  garanteNo(d.bancada).movimento = {classe: d.classe, conf: d.conf, ts: d.ts};
  desenhaNos(); eventoNaLista({ts: d.ts, bancada: d.bancada, tipo: "movimento", texto: d.classe, extra: d.conf});
});
sock.on("comando", (d) => eventoNaLista({ts: d.ts, bancada: d.bancada, tipo: "comando", texto: d.texto}));

function garanteNo(b) {
  return (estado.nos[b] ||= {status: "desconhecido", ultimo: {}, movimento: null});
}

/* ---------------- cartões dos nós ---------------- */
function desenhaNos() {
  const nomes = Object.keys(estado.nos).sort();
  const raiz = $("nos");
  if (!nomes.length) return;
  raiz.innerHTML = "";
  for (const b of nomes) {
    const n = estado.nos[b], el = document.createElement("div");
    el.className = "no";
    const valores = Object.keys(n.ultimo).sort().map((g) => {
      const u = n.ultimo[g], idade = Date.now() / 1000 - u.ts;
      return `<div class="valor ${idade > VELHO_S ? "velho" : ""}"><span class="n">${u.valor.toFixed(1)}</span>` +
             `<span class="u">${UNIDADES[g] || g}</span><span class="r">${g} · há ${Math.round(idade)} s</span></div>`;
    }).join("");
    const mov = n.movimento
      ? `<div class="mov">movimento: <span class="chip">${esc(n.movimento.classe)}</span> ${Math.round(n.movimento.conf * 100)}%</div>` : "";
    el.innerHTML =
      `<div class="no-topo"><h3>${esc(b)}</h3><span class="pilula ${esc(n.status)}">${esc(n.status)}</span></div>` +
      `<div class="valores">${valores || '<span class="vazio">sem leituras</span>'}</div>${mov}` +
      `<div class="acoes"><button class="liga" data-b="${esc(b)}" data-v="1">LED on</button>` +
      `<button data-b="${esc(b)}" data-v="0">LED off</button></div>`;
    raiz.appendChild(el);
  }
}
$("nos").addEventListener("click", (ev) => {
  const bt = ev.target.closest("button[data-b]");
  if (bt) comanda(bt.dataset.b, Number(bt.dataset.v));
});
setInterval(() => { if (Object.keys(estado.nos).length) desenhaNos(); }, 1000);   // atualiza o "há X s"

function esc(s) { return String(s).replace(/[&<>"']/g, (c) => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"}[c])); }

/* ---------------- comandos (REST + token opcional) ---------------- */
async function comanda(bancada, valor) {
  const token = sessionStorage.getItem("dash_token") || "";
  const r = await fetch("/api/cmd/led", {
    method: "POST",
    headers: {"Content-Type": "application/json", ...(token ? {Authorization: "Bearer " + token} : {})},
    body: JSON.stringify({bancada, valor}),
  });
  if (r.status === 401) { pedeToken(); return; }
  toast(r.ok ? `LED ${valor ? "ligado" : "desligado"} em ${bancada}` : "falha: " + ((await r.json()).erro || r.status));
}
function pedeToken() {
  const dlg = $("dlg-token");
  dlg.showModal();
  dlg.addEventListener("close", () => {
    if (dlg.returnValue === "ok") sessionStorage.setItem("dash_token", $("in-token").value);
  }, {once: true});
}
function toast(t) {
  const el = $("toast"); el.textContent = t; el.classList.add("vis");
  setTimeout(() => el.classList.remove("vis"), 2500);
}

/* ---------------- eventos ---------------- */
function desenhaEventos(lista) { $("eventos").innerHTML = ""; [...lista].reverse().forEach(eventoNaLista); }
function eventoNaLista(e) {
  const ul = $("eventos"), li = document.createElement("li");
  const extra = e.tipo === "movimento" && e.extra != null ? ` (${Math.round(e.extra * 100)}%)` : "";
  li.innerHTML = `<span class="h">${fmtHora(e.ts)}</span><span><strong>${esc(e.bancada)}</strong> · ${esc(e.tipo)}: ${esc(e.texto)}${extra}</span>`;
  ul.prepend(li); while (ul.children.length > 20) ul.lastChild.remove();
}

/* ---------------- gráfico ---------------- */
let grafico = null;
function preencheGrandezas() {
  const sel = $("sel-grandeza"), gs = Object.keys(estado.series).sort();
  if ([...sel.options].map((o) => o.value).join() === gs.join()) return;
  sel.innerHTML = gs.map((g) => `<option>${esc(g)}</option>`).join("");
  if (!estado.grandeza || !gs.includes(estado.grandeza)) estado.grandeza = gs.includes("temp") ? "temp" : gs[0];
  sel.value = estado.grandeza; montaGrafico();
}
$("sel-grandeza").addEventListener("change", (e) => { estado.grandeza = e.target.value; montaGrafico(); });

function datasets() {
  const por = estado.series[estado.grandeza] || {};
  return Object.keys(por).sort().map((b, i) => ({
    label: b, data: por[b].map(([t, v]) => ({x: t, y: v})),
    borderColor: CORES[i % CORES.length], backgroundColor: CORES[i % CORES.length],
    borderWidth: 2, pointRadius: 0, tension: 0.25,
  }));
}
function montaGrafico() {
  if (!estado.grandeza) return;
  if (grafico) grafico.destroy();
  grafico = new Chart($("grafico"), {
    type: "line", data: {datasets: datasets()},
    options: {
      animation: false, responsive: true, maintainAspectRatio: false, parsing: false,
      scales: {
        x: {type: "linear", ticks: {maxTicksLimit: 6, callback: (v) => fmtHora(v)}},
        y: {title: {display: true, text: `${estado.grandeza} (${UNIDADES[estado.grandeza] || "—"})`}},
      },
      plugins: {legend: {position: "bottom"}, tooltip: {callbacks: {title: (it) => fmtHora(it[0].parsed.x)}}},
    },
  });
}
function atualizaGrafico() {
  if (!grafico) return montaGrafico();
  const novos = datasets();
  if (novos.length !== grafico.data.datasets.length) return montaGrafico();
  novos.forEach((d, i) => (grafico.data.datasets[i].data = d.data));
  grafico.update("none");
}
```

</details>

<details>
<summary><code>src/dashboard/templates/historico.html</code> (25 linhas) — clique para abrir</summary>

```html
{% extends "base.html" %}
{% block titulo %}Painel IoT — histórico{% endblock %}
{% block nav_hist %}ativo{% endblock %}
{% block conteudo %}
<section>
  <h2>Histórico (SQLite)</h2>
  <form id="filtros" class="filtros">
    <label>Bancada <select id="f-bancada"></select></label>
    <label>Grandeza <select id="f-grandeza"></select></label>
    <label>Período
      <select id="f-horas">
        <option value="0.25">15 min</option><option value="1" selected>1 hora</option>
        <option value="6">6 horas</option><option value="24">24 horas</option>
        <option value="168">7 dias</option>
      </select></label>
    <a id="baixar" class="botao" href="#">Baixar CSV</a>
  </form>
  <div class="caixa-grafico"><canvas id="grafico-hist"></canvas></div>
  <p id="resumo" class="resumo"></p>
</section>
{% endblock %}
{% block scripts %}
<script src="{{ url_for('static', filename='vendor/chart.umd.js') }}"></script>
<script src="{{ url_for('static', filename='historico.js') }}"></script>
{% endblock %}
```

</details>

<details>
<summary><code>src/dashboard/static/historico.js</code> (54 linhas) — clique para abrir</summary>

```javascript
/* Página de histórico: consulta /api/historico (SQLite reamostrado no servidor). */
"use strict";
const $ = (id) => document.getElementById(id);
const fmt = (ts, horas) => new Date(ts * 1000).toLocaleString("pt-BR",
  horas > 24 ? {day: "2-digit", month: "2-digit", hour: "2-digit", minute: "2-digit"} : {hour: "2-digit", minute: "2-digit"});
let nos = {}, grafico = null;

async function inicia() {
  nos = await (await fetch("/api/nos")).json();
  const bs = Object.keys(nos).sort();
  $("f-bancada").innerHTML = bs.map((b) => `<option>${b}</option>`).join("");
  atualizaGrandezas(); carrega();
}
function atualizaGrandezas() {
  const gs = (nos[$("f-bancada").value] || {grandezas: []}).grandezas;
  $("f-grandeza").innerHTML = gs.map((g) => `<option>${g}</option>`).join("");
}
function params() {
  return new URLSearchParams({bancada: $("f-bancada").value, grandeza: $("f-grandeza").value,
                              horas: $("f-horas").value, pontos: 300});
}
async function carrega() {
  if (!$("f-bancada").value || !$("f-grandeza").value) { $("resumo").textContent = "Sem dados no banco ainda."; return; }
  const p = params(), horas = Number($("f-horas").value);
  $("baixar").href = "/api/historico.csv?" + p;
  const r = await (await fetch("/api/historico?" + p)).json();
  const pts = r.pontos;
  if (grafico) grafico.destroy();
  const base = {pointRadius: 0, borderWidth: 2, tension: 0.2, parsing: false};
  grafico = new Chart($("grafico-hist"), {
    type: "line",
    data: {datasets: [
      {...base, label: "máximo", data: pts.map((q) => ({x: q.t, y: q.max})), borderColor: "#d2691e66", borderDash: [4, 3], borderWidth: 1},
      {...base, label: "média", data: pts.map((q) => ({x: q.t, y: q.media})), borderColor: "#1f6fb2"},
      {...base, label: "mínimo", data: pts.map((q) => ({x: q.t, y: q.min})), borderColor: "#2e7d3266", borderDash: [4, 3], borderWidth: 1},
    ]},
    options: {animation: false, responsive: true, maintainAspectRatio: false,
      scales: {x: {type: "linear", ticks: {maxTicksLimit: 8, callback: (v) => fmt(v, horas)}}},
      plugins: {legend: {position: "bottom"}, tooltip: {callbacks: {title: (it) => fmt(it[0].parsed.x, horas)}}}},
  });
  if (!pts.length) { $("resumo").textContent = "Nenhuma leitura neste período."; return; }
  const n = pts.reduce((a, q) => a + q.n, 0);
  const med = pts.reduce((a, q) => a + q.media * q.n, 0) / n;
  $("resumo").textContent = `${n} leituras em ${pts.length} pontos (balde de ${r.bucket_s} s) · ` +
    `média ${med.toFixed(2)} · mín ${Math.min(...pts.map((q) => q.min)).toFixed(2)} · máx ${Math.max(...pts.map((q) => q.max)).toFixed(2)}`;
}
$("f-bancada").addEventListener("change", () => { atualizaGrandezas(); carrega(); });
$("f-grandeza").addEventListener("change", carrega);
$("f-horas").addEventListener("change", carrega);
(async () => {   // selo do MQTT; esta página não usa WebSocket, então o outro selo some
  const ws = $("selo-ws"); if (ws) ws.remove();
  try { const s = await (await fetch("/api/saude")).json(); $("selo-mqtt").className = "selo " + (s.mqtt ? "on" : "off"); } catch (e) {}
})();
inicia();
```

</details>

O `style.css` (55 linhas) usa variáveis CSS e `prefers-color-scheme` para o tema escuro; as
bibliotecas Chart.js e socket.io-client estão **copiadas** em `static/vendor/` (sem CDN: funciona
sem internet).

### D.7 Rodando o painel completo

```bash
cd ~/sis-emb-2026-2/labs-extra/dashboard-iot/src
python3 tools/popular_banco.py --db dashboard.db --horas 24 --periodo 10 --bancadas bancada1 bancada2
MQTT_HOST=localhost python3 -m dashboard          # porta 5003
```

`popular_banco.py` cria **24 h de histórico sintético** — assim a página Histórico não nasce
vazia (e esses dados são de mentira: **não** os apresente como medições). Abra
`http://<ip-do-rpi>:5003/` (ao vivo) e `/historico`.

![Painel ao vivo: cartões, gráfico em tempo real e eventos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_tela_home.png)
*Figura L-4 — O que você deve ver (dados simulados).*

![Página Histórico](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_tela_hist.png)
*Figura L-5 — Histórico: média, faixa mín–máx e botão "Baixar CSV".*

![Painel no celular, uma coluna](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_tela_mobile.png)
*Figura L-6 — O mesmo painel num celular (390 px de largura): cartões empilhados, gráfico fluido.*

### D.8 Os cinco testes de aceitação da Parte D

| # | Teste | Como fazer | Resultado esperado |
|---|---|---|---|
| 1 | **Offline por LWT** | `python3 tools/simulador_no.py --bancadas bancada9 --morrer-apos 20` | cartão `bancada9` aparece ONLINE e, ≈ 7,5 s após o processo morrer, vira OFFLINE |
| 2 | **Persistência** | pare o painel (Ctrl+C), suba de novo | o histórico e o gráfico inicial continuam lá |
| 3 | **Comando** | `mosquitto_sub -t 'ifes/+/cmd/#' -v` e clique em "LED on" | `ifes/bancada1/cmd/led 1` aparece; um evento "comando" entra na lista |
| 4 | **Token** | `DASH_TOKEN=segredo MQTT_HOST=localhost python3 -m dashboard`; `curl` sem e com `-H "Authorization: Bearer segredo"` | sem token **401**; com token **200** |
| 5 | **Entrada hostil** | `mosquitto_pub -t ifes/bancada1/temp -m nan` e `-m '<script>'` e `-t ifes/bancada1/movimento -m '{"classe":"<b>x","conf":2}'` | nada quebra, nada entra no banco nem na tela |

Exemplo do teste 4 (`valor` 0 ou 1, JSON):

```bash
curl -s -o /dev/null -w "%{http_code}\n" -X POST -H "Content-Type: application/json" \
     -d '{"bancada":"bancada1","valor":1}' localhost:5003/api/cmd/led                       # 401
curl -s -o /dev/null -w "%{http_code}\n" -X POST -H "Content-Type: application/json" \
     -H "Authorization: Bearer segredo" -d '{"bancada":"bancada1","valor":1}' localhost:5003/api/cmd/led   # 200
```

*Se o navegador pedir o token ao clicar nos botões, é o esperado: ele guarda o valor em
`sessionStorage` (some ao fechar a aba).*

### D.9 Desafios de implementação (escolha pelo menos um)

1. **Nova grandeza** `luz`: publique `mosquitto_pub -t ifes/bancada1/luz -m 400` e veja o painel
   listá-la **sem mudar o Python**. Explique por quê (*`interpreta` aceita qualquer grandeza do
   contrato; o seletor é montado do que existe nos dados*).
2. **Alarme com histerese**: ao passar de 30 °C, grave um evento `alarme` e destaque o cartão;
   só normalize abaixo de 29 °C. Escreva o teste **antes**.
3. **Segundo atuador** `cmd/rele`: generalize `publica_led` → `publica_cmd(bancada, atuador, valor)`.
4. **Exportar CSV de eventos**: nova rota `/api/eventos.csv`.
5. **Limite de taxa** em `POST /api/cmd/led` (no máximo 5 comandos/s por IP) — com teste.

## Parte E — Medições (aula 5)

**Objetivo:** trocar "acho que" por números **seus**.

**E.1 — Tráfego: polling × WebSocket.** Com o painel (Parte D) e o simulador rodando:

```bash
cd src && python3 tools/medir_trafego.py --alvo 127.0.0.1:5003 --segundos 60 --json meu_trafego.json
```

O script põe um proxy contador de bytes entre cliente e servidor. **Antes** de rodar, escreva a sua
previsão no caderno. Resultado de referência medido pelo autor (3 bancadas, 1 Hz, 60 s; loopback;
só bytes de aplicação):

| | requisições/mensagens | bytes/s |
|---|---|---|
| Polling `GET /api/nos` a 2 Hz | 112 | **2.062** |
| WebSocket | 360 | **707** |

Razão ≈ 2,9×. O **seu** resultado deve ser da mesma ordem de grandeza, mas não idêntico —
discuta a diferença (nº de bancadas, período do simulador, versões).

![Polling 2.062 B/s contra WebSocket 707 B/s](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_trafego.png)
*Figura L-7 — Medição de referência do autor.*

**E.2 — Disco.** Meça o seu banco: `ls -l dashboard.db*` e `sqlite3 dashboard.db "select count(*) from leituras"`;
divida e obtenha **bytes por linha**. Refaça a conta do Exemplo D.2 da teoria para o **seu**
projeto (quantas bancadas, quantas grandezas, qual período) e decida `RETENCAO_DIAS`.

**E.3 — Downsampling.** Peça a mesma janela de 24 h com 300 pontos e com 2000 pontos e compare
o tamanho da resposta e o tempo:

```bash
curl -s -o /tmp/a.json -w "%{size_download} B  %{time_total} s\n" \
  "localhost:5003/api/historico?bancada=bancada1&grandeza=temp&horas=24&pontos=300"
curl -s -o /tmp/b.json -w "%{size_download} B  %{time_total} s\n" \
  "localhost:5003/api/historico?bancada=bancada1&grandeza=temp&horas=24&pontos=2000"
```

![24 h de dados: 9.092 pontos brutos contra 289 pontos de balde de 5 min](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_downsampling.png)
*Figura L-8 — Referência (dados sintéticos): 31× menos pontos, mesma forma, e a faixa mín–máx
preserva os picos.*

✅ **Entregável da Parte E:** a tabela do E.1 com os **seus** números, a conta de disco do E.2 e
as duas linhas do E.3.

## Parte F — Produção: nginx + gunicorn + systemd (aula 6)

**Objetivo:** o painel sobe sozinho no boot, se recupera de falhas e é acessado em
`http://<ip-do-rpi>/`, sem número de porta. Livro: **Dalmaris, caps. 40–46 (p. 130–147; Figura
40.51, p. 131)** — o nosso servidor de aplicação é o gunicorn em vez do uWSGI (justificativa na
[trilha](../../docs/trilha-fullstack.md#etapa-5--pilha-de-produção-nginx--gunicorn--flask--sqlite--systemd)).

![nginx, gunicorn, Flask, SQLite e Mosquitto sob o systemd](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_producao.png)
*Figura L-9 — A pilha que você vai montar.*

### F.1 Arquivos de implantação

<details>
<summary><code>src/deploy/dashboard.service</code> (18 linhas) — clique para abrir</summary>

```ini
# Modelo: instalar.sh troca @USUARIO@, @SRC@ e @VENV@ pelos valores reais.
[Unit]
Description=Painel IoT (Flask + Socket.IO + MQTT) - Sistemas Embarcados
After=network-online.target mosquitto.service
Wants=network-online.target

[Service]
User=@USUARIO@
WorkingDirectory=@SRC@
EnvironmentFile=/etc/default/dashboard
# UM worker (-w 1): um cliente MQTT, uma sessão Socket.IO. Paralelismo = threads.
ExecStart=@VENV@/bin/gunicorn -k gthread -w 1 --threads 100 \
          -b 127.0.0.1:8000 --access-logfile - dashboard.wsgi:app
Restart=on-failure
RestartSec=3

[Install]
WantedBy=multi-user.target
```

</details>

<details>
<summary><code>src/deploy/nginx-dashboard.conf</code> (26 linhas) — clique para abrir</summary>

```nginx
# /etc/nginx/sites-available/dashboard  (ln -s para sites-enabled; remova o "default")
map $http_upgrade $connection_upgrade { default upgrade; '' close; }

server {
    listen 80;
    server_name _;

    # (produção) autenticação básica:  auth_basic "Painel"; auth_basic_user_file /etc/nginx/.htpasswd;

    location / {
        proxy_pass http://127.0.0.1:8000;
        proxy_set_header Host $http_host;   # com porta: o Socket.IO compara com o Origin
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }

    # WebSocket: sem estes cabeçalhos o navegador cai em polling longo ou falha
    location /socket.io/ {
        proxy_pass http://127.0.0.1:8000/socket.io/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;
        proxy_set_header Host $http_host;   # com porta: o Socket.IO compara com o Origin
        proxy_read_timeout 3600s;     # conexão longa: não cortar após 60 s
    }
}
```

</details>

<details>
<summary><code>src/deploy/dashboard.env</code> (10 linhas) — clique para abrir</summary>

```bash
# copie para /etc/default/dashboard  (sudo cp dashboard.env /etc/default/dashboard; sudo chmod 640 ...)
MQTT_HOST=localhost
MQTT_PORT=1883
MQTT_USER=
MQTT_PASS=
TOPICO_BASE=ifes
DB_PATH=@SRC@/../dashboard.db
# token exigido em POST /api/cmd/led (deixe vazio no laboratório; em produção: openssl rand -hex 16)
DASH_TOKEN=
RETENCAO_DIAS=7
```

</details>

<details>
<summary><code>src/deploy/mosquitto-lab.conf</code> (4 linhas) — clique para abrir</summary>

```ini
# Laboratório: aberto, apenas rede local. NÃO use em rede exposta.
listener 1883
allow_anonymous true
persistence true
```

</details>

<details>
<summary><code>src/deploy/mosquitto-seguro.conf</code> (6 linhas) — clique para abrir</summary>

```ini
# Produção mínima: usuário/senha. Crie com: sudo mosquitto_passwd -c /etc/mosquitto/passwd painel
listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
persistence true
# TLS (8883): acrescente listener 8883 + cafile/certfile/keyfile (veja guia de problemas)
```

</details>

<details>
<summary><code>src/deploy/instalar.sh</code> (22 linhas) — clique para abrir</summary>

```bash
#!/usr/bin/env bash
# Instala o painel no Raspberry Pi OS. Execute como SEU usuário (não root), de qualquer pasta:
#   bash deploy/instalar.sh     (a partir de .../dashboard-iot/src)
set -euo pipefail
sudo apt-get update
sudo apt-get install -y python3-venv nginx mosquitto mosquitto-clients
cd "$(dirname "$0")/.."
SRC="$(pwd)"; VENV="$SRC/../venv"; USUARIO="$(id -un)"
python3 -m venv "$VENV"
"$VENV/bin/pip" install -r requirements.txt
sudo cp deploy/mosquitto-lab.conf /etc/mosquitto/conf.d/lab.conf
sudo systemctl restart mosquitto
[ -f /etc/default/dashboard ] || sed "s|@SRC@|$SRC|" deploy/dashboard.env | sudo tee /etc/default/dashboard >/dev/null
sed -e "s|@USUARIO@|$USUARIO|" -e "s|@SRC@|$SRC|" -e "s|@VENV@|$VENV|" deploy/dashboard.service | sudo tee /etc/systemd/system/dashboard.service >/dev/null
sudo cp deploy/nginx-dashboard.conf /etc/nginx/sites-available/dashboard
sudo ln -sf /etc/nginx/sites-available/dashboard /etc/nginx/sites-enabled/dashboard
sudo rm -f /etc/nginx/sites-enabled/default
sudo nginx -t
sudo systemctl daemon-reload
sudo systemctl enable --now dashboard
sudo systemctl restart nginx
echo "Pronto: abra http://$(hostname -I | awk '{print $1}')/"
```

</details>

### F.2 Passo a passo manual (faça **uma vez** assim, para entender)

1. **Teste o gunicorn à mão**, sem systemd. Note `-w 1` (teoria §3.2) e o `-b 127.0.0.1:8000`:

   ```bash
   cd ~/sis-emb-2026-2/labs-extra/dashboard-iot/src
   MQTT_HOST=localhost ../venv/bin/gunicorn -k gthread -w 1 --threads 100 -b 127.0.0.1:8000 dashboard.wsgi:app
   curl -s localhost:8000/api/saude          # {"mqtt":true,"status":"ok",…}
   ```

2. **Instale e valide o nginx.** Troque o site padrão pelo nosso e **valide antes de recarregar**:

   ```bash
   sudo apt install -y nginx
   sudo cp deploy/nginx-dashboard.conf /etc/nginx/sites-available/dashboard
   sudo ln -sf /etc/nginx/sites-available/dashboard /etc/nginx/sites-enabled/dashboard
   sudo rm -f /etc/nginx/sites-enabled/default
   sudo nginx -t && sudo systemctl reload nginx     # "syntax is ok" e "test is successful"
   ```

   Abra `http://<ip-do-rpi>/` — mesmo painel, **sem `:5003`**. No navegador (F12 → Network) a
   conexão `socket.io` deve mostrar **101** (WebSocket). Se aparecer só `polling`, falta
   `Upgrade/Connection` no `location /socket.io/`.

3. **Crie o serviço.** Pare o gunicorn manual (Ctrl+C), preencha os `@…@` do modelo e ative:

   ```bash
   SRC=$(pwd); VENV=$(realpath $SRC/../venv)
   sed "s|@SRC@|$SRC|" deploy/dashboard.env | sudo tee /etc/default/dashboard
   sed -e "s|@USUARIO@|$USER|" -e "s|@SRC@|$SRC|" -e "s|@VENV@|$VENV|" deploy/dashboard.service \
       | sudo tee /etc/systemd/system/dashboard.service
   sudo systemctl daemon-reload
   sudo systemctl enable --now dashboard
   systemctl status dashboard                       # active (running)
   journalctl -u dashboard -f                       # logs em tempo real (Ctrl+C sai)
   ```

   *(`deploy/instalar.sh` faz tudo isto de uma vez — rode-o numa 2ª vez, num cartão limpo, **depois**
   de ter entendido cada passo.)*

### F.3 Os quatro testes de produção

| # | Teste | Comando | Esperado |
|---|---|---|---|
| 1 | Está no ar? | `curl -s localhost/api/saude` | JSON `status: ok` (pela porta 80!) |
| 2 | Se cair, volta? | `sudo pkill -9 -f gunicorn; sleep 5; systemctl status dashboard` | `active (running)` de novo (Restart em ~3 s) |
| 3 | Sobrevive a reboot? | `sudo reboot`; espere; abra `http://<ip>/` | painel no ar **sem** nenhum comando seu |
| 4 | A porta interna está fechada? | de **outro** PC: `curl -m 3 http://<ip>:8000/` | falha (só o nginx responde de fora) |

**Segurança de produção (nota — descreva no relatório, ligue o que puder):**
1. *Painel:* descomente `auth_basic` no `nginx-dashboard.conf` (`sudo apt install apache2-utils`;
   `sudo htpasswd -c /etc/nginx/.htpasswd aluno`) e defina `DASH_TOKEN` em `/etc/default/dashboard`.
2. *Broker:* use `mosquitto-seguro.conf` (`mosquitto_passwd`); ponha `MQTT_USER`/`MQTT_PASS` no
   arquivo de ambiente **e nos firmwares**.
3. *Canal:* HTTPS e MQTT/TLS — com certificado autoassinado no laboratório, Let's Encrypt fora dele.
   Sem isso, senha e token trafegam em **texto puro**: diga isso no relatório.

✅ **Chegamos!** — os quatro testes passam e há `systemctl status` + um print do navegador em
`http://<ip>/` no relatório.

## Parte G — Demonstração e relatório (aula 6)

**Demonstração de 5 minutos** (roteiro): (1) nós ESP32 (ou simulador) publicando — mostre o
`mosquitto_sub`; (2) abra o painel em **duas** telas, uma delas o celular; (3) clique **LED on** e
mostre o atuador/`mosquitto_sub`; (4) **desligue um nó** e mostre o OFFLINE; (5) abra o
**Histórico** e baixe o CSV; (6) `sudo reboot` (ou mostre o `systemctl status` com o *uptime*);
(7) cite **uma limitação** que você conhece.

**Checklist final:**

- [ ] `pytest` verde (print) · [ ] teste do TDD da D.2 commitado · [ ] medições da Parte E com **seus** números
- [ ] `systemctl status` e reboot (Parte F) · [ ] segurança: o que ligou e o que ficou como nota
- [ ] relatório no [modelo](relatorio-modelo.md), com **limitações honestas**

**Erros que custam ponto:** dados sintéticos apresentados como medição; "funciona" sem print;
copiar o código de referência sem conseguir explicar `-w 1`, o `on_connect` ou o `Host $http_host`.

## Resumo dos comandos

```bash
# Parte 0
python3 -m venv venv && source venv/bin/activate && pip install -r src/requirements.txt
python3 tools/simulador_no.py --bancadas bancada1 bancada2 --periodo 1 --movimento
mosquitto_sub -h localhost -t 'ifes/#' -v
python3 -m pytest tests -q
# Partes A–C
python3 etapa1_rest/app.py  ·  python3 etapa2_websocket/app.py  ·  MQTT_HOST=localhost python3 etapa3_mqtt_web/app.py
# Parte D–E
python3 tools/popular_banco.py --db dashboard.db --horas 24 --periodo 10 --bancadas bancada1 bancada2
MQTT_HOST=localhost python3 -m dashboard
python3 tools/medir_trafego.py --alvo 127.0.0.1:5003 --segundos 60
# Parte F
sudo nginx -t && sudo systemctl reload nginx ; sudo systemctl enable --now dashboard ; journalctl -u dashboard -f
```
