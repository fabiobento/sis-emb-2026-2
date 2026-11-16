#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Etapa 3, Parte C: janela deslizante sobre as amostras do acelerômetro.

Acumula as amostras numa lista "achatada" [x0, y0, z0, x1, y1, z1, ...] — o mesmo
formato que o modelo do Edge Impulse espera ("raw features"). Quando a lista
atinge WINDOW_SIZE amostras (× 3 eixos), processa a janela e descarta as STRIDE
amostras mais antigas.

Com 62,5 Hz:  WINDOW_SIZE = 125 amostras = 2,0 s  (igual ao "Window size" do Studio)
              STRIDE      = 12 amostras  ≈ 0,19 s (uma decisão a cada ~0,2 s)

Uso:
    python3 03_janela_deslizante.py --porta /dev/ttyUSB0
"""
import argparse
import sys
import time

import serial

# Hiperparâmetros da janela deslizante
EIXOS = 3            # accX, accY, accZ
WINDOW_SIZE = 125    # amostras por janela (2 s a 62,5 Hz)
STRIDE = 12          # amostras descartadas a cada janela processada


def process_line(line_str):
    """Mesma função da Parte B: devolve [acc_x, acc_y, acc_z] ou None."""
    # TODO 1: cole aqui o corpo da process_line que você completou na Parte B
    ...


def process_window(window_data, n_janela, t_janela):
    """Chamada quando a janela está cheia. Por enquanto, só mostra informações."""
    n_amostras = len(window_data) // EIXOS
    print(f"--- Janela {n_janela}: {n_amostras} amostras "
          f"({len(window_data)} valores), {t_janela:.2f} s desde a anterior ---")
    print(f"  Primeira amostra: {window_data[0:3]}")
    print(f"  Última amostra:   {window_data[-3:]}")


def main():
    parser = argparse.ArgumentParser(description="Janela deslizante sobre a serial")
    parser.add_argument("--porta", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    print("Etapa 3-C: janela deslizante...")
    data_window = []
    n_janela = 0
    t_ant = time.monotonic()

    try:
        with serial.Serial(args.porta, args.baud, timeout=1) as ser:
            print(f"Porta serial '{args.porta}' aberta com sucesso.")
            ser.reset_input_buffer()

            while True:
                line_str = ser.readline().decode("utf-8", errors="replace").strip()
                if line_str:
                    features = process_line(line_str)
                    if features:
                        # TODO 2: acrescente os 3 valores de features no FIM de data_window
                        #         (dica: a lista é "achatada" — use extend, não append)
                        ...

                # janela cheia? (WINDOW_SIZE amostras × 3 eixos)
                if ...:  # TODO 3: a janela tem WINDOW_SIZE amostras de EIXOS valores?
                    agora = time.monotonic()
                    n_janela += 1
                    process_window(data_window, n_janela, agora - t_ant)
                    t_ant = agora
                    # desliza: remove as STRIDE amostras mais antigas (STRIDE × 3 valores)
                    data_window = ...  # TODO 4: descarte as STRIDE amostras mais antigas

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
