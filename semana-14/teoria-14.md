# Aula 14 — Projetos de Aplicação: Wi-Fi, MQTT e a Arquitetura ESP32 + Raspberry Pi (ementa)

> **Pré-requisito**: semanas 6 (event groups), 10 (CAN — você já viu pub–sub!), 11 (Linux) e 13
> (controle no MCU).
> **Como usar**: texto autossuficiente. Os Exemplos 14.1–14.4 são o modelo das questões 7–12 da
> Lista 5. Esta aula é, deliberadamente, o gabarito de arquitetura do seu projeto final.

Cada placa da disciplina trabalhou, até aqui, sozinha. Mas nenhum produto IoT é uma placa
sozinha: é uma **arquitetura** — nós sensores baratos e econômicos na ponta, um cérebro Linux
agregando e decidindo perto deles, e (quando faz sentido) a nuvem lá em cima. Esta aula monta
essa arquitetura com as nossas duas plataformas: o **ESP32 ganha Wi-Fi** e aprende o
protocolo que virou a língua franca da IoT — o **MQTT** — publicando leituras para um
**broker Mosquitto rodando no RPi 3**. No caminho, três padrões de firmware que você levará
para a vida: conexão orientada a eventos com *event group* (a semana 6 fecha seu arco), *last
will* para detectar nó morto, e as contas de energia que decidem se um produto a bateria vive
1 dia ou 2 meses. É a última aula de conteúdo novo.

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) conectar o ESP32 a uma rede Wi-Fi com ESP-IDF e sincronizar a aplicação por eventos;
- (b) explicar o modelo publish–subscribe (broker, tópicos, QoS, retain, LWT) e projetar uma
  árvore de tópicos;
- (c) montar o broker no RPi e depurar com as ferramentas Mosquitto;
- (d) estimar autonomia com deep sleep;
- (e) escolher o transporte certo (REST × WebSockets × MQTT) para cada dado.

---

## 1. A arquitetura em camadas de uma aplicação IoT

```
  NUVEM (opcional)      dashboards, banco histórico, ML pesado
        ▲
  BORDA / GATEWAY       RPi 3: broker Mosquitto, registro, regras, painel web
        ▲  (rede local)
  COISAS                nós ESP32: sensor + tempo real + rádio + bateria
```

Nosso laboratório implementa as duas camadas de baixo — e a divisão de trabalho é exatamente
a moral das semanas 11–13: o **MCU** faz o determinístico e o econômico (amostrar, controlar,
dormir); o **Linux** faz o conveniente e o pesado (agregar, armazenar, servir). Produtos
reais de estufa, frota e indústria têm esse desenho; o seu projeto final, idealmente,
também. O diagrama abaixo detalha quem fala com quem na nossa bancada:

![Arquitetura completa: nó ESP32, broker no Raspberry Pi e clientes externos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/arquitetura_esp32_rpi.png)

*Figura 14-A — A arquitetura do Lab 14 e do projeto final: tudo passa pelo broker; os nós
não se conhecem — se um cai, os outros nem notam (a não ser pelo LWT, seção 3).*

## 2. Wi-Fi no ESP32: programação orientada a eventos

Conectar ao Wi-Fi não é uma chamada — é uma **conversa assíncrona** com o rádio, e o ESP-IDF
a modela por eventos: você registra *handlers* no *event loop* e reage. A sequência mínima
(toda ela no `no_mqtt/main.c` do lab):

```c
ESP_ERROR_CHECK(nvs_flash_init());          // NVS primeiro: o driver guarda calibração lá (sem. 2!)
esp_netif_init(); esp_event_loop_create_default();
esp_netif_create_default_wifi_sta();        // interface "station" (cliente de AP)
esp_wifi_init(&cfg);
esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL);
esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handler, NULL);
/* SSID/senha em wifi_config_t */
esp_wifi_start();                           // dispara a máquina; o resto acontece nos eventos
```

E o handler é uma pequena máquina de estados de rede:

```c
static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)        esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(eg, BIT_GOT_IP);                    // ficamos "offline"
        esp_wifi_connect();                                      // reconexão automática
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
        xEventGroupSetBits(eg, BIT_GOT_IP);                      // AGORA temos rede
}
```

**Exemplo resolvido 14.1 (sincronizando a conexão)** — Por que não chamar `mqtt_start()`
logo após `esp_wifi_connect()`? Porque a conexão é assíncrona: o IP só existe após
`IP_EVENT_STA_GOT_IP`, que pode demorar segundos (ou nunca vir — senha errada, AP longe).
Solução canônica: a tarefa principal **bloqueia** em `xEventGroupWaitBits(eg, BIT_GOT_IP,
...)` — dormindo sem gastar CPU (estado nobre da semana 5) — e o handler faz
`xEventGroupSetBits()`. É o padrão evento→tarefa da semana 6 aplicado a rede; e o *event
group* (não um semáforo) porque logo esperaremos **combinações** de bits: rede E broker.
Repare na elegância do desenho: nenhum `while(!conectado) delay(100)` — polling morreu na
semana 5 e não ressuscita nem no Wi-Fi.

## 3. MQTT: publish–subscribe para o mundo físico

O MQTT é um protocolo leve sobre TCP, projetado (anos 90, para oleodutos por link de
satélite!) para dispositivos pequenos em redes ruins. A ideia central: **ninguém fala com
ninguém diretamente** — todos falam com um **broker** (o nosso: Mosquitto no RPi 3):

![Fluxo básico de comunicação MQTT entre publicadores, broker e assinantes](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mqtt_fluxo_basico.png)

*Figura 14-B — O fluxo MQTT: publicadores enviam mensagens ao broker, que as distribui aos
assinantes dos tópicos correspondentes. Fonte: Raspberry Pi and MQTT Essentials (Packt),
cap. 1, Fig. 1.1.*

- quem produz dado **publica** numa string hierárquica chamada **tópico**:
  `ifes/bancada3/temp`;
- quem precisa do dado **assina** o tópico — inclusive com curingas: `+` (um nível:
  `ifes/+/temp` = a temperatura de todas as bancadas) e `#` (tudo abaixo: `ifes/#`);
- produtor e consumidor ficam **desacoplados**: nenhum conhece o endereço, a linguagem ou a
  existência do outro. Já viu esse filme? É a arbitragem por ID do CAN (semana 10) em versão
  software — mensagens identificadas pelo *assunto*, não pelo remetente.

![Visão detalhada do MQTT: cliente, broker, tópicos e sessões](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mqtt_overview.png)

*Figura 14-C — O MQTT em detalhe: clientes mantêm sessões com o broker; mensagens vivem em
tópicos; o broker roteia por assinatura. Fonte: Raspberry Pi and MQTT Essentials (Packt),
cap. 2, Fig. 2.1.*

Os três conceitos que separam o usuário do projetista:

- **QoS** — 0: entrega no máximo 1× ("dispara e esquece"); 1: ao menos 1× (confirmação
  PUBACK; pode duplicar); 2: exatamente 1× (handshake duplo, caro). A escada de
  confirmações:

![Os três níveis de QoS do MQTT: 0, 1 e 2](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mqtt_qos.png)

*Figura 14-D — QoS em diagrama: cada degrau troca tráfego e latência por garantia.
Telemetria periódica ⇒ QoS 0 (a próxima leitura vem em segundos); comandos ⇒ QoS 1; QoS 2
só quando duplicata é inaceitável.*

- **Retained** — o broker guarda a última mensagem do tópico e a entrega imediatamente a
  quem assinar depois: painel que liga já mostra o estado atual, sem esperar o próximo
  ciclo de publicação.
- **LWT (*last will*)** — o "testamento": mensagem que o **broker** publica em nome do
  cliente se a sessão cair sem despedida (keepalive estourado). Resolve o problema que o
  TCP sozinho não resolve: os *outros* interessados saberem que o nó morreu — o painel
  mostra "nó 3 offline" em vez de um valor congelado e mentiroso.

**Exemplo resolvido 14.2 (projeto de tópicos e QoS)** — Estufa com 6 nós, cada um
publicando temperatura e umidade a cada 10 s, e recebendo comando de ventilador. Proposta:
`estufa/no<i>/temp` e `estufa/no<i>/umid` em **QoS 0** (perder uma leitura não importa — em
10 s vem outra) e `estufa/no<i>/cmd/vent` em **QoS 1 + retained** (comando não pode se
perder; nó que reinicia recebe o último estado ao reassinar). O painel assina
`estufa/+/temp`; a auditoria, `estufa/#`. Tráfego: 12 msgs/10 s ≈ 1,2 msg/s — desprezível
até para um Zero 2 W como broker. Repare no critério, não na receita: *o QoS segue a
semântica do dado*, e retain só onde "último estado" tem sentido (num comando de
incremento, retain seria um bug — o nó recém-ligado executaria um incremento velho).

> 💡 **Pense aí — quantos tópicos é demais?** Cada tópico é grátis no MQTT (não há
> registro), mas cada *assinatura* custa memória no broker e cada curinga larga (`#` na
> raiz) inunda quem assina. Regra prática: hierarquia com significado (`local/nó/grandeza`),
> curingas para agregação, e jamais publique dados distintos no mesmo tópico ("temp" que às
> vezes é umidade é receita de painel quebrado).

## 4. O firmware do nó, pelos olhos de quem já cursou as semanas 1–13

O `no_mqtt/main.c` é a disciplina inteira num arquivo — leia-o com este mapa:

```c
mqtt_start():
  .session.last_will = { .topic = T_STATUS, .msg = "offline", .qos = 1, .retain = 1 }
  .session.keepalive = 5                       // broker declara morte após ~7-10 s
                                               // → LWT: seção 3, demonstrado no lab (B.5)
mqtt_handler(), em MQTT_EVENT_CONNECTED:
  esp_mqtt_client_publish(cli, T_STATUS, "online", 0, 1, 1);   // status retained
  esp_mqtt_client_subscribe(cli, T_CMD_LED, 1);                // comandos em QoS 1 (Ex. 14.2)
  xEventGroupSetBits(eg, BIT_MQTT_OK);                         // acorda a app (Ex. 14.1)
mqtt_handler(), em MQTT_EVENT_DATA:
  gpio_set_level(PINO_LED, ...);               // comando via tópico vira nível de pino: sem. 3
app_main(), laço principal:
  dht11_ler(&t, &u);                           // bit-banging de µs: sem. 4 (bordas + carimbos)
  esp_mqtt_client_publish(cli, T_TEMP, msg, 0, 0, 0);   // telemetria QoS 0
  vTaskDelayUntil(&prox, pdMS_TO_TICKS(5000)); // período cravado: sem. 5
```

> **Observação — credenciais no código?** Só em bancada. Em produto, SSID/senha vivem na
> **NVS** (semana 2) gravados em provisionamento — nunca hardcoded num binário que vai para
> o GitHub. Já retire as suas antes de commitar o lab. (E o Wi-Fi do lab usa SSID dedicado
> com senha de bancada, não a sua senha pessoal.)

## 5. As contas que fecham o produto

**Exemplo resolvido 14.3 (autonomia com deep sleep)** — Nó a bateria (2000 mAh) que acorda,
mede, publica e dorme. Ciclo: 5 s ativo a 80 mA (o rádio!) a cada 5 min; dormindo, 10 µA.

*Passo a passo* — a mesma mecânica do Exemplo 1.1 (semana 1!), agora com o vilão
identificado:

1. Carga ativa: 5 s × 80 mA = 400 mA·s
2. Carga dormindo: 295 s × 0,01 mA = 2,95 mA·s
3. I_médio = (400 + 2,95)/300 ≈ **1,34 mA** → 2000/1,34 ≈ 1490 h ≈ **62 dias**

O rádio manda no consumo (98,5 % da carga do ciclo é dos 5 s acordados!), e o padrão de
projeto é "acorda–mede–publica–dorme", com o estado sobrevivendo na RTC RAM (semana 2).
Quem otimiza autonomia otimiza **tempo acordado**: conexão rápida (DHCP estático ou IP fixo
economiza segundos de handshake), QoS 0, payload curto. Um nó que fica 30 s procurando Wi-Fi
queima 6× mais bateria que um que conecta em 5 — a configuração da rede **é** especificação
de energia.

**Exemplo resolvido 14.4 (escolhendo o transporte)** — Dashboard com 4 leituras/s e um
botão de comando; três candidatos do mundo web/IoT: **REST** (HTTP: pergunta→resposta)
obrigaria o painel a fazer *polling* — 4 requisições/s × overhead de HTTP ≈ kB/s
desperdiçados e latência de até um período; **WebSockets** mantém um canal aberto com
*push* — ótimo navegador↔servidor; **MQTT** dá o push *e* o desacoplamento multi-cliente (N
painéis assinam sem mudar o nó) *e* LWT/retain de graça. Regra da casa: **dispositivo↔
dispositivo ⇒ MQTT; navegador↔servidor ⇒ WebSockets; consulta esporádica/integração ⇒
REST**. (A trilha full-stack — `docs/` — implementa os três com o livro do Smart; compare
os tráfegos e tire a prova.)

E como fica um teste ponta a ponta no terminal — publicador e assinante em janelas lado a
lado:

![Teste local de MQTT com mosquitto_pub e mosquitto_sub](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mqtt_teste_local.png)

*Figura 14-E — O "multímetro" do MQTT: uma janela assina (`mosquitto_sub`), outra publica
(`mosquitto_pub`), e a mensagem atravessa o broker. Fonte: Raspberry Pi and MQTT Essentials
(Packt), cap. 1, Fig. 1.45.*

## 6. O broker no RPi 3 (e a espiada no futuro)

`sudo apt install mosquitto mosquitto-clients` e o RPi vira o centro nervoso. A partir do
Mosquitto 2.x, liberar a rede local exige duas linhas em `/etc/mosquitto/conf.d/lab.conf`
(`listener 1883` + `allow_anonymous true` — **apenas em laboratório**; produto usa
usuário/senha e TLS). As ferramentas `mosquitto_sub -t 'ifes/#' -v` e `mosquitto_pub` são o
"multímetro" do MQTT — teste o barramento antes de culpar o firmware, exatamente como o
`i2cdetect` da semana 12 testava o barramento antes de culpar o sensor.

E daqui para frente, três estradas mapeadas para o projeto final: **OTA** (atualização de
firmware pelo ar: partições `ota_0/ota_1` + `esp_https_ota` — o produto se atualiza sem
visita técnica); **ML embarcado** (classificação no próprio nó — a trilha dirigida
`docs/trilha-tinyml.md` leva do zero a um classificador de movimento no ESP32 com o Edge Impulse,
usando o MPU-6050 da semana 9; há um livro sobre deep learning em Jetson/RPi no nosso
acervo); **Home Assistant** (domótica pronta sobre nós MQTT — livro no acervo). A trilha
dirigida `docs/trilha-fullstack.md` leva o grupo interessado do zero ao painel web
profissional.

![Exemplo de aplicação MQTT completa: sensores, broker e controle remoto](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/iot_mqtt_visao_geral.png)

*Figura 14-F — Para onde essa arquitetura cresce: um robô teleoperado com sensores
publicando e recebendo comandos via MQTT — o mesmo desenho do nosso lab, com outra roupa.
Fonte: Internet of Things Programming Projects, 2. ed. (Packt), cap. 1, Fig. 1.13.*

## Resumindo

- Arquitetura em camadas: MCU na ponta (tempo real + µA), Linux na borda (agregação +
  serviço) — a divisão que as semanas 11–13 justificaram.
- Wi-Fi no ESP-IDF é orientado a eventos; a app sincroniza com *event group* (BIT_GOT_IP,
  BIT_MQTT_OK — Exemplo 14.1); reconexão vive no handler, nunca em polling.
- MQTT: broker no centro, tópicos hierárquicos com `+`/`#`, desacoplamento total; QoS segue
  a semântica do dado, retain onde "último estado" faz sentido, LWT denuncia nó morto
  (Exemplo 14.2).
- Autonomia = duty cycle do rádio: acorda–publica–dorme dá 62 dias onde o rádio ligado daria
  1 (Exemplo 14.3, ecoando o 1.1); tempo acordado é o alvo da otimização.
- Transporte certo: MQTT entre dispositivos, WebSockets para navegador, REST para consulta
  (Exemplo 14.4).
- Mosquitto no RPi + `mosquitto_sub/pub` como instrumento de bancada; credenciais fora do
  código; OTA/ML/Home Assistant como próximos passos mapeados.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| broker | servidor central que roteia mensagens MQTT |
| tópico | string hierárquica que identifica o assunto |
| publish / subscribe | publicar / assinar tópicos |
| QoS 0/1/2 | níveis de garantia de entrega |
| retained | última mensagem guardada pelo broker |
| LWT | testamento publicado pelo broker se o nó cair |
| keepalive | batimentos que mantêm a sessão viva |
| event loop | laço de eventos do ESP-IDF |
| deep sleep | sono profundo do ESP32 (µA) |
| OTA | atualização de firmware pelo ar |

## 📖 Onde aprofundar (opcional)

- *Raspberry Pi and MQTT Essentials* (Packt), caps. 1–3 — broker, conexão e publicação com
  telas (entre os PDFs da disciplina; origem de várias figuras desta aula).
- **Smart**, *Practical Python Programming for IoT*, caps. 3–4 — REST/WebSockets e MQTT em
  Python: a outra ponta da nossa arquitetura (entre os PDFs).
- ESP-IDF Programming Guide: *Wi-Fi Driver* e *ESP-MQTT*.
- *Building Smart Home Automation Solutions with Home Assistant* (Packt) — para a estrada da
  domótica (entre os PDFs).

## Exercícios

Lista 5, questões 7–12 (estilo dos Exemplos 14.1–14.4).
