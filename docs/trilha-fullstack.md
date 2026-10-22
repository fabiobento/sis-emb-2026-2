# Trilha opcional — Full Stack IoT no Raspberry Pi (dashboard web profissional)

Guia de estudo dirigido para grupos que queiram um **dashboard web profissional** no projeto
final, combinando dois livros do acervo da turma com um **código de referência testado** que
vive no repositório. Pré-requisitos: semanas 11–12 (Linux/GPIO/systemd) e 14 (MQTT). Tudo roda
no RPi 3 do laboratório — e, para desenvolver, em qualquer PC com Python 3.10+.

> 🗺️ **Esta página é o mapa.** O detalhamento passo a passo, com todo o código, está em
> [`labs-extra/dashboard-iot/`](../labs-extra/dashboard-iot/README-dashboard.md):
> [teoria](../labs-extra/dashboard-iot/teoria-dashboard.md) ·
> [roteiro do lab](../labs-extra/dashboard-iot/lab-dashboard.md) ·
> [guia de problemas](../labs-extra/dashboard-iot/guia-problemas.md) ·
> [modelo de relatório](../labs-extra/dashboard-iot/relatorio-modelo.md). Leia o mapa até o fim,
> depois abra o roteiro e trabalhe etapa por etapa.

## O que você vai construir

Nas semanas 13–14, a arquitetura do projeto final ficou assim: os nós ESP32 medem e atuam, o
RPi hospeda o broker MQTT e concentra os dados (Fig. 14-A da teoria-14, reproduzida abaixo).
Esta trilha adiciona a última camada: **uma aplicação web servida pelo próprio RPi**, que lê do
barramento MQTT e apresenta leituras em tempo real, histórico em gráficos e botões de comando
no navegador de qualquer máquina da rede do laboratório.

![Arquitetura ESP32 + RPi: a trilha adiciona a camada web sobre o broker que já existe](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/arquitetura_esp32_rpi.png)
*Figura T-1 — A arquitetura da semana 14. A trilha full-stack constrói a peça que falta: do
broker MQTT para o navegador. Fonte: diagrama da apostila (teoria-14, Fig. 14-A).*

O resultado final é este painel (capturado do código de referência, com três bancadas
**simuladas** publicando por MQTT):

![Painel ao vivo: cartões por bancada, gráfico em tempo real e lista de eventos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_tela_home.png)
*Figura T-2 — Página "Ao vivo": estado de cada nó (online/offline por LWT), última leitura, classe
de movimento da [trilha TinyML](trilha-tinyml.md), botões de LED, gráfico em tempo real e eventos.*

### A arquitetura por dentro

![Arquitetura do painel, do sensor ao navegador, com o caminho do comando de volta](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_arquitetura.png)
*Figura T-3 — Seis fluxos numerados. Os de **dados** (1→2→3) vão do sensor à tela; o de
**comando** (4→5→MQTT) faz o caminho inverso. O ESP32 só conhece MQTT: nunca "sabe" que existe web.*

Cada tecnologia aparece **onde é a escolha certa** — é a regra da casa do Exemplo resolvido 14.4:

| Trecho | Tecnologia | Por quê |
|---|---|---|
| ESP32 ↔ broker ↔ ponte | **MQTT** | leve, desacoplado, com QoS e *last will* |
| servidor → navegador (dados novos) | **WebSocket** (Socket.IO) | o servidor empurra no instante em que o dado existe |
| navegador → servidor (comando, histórico) | **REST** (HTTP/JSON) | esporádico, cacheável, testável com `curl` |
| histórico | **SQLite** | um arquivo, sem servidor, consultável por SQL |

## Roteiro de seis etapas

![Seis etapas cumulativas, cada uma com um teste de aceitação](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_roadmap.png)
*Figura T-4 — As etapas são cumulativas. Cada uma termina com algo funcionando e demonstrável.*

| Etapa | Entrega | Pasta de referência (em `labs-extra/dashboard-iot/src/`) | Sessões |
|---|---|---|---|
| 0 | venv, broker, simulador de nós rodando | `requirements.txt`, `tools/simulador_no.py` | 1 |
| 1 | API REST controlando um LED | `etapa1_rest/` | 1 |
| 2 | página "ao vivo" por WebSocket + medição do polling | `etapa2_websocket/` | 1 |
| 3 | ponte MQTT ↔ Web (dados e comandos) | `etapa3_mqtt_web/` | 1–2 |
| 4 | dashboard completo: SQLite, histórico, CSV, segurança | `dashboard/` | 2–3 |
| 5 | produção: gunicorn + nginx + systemd, sobrevive a reboot | `deploy/` | 1–2 |

> 🧭 **Como usar esta trilha:** as etapas são cumulativas — não pule. Ao final de cada uma, você
> deve ter algo **funcionando e demonstrável** (não apenas lido). O checklist "✅ Chegamos!" de
> cada etapa é o que vale na rubrica. **Escreva o seu código** seguindo a explicação; o código de
> referência serve para comparar quando algo não funcionar — copiar sem entender não passa na
> defesa oral.

> 🧪 **Sem ESP32 na mão?** Tudo roda **sem hardware**: `tools/simulador_no.py` publica no broker
> exatamente os mesmos tópicos que o firmware da semana 14 (inclusive `status` com *last will* e,
> opcionalmente, `movimento` da trilha TinyML). Você só liga os nós reais na demonstração final.

As siglas dos livros:
- **Smart** = *Practical Python Programming for IoT* (Gary Smart, Packt) — está no acervo da
  disciplina em PDF;
- **Dalmaris** = *Raspberry Pi 4 OR 5 AND Pico: Cool Projects for Test, Measurement, and
  Control* (Elektor) — também no acervo.

O contrato de tópicos que todas as etapas usam (o mesmo da semana 14):

```text
ifes/<bancada>/<grandeza>        número em texto   ex.: ifes/bancada1/temp   26.8
ifes/<bancada>/status            online | offline  (retain + last will)
ifes/<bancada>/movimento         {"classe":"aceno","conf":0.93}   (trilha TinyML)
ifes/<bancada>/cmd/led           0 | 1   (o dashboard publica, QoS 1; o ESP32 assina)
```

## Etapa 0 — Ambiente Python correto
`venv` por projeto (nunca instalar com sudo pip no sistema):
**Smart, cap. 1 (p. 20–35; Figura 1.1, p. 25)**. Padrão adotado em todos os labs do Bloco 2.

**Por que isso existe:** o Raspberry Pi OS já traz um Python de sistema que o próprio sistema
operacional usa. Instalar pacotes nele com `sudo pip` mistura suas dependências com as do
sistema e é a receita clássica para quebrar o RPi numa atualização. A `venv` cria uma pasta
isolada por projeto, com seu próprio `pip` e suas próprias versões — é o equivalente Python ao
que o `idf.py` já faz por você no mundo ESP-IDF. Relembrando o que o Lab 11 já praticou:

```bash
cd ~/sis-emb-2026-2/labs-extra/dashboard-iot
python3 -m venv venv                  # cria o ambiente (pasta venv/ ao lado de src/)
source venv/bin/activate              # "entra" nele (o prompt muda)
pip install -r src/requirements.txt   # instala só dentro da venv
sudo apt install -y mosquitto mosquitto-clients   # o broker (se ainda não tiver da semana 14)
```

Para ter dados sem hardware, deixe o simulador rodando em um segundo terminal:

```bash
cd src && python3 tools/simulador_no.py --bancadas bancada1 bancada2 --periodo 1 --movimento
mosquitto_sub -h localhost -t 'ifes/#' -v      # terceiro terminal: você deve VER as mensagens
```

✅ **Chegamos!** — `which python` dentro da venv aponta para `.../venv/bin/python`;
`mosquitto_sub` mostra `ifes/bancada1/temp 26.7` rolando a cada segundo.

## Etapa 1 — API REST no RPi (Flask)
O RPi expõe leituras e comandos por HTTP/JSON: `GET /led`, `POST /led {"level": 50}`.
**Smart, cap. 3, seção RESTful APIs (p. 60–85; Figura 3.1, p. 82)**. Teste com `curl` e pelo
navegador do PC do laboratório.

**A ideia em um parágrafo:** REST é o modelo "pergunta–resposta" da web. Cada recurso (um LED,
uma leitura de sensor) vira uma URL; os verbos HTTP dizem o que fazer com ele (`GET` lê, `POST`
muda); os dados viajam em JSON, que é texto puro e legível. O Flask é o framework mínimo que
transforma funções Python nesses endpoints — você já viu a essência dele no Exemplo resolvido
14.4 da teoria-14, na comparação REST × WebSocket × MQTT.

```python
# etapa1_rest/app.py (resumo) — o "hello world" REST do nosso laboratório
from flask import Flask, jsonify, request
app = Flask(__name__)
estado = {"level": 0}

@app.get("/led")
def ler_led():
    return jsonify(estado)

@app.post("/led")
def escrever_led():
    level = (request.get_json(silent=True) or {}).get("level")
    if not isinstance(level, (int, float)) or isinstance(level, bool) or not 0 <= level <= 100:  # NUNCA confie na entrada
        return jsonify(erro="level deve estar entre 0 e 100"), 400
    estado["level"] = level                       # aqui entra o gpiozero/PWM
    return jsonify(estado)

app.run(host="0.0.0.0", port=5000)   # 0.0.0.0 = aceita conexões de outros PCs da rede
```

Teste de outra máquina (ou do próprio RPi):
```bash
curl http://<ip-do-rpi>:5000/led
curl -X POST -H "Content-Type: application/json" -d '{"level": 50}' http://<ip-do-rpi>:5000/led
curl -X POST -H "Content-Type: application/json" -d '{"level": 500}' http://<ip-do-rpi>:5000/led   # 400!
```

✅ **Chegamos!** — um LED real no GPIO do RPi muda de brilho via `curl` do PC (encaixe aqui o
`gpiozero.PWMLED` da semana 12; sem hardware, o código de referência só imprime o nível). Uma
entrada inválida devolve **400** e não muda o estado. Anote o tráfego gerado por um polling a
2 Hz — você vai compará-lo na Etapa 2.

## Etapa 2 — Dashboard "ao vivo" (WebSockets)
Substitua o *polling* por push: **Smart, cap. 3, seção Web Sockets (p. 86–100; Figura 3.2,
p. 97)**. Compare o tráfego das duas versões (Exemplo resolvido 14.4 da teoria).

**A ideia em um parágrafo:** com REST, quem quer dados novos precisa perguntar o tempo todo
(polling) — cada pergunta carrega cabeçalhos HTTP completos, e a latência média é metade do
período de polling. WebSocket abre **uma** conexão TCP persistente e bidirecional: depois do
aperto de mãos inicial, o servidor **empurra** cada leitura nova no instante em que ela existe,
com frames de poucos bytes. No Python, a rota simples é o `Flask-SocketIO`; no navegador, o
cliente `socket.io` em JavaScript.

![Polling pergunta sempre; WebSocket é avisado quando há dado novo](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_sequencia.png)
*Figura T-5 — Diagrama de sequência das duas técnicas.*

**Medimos, não só afirmamos.** O script `tools/medir_trafego.py` põe um proxy TCP contador entre
o cliente e o painel (3 bancadas publicando 1 amostra/s, 60 s de medição):

![Barras: polling 2062 B/s contra WebSocket 707 B/s](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_trafego.png)
*Figura T-6 — No nosso teste, o polling a 2 Hz gastou ≈ 2,9× mais bytes que o WebSocket **e** ainda
atrasa a tela em até 0,5 s. A diferença cresce com mais bancadas/grandezas por tela ou com
cabeçalhos maiores (cookies, autenticação). Medida em loopback, só bytes de aplicação — sem os
cabeçalhos TCP/IP. **Meça no seu cenário e reporte o seu número.***

> 💡 **Conexão com a disciplina:** a escolha polling × push é a mesma da semana 4 (polling ×
> interrupção) em outra escala — trocar "ficar perguntando" por "ser avisado" economiza recurso
> e reduz latência nos dois mundos.

✅ **Chegamos!** — uma página HTML servida pelo RPi mostra um valor que se atualiza sozinho
(sem F5, sem `setInterval` de polling) e **duas abas abertas ficam sincronizadas**. Você tem a
tabela "polling × push" medida por você.

## Etapa 3 — MQTT ↔ Web
Ponte entre o barramento de nós ESP32 e a aplicação web: **Smart, cap. 4 (p. 101–130;
Figuras 4.1–4.2, p. 113–115)** — mesmo paho-mqtt do nosso `semana-14/src/logger_rpi.py`.

**A ideia em um parágrafo:** até aqui, sua aplicação web só enxerga o próprio RPi. Agora ela
vira **mais um cliente MQTT** do broker local (o Mosquitto do Lab 14): assina os tópicos dos
nós ESP32 e republica cada mensagem recebida nos clientes WebSocket conectados. O padrão é
exatamente o do `logger_rpi.py` da semana 14 — callback `on_message` — só que, em vez de gravar
num arquivo, o callback empurra o dado para o navegador:

```python
import paho.mqtt.client as mqtt
# dentro do callback: repassa para os navegadores via SocketIO
def on_message(client, userdata, msg):
    socketio.emit("leitura", {"topico": msg.topic, "valor": msg.payload.decode()})
```

Os comandos fluem no sentido contrário: o botão do dashboard dispara um `client.publish()` no
tópico de comando do nó (com o QoS justificado na sua Lista 05, Q8–Q9). Pronto: o ESP32 nem
"sabe" que existe web — ele continua falando só MQTT. Esse desacoplamento é a vantagem nº 1 do
publish–subscribe (Lista 05, Q7) funcionando na prática.

**Quatro cuidados que separam "funciona" de "aguenta o laboratório"** (todos explicados na
[teoria](../labs-extra/dashboard-iot/teoria-dashboard.md) e cobertos por testes):
1. **`client_id` único** — dois clientes com o mesmo id se derrubam em loop;
2. **assinar dentro de `on_connect`** — a assinatura some quando a sessão cai; reassine a cada conexão;
3. **callbacks curtos e com `try/except`** — uma mensagem malformada não pode matar a thread do MQTT;
4. **validar o payload** — `float("nan")` passa em `float()`; um nó defeituoso não deve quebrar o gráfico.

> 💡 **Ideia de integração:** se o seu grupo também fez a [trilha TinyML](trilha-tinyml.md), o tópico
> `ifes/<bancada>/movimento` (JSON `{"classe":…,"conf":…}`) é só mais um tópico para o dashboard
> mostrar como etiqueta em tempo real — o código de referência já faz isso.

✅ **Chegamos!** — girar o potenciômetro no ESP32 (ou o `simulador_no.py`) move um gráfico no
navegador em tempo real, e um botão no navegador publica no tópico `…/cmd/led` — confirmado com
`mosquitto_sub -t 'ifes/+/cmd/#' -v` — tudo via broker.

## Etapa 4 — O dashboard completo (SQLite, histórico, segurança)
Aqui o "protótipo de uma página" vira **aplicação**: pacote Python organizado, configuração por
variáveis de ambiente, banco de dados, duas páginas, API documentada e testes automáticos. Livro de
apoio: **Dalmaris** — "Sensor → banco → página" (cap. 58+, p. 185 em diante; **Figura 58.76,
p. 188**), templates Jinja (caps. 50–54, p. 158–175; **Figura 54.72, p. 174**) e gráficos
(Google Charts, caps. 70–72, p. 225–236, ou Plotly, caps. 81–84, p. 260–276). **Nossa
implementação usa Chart.js servido localmente** — veja a nota de projeto abaixo.

![Tópico MQTT → função interpreta() → tabela](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_topicos.png)
*Figura T-7 — Cada mensagem passa por uma função **pura** (`interpreta`, sem rede nem banco) que
decide se ela é leitura, status, movimento ou lixo. Ser pura é o que a torna fácil de testar.*

O que a Etapa 4 acrescenta, e por quê:

| Peça | Arquivo | Ideia central |
|---|---|---|
| Configuração | `dashboard/config.py` | senha e IP **fora do código** (variáveis de ambiente; 12-factor) |
| Interpretação | `dashboard/topicos.py` | validar por regex/limites; ignorar o que não é do contrato |
| Persistência | `dashboard/db.py` | SQLite em modo WAL, SQL **parametrizado**, retenção |
| Ponte | `dashboard/mqtt_bridge.py` | cliente paho robusto; publica comandos QoS 1 |
| Aplicação | `dashboard/app.py` | rotas REST + eventos Socket.IO + autenticação por token |
| Front-end | `templates/`, `static/` | Chart.js, cartões, tema claro/escuro, layout de celular |

**Histórico com *downsampling* no banco.** 24 h a 0,1 Hz são ~8.600 pontos por série — demais
para enviar e desenhar. O truque é reduzir **no SQL** (`GROUP BY` por balde de tempo, com média,
mínimo e máximo), não no navegador:

![24 h de dados: 9.092 pontos brutos contra 289 pontos de balde de 5 min](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_downsampling.png)
*Figura T-8 — Dados do banco de demonstração (**sintéticos**). A faixa mín–máx preserva os picos
que a média sozinha esconderia.*

![Página Histórico: média e faixa mín–máx, seletor de bancada/grandeza/período, botão CSV](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_tela_hist.png)
*Figura T-9 — Página "Histórico" (dados da demonstração). "Baixar CSV" exporta exatamente a
série reamostrada, para a planilha do relatório.*

**Segurança em camadas** — o painel comanda hardware, então trate-o como tal:

![Cinco camadas: navegador, nginx, Flask, SQLite, Mosquitto](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_seguranca.png)
*Figura T-10 — Em laboratório, as camadas Flask e SQLite já vêm ligadas (validação, SQL
parametrizado, token opcional). As demais são "nota de produção" para o relatório.*

✅ **Chegamos!** — (a) `python3 -m pytest tests -q` passa; (b) você derruba o simulador de uma
bancada e o cartão dela fica **OFFLINE** em poucos segundos (last will); (c) reinicia o painel e o
histórico **continua lá**; (d) `curl` sem token no `POST /api/cmd/led` devolve **401** quando
`DASH_TOKEN` está definido.

## Etapa 5 — Pilha de produção: nginx + gunicorn + Flask + SQLite + systemd
Até aqui você usou o servidor de desenvolvimento do Flask (aquele que avisa *"do not use it in
a production deployment"*). Esta etapa veste a aplicação para o mundo real, camada por camada.
A sequência do **Dalmaris** (capítulos curtos, um por camada — siga na ordem):
- Visão da pilha: **Figura 40.51, cap. 40, p. 131** (o diagrama-guia da trilha inteira).
- nginx: caps. 40–44 (p. 130–141); "Hello Flask" atrás do servidor de aplicação: caps. 45–46
  (p. 142–147; **Figura 46.59, p. 147**).

![nginx na porta 80, gunicorn em 127.0.0.1:8000, Flask, SQLite e Mosquitto, todos sob o systemd](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_producao.png)
*Figura T-11 — A nossa pilha. Compare com a Figura 40.51 do Dalmaris: a única troca é o servidor
de aplicação.*

**Quem faz o quê na pilha:**
- **nginx** — a portaria: atende na porta 80, repassa as requisições para dentro e, na rota
  `/socket.io/`, **transforma a conexão em WebSocket** (cabeçalhos `Upgrade`/`Connection`);
- **gunicorn** — o tradutor: converte as requisições do nginx em chamadas Python (protocolo WSGI)
  e supervisiona a aplicação;
- **Flask** — a sua aplicação: as rotas das Etapas 1–4;
- **SQLite** — o histórico: um banco SQL inteiro num único arquivo, sem servidor separado;
- **systemd** — o síndico: inicia tudo no boot e reinicia o painel se ele cair (semana 11).

> ⚙️ **Nota de projeto — por que gunicorn e não uWSGI (como no Dalmaris)?** O livro usa
> nginx + uWSGI, que é perfeito para páginas "pergunta–resposta". Nosso painel mantém
> **WebSockets abertos**, e o uWSGI só os atende bem com *gevent* e opções específicas — mais
> peças para aprender e depurar. O gunicorn com *threads* (`-k gthread`) cumpre o mesmo papel
> com uma linha de comando. A arquitetura da Figura 40.51 **continua valendo**; troque uma caixa.
> *(O caminho com uWSGI não foi testado pelo autor deste material.)*
>
> **Por que `-w 1` (um worker só)?** Cada processo abriria o **seu** cliente MQTT — cada
> mensagem seria gravada várias vezes — e o Socket.IO exige que o navegador fale sempre com o
> mesmo processo. O paralelismo vem das threads (`--threads 100`). Esta é a pegadinha nº 1 de
> quem escala o painel "só aumentando os workers".

**Instalação automática (e entendendo o que ela faz):** `deploy/instalar.sh` executa os passos
manuais que o roteiro do lab detalha — pacotes, venv, Mosquitto, `dashboard.service`, site do
nginx, `systemctl enable --now`. Leia o script **antes** de rodar; no relatório você explica
cada linha.

✅ **Chegamos!** — o dashboard sobrevive a **reboot do RPi** (`sudo reboot`, espere, abra
`http://<ip-do-rpi>/` sem número de porta), mostra gráfico do histórico gravado no SQLite, e
`systemctl status dashboard` mostra `active (running)`. Se matar o processo
(`sudo pkill -f gunicorn`), o systemd o ressuscita em ~3 s.

## Etapa 6 (opcional) — Integrações externas
- IFTTT/webhooks e ThingSpeak: **Smart, cap. 13 (p. 400–427)** — mande um alerta para o celular
  quando um valor passar do limiar, ou publique um canal público de telemetria sem abrir porta
  nenhuma no laboratório.
- Projeto integrador de referência ("IoTree", com REST + MQTT + dweet): **Smart, cap. 14
  (p. 435–460; Figura 14.3, p. 442 e Figura 14.5, p. 447)** — diagramas de arquitetura
  para imitar no relatório do projeto final.
- Domótica pronta: *Building Smart Home Automation Solutions with Home Assistant* (acervo) —
  para grupos cujo projeto é domótico e querem integrar com uma plataforma pronta em vez de
  construir o dashboard do zero.

## Desafios para ir além (escolha 1–2, valem pontos de "qualidade técnica")
1. **Alarme com histerese:** quando `temp` passar de um limiar, destaque o cartão e grave um
   evento `alarme`; só normalize após cair 1 °C abaixo (evita "pisca-pisca").
2. **Nova grandeza:** acrescente `dist` (HC-SR04) ou `luz` e veja o painel listá-la **sem mudar o
   Python** (o seletor se monta a partir do que existe no banco).
3. **Segundo atuador:** `ifes/<bancada>/cmd/rele` — generalize `publica_led` e a rota.
4. **Login real:** troque o token único por usuário/senha (`auth_basic` no nginx) e registre quem
   mandou cada comando.
5. **Mosquitto seguro:** use `deploy/mosquitto-seguro.conf` (usuário/senha) e ajuste
   `MQTT_USER`/`MQTT_PASS` — os firmwares também precisam das credenciais.
6. **Latência fim a fim:** meça do `publish` no ESP32 até o pixel mudar (câmera lenta do
   celular ou timestamps) e compare com a estimativa do Exemplo 14.4.

## Entregáveis e rubrica da trilha
A trilha é **opcional** e conta em "qualidade técnica" e "documentação" do projeto final.

| Item | Evidência exigida |
|---|---|
| Etapas 1–3 funcionando | demonstração ao vivo + `curl`/`mosquitto_sub` na tela |
| Medição polling × push | tabela com **seus** números e a conclusão |
| Etapa 4 | testes passando; reinício preserva histórico; offline por LWT |
| Etapa 5 | `systemctl status`, `sudo reboot` e painel volta; `nginx -t` |
| Relatório | [relatorio-modelo.md](../labs-extra/dashboard-iot/relatorio-modelo.md) preenchido, com limitações honestas |

## Observações do professor
- O Dalmaris usa RPi 4 e o Smart usa RPi 4/venv — **tudo funciona igual no nosso RPi 3**
  (apenas mais lento); onde os livros citarem `raspi-config`/caminhos antigos, use as notas do
  nosso `docs/instalacao.md`.
- Grupos nesta trilha podem substituir a "Parte C" do Lab 14 pelo dashboard Flask.
- Se faltar tempo, priorize as Etapas 1–3: um dashboard WebSocket alimentado por MQTT já é um
  projeto completo. As Etapas 4–5 são o que diferencia "funciona na minha sessão SSH" de "produto".
- **O que foi e o que não foi testado (transparência):** o código Python tem **42 testes
  automáticos** que passam (inclui broker Mosquitto real, comando de ida e volta e *last will*), e
  a pilha nginx + gunicorn foi testada com WebSocket e token no PC. **Não foram testados** pelo
  autor: o `systemd` e o `instalar.sh` em um Raspberry Pi real, nós ESP32 reais (usou-se o
  simulador), PWM de LED por `gpiozero` no hardware e o caminho com uWSGI. As capturas de tela e o
  histórico dos gráficos usam **dados simulados/sintéticos**. Se algo divergir no seu RPi, o
  [guia de problemas](../labs-extra/dashboard-iot/guia-problemas.md) é o primeiro lugar a olhar —
  e vale um *issue* para corrigirmos o material.
