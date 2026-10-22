"""Configuração por variáveis de ambiente (12-factor): nada de senha dentro do código.

Em produção as variáveis vêm de /etc/default/dashboard (lido pelo systemd; ver deploy/).
"""
import os


def _int(nome, padrao):
    try:
        return int(os.getenv(nome, padrao))
    except ValueError:
        return padrao


class Config:
    MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
    MQTT_PORT = _int("MQTT_PORT", 1883)
    MQTT_USER = os.getenv("MQTT_USER", "")
    MQTT_PASS = os.getenv("MQTT_PASS", "")
    TOPICO_BASE = os.getenv("TOPICO_BASE", "ifes")           # ifes/<bancada>/<grandeza>
    DB_PATH = os.getenv("DB_PATH", "dashboard.db")
    DASH_TOKEN = os.getenv("DASH_TOKEN", "")                 # vazio = sem token (laboratório)
    RETENCAO_DIAS = _int("RETENCAO_DIAS", 7)                 # leituras mais antigas são apagadas
    PONTOS_INICIAIS = _int("PONTOS_INICIAIS", 120)           # histórico enviado ao abrir a página
    PURGA_A_CADA = _int("PURGA_A_CADA", 500)                 # inserções entre duas purgas
