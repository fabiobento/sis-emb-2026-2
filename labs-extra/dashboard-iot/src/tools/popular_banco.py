#!/usr/bin/env python3
"""Preenche o SQLite com HISTÓRICO SINTÉTICO (não são medições reais!).

Para ver a página de histórico sem esperar horas, e para testar o desempenho das consultas.
Uso:  python3 popular_banco.py [--db dashboard.db] [--horas 24] [--periodo 5] [--bancadas bancada1 bancada2]
"""
import argparse
import math
import random
import sys
import time

sys.path.insert(0, __file__.rsplit("/tools/", 1)[0])      # permite rodar de qualquer pasta
from dashboard.db import Banco                           # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="dashboard.db")
    ap.add_argument("--horas", type=float, default=24)
    ap.add_argument("--periodo", type=float, default=5, help="segundos entre leituras")
    ap.add_argument("--bancadas", nargs="+", default=["bancada1", "bancada2"])
    a = ap.parse_args()
    banco = Banco(a.db)
    banco.iniciar()
    agora = time.time()
    n = int(a.horas * 3600 / a.periodo)
    import sqlite3
    con = sqlite3.connect(a.db)
    linhas = []
    for k, b in enumerate(a.bancadas):
        fase = k * 1.3
        for i in range(n):
            ts = agora - (n - i) * a.periodo
            ciclo = (ts - agora) / 86400 * 2 * math.pi + fase * 0.3     # ciclo de 24 h que TERMINA perto da média
            temp = 26.5 + 2 * math.sin(ciclo) + random.gauss(0, 0.15)    # ciclo diário + ruído
            umid = 62 - 6 * math.sin(ciclo) + random.gauss(0, 0.8)
            linhas.append((ts, b, "temp", round(temp, 2)))
            linhas.append((ts, b, "umid", round(umid, 1)))
    con.executemany("INSERT INTO leituras (ts,bancada,grandeza,valor) VALUES (?,?,?,?)", linhas)
    con.commit(); con.close()
    print(f"{len(linhas)} leituras sintéticas inseridas em {a.db} ({a.horas} h, {len(a.bancadas)} bancadas)")


if __name__ == "__main__":
    main()
