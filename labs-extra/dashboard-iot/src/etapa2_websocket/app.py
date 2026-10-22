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
