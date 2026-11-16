#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GABARITO — Etapa 3, Parte B: extração das features (acc_x, acc_y, acc_z).

Lê a serial, valida cada linha e converte os três campos em float, atribuindo
cada um a uma variável com nome físico. Linhas malformadas (boot do ESP32,
linha cortada no meio, "ERR: ...") são descartadas com aviso.

Uso:
    python3 02_extrai_features.py --porta /dev/ttyUSB0
"""
import argparse
import sys

import serial


def process_line(line_str):
    """
    Processa UMA linha de texto: valida, extrai e converte os dados.
    Retorna [acc_x, acc_y, acc_z] (floats, em m/s²) ou None se a linha for inválida.
    """
    # Divide a linha em partes usando a vírgula como separador
    partes = line_str.split(",")
    if len(partes) == 3:
        try:
            # Converte cada parte para float
            acc_x = float(partes[0])
            acc_y = float(partes[1])
            acc_z = float(partes[2])
            return [acc_x, acc_y, acc_z]
        except (ValueError, IndexError):
            print(f"Aviso: não foi possível converter a linha: '{line_str}'")
            return None
    else:
        if line_str:
            print(f"Aviso: linha malformada ignorada: '{line_str}'")
        return None


def main():
    parser = argparse.ArgumentParser(description="Extração de features do acelerômetro")
    parser.add_argument("--porta", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    print("Etapa 3-B: extração de features...")
    try:
        with serial.Serial(args.porta, args.baud, timeout=1) as ser:
            print(f"Porta serial '{args.porta}' aberta com sucesso.")
            ser.reset_input_buffer()

            while True:
                line_str = ser.readline().decode("utf-8", errors="replace").strip()
                if line_str:
                    features = process_line(line_str)
                    if features:
                        acc_x, acc_y, acc_z = features
                        print(f"X: {acc_x:7.2f}  Y: {acc_y:7.2f}  Z: {acc_z:7.2f}  (m/s²)")

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
