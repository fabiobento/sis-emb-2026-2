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
