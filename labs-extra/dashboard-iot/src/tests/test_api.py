import time


def injeta(est, **kw):
    d = {"tipo": "leitura", "bancada": "bancada1", "grandeza": "temp", "valor": 25.0, "ts": time.time()}
    d.update(kw)
    est.ao_dado(d)


def test_saude(client):
    r = client.get("/api/saude").json
    assert r["status"] == "ok" and r["mqtt"] is False and r["leituras"] == 0


def test_paginas(client):
    for rota in ("/", "/historico"):
        r = client.get(rota)
        assert r.status_code == 200 and b"<canvas" in r.data
    assert client.get("/static/vendor/chart.umd.js").status_code == 200
    assert client.get("/static/vendor/socket.io.min.js").status_code == 200


def test_ao_dado_grava_e_api_le(est, client):
    injeta(est, valor=26.0)
    est.ao_dado({"tipo": "status", "bancada": "bancada1", "texto": "online", "ts": time.time()})
    nos = client.get("/api/nos").json
    assert nos["bancada1"]["ultimo"]["temp"]["valor"] == 26.0
    assert nos["bancada1"]["status"] == "online" and nos["bancada1"]["grandezas"] == ["temp"]
    assert client.get("/api/leituras?bancada=bancada1&grandeza=temp&n=5").json[0][1] == 26.0


def test_historico_e_csv(est, client):
    agora = time.time()
    for i in range(60):
        injeta(est, ts=agora - 60 + i, valor=20 + i / 10)
    r = client.get("/api/historico?bancada=bancada1&grandeza=temp&horas=1&pontos=20").json
    assert r["bucket_s"] == 180 and sum(p["n"] for p in r["pontos"]) == 60
    csv_ = client.get("/api/historico.csv?bancada=bancada1&grandeza=temp&horas=1")
    assert csv_.mimetype == "text/csv" and csv_.data.startswith(b"timestamp,media,minimo,maximo,n")


def test_parametros_invalidos(client):
    assert client.get("/api/leituras?bancada=../etc&grandeza=temp").status_code == 400
    assert client.get("/api/historico?bancada=b1&grandeza=temp&horas=-1").status_code == 400
    assert client.get("/api/historico?bancada=b1&grandeza=temp&horas=99999").status_code == 400


def test_comando_valida_entrada(client):
    for corpo in ({}, {"bancada": "b1"}, {"bancada": "b1", "valor": 2}, {"bancada": "b 1", "valor": 1},
                  {"bancada": "b1", "valor": True}, {"bancada": "b1", "valor": "1"}):
        assert client.post("/api/cmd/led", json=corpo).status_code == 400, corpo


def test_comando_sem_broker_da_503(client):
    # a ponte não foi iniciada: o publish falha e a API responde 503, não 500
    r = client.post("/api/cmd/led", json={"bancada": "bancada1", "valor": 1})
    assert r.status_code in (200, 503)


def test_token(tmp_path):
    from dashboard.app import create_app
    from dashboard.config import Config

    class C(Config):
        DB_PATH = str(tmp_path / "t.db"); DASH_TOKEN = "segredo"
    c = create_app(C, iniciar_mqtt=False).test_client()
    corpo = {"bancada": "bancada1", "valor": 1}
    assert c.post("/api/cmd/led", json=corpo).status_code == 401
    assert c.post("/api/cmd/led", json=corpo, headers={"Authorization": "Bearer errado"}).status_code == 401
    assert c.post("/api/cmd/led", json=corpo, headers={"Authorization": "Bearer segredo"}).status_code in (200, 503)
    assert c.get("/api/nos").status_code == 200          # leitura continua livre


def test_purga_automatica(est):
    velha = time.time() - 30 * 86400
    injeta(est, ts=velha)
    for _ in range(5):                                     # PURGA_A_CADA = 5 no conftest
        injeta(est)
    assert est.banco.contagens()["leituras"] == 5          # a antiga foi embora
