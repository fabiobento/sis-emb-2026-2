# Aula 11 — Linux Embarcado e o Raspberry Pi (U3) • Início do Bloco 2

> **Pré-requisito**: todo o Bloco 1 (especialmente semanas 2 e 5 — arquitetura e escalonamento).
> **Como usar**: texto autossuficiente. Os Exemplos 11.1–11.2 são o modelo das questões 1–6 da
> Lista 4. A tabela de comandos da seção 4 merece ser impressa e colada na bancada.

Bem-vindos ao Bloco 2. Nas últimas dez semanas você programou "no metal": seu código era o
único dono da máquina, cada microssegundo era seu. A partir de hoje, o cenário muda de
figura: o **Raspberry Pi 3** roda um sistema operacional completo — Linux — com dezenas de
processos, usuários, sistema de arquivos, pilha de rede e drivers prontos. Você ganha
superpoderes (Python, OpenCV, bancos de dados, servidores web) e paga um preço que precisa
entender com precisão: **perde o determinismo temporal**. Esta semana é sobre o sistema em
si: quando usá-lo, como ele é organizado, como ele liga — e como operá-lo do jeito
profissional, **sem monitor nem teclado**, só pela rede (*headless*). É também a semana em
que a linha de comando deixa de ser folclore e vira ferramenta de trabalho.

**Objetivos de aprendizagem** — ao final desta aula você deve ser capaz de:

- (a) justificar quando um produto pede Linux embarcado em vez de MCU;
- (b) explicar a separação usuário × kernel e o mantra "tudo é arquivo";
- (c) descrever o boot do RPi etapa por etapa;
- (d) operar o sistema por SSH com os ~20 comandos de sobrevivência;
- (e) explicar por que Linux comum não serve para malhas de controle rápidas.

---

## 1. Quando o MCU não basta

Volte à tabela de decisão da semana 1. O ESP32 venceu dez semanas de disciplina porque nossos
problemas eram de tempo real e periferia. Mas experimente pedir a ele:

- uma **câmera** com detecção de objetos (OpenCV pesa centenas de MB e quer gigahertz);
- um **banco de dados** com meses de histórico consultável;
- um **servidor web** de verdade, com TLS, para dezenas de clientes;
- **atualizações** de segurança de terceiros chegando prontas (`apt upgrade`);
- desenvolvimento **na própria máquina** (editar, rodar, depurar sem cross-compile).

Cada item desses é uma tarde de trabalho no Linux e um projeto de pesquisa num MCU. O SO de
propósito geral **compensa** seu custo — 1 GB de RAM, boot de ~30 s, watts em vez de
miliwatts e, crucialmente, **não determinismo temporal** (seção 5) — quando o produto precisa
dessa pilha. O espectro da semana 5 se completa: bare-metal → RTOS → **Linux embarcado**. E
o produto maduro, como veremos na semana 14, frequentemente usa **os dois**: MCU no tempo
real, Linux na inteligência.

> 💡 **Pense aí**: se o Linux é “melhor em tudo”, por que o ESP32 não morreu? *Releia o
> Exemplo 1.1: watts e boot de 30 s condenam qualquer nó a bateria; e a seção 5 mostrará o
> jitter. Potência errada é defeito — a frase da semana 1 continua valendo.*

## 2. Arquitetura: espaço de usuário × kernel

O Linux divide o mundo em dois andares:

![Separação entre espaço de usuário e espaço de kernel, com a fronteira das syscalls](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/usuario_kernel.png)

*Figura 11-A — A fronteira usuário/kernel: seus programas moram em cima; o hardware pertence
ao kernel; a única porta de entrada é a chamada de sistema. É a Figura-mapa de todo o Bloco
2 — volte a ela nas semanas 12–14.*

```
 ESPAÇO DE USUÁRIO   │  seus programas: python3, bash, mosquitto, gpiozero…
                     │  (sem acesso direto ao hardware — e é assim que deve ser)
 ────── syscalls ────┼──────────────────────────────────────────────────────────
 ESPAÇO DE KERNEL    │  escalonador • gerência de memória (MMU!) • VFS •
                     │  pilha de rede • DRIVERS (GPIO, I2C, SPI, USB, vídeo…)
 ────────────────────┼──────────────────────────────────────────────────────────
 HARDWARE            │  BCM2837: 4× Cortex-A53, DRAM, periféricos
```

Seu programa **pede** serviços ao kernel via *syscalls* (`open`, `read`, `write`, `ioctl`…)
— e o kernel decide se, quando e como atender. Compare com o ESP32: lá, `gpio_set_level()`
escrevia direto no registrador (semana 2); aqui, há um pedágio de proteção no caminho. Esse
pedágio é o que permite 50 processos coexistirem sem se corromperem: a **MMU** traduz
endereços virtuais e isola cada processo no seu mundinho — se o seu programa pisa fora do
próprio território, o kernel o mata (o famoso *segmentation fault*) em vez de deixar o
sistema inteiro cair. No ESP32, sem MMU, a mesma pisada corrompe o vizinho em silêncio.

![Tradução de endereços virtuais pela MMU entre processos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/memoria_virtual.png)

*Figura 11-B — Memória virtual: cada processo “vê” o mesmo espaço de endereços, que a MMU
mapeia para páginas físicas diferentes. Isolamento + ilusão de memória infinita — o luxo que
o Cortex-A53 tem e o LX6 não.*

E a decisão de projeto mais elegante do Unix: **"tudo é arquivo"**. Dispositivos, estado do
kernel, até o LED da placa aparecem como arquivos em árvores virtuais — `/proc` (processos e
sistema) e `/sys` (dispositivos). Ler um arquivo é consultar o hardware; escrever é
comandá-lo:

**Exemplo resolvido 11.2 (hardware via `echo`)** — Encontrar onde "vive" o LED verde de
atividade do RPi: `ls /sys/class/leds/` → `led0`. Então:

```bash
echo none | sudo tee /sys/class/leds/led0/trigger    # desliga o gatilho automático (mmc0)
echo 1    | sudo tee /sys/class/leds/led0/brightness # acende
echo 0    | sudo tee /sys/class/leds/led0/brightness # apaga
```

Você acabou de acionar hardware **sem escrever uma linha de C** — com `echo`. O poder (e a
filosofia) do "tudo é arquivo": qualquer linguagem que saiba abrir arquivos sabe falar com o
hardware. (Por que `sudo tee` e não `sudo echo >`? Porque o redirecionamento `>` é feito
pelo *seu* shell, antes do sudo ganhar poderes — pegadinha clássica: o shell tenta abrir o
arquivo como usuário comum e falha. O `tee` roda *dentro* do sudo e resolve.)

## 3. O boot do RPi: uma história em cinco atos

No ESP32, do reset ao `app_main` são milissegundos (ROM → bootloader → seu código, tudo na
flash do módulo). No RPi, ligar é uma cerimônia — e com uma peculiaridade famosa: **quem
acorda primeiro é a GPU**, não a CPU.

![Sequência de boot do Raspberry Pi em cinco etapas](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/boot_rpi.png)

*Figura 11-C — Do botão de energia ao login: a GPU acorda primeiro, lê o cartão SD, e só
depois apresenta o kernel Linux à CPU. Tudo vem do cartão — por isso cartão corrompido = Pi
morto, e por isso o Lab 11 ensina a gravar e verificar o cartão com carinho.*

```
 1. ROM da GPU        lê o cartão SD, procura o bootloader de 2ª etapa
 2. bootcode.bin      (na partição FAT do cartão) carrega o firmware da GPU
 3. start.elf         firmware da GPU: lê config.txt (overclock, overlays, I2C on/off!),
                      configura clocks e memória, e só então…
 4. kernel + DT       …carrega o kernel Linux e o DEVICE TREE — o "mapa do hardware"
                      que diz ao kernel genérico o que existe NESTA placa
 5. systemd           kernel monta o rootfs (partição ext4 do cartão) e entrega ao
                      init/systemd, que sobe serviços (ssh!, rede…) até o login
```

Dois conceitos novos que valem sublinhar:

- **`config.txt`**: o "menuconfig" do RPi, lido pela GPU antes do Linux existir. É lá (etapa
  3) que se habilita o overlay de I2C, por exemplo — resposta da questão 2 da Lista 4 e
  requisito físico do Lab 12.
- **Device tree**: o kernel Linux é o mesmo para milhares de placas; o device tree é o
  arquivo de dados que descreve o hardware *desta* placa (quais periféricos, em que
  endereços, com que interrupções). É o equivalente, em dados, do que o ESP-IDF resolvia em
  código de inicialização. Quando o mercado fala em "portar Linux para uma placa", boa parte
  do trabalho é escrever o device tree dela.

## 4. Sobrevivência na linha de comando (headless)

Em campo, o RPi não tem monitor: você o administra por **SSH** — um terminal remoto cifrado.
`ssh aluno@bancadaN.local` e você "está" na máquina. A figura abaixo resume o que o protocolo
faz por você (autenticação por chaves, canal cifrado — seus comandos não viajam em texto
claro):

![Como o SSH funciona: autenticação e canal cifrado entre cliente e servidor](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/ssh_como_funciona.png)

*Figura 11-D — O SSH em um desenho: chaves, autenticação e um túnel cifrado entre o seu
notebook e o Pi. Fonte: Raspberry Pi and MQTT Essentials (Packt), cap. 1, Fig. 1.33.*

E a anatomia de um comando — você digita frases com gramática: comando, opções e argumentos:

![Anatomia da linha de comando: comando, opções e argumentos](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/terminal_anatomia.png)

*Figura 11-E — A gramática do terminal: *comando* (o verbo), *opções* (os advérbios,
`-l`, `-a`) e *argumentos* (os objetos). Fonte: Conquer the Command Line (Raspberry Pi
Press), cap. 1, Fig. 1-1.*

Os ~20 comandos que resolvem 95 % da vida (o lab pratica todos; *Conquer the Command Line* é
o manual de bolso — enviado junto com os livros da disciplina):

| Preciso de… | Comando |
|---|---|
| onde estou / o que há aqui | `pwd` • `ls -la` |
| mover-me / mexer em arquivos | `cd` • `cp` • `mv` • `rm` • `mkdir` |
| ver conteúdo | `cat` • `less` • `head` • `tail -f` (segue um log ao vivo!) |
| procurar | `grep -r "texto" .` • `find / -name "*.conf"` |
| instalar software | `sudo apt update && sudo apt install pacote` |
| processos | `ps aux` • `top` • `kill PID` |
| hardware/saúde | `cat /proc/cpuinfo` • `free -h` • `df -h` • `vcgencmd measure_temp` • `gpioinfo` |
| copiar PC↔RPi | `scp arquivo aluno@bancadaN.local:~/` |
| permissões/superusuário | `chmod` • `sudo` |
| mensagens do kernel | `dmesg | tail` (o "monitor serial" do Linux) |

E antes do primeiro boot: a gravação do cartão. O **Raspberry Pi Imager** é o gravador
oficial — escolhe o sistema, grava, verifica:

![Aplicativo Raspberry Pi Imager para gravação do cartão SD](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/rpi_imager.png)

*Figura 11-F — O Raspberry Pi Imager: três botões (sistema, cartão, gravar) e opções
escondidas de pré-configuração — hostname, usuário, Wi-Fi e **SSH habilitado** — que fazem o
boot já nascer *headless*. Fonte: Raspberry Pi and MQTT Essentials (Packt), cap. 1,
Fig. 1.20.*

> **Observação — `sudo` com respeito.** `sudo` executa como root, o usuário que pode tudo —
> inclusive `rm -rf /`. A regra da disciplina: use `sudo` quando o comando falhar por
> permissão e você **entender por quê** — nunca por reflexo. E o atalho mais produtivo do
> terminal é a tecla **Tab** (completa nomes de arquivos e comandos): quem digita tudo na
> mão trabalha o triplo e erra o dobro.

## 5. O elefante na sala: Linux é tempo real?

Não — e é fundamental saber *quantificar* o "não":

**Exemplo resolvido 11.1 (tempo real?)** — Um `while(1)` em Python togglando GPIO alcança
períodos médios de ~100 µs… mas com **jitter ocasional de milissegundos**. De onde vêm os
buracos? O escalonador do Linux preempta seu processo para rodar outros (há dezenas deles
acordando o tempo todo); interrupções de rede e disco passam na frente; o interpretador
Python ainda soma pausas próprias (o coletor de lixo pode parar o mundo por alguns ms).
Nada disso é bug — é o preço do propósito geral: o sistema foi otimizado para *vazão total*
e *justiça entre processos*, não para o pior caso de um processo só.

Conclusões de projeto: (i) malhas de controle rápidas (a PID de 20 ms da semana 13) ficam
**no MCU**; (ii) o Linux assume supervisão, registro, rede e interface — a **arquitetura
híbrida ESP32 + RPi** da semana 14; (iii) quando o Linux *precisa* de latência, existe o
kernel **PREEMPT_RT** (patch que torna quase todo o kernel preemptável e leva o pior caso a
dezenas de µs — melhor, não milagroso; e com custo de vazão). Compare com a pergunta da
Lista 4 sobre onde cada pedaço de um produto real deve morar.

> 💡 **Regra de bolso para o resto do curso**: prazo em **µs–ms** ⇒ MCU/RTOS; prazo em
> **centenas de ms–s** ⇒ Linux pode. Entre os dois, análise cuidadosa — e quase sempre a
> resposta honesta é "divide: MCU mede e atua, Linux pensa e conversa" (a semana 14 inteira
> é essa frase implementada).

## Resumindo

- Linux embarcado quando o produto pede *pilha* (câmera, banco, web, updates); MCU quando
  pede *prazo e µW*; produtos maduros combinam os dois.
- Usuário × kernel via syscalls; MMU isola processos (segfault mata um, não o sistema);
  **"tudo é arquivo"**: `/proc` e `/sys` expõem sistema e hardware a qualquer linguagem
  (Exemplo 11.2 — LED via `echo`).
- Boot em 5 atos, começando pela **GPU**; `config.txt` configura antes do Linux; **device
  tree** descreve o hardware ao kernel genérico.
- Headless: Imager (com SSH pré-habilitado) + SSH + os ~20 comandos de sobrevivência; `sudo`
  com entendimento, Tab sempre.
- Linux comum tem jitter de ms (Exemplo 11.1) ⇒ controle rápido no MCU, inteligência no
  Linux; PREEMPT_RT ameniza quando preciso.

### 📌 Vocabulário da semana

| Termo | Significado |
|---|---|
| syscall | chamada de sistema: o pedido do usuário ao kernel |
| MMU | unidade de gerenciamento de memória (isola processos) |
| memória virtual | cada processo com seu espaço de endereços ilusório |
| "tudo é arquivo" | dispositivos e estado expostos como arquivos (/proc, /sys) |
| device tree | descrição do hardware da placa para o kernel |
| systemd | o init: primeiro processo, sobe os serviços |
| headless | operar sem monitor/teclado, pela rede |
| SSH | terminal remoto cifrado |
| jitter | variação temporal (o inimigo do controle) |
| PREEMPT_RT | patch de tempo real para o kernel Linux |

## 📖 Onde aprofundar (opcional)

- ***Conquer the Command Line*** (Smedley, 3. ed.) — guia curto e excelente; caps. 1–5
  cobrem tudo que o lab exige (está entre os PDFs da disciplina).
- ***Raspberry Pi and MQTT Essentials*** (Packt), cap. 1: preparação do cartão e SSH com
  telas (idém, entre os PDFs).
- **Smart**, *Practical Python Programming for IoT*, cap. 1: preparação do ambiente Python
  com `venv` no RPi — adotaremos esse padrão nos labs 12–14 (entre os PDFs).
- **Molloy**, *Exploring Raspberry Pi*, caps. 2–3 — a referência clássica de Linux
  embarcado no RPi.
- **Upton & Duntemann**, cap. 8 *Operating Systems* — o que é um SO e por que o RPi precisa
  de um.

## Exercícios

Lista 4, questões 1–6 (estilo dos Exemplos 11.1–11.2).
