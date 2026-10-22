# Guia de problemas — Dashboard web (Flask + Socket.IO + MQTT + SQLite + nginx)

Tabelas de **sintoma → causa provável → como testar → correção**, na ordem em que os problemas
costumam aparecer. Os diagnósticos vêm de erros **reais** que apareceram ao desenvolver e testar
o material (marcados com 🔧) e de erros comuns de integração (os demais são causas prováveis, não reproduzidas por nós). O que **não** foi testado em
Raspberry Pi real está dito no [lab](lab-dashboard.md); se o seu RPi divergir, vale um *issue*.

> 🔎 **Método:** o dado atravessa seis saltos — **ESP32 → broker → ponte → SQLite → Socket.IO →
> navegador**. Descubra **em qual salto** ele some antes de mudar qualquer coisa, e mude **uma
> coisa por vez**. O fluxograma abaixo resume o caminho; cada linha tem um comando que responde "sim"
> ou "não".

![Fluxograma de diagnóstico: onde o dado parou?](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_diagnostico.png)
*Figura G-1 — Cinco perguntas, nesta ordem. A primeira que responder "não" diz onde procurar.*

## 1. Ambiente (venv, pacotes, testes)

| Sintoma | Causa provável | Como testar | Correção |
|---|---|---|---|
| `error: externally-managed-environment` | `pip` fora de uma venv (Raspberry Pi OS Bookworm protege o Python do sistema) | `which pip` | `source venv/bin/activate` antes do `pip`; **não** use `--break-system-packages` no RPi |
| `ModuleNotFoundError: flask_socketio` | venv não ativada, ou pacote instalado em outra venv | `which python` deve apontar para `…/venv/bin/python` | ativar a venv e `pip install -r src/requirements.txt` |
| `ModuleNotFoundError: dashboard` | rodou `python3 -m dashboard` fora de `src/` | `pwd` | `cd …/dashboard-iot/src` |
| `pytest`: `42 passed` vira `… skipped` | sem `mosquitto` no PATH: o teste ponta a ponta é pulado | `which mosquitto` | `sudo apt install mosquitto` |
| 🔧 teste ponta a ponta **falha de vez em quando** | teste correndo à frente das *threads* (condição de corrida) | rodar 10× seguidas | esperar **pela condição** (com *timeout*), nunca `sleep` fixo |

## 2. Broker e nós

| Sintoma | Causa provável | Como testar | Correção |
|---|---|---|---|
| `mosquitto_sub` não mostra nada | simulador parado, tópico errado, broker fora do ar | `systemctl status mosquitto`; `mosquitto_pub -t t -m 1` com `mosquitto_sub -t t` | subir o broker; conferir o prefixo `ifes/` |
| Nó em **outra máquina** não conecta (Mosquitto 2.x) | o padrão só aceita conexões locais | `ss -ltn | grep 1883` mostra `127.0.0.1` | `listener 1883` + `allow_anonymous true` (laboratório) — `deploy/mosquitto-lab.conf` |
| Dois nós se "derrubam" em loop | mesmo `client_id` | log do Mosquitto: *already connected, closing old connection* | `client_id` único por nó (e por painel) |
| Cartão **nunca** fica OFFLINE | o nó não registrou *last will* ou o `status` não é retido | `mosquitto_sub -t 'ifes/+/status' -v` ao ligar/desligar o nó | `will_set(topic, "offline", qos=1, retain=True)` **antes** de conectar; publicar `online` retido |
| OFFLINE demora a aparecer | *keepalive* grande (o broker espera 1,5× o keepalive) | ver `keepalive` no firmware | 10–60 s no real; 5 s no simulador |
| Falsos OFFLINE no Wi-Fi ruim | *keepalive* curto demais | contar quedas no `journalctl` | aumentar o keepalive; melhorar sinal |

## 3. A ponte MQTT (dados não chegam ao painel)

| Sintoma | Causa provável | Como testar | Correção |
|---|---|---|---|
| `curl /api/saude` mostra `"mqtt": false` | `MQTT_HOST/PORT` errados, broker fora, senha | `journalctl -u dashboard` (ou o terminal): `MQTT RECUSADO` / `Connection refused` | ajustar variáveis de ambiente; se há senha, `MQTT_USER/MQTT_PASS` |
| Painel conectado, **dados duplicados** no gráfico e o banco com o dobro de linhas | **dois** clientes assinando: `-w 2`, ou um painel de teste ainda rodando apontando para o mesmo banco | `ps aux | grep -E "gunicorn|dashboard"` | **um** worker (`-w 1`); matar o processo antigo |
| Funcionava, o Mosquitto reiniciou e o painel ficou **surdo** | `subscribe` fora do `on_connect` | reiniciar o broker e olhar se `contagens.leituras` para de subir | assinar **dentro** de `on_connect` |
| `leituras` não sobe, mas `mosquitto_sub` mostra dados | tópico fora do contrato (`ifes/<bancada>/<grandeza>`), prefixo errado, payload não numérico | `python3 -c "from dashboard.topicos import interpreta; print(interpreta('ifes','ifes/bancada1/temp','26.8',0))"` | corrigir o firmware; `TOPICO_BASE` igual ao do nó |
| Valor `nan`/`inf` some | **de propósito**: `interpreta` barra valores não finitos | idem | corrigir o sensor/firmware (divisão por zero, sensor desligado) |
| O nome da bancada com maiúscula/espaço é ignorado | regex `^[a-z0-9_-]{1,32}$` | `interpreta(...)` retorna `None` | use minúsculas, sem espaço (`bancada1`) |
| Exceção no log: `erro ao processar ifes/...` | payload inesperado; o `try/except` salvou a thread | ler o *traceback* no log | tratar o caso em `interpreta` e **escrever o teste** |
| Hora errada nos gráficos | fuso do navegador × servidor | comparar `date` no RPi e no PC | `sudo timedatectl set-timezone America/Sao_Paulo`; o banco guarda *epoch* (sem fuso) |

## 4. Banco de dados

| Sintoma | Causa provável | Como testar | Correção |
|---|---|---|---|
| `sqlite3.OperationalError: database is locked` | sem WAL, ou outra ferramenta com o banco aberto e transação pendente | `sqlite3 dashboard.db "pragma journal_mode"` deve dar `wal` | o código já liga o WAL; feche a ferramenta; não edite o banco com o painel gravando |
| `unable to open database file` | caminho de `DB_PATH` inexistente ou sem permissão (serviço roda como outro usuário) | `ls -ld $(dirname $DB_PATH)`; `sudo -u <usuario> touch $DB_PATH` | ajustar `DB_PATH` em `/etc/default/dashboard`; dono correto |
| Aparecem arquivos `dashboard.db-wal` e `-shm` | **normal** no modo WAL | — | não apague com o painel rodando; copie com `sqlite3 dashboard.db ".backup copia.db"` |
| Disco enche | sem retenção / período muito curto | `du -sh dashboard.db*`, `df -h` | `RETENCAO_DIAS` menor; publicar a cada 10 s; (teoria, Exemplo D.2) |
| Histórico vazio, ao vivo funciona | painel recém-instalado; ainda sem dados na janela escolhida | `curl /api/saude` → `leituras` | espere, ou rode `tools/popular_banco.py` (**dados sintéticos!**) |
| Consulta de histórico lenta | índice ausente (banco criado à mão) | `sqlite3 dashboard.db ".indexes"` deve listar `ix_leituras` | recriar com o esquema de `db.py` |
| Gráfico "pula" entre o histórico e o ao vivo | 🔧 dados **sintéticos** de demonstração com nível diferente dos reais | olhar a série | normal na demonstração; apague o banco de teste antes da demo real |

## 5. Socket.IO e navegador

| Sintoma | Causa provável | Como testar | Correção |
|---|---|---|---|
| Selo "WebSocket" **vermelho**; console: erro de conexão | servidor fora, porta errada, bloqueio de firewall | `curl localhost:5003/api/saude` | subir o painel; abrir a porta certa |
| **400 BAD REQUEST** no *handshake* atrás do nginx, log: `… is not an accepted origin` | 🔧 `Host` repassado **sem a porta** (`$host`); o Socket.IO compara com o `Origin` | log do gunicorn; F12 → Network | `proxy_set_header Host $http_host;` |
| Atrás do nginx a página funciona mas **só em `polling`** (sem 101) | faltam `Upgrade`/`Connection` no `location /socket.io/` | F12 → Network → `socket.io`: o tipo é `xhr`, não `websocket` | copiar o bloco do `nginx-dashboard.conf`; `nginx -t && sudo systemctl reload nginx` |
| Conexão cai a cada ~60 s atrás do nginx | `proxy_read_timeout` padrão (60 s) | log do navegador: reconexões regulares | `proxy_read_timeout 3600s;` |
| Página em branco, console: `io is not defined` / `Chart is not defined` | `static/vendor/*.js` ausentes/corrompidos ou caminho errado | F12 → Network: 404 nos `.js` | restaurar os arquivos do repositório (`git checkout -- src/dashboard/static/vendor`) |
| Alterei o JS/CSS e nada mudou | cache do navegador | Ctrl+F5 | recarga forçada; em produção, versionar o arquivo |
| Cartões com "há 3 s" mas números **cinza** | dado velho (> 30 s) — **de propósito** | `mosquitto_sub` | o nó parou de publicar |
| Botão LED: toast "token…"/401 | `DASH_TOKEN` definido e o navegador sem o token | `curl -i` sem `Authorization` → 401 | informar o token no diálogo; ele fica no `sessionStorage` |
| Botão LED responde 503 | broker indisponível no momento do clique | `curl /api/saude` → `mqtt:false` | ver a seção 3 |
| Botão LED responde OK mas o ESP32 não reage | nó não assina `ifes/<b>/cmd/led` / assina com outro nome / offline | `mosquitto_sub -t 'ifes/+/cmd/#' -v` mostra a mensagem? | a ponta do **firmware** — a ponte já entregou |

## 6. nginx, gunicorn e systemd

| Sintoma | Causa provável | Como testar | Correção |
|---|---|---|---|
| `nginx -t` acusa erro | vírgula/`;` faltando, `map` fora do `http` | a mensagem traz arquivo e linha | corrigir; **só** então `reload` |
| Página padrão do nginx ("Welcome to nginx") | site `default` ainda habilitado | `ls /etc/nginx/sites-enabled` | `sudo rm /etc/nginx/sites-enabled/default` |
| **502 Bad Gateway** | gunicorn fora do ar ou em outra porta | `curl localhost:8000/api/saude`; `systemctl status dashboard` | corrigir o serviço; porta igual no `proxy_pass` |
| `systemctl status dashboard` → `failed`, `203/EXEC` | caminho do `gunicorn` no `ExecStart` errado | `ls -l <caminho>` | `@VENV@` no modelo deve ser a venv real (`…/dashboard-iot/venv`) |
| `failed`, `status=1/FAILURE`, `ModuleNotFoundError` | serviço rodando fora da venv/pasta certa | `journalctl -u dashboard -n 50` | `WorkingDirectory` = `…/src`; `ExecStart` com o gunicorn **da venv** |
| `Permission denied` no banco | `User=` diferente do dono da pasta | `ls -l` | mesmo usuário; ou `chown` |
| Variáveis do `/etc/default/dashboard` não valem | esqueceu `daemon-reload`/`restart`; aspas/`export` no arquivo | `sudo cat /proc/$(systemctl show -p MainPID --value dashboard)/environ | tr '\0' '\n'` (o arquivo é `NOME=valor`, sem `export` nem aspas desnecessárias) | `sudo systemctl daemon-reload && sudo systemctl restart dashboard` |
| Não sobe depois do **reboot** | `enable` não foi feito; o serviço sobe antes do broker/rede | `systemctl is-enabled dashboard`; `journalctl -b -u dashboard` | `sudo systemctl enable dashboard`; `After=network-online.target mosquitto.service` (já no modelo) |
| Dados duplicados após "escalar" | `-w 4` | `ps aux | grep gunicorn` | `-w 1 --threads 100` (teoria §3.2) |
| Porta 8000 acessível de fora | `-b 0.0.0.0:8000` | de outro PC: `curl http://<ip>:8000` | `-b 127.0.0.1:8000`; só o nginx fica exposto |

## 7. Medições

| Sintoma | Causa provável | Correção |
|---|---|---|
| `medir_trafego.py`: `Connection refused` | painel não está em `--alvo` | conferir `host:porta`; subir o painel |
| WebSocket com 0 mensagens | simulador parado: sem dados novos, não há push | subir `simulador_no.py` |
| Números muito diferentes dos do material | outro nº de bancadas/período; medida em rede real | **normal** — reporte o seu e explique a diferença |
| Razão polling/WebSocket < 1 ou ≈ 1 | polling lento (período longo) ou pouquíssimos dados | repita com o período e a carga que você realmente usa |

## 8. Perguntas que o professor vai fazer na defesa

1. Mostre o dado indo do `mosquitto_pub` à tela. Em qual **arquivo** e função ele muda de forma?
2. Por que `-w 1`? O que acontece com `-w 2`?
3. Por que a assinatura MQTT está no `on_connect`?
4. O que impede alguém de apagar a tabela por um nome de bancada malicioso?
5. O que acontece se o RPi perder a energia agora? E se o banco crescer por um ano?
6. O que **não** está seguro neste painel? (resposta honesta: HTTP puro, token em texto, broker aberto
   no laboratório)
