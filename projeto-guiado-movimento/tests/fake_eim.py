#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
.eim FALSO — só para testar os scripts sem uma conta no Edge Impulse.

Implementa o mesmo protocolo do executável .eim real: recebe o caminho de um
socket Unix como argumento, escuta nele e responde mensagens JSON terminadas
em byte nulo: {"hello":1} e {"classify":[...]}.

O "modelo" é uma regra simples baseada na variância de cada eixo — NÃO é o
modelo treinado. Ele valida, porém, o que importa no teste: o número exato de
valores por janela (input_features_count) e o formato da resposta.
"""
import json
import os
import socket
import sys

FREQ = 62.5
JANELA = 125            # amostras (2 s)
EIXOS = 3
LABELS = ["idle", "lift", "maritime", "terrestrial"]


def var(v):
    m = sum(v) / len(v)
    return sum((x - m) ** 2 for x in v) / len(v)


def classifica(feat):
    xs, ys, zs = feat[0::3], feat[1::3], feat[2::3]
    vx, vy, vz = var(xs), var(ys), var(zs)
    tot = vx + vy + vz
    if tot < 0.05:
        c = "idle"
    elif vz > 2 * (vx + vy):
        c = "lift"
    elif vx > 4 * (vy + vz):
        c = "terrestrial"
    else:
        c = "maritime"
    res = {l: 0.02 for l in LABELS}
    res[c] = 1.0 - 0.02 * (len(LABELS) - 1)
    return res, min(tot / 50.0, 3.0) - 1.0


def main():
    caminho = sys.argv[1]
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(caminho)
    srv.listen(1)
    con, _ = srv.accept()
    buf = b""
    while True:
        dado = con.recv(65536)
        if not dado:
            break
        buf += dado
        try:
            msg = json.loads(buf.decode())
        except ValueError:
            continue                      # mensagem ainda incompleta
        buf = b""
        if "hello" in msg:
            resp = {"id": msg["id"], "success": True,
                    "project": {"owner": "Teste", "name": "fake-movimento", "id": 0,
                                "deploy_version": 1},
                    "model_parameters": {"axis_count": EIXOS, "frequency": FREQ,
                                         "interval_ms": 1000 / FREQ,
                                         "input_features_count": JANELA * EIXOS,
                                         "labels": LABELS, "label_count": len(LABELS),
                                         "has_anomaly": 1, "sensor": 2}}
        elif "classify" in msg:
            f = msg["classify"]
            if len(f) != JANELA * EIXOS:
                resp = {"id": msg["id"], "success": False,
                        "error": f"The features array is {len(f)} elements, "
                                 f"expected {JANELA * EIXOS}"}
            else:
                c, a = classifica(f)
                resp = {"id": msg["id"], "success": True,
                        "result": {"classification": c, "anomaly": a},
                        "timing": {"dsp": 0, "classification": 0, "anomaly": 0}}
        else:
            resp = {"id": msg.get("id", 0), "success": False, "error": "unknown"}
        con.sendall(json.dumps(resp).encode() + b"\x00")


if __name__ == "__main__":
    main()
