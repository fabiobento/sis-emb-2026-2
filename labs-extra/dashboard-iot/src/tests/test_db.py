from dashboard.db import Banco


def test_inserir_e_ultimas(tmp_path):
    b = Banco(str(tmp_path / "x.db")); b.iniciar()
    for i in range(10):
        b.inserir_leitura(100 + i, "b1", "temp", 20 + i)
    assert b.ultimas("b1", "temp", 3) == [[107, 27], [108, 28], [109, 29]]     # ordem cronológica
    assert b.ultimas("b1", "umid", 3) == []


def test_historico_reamostra(tmp_path):
    b = Banco(str(tmp_path / "x.db")); b.iniciar()
    for i in range(100):                      # 1 leitura por segundo, valor = i
        b.inserir_leitura(1000 + i, "b1", "temp", i)
    h = b.historico("b1", "temp", 1000, 1099, 10)
    assert len(h) == 10 and h[0]["n"] == 10
    assert h[0]["min"] == 0 and h[0]["max"] == 9 and h[0]["media"] == 4.5


def test_nos_estado(tmp_path):
    b = Banco(str(tmp_path / "x.db")); b.iniciar()
    b.inserir_leitura(1, "b1", "temp", 20); b.inserir_leitura(5, "b1", "temp", 22)
    b.inserir_leitura(3, "b1", "umid", 60)
    b.inserir_evento(2, "b1", "status", "online"); b.inserir_evento(6, "b1", "status", "offline")
    b.inserir_evento(4, "b2", "movimento", "aceno", 0.9)
    n = b.nos()
    assert n["b1"]["ultimo"]["temp"] == {"valor": 22, "ts": 5}
    assert n["b1"]["status"] == "offline"
    assert n["b2"]["status"] == "desconhecido" and n["b2"]["movimento"]["classe"] == "aceno"


def test_purgar(tmp_path):
    b = Banco(str(tmp_path / "x.db")); b.iniciar()
    b.inserir_leitura(1000, "b1", "temp", 1)                   # muito antiga
    b.inserir_leitura(1000 + 9 * 86400, "b1", "temp", 2)       # recente
    apagadas = b.purgar(7, agora=1000 + 10 * 86400)
    assert apagadas == 1 and b.contagens()["leituras"] == 1


def test_sql_injection_nao_funciona(tmp_path):
    b = Banco(str(tmp_path / "x.db")); b.iniciar()
    b.inserir_leitura(1, "b1", "temp", 1)
    assert b.ultimas("b1'; DROP TABLE leituras;--", "temp", 5) == []
    assert b.contagens()["leituras"] == 1                       # a tabela continua lá (consulta parametrizada)
