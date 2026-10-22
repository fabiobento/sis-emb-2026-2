"""Mede quantos bytes cada técnica gasta para manter o navegador atualizado.

Um proxy TCP "contador" fica entre o cliente e o painel e soma os bytes nos dois sentidos.
  polling  : GET /api/nos a cada 0,5 s (conexão keep-alive, como um navegador faria)
  websocket: uma conexão Socket.IO; o servidor empurra cada leitura quando ela chega

Uso (painel já rodando em 127.0.0.1:5003, com simulador publicando):
    python3 tools/medir_trafego.py --alvo 127.0.0.1:5003 --segundos 60 --json trafego.json

ATENÇÃO: conta bytes de aplicação (HTTP/WebSocket). Cabeçalhos TCP/IP (~40-52 B por pacote)
não entram, então em rede real os dois números são um pouco maiores.
"""
import argparse
import json
import socket
import threading
import time

import requests
import socketio


class ProxyContador:
    def __init__(self, alvo, porta=0):
        self.alvo = (alvo[0], int(alvo[1]))
        self.srv = socket.socket()
        self.srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.srv.bind(("127.0.0.1", porta))
        self.srv.listen(16)
        self.porta = self.srv.getsockname()[1]
        self.ida = self.volta = 0          # cliente->servidor, servidor->cliente
        self.lock = threading.Lock()
        threading.Thread(target=self._aceita, daemon=True).start()

    def _aceita(self):
        while True:
            try:
                c, _ = self.srv.accept()
            except OSError:
                return
            s = socket.create_connection(self.alvo)
            threading.Thread(target=self._bomba, args=(c, s, True), daemon=True).start()
            threading.Thread(target=self._bomba, args=(s, c, False), daemon=True).start()

    def _bomba(self, de, para, eh_ida):
        try:
            while True:
                d = de.recv(65536)
                if not d:
                    break
                with self.lock:
                    if eh_ida:
                        self.ida += len(d)
                    else:
                        self.volta += len(d)
                para.sendall(d)
        except OSError:
            pass
        finally:
            for x in (de, para):
                try:
                    x.close()
                except OSError:
                    pass

    def zera(self):
        with self.lock:
            self.ida = self.volta = 0

    def total(self):
        with self.lock:
            return self.ida, self.volta


def mede_polling(proxy, seg, periodo=0.5):
    s = requests.Session()
    s.get(f"http://127.0.0.1:{proxy.porta}/api/nos")        # abre a conexão antes de zerar
    proxy.zera()
    n = 0
    fim = time.time() + seg
    while time.time() < fim:
        s.get(f"http://127.0.0.1:{proxy.porta}/api/nos").raise_for_status()
        n += 1
        time.sleep(periodo)
    return n, *proxy.total()


def mede_websocket(proxy, seg):
    msgs = []
    sio = socketio.Client()
    for ev in ("leitura", "evento", "estado"):
        sio.on(ev, lambda d, ev=ev: msgs.append(ev))
    sio.connect(f"http://127.0.0.1:{proxy.porta}", transports=["websocket"])
    time.sleep(1)
    proxy.zera()
    msgs.clear()
    time.sleep(seg)
    r = proxy.total()
    n = len(msgs)
    sio.disconnect()
    return n, *r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--alvo", default="127.0.0.1:5003")
    ap.add_argument("--segundos", type=int, default=60)
    ap.add_argument("--json")
    a = ap.parse_args()
    h, p = a.alvo.split(":")
    proxy = ProxyContador((h, p))
    n1, i1, v1 = mede_polling(proxy, a.segundos)
    n2, i2, v2 = mede_websocket(proxy, a.segundos)
    res = {"segundos": a.segundos,
           "polling": {"requisicoes": n1, "bytes_ida": i1, "bytes_volta": v1},
           "websocket": {"mensagens": n2, "bytes_ida": i2, "bytes_volta": v2}}
    for nome, d in (("polling", res["polling"]), ("websocket", res["websocket"])):
        tot = d["bytes_ida"] + d["bytes_volta"]
        d["total"] = tot
        d["bytes_por_s"] = round(tot / a.segundos, 1)
        print(f"{nome:10s} ida={d['bytes_ida']:8d} B  volta={d['bytes_volta']:8d} B  total={tot:8d} B  ({d['bytes_por_s']} B/s)")
    if a.json:
        with open(a.json, "w") as f:
            json.dump(res, f, indent=1)


if __name__ == "__main__":
    main()
