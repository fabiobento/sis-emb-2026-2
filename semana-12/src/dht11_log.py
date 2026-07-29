#!/usr/bin/env python3
# Semana 12 — DHT11 com log CSV
# instalar: sudo apt install python3-pip && pip3 install adafruit-circuitpython-dht
#           sudo apt install libgpiod2
import time, csv, board, adafruit_dht

SENSOR = adafruit_dht.DHT11(board.D4)     # dado no BCM 4 (pino fisico 7)
ARQ = "dht11_log.csv"

with open(ARQ, "a", newline="") as f:
    w = csv.writer(f)
    w.writerow(["epoch", "temp_C", "umid_pct"])
    while True:
        try:
            t, h = SENSOR.temperature, SENSOR.humidity
            print(f"T={t} C  UR={h} %")
            w.writerow([int(time.time()), t, h]); f.flush()
        except RuntimeError as e:          # DHT11 falha leituras: e normal, ignore
            print("leitura falhou:", e)
        time.sleep(2)
