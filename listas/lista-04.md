# Lista de Exercícios 04 — Semanas 11 e 12
**Temas:** Linux embarcado; Raspberry Pi; administração por linha de comando; GPIO, sensores e
barramentos no RPi.
**Entrega:** individual, em PDF ou markdown no GitHub, até a aula teórica da semana 13.

## 📚 Como estudar para esta lista

1. Refaça os exemplos resolvidos **11.1–12.3** sem olhar. Em especial: o Exemplo 11.1 (jitter),
   o 11.2 (LED via `/sys`) e o 12.2 (divisor do ECHO) — são cobrados quase literalmente. As
   figuras de apoio de cada questão aparecem junto do enunciado.
2. Aprofundamento opcional: **Molloy caps. 2–3 e 6**; *Conquer the Command Line* (está entre os
   PDFs da disciplina — capítulos 1–5 cobrem tudo que os labs pedem).

## Parte A — Linux embarcado (semana 11)
**Q1.** Explique a separação entre espaço de usuário e kernel: por que programas não tocam o
hardware diretamente, o que é uma *syscall* e o papel do kernel como dono dos drivers.
> 💡 *Figura 11-A da teoria-11: desenhe os dois andares, a fronteira e um exemplo de syscall
> cruzando-a (ex.: o `write` de um `led.on()`).*

<details><summary>🖼️ Confira seu desenho (Fig. 11-A da teoria-11)</summary>

![Separação entre espaço de usuário e espaço de kernel, com a fronteira das syscalls](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/usuario_kernel.png)

</details>

**Q2.** Diferencie **processo** (Linux) de **tarefa** (FreeRTOS) em isolamento de memória (MMU!),
segurança e custo de criação. Por que um `segfault` mata só o processo no Linux, mas no FreeRTOS
pode derrubar o sistema inteiro?

![Tradução de endereços virtuais pela MMU entre processos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/memoria_virtual.png)

**Q3.** Descreva as etapas do boot do RPi (ROM da GPU → bootloader no SD → firmware/kernel →
systemd → serviços) e onde o arquivo `config.txt` atua (ex.: habilitar o I2C do Lab 12).

![Sequência de boot do Raspberry Pi em cinco etapas](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/boot_rpi.png)

**Q4.** *(estilo Exemplo 11.1)* Por que o Linux não é adequado a uma malha de controle de 1 ms?
Explique com o jitter do escalonador e o que é o patch **PREEMPT_RT**. Quando ele ajuda e quando
a resposta certa é "deixe a malha no MCU"?

**Q5.** Escreva os comandos para: (a) descobrir o IP do RPi na rede; (b) conectar via SSH como
`aluno` num host `bancada3.local`; (c) copiar `log.csv` do PC para o home do RPi; (d) descobrir
a temperatura do SoC; (e) atualizar a lista de pacotes e instalar o `mosquitto`.
> 💡 *Todas as cinco foram feitas nas Partes B–D do Lab 11 e A do Lab 14 — consulte os roteiros.*

**Q6.** O que significa "tudo é arquivo" no Unix/Linux? Dê três exemplos (`/proc`, `/sys`, device
files) de como isso unifica o acesso a sistema e hardware — um deles sendo o LED da placa
(Exemplo 11.2).

## Parte B — Interfaceamento no RPi (semana 12)
**Q7.** Descreva o header de 40 pinos: numeração física × BCM, limites de tensão/corrente e os
cuidados ao usar pinos com função alternativa (I2C BCM 2/3 com pull-ups embutidos; UART
BCM 14/15).

![Pôster de referência do cabeçalho GPIO do Raspberry Pi](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/rpi_poster_gpio.png)

![Esquemas de numeração dos pinos GPIO: físico (BOARD) versus BCM](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/gpio_numeracao.png)

**Q8.** O RPi não tem ADC nativo. Apresente três estratégias para ler um sensor analógico
(sensor digital; ADC externo MCP3008/ADS1115; delegar a um MCU) e quando cada uma faz sentido.
> 💡 *A Figura 12-E da teoria-12 (LDR + ADS1115) ilustra a estratégia 2 — desenhe-a como parte
> da resposta.*

**Q9.** *(estilo Exemplo 12.1)* Compare acesso a GPIO via **sysfs × libgpiod × gpiozero**:
maturidade, velocidade típica de toggling no RPi 3, caso de uso (protótipo × produção).

![Pilha de software entre o código Python e o pino físico no Linux](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/stack_gpio_linux.png)

**Q10.** Por que o DHT11 falha ocasionalmente de leitura no Linux? Relacione com os tempos do
protocolo (bits de dezenas de µs) e com o Exemplo 11.1 (jitter). Que padrão de tratamento
adotamos no `dht11_log.py`?
> 💡 *Vocês mediram a taxa de falha no Lab 12, Parte B.3 — citar o número medido na bancada
> enriquece a resposta.*

![Circuito do DHT11 ligado ao Raspberry Pi com resistor de pull-up no pino de dados](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/dht_circuito.png)

**Q11.** *(estilo Exemplo 12.2)* Desenhe o divisor de tensão do ECHO do HC-SR04 para o GPIO de
3,3 V com 1 kΩ/2 kΩ: mostre o cálculo da tensão resultante e explique o risco de ligar o ECHO
direto no RPi.
> 💡 *Figura 12-H da teoria-12 (o circuito fotografado) como referência de montagem; a conta é
> a do divisor da semana 7 em novo papel.*

<details><summary>🖼️ Confira sua montagem (Fig. 12-H da teoria-12)</summary>

![Montagem do HC-SR04 em protoboard com divisor de tensão no ECHO](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/hcsr04_circuito.png)

</details>

![Diagrama temporal do funcionamento do HC-SR04: disparo, emissão e eco](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/ultrassom_funcionamento.png)

**Q12.** Escreva os comandos para: habilitar o I2C no RPi sem interface gráfica; varrer o
barramento; e ler o registrador `WHO_AM_I (0x75)` do MPU-6050 em `0x68`. O que cada saída
esperada confirma?

## Bônus (opcional, +0,5 pt)
**Q13.** Projete em alto nível um produto real (estação meteorológica com painel web):
identifique qual parte mora no ESP32 (tempo real, sensores com timing crítico, deep sleep) e
qual no RPi (agregação, banco, interface), justificando com os Exemplos 11.1 e 14.3. Desenhe o
diagrama de blocos da arquitetura — será cobrado no seu projeto final!
