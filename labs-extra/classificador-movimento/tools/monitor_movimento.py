#!/usr/bin/env python3
"""Painel de terminal (no RPi ou no PC) para o classificador de movimento.

Assina  ifes/<bancada>/movimento  e  ifes/<bancada>/movimento/info  e  ifes/<bancada>/status,
mostra cada mudanca de classe com a duracao da anterior, conta repeticoes de uma classe
(--conta sobe_desce: contador de repeticoes de exercicio) e grava tudo em movimento.csv.

Uso:  python3 monitor_movimento.py [--broker localhost] [--bancada bancada1] [--conta sobe_desce]
Dep.: sudo apt install python3-paho-mqtt   (ou pip3 install paho-mqtt)
"""
import argparse
import csv
import json
import time

import paho.mqtt.client as mqtt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--broker", default="localhost")
    ap.add_argument("--bancada", default="bancada1")
    ap.add_argument("--conta", default=None, help="classe cujas ocorrencias serao contadas")
    ap.add_argument("--csv", default="movimento.csv")
    a = ap.parse_args()
    base = f"ifes/{a.bancada}"
    estado = {"classe": None, "desde": time.time(), "n": 0}

    def on_connect(cli, ud, flags, rc, props=None):
        print(f"broker {a.broker} (rc={rc}); assinando {base}/#")
        cli.subscribe([(f"{base}/movimento", 0), (f"{base}/movimento/info", 0), (f"{base}/status", 1)])

    def on_message(cli, ud, msg):
        txt = msg.payload.decode(errors="replace")
        agora = time.strftime("%H:%M:%S")
        if msg.topic.endswith("/status"):
            print(f"{agora}  [status] {txt}")
        elif msg.topic.endswith("/info"):
            print(f"{agora}  [info]   {txt}")
        else:
            try:
                d = json.loads(txt)
            except ValueError:
                print(f"{agora}  payload invalido: {txt!r}"); return
            dur = time.time() - estado["desde"]
            ant = estado["classe"]
            if d["classe"] == a.conta:
                estado["n"] += 1
            extra = f"  | {a.conta}: {estado['n']} repeticoes" if a.conta else ""
            print(f"{agora}  {ant or '-'} ({dur:.1f}s) -> {d['classe']}  conf={d['conf']:.2f}{extra}")
            with open(a.csv, "a", newline="") as f:
                csv.writer(f).writerow([time.strftime("%Y-%m-%d %H:%M:%S"), d["classe"], d["conf"], ant, f"{dur:.2f}"])
            estado.update(classe=d["classe"], desde=time.time())

    cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    cli.on_connect, cli.on_message = on_connect, on_message
    cli.connect(a.broker, 1883, 60)
    cli.loop_forever()


if __name__ == "__main__":
    main()
