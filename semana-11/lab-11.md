# Lab 11 — RPi headless: do cartão virgem ao sistema explorado por SSH

> **Antes de começar**: leia a [teoria-11](teoria-11.md) — a Figura 11-C (boot em 5 atos) é o
> que acontecerá dentro da sua placa, e a tabela de comandos da seção 4 é a sua cola de hoje.

**Objetivo**: gravar o Raspberry Pi OS, fazer o **primeiro boot 100 % headless**, dominar a
sessão SSH e explorar o sistema (kernel, /proc, /sys, dmesg) — terminando com um relatório
gerado por script de shell.

**Duração**: 2 aulas.
**Material (por bancada)**: RPi 3 Model B, cartão microSD ≥ 16 GB, fonte micro-USB ≥ 2,5 A
(fonte fraca é a causa nº 1 de "Pi que reinicia sozinho" — o relâmpago amarelo no canto da
tela, quando há tela, denuncia), cabo de rede (ou Wi-Fi do laboratório), leitor de cartão no
PC Ubuntu.

---

## Parte 0 — Sincronize o repositório (no PC)

```bash
cd ~/sis-emb-2026-2 && git fetch && git reset --hard origin/main
```

## Parte A — Gravando o sistema (30 min)

1. No PC Ubuntu: `sudo apt install rpi-imager` (se faltar) e abra o **Raspberry Pi Imager**.

![Aplicativo Raspberry Pi Imager: sistema, cartão, gravar](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/rpi_imager.png)

*Figura L11-A — O Imager: simples por fora; o passo crucial está escondido nas opções
avançadas. Fonte: Raspberry Pi and MQTT Essentials (Packt), cap. 1, Fig. 1.20.*

2. Escolha: *Device* = Raspberry Pi 3 → *OS* = **Raspberry Pi OS Lite (64-bit)** (Lite = sem
   interface gráfica: headless de verdade, cartão e RAM agradecem) → *Storage* = seu cartão.
3. **Antes de gravar**, clique na engrenagem ⚙️ (ou `Ctrl+Shift+X`) — o passo que torna o
   headless possível:
   - hostname: `bancadaN` (N = sua bancada);
   - usuário `aluno` + a senha combinada da turma;
   - **Enable SSH** (por senha);
   - Wi-Fi do laboratório (SSID/senha) e país `BR`.
4. Grave (~5 min). Ejete, insira no RPi, energize. **Sem monitor, sem teclado** — só o LED
   verde piscando (atividade no cartão: o boot de 5 atos da teoria acontecendo diante de
   você; o primeiro boot expande o sistema de arquivos para ocupar o cartão inteiro — por
   isso é mais demorado).

## Parte B — Primeiro contato por SSH (25 min)

5. Aguarde ~60 s e, do PC:

```bash
ping -c 3 bancadaN.local
ssh aluno@bancadaN.local
```

Saída esperada no primeiro acesso:

```
The authenticity of host 'bancadaN.local' can't be established.
... Are you sure you want to continue connecting? yes
aluno@bancadaN:~ $
```

Essa pergunta sobre "authenticity" é o SSH desconfiando de um desconhecido (proteção contra
impostores — depois de aceita, a chave do Pi fica registrada; se ela mudar um dia, o SSH
grita). O que o protocolo faz por você, num desenho:

![Como o SSH funciona: autenticação e canal cifrado](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/ssh_como_funciona.png)

*Figura L11-B — Chaves, autenticação e um túnel cifrado: seus comandos atravessam o
laboratório sem que ninguém os leia. Fonte: Raspberry Pi and MQTT Essentials (Packt), cap. 1,
Fig. 1.33.*

> **Observação:** o `.local` funciona por mDNS (o Pi anuncia seu nome na rede). Se o ping
> falhar: (i) espere mais 1 min (o primeiro boot expande o sistema de arquivos); (ii)
> confira o Wi-Fi digitado na engrenagem regravando; (iii) plano B com cabo de rede:
> descubra o IP no roteador do lab e `ssh aluno@IP`.

6. Você está *dentro* do RPi. Prove — execute e **anote as saídas** no relatório:

```bash
uname -a                      # kernel: versão, arquitetura aarch64
cat /proc/cpuinfo | tail -4   # o BCM2837 se apresenta
free -h                       # ~1 GB de RAM (compare com 520 KB do ESP32!)
df -h /                       # quanto do cartão o sistema ocupa
vcgencmd measure_temp         # temperatura do SoC
```

Para cada saída, uma linha de comentário no relatório: *o que ela revela?* (Ex.: `free -h`
→ "2000× a RAM do ESP32 — e ainda assim o ESP32 liga em ms e o Pi, em 30 s").

## Parte C — "Tudo é arquivo" na prática (30 min)

7. Reproduza o **Exemplo resolvido 11.2** (LED de atividade via `/sys`):

```bash
ls /sys/class/leds/                                  # descubra o nome (led0 ou ACT)
echo none | sudo tee /sys/class/leds/led0/trigger
echo 1    | sudo tee /sys/class/leds/led0/brightness # olhe para a placa!
echo 0    | sudo tee /sys/class/leds/led0/brightness
echo mmc0 | sudo tee /sys/class/leds/led0/trigger    # devolva o gatilho original
```

Pare um segundo para apreciar: você comandou hardware físico com `echo`. Nenhuma biblioteca,
nenhum compilador — só arquivos. (E o último comando é o boa-praça: devolve o LED ao seu
trabalho original de indicar atividade do cartão.)

8. Explore mais três "arquivos que são hardware" e anote o que cada um respondeu:

```bash
cat /sys/class/thermal/thermal_zone0/temp    # temperatura em mili-°C (confira com o vcgencmd!)
cat /proc/device-tree/model                  # o device tree se apresentando
cat /proc/uptime                             # segundos desde o boot
```

9. **dmesg como monitor serial**: rode `dmesg | tail -5`, então **plugue um pendrive** na
   USB e rode `dmesg | tail -15` de novo. Identifique no texto: o kernel detectando o
   dispositivo, o driver de armazenamento assumindo, o nome atribuído (`sda`). É o análogo
   do nosso `idf.py monitor` — o kernel narrando a própria vida.

## Parte D — Preparando o terreno do Bloco 2 (25 min)

10. Atualize e instale o kit das próximas semanas:

```bash
sudo apt update && sudo apt full-upgrade -y
sudo apt install -y python3-gpiozero python3-pip i2c-tools git gpiod
```

11. Habilite o I2C **sem interface gráfica** (a semana 12 agradece):

```bash
sudo raspi-config nonint do_i2c 0     # 0 = habilita (sim, é contraintuitivo)
```

12. Copie o script de inventário do PC para o RPi e execute-o **no RPi**:

```bash
# no PC:
scp ~/sis-emb/semana-11/src/inventario_sistema.sh aluno@bancadaN.local:~/
# no RPi (via ssh):
bash inventario_sistema.sh
```

    Saída esperada: `gerado: relatorio_bancadaN.txt`. Abra o script com `less` e leia: são
    os comandos das Partes B–C empacotados em shell — seu primeiro script de administração.
13. Traga o relatório de volta ao PC (`scp aluno@bancadaN.local:~/relatorio_bancadaN.txt .`)
    e suba no GitHub da bancada.

> 💡 **Hábito que vale ouro**: `scp` PC↔RPi é o correio do Bloco 2. Edite no VS Code do PC
> (confortável), envie com `scp`, execute via `ssh`. Três janelas de terminal, um fluxo de
> trabalho profissional.

---

## 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| `bancadaN.local` não responde | primeiro boot / mDNS / Wi-Fi errado | espere 2 min; regrave conferindo SSID |
| Pi reinicia/tela com relâmpago | fonte fraca | ≥ 2,5 A, cabo curto e grosso |
| SSH recusado | SSH não habilitado na engrenagem | regrave habilitando |
| `tee` pede senha toda hora | normal — sudo por comando | paciência, ou `sudo -i` (com cuidado!) |

## Entrega (GitHub da bancada, `lab-11/relatorio.md`)

1. `relatorio_bancadaN.txt` gerado pelo script (Parte D) — anexado no repositório.
2. Saídas anotadas da Parte B.6 com uma linha de comentário em cada (o que ela revela?).
3. Confirmação do Exemplo 11.2 (foto do LED comandado por `echo`, se conseguirem!) + a
   resposta: por que `sudo echo 1 > arquivo` falha e `echo 1 | sudo tee arquivo` funciona?
4. Trecho do `dmesg` do pendrive com 3 linhas identificadas (detecção/driver/nome).
5. Duas linhas de fechamento: cite dois superpoderes que o RPi ganhou sobre o ESP32 nesta
   aula e o preço que pagou (Exemplo 11.1).

## Desafio (opcional)

Vigia térmico em shell: escreva `vigia_temp.sh` que, a cada 5 s, leia
`/sys/class/thermal/thermal_zone0/temp` e, acima de 55 °C, acenda o LED de atividade e
registre uma linha com data em `alertas.log` (dica: `while true; do ...; sleep 5; done`,
`date`, e o Exemplo 11.2). Deixe rodando enquanto compila algo pesado e mostre o log.
