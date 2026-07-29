# Lab Extra — Medidor de Energia Elétrica (PZEM-004T v3.0 + CT)

Lab integrador **opcional/avançado** que une UART/Modbus (semana 9), CA e RMS/potência (PDS,
semanas 7 e 13), Linux embarcado (11–12) e IoT/MQTT (14) num sistema que mede o consumo elétrico
real de aparelhos e publica o custo em R$.

> ⚠️ **Envolve a rede de 220 V.** O lado de corrente alternada é montado, isolado e energizado
> **exclusivamente pelo professor**. Alunos manuseiam **somente o lado TTL** (5V/GND/RX/TX).
> Detalhes no topo de `teoria.md`.

## Conteúdo

- `teoria.md` — CA e valor RMS, potência ativa/aparente/reativa e fator de potência, o
  transformador de corrente (CT) de núcleo partido, e o protocolo Modbus-RTU completo (frame,
  mapa de registradores, CRC-16). Formato tutorial, com exemplos resolvidos E.1–E.4 e mini-lista.
- `lab.md` — roteiro guiado: reconhecimento do protocolo, leitura no ESP32, o experimento das
  três cargas (incandescente × LED × ventilador, que prova a diferença ativa/aparente), custo em
  R$ na borda e o caminho alternativo RPi-direto.
- `src/pzem_esp32/main.c` — firmware ESP-IDF: lê o PZEM por UART/Modbus e publica em `energia/#`
  via MQTT.
- `src/rpi/pzem_rpi.py` — leitura direta no RPi por adaptador USB-TTL (Modbus "na mão" +
  publicação MQTT).
- `src/rpi/painel_energia.py` — assina `energia/#`, grava CSV e calcula custo acumulado em R$.

## Hardware (o que você comprou)

- **PZEM-004T v3.0 (modelo 100 A)** — medidor CA por Modbus-RTU/TTL: V (80–260 V), I (até 100 A),
  P ativa, energia (kWh), frequência, fator de potência.
- **Transformador de corrente PZKHCT 0–100 A**, relação **1000:1**, burden 10 Ω, **núcleo
  partido** (garra) — mede corrente por indução, isolado da rede.

## Pré-requisitos e onde encaixa

Melhor depois da semana 14 (usa Wi-Fi/MQTT), mas a leitura Modbus no ESP32 pode ser demonstrada
já após a semana 9. Serve como **lab extra**, como **aprofundamento de UART na semana 9** (ver a
nota no fim de `../../semana-09/teoria.md`) e como **kit de projeto final** (temas em
`lab.md` e em `../../projeto-final/README.md`).
