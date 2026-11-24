#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Etapa 3, Parte D: classificação em tempo real com o modelo .eim.

Lê as amostras do ESP32 pela serial, monta a janela deslizante e entrega cada
janela cheia ao modelo do Edge Impulse (arquivo .eim) através do SDK Python.

O tamanho da janela NÃO é "chutado": ele é lido do próprio modelo
(input_features_count ÷ axis_count). Assim o script se adapta a qualquer
"Window size" escolhido no Studio — e acusa se o contrato foi quebrado.

Antes, baixe o modelo (no computador onde ele vai rodar):
    edge-impulse-linux-runner --clean --download modelfile.eim

Uso:
    python3 04_classifica.py ./modelfile.eim --porta /dev/ttyUSB0
    python3 04_classifica.py ./modelfile.eim --porta /dev/ttyUSB0 --csv saida.csv
"""
import argparse
import os
import sys
import time

import serial
from edge_impulse_linux.runner import ImpulseRunner

FS_FIRMWARE_HZ = 62.5   # taxa do firmware imu_serial (o "contrato")
STRIDE = 12             # amostras entre classificações (~0,19 s a 62,5 Hz)



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
    """Mesma função da Parte B: devolve [acc_x, acc_y, acc_z] ou None."""
    # TODO 1: cole aqui a process_line da Parte B (pode remover os prints de aviso)
    ...


def process_window(window_data, runner, limiar, t_inicio, log):
    """Classifica UMA janela e imprime o resultado de forma legível."""
    t0 = time.perf_counter()
    res = ...  # TODO 2: entregue a janela ao modelo (método classify do runner)
    t_total_ms = (time.perf_counter() - t0) * 1000.0

    scores = res["result"]["classification"]           # {'idle': 0.01, 'lift': 0.97, ...}
    classe = ...  # TODO 3: rótulo de MAIOR probabilidade (dica: max com key=scores.get)
    conf = scores[classe]
    anomalia = res["result"].get("anomaly")             # só existe se houver bloco K-means
    timing = res.get("timing", {})

    rotulo = classe if conf >= limiar else f"incerto ({classe}?)"
    texto_anom = f" | anomalia {anomalia:6.2f}" if anomalia is not None else ""
    print(f"[{time.monotonic() - t_inicio:7.2f} s] {rotulo:<22} conf {conf:4.2f}"
          f"{texto_anom} | dsp {timing.get('dsp', 0)} ms, "
          f"nn {timing.get('classification', 0)} ms, total {t_total_ms:.1f} ms")

    if log is not None:
        log.write(f"{time.time():.3f},{classe},{conf:.4f},"
                  f"{'' if anomalia is None else f'{anomalia:.4f}'},{t_total_ms:.2f}\n")
        log.flush()


def main():
    confere_todos()
    parser = argparse.ArgumentParser(description="Classificação em tempo real (Edge Impulse)")
    parser.add_argument("modelo", nargs="?", default="./modelfile.eim", help="arquivo .eim")
    parser.add_argument("--porta", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--limiar", type=float, default=0.6,
                        help="confiança mínima para aceitar a classe (0 a 1)")
    parser.add_argument("--csv", default=None, help="grava cada inferência neste CSV")
    args = parser.parse_args()

    print("Etapa 3-D: classificação de dados...")

    modelo = os.path.abspath(args.modelo)
    runner = ImpulseRunner(modelo)
    log = None

    try:
        model_info = runner.init()   # inicia o processo .eim e pergunta "quem é você?"
        mp = model_info["model_parameters"]
        print(f'Carregado o runner para "{model_info["project"]["owner"]} / '
              f'{model_info["project"]["name"]}"')

        # ---- o contrato, lido do próprio modelo ----
        eixos = mp.get("axis_count", 3)
        window_size = ...  # TODO 4: amostras por janela = input_features_count ÷ eixos
        freq = mp.get("frequency", 0)
        duracao = f"{window_size / freq:.2f} s" if freq else "duração desconhecida"
        print(f"Modelo: {eixos} eixos, {freq} Hz, janela de {window_size} amostras "
              f"({duracao}), rótulos {mp['labels']}")
        if eixos != 3:
            print("ERRO: o modelo não espera 3 eixos — confira o 'Create impulse'.")
            sys.exit(1)
        if freq and abs(freq - FS_FIRMWARE_HZ) / FS_FIRMWARE_HZ > 0.02:
            print(f"ATENÇÃO: o modelo foi treinado a {freq} Hz e o firmware envia "
                  f"{FS_FIRMWARE_HZ} Hz. O contrato foi quebrado!")

        if args.csv:
            log = open(args.csv, "w", encoding="utf-8")
            log.write("epoch_s,classe,confianca,anomalia,tempo_ms\n")

        data_window = []
        t_inicio = time.monotonic()
        with serial.Serial(args.porta, args.baud, timeout=1) as ser:
            print(f"Porta serial '{args.porta}' aberta com sucesso.")
            ser.reset_input_buffer()

            while True:
                line_str = ser.readline().decode("utf-8", errors="replace").strip()
                if line_str:
                    features = process_line(line_str)
                    if features:
                        ...  # TODO 5: acrescente features ao fim de data_window

                if len(data_window) >= window_size * eixos:
                    process_window(data_window, runner, args.limiar, t_inicio, log)
                    data_window = ...  # TODO 6: deslize a janela (descarte STRIDE amostras)

    except serial.SerialException as e:
        print(f"Erro crítico: não foi possível abrir a porta serial '{args.porta}'.")
        print(f"Detalhes: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nPrograma encerrado pelo usuário.")
    finally:
        if runner:
            runner.stop()          # encerra o processo .eim
        if log:
            log.close()
        print("Finalizando o programa.")


if __name__ == "__main__":
    main()
