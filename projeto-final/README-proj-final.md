# Projeto Final Integrador — Regulamento e Rubrica

## Objetivo
Projetar, implementar e documentar um **sistema embarcado completo** que integre, no mínimo:
**sensoriamento + processamento/controle + atuação ou comunicação**. Combinações ESP32 + RPi
(nó MCU + borda Linux, como na semana 14) são fortemente incentivadas, mas não obrigatórias.

## Regras
- **Grupos de 3–4 alunos**, definidos até a **semana 10** com entrega de proposta de 1 página
  (problema, diagrama de blocos, lista de componentes do inventário, cronograma interno).
- Aprovação da proposta pelo professor na própria semana 10 (viabilidade × acervo do lab).
- Desenvolvimento acompanhado nas semanas 10–14 (parte do horário de laboratório reservada).
- **Repositório GitHub obrigatório desde a proposta**: commits distribuídos ao longo das semanas
  (não vale um único commit na véspera), README com descrição, esquemático (Fritzing/foto
  legível/Wokwi), instruções de compilação e execução, e vídeo curto (≤2 min) da demonstração.
- Hardware: inventário do laboratório.
- Simulação Wokwi aceita como **complemento** (validação), nunca como substituto da demonstração
  em hardware — exceto para partes comprovadamente inviáveis com o acervo.

## Sugestões de tema
1. **Estação meteorológica MQTT**: 2–3 nós ESP32 (DHT11 + LDR) → Mosquitto no RPi + painel.
2. **Controle de nível/irrigação**: sensor de umidade de solo + bomba d'água via relé, PID/histerese
   no ESP32, supervisão no RPi.
3. **Fechadura RFID**: RC522 (SPI) + servo + registro de acessos no RPi (CSV/SQLite).
4. **Mini-esteira/robô 2WD**: chassi + L298N + encoder KY-040, controle de velocidade PID,
   comando via MQTT.
5. **Monitor de vibração**: MPU-6050 a 100 Hz + FFT/limiares no RPi; alarme via buzzer — quem
   quiser trocar os limiares por um classificador treinado tem a
   [trilha TinyML](../docs/trilha-tinyml.md) (Edge Impulse, o mesmo MPU-6050 no ESP32).
6. **Domótica com Home Assistant** no RPi + nós ESP32 MQTT (livro *Building Smart Home
   Automation Solutions with Home Assistant* como guia).
7. **Painel full-stack**: nós ESP32 → Mosquitto → Flask + SQLite + gráficos servidos por
   nginx/uWSGI no RPi 3, seguindo a trilha dirigida em `docs/trilha-fullstack.md`
   (livros-guia: Dalmaris, *Raspberry Pi Full Stack*, e Smart, *Practical Python Programming
   for IoT*, caps. 3–4 e 14).
8. **Monitor de energia elétrica** com PZEM-004T + transformador de corrente: mede consumo real
   (V, I, potência ativa, kWh, fator de potência) por **Modbus-RTU**, publica via MQTT e mostra
   custo em R$ / eficiência num painel (casa com a trilha full-stack). Base pronta em
   `labs-extra/medidor-energia/`. **Requer supervisão do professor na parte de 220 V.**

## Rubrica (30% da nota semestral)
| Critério | Peso | O que se avalia |
|---|---|---|
| Funcionamento | 40% | Sistema demonstra o ciclo completo sensor→processamento→ação; robustez na demo; tratamento de erros básicos. |
| Qualidade técnica | 25% | Uso correto dos conceitos da disciplina (RTOS/tarefas, ISR, protocolos, controle); código organizado e comentado; dimensionamento elétrico correto. |
| Documentação (GitHub) | 20% | README reproduzível, esquemático, histórico de commits distribuído, vídeo. |
| Apresentação | 15% | Clareza, domínio do grupo às perguntas, gestão do tempo (10+5 min). |

Nota individual pode ser ajustada (±20%) pelo histórico de commits e pela arguição.

## Datas
| Semana | Marco |
|---|---|
| 10 | Entrega e aprovação da proposta |
| 12 | Checkpoint 1: hardware montado + leitura de sensores |
| 14 | Checkpoint 2: integração/comunicação funcionando |
| 15 | Demonstração final + repositório congelado (tag `v1.0`) |
