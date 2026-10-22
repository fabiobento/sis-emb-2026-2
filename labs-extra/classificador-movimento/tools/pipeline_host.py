#!/usr/bin/env python3
"""Pipeline de movimento NO PC: janela -> features espectrais -> classificador -> matriz de confusao.

NAO e o Edge Impulse. E uma reimplementacao didatica e simplificada do mesmo raciocinio
(janela + DSP por FFT + classificador pequeno) para voce:
  1) entender o que o bloco 'Spectral Features' faz com cada janela;
  2) ter um BASELINE para comparar com o numero do Studio (se o Studio der muito menos que este
     baseline simples, algo esta errado nos seus dados/configuracao do Impulse);
  3) testar o pipeline com dados sinteticos antes de ter sensor.
Os numeros daqui NAO substituem o 'Model testing' do Studio.

Uso:
  python3 pipeline_host.py PASTA_COM_CSVs [--fs 100] [--janela-ms 2000] [--passo-ms 200]
Os arquivos devem se chamar  <classe>.<pessoa>.<nn>.csv  (cabecalho timestamp,accX,accY,accZ).
A divisao treino/teste e POR PESSOA (a pessoa de teste nunca aparece no treino), que e o teste
honesto para um gesto. Com uma so pessoa, divide por gravacao.
"""
import argparse
import glob
import os
import numpy as np

BANDAS_HZ = [(0.0, 0.7), (0.7, 1.5), (1.5, 2.5), (2.5, 4.0), (4.0, 7.0), (7.0, 15.0), (15.0, 50.0)]


def carrega(pasta):
    """Retorna lista de (classe, pessoa, array Nx3)."""
    out = []
    for fn in sorted(glob.glob(os.path.join(pasta, "*.csv"))):
        partes = os.path.basename(fn).split(".")
        classe, pessoa = partes[0], (partes[1] if len(partes) > 3 else "p1")
        d = np.loadtxt(fn, delimiter=",", skiprows=1)
        out.append((classe, pessoa, d[:, 1:4]))
    return out


def janelas(sinal, fs, janela_ms, passo_ms):
    n = int(fs * janela_ms / 1000); p = max(1, int(fs * passo_ms / 1000))
    return [sinal[i:i + n] for i in range(0, len(sinal) - n + 1, p)]


def features(jan, fs):
    """Por eixo: RMS e desvio (do sinal sem media), e energia espectral por faixa (FFT).
    A media (gravidade/inclinacao) e removida: o que distingue o gesto e a parte AC."""
    f = []
    n = len(jan)
    w = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1 / fs)
    for e in range(3):
        x = jan[:, e] - jan[:, e].mean()
        f += [np.sqrt(np.mean(x ** 2))]
        esp = np.abs(np.fft.rfft(x * w)) ** 2 / n
        tot = esp.sum() + 1e-12
        for lo, hi in BANDAS_HZ:
            m = (freqs >= lo) & (freqs < hi)
            f.append(np.log10(esp[m].sum() + 1e-9))          # energia na faixa (escala log)
        f.append(freqs[np.argmax(esp[1:]) + 1])               # frequencia dominante
    return np.array(f)


def monta(dados, fs, janela_ms, passo_ms):
    X, y, pes = [], [], []
    for classe, pessoa, s in dados:
        for j in janelas(s, fs, janela_ms, passo_ms):
            X.append(features(j, fs)); y.append(classe); pes.append(pessoa)
    return np.array(X), np.array(y), np.array(pes)


def softmax_treina(X, y, n_cls, epocas=600, lr=0.1, l2=1e-3):
    mu, sd = X.mean(0), X.std(0) + 1e-9
    Xn = (X - mu) / sd
    W = np.zeros((Xn.shape[1], n_cls)); b = np.zeros(n_cls)
    Y = np.eye(n_cls)[y]
    for _ in range(epocas):
        z = Xn @ W + b; z -= z.max(1, keepdims=True)
        p = np.exp(z); p /= p.sum(1, keepdims=True)
        g = (p - Y) / len(Xn)
        W -= lr * (Xn.T @ g + l2 * W); b -= lr * g.sum(0)
    return dict(W=W, b=b, mu=mu, sd=sd)


def preve(m, X):
    z = ((X - m["mu"]) / m["sd"]) @ m["W"] + m["b"]; z -= z.max(1, keepdims=True)
    p = np.exp(z); return p / p.sum(1, keepdims=True)


def confusao(y, yp, n):
    M = np.zeros((n, n), int)
    for a, b in zip(y, yp): M[a, b] += 1
    return M


def executa(pasta, fs=100, janela_ms=2000, passo_ms=200, verbose=True, split="pessoa"):
    dados = carrega(pasta)
    classes = sorted({c for c, _, _ in dados}, key=lambda c: (c != "parado", c))
    X, y, pes = monta(dados, fs, janela_ms, passo_ms)
    yi = np.array([classes.index(c) for c in y])
    pessoas = sorted(set(pes))
    teste_p = pessoas[-1] if (len(pessoas) > 1 and split == "pessoa") else None
    # split="janela": EMBARALHA janelas de gravacoes sobrepostas -> VAZAMENTO (so para demonstrar)
    rs = np.random.default_rng(0)
    tr = pes != teste_p if teste_p else rs.random(len(y)) > 0.2
    te = ~tr
    m = softmax_treina(X[tr], yi[tr], len(classes))
    acc_tr = (preve(m, X[tr]).argmax(1) == yi[tr]).mean()
    pt = preve(m, X[te]).argmax(1)
    acc_te = (pt == yi[te]).mean()
    M = confusao(yi[te], pt, len(classes))
    if verbose:
        print(f"{len(dados)} gravacoes, {len(y)} janelas ({janela_ms} ms, passo {passo_ms} ms), "
              f"{X.shape[1]} features/janela")
        print(f"teste = pessoa {teste_p}" if teste_p else "teste = 20% das janelas sorteadas (AVISO: vazamento, otimista)")
        print(f"acuracia treino = {acc_tr:.1%}   teste = {acc_te:.1%}")
        print("matriz de confusao (linhas = real, colunas = previsto):")
        print("            " + " ".join(f"{c[:10]:>10}" for c in classes))
        for i, c in enumerate(classes): print(f"{c[:11]:>11} " + " ".join(f"{v:10d}" for v in M[i]))
    return dict(classes=classes, X=X, y=yi, pessoas=pes, M=M, acc_tr=acc_tr, acc_te=acc_te, modelo=m)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("pasta"); ap.add_argument("--fs", type=int, default=100)
    ap.add_argument("--split", choices=["pessoa", "janela"], default="pessoa")
    ap.add_argument("--janela-ms", type=int, default=2000); ap.add_argument("--passo-ms", type=int, default=200)
    a = ap.parse_args()
    executa(a.pasta, a.fs, a.janela_ms, a.passo_ms, split=a.split)
