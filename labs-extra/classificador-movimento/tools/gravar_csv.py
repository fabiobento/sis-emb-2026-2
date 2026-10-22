#!/usr/bin/env python3
"""Grava gestos direto da serial do ESP32 (firmware src/coleta_mpu) em arquivos CSV.

Alternativa ao edge-impulse-data-forwarder: voce fica com uma COPIA LOCAL dos dados (backup,
analise no PC com pipeline_host.py) e depois envia ao Studio com o uploader da CLI ou pelo
'Upload data' do proprio Studio.

Uso:
  pip install pyserial
  python3 gravar_csv.py --porta /dev/ttyUSB0 --classe aceno --pessoa p1 --n 10 --duracao 10
Saida: dados/<classe>.<pessoa>.<nn>.csv  com cabecalho  timestamp,accX,accY,accZ
  (timestamp em ms = indice*1000/fs, pois a amostragem do firmware e periodica)

Depois, para enviar ao Studio (rotulo vem de --label; ajuste a chave de API do projeto):
  edge-impulse-uploader --category split --label aceno dados/aceno.*.csv
(--category split deixa o Studio dividir treino/teste; para teste POR PESSOA, prefira enviar os
 arquivos da pessoa reservada com --category testing.)

O script confere a taxa efetiva (amostras/segundo medidas no relogio do PC) e AVISA se ela fugir
de --fs em mais de 3%: taxa errada = espectro errado = classificador pior (Nyquist, semana 7).
"""
import argparse
import os
import re
import sys
import time

LINHA = re.compile(r"^\s*(-?\d+\.\d+),(-?\d+\.\d+),(-?\d+\.\d+)\s*$")


def grava_uma(ser, n_amostras, fs):
    ser.reset_input_buffer()
    amostras, t0 = [], None
    while len(amostras) < n_amostras:
        linha = ser.readline().decode(errors="ignore")
        m = LINHA.match(linha)
        if not m:
            continue                       # logs de boot, '# WHO_AM_I...', linha truncada
        if t0 is None:
            t0 = time.time()
        amostras.append(tuple(float(g) for g in m.groups()))
    dt = time.time() - t0
    return amostras, (len(amostras) - 1) / dt if dt > 0 else 0.0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--porta", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--classe", required=True, help="parado | aceno | sobe_desce | circulo (sem acento/espaco)")
    ap.add_argument("--pessoa", default="p1")
    ap.add_argument("--n", type=int, default=5, help="quantas gravacoes seguidas")
    ap.add_argument("--duracao", type=float, default=10.0, help="segundos por gravacao")
    ap.add_argument("--fs", type=int, default=100)
    ap.add_argument("--saida", default="dados")
    a = ap.parse_args()
    import serial                           # pyserial
    os.makedirs(a.saida, exist_ok=True)
    existentes = [f for f in os.listdir(a.saida) if f.startswith(f"{a.classe}.{a.pessoa}.")]
    k0 = len(existentes) + 1
    with serial.Serial(a.porta, a.baud, timeout=2) as ser:
        for k in range(k0, k0 + a.n):
            input(f"[{k - k0 + 1}/{a.n}] classe='{a.classe}' pessoa={a.pessoa}. ENTER e comece o gesto... ")
            for s in (3, 2, 1):
                print(s, end=" ", flush=True); time.sleep(0.5)
            print("GRAVANDO", flush=True)
            amostras, fs_med = grava_uma(ser, int(a.duracao * a.fs), a.fs)
            nome = os.path.join(a.saida, f"{a.classe}.{a.pessoa}.{k:02d}.csv")
            with open(nome, "w") as f:
                f.write("timestamp,accX,accY,accZ\n")
                for i, (x, y, z) in enumerate(amostras):
                    f.write(f"{i * 1000 // a.fs},{x:.4f},{y:.4f},{z:.4f}\n")
            aviso = "" if abs(fs_med - a.fs) <= 0.03 * a.fs else "  <-- TAXA FORA DE 3%: confira o firmware/cabo!"
            sat = sum(1 for (x, y, z) in amostras if max(abs(x), abs(y), abs(z)) >= 1.99)
            aviso += f"  <-- {sat} amostras saturadas (+-2 g): suba MPU_RANGE" if sat > 0.01 * len(amostras) else ""
            print(f"  salvo {nome}: {len(amostras)} amostras, taxa medida {fs_med:.1f} Hz{aviso}")


if __name__ == "__main__":
    sys.exit(main())
