# Lista de Exercícios 01 — Semanas 1 a 3
**Temas:** introdução a sistemas embarcados; arquitetura de microcontroladores; C embarcado e GPIO.
**Entrega:** individual, em PDF ou markdown no GitHub, até a aula teórica da semana 4.

## 📚 Como estudar para esta lista

1. **Primeiro, refaça os exemplos resolvidos** das teorias 1–3 com papel e lápis (sem olhar a
   solução): 1.1, 1.2, 1.3 (semana 1); 2.1–2.4 (semana 2); 3.1–3.3 (semana 3). As questões
   numéricas abaixo são **variações diretas** deles — quem domina os exemplos, domina a lista.
2. Confira **sempre as unidades** no final de cada conta (mAh ÷ mA = h; ciclos ÷ Hz = s).
   Questão com unidade errada perde metade da nota.
3. As figuras de apoio de cada questão aparecem **junto do enunciado**, logo abaixo dele.

## Parte A — Introdução (semana 1)
**Q1.** Defina sistema embarcado e cite as cinco restrições de projeto discutidas em aula
(tempo real, energia, custo, memória, confiabilidade), dando um exemplo de produto em que cada
uma é a restrição **dominante**.

**Q2.** Classifique como MCU, MPU/SoC ou FPGA a melhor tecnologia para: (a) controle de airbag;
(b) roteador Wi-Fi doméstico; (c) protótipo de codec de vídeo proprietário; (d) sensor de umidade
a bateria por 2 anos. Justifique com as restrições da Q1.
> 💡 *Método: para cada item, pergunte primeiro "qual é a restrição dominante?" — a resposta da
> classificação sai sozinha. Compare com a tabela de decisão da teoria-01, seção 2.2.*

**Q3.** *(estilo Exemplo 1.1)* Um rastreador GPS consome 120 mA por 3 s a cada transmissão
(1 transmissão/10 min) e 40 µA dormindo. Calcule a corrente média e a autonomia com bateria de
1200 mAh. O que domina o consumo? Que mudança de **software** dobraria a autonomia?

**Q4.** Explique a diferença entre tempo real **rígido** (*hard*) e **brando** (*soft*), com um
exemplo de cada num automóvel. Em qual categoria está o nosso projeto de malha de luminosidade
(semana 13)?

**Q5.** No fluxo de projeto visto em aula (requisitos → particionamento HW/SW → protótipo →
validação), explique o papel do **simulador** (Wokwi) e cite duas limitações dele em relação ao
hardware real.
> 💡 *Pense no que o Wokwi não simula: tolerâncias de componentes reais, ruído elétrico,
> bouncing mecânico (Lab 3!), quedas de tensão em fontes fracas…*

## Parte B — Arquitetura (semana 2)
**Q6.** Desenhe (à mão) os diagramas Von Neumann e Harvard e explique por que MCUs adotam
majoritariamente Harvard (ou Harvard modificada). Onde o ESP32 se encaixa?
> 💡 *Base: Figura 2-A da teoria-02. Seu desenho deve mostrar os barramentos e explicar o
> gargalo de Von Neumann — e a "ponte" da Harvard modificada (leitura de constantes da flash).*

<details><summary>🖼️ Confira seu desenho (Fig. 2-A da teoria-02)</summary>

![Arquiteturas Von Neumann (barramento único) e Harvard (barramentos separados)](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/von_neumann_harvard.png)

</details>

**Q7.** *(estilo Exemplo 2.1)* Um laço de controle precisa executar 12 000 instruções por
iteração a 1 kHz. Que fração da CPU de um núcleo de 240 MHz (1 instr/ciclo) isso ocupa? E num
AVR de 16 MHz? O que isso diz sobre a escolha Arduino × ESP32 para PDS?

**Q8.** Diferencie Flash, SRAM e memória RTC do ESP32 quanto a: volatilidade, velocidade, uso
típico (código, dados, deep sleep). Por que variáveis globais "vivem" na SRAM mas seu valor
inicial vem da Flash?
> 💡 *A segunda parte é a pergunta da cópia `.data` no boot — teoria-02, seção 4.1. E o "Pense
> aí" da mesma seção responde quanto cada tipo de global custa em cada memória.*

![Pirâmide da hierarquia de memória com exemplos do ESP32](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/hierarquia_memoria.png)

**Q9.** *(estilo Exemplo 2.3)* O registrador `GPIO_OUT_W1TS_REG` (0x3FF44008) liga os bits
escritos em 1 sem tocar nos demais. Escreva a linha C (ponteiro + `volatile`) que acende o
GPIO2 usando esse registrador e explique por que `W1TS/W1TC` evitam a sequência lê-modifica-escreve.

![Mapa de memória do ESP32 mostrando a faixa de periféricos e o endereço do GPIO_OUT_REG](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/mapa_memoria_esp32.png)

**Q10.** *(estilo Exemplo 2.4)* Dimensione a memória para gravar 5 s de acelerômetro de 3 eixos,
16 bits/eixo, a 1 kHz. Cabe na SRAM do ESP32 (~520 KB)? E se fossem 60 s? Proponha solução para
o caso que não cabe (dica: *streaming*/double buffering — teoria-02, Exemplo 2.4).

## Parte C — C embarcado e GPIO (semana 3)
**Q11.** Explique, com um cenário concreto de ISR (semana 4), o que o qualificador `volatile`
impede o compilador de fazer e o bug silencioso que ocorre sem ele.
> 💡 *O cenário do `while(!pronto){}` da teoria-03, seção 1.3, é o modelo. E lembre: volatile ≠
> mutex — complete dizendo o que cada um garante.*

**Q12.** *(estilo Exemplo 3.1)* Num registrador de 8 bits, sem alterar os demais bits:
zere os bits 3–2 e depois escreva neles o valor `01`; inverta o bit 7. Dê as expressões com
máscaras (`&`, `|`, `^`, `<<`).

![As três operações fundamentais sobre bits de um registrador: SET, CLEAR e TOGGLE](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/bitwise_ops.png)

**Q13.** *(estilo Exemplo 3.2)* Calcule o resistor série para um LED azul (V_F = 3,0 V) em GPIO
de 3,3 V com 4 mA, e comente o resultado prático (margem pequena!). Repita para o mesmo LED em
5 V (RPi? cuidado!) — por que **nunca** ligamos cargas de 5 V direto no GPIO de 3,3 V?
> 💡 *A "margem pequena": com V_R de só 0,3 V, pequenas variações de V_F mudam muito a corrente —
> calcule a corrente se V_F for 3,2 V e comente.*

**Q14.** Diferencie pull-up e pull-down, desenhe o circuito do botão com pull-up interno e
indique o nível lógico lido com o botão solto e pressionado.
> 💡 *Figura 3-C da teoria-03 como modelo; não esqueça de justificar por que o botão "vence" o
> resistor.*

<details><summary>🖼️ Confira seu desenho (Fig. 3-C da teoria-03)</summary>

![Circuitos com resistor de pull-up, pull-down e entrada flutuante](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/pullup_pulldown.png)

</details>

**Q15.** *(estilo Exemplo 3.3)* Um botão gera bordas espúrias por até 6 ms. Proponha e
justifique um debounce por software (janela de confirmação), indicando o valor escolhido e o
compromisso latência × robustez.

## Bônus (opcional, +0,5 pt)
**Q16.** Explique com suas palavras, apoiado nas Figuras 2-B (pipeline) e 2-C (hierarquia) da
teoria-02, como **pipeline** e **cache** fazem o Cortex-A53 do RPi render muito mais que o clock
sugere — e por que, ainda assim, um MCU simples pode ser **mais previsível** no tempo de resposta
(dica: miss de cache e ISR — e o Exemplo resolvido 2.2).

![Diagrama temporal de um pipeline de 5 estágios com 5 instruções](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/pipeline.png)
