import pytest

from dashboard.topicos import interpreta

T = 1000.0


def test_leitura_numerica():
    d = interpreta("ifes", "ifes/bancada3/temp", b"27.5", T)
    assert d == {"tipo": "leitura", "bancada": "bancada3", "grandeza": "temp", "valor": 27.5, "ts": T}


def test_status_online_offline():
    assert interpreta("ifes", "ifes/bancada3/status", b"online", T)["texto"] == "online"
    assert interpreta("ifes", "ifes/bancada3/status", b"offline", T)["texto"] == "offline"
    assert interpreta("ifes", "ifes/bancada3/status", b"talvez", T) is None


def test_movimento_json():
    d = interpreta("ifes", "ifes/b1/movimento", b'{"classe":"aceno","conf":0.93,"t":123}', T)
    assert d["tipo"] == "movimento" and d["classe"] == "aceno" and d["conf"] == 0.93


@pytest.mark.parametrize("payload", [b"nao-json", b'{"classe":"aceno"}', b'{"classe":"a b","conf":0.5}',
                                     b'{"classe":"aceno","conf":1.5}', b'{"classe":"aceno","conf":"x"}',
                                     b'{"classe":7,"conf":0.5}'])
def test_movimento_invalido(payload):
    assert interpreta("ifes", "ifes/b1/movimento", payload, T) is None


@pytest.mark.parametrize("topico", ["ifes/teste", "outro/b1/temp", "ifes/b1/cmd/led", "ifes/b1/movimento/info",
                                    "ifes/B1/temp", "ifes/b 1/temp", "ifes/b1/Temp", "ifes//temp", "ifes"])
def test_topicos_ignorados(topico):
    assert interpreta("ifes", topico, b"1", T) is None


@pytest.mark.parametrize("payload", [b"abc", b"", b"nan", b"inf", b"-inf", b"1,5;DROP TABLE x"])
def test_valores_invalidos(payload):
    assert interpreta("ifes", "ifes/b1/temp", payload, T) is None


def test_payload_com_espacos_e_str():
    assert interpreta("ifes", "ifes/b1/umid", " 61 \n", T)["valor"] == 61.0
