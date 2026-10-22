"""Modo desenvolvimento:  python -m dashboard   (produção: gunicorn, ver deploy/)."""
import logging

from .app import create_app, socketio

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s %(message)s")
app = create_app()
socketio.run(app, host="0.0.0.0", port=5003, allow_unsafe_werkzeug=True)
