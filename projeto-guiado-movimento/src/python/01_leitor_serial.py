#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Etapa 3, Parte A — Leitura básica da serial do ESP32 no computador.

Abre a porta serial onde o ESP32 está ligado, lê as linhas "accX,accY,accZ"
enviadas pelo firmware imu_serial e as exibe no terminal. A cada segundo,
mostra também a TAXA medida (linhas/s), que deve ficar perto de 62,5 Hz.

Uso:
    python3 01_leitor_serial.py                      # porta padrão /dev/ttyUSB0
    python3 01_leitor_serial.py --porta /dev/ttyACM0
    python3 01_leitor_serial.py --porta /dev/pts/5   # com o simulador_imu.py

Encerrar: Ctrl+C
"""
import argparse
import sys
import time

import serial  # pacote pyserial


def main():
    parser = argparse.ArgumentParser(description="Leitor serial do acelerômetro")
    parser.add_argument("--porta", default="/dev/ttyUSB0", help="porta serial do ESP32")
    parser.add_argument("--baud", type=int, default=115200, help="taxa da UART (bps)")
    args = parser.parse_args()

    print("Etapa 3-A: leitura básica da serial...")

    try:
        # O 'with' garante que a porta será fechada mesmo se houver erro ou Ctrl+C.
        with serial.Serial(args.porta, args.baud, timeout=1) as ser:
            print(f"Porta serial '{args.porta}' aberta com sucesso ({args.baud} bps).")
            ser.reset_input_buffer()  # descarta o que acumulou antes de abrirmos

            n_linhas = 0
            t0 = time.monotonic()
            while True:
                # readline() lê bytes até o '\n' (ou até o timeout de 1 s)
                linha = ser.readline().decode("utf-8", errors="replace").strip()

                if linha:
                    print(f"Recebido: {linha}")
                    n_linhas += 1

                # a cada 1 s, mostra quantas linhas chegaram (= taxa de amostragem)
                agora = time.monotonic()
                if agora - t0 >= 1.0:
                    print(f"---- taxa medida: {n_linhas / (agora - t0):.1f} linhas/s ----")
                    n_linhas = 0
                    t0 = agora

    except serial.SerialException as e:
        print(f"Erro crítico: não foi possível abrir a porta serial '{args.porta}'.")
        print(f"Detalhes: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nPrograma encerrado pelo usuário.")
    finally:
        print("Finalizando o programa.")


if __name__ == "__main__":
    main()
