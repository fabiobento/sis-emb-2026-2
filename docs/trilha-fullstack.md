# Trilha opcional — Full Stack IoT no Raspberry Pi (para projetos finais)

Guia de estudo dirigido para grupos que queiram um **dashboard web profissional** no projeto
final, combinando dois livros do acervo da turma. Pré-requisitos: semanas 11–12 (Linux/GPIO) e
14 (MQTT). Tudo roda no RPi 3 do laboratório.

## O que você vai construir

Nas semanas 13–14, a arquitetura do projeto final ficou assim: os nós ESP32 medem e atuam, o
RPi hospeda o broker MQTT e concentra os dados (Fig. 14-A da teoria-14, reproduzida abaixo).
Esta trilha adiciona a última camada: **uma aplicação web servida pelo próprio RPi**, que lê do
barramento MQTT e apresenta leituras em tempo real, histórico em gráficos e botões de comando
no navegador de qualquer máquina da rede do laboratório.

![Arquitetura ESP32 + RPi: a trilha adiciona a camada web sobre o broker que já existe](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/arquitetura_esp32_rpi.png)
*Figura T-1 — A arquitetura da semana 14. A trilha full-stack constrói a peça que falta: do
broker MQTT para o navegador. Fonte: diagrama da apostila (teoria-14, Fig. 14-A).*

Cada etapa abaixo explica **o que** fazer e **por quê**, com a teoria mínima para entender a
peça que está sendo encaixada; os ponteiros de livro/página são o aprofundamento (lá estão os
códigos completos comentados). As siglas dos livros:
- **Smart** = *Practical Python Programming for IoT* (Gary Smart, Packt) — está no acervo da
  disciplina em PDF;
- **Dalmaris** = *Raspberry Pi 4 OR 5 AND Pico: Cool Projects for Test, Measurement, and
  Control* (Elektor) — também no acervo.

> 🧭 **Como usar esta trilha:** as etapas são cumulativas — não pule. Reserve ~2 sessões de
> laboratório por etapa. Ao final de cada uma, você deve ter algo **funcionando e demonstrável**
> (não apenas lido). O checklist de "chegamos!" de cada etapa é o que vale na rubrica.

## Etapa 0 — Ambiente Python correto
`venv` por projeto (nunca instalar com sudo pip no sistema):
**Smart, cap. 1 (p. 20–35; Figura 1.1, p. 25)**. Padrão adotado em todos os labs do Bloco 2.

**Por que isso existe:** o Raspberry Pi OS já traz um Python de sistema que o próprio sistema
operacional usa. Instalar pacotes nele com `sudo pip` mistura suas dependências com as do
sistema e é a receita clássica para quebrar o RPi numa atualização. A `venv` cria uma pasta
isolada por projeto, com seu próprio `pip` e suas próprias versões — é o equivalente Python ao
que o `idf.py` já faz por você no mundo ESP-IDF. Relembrando o que o Lab 11 já praticou:

```bash
cd ~
python3 -m venv dashboard-venv        # cria o ambiente
source dashboard-venv/bin/activate    # "entra" nele (prompt muda)
pip install flask paho-mqtt           # instala só dentro da venv
deactivate                            # "sai" quando terminar
```

✅ **Chegamos!** — `which python` dentro da venv aponta para
`/home/aluno/dashboard-venv/bin/python`, e `pip freeze` lista só o que você instalou.

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
# app.py — o "hello world" REST do nosso laboratório
from flask import Flask, jsonify, request
app = Flask(__name__)
estado = {"level": 0}

@app.get("/led")
def ler_led():
    return jsonify(estado)

@app.post("/led")
def escrever_led():
    estado["level"] = request.json["level"]   # aqui entraria o gpiozero/PWM
    return jsonify(estado)

app.run(host="0.0.0.0", port=5000)   # 0.0.0.0 = aceita conexões de outros PCs da rede
```

Teste de outra máquina (ou do próprio RPi):
```bash
curl http://<ip-do-rpi>:5000/led
curl -X POST -H "Content-Type: application/json" -d '{"level": 50}' http://<ip-do-rpi>:5000/led
```

✅ **Chegamos!** — um LED real no GPIO do RPi muda de brilho via `curl` do PC (encaixe aqui o
`gpiozero.PWMLED` da semana 12). Anote o tráfego gerado por um polling a 2 Hz — você vai
compará-lo na Etapa 2.

## Etapa 2 — Dashboard "ao vivo" (WebSockets)
Substitua o *polling* por push: **Smart, cap. 3, seção Web Sockets (p. 86–100; Figura 3.2,
p. 97)**. Compare o tráfego das duas versões (Exemplo resolvido 14.4 da teoria).

**A ideia em um parágrafo:** com REST, quem quer dados novos precisa perguntar o tempo todo
(polling) — cada pergunta carrega cabeçalhos HTTP completos, e a latência média é metade do
período de polling. WebSocket abre **uma** conexão TCP persistente e bidirecional: depois do
aperto de mãos inicial, o servidor **empurra** cada leitura nova no instante em que ela existe,
com frames de poucos bytes. Para um dashboard a 2 Hz, é a diferença entre ~1,5 kB/s de
cabeçalho repetido e algumas dezenas de bytes por atualização (a conta exata está no Exemplo
14.4). No Python, a rota simples é o `Flask-SocketIO`; no navegador, o cliente `socket.io` em
JavaScript.

> 💡 **Conexão com a disciplina:** a escolha polling × push é a mesma da semana 4 (polling ×
> interrupção) em outra escala — trocar "ficar perguntando" por "ser avisado" economiza recurso
> e reduz latência nos dois mundos.

✅ **Chegamos!** — uma página HTML servida pelo RPi mostra um valor que se atualiza sozinho
(sem F5, sem `setInterval` de polling) quando você publica um valor novo.

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

✅ **Chegamos!** — girar o potenciômetro no ESP32 move um gráfico no navegador em tempo real, e
um botão no navegador acende o LED do ESP32 — tudo via broker.

## Etapa 4 — Pilha de produção: nginx + uWSGI + Flask + SQLite
Até aqui você usou o servidor de desenvolvimento do Flask (aquele que avisa *"do not use it in
a production deployment"*). Esta etapa veste a aplicação para o mundo real, camada por camada.
A sequência do **Dalmaris** (capítulos curtos, um por camada — siga na ordem):
- Visão da pilha: **Figura 40.51, cap. 40, p. 131** (o diagrama-guia da trilha inteira).
- nginx: caps. 40–44 (p. 130–141); "Hello Flask" atrás do uWSGI: caps. 45–46 (p. 142–147;
  **Figura 46.59, p. 147**).
- Templates e páginas dinâmicas: caps. 50–54 (p. 158–175; **Figura 54.72, p. 174**).
- Sensor → banco → página: caps. 58+ (p. 185 em diante; **Figura 58.76, p. 188** — o painel
  com leituras reais que é o "chegamos!" da trilha).
- Gráficos: Google Charts (caps. 70–72, p. 225–236) ou Plotly (caps. 81–84, p. 260–276).

**Quem faz o quê na pilha (para você entender o diagrama sem o livro aberto):**
- **nginx** — a portaria: atende na porta 80, serve os arquivos estáticos (HTML/CSS/JS) com
  eficiência e repassa as requisições dinâmicas para dentro;
- **uWSGI** — o tradutor: converte as requisições do nginx em chamadas Python (protocolo WSGI)
  e gerencia os processos da sua aplicação (se um morrer, ele ressuscita);
- **Flask** — a sua aplicação: a lógica das rotas das Etapas 1–3, agora renderizando templates
  Jinja (páginas HTML com buracos preenchidos pelo Python) em vez de JSON puro;
- **SQLite** — o histórico: um banco SQL inteiro num único arquivo, sem servidor separado —
  ideal no RPi. O papel dele é o do `log.csv` da semana 14, só que consultável: "dê-me as
  leituras das últimas 2 h" vira um `SELECT`, e o gráfico da página nasce dessa consulta.

✅ **Chegamos!** — o dashboard sobrevive a reboot do RPi (serviços systemd, conceito da semana
11), mostra gráfico do histórico gravado no SQLite e é acessível por `http://<ip-do-rpi>/` sem
número de porta.

## Etapa 5 (opcional) — Integrações externas
- IFTTT/webhooks e ThingSpeak: **Smart, cap. 13 (p. 400–427)** — mande um alerta para o celular
  quando um valor passar do limiar, ou publique um canal público de telemetria sem abrir porta
  nenhuma no laboratório.
- Projeto integrador de referência ("IoTree", com REST + MQTT + dweet): **Smart, cap. 14
  (p. 435–460; Figura 14.3, p. 442 e Figura 14.5, p. 447)** — diagramas de arquitetura
  para imitar no relatório do projeto final.
- Domótica pronta: *Building Smart Home Automation Solutions with Home Assistant* (acervo) —
  para grupos cujo projeto é domótico e querem integrar com uma plataforma pronta em vez de
  construir o dashboard do zero.

## Observações do professor
- O Dalmaris usa RPi 4 e o Smart usa RPi 4/venv — **tudo funciona igual no nosso RPi 3**
  (apenas mais lento); onde os livros citarem `raspi-config`/caminhos antigos, use as notas do
  nosso `docs/instalacao.md`.
- Grupos nesta trilha podem substituir a "Parte C" do Lab 14 pelo dashboard Flask.
- Rubrica: a trilha conta em "qualidade técnica" e "documentação" — não é obrigatória.
- Se faltar tempo, priorize as Etapas 1–3: um dashboard WebSocket alimentado por MQTT já é um
  projeto completo. A Etapa 4 é o que diferencia "funciona na minha sessão SSH" de "produto".
