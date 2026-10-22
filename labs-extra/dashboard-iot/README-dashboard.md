# Lab Extra — Dashboard web profissional no Raspberry Pi (Flask + Socket.IO + MQTT + SQLite)

Lab integrador **opcional/avançado** que fecha o projeto final: uma aplicação web, servida pelo
próprio RPi, que mostra **ao vivo** as leituras dos nós ESP32 (via MQTT), guarda o **histórico**
em SQLite, permite **comandar** atuadores pelo navegador e roda como **serviço de produção**
(nginx + gunicorn + systemd). Une MQTT (semana 14), Linux/systemd/GPIO (semanas 11–12) e
conceitos de redes (REST × WebSocket × MQTT) num sistema que sobrevive a reboot.

![Painel ao vivo com três bancadas simuladas](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_tela_home.png)

> As telas usam **três bancadas simuladas** (`tools/simulador_no.py`). Nós ESP32 reais, o
> `systemd` em um RPi real e o PWM de LED por `gpiozero` **não foram testados** pelo autor.

## Por onde começar

1. Leia o mapa geral, com livros e rubrica: [trilha full-stack](../../docs/trilha-fullstack.md).
2. Leia a [teoria](teoria-dashboard.md) (≈ 50 min).
3. Siga o [roteiro](lab-dashboard.md): Parte 0 → G.
4. Documente no [modelo de relatório](relatorio-modelo.md); se travar, o [guia de problemas](guia-problemas.md).

**Atalho de 5 minutos, sem hardware** (para ver o painel funcionando):

```bash
sudo apt install -y mosquitto mosquitto-clients        # se ainda não tiver
cd labs-extra/dashboard-iot
python3 -m venv venv && source venv/bin/activate && pip install -r src/requirements.txt
cd src
python3 tools/simulador_no.py --bancadas bancada1 bancada2 --periodo 1 --movimento &
python3 tools/popular_banco.py --db dashboard.db --horas 24 --periodo 10 --bancadas bancada1 bancada2
MQTT_HOST=localhost python3 -m dashboard               # abra http://localhost:5003/
```

## Conteúdo

```
dashboard-iot/
├── README-dashboard.md      este arquivo
├── teoria-dashboard.md      REST×WS×MQTT, threads, ponte, SQLite, segurança, produção; exemplos D.1–D.3
├── lab-dashboard.md         roteiro guiado, Partes 0, A–G, com todo o código embutido
├── guia-problemas.md        sintoma → causa → teste → correção (+ fluxograma de diagnóstico)
├── relatorio-modelo.md      modelo de relatório + rubrica (100 pontos)
└── src/
    ├── requirements.txt
    ├── etapa1_rest/         Etapa 1: GET/POST /led com validação (porta 5000)
    ├── etapa2_websocket/    Etapa 2: push por Socket.IO × polling (porta 5001)
    ├── etapa3_mqtt_web/     Etapa 3: ponte MQTT ↔ Web (porta 5002)
    ├── dashboard/           Etapa 4: o painel completo (pacote Python; porta 5003)
    │   ├── config.py · topicos.py · db.py · mqtt_bridge.py · app.py · wsgi.py
    │   ├── templates/       base, index (ao vivo), historico
    │   └── static/          app.js, historico.js, style.css, vendor/ (Chart.js, socket.io — sem CDN)
    ├── deploy/              Etapa 5: dashboard.service, nginx-dashboard.conf, dashboard.env,
    │                        mosquitto-lab.conf, mosquitto-seguro.conf, instalar.sh
    ├── tools/               simulador_no.py · popular_banco.py · medir_trafego.py (+ trafego_medido.json)
    └── tests/               42 testes: tópicos, banco, API, Socket.IO e ponta a ponta com Mosquitto real
```

## Portas usadas

A · 5000 — B · 5001 — C · 5002 — painel · 5003 — gunicorn · 8000 (interno) — nginx · 80 — MQTT · 1883.

## Licença e uso

Material didático do curso. O Chart.js (MIT) e o socket.io-client (MIT) em `static/vendor/` são
redistribuídos conforme suas licenças.
