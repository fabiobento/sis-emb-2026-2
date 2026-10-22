# Guia de problemas — Classificador de movimento (Edge Impulse + ESP32)

Tabelas de **sintoma → causa provável → como testar → correção**, na ordem em que os problemas
costumam aparecer. Os diagnósticos vêm da lógica do sistema e de erros comuns de integração; os
trechos sobre o Studio e a CLI dependem da versão — confira a
[documentação oficial](https://docs.edgeimpulse.com/) se algo divergir.

> 🔎 **Método:** o sistema tem 4 elos — sensor → firmware de coleta → Studio → firmware de
> inferência. Descubra **em qual elo** está o problema antes de mudar qualquer coisa (veja o
> fluxograma da Figura L-7 no [lab](lab-classificador.md)) e mude **uma coisa por vez**.

## 1. Sensor e firmware de coleta

| Sintoma | Causa provável | Como testar | Correção |
|---|---|---|---|
| `WHO_AM_I=0x00` ou `0xFF` | fiação, endereço, sem alimentação | scanner I²C da semana 9 | refazer SDA/SCL/3V3/GND; `AD0` em GND ⇒ `0x68`; fios curtos |
| `WHO_AM_I=0x70`/`0x71`, valores estranhos | módulo é MPU-6500/9250, não MPU-6050 | ler o chip no módulo | muitos registradores coincidem, mas confira o datasheet; o `0x68` esperado é do MPU-6050 |
| Valores fixos (sempre iguais) | sensor em *sleep* | `REG_PWR` (0x6B) precisa ser 0 | o firmware já escreve 0; verifique se `wr()` retorna `ESP_OK` |
| `aZ ≈ 1` mas **gesto não muda nada** | I²C falhando e o firmware repetindo a última amostra | contar erros (`perdidas_i2c` na inferência) | mau contato; baixe o clock para 100 kHz |
| Taxa medida ≠ 100 Hz (`gravar_csv.py` avisa) | `CONFIG_FREERTOS_HZ` ≠ 1000, console lento, cabo USB ruim | `idf.py menuconfig` → FreeRTOS tick rate | tick de 1000 Hz ⇒ período = 10 ticks; troque o cabo/porta |
| Amostras saturadas em ±2,0 g | gesto vigoroso > 2 g | aviso do `gravar_csv.py` | `MPU_RANGE 1` (±4 g) **nos dois firmwares**; regravar tudo |
| Linhas truncadas/`ets …` no meio | boot, *brownout*, reset | ver o monitor | alimentação estável; o parser ignora linhas inválidas |

## 2. Serial e *data forwarder*

| Sintoma | Causa provável | Correção |
|---|---|---|
| `Permission denied: /dev/ttyUSB0` (Linux) | usuário fora do grupo `dialout` | `sudo usermod -aG dialout $USER` e relogar |
| `Device or resource busy` | o `idf.py monitor` (ou outro programa) está com a porta | feche o monitor (Ctrl+]) antes do forwarder/`gravar_csv.py` |
| Forwarder não detecta eixos | linhas não numéricas no início; baud errado | o firmware usa 115200; o forwarder lê esse valor por padrão — confira na documentação se precisa de `--baud-rate` |
| Forwarder detecta 1 eixo só | separador errado (`;` em vez de `,`) | o firmware usa `,` — não troque |
| Aviso de frequência no Studio | a taxa real não é a que você informou | rode `gravar_csv.py` e veja a taxa medida |
| No Windows | a porta é `COMx` | use `--porta COM5` |

## 3. Dados e Studio

| Sintoma | Causa provável | Como testar | Correção |
|---|---|---|---|
| Classes misturadas no Feature explorer | gestos parecidos; pouca variação; orientação diferente por gravação | `pipeline_host.py dados` (baseline local) | gestos mais distintos; orientação fixa; mais exemplos |
| Feature explorer agrupa **por pessoa**, não por classe | só a inclinação difere entre pessoas | Figura C-8 (direita) | confira se o *Spectral Features* está ativo e se o sinal é dominado por CA |
| Acurácia de teste ≈ 100% fácil demais | vazamento (gravações em treino e teste) ou classes fáceis | Figuras C-5/C-6 | reservar **pessoa inteira** para teste |
| Treino 99%, teste 70% | *overfitting* | Exemplo C.5 | mais dados/pessoas; menos épocas; rede menor |
| Uma classe com *recall* baixo | exemplos insuficientes ou parecidos com outra | matriz de confusão (Exemplo C.6) | mais exemplos dessa classe; torná-la mais distinta |
| `parado` vira refúgio do modelo | `parado` com muito mais dados que as outras | contar minutos por classe | balancear minutos por classe |
| "Not enough data" no treino | poucas janelas | olhar *Data acquisition* | mais gravações; janela menor |

## 4. Integração: o Studio acerta, o ESP32 não (teste de sanidade falha)

Faça estes testes **nesta ordem**: cada um elimina uma família de causas.

| # | Teste | Se falhar |
|---|---|---|
| 1 | Cole as *raw features* de uma janela no `features[]` e compare com o Studio | Se **bate**, a biblioteca está certa; o problema é o que o firmware ao vivo alimenta (itens 2–5). Se **não bate**, a biblioteca/projeto está mal montado: pastas copiadas na raiz? versão da biblioteca = versão do projeto no Studio? Reexporte e recompile (`idf.py fullclean`) |
| 2 | `static_assert` do `classificador.h` compila? | A janela/frequência/eixos do firmware ≠ Impulse. Acerte `projeto_config.h` |
| 3 | Ordem e unidade dos eixos | x, y, z **em g**, intercalados (`x0 y0 z0 x1 y1 z1 …`). Em **m/s²** ou `y,x,z` o modelo recebe "outra coisa" |
| 4 | Configuração do sensor | `MPU_RANGE` e `MPU_DLPF` iguais aos da **coleta**; mudou algo desde que gravou? |
| 5 | Orientação física do sensor | a mesma da coleta (Figura L-3) |

## 5. Firmware de inferência

| Sintoma | Causa provável | Correção |
|---|---|---|
| `Stack canary watchpoint triggered (inferencia)` | pilha pequena | aumente de `16384` em `xTaskCreate(inferencia_task…)` |
| `region 'dram0_0_seg' overflowed` ao linkar | modelo/arena grande demais | veja RAM estimada no Studio; reduza rede/features, quantize (int8) |
| `Task watchdog … IDLE0` | tarefa ocupando a CPU sem ceder | `amostra_task` usa `vTaskDelayUntil`; veja se alguma tarefa fica em laço sem bloquear |
| `[janela]` com intervalo irregular | `amostra_task` atrasada (inferência longa, prioridades) | `amostra_task` prioridade **maior**; confira `dsp+clf` ≪ 500 ms |
| `perdidas_i2c` cresce | I²C instável | fios; pull-ups; clock 100 kHz |
| Compila com `USAR_EI=0`, falha com `USAR_EI=1` | includes/CMake da biblioteca | refaça a Parte E (exemplo oficial compila sozinho?) e só depois junte os arquivos do lab |
| Primeira saída só após ~3 s | é o esperado: janela de 2 s + `N_CONFIRMA` | teoria §8 |
| Saída "pisca" entre classes | limiar baixo, `N` pequeno | `LIMIAR_CONF` ↑, `N_CONFIRMA` ↑ |
| Demora para mudar de classe | `N` grande, janela longa | `N_CONFIRMA` ↓, `STRIDE_MS` ↓ (teoria §8) |
| Nada muda mesmo com gestos | `conf < LIMIAR_CONF` sempre (modelo inseguro) | olhe as probabilidades em `[janela]`; baixe o limiar só para diagnóstico |

## 6. Wi-Fi e MQTT (semana 14)

| Sintoma | Causa provável | Correção |
|---|---|---|
| Fica esperando IP | SSID/senha; rede 5 GHz (ESP32 só usa 2,4 GHz) | conferir `projeto_config.h`; rede 2,4 GHz |
| Fica esperando MQTT | IP do broker errado; Mosquitto só em `localhost` | `hostname -I` no RPi; `listener 1883` e `allow_anonymous true` conforme a semana 14 |
| `mosquitto_sub` não mostra nada | tópico ≠ | assine `ifes/#`; compare `BANCADA` |
| `status offline` aparece sozinho | queda do Wi-Fi / `keepalive` | normal se a placa reinicia; é o LWT funcionando |
| Painel antigo mostra classe velha | mensagem **retida** | é intencional (estado atual); limpe com `mosquitto_pub -r -n -t ifes/bancadaN/movimento` |

## 7. Quando tudo falha

1. Rode o **teste no PC**: `tests/test_logica.c` e `tests/test_stub.cpp` — se passarem, a lógica de
   janela/*debounce* está certa.
2. Volte ao **Lab 9** (o MPU-6050 lê?) e à **Parte E** (o exemplo oficial do Edge Impulse compila
   e passa na sanidade?) separadamente. Só então junte.
3. Registre o que testou no relatório: um diagnóstico bem feito vale mais do que um sistema que
   "funcionou por sorte".
