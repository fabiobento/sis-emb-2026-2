"""Camada de persistência — SQLite (um arquivo, sem servidor).

Duas tabelas:
  leituras(ts, bancada, grandeza, valor)   séries numéricas (temp, umid, ...)
  eventos(ts, bancada, tipo, texto, extra) status online/offline, classe de movimento, comandos

Modo WAL: o escritor (thread do MQTT) e os leitores (requisições web) não se bloqueiam.
Uma conexão curta por operação: simples e seguro com threads (sqlite3 não compartilha
conexões entre threads por padrão).
"""
import sqlite3
import time
from contextlib import contextmanager

ESQUEMA = """
CREATE TABLE IF NOT EXISTS leituras (
    id       INTEGER PRIMARY KEY,
    ts       REAL NOT NULL,
    bancada  TEXT NOT NULL,
    grandeza TEXT NOT NULL,
    valor    REAL NOT NULL
);
CREATE INDEX IF NOT EXISTS ix_leituras ON leituras (bancada, grandeza, ts);
CREATE TABLE IF NOT EXISTS eventos (
    id      INTEGER PRIMARY KEY,
    ts      REAL NOT NULL,
    bancada TEXT NOT NULL,
    tipo    TEXT NOT NULL,
    texto   TEXT NOT NULL,
    extra   REAL
);
CREATE INDEX IF NOT EXISTS ix_eventos ON eventos (bancada, tipo, ts);
"""


class Banco:
    def __init__(self, caminho):
        self.caminho = caminho

    @contextmanager
    def _con(self):
        con = sqlite3.connect(self.caminho, timeout=5)
        con.row_factory = sqlite3.Row
        try:
            yield con
            con.commit()
        finally:
            con.close()

    def iniciar(self):
        with self._con() as con:
            con.execute("PRAGMA journal_mode=WAL")
            con.executescript(ESQUEMA)

    # ---------------- escrita ----------------
    def inserir_leitura(self, ts, bancada, grandeza, valor):
        with self._con() as con:
            con.execute("INSERT INTO leituras (ts,bancada,grandeza,valor) VALUES (?,?,?,?)",
                        (ts, bancada, grandeza, valor))

    def inserir_evento(self, ts, bancada, tipo, texto, extra=None):
        with self._con() as con:
            con.execute("INSERT INTO eventos (ts,bancada,tipo,texto,extra) VALUES (?,?,?,?,?)",
                        (ts, bancada, tipo, texto, extra))

    def purgar(self, dias, agora=None):
        limite = (agora or time.time()) - dias * 86400
        with self._con() as con:
            a = con.execute("DELETE FROM leituras WHERE ts < ?", (limite,)).rowcount
            b = con.execute("DELETE FROM eventos WHERE ts < ?", (limite,)).rowcount
        return a + b

    # ---------------- leitura ----------------
    def ultimas(self, bancada, grandeza, n):
        """Últimas n leituras, em ordem cronológica: [[ts, valor], ...]."""
        with self._con() as con:
            linhas = con.execute(
                "SELECT ts, valor FROM leituras WHERE bancada=? AND grandeza=? "
                "ORDER BY ts DESC LIMIT ?", (bancada, grandeza, n)).fetchall()
        return [[r["ts"], r["valor"]] for r in reversed(linhas)]

    def historico(self, bancada, grandeza, desde, ate, bucket_s):
        """Série reamostrada: média/mín/máx por balde de bucket_s segundos.

        Reduzir no BANCO (GROUP BY) e não no navegador: 24 h a 0,2 Hz são ~17 mil pontos;
        o gráfico só precisa de algumas centenas.
        """
        with self._con() as con:
            linhas = con.execute(
                "SELECT CAST(ts / :b AS INTEGER) * :b AS t, AVG(valor) AS media, "
                "MIN(valor) AS minimo, MAX(valor) AS maximo, COUNT(*) AS n "
                "FROM leituras WHERE bancada=:ban AND grandeza=:g AND ts BETWEEN :d AND :a "
                "GROUP BY 1 ORDER BY 1",
                {"b": bucket_s, "ban": bancada, "g": grandeza, "d": desde, "a": ate}).fetchall()
        return [{"t": r["t"], "media": r["media"], "min": r["minimo"], "max": r["maximo"],
                 "n": r["n"]} for r in linhas]

    def grandezas(self, bancada):
        with self._con() as con:
            return [r[0] for r in con.execute(
                "SELECT DISTINCT grandeza FROM leituras WHERE bancada=? ORDER BY 1", (bancada,))]

    def eventos_recentes(self, n=30):
        with self._con() as con:
            linhas = con.execute("SELECT ts,bancada,tipo,texto,extra FROM eventos "
                                 "ORDER BY ts DESC, id DESC LIMIT ?", (n,)).fetchall()
        return [dict(r) for r in linhas]

    def nos(self):
        """Estado atual de cada nó: último status, último valor de cada grandeza, último movimento."""
        with self._con() as con:
            bancadas = sorted({r[0] for r in con.execute(
                "SELECT bancada FROM leituras UNION SELECT bancada FROM eventos")})
            saida = {}
            for b in bancadas:
                ultimo = {}
                for r in con.execute(
                        "SELECT grandeza, valor, MAX(ts) AS ts FROM leituras "
                        "WHERE bancada=? GROUP BY grandeza", (b,)):
                    ultimo[r["grandeza"]] = {"valor": r["valor"], "ts": r["ts"]}
                st = con.execute("SELECT texto, ts FROM eventos WHERE bancada=? AND tipo='status' "
                                 "ORDER BY ts DESC, id DESC LIMIT 1", (b,)).fetchone()
                mv = con.execute("SELECT texto, extra, ts FROM eventos WHERE bancada=? AND "
                                 "tipo='movimento' ORDER BY ts DESC, id DESC LIMIT 1", (b,)).fetchone()
                saida[b] = {
                    "status": st["texto"] if st else "desconhecido",
                    "status_ts": st["ts"] if st else None,
                    "ultimo": ultimo,
                    "movimento": ({"classe": mv["texto"], "conf": mv["extra"], "ts": mv["ts"]}
                                  if mv else None),
                }
        return saida

    def contagens(self):
        with self._con() as con:
            return {"leituras": con.execute("SELECT COUNT(*) FROM leituras").fetchone()[0],
                    "eventos": con.execute("SELECT COUNT(*) FROM eventos").fetchone()[0]}
