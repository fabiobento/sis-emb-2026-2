# Lista de Exercícios 03 — Semanas 7 a 10
**Temas:** ADC/DAC e introdução a PDS; PWM e atuadores; UART/SPI/I2C; barramento CAN.
**Entrega:** individual, em PDF ou markdown no GitHub, até a aula teórica da semana 11.
É a primeira lista do escopo da **P2** (semanas 7–14), com as listas 4 e 5.

## 📚 Como estudar para esta lista

1. Refaça os exemplos resolvidos **7.1–10.3** sem olhar — cada questão numérica abaixo declara
   o exemplo-irmão no seu enunciado. As figuras de apoio de cada questão aparecem junto do
   enunciado.
2. Aprofundamento opcional: **Molloy caps. 8–10** (barramentos e atuadores — o mesmo raciocínio
   vale para o ESP32); *Hacking Electronics* cap. 3 (divisores, LDR, MOSFET).

## Parte A — ADC, DAC e sinais (semana 7)
**Q1.** *(estilo Exemplo 7.1)* Para um ADC de 12 bits com fundo de escala 3,1 V: calcule o LSB,
o número de níveis e a leitura esperada para 1,20 V. Qual o erro máximo de quantização em mV?

![Função de transferência de um ADC ideal de 3 bits](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/adc_3bits.png)

![Erro de quantização confinado entre mais e menos meio LSB](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/erro_quantizacao.png)

![Comparação das funções de transferência de ADCs de 4, 8 e 12 bits](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/adc_transferencia.png)

**Q2.** *(estilo Exemplo 7.2)* Projete o divisor para um sensor resistivo que varia de 5 kΩ a
50 kΩ com resistor fixo de 10 kΩ em 3,3 V (sensor no lado de cima). Calcule V_out nos extremos
e a "sensibilidade" (ΔV). Valeria trocar o fixo por 22 kΩ? Justifique com contas.
> 💡 *A regra do máximo contraste (teoria-07, Ex. 7.2): R_fixo ≈ √(R_min · R_max) = √(5k·50k) ≈
> 15,8 kΩ. Calcule ΔV com 10k, com 22k e com 15,8k e compare — a resposta deve sair das contas.*

**Q3.** *(estilo Exemplo 7.3)* Um sinal contém componentes em 30 Hz e 700 Hz. Amostrado a
1 kHz, o que aparece no espectro digital? Que providências (analógica e digital) evitam o
problema? Cite o teorema envolvido.

![Sinal de 9 Hz amostrado a 10 Hz produzindo um alias de 1 Hz](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/aliasing_nyquist.png)

**Q4.** *(estilo Exemplo 7.4)* Aplique média móvel de M=4 à sequência 100, 104, 98, 500(spike),
102, 101 e comente o efeito no spike e o atraso introduzido. Quando a **mediana** seria melhor?

**Q5.** O DAC de 8 bits do ESP32 gera senoide por tabela de 32 pontos atualizada a 3,2 kHz.
Qual a frequência da senoide? Quantos pontos por período restariam se quiséssemos 400 Hz na
mesma taxa, e qual o efeito audível/visível disso?

## Parte B — PWM e atuadores (semana 8)
**Q6.** *(estilo Exemplo 8.1)* No LEDC com clock de 80 MHz, verifique se f=20 kHz com 12 bits é
alcançável; se não, dê a resolução máxima em 20 kHz e o duty de 25 % em contagens.

![Formas de onda de PWM com diferentes duty cycles e suas tensões médias](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/pwm_tensao_media.png)

![Oscilogramas de PWM com duty cycles de 50, 75 e 25 por cento](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/pwm_duty_cycles.png)

**Q7.** *(estilo Exemplo 8.2)* Para um servo em 50 Hz com timer de 14 bits, calcule as contagens
para 0,6 ms, 1,5 ms e 2,4 ms e a resolução angular aproximada (faixa 0–180°).

![Mapeamento entre largura de pulso e ângulo do servo](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/servo_pulsos.png)

**Q8.** Explique por que motor DC não pode ser ligado direto no GPIO (corrente, indutância,
fcem) e o papel de: transistor/ponte H, diodo de roda-livre e GND comum entre fontes.
> 💡 *Figura 8-E da teoria-08 (ponte H) como base do desenho; o checklist anti-fumaça da mesma
> aula é o esqueleto da resposta.*

<details><summary>🖼️ Confira seu desenho (Fig. 8-E da teoria-08)</summary>

![Os três estados de uma ponte H: frente, ré e o proibido shoot-through](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/ponte_h.png)

</details>

**Q9.** *(estilo Exemplo 8.3)* Motor de 6 V/300 mA acionado por L298N alimentado com 7,4 V
(queda típica 1,4 V): qual a tensão efetiva no motor? Estime a potência dissipada na ponte com
os dois enables a 100 % e discuta por que drivers MOSFET modernos (TB6612/DRV8833) aquecem menos.

**Q10.** Um aluno dimeriza LED com PWM de 30 Hz e outro filma o resultado com o celular.
Explique a cintilação percebida/filmada e escolha uma frequência adequada justificando pelo
sistema visual humano e pela taxa de captura de câmeras.
> 💡 *Vocês reproduziram isso no Lab 8, Parte A.3 — citem a observação da bancada.*

## Parte C — UART, SPI e I2C (semana 9)
**Q11.** *(estilo Exemplo 9.1)* A 9600 baud 8N1, quanto tempo leva para transmitir a string
`"T=25.4\r\n"` (8 caracteres)? E a 115 200? Por que o formato 8N1 gasta 10 bits por byte?

![Estrutura do frame UART: repouso, start bit, 8 bits de dados LSB-first, stop bit](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/uart_frame.png)
> 💡 *A Figura 9-A da teoria-09 mostra onde os 2 bits extras moram.*

**Q12.** Desenhe a ligação SPI mestre + 2 escravos (MOSI/MISO/SCLK/CS0/CS1) e explique por que
o SPI escala mal em número de fios, mas vence em velocidade. O que definem CPOL e CPHA?

<details><summary>🖼️ Confira seu desenho (Fig. 9-B da teoria-09)</summary>

![Sinais de uma transação SPI: CS, clock, MOSI e MISO](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/spi_transacao.png)

</details>

**Q13.** *(estilo Exemplo 9.3)* Explique o endereçamento de 7 bits do I2C, o papel do bit R/W e
como dois MPU-6050 convivem no mesmo barramento. O que o scanner do Lab 9 realmente faz no
protocolo para "descobrir" um endereço?

![Sequência de uma transação I2C: START, endereço de 7 bits, R/W, ACK, dados](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/i2c_transacao.png)

**Q14.** *(estilo Exemplo 9.4)* Por que o I2C exige pull-ups externos (dreno aberto)? Com
capacitância de barramento de 150 pF e alvo de t_r ≈ 0,3·R·C ≤ 300 ns (400 kHz), calcule o
pull-up máximo e comente o compromisso com o consumo.

**Q15.** Preencha a tabela comparativa UART × SPI × I2C (fios, topologia, velocidade típica,
endereçamento, caso de uso no nosso curso) e escolha o barramento para: (a) GPS; (b) display
rápido; (c) 5 sensores lentos na mesma placa.

## Parte D — CAN (semana 10)
**Q16.** Explique dominante × recessivo na camada física do CAN e por que essa assimetria
elétrica é o que torna a arbitragem possível.

![Sinalização diferencial CAN_H/CAN_L e topologia de barramento com terminação](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/can_diferencial_topologia.png)

**Q17.** *(estilo Exemplo 10.1)* IDs 0x2A5 e 0x2B1 disputam o barramento. Converta para binário
(11 bits), aponte o bit em que a arbitragem decide e diga quem vence e o que o perdedor faz.

![Campos do quadro CAN clássico: SOF, identificador, controle, dados, CRC, ACK](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/can_frame.png)

**Q18.** *(estilo Exemplo 10.2)* Rede a 250 kbit/s com 20 mensagens periódicas de 8 bytes
(~111 bits) a cada 20 ms: calcule a carga do barramento e avalie contra a regra prática de 50 %.

**Q19.** Compare CAN e I2C quanto a: alcance/robustez elétrica, arbitragem, detecção de erros e
aplicação típica. Por que I2C não sai da placa e CAN percorre o carro inteiro?

**Q20.** No ESP32, qual o papel do controlador TWAI e do transceptor SN65HVD230 (o que cada um
faz e por que os dois são necessários)? Indique os sinais entre eles e para o barramento.
> 💡 *Figura 10-C (frame) e 10-B (topologia) da teoria-10 respondem a maior parte; o teste dos
> ~60 Ω do Lab 10 fecha a parte física.*
