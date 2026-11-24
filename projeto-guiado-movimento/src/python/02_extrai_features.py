#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Etapa 3, Parte B: extração das features (acc_x, acc_y, acc_z).

Lê a serial, valida cada linha e converte os três campos em float, atribuindo
cada um a uma variável com nome físico. Linhas malformadas (boot do ESP32,
linha cortada no meio, "ERR: ...") são descartadas com aviso.

Uso:
    python3 02_extrai_features.py --porta /dev/ttyUSB0
"""
import argparse
import sys

import serial



def confere_todos():
    """Antes de abrir a serial, avisa se ainda há TODOs sem completar neste arquivo."""
    import re
    linhas = open(__file__, encoding="utf-8").read().splitlines()
    pendentes = []
    for i, linha in enumerate(linhas):
        codigo = linha.split("#", 1)[0]
        if re.search(r"(^\s*|=\s*|if\s+)\.\.\.\s*:?\s*$", codigo):
            num = None
            for vizinha in (linha, linhas[i - 1] if i else "", linhas[i - 2] if i > 1 else ""):
                m = re.search(r"TODO\s*(\d+)", vizinha)
                if m:
                    num = m.group(1)
                    break
            pendentes.append(f"TODO {num} (linha {i + 1})" if num else f"linha {i + 1}")
    if pendentes:
        print("⚠️  Este arquivo ainda tem TODOs sem completar: " + ", ".join(pendentes) + ".")
        print("    Troque cada '...' pelo código pedido no comentário (veja o roteiro da")
        print("    Etapa 3) e rode de novo.")
        sys.exit(1)

def process_line(line_str):
    """
    Processa UMA linha de texto: valida, extrai e converte os dados.
    Retorna [acc_x, acc_y, acc_z] (floats, em m/s²) ou None se a linha for inválida.
    """
    # Divide a linha em partes usando a vírgula como separador
    partes = ...  # TODO 1: separe line_str pela vírgula (método split)
    if len(partes) == 3:
        try:
            # Converte cada parte para float
            acc_x = ...  # TODO 2: converta o 1º campo para float
            acc_y = ...  # TODO 3: converta o 2º campo para float
            acc_z = ...  # TODO 4: converta o 3º campo para float
            return [acc_x, acc_y, acc_z]
        except (ValueError, IndexError):
            print(f"Aviso: não foi possível converter a linha: '{line_str}'")
            return None
    else:
        if line_str:
            print(f"Aviso: linha malformada ignorada: '{line_str}'")
        return None


def main():
    confere_todos()
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
