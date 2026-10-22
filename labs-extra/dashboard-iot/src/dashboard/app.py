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
