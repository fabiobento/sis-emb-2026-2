import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))     # src/ no caminho de import

from dashboard.app import create_app, socketio     # noqa: E402
from dashboard.config import Config                # noqa: E402


@pytest.fixture
def cfg(tmp_path):
    class C(Config):
        DB_PATH = str(tmp_path / "t.db")
        PURGA_A_CADA = 5
    return C


@pytest.fixture
def app(cfg):
    return create_app(cfg, iniciar_mqtt=False)


@pytest.fixture
def est(app):
    return app.extensions["dash"]


@pytest.fixture
def client(app):
    return app.test_client()


@pytest.fixture
def sio(app):
    return socketio
