#!/usr/bin/env python3
"""
Semana 14 — Logger de telemetria na borda (RPi 3).
Assina ifes/# no broker local e grava CSV com timestamp: telemetria.csv

Uso:  python3 logger_rpi.py [host_do_broker]   (padrão: localhost)
Dep.: sudo apt install python3-paho-mqtt   (ou pip3 install paho-mqtt)
"""
import csv
import sys
import time

import paho.mqtt.client as mqtt

HOST = sys.argv[1] if len(sys.argv) > 1 else "localhost"
ARQ = "telemetria.csv"


def on_connect(cli, userdata, flags, rc, properties=None):
    print(f"Conectado ao broker {HOST} (rc={rc}); assinando ifes/#")
    cli.subscribe("ifes/#")


def on_message(cli, userdata, msg):
    linha = [time.strftime("%Y-%m-%d %H:%M:%S"), msg.topic, msg.payload.decode(errors="replace")]
    print(*linha, sep=",")
    with open(ARQ, "a", newline="") as f:
        csv.writer(f).writerow(linha)


cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
cli.on_connect = on_connect
cli.on_message = on_message
cli.connect(HOST, 1883, 60)
print(f"Gravando em {ARQ} — Ctrl+C para sair")
cli.loop_forever()
