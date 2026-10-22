"""Ponto de entrada de produção:  gunicorn -k gthread -w 1 --threads 100 dashboard.wsgi:app

ATENÇÃO: UM worker só (-w 1). Cada worker abriria o PRÓPRIO cliente MQTT e gravaria cada
mensagem várias vezes; além disso o Socket.IO precisa que o cliente fale sempre com o mesmo
processo. O paralelismo vem das threads (--threads), não de processos.
"""
import logging

from .app import create_app

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s %(message)s")
app = create_app()
