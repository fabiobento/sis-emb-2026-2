import time


def test_estado_ao_conectar_e_push(app, est, sio):
    est.ao_dado({"tipo": "leitura", "bancada": "bancada1", "grandeza": "temp", "valor": 25.0, "ts": time.time()})
    cli = sio.test_client(app)
    ev = cli.get_received()
    assert ev[0]["name"] == "estado"
    snap = ev[0]["args"][0]
    assert snap["nos"]["bancada1"]["ultimo"]["temp"]["valor"] == 25.0
    assert snap["series"]["temp"]["bancada1"][0][1] == 25.0
    # push: nova leitura chega sem o cliente pedir
    est.ao_dado({"tipo": "leitura", "bancada": "bancada1", "grandeza": "temp", "valor": 26.5, "ts": time.time()})
    ev = cli.get_received()
    assert ev[0]["name"] == "leitura" and ev[0]["args"][0]["valor"] == 26.5
    est.ao_dado({"tipo": "movimento", "bancada": "bancada1", "classe": "aceno", "conf": 0.9, "ts": time.time()})
    assert cli.get_received()[0]["name"] == "movimento"
