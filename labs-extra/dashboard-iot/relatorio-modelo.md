# Modelo de relatório — Dashboard web profissional (Flask + MQTT + SQLite)

Copie este arquivo para `lab-dashboard/relatorio.md` no repositório da bancada e preencha. Todo
número deve vir **de uma execução sua**, com a fonte indicada (print, arquivo, comando). Dado
**simulado ou sintético** deve ser dito como tal — apresentá-lo como medição custa ponto.

## 0. Identificação

- Bancada / integrantes / data:
- Raspberry Pi: modelo ___ · SO ___ · IP ___ · Python ___ · versões (`pip freeze | grep -iE "flask|socketio|paho|gunicorn"`):
- Nós usados: ( ) ESP32 reais — quantos ___ · ( ) simulador · ( ) ambos
- Grandezas e atuadores do seu projeto (tópicos exatos):
- Repositório / *commit* final:

## 1. Arquitetura (Partes A–C)

Desenhe (ou anexe) **o seu** diagrama de blocos adaptado da Figura T-3 da trilha, com **seus**
tópicos. Explique em 5–8 linhas: por que MQTT entre nós e servidor, WebSocket até o navegador e
REST para comando/histórico (regra da casa, Exemplo 14.4).

| Evidência | Print/arquivo |
|---|---|
| `curl` válido (200) e inválido (400) na API do LED (Parte A) | |
| Duas abas sincronizadas e aba Network com **101** (Parte B) | |
| `mosquitto_pub` → tela; clique → `mosquitto_sub` (Parte C) | |
| Experimento do `subscribe` dentro × fora do `on_connect`: o que observou? | |

## 2. Decisões de projeto (Parte D)

Para cada item, **uma frase de justificativa** com suas palavras:

| Decisão | Justificativa |
|---|---|
| Função pura `interpreta` separada da ponte | |
| SQLite em modo WAL, conexão curta por operação | |
| SQL parametrizado + regex nos nomes | |
| Timestamp na chegada (servidor) | |
| Snapshot ao conectar o Socket.IO | |
| Chart.js local (sem CDN) | |
| Threading mode + `-w 1` | |

Liste **cinco** melhorias de robustez da ponte final em relação à Etapa 3 (D.4) e **o teste novo**
que você escreveu no exercício TDD (D.2): colar o código do teste e o resultado.

## 3. Testes (Partes 0 e D)

- Resultado de `python3 -m pytest tests -q` (print): ___ passed, ___ skipped.
- Os cinco testes de aceitação da D.8:

| # | Teste | Resultado observado (print) | Passou? |
|---|---|---|---|
| 1 | Offline por LWT — tempo medido até aparecer: ___ s | | |
| 2 | Persistência após reiniciar | | |
| 3 | Comando (`mosquitto_sub`) | | |
| 4 | Token: sem = ___ · com = ___ | | |
| 5 | Entrada hostil (`nan`, `<script>`, `conf: 2`) | | |

- Desafio(s) de D.9 que fez e como testou:

## 4. Medições (Parte E)

**E.1 Tráfego** (preencha **antes** a previsão):

| | Previsão | Medido: requisições/mensagens | Medido: bytes/s |
|---|---|---|---|
| Polling a 2 Hz | | | |
| WebSocket | | | |

Razão medida = ___. Compare com a de referência do material (≈ 2,9×) e explique a diferença
(nº de bancadas, período, rede). Lembre: **bytes de aplicação, sem TCP/IP**.

**E.2 Disco:** ___ linhas · ___ bytes · ___ B/linha. Cenário do **seu** projeto (___ bancadas ×
___ grandezas × 1 amostra a cada ___ s) → ___ MB/dia → `RETENCAO_DIAS` = ___ e por quê.

**E.3 Downsampling:** 24 h com 300 pontos: ___ B, ___ s · com 2000 pontos: ___ B, ___ s.
Que balde (`bucket_s`) o servidor escolheu em cada caso? Confere com `⌈horas·3600/pontos⌉`?

## 5. Produção (Parte F)

| Teste | Resultado (print/saída) |
|---|---|
| `sudo nginx -t` | |
| `curl localhost/api/saude` pela porta 80 | |
| `systemctl status dashboard` (*active (running)*) | |
| Matar o processo → volta sozinho? Quanto tempo? | |
| **Reboot**: painel no ar sem intervenção? | |
| Porta 8000 inacessível de outro PC? | |

Explique, **linha a linha**, um trecho do `dashboard.service` à sua escolha e o bloco
`location /socket.io/` do nginx. Qual problema o `Host $http_host` evita?

## 6. Segurança

| Camada | Ligada no seu sistema? | Observação |
|---|---|---|
| Escape no navegador / regex no servidor | | |
| SQL parametrizado | | |
| Token (`DASH_TOKEN`) / `auth_basic` | | |
| Mosquitto com senha / ACL | | |
| HTTPS / MQTT-TLS | | |

Escreva o que **continua inseguro** no seu sistema e que risco isso traz.

## 7. Demonstração (Parte G)

Link do vídeo ou data/hora da demonstração ao vivo; checklist dos 7 itens do roteiro:

## 8. Limitações e trabalho futuro

Pelo menos **três** limitações **honestas** (ex.: só testado com simulador; timestamp do
servidor; um único processo; HTTP puro; histórico sem agregação de longo prazo; não testado com N
navegadores) e o que você faria a seguir.

## 9. Referências

Livros usados, com página (Smart, Dalmaris), documentação oficial consultada, e **o que foi copiado
do código de referência** (arquivo e linhas) × **o que foi escrito por vocês**.

---

## Rubrica (100 pontos)

| Critério | Pontos | O que se espera |
|---|---|---|
| Etapas 1–3 funcionando e demonstradas | 15 | REST com validação; WebSocket com 2 abas; ponte nos dois sentidos |
| Dashboard completo (Etapa 4) | 20 | ao vivo + histórico + CSV; LWT→offline; persistência |
| Testes e TDD | 10 | `pytest` verde; teste próprio escrito **antes** do código |
| Medições com números próprios | 15 | tráfego, disco, downsampling — com previsão e conclusão |
| Produção (Etapa 5) | 15 | serviço, nginx, WebSocket 101, reboot, falha recuperada |
| Segurança e robustez | 10 | camadas ligadas/desligadas **declaradas**; entrada hostil tratada |
| Honestidade e limitações | 10 | sintético × medido; o que não foi testado; defesa oral |
| Clareza do relatório | 5 | diagramas, prints legendados, referências |
