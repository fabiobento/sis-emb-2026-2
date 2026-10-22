# Teoria — Dashboard web profissional para IoT (Flask + Socket.IO + MQTT + SQLite)

Texto de apoio do [lab do dashboard](lab-dashboard.md) e da [trilha full-stack](../../docs/trilha-fullstack.md).
Aqui ficam os **porquês**: as decisões de projeto, as contas e as armadilhas. O lab traz os
**comandos**. Leia esta teoria antes da Etapa 3 — é nela que as peças deixam de ser "mágica".

**Pré-requisitos:** semana 14 (MQTT, QoS, retain, LWT), semanas 11–12 (Linux, venv, systemd) e
noções de Python (funções, dicionários, *threads* em nível de uso).

## 1. O problema: do dado de um sensor à decisão de uma pessoa

Um ESP32 mede 26,8 °C. Para isso virar **informação útil**, alguém precisa (a) ver o valor sem
abrir terminal, (b) ver como ele evoluiu nas últimas horas, (c) agir ("ligar o LED") e (d)
confiar que o painel continua de pé amanhã. Cada verbo vira uma exigência técnica:

| Necessidade | Exigência | Peça que resolve |
|---|---|---|
| ver agora | baixa latência, sem F5 | WebSocket (push) |
| ver a evolução | armazenamento e consulta | SQLite + REST |
| agir | caminho de volta, confiável | REST → MQTT QoS 1 |
| confiar | recuperar-se sozinho, não aceitar lixo | systemd, validação, retenção |

![Arquitetura do painel, do sensor ao navegador, com o caminho do comando de volta](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_arquitetura.png)
*Figura D-1 — A arquitetura inteira. Volte a ela sempre que perder o fio: os números 1–6 são os
fluxos que a teoria explica um a um.*

## 2. Três modelos de comunicação, três usos

| | REST (HTTP) | WebSocket | MQTT |
|---|---|---|---|
| Quem inicia | cliente, sempre | cliente abre; depois **ambos** falam | clientes falam com o **broker** |
| Conexão | curta (ou *keep-alive*) | uma, longa, bidirecional | uma, longa, bidirecional |
| Modelo | pergunta → resposta | fluxo de mensagens | publish/subscribe por tópico |
| Custo por mensagem | cabeçalhos HTTP (centenas de bytes) | 2–14 B de moldura + corpo | 2 B de cabeçalho fixo + tópico |
| Melhor para | ações esporádicas, integração, `curl` | tempo real **no navegador** | dispositivo ↔ dispositivo |
| Neste projeto | comando, histórico, saúde | dados novos → tela | ESP32 ↔ broker ↔ ponte |

> 💡 **Regra da casa (Exemplo 14.4):** *dispositivo↔dispositivo = MQTT; navegador↔servidor =
> WebSocket; esporádico/integração = REST.* O navegador **não** fala MQTT puro (precisaria de
> MQTT sobre WebSocket e de expor o broker); por isso o servidor Flask faz a **ponte**.

### Exemplo resolvido D.1 — quanto custa perguntar o tempo todo?

Medimos com o `tools/medir_trafego.py` (proxy TCP contador; 3 bancadas × 2 grandezas publicando
a 1 Hz; 60 s):

- **Polling** (`GET /api/nos` a cada 0,5 s): 112 requisições. Cada uma gastou ≈ 153 B de ida
  (pedido) + ≈ 951 B de volta (167 B de cabeçalho + 780 B de JSON), isto é, ≈ 1,1 kB. Total
  **2.062 B/s**.
- **WebSocket**: 360 mensagens (6 leituras/s) em 60 s, ≈ 118 B cada. Total **707 B/s**.

Razão: 2062 / 707 ≈ **2,9×**. E há um segundo custo, a **latência**: se o dado nasce em um
instante aleatório entre duas perguntas, ele espera em média **T/2 = 0,25 s** (pior caso 0,5 s) para
aparecer. No push, ele sai no instante em que chega.

Dois detalhes honestos: (1) o polling repete o JSON **inteiro** mesmo quando só um número mudou —
poderia-se otimizar com *ETag* ou enviando só a diferença, mas isso é reinventar o push; (2) os
valores são de **loopback** e só contam bytes de aplicação — sem cabeçalhos TCP/IP (≈ 40–52 B por
pacote), que pioram **os dois** lados, mas mais o polling (mais pacotes). **Meça no seu cenário.**

![Polling pergunta sempre; WebSocket é avisado quando há dado novo](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_sequencia.png)
*Figura D-2 — Esquerda: cada ciclo paga cabeçalhos e devolve o estado completo. Direita: uma
conexão e mensagens pequenas, só quando há novidade.*

![Polling 2.062 B/s contra WebSocket 707 B/s](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_trafego.png)
*Figura D-3 — O resultado da medição do Exemplo D.1.*

**Elo com a disciplina:** polling × push é a escolha da semana 4 (polling × interrupção) em outra
escala. E o MQTT *retain* da semana 14 faz papel parecido com o "estado inicial" que o servidor
manda ao navegador recém-conectado.

## 3. O servidor: Flask, Socket.IO e *threads*

**Flask** transforma funções Python em rotas HTTP. **Flask-SocketIO** acrescenta, sobre o mesmo
servidor, canais de eventos bidirecionais: `socketio.emit("leitura", dado)` entrega `dado` a todos
os navegadores conectados. **Socket.IO** é um protocolo em cima do WebSocket que acrescenta
nomes de eventos, reconexão automática e *fallback* para *long polling* quando o WebSocket está
bloqueado — por isso a página funciona mesmo atrás de proxies teimosos (mais lenta, mas funciona).

### 3.1 Modos assíncronos e por que usamos `threading`

Um servidor precisa atender **muitos** clientes "ao mesmo tempo", e o WebSocket os mantém
conectados por muito tempo. Há três estratégias em Python:

| Modo | Como funciona | Quando usar |
|---|---|---|
| `threading` | uma *thread* por conexão | **poucos** clientes (lab, dezenas); simples; **escolhido** |
| `eventlet`/`gevent` | corrotinas cooperativas (*green threads*) | milhares de conexões; exige "monkey-patch" |
| `asyncio` | `async/await` | código inteiro assíncrono (outro framework) |

No laboratório teremos poucas abas abertas. O modo `threading` evita o *monkey-patch* (que
interfere com o paho-mqtt e com o SQLite) e se depura com `print`. Custo: uma *thread* por
cliente — o RPi 3 aguenta dezenas sem suar.

### 3.2 Por que **um** worker no gunicorn

![nginx, gunicorn, Flask, SQLite e Mosquitto sob o systemd](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_producao.png)
*Figura D-4 — A pilha de produção.*

Comando de produção: `gunicorn -k gthread -w 1 --threads 100 dashboard.wsgi:app`.

- **`-w 1`**: cada *worker* é um **processo** que executa `create_app()` e, portanto, abre o **seu**
  cliente MQTT. Com `-w 4`, cada mensagem do broker seria recebida **4 vezes** e gravada 4 vezes
  no banco. Além disso o Socket.IO precisa que as requisições de uma sessão cheguem sempre ao
  **mesmo** processo (afinidade de sessão); com vários workers sem *sticky sessions*, a conexão
  quebra de modo intermitente e de difícil diagnóstico.
- **`--threads 100`**: o paralelismo vem das threads dentro do único processo.
- **Por que gunicorn e não o `app.run()`?** O servidor de desenvolvimento (Werkzeug) não foi feito
  para produção: sem supervisão de processos, sem proteção contra clientes lentos, e imprime o
  aviso *"do not use it in a production deployment"*.

> ⚠️ **E se eu precisar de mais de um processo?** O padrão profissional é separar: um processo
> **só** com a ponte MQTT (grava no banco e publica num *message queue* como Redis) e vários
> workers web que se inscrevem nessa fila. É uma evolução natural, fora do escopo — mencione-a no
> relatório como "trabalho futuro".

## 4. A ponte MQTT: o dashboard é só mais um cliente

A ponte (`dashboard/mqtt_bridge.py`) segue o padrão do `logger_rpi.py` da semana 14. As regras
que a tornam **robusta**:

1. **`client_id` único.** O broker só admite **uma** conexão por `client_id`; se dois clientes usam o
   mesmo, o novo derruba o antigo, que reconecta e derruba o novo — em loop. Usamos
   `dashboard-<hostname>-<tempo>`.
2. **Assine em `on_connect`, não depois do `connect()`.** Numa sessão limpa (*clean session*), o
   broker esquece suas assinaturas quando a conexão cai. Se a assinatura estiver fora do
   `on_connect`, após a primeira queda o painel reconecta... e fica **surdo**.
3. **`reconnect_delay_set(1, 30)`** e `loop_start()`: o paho tenta reconectar sozinho, com espera
   crescente (1 s … 30 s), numa *thread* própria.
4. **Callbacks curtos, com `try/except`.** Uma exceção solta em `on_message` mata a thread de rede
   do paho — o painel continua "no ar" e **nunca mais** recebe nada. Registre o erro e siga.
5. **Os callbacks rodam em outra thread** (a do paho), não na do Flask. Tudo que eles tocam —
   banco, `socketio.emit` — precisa ser seguro para uso entre threads (veja §5.2).

### 4.1 Quem diz que o nó caiu? O *last will*

Um nó que perde energia **não consegue avisar** que caiu. Por isso, ao conectar, o ESP32 registra
no broker uma **mensagem de testamento** (*last will*): "se eu sumir sem me despedir, publique
`offline` em `ifes/<bancada>/status`" (retida). Ao conectar, o próprio nó publica `online`
(também retida). O painel só precisa assinar `status`:

```text
ESP32 conecta       →  publica online  (retain)
ESP32 perde energia →  broker detecta (keepalive estourado) → publica o will: offline (retain)
```

O tempo até o *offline* aparecer é ≈ 1,5 × *keepalive* (o simulador usa 5 s; no firmware real,
escolha entre 10 e 60 s — curto demais gera falsos *offline* em Wi-Fi ruim).

### 4.2 Timestamp: o relógio é do servidor

O ESP32 não tem relógio de tempo real: sem NTP ele não sabe a hora. Por isso o carimbo de tempo
da leitura é o instante de **chegada** no servidor (`time.time()` na ponte). Isso embute o atraso
de rede (milissegundos no Wi-Fi do laboratório) — aceitável aqui; se você precisar de precisão,
o nó deve enviar o próprio timestamp (com NTP) **e** você deve registrar as duas horas.

### 4.3 O contrato de tópicos e a função pura `interpreta`

![Do tópico MQTT à linha do banco](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_topicos.png)
*Figura D-5 — Cada tópico do contrato tem um destino. Tudo que não casa é ignorado.*

A decisão de arquitetura mais útil do código é separar **interpretar** de **fazer**:
`interpreta(base, topico, payload, agora)` recebe texto e devolve um dicionário (ou `None`) — sem
rede e sem banco. Consequências: testa-se com 20 linhas de `pytest` sem broker; o mesmo
código serve a quem queira trocar o broker; e o "lixo" (payload `nan`, JSON malformado,
`conf` fora de 0–1, tópicos estranhos) é barrado **num lugar só**.

Um detalhe que pega todo mundo: em Python, `float("nan")` e `float("inf")` **funcionam**. Um
sensor defeituoso que publique `nan` passaria na conversão e derrubaria o gráfico (e o SQLite
guardaria `NaN` como `NULL`). Daí o teste explícito `math.isfinite`.

## 5. Persistência: SQLite no Raspberry Pi

### 5.1 Por que SQLite

Um banco SQL **num arquivo**, sem servidor, sem senha, sem configuração, incluído no Python
(`import sqlite3`). Para um painel com um escritor (a ponte) e poucos leitores, é a escolha certa.
Quando **não** serve: vários processos gravando com alta concorrência, dados em máquinas
distintas ou terabytes de histórico — aí pense em PostgreSQL/InfluxDB/TimescaleDB.

### 5.2 Esquema, índice e WAL

```sql
CREATE TABLE leituras (id INTEGER PRIMARY KEY, ts REAL NOT NULL,
                       bancada TEXT NOT NULL, grandeza TEXT NOT NULL, valor REAL NOT NULL);
CREATE INDEX ix_leituras ON leituras (bancada, grandeza, ts);
CREATE TABLE eventos (id INTEGER PRIMARY KEY, ts REAL NOT NULL, bancada TEXT NOT NULL,
                      tipo TEXT NOT NULL, texto TEXT NOT NULL, extra REAL);
```

- **Por que duas tabelas?** `leituras` é uma **série temporal numérica** (muitas linhas, todas
  iguais em forma). `eventos` são fatos discretos (um nó ficou offline; a classe de movimento
  mudou; alguém mandou um comando). Misturar tudo numa tabela com colunas `NULL` para todo lado
  é a receita da tabela ilegível.
- **O índice composto `(bancada, grandeza, ts)`** casa exatamente com o filtro das consultas
  ("esta bancada, esta grandeza, neste intervalo"): o SQLite vai direto às linhas certas em vez de
  varrer a tabela inteira. Sem ele, a consulta de 24 h degrada linearmente com o tamanho do
  histórico.
- **Modo WAL** (`PRAGMA journal_mode=WAL`): o escritor grava num arquivo de log lateral e os
  leitores seguem lendo o arquivo principal — **o escritor (thread do MQTT) não bloqueia os
  leitores (requisições web)**, e vice-versa. Sem WAL você verá `database is locked` sob carga.
- **Uma conexão curta por operação** (abrir, usar, fechar): o módulo `sqlite3` não permite
  compartilhar uma conexão entre threads por padrão. A abertura é barata (microssegundos) e
  elimina uma classe inteira de erros de concorrência.
- **SQL parametrizado**, sempre: `execute("... WHERE bancada=?", (b,))`. **Nunca**
  `f"... WHERE bancada='{b}'"`. Com o `f`-string, a bancada `x'; DROP TABLE leituras; --` vira
  comando. Com o `?`, vira apenas um texto estranho.

### Exemplo resolvido D.2 — quanto disco o histórico consome?

Medimos no banco de demonstração: 55.494 linhas ocupavam 4.153.344 bytes, isto é **≈ 75 B por
linha** (inclui o índice). Cenário: 3 bancadas × 2 grandezas, 1 amostra/s cada:

```text
linhas/dia = 3 × 2 × 86 400 = 518 400
bytes/dia  ≈ 518 400 × 75 ≈ 38,9 MB
em 7 dias  ≈ 272 MB          (RETENCAO_DIAS=7, o padrão)
em 1 ano   ≈ 14 GB           ← estoura um cartão de 8–16 GB !
```

Conclusão que vai para o relatório: **sem política de retenção o painel enche o cartão SD**. Daí
(1) `RETENCAO_DIAS` e a purga periódica; (2) publicar com período maior (a cada 10 s o consumo cai
10×); (3) para histórico longo, guardar **agregados** (média por hora) em vez do bruto. *(Os 75 B
são do nosso esquema; mude o esquema e a conta muda.)*

### 5.3 Downsampling: reduza no banco, não no navegador

Um gráfico de 24 h com 8.640 pontos por série é lento de enviar e não tem pixels para desenhar
(uma tela tem ~1.500 colunas). O servidor agrupa as leituras em **baldes** de tempo:

```sql
SELECT CAST(ts / :b AS INTEGER) * :b AS t,
       AVG(valor), MIN(valor), MAX(valor), COUNT(*)
FROM leituras WHERE bancada=:ban AND grandeza=:g AND ts BETWEEN :d AND :a
GROUP BY 1 ORDER BY 1
```

`ts / :b` truncado dá o número do balde; multiplicar de volta dá o **início** do balde.

### Exemplo resolvido D.3 — qual balde escolher?

A API recebe `horas` e `pontos` (alvo) e calcula `bucket = ceil(horas × 3600 / pontos)`:

| Janela | pontos-alvo | balde | pontos devolvidos |
|---|---|---|---|
| 1 h | 300 | ⌈3600/300⌉ = 12 s | ≈ 300 |
| 24 h | 300 | ⌈86 400/300⌉ = 288 s | ≈ 300 |
| 7 dias | 300 | ⌈604 800/300⌉ = 2 016 s (33,6 min) | ≈ 300 |

No banco de demonstração (24 h; 9.092 pontos brutos), o balde de 5 min devolveu **289 pontos**
(redução de 31×), e a consulta levou **≈ 3 ms** no PC de desenvolvimento *(o tempo no RPi 3 não foi
medido; espere algumas vezes mais)*. **Por que também devolver mín e máx?** Porque a média **esconde
picos**: um pulso de 1 s a 40 °C some numa média de 5 min, mas aparece na faixa máxima.

![9.092 pontos brutos contra 289 pontos de balde de 5 min](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_downsampling.png)
*Figura D-6 — Mesma informação visual, 31× menos dados. Dados sintéticos de demonstração.*

## 6. Segurança em camadas

Um painel que **comanda hardware** e fica numa rede compartilhada merece o mesmo cuidado que
um sistema web comum. Pense em camadas: cada uma barra uma classe de problema, e nenhuma
sozinha basta.

![Cinco camadas de defesa e o que cada uma impede](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_seguranca.png)
*Figura D-7 — Navegador, nginx, Flask, SQLite e Mosquitto.*

| Ameaça | Exemplo concreto | Defesa no nosso código |
|---|---|---|
| **XSS** | um nó publica `<img src=x onerror=...>` em `movimento`; o painel o insere no HTML | `esc()` em todo texto vindo do MQTT antes de ir ao HTML; regex na `classe` já no servidor |
| **Injeção de SQL** | bancada `x'; DROP TABLE …` | SQL parametrizado + regex `^[a-z0-9_-]{1,32}$` |
| **Comando forjado** | alguém na rede faz `curl -X POST …/api/cmd/led` | token `Bearer` opcional (`DASH_TOKEN`) → 401; em produção, `auth_basic` no nginx |
| **Entrada inválida** | `{"valor": 7}` ou JSON quebrado | `valor ∈ {0,1}`, `get_json(silent=True)`, resposta 400 |
| **Disco cheio** | histórico sem fim | `RETENCAO_DIAS` e purga periódica |
| **Escuta/espelho** | senha do painel trafegando em texto | HTTPS (certbot/Let's Encrypt ou certificado interno) *(nota de produção, não ligada no lab)* |
| **Broker aberto** | qualquer um publica `cmd/led` | `mosquitto-seguro.conf`: `allow_anonymous false` + senha; ACL por tópico; TLS na 8883 |

> ⚠️ **Limites honestos:** um *token* fixo na URL/cabeçalho em HTTP puro **pode ser lido por quem
> escuta a rede**. Em laboratório isolado, ele impede o acidente e o curioso; para o mundo real
> você precisa de HTTPS. O objetivo da trilha é você saber **onde** está o risco, não prometer
> invulnerabilidade — escreva isso no relatório.

**O ponto sutil do Origin.** O Socket.IO compara o cabeçalho `Origin` do navegador com o `Host`
que o servidor enxerga (proteção contra *cross-site WebSocket hijacking*). Atrás do nginx, se o
`Host` repassado **perder a porta** (`proxy_set_header Host $host;`), o navegador em
`http://pi:8080` envia `Origin: …:8080` e o servidor vê `Host: pi` → **recusa a conexão (400)**.
Daí `proxy_set_header Host $http_host;` na nossa configuração. *(Este problema apareceu de verdade nos
nossos testes; na porta 80 ele fica invisível, e é por isso que aparece "só em produção".)*

## 7. O front-end sem *build step*

Poderíamos usar React, npm e webpack. **Escolhemos não usar:** o RPi 3 fica lento com *toolchains*
de Node, o laboratório pode estar **sem internet**, e o objetivo é entender o fluxo de dados, não
um framework. Resultado: HTML + CSS + JavaScript puro e duas bibliotecas **copiadas para
`static/vendor/`** (Chart.js 4.4.4 e socket.io-client 4.7.5) — sem CDN, sem `npm install`,
funcionando offline.

Princípios do `app.js`:

1. **Estado único** (`estado.nos`, `estado.series`): os eventos do Socket.IO **atualizam o estado**
   e uma função `desenha()` redesenha a partir dele. Evita a bagunça de manipular o DOM em dez
   lugares.
2. **Ao conectar, o servidor envia um *snapshot*** (`estado`): últimos pontos de cada série,
   status dos nós e eventos — assim a aba recém-aberta **já nasce com gráfico**, sem esperar
   dados novos.
3. **Reconexão é do Socket.IO**; mostramos o selo "WebSocket" verde/vermelho para o usuário
   saber quando está olhando para dados velhos.
4. **Escapar tudo** que veio de fora (`esc()`, que troca `& < > " '` por entidades HTML): o payload MQTT é entrada **não confiável**.
5. **Janela deslizante**: guardamos só os últimos N pontos por série no navegador (memória
   constante, mesmo com a aba aberta por dias).
6. **Idade do dado** ("temp · há 3 s"): um número bonito mas velho é pior que nenhum. O cartão
   recalcula o "há X s" a cada segundo e o dado com mais de 30 s é marcado como velho.

![Painel ao vivo, tema claro](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_tela_home.png)
*Figura D-8 — A página "Ao vivo" (dados simulados).*

O layout é **responsivo** (CSS Grid com `auto-fill`) e respeita o tema do sistema
(`prefers-color-scheme`):

![Painel em tema escuro](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/fs_tela_home_dark.png)
*Figura D-9 — Mesmo HTML, tema escuro; as cores vêm de variáveis CSS.*

## 8. Produção: nginx, systemd e o que "profissional" significa

Profissional não é "mais bonito": é **previsível**. Três perguntas que o painel de produção
responde e o de desenvolvimento não:

| Pergunta | Resposta em produção |
|---|---|
| E se o programa cair? | `Restart=on-failure` no systemd reinicia em 3 s |
| E se o RPi reiniciar? | `systemctl enable` faz o serviço subir sozinho no boot |
| E se eu errar a configuração? | `nginx -t` valida **antes** de recarregar; `journalctl -u dashboard` mostra o motivo |

### 8.1 O arquivo de serviço, linha a linha

```ini
[Unit]
After=network-online.target mosquitto.service   # só sobe depois da rede e do broker
[Service]
User=<seu usuário>                              # NUNCA root
EnvironmentFile=/etc/default/dashboard           # configuração e segredos fora do código
ExecStart=<venv>/bin/gunicorn -k gthread -w 1 --threads 100 -b 127.0.0.1:8000 dashboard.wsgi:app
Restart=on-failure
```

O `-b 127.0.0.1:8000` é deliberado: o gunicorn só aceita conexões **da própria máquina**; quem
fala com o mundo é o nginx. Assim há **uma** porta exposta e ela é a que sabe de autenticação e
HTTPS.

### 8.2 O bloco do WebSocket no nginx

```nginx
location /socket.io/ {
    proxy_pass http://127.0.0.1:8000/socket.io/;
    proxy_http_version 1.1;                          # WebSocket exige HTTP/1.1
    proxy_set_header Upgrade $http_upgrade;          # repassa o pedido de "upgrade"
    proxy_set_header Connection $connection_upgrade; # idem
    proxy_set_header Host $http_host;                # com a porta! (§6)
    proxy_read_timeout 3600s;                        # conexão longa: não cortar em 60 s
}
```

Esquecer `Upgrade/Connection` é o erro mais comum: a página parece funcionar (o Socket.IO cai para
*long polling*) mas fica lenta e gasta mais — confira na aba **Network** do navegador se aparece a
linha `websocket` com status **101 Switching Protocols**.

## 9. Testabilidade: como saber que funciona sem olhar a tela

O projeto traz **42 testes automáticos** (`python3 -m pytest tests -q`), em três níveis:

| Nível | Arquivo | O que prova | Precisa de broker? |
|---|---|---|---|
| Unidade | `test_topicos.py`, `test_db.py` | `interpreta` e o banco isolados | não |
| Integração | `test_api.py`, `test_socket.py` | rotas REST, validação, token, eventos Socket.IO | não |
| Ponta a ponta | `test_e2e_mqtt.py` | MQTT real → banco → Socket.IO; **comando de ida e volta**; LWT → offline | **sim** (sobe um Mosquitto temporário; é pulado se não houver `mosquitto`) |

Por que isso importa: **testes são o relatório de que o painel se comporta como você diz**. E um
exemplo real do que eles pegam: o teste ponta a ponta *falhava de vez em quando* porque
assertava as leituras antes de os eventos terem sido gravados — uma **condição de corrida do
próprio teste**. A correção (esperar a condição, com *timeout*) é o padrão para qualquer teste que
envolva threads e rede: **nunca** `sleep(1)` e torcer; espere pelo estado desejado.

## 10. Autoteste (com respostas)

1. **Por que o navegador não assina tópicos MQTT diretamente neste projeto?**
   *Precisaria de MQTT sobre WebSocket no broker (porta extra exposta, credenciais no JavaScript) e
   o painel perderia o ponto único de validação/gravação. A ponte no servidor mantém o broker
   interno e o ESP32 desacoplado da web.*
2. **O painel mostra cada leitura duplicada no gráfico e o banco tem o dobro de linhas. Qual é a
   causa mais provável?**
   *Dois clientes MQTT assinando o mesmo tópico: `-w 2` no gunicorn, ou dois painéis (um de
   teste ainda rodando) apontando para o mesmo banco. Veja `ps aux | grep -E "gunicorn|dashboard"`.*
3. **Após reiniciar o Mosquitto, o painel continua "online" mas não recebe mais nada.** *Assinatura
   feita fora do `on_connect`: a sessão limpa perdeu a assinatura e ninguém reassinou.*
4. **Calcule o balde para um gráfico de 3 dias com ~500 pontos.** *3 × 86 400 / 500 = 518,4 →
   ⌈⌉ = **519 s** (≈ 8,6 min).*
5. **Um nó publica `ifes/bancada1/temp` com `abc`. O que o painel faz?** *`interpreta` devolve
   `None` (a conversão `float` falha) e a mensagem é descartada; nada vai ao banco nem à tela.*
6. **Por que gravar o timestamp na chegada e não confiar no ESP32?** *Sem NTP/RTC o relógio do nó
   é "segundos desde o boot"; misturaria origens de tempo. Preço: inclui o atraso de rede.*
7. **O que acontece com o histórico se o RPi perder energia no meio de uma gravação?** *O SQLite é
   transacional (journal/WAL): ou a linha foi gravada inteira ou não foi; o arquivo não corrompe.
   Perdem-se as leituras que chegaram durante o apagão (ninguém estava ouvindo).*

## 11. Glossário

**Broker** — servidor central do MQTT. **Ponte** — programa que traduz entre dois mundos (MQTT ↔
WebSocket). **Push** — o servidor envia sem ser perguntado. **Polling** — o cliente pergunta
repetidamente. **LWT** — *last will and testament*: mensagem que o broker publica se o cliente some.
**Retain** — o broker guarda a última mensagem do tópico para quem assinar depois. **QoS 1** —
entrega ao menos uma vez. **WSGI** — interface padrão entre servidores web e aplicações Python.
**WAL** — *write-ahead log*, modo do SQLite em que leitores e escritor não se bloqueiam.
**Downsampling** — reduzir a quantidade de pontos preservando a forma. **Proxy reverso** — servidor
na frente da aplicação (nginx). **Retenção** — por quanto tempo guardar os dados. **XSS** —
injeção de script na página por texto não escapado. **12-factor** — receita de aplicação que guarda
configuração em variáveis de ambiente.
