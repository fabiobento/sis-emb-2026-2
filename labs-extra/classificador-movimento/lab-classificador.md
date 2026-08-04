# Roteiro — Lab Extra: Classificador de movimento no ESP32 (Edge Impulse + MPU-6050)

Lab integrador **opcional/avançado** (~2 aulas). Você treina um classificador de movimento na
plataforma web Edge Impulse e o roda **na borda**, no ESP32, com o MPU-6050 da semana 9 — fechando
o ciclo sensor → DSP → inferência → ação. Ao final, o nó publica a classe detectada por MQTT no
painel do RPi (semana 14).

**Pré-requisitos:** semanas 5 (tarefas), 7 (amostragem/PDS), 9 (I2C/MPU-6050) e 14 (MQTT). Leia
antes a `teoria-classificador.md` deste diretório. Material de fundo e visão ampla: a
[trilha TinyML](../../docs/trilha-tinyml.md).

**O que você precisa:** a bancada padrão (ESP32 + MPU-6050 ligado por I2C, como no Lab 9), a
[Edge Impulse CLI](https://docs.edgeimpulse.com/tools/clis/edge-impulse-cli) instalada, e uma conta
gratuita no [Edge Impulse](https://studio.edgeimpulse.com) (o campus tem acesso — confirme a conta
com o professor).

> As classes deste roteiro são **parado / aceno / sobe-desce / círculo**. Você pode trocá-las (ex.:
> estados de vibração de um motor), mas comece com estas quatro, que são bem separáveis.

## Parte A — Coleta (aula 1, primeira metade)

1. **Grave o coletor.** Compile e grave `src/coleta_mpu/main.c`. Ele lê o acelerômetro e imprime
   `aX,aY,aZ` por linha, a 100 Hz cravados (`vTaskDelayUntil`). Confira no monitor serial que saem
   ~100 linhas por segundo.

   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

2. **Confira a taxa (semana 7).** Por que 100 Hz e não 10 Hz nem 1 kHz? Escreva a justificativa por
   Nyquist no relatório (Exemplo C.1 da teoria).

3. **Ligue o data forwarder.** Feche o monitor e rode a CLI, que encaminha a serial para o Studio:

   ```bash
   edge-impulse-data-forwarder --frequency 100
   ```

   Quando perguntar os nomes dos eixos, informe `accX,accY,accZ`. O ESP32 aparece como dispositivo
   no seu projeto do Edge Impulse.

4. **Grave as amostras.** Na aba **Data acquisition** do Studio, grave janelas de ~10 s para cada
   classe (`parado`, `aceno`, `sobe-desce`, `círculo`). **Cada integrante da bancada grava as suas**, e
   varie a posição do sensor (mão, pulso). Meta: pelo menos ~2–3 min por classe, e reserve ~20% para
   teste.

   > 🛠️ **Poucos dados, todos iguais** é a receita do fracasso: o modelo decora e falha na demo.
   > Variedade > quantidade bruta.

## Parte B — Impulse, treino e avaliação (aula 1, segunda metade)

5. **Monte o Impulse** (aba *Create impulse*): bloco de *Time series* (janela ~2 s, passo ~80 ms,
   100 Hz) → bloco de processamento **Spectral Features** (FFT) → bloco de aprendizado
   **Classification**.

6. **Gere as features** e abra o **Feature explorer**. As quatro classes aparecem em nuvens
   separadas? Se sim, o DSP fez o trabalho (teoria, §2). Se estão embaralhadas, volte à Parte A: mais
   dados, movimentos mais distintos. **Anote um print do Feature explorer no relatório.**

7. **Treine** (aba *Classifier*) e leia:
   - a **acurácia de validação** e, principalmente, a **matriz de confusão** (onde ele erra?);
   - depois rode **Model testing** com os 20% reservados → essa é a acurácia honesta.
   Registre no relatório **treino × teste** e comente a diferença (Exemplos C.2 e C.3).

   > 🛠️ **treino ≫ teste** = overfitting. Colete mais/melhor, diminua a rede, aumente o passo.

## Parte C — Deploy na borda (aula 2, primeira metade)

8. **Exporte a biblioteca C++** (aba *Deployment* → *C++ library* → *Build*) e baixe o `.zip`.

9. **Monte o projeto ESP-IDF.** Clone o repo-base e copie as pastas do `.zip` para a raiz dele
   (segue [a doc oficial](https://docs.edgeimpulse.com/hardware/deployments/run-cpp-espressif-esp32.md)):

   ```bash
   git clone https://github.com/edgeimpulse/example-standalone-inferencing-espressif-esp32
   ```

10. **Teste com amostra fixa (sanidade).** No Studio, em *Live classification*, copie as **Raw
    features** de uma janela e cole no `features[]` do `ei_main.cpp`; então `idf.py build` e
    `idf.py flash`. A classe impressa na serial (115200) **deve bater** com a do Studio. Se bate, a
    biblioteca está integrada corretamente. **Print no relatório.**

## Parte D — Fechar o ciclo: sensor ao vivo + MQTT (aula 2, segunda metade)

11. **Ligue no sensor de verdade.** Use `src/inferencia_mqtt/main.c` como esqueleto: uma tarefa
    enche a janela com leituras reais do MPU-6050 (100 Hz), outra roda o classificador quando a
    janela fecha (semanas 5 e 6). Descomente os trechos `{{EI}}` e aponte-os para os headers da
    biblioteca que você exportou.

12. **Publique a conclusão por MQTT.** Reusando o cliente da semana 14 (trechos `{{S14}}`), publique
    `{"classe":"...","conf":...}` em `movimento/no01`. **Só a conclusão vai para a rede** — poucos
    bytes — não o fluxo bruto do acelerômetro.

13. **Veja no painel.** Assine `movimento/#` no RPi (ou no painel da trilha full-stack) e demonstre:
    faça cada movimento e mostre a classe mudando ao vivo.

## Entrega

No repositório da bancada, pasta `lab-classificador/`:

- `relatorio.md` com: justificativa da taxa (Nyquist); print do Feature explorer; **treino × teste**
  e leitura da matriz de confusão; print do teste de sanidade (Studio × dispositivo batendo);
  e o **project ID** do Edge Impulse (para reprodutibilidade).
- Um vídeo curto da demonstração ao vivo (passo 13).

## Desafio opcional

- Troque as quatro classes por **estados de vibração** de um ventilador/motor (`normal` ×
  `desbalanceado`) — mesmo pipeline, e vira direto um *monitor de vibração* para o projeto final.
- Faça o nó **atuar localmente**: `círculo` liga um LED/relé por PWM (semana 8), sem passar pela
  rede — inferência → ação na própria borda, latência de milissegundos.
