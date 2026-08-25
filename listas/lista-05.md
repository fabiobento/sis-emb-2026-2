# Lista de Exercícios 05 — Semanas 13 e 14
**Temas:** PDS embarcado e PID; Wi-Fi, MQTT e arquitetura IoT.
**Entrega:** Por bancada, em PDF ou markdown no GitHub, até o primeiro encontro da semana 15
(é preparação direta para a P2).

## 📚 Como estudar para esta lista

1. Refaça os exemplos resolvidos **13.1–14.4** sem olhar. Em especial: o Exemplo 13.1 (taxa de
   amostragem), o 13.2 (média móvel com zero em 60 Hz), o 13.3 (PID iterado à mão), o 14.2
   (árvore de tópicos), o 14.3 (autonomia de bateria) e o 14.4 (REST × WebSocket × MQTT) —
   são cobrados quase literalmente.
2. As figuras de apoio de cada questão aparecem junto do enunciado.

3. Aprofundamento opcional: *RPi and MQTT Essentials* caps. 1–3; *IoT from Scratch* caps. 2–3;
   revisão de Análise de Sinais e Sistemas (amostragem, Nyquist, resposta em frequência).

## Parte A — PDS e controle embarcado (semana 13)
**Q1.** *(estilo Exemplo 13.1)* Um processo térmico tem constante de tempo ≈ 2 s. Proponha a
taxa de amostragem do laço de controle (regra prática 10–30× a banda) e explique por que
amostrar a 10 kHz seria desperdício **e** problema (ruído, custo, derivada).
> 💡 *Banda ≈ 1/τ = 0,5 Hz → T_s entre ~20 e 60 ms. No item "problema", lembre de que o termo
> derivativo amplifica ruído de alta frequência — amostrar rápido demais é dar palco para ele.*

![Diagrama de blocos de uma malha fechada de controle com realimentação](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/malha_fechada.png)

**Q2.** *(estilo Exemplo 13.2)* Sinal útil até 5 Hz com ruído de rede em 60 Hz, f_s = 500 Hz.
Projete uma média móvel que atenue fortemente 60 Hz (dica: zeros da média móvel em f_s/M) e dê
o atraso de grupo resultante em ms.
> 💡 *Queremos um zero em 60 Hz: f_s/M = 60 → M = 500/60 ≈ 8,33... como M precisa ser inteiro,
> refaça a conta exata do Exemplo 13.2 e veja como o exemplo resolve o arredondamento. O atraso
> de grupo da média móvel é (M−1)/2 amostras — converta para ms com T_s = 1/f_s.*

**Q3.** Escreva a lei do PID posicional discreto (com T_s explícito) e explique o papel de cada
termo na malha de luminosidade do Lab 13 (o que cada ganho "conserta" e o que estraga em excesso).
> 💡 *A Fig. 13-B da teoria-13 é o resumo em uma imagem: P tira o sistema da inércia mas deixa
> erro permanente; I zera o erro mas traz oscilação; D amortece mas odeia ruído. Na sua resposta,
> cite um sintoma observado na bancada para cada ganho exagerado.*

![Respostas ao degrau de controladores P, PI e PID comparadas](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/pid_respostas.png)

**Q4.** *(estilo Exemplo 13.3)* Com K_p=1,2; K_i=3,0; K_d=0; T_s=0,02 s; referência 60;
medições consecutivas 40, 46: calcule u nas duas iterações (integrador começa em 0) e indique a
saturação se u ∈ [0, 100].
> 💡 *Iteração 1: e = 60 − 40 = 20; u = 1,2·20 + 3,0·0,02·(0 + 20). Iteração 2: e = 14 e o
> integrador já acumula 20 + 14. Faça as contas completas e compare com o Exemplo 13.3 — ele é
> literalmente este roteiro com outros números.*

**Q5.** O que é *windup* do integrador, em que situação do nosso lab ele aparece (referência
impossível com o copo aberto) e como o anti-windup por *clamping* funciona?
> 💡 *No Lab 13, Parte C, o LED no máximo não conseguia levar o lux ao alvo e o integrador
> "enchia" sem parar — ao abrir o copo, a saída demorava segundos para voltar. O clamping
> simplesmente para de integrar quando u está saturado e o erro tem o mesmo sinal da integral.
> Releia o quadro "Observação" da seção de PID na teoria-13.*

**Q6.** Por que jitter de amostragem degrada o termo derivativo em especial? Relacione com a
escolha de `vTaskDelayUntil` (semana 5) e com o motivo de a malha rodar no ESP32 e não no RPi.
> 💡 *O termo D é (e[k] − e[k−1])/T_s — se o T_s real oscila, o denominador que você usou na
> conta não é o que aconteceu no mundo, e o erro relativo explode porque Δe é pequeno. Amarre
> as três pontas: Fig. 5-C da teoria-05 (vTaskDelay × vTaskDelayUntil), Exemplo 11.1 (jitter no
> Linux) e a decisão de arquitetura da Fig. 14-A (malha no MCU, RPi só supervisão).*

![Comparação entre vTaskDelay (período relativo, com deriva) e vTaskDelayUntil (período absoluto)](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/vtaskdelay_vs_until.png)

## Parte B — IoT e MQTT (semana 14)
**Q7.** Explique o modelo publish–subscribe e três vantagens sobre conexões diretas
sensor→servidor (desacoplamento, escala, resiliência). Onde o broker roda na nossa arquitetura?
> 🖼️ Use a **Fig. 14-B da teoria-14** (fluxo MQTT, abaixo) como esqueleto da resposta e situe o
> broker no RPi 3 da bancada na **Fig. 14-A** (arquitetura, também abaixo).
> (Opcional: *RPi and MQTT Essentials*, Fig. 1.1, cap. 1, p. 5 — é a fonte da nossa Fig. 14-B —
> e *IoT from Scratch*, Fig. 3.4, cap. 3, p. 75.)

![Fluxo básico de comunicação MQTT entre publicadores, broker e assinantes](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mqtt_fluxo_basico.png)

![Arquitetura completa: nó ESP32, broker no Raspberry Pi e clientes externos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/arquitetura_esp32_rpi.png)

**Q8.** *(estilo Exemplo 14.2)* Projete a árvore de tópicos para um estacionamento com 3 pisos ×
20 vagas (sensor por vaga) + cancela comandável. Dê exemplos com curingas `+` e `#` para: painel
de um piso; auditoria geral; comando da cancela — e atribua QoS/retain justificando.
> 💡 *Estruture como `<local>/<piso>/<vaga>/<grandeza>` e pense em quem assina o quê: o painel do
> piso 1 quer `estac/p1/+/ocupada`; a auditoria quer `estac/#`. O retain é o que faz um painel
> recém-ligado já mostrar o mapa sem esperar 60 vagas se manifestarem. Compare com a árvore do
> Exemplo 14.2 antes de entregar.*

![Visão detalhada do MQTT: cliente, broker, tópicos e sessões](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mqtt_overview.png)

**Q9.** Diferencie QoS 0, 1 e 2 (garantia, custo, duplicatas) e dê um exemplo de dado do nosso
laboratório adequado a cada um.
> 💡 *A **Fig. 14-D da teoria-14** (QoS, abaixo) mostra o aperto de mãos de
> cada nível. Para os exemplos, pense na telemetria de luminosidade a 2 Hz (perder uma amostra
> não importa) × no comando da cancela da Q8 × numa contagem de passagem que não pode duplicar.*

![Os três níveis de QoS do MQTT: 0, 1 e 2](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mqtt_qos.png)

**Q10.** O que é LWT (*last will*) e por que ele resolve o problema "o nó morreu ou só está
calado?" que o TCP sozinho não resolve? Como demonstramos isso no Lab 14?
> 💡 *Lembre do Lab 14, Parte D: desligamos o ESP32 no meio e o tópico `.../status` virou
> `offline` sozinho, sem ninguém publicar. Explique quem guarda o testamento (o broker), quando
> ele é executado (queda anormal da sessão) e por que um keep-alive de TCP não distingue "nó
> saudável em silêncio" de "nó morto".*

![Teste local de MQTT com mosquitto_pub e mosquitto_sub](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mqtt_teste_local.png)

**Q11.** *(estilo Exemplo 14.3)* Nó com bateria de 1500 mAh acorda a cada 2 min, fica 3 s ativo
a 90 mA e dorme a 15 µA. Calcule a autonomia em dias. Repita para acordar a cada 10 min e
conclua qual parâmetro domina o projeto.
> 💡 *I_medio = (t_ativo·I_ativo + t_sono·I_sono)/(t_ativo + t_sono). Note que 15 µA parece
> desprezível, mas some 120 s dele contra 3 s de 90 mA — o Exemplo 14.3 mostra a surpresa. O
> "parâmetro que domina" é justamente o que essa soma revelar.*

**Q12.** No firmware da semana 14, a conexão usa *event group* (bits GOT_IP e MQTT_OK).
Explique o fluxo de eventos Wi-Fi→IP→MQTT, por que a tarefa principal **bloqueia** (em vez de
laço de espera) e o que acontece na reconexão após queda do Wi-Fi.
> 💡 *Desenhe a linha do tempo: `WIFI_EVENT/START` → conectar → `IP_EVENT/GOT_IP` → seta bit →
> handler MQTT → `MQTT_EVENT_CONNECTED` → seta bit. O bloqueio em `xEventGroupWaitBits` é a
> semana 6 reaparecendo: CPU zero enquanto espera. Para a reconexão, cite o comportamento que
> observamos na Parte D do Lab 14 (quais bits caem, quais handlers rodam de novo).
> Opcional, para ver os pacotes "no ar": *RPi and MQTT Essentials*, Figs. 2.5 (p. 45), 2.8–2.9
> (p. 48–49) e 2.16 (p. 56, Wireshark), cap. 2.*

## Bônus (opcional, +0,5 pt)
**Q13.** *(estilo Exemplo 14.4)* Seu projeto final terá um dashboard no navegador com leituras a
2 Hz e um botão liga/desliga. Compare **REST com polling** × **WebSocket** × **MQTT sobre
WebSocket** para esse caso (tráfego, latência, complexidade) e escolha justificando.
> 💡 *O Exemplo resolvido 14.4 da teoria-14 faz exatamente essa comparação com números (bytes
> por segundo de cabeçalho HTTP a 2 Hz × um frame WebSocket). A Fig. 14-A mostra onde cada
> protocolo vive na nossa pilha. Quem seguir a trilha full-stack (`docs/trilha-fullstack.md`)
> implementa as três nas Etapas 1–3 — vale citar a experiência. Referências opcionais: *Smart*,
> cap. 3 (Figs. 3.1, p. 82 e 3.2, p. 97) e o diagrama da pilha completa em *Dalmaris*, Fig.
> 40.51, cap. 40, p. 131.*
