#!/usr/bin/env python3
"""Simulador de nós ESP32 — publica no broker como o firmware da semana 14 (e o da trilha TinyML).

Serve para desenvolver e testar o dashboard SEM hardware e para demonstrar o LWT.
Cada nó simulado: LWT em ifes/<b>/status, `online` retido ao conectar, temp e umid a cada
--periodo s, comando ifes/<b>/cmd/led assinado, e (opcional) classes de movimento.

Uso:
  python3 simulador_no.py                                 # bancada1, bancada2; temp/umid a cada 2 s
  python3 simulador_no.py --bancadas bancada1 --movimento # + ifes/bancada1/movimento
  python3 simulador_no.py --morrer-apos 30                # mata o processo SEM despedida: veja o LWT
"""
import argparse
import json
import os
import random
import threading
import time

import paho.mqtt.client as mqtt


def no(bancada, a):
    base = f"{a.base}/{bancada}"
    cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"sim-{bancada}-{os.getpid()}")
    cli.will_set(f"{base}/status", "offline", qos=1, retain=True)       # o "testamento"

    def on_connect(c, ud, fl, rc, props=None):
        c.publish(f"{base}/status", "online", qos=1, retain=True)
        c.subscribe(f"{base}/cmd/led", qos=1)

    def on_message(c, ud, msg):
        print(f"[{bancada}] comando recebido: {msg.topic} = {msg.payload.decode()}")

    cli.on_connect, cli.on_message = on_connect, on_message
    cli.connect(a.broker, a.porta, keepalive=5)
    cli.loop_start()
    t, u = random.uniform(26, 27), random.uniform(60, 64)
    classes = ["parado", "aceno", "sobe_desce", "circulo"]
    prox_mov = time.time() + 5
    while True:
        t += random.uniform(-0.25, 0.25) + (26.5 - t) * 0.02          # passeio aleatório com retorno à média
        u += random.uniform(-0.6, 0.6) + (62 - u) * 0.02
        cli.publish(f"{base}/temp", f"{t:.1f}")
        cli.publish(f"{base}/umid", f"{u:.0f}")
        if a.movimento and time.time() >= prox_mov:
            c = random.choice(classes)
            cli.publish(f"{base}/movimento", json.dumps({"classe": c, "conf": round(random.uniform(0.82, 0.99), 2),
                                                        "t": int(time.time() * 1000) % 10**9}))
            prox_mov = time.time() + random.uniform(4, 9)
        time.sleep(a.periodo)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--broker", default="localhost")
    ap.add_argument("--porta", type=int, default=1883)
    ap.add_argument("--base", default="ifes")
    ap.add_argument("--bancadas", nargs="+", default=["bancada1", "bancada2"])
    ap.add_argument("--periodo", type=float, default=2.0)
    ap.add_argument("--movimento", action="store_true", help="publica também classes de movimento")
    ap.add_argument("--morrer-apos", type=float, default=0, help="encerra abruptamente após N s (testa o LWT)")
    a = ap.parse_args()
    for b in a.bancadas:
        threading.Thread(target=no, args=(b, a), daemon=True).start()
    print(f"simulando {a.bancadas} em {a.broker}:{a.porta} (Ctrl+C para sair)")
    try:
        if a.morrer_apos:
            time.sleep(a.morrer_apos)
            print("morrendo sem DISCONNECT… o broker publicará o LWT em ~7,5 s")
            os._exit(0)                      # sem fechar o socket: o broker é quem avisa
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
