"""Ponte MQTT: o dashboard como mais um cliente do broker (padrão do logger_rpi.py, semana 14).

- assina  <base>/#  em CADA (re)conexão (a assinatura some com a sessão limpa);
- entrega cada mensagem interpretada ao callback `ao_dado` (que grava e empurra aos navegadores);
- publica comandos (QoS 1) em  <base>/<bancada>/cmd/<atuador>.
"""
import logging
import re
import socket
import time

import paho.mqtt.client as mqtt

from .topicos import interpreta

log = logging.getLogger("dashboard.mqtt")
RE_BANCADA = re.compile(r"^[a-z0-9_-]{1,32}$")


class Ponte:
    def __init__(self, cfg, ao_dado):
        self.cfg, self.ao_dado = cfg, ao_dado
        self.conectado = False
        # client_id ÚNICO: dois clientes com o mesmo id derrubam um ao outro em loop
        self.cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                               client_id=f"dashboard-{socket.gethostname()}-{int(time.time())}")
        if cfg.MQTT_USER:
            self.cli.username_pw_set(cfg.MQTT_USER, cfg.MQTT_PASS)
        self.cli.reconnect_delay_set(min_delay=1, max_delay=30)
        self.cli.on_connect = self._on_connect
        self.cli.on_disconnect = self._on_disconnect
        self.cli.on_message = self._on_message

    # ---- ciclo de vida ----
    def iniciar(self):
        self.cli.connect_async(self.cfg.MQTT_HOST, self.cfg.MQTT_PORT, keepalive=30)
        self.cli.loop_start()          # thread própria do paho (rede + reconexão automática)

    def parar(self):
        self.cli.loop_stop()
        self.cli.disconnect()

    # ---- callbacks (thread do paho: mantenha-os curtos e sem exceções não tratadas) ----
    def _on_connect(self, cli, userdata, flags, reason_code, properties=None):
        ok = not reason_code.is_failure
        self.conectado = ok
        log.info("MQTT %s (%s)", "conectado" if ok else "RECUSADO", reason_code)
        if ok:
            cli.subscribe(f"{self.cfg.TOPICO_BASE}/#", qos=0)

    def _on_disconnect(self, cli, userdata, flags, reason_code, properties=None):
        self.conectado = False
        log.warning("MQTT desconectado (%s); o paho tentará reconectar", reason_code)

    def _on_message(self, cli, userdata, msg):
        try:
            dado = interpreta(self.cfg.TOPICO_BASE, msg.topic, msg.payload, time.time())
            if dado is not None:
                self.ao_dado(dado)
        except Exception:                      # uma mensagem ruim não pode derrubar a ponte
            log.exception("erro ao processar %s", msg.topic)

    # ---- comandos ----
    def publica_led(self, bancada, valor):
        if not RE_BANCADA.match(bancada) or valor not in (0, 1):
            return False
        info = self.cli.publish(f"{self.cfg.TOPICO_BASE}/{bancada}/cmd/led", str(valor), qos=1)
        return info.rc == mqtt.MQTT_ERR_SUCCESS
