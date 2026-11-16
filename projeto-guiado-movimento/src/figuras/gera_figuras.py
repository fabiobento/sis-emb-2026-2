#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Gera as figuras do projeto guiado (pgm_*.png) em assets/figuras/."""
import os
import sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Rectangle, Circle

AQUI = os.path.dirname(os.path.abspath(__file__))
SAIDA = os.path.abspath(os.path.join(AQUI, "../../../assets/figuras"))
sys.path.insert(0, os.path.join(AQUI, "../python"))
from simulador_imu import amostra  # mesmos sinais sintéticos do simulador

plt.rcParams.update({"font.size": 11, "font.family": "DejaVu Sans"})
AZUL, VERM, VERDE, AMAR, CINZA, ROXO = "#1f6fb2", "#c0392b", "#2e7d32", "#f2b705", "#555555", "#7b3fa0"
CORES_EIXO = [VERM, VERDE, AZUL]
CLASSES = ["idle", "lift", "terrestrial", "maritime"]
FS = 62.5


def caixa(ax, x, y, w, h, texto, cor=AZUL, cor_txt="white", fs=11, preench=True, peso="bold"):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.08",
                                fc=cor if preench else "white", ec=cor, lw=2))
    ax.text(x + w / 2, y + h / 2, texto, ha="center", va="center", fontsize=fs,
            color=cor_txt if preench else "#222", weight=peso, linespacing=1.3)


def seta(ax, x1, y1, x2, y2, texto=None, cor="#333", dy=0.12, fs=9.5, estilo="-|>"):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle=estilo, mutation_scale=16,
                                 lw=1.8, color=cor))
    if texto:
        ax.text((x1 + x2) / 2, (y1 + y2) / 2 + dy, texto, ha="center", va="bottom", fontsize=fs,
                color=cor)


def salva(fig, nome):
    fig.savefig(os.path.join(SAIDA, nome), dpi=130, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print("gerada:", nome)


def sinal(classe, dur=4.0, t0=0.0):
    t = np.arange(0, dur, 1 / FS)
    np.random.seed(7)
    import random
    random.seed(7)
    xyz = np.array([amostra(classe, t0 + tt) for tt in t])
    return t, xyz


# --------------------------------------------------------------------------- 1
def fig_arquitetura():
    fig, ax = plt.subplots(figsize=(13, 6.2))
    ax.set_xlim(0, 13); ax.set_ylim(0, 6.2); ax.axis("off")
    ax.text(6.5, 5.95, "O projeto inteiro em um desenho: do sensor ao modelo e de volta à borda",
            ha="center", fontsize=14, weight="bold")
    # linha de cima: coleta e treino
    caixa(ax, 0.2, 3.6, 2.3, 1.4, "MPU-6050\n+ ESP32\n(firmware\nimu_serial)", AZUL, fs=10)
    caixa(ax, 3.6, 3.6, 2.5, 1.4, "PC do lab\nedge-impulse-\ndata-forwarder", VERDE)
    caixa(ax, 7.3, 3.6, 2.6, 1.4, "Edge Impulse\nStudio (nuvem)\ndados → impulso\n→ treino", ROXO, fs=10)
    caixa(ax, 10.8, 3.6, 2.0, 1.4, "Deployment\nmodelfile.eim\n(Linux x86 ou\nAARCH64)", CINZA, fs=9.5)
    seta(ax, 2.5, 4.3, 3.6, 4.3, "USB-serial\n115200 bps\n\"ax,ay,az\\n\"", dy=0.1, fs=8.5)
    seta(ax, 6.1, 4.3, 7.3, 4.3, "internet\n(amostras\nrotuladas)", dy=0.1, fs=8.5)
    seta(ax, 9.9, 4.3, 10.8, 4.3)
    ax.text(0.2, 5.2, "Etapas 1–2: COLETAR e TREINAR", color=VERDE, weight="bold", fontsize=11)
    # linha de baixo: inferência
    ax.text(0.2, 2.75, "Etapas 3–4: CLASSIFICAR na borda (sem nuvem)", color=VERM, weight="bold",
            fontsize=11)
    caixa(ax, 0.2, 0.5, 2.3, 1.4, "MPU-6050\n+ ESP32\n(o MESMO\nfirmware)", AZUL, fs=10)
    caixa(ax, 3.6, 0.5, 4.2, 1.9, "", VERM, preench=False)
    ax.text(5.7, 2.15, "PC (Etapa 3)  ou  Raspberry Pi (Etapa 4)", ha="center", fontsize=10,
            weight="bold", color=VERM)
    caixa(ax, 3.8, 0.7, 1.85, 1.2, "script Python\n04_classifica.py\n(janela\ndeslizante)", VERM,
          fs=8.5)
    caixa(ax, 5.8, 0.7, 1.8, 1.2, "modelfile.eim\n(DSP + rede\n+ K-means)", "#e67e22", fs=8.5)
    seta(ax, 5.65, 1.4, 5.8, 1.4, estilo="<|-|>")
    seta(ax, 2.5, 1.2, 3.8, 1.2, "USB-serial", dy=0.08, fs=8.5)
    caixa(ax, 9.0, 0.7, 3.8, 1.2, "terminal:\n[12.3 s] maritime  conf 0.94\nanomalia -0.31 | dsp 1 ms",
          "#222", fs=9.5, peso="normal")
    seta(ax, 7.8, 1.3, 9.0, 1.3)
    # o modelo desce da nuvem para a borda
    ax.add_patch(FancyArrowPatch((11.8, 3.6), (6.9, 1.95), connectionstyle="arc3,rad=-0.25",
                                 arrowstyle="-|>", mutation_scale=16, lw=1.8, color=CINZA, ls="--"))
    ax.text(11.4, 2.6, "download do .eim\n(uma vez)", fontsize=9, color=CINZA, ha="left")
    salva(fig, "pgm_arquitetura.png")


# --------------------------------------------------------------------------- 2
def fig_classes():
    fig = plt.figure(figsize=(14, 6.6))
    desc = {"idle": "idle — parado\n(armazém)",
            "lift": "lift — sobe e desce\n(empilhadeira)",
            "terrestrial": "terrestrial — vai e vem\nhorizontal (caminhão)",
            "maritime": "maritime — ondulação em\ntodos os eixos (barco)"}
    for i, c in enumerate(CLASSES):
        # desenho da caixa com setas
        a = fig.add_axes([0.02 + i * 0.245, 0.58, 0.21, 0.36])
        a.set_xlim(-2, 2); a.set_ylim(-2, 2); a.axis("off"); a.set_aspect("equal")
        a.add_patch(Rectangle((-0.7, -0.5), 1.4, 1.0, fc="#d9b38c", ec="#7a5230", lw=2))
        a.text(0, 0, "ESP32\n+MPU", ha="center", va="center", fontsize=8.5)
        kw = dict(arrowstyle="<|-|>", mutation_scale=18, lw=2.5, color=VERM)
        if c == "lift":
            a.add_patch(FancyArrowPatch((1.2, -1.4), (1.2, 1.4), **kw))
        elif c == "terrestrial":
            a.add_patch(FancyArrowPatch((-1.6, -1.1), (1.6, -1.1), **kw))
        elif c == "maritime":
            xs = np.linspace(-1.7, 1.7, 100)
            a.plot(xs, -1.2 + 0.25 * np.sin(4 * xs), color=AZUL, lw=2.5)
            a.add_patch(FancyArrowPatch((1.2, -0.9), (1.2, 1.3), connectionstyle="arc3,rad=0.5",
                                        **kw))
            a.add_patch(FancyArrowPatch((-1.2, 1.3), (-1.2, -0.9), connectionstyle="arc3,rad=0.5",
                                        **kw))
        else:
            a.text(0, -1.25, "z Z z", ha="center", fontsize=13, color=CINZA)
        a.set_title(desc[c], fontsize=11, weight="bold")
        # sinal
        b = fig.add_axes([0.035 + i * 0.245, 0.08, 0.205, 0.38])
        t, xyz = sinal(c, 4.0)
        for k in range(3):
            b.plot(t, xyz[:, k], color=CORES_EIXO[k], lw=1.2, label=["accX", "accY", "accZ"][k])
        b.set_ylim(-9, 22); b.set_xlabel("tempo (s)")
        if i == 0:
            b.set_ylabel("aceleração (m/s²)")
            b.legend(loc="upper right", fontsize=8, ncol=3)
        b.grid(alpha=0.3)
    fig.text(0.5, 1.06, "As 4 classes do projeto de referência (cenário de Rovai: estresse mecânico no transporte)",
             ha="center", fontsize=13, weight="bold")
    fig.text(0.5, 1.015, "sinais ilustrativos (sintéticos), sensor deitado: a gravidade (~9,8 m/s²) aparece no eixo Z",
             ha="center", fontsize=10.5, color=CINZA)
    salva(fig, "pgm_classes.png")


# --------------------------------------------------------------------------- 3
def fig_impulse():
    fig, ax = plt.subplots(figsize=(14, 5.6))
    ax.set_xlim(0, 14); ax.set_ylim(0, 5.6); ax.axis("off")
    ax.text(7, 5.35, "O \"impulso\" do Edge Impulse: janela → processamento de sinal → aprendizado",
            ha="center", fontsize=14, weight="bold")
    caixa(ax, 0.2, 2.0, 2.4, 2.0, "Time series data\n3 eixos @ 62,5 Hz\n\nWindow size 2000 ms\n"
          "Window increase 80 ms", AZUL, fs=10)
    ax.text(1.4, 1.55, "125 amostras × 3 eixos\n= 375 valores (\"raw features\")", ha="center",
            fontsize=9, color=AZUL)
    caixa(ax, 3.5, 2.0, 3.0, 2.0, "Spectral analysis\n(bloco de DSP)\n\nfiltro passa-baixas\nFFT → "
          "potência por faixa\nRMS, assimetria, curtose", VERDE, fs=10)
    ax.text(5.0, 1.55, "~33 a 63 números por janela\n(as \"features\")", ha="center", fontsize=9,
            color=VERDE)
    seta(ax, 2.6, 3.0, 3.5, 3.0)
    # rede neural
    ax.add_patch(FancyBboxPatch((7.4, 2.75), 3.4, 2.25, boxstyle="round,pad=0.02", fc="white",
                                ec=ROXO, lw=2))
    ax.text(9.1, 4.8, "Classification (rede neural)", ha="center", fontsize=10, weight="bold",
            color=ROXO)
    camadas = [6, 5, 4, 4]
    nomes = ["features", "20", "10", "4 classes"]
    xs = np.linspace(7.75, 10.45, 4)
    pos = []
    for x, n in zip(xs, camadas):
        ys = np.linspace(3.05, 4.45, n)
        pos.append([(x, y) for y in ys])
    for l in range(3):
        for p in pos[l]:
            for q in pos[l + 1]:
                ax.plot([p[0], q[0]], [p[1], q[1]], color=ROXO, lw=0.4, alpha=0.5)
    for l, camada in enumerate(pos):
        for (x, y) in camada:
            ax.add_patch(Circle((x, y), 0.07, fc=ROXO, ec="white", zorder=3))
        ax.text(xs[l], 2.82, nomes[l], ha="center", fontsize=7.5, color=ROXO)
    ax.add_patch(FancyBboxPatch((7.4, 0.45), 3.4, 1.85, boxstyle="round,pad=0.02", fc="white",
                                ec="#e67e22", lw=2))
    ax.text(9.1, 2.05, "Anomaly detection (K-means)", ha="center", fontsize=10, weight="bold",
            color="#e67e22")
    rng = np.random.default_rng(3)
    for cx, cy in [(8.2, 1.3), (9.0, 0.95), (9.9, 1.45)]:
        pts = rng.normal(0, 0.09, (12, 2)) + [cx, cy]
        ax.scatter(pts[:, 0], pts[:, 1], s=6, color="#e67e22")
        ax.add_patch(Circle((cx, cy), 0.28, fill=False, ec="#e67e22", ls="--"))
    ax.scatter([10.45], [0.8], marker="x", s=60, color=VERM, zorder=4)
    ax.text(10.45, 0.55, "longe de tudo\n= anomalia", fontsize=7.5, color=VERM, ha="center",
            va="top")
    seta(ax, 6.5, 3.2, 7.4, 3.8)
    seta(ax, 6.5, 2.8, 7.4, 1.6)
    caixa(ax, 11.6, 3.05, 2.2, 1.6, "idle 0.01\nlift 0.02\nmaritime 0.95\nterrestrial 0.02", ROXO,
          fs=9.5, peso="normal")
    caixa(ax, 11.6, 0.95, 2.2, 1.0, "anomaly score\n-0.31 (normal)", "#e67e22", fs=9.5,
          peso="normal")
    seta(ax, 10.8, 3.85, 11.6, 3.85)
    seta(ax, 10.8, 1.45, 11.6, 1.45)
    salva(fig, "pgm_impulse.png")


# --------------------------------------------------------------------------- 4
def fig_janela():
    fig, axs = plt.subplots(2, 1, figsize=(13, 6.4), gridspec_kw={"height_ratios": [1.3, 1]})
    t, xyz = sinal("terrestrial", 3.2)
    a = axs[0]
    a.plot(t, xyz[:, 0], color=VERM, lw=1.3)
    a.set_xlim(0, 3.2); a.set_ylim(-14, 14)
    a.set_ylabel("accX (m/s²)")
    a.set_title("Janela deslizante: o modelo sempre enxerga os ÚLTIMOS 2 s; a cada STRIDE amostras, "
                "uma nova decisão", fontsize=12.5, weight="bold")
    cores = [AZUL, VERDE, ROXO]
    for k in range(3):
        ini = k * 12 / FS
        a.add_patch(Rectangle((ini, -13 + k * 2.6), 2.0, 2.2, fc=cores[k], alpha=0.25, ec=cores[k]))
        a.text(ini + 1.0, -11.9 + k * 2.6, f"janela {k + 1}: amostras {k * 12}–{k * 12 + 124}",
               ha="center", va="center", fontsize=9, color=cores[k], weight="bold")
    a.annotate("", xy=(12 / FS, 11), xytext=(0, 11),
               arrowprops=dict(arrowstyle="<->", color="#222"))
    a.text(6 / FS, 11.8, "STRIDE = 12 amostras ≈ 0,19 s", fontsize=9.5, ha="left")
    a.grid(alpha=0.3)
    # parte de baixo: a lista achatada
    b = axs[1]
    b.set_xlim(0, 13); b.set_ylim(0, 3.4); b.axis("off")
    b.text(0, 3.1, "No Python, a janela é uma lista \"achatada\" de 125 × 3 = 375 floats "
           "(o formato que runner.classify() espera):", fontsize=11)
    vals = ["x0", "y0", "z0", "x1", "y1", "z1", "x2", "y2", "z2", "…", "x124", "y124", "z124"]
    for i, v in enumerate(vals):
        cor = {"x": VERM, "y": VERDE, "z": AZUL}.get(v[0], "#999")
        b.add_patch(Rectangle((0.2 + i * 0.95, 1.6), 0.9, 0.8, fc=cor, alpha=0.25 if v != "…" else 0,
                              ec=cor))
        b.text(0.65 + i * 0.95, 2.0, v, ha="center", va="center", fontsize=10)
    b.text(0.2, 1.05, "data_window.extend([acc_x, acc_y, acc_z])   →  acrescenta 3 valores no fim",
           fontsize=10.5, family="monospace")
    b.text(0.2, 0.45, "data_window = data_window[STRIDE * 3:]      →  descarta as 12 amostras mais "
           "antigas", fontsize=10.5, family="monospace")
    fig.tight_layout()
    salva(fig, "pgm_janela.png")


# --------------------------------------------------------------------------- 5
def fig_espectro():
    fig, axs = plt.subplots(2, 4, figsize=(15, 6.2))
    for i, c in enumerate(CLASSES):
        t, xyz = sinal(c, 2.0, t0=1.0)
        a = axs[0, i]
        for k in range(3):
            a.plot(t, xyz[:, k] - xyz[:, k].mean(), color=CORES_EIXO[k], lw=1.1,
                   label=["accX", "accY", "accZ"][k])
        a.set_title(c, weight="bold")
        a.set_ylim(-8, 8); a.grid(alpha=0.3)
        a.set_xlabel("tempo (s) — uma janela de 2 s")
        if i == 0:
            a.set_ylabel("sinal sem a média (m/s²)")
            a.legend(fontsize=7.5, loc="upper right")
        b = axs[1, i]
        f = np.fft.rfftfreq(128, 1 / FS)
        for k in range(3):
            x = xyz[:, k] - xyz[:, k].mean()
            X = np.abs(np.fft.rfft(x * np.hanning(len(x)), 128)) ** 2
            b.plot(f, 10 * np.log10(X + 1e-3), color=CORES_EIXO[k], lw=1.2)
        b.set_xlim(0, 31.25); b.set_ylim(-30, 60); b.grid(alpha=0.3)
        b.set_xlabel("frequência (Hz)")
        if i == 0:
            b.set_ylabel("potência (dB)")
    fig.suptitle("O que o bloco Spectral analysis \"vê\": no tempo (cima) e na frequência (baixo). "
                 "Cada classe tem uma assinatura diferente de energia por eixo e por faixa.",
                 fontsize=12.5, weight="bold")
    fig.tight_layout()
    salva(fig, "pgm_espectro.png")


# --------------------------------------------------------------------------- 6
def fig_amostragem():
    fig, axs = plt.subplots(2, 1, figsize=(13, 6.6), gridspec_kw={"height_ratios": [1, 1.25]})
    a = axs[0]
    a.set_xlim(0, 135); a.set_ylim(0, 3); a.set_yticks([]); a.set_xticks(range(0, 101, 20))
    for x in range(0, 101, 10):
        a.plot([x, x], [0, 2.4], color="#bbb", lw=0.8)
    a.text(50, 2.55, "ticks do FreeRTOS (100 Hz → um a cada 10 ms)", ha="center", fontsize=10,
           color="#777")
    for x in range(0, 100, 16):
        a.plot([x, x], [1.6, 2.3], color=VERDE, lw=3)
    a.text(103, 1.95, "desejado: 16 ms\n(62,5 Hz)", ha="left", va="center", fontsize=9.5,
           color=VERDE, weight="bold")
    for x in range(0, 100, 10):
        a.plot([x, x], [0.4, 1.1], color=VERM, lw=3)
    a.text(103, 0.75, "vTaskDelayUntil(16 ms)\n= 1 tick = 10 ms ✗", ha="left", va="center",
           fontsize=9.5, color=VERM, weight="bold")
    a.set_xlabel("tempo (ms)")
    a.set_title("Por que não vTaskDelayUntil? 16 ms não cabe na grade de 10 ms do tick",
                fontsize=12.5, weight="bold")
    b = axs[1]
    b.set_xlim(0, 13); b.set_ylim(0, 3.2); b.axis("off")
    b.text(6.5, 3.0, "A solução: esp_timer (resolução de µs, semana 4) + semáforo binário (semana 6)",
           ha="center", fontsize=12.5, weight="bold")
    caixa(b, 0.2, 1.0, 2.6, 1.3, "esp_timer\nperiódico\n16 000 µs", AZUL)
    caixa(b, 3.9, 1.0, 2.3, 1.3, "timer_cb()\nxSemaphoreGive", AMAR, cor_txt="#222")
    caixa(b, 7.2, 1.0, 2.5, 1.3, "tarefa_amostragem\nxSemaphoreTake\n→ lê 6 bytes (I2C)", VERDE,
          fs=10)
    caixa(b, 10.6, 1.0, 2.2, 1.3, "printf\n\"ax,ay,az\\n\"\n→ UART0 → USB", "#222", fs=10)
    seta(b, 2.8, 1.65, 3.9, 1.65, "a cada 16 ms", dy=0.1, fs=9)
    seta(b, 6.2, 1.65, 7.2, 1.65, "campainha", dy=0.1, fs=9)
    seta(b, 9.7, 1.65, 10.6, 1.65)
    b.text(5.05, 0.6, "curto: só sinaliza\n(nada de I2C/printf aqui)", ha="center", fontsize=9,
           color="#7a5a00")
    b.text(8.45, 0.6, "~0,3 ms de I2C a 400 kHz", ha="center", fontsize=9, color=VERDE)
    b.text(11.7, 0.6, "~2 ms de UART\n(24 bytes a 115200 bps)", ha="center", fontsize=9)
    fig.tight_layout()
    salva(fig, "pgm_amostragem.png")


# --------------------------------------------------------------------------- 7
def fig_eim():
    fig, ax = plt.subplots(figsize=(13, 4.6))
    ax.set_xlim(0, 13); ax.set_ylim(0, 4.6); ax.axis("off")
    ax.text(6.5, 4.35, "Como o Python conversa com o modelo: o .eim é um PROGRAMA separado",
            ha="center", fontsize=13.5, weight="bold")
    caixa(ax, 0.2, 1.2, 3.6, 2.4, "seu script Python\n\nrunner = ImpulseRunner(\n  'modelfile.eim')\n"
          "runner.init()\nres = runner.classify(janela)", AZUL, fs=9.5, peso="normal")
    caixa(ax, 4.9, 1.9, 3.0, 1.1, "socket Unix\n/tmp/…/runner.sock", CINZA, fs=10)
    caixa(ax, 9.0, 1.2, 3.8, 2.4, "processo modelfile.eim\n(C++ compilado para x86_64\nou aarch64)\n\n"
          "DSP + rede neural + K-means", "#e67e22", fs=9.5, peso="normal")
    seta(ax, 3.8, 2.75, 4.9, 2.75)
    ax.text(6.4, 3.25, 'ida:  {"classify": [375 floats]}', ha="center", fontsize=9)
    seta(ax, 7.9, 2.75, 9.0, 2.75)
    seta(ax, 9.0, 2.15, 7.9, 2.15)
    seta(ax, 4.9, 2.15, 3.8, 2.15)
    ax.text(6.4, 1.5, 'volta:  {"result": {...}, "timing": {...}}', ha="center", fontsize=9)
    ax.text(6.5, 0.45, "init() inicia o processo e pergunta \"hello\" → recebe model_parameters "
            "(eixos, frequência, input_features_count, labels).\nstop() encerra o processo — por "
            "isso ele fica no finally.", ha="center", fontsize=9.5, color="#333")
    salva(fig, "pgm_eim.png")


# --------------------------------------------------------------------------- 8
def fig_rpi():
    fig, ax = plt.subplots(figsize=(13, 4.8))
    ax.set_xlim(0, 13); ax.set_ylim(0, 4.8); ax.axis("off")
    ax.text(6.5, 4.55, "Etapa 4: o Raspberry Pi assume o papel do PC — o PC vira só um terminal "
            "(ssh/scp)", ha="center", fontsize=13, weight="bold")
    caixa(ax, 0.2, 1.4, 2.4, 1.6, "ESP32\n+ MPU-6050\n(firmware\nimu_serial)", AZUL, fs=10)
    caixa(ax, 4.0, 0.9, 4.2, 2.6, "", VERM, preench=False)
    ax.text(6.1, 3.7, "Raspberry Pi 3 (Raspberry Pi OS Lite 64-bit)", ha="center", fontsize=10,
            weight="bold", color=VERM)
    caixa(ax, 4.2, 1.1, 1.9, 1.75, "~/venv-ei\n04_classifica.py", VERM, fs=9.5)
    caixa(ax, 6.2, 1.1, 1.85, 1.75, "modelfile-\naarch64.eim", "#e67e22", fs=9.5)
    seta(ax, 2.6, 2.2, 4.2, 2.2, "USB\n/dev/ttyUSB0", dy=0.1, fs=8.5)
    caixa(ax, 10.0, 1.4, 2.8, 1.6, "PC do lab\nssh aluno@bancadaN.local\nscp …", "#222", fs=9.5,
          peso="normal")
    seta(ax, 10.0, 2.5, 8.2, 2.5, "ssh (comandos)", dy=0.1, fs=9)
    seta(ax, 10.0, 1.8, 8.2, 1.8, "scp (.py e .eim)", dy=-0.4, fs=9)
    ax.text(6.1, 0.35, "Wi-Fi do laboratório: o .eim também pode ser baixado direto no RPi "
            "(caminho B, com a CLI instalada)", ha="center", fontsize=9.5, color=CINZA)
    salva(fig, "pgm_rpi.png")


if __name__ == "__main__":
    os.makedirs(SAIDA, exist_ok=True)
    fig_arquitetura(); fig_classes(); fig_impulse(); fig_janela()
    fig_espectro(); fig_amostragem(); fig_eim(); fig_rpi()
