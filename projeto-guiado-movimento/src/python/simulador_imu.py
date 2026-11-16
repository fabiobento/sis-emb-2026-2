#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Simulador do ESP32 + MPU-6050 (para testar os scripts SEM hardware).

Cria uma porta serial VIRTUAL (um pseudo-terminal, /dev/pts/N) e escreve nela
linhas "accX,accY,accZ" em m/s², a 62,5 Hz — exatamente como o firmware
imu_serial. Os scripts 01–04 abrem essa porta como se fosse o ESP32.

ATENÇÃO: os sinais são SINTÉTICOS (senoides + ruído). Servem para testar o
código (leitura, janela, chamada do modelo), não para treinar um modelo.

Uso (dois terminais):
    terminal 1:  python3 simulador_imu.py                   # alterna as classes a cada 8 s
                 python3 simulador_imu.py --classe lift     # fica numa classe só
    terminal 2:  python3 01_leitor_serial.py --porta /dev/pts/N   (N impresso no terminal 1)
"""
import argparse
import math
import os
import pty
import random
import time
import tty

FS_HZ = 62.5
G = 9.80665
CLASSES = ["idle", "lift", "maritime", "terrestrial"]


def amostra(classe, t):
    """Aceleração (m/s²) de um sensor deitado na mesa (gravidade no eixo Z)."""
    r = lambda s: random.gauss(0.0, s)
    if classe == "idle":            # parado: só gravidade + ruído
        return 0.05 + r(0.03), -0.08 + r(0.03), G + r(0.03)
    if classe == "lift":            # empilhadeira: sobe-desce (eixo Z)
        return r(0.2), r(0.2), G + 4.0 * math.sin(2 * math.pi * 0.9 * t) + r(0.3)
    if classe == "terrestrial":     # caminhão: vai-e-vem horizontal (eixo X)
        return 5.0 * math.sin(2 * math.pi * 1.6 * t) + r(0.4), 0.8 * math.sin(2 * math.pi * 3.2 * t) + r(0.3), G + r(0.3)
    # maritime — barco: ondulação lenta em todos os eixos, com inclinação
    incl = 0.35 * math.sin(2 * math.pi * 0.4 * t)
    return (G * math.sin(incl) + 2.5 * math.sin(2 * math.pi * 0.6 * t) + r(0.3),
            2.5 * math.sin(2 * math.pi * 0.5 * t + 1.0) + r(0.3),
            G * math.cos(incl) + 1.5 * math.sin(2 * math.pi * 0.45 * t) + r(0.3))


def main():
    p = argparse.ArgumentParser(description="ESP32 + MPU-6050 virtual")
    p.add_argument("--classe", choices=CLASSES, default=None,
                   help="fixa uma classe (padrão: alterna a cada --troca s)")
    p.add_argument("--troca", type=float, default=8.0, help="segundos por classe")
    p.add_argument("--sem-boot", action="store_true",
                   help="não imprime o 'lixo' de boot no início")
    a = p.parse_args()

    mestre, escravo = pty.openpty()
    tty.setraw(escravo)                       # sem eco nem tradução de fim de linha
    print(f"Porta virtual criada: {os.ttyname(escravo)}   (Ctrl+C encerra)")

    if not a.sem_boot:                        # imita o que o ESP32 cospe ao resetar
        os.write(mestre, b"ets Jun  8 2016 00:22:57\r\n\r\nrst:0x1 (POWERON_RESET),boot:0x13\r\n")

    periodo = 1.0 / FS_HZ
    t0 = time.monotonic()
    k = 0
    classe_ant = None
    try:
        while True:
            t = k * periodo
            classe = a.classe or CLASSES[int(t // a.troca) % len(CLASSES)]
            if classe != classe_ant:
                print(f"[{t:6.1f} s] simulando: {classe}")
                classe_ant = classe
            x, y, z = amostra(classe, t)
            os.write(mestre, f"{x:.4f},{y:.4f},{z:.4f}\n".encode())
            k += 1
            # espera até o instante ABSOLUTO da próxima amostra (sem deriva — semana 5)
            espera = t0 + k * periodo - time.monotonic()
            if espera > 0:
                time.sleep(espera)
            elif espera < -0.2:               # ficou bloqueado (ninguém lendo): ressincroniza
                t0 = time.monotonic() - k * periodo
    except KeyboardInterrupt:
        print("\nSimulador encerrado.")
    except OSError:
        pass


if __name__ == "__main__":
    main()
