#!/usr/bin/env python3
# Semana 12 — HC-SR04 (TRIG=BCM23, ECHO=BCM24 VIA DIVISOR 1k/2k!) + estatistica
from gpiozero import DistanceSensor
from statistics import mean, stdev
import time

s = DistanceSensor(echo=24, trigger=23, max_distance=2.0)
amostras = []
print("coletando 100 amostras...")
for i in range(100):
    amostras.append(s.distance * 100)     # cm
    time.sleep(0.06)                      # HC-SR04: aguardar >60 ms entre disparos
print(f"media = {mean(amostras):.2f} cm | desvio = {stdev(amostras):.2f} cm "
      f"| min = {min(amostras):.2f} | max = {max(amostras):.2f}")
