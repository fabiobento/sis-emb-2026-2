"""Ponta a ponta com um Mosquitto REAL em porta alta (pula se não houver mosquitto instalado)."""
import json
import shutil
import socket
import subprocess
import time

import paho.mqtt.client as mqtt
import pytest

from dashboard.app import create_app
from dashboard.config import Config

pytestmark = pytest.mark.skipif(shutil.which("mosquitto") is None, reason="mosquitto não instalado")


def porta_livre():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p


@pytest.fixture
def broker(tmp_path):
    porta = porta_livre()
    conf = tmp_path / "m.conf"
    conf.write_text(f"listener {porta} 127.0.0.1\nallow_anonymous true\npersistence false\n")
    p = subprocess.Popen(["mosquitto", "-c", str(conf)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(50):
        try:
            socket.create_connection(("127.0.0.1", porta), timeout=0.2).close(); break
        except OSError:
            time.sleep(0.1)
    yield porta
    p.terminate(); p.wait()


def espera(cond, t=5):
    fim = time.time() + t
    while time.time() < fim:
        if cond():
            return True
        time.sleep(0.05)
    return False


def pub(porta):
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"teste-{time.time()}")
    c.connect("127.0.0.1", porta); c.loop_start(); return c


def test_mqtt_para_banco_e_comando_de_volta(broker, tmp_path):
    class C(Config):
        MQTT_HOST = "127.0.0.1"; MQTT_PORT = broker; DB_PATH = str(tmp_path / "e2e.db")
    app = create_app(C, iniciar_mqtt=True)
    est = app.extensions["dash"]
    assert espera(lambda: est.ponte.conectado), "dashboard não conectou ao broker"

    p = pub(broker)
    p.publish("ifes/bancada1/temp", "27.5").wait_for_publish()
    p.publish("ifes/bancada1/umid", "61").wait_for_publish()
    p.publish("ifes/bancada1/status", "online", retain=False).wait_for_publish()
    p.publish("ifes/bancada1/movimento", json.dumps({"classe": "aceno", "conf": 0.9})).wait_for_publish()
    p.publish("ifes/bancada1/temp", "lixo").wait_for_publish()          # deve ser ignorado
    assert espera(lambda: est.banco.contagens() == {"leituras": 2, "eventos": 2})   # temp, umid, status, movimento
    nos = app.test_client().get("/api/nos").json
    assert nos["bancada1"]["ultimo"]["temp"]["valor"] == 27.5
    assert nos["bancada1"]["status"] == "online" and nos["bancada1"]["movimento"]["classe"] == "aceno"

    # comando: navegador -> API -> MQTT -> "nó" (assinante)
    recebido = []
    nodo = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="no-teste")
    nodo.on_message = lambda c, u, m: recebido.append((m.topic, m.payload.decode()))
    nodo.connect("127.0.0.1", broker); nodo.subscribe("ifes/bancada1/cmd/led", qos=1); nodo.loop_start()
    time.sleep(0.3)
    r = app.test_client().post("/api/cmd/led", json={"bancada": "bancada1", "valor": 1})
    assert r.status_code == 200
    assert espera(lambda: ("ifes/bancada1/cmd/led", "1") in recebido)
    est.ponte.parar(); p.loop_stop(); nodo.loop_stop()


def test_lwt_vira_offline(broker, tmp_path):
    class C(Config):
        MQTT_HOST = "127.0.0.1"; MQTT_PORT = broker; DB_PATH = str(tmp_path / "lwt.db")
    app = create_app(C, iniciar_mqtt=True)
    est = app.extensions["dash"]
    assert espera(lambda: est.ponte.conectado)
    no = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="no-morredor")
    no.will_set("ifes/bancada9/status", "offline", qos=1, retain=True)
    no.connect("127.0.0.1", broker, keepalive=2); no.loop_start()
    no.publish("ifes/bancada9/status", "online", qos=1, retain=True).wait_for_publish()
    assert espera(lambda: app.test_client().get("/api/nos").json.get("bancada9", {}).get("status") == "online")
    no._sock.close()                     # morte abrupta: sem DISCONNECT
    no.loop_stop()
    assert espera(lambda: app.test_client().get("/api/nos").json["bancada9"]["status"] == "offline", t=8)
    est.ponte.parar()
