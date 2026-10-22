#!/usr/bin/env python3
"""Gera gravacoes SINTETICAS de movimento (nao sao dados reais!) no mesmo formato CSV do
Edge Impulse:  timestamp,accX,accY,accZ  (timestamp em ms, aceleracao em g).

Para que serve: testar o pipeline (tools/pipeline_host.py, upload no Studio, figuras) ANTES de
ter sensor e antes de ter gravacoes reais. NAO use dados sinteticos para tirar conclusoes sobre o
seu classificador: o numero que vale e o de teste com gestos reais, de pessoas diferentes.

Uso:  python3 gera_sinteticos.py [pasta_saida] [--fs 100] [--seed 1]
Saida: pasta/<classe>.<pessoa>.<nn>.csv   (ex.: aceno.p2.03.csv)
"""
import argparse
import os
import numpy as np

CLASSES = ["parado", "aceno", "sobe_desce", "circulo"]


def gesto(classe, n, fs, rng, pessoa, dificil=False):
    """Uma gravacao de n amostras. 'pessoa' muda frequencia, amplitude e inclinacao tipicas."""
    t = np.arange(n) / fs
    f_mult, a_mult, tilt = pessoa
    # gravidade: o sensor esta levemente inclinado (varia por pessoa/posicao de segurar)
    gx, gy = np.sin(tilt[0]), np.sin(tilt[1])
    gz = np.sqrt(max(1 - gx**2 - gy**2, 0))
    x, y, z = np.full(n, gx), np.full(n, gy), np.full(n, gz)
    ph = rng.uniform(0, 2 * np.pi, 3)
    # modo dificil: faixas de frequencia SE SOBREPOEM (mais proximo da vida real)
    FR = {"aceno": (1.8, 2.8), "sobe_desce": (1.3, 2.3), "circulo": (1.0, 2.0)} if dificil else \
         {"aceno": (2.0, 3.0), "sobe_desce": (1.0, 1.6), "circulo": (0.8, 1.3)}
    if classe == "aceno":          # oscilacao lateral rapida (~2-3 Hz), eixo x dominante
        f = rng.uniform(*FR["aceno"]) * f_mult; A = rng.uniform(0.45, 0.8) * a_mult
        x += A * np.sin(2 * np.pi * f * t + ph[0]); y += 0.25 * A * np.sin(2 * np.pi * f * t + ph[1])
    elif classe == "sobe_desce":   # oscilacao vertical mais lenta (~1-1,6 Hz), eixo z dominante
        f = rng.uniform(*FR["sobe_desce"]) * f_mult; A = rng.uniform(0.35, 0.7) * a_mult
        z += A * np.sin(2 * np.pi * f * t + ph[0]); x += 0.2 * A * np.sin(2 * np.pi * f * t + ph[1])
    elif classe == "circulo":      # x e y em quadratura (~0,8-1,3 Hz), z quase parado
        f = rng.uniform(*FR["circulo"]) * f_mult; A = rng.uniform(0.25, 0.5) * a_mult
        x += A * np.sin(2 * np.pi * f * t + ph[0]); y += A * np.cos(2 * np.pi * f * t + ph[0])
        z += 0.1 * A * np.sin(2 * np.pi * 2 * f * t)
    ruido = (0.04 if dificil else 0.012) * (1 + 0.5 * rng.random())   # ruido do sensor + tremor da mao
    x += rng.normal(0, ruido, n); y += rng.normal(0, ruido, n); z += rng.normal(0, ruido, n)
    v = np.stack([x, y, z], 1)
    if dificil:    # sensor preso em orientacao diferente em cada pessoa: gira o vetor inteiro
        ang = tilt[0] * 3.0
        R = np.array([[np.cos(ang), -np.sin(ang), 0], [np.sin(ang), np.cos(ang), 0], [0, 0, 1]])
        v = v @ R.T
    return np.clip(v, -2.0, 2.0)                       # o MPU em +-2 g satura


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("saida", nargs="?", default="dados_sinteticos")
    ap.add_argument("--fs", type=int, default=100)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--pessoas", type=int, default=4)
    ap.add_argument("--por-classe", type=int, default=6, help="gravacoes por classe e por pessoa")
    ap.add_argument("--duracao", type=float, default=10.0)
    ap.add_argument("--dificil", action="store_true", help="frequencias sobrepostas, mais ruido, sensor girado")
    a = ap.parse_args()
    rng = np.random.default_rng(a.seed)
    os.makedirs(a.saida, exist_ok=True)
    n = int(a.duracao * a.fs)
    total = 0
    for p in range(1, a.pessoas + 1):
        pessoa = (rng.uniform(0.85, 1.2), rng.uniform(0.75, 1.3), rng.uniform(-0.35, 0.35, 2))
        for c in CLASSES:
            for k in range(1, a.por_classe + 1):
                d = gesto(c, n, a.fs, rng, pessoa, a.dificil)
                nome = f"{c}.p{p}.{k:02d}.csv"
                with open(os.path.join(a.saida, nome), "w") as f:
                    f.write("timestamp,accX,accY,accZ\n")
                    for i in range(n):
                        f.write(f"{i * 1000 // a.fs},{d[i,0]:.4f},{d[i,1]:.4f},{d[i,2]:.4f}\n")
                total += 1
    print(f"{total} arquivos em {a.saida}/  ({a.pessoas} pessoas x {len(CLASSES)} classes x {a.por_classe})")


if __name__ == "__main__":
    main()
