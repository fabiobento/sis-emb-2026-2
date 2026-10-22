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
