"""Interpretação dos tópicos MQTT — função PURA (sem rede, sem banco), por isso fácil de testar.

Árvore de tópicos adotada (padrão da semana 14, prefixo `ifes`):

    ifes/<bancada>/<grandeza>            número em texto  -> leitura (temp, umid, dist, ...)
    ifes/<bancada>/status                online | offline -> estado do nó (LWT)
    ifes/<bancada>/movimento             JSON {"classe":..,"conf":..}  (trilha TinyML)
    ifes/<bancada>/movimento/info        ignorado (diagnóstico do nó)
    ifes/<bancada>/cmd/<atuador>         comandos (o dashboard os PUBLICA; não os grava)
"""
import json
import math
import re

RE_BANCADA = re.compile(r"^[a-z0-9_-]{1,32}$")
RE_GRANDEZA = re.compile(r"^[a-z0-9_]{1,32}$")
RE_CLASSE = re.compile(r"^[A-Za-z0-9_]{1,32}$")
RESERVADOS = {"status", "movimento", "cmd"}


def interpreta(base, topico, payload, agora):
    """Devolve um dict descrevendo o dado, ou None se a mensagem deve ser ignorada.

    base: prefixo (ex. "ifes"); topico: str; payload: bytes|str; agora: timestamp (s).
    O ESP32 não tem relógio de parede: o carimbo de tempo é o da CHEGADA ao servidor.
    """
    partes = topico.split("/")
    if len(partes) < 3 or partes[0] != base:
        return None
    bancada, resto = partes[1], partes[2:]
    if not RE_BANCADA.match(bancada):
        return None
    if isinstance(payload, bytes):
        payload = payload.decode("utf-8", errors="replace")
    texto = payload.strip()

    if resto == ["status"]:
        if texto not in ("online", "offline"):
            return None
        return {"tipo": "status", "bancada": bancada, "texto": texto, "ts": agora}

    if resto == ["movimento"]:
        try:
            d = json.loads(texto)
            classe, conf = d["classe"], float(d["conf"])
        except (ValueError, KeyError, TypeError):
            return None
        if not isinstance(classe, str) or not RE_CLASSE.match(classe) or not 0.0 <= conf <= 1.0:
            return None
        return {"tipo": "movimento", "bancada": bancada, "classe": classe,
                "conf": round(conf, 3), "ts": agora}

    if len(resto) == 1 and resto[0] not in RESERVADOS and RE_GRANDEZA.match(resto[0]):
        try:
            valor = float(texto)
        except ValueError:
            return None
        if not math.isfinite(valor):                 # "nan" e "inf" passam em float(): barre aqui
            return None
        return {"tipo": "leitura", "bancada": bancada, "grandeza": resto[0],
                "valor": valor, "ts": agora}
    return None
