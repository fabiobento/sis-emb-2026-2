# Preparação do ambiente de laboratório (Ubuntu 24.04)

## 1. ESP-IDF (Bloco 1 — ESP32)

```bash
sudo apt update
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

mkdir -p ~/esp && cd ~/esp
git clone -b v5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32

# ativa o ambiente do IDF automaticamente em todo terminal novo
echo '. $HOME/esp/esp-idf/export.sh > /dev/null 2>&1' >> ~/.bashrc

# permissão da porta serial (relogar depois):
sudo usermod -aG dialout $USER
```

Teste: Em um novo terminal digite
```bash
idf.py --version
```

## 2. VS Code + extensões

1. Instale o VS Code (`sudo snap install code --classic`).
2. Extensões: **Espressif IDF**, **C/C++**, **Python**, **Wokwi Simulator** (há uma licença paga Wokwi
   para VS Code; mas todos podem usar o acesso em <https://wokwi.com> no navegador).

## 3. Wokwi (todos os alunos)

- Criar conta em <https://wokwi.com>; template "ESP32 (ESP-IDF)".
- Todos os roteiros do Bloco 1 têm uma versão simulável — monte primeiro no Wokwi, depois no hardware.

## 4. Raspberry Pi (Bloco 2)

- Gravar **Raspberry Pi OS Lite (64-bit)** com o *Raspberry Pi Imager* (`sudo snap install rpi-imager`),
  habilitando SSH e usuário/senha no menu ⚙️ (OS customization).
- Acesso: `ssh aluno@raspberrypi.local` a partir do PC do laboratório.

📖 Passo a passo com telas: *Raspberry Pi and MQTT Essentials* (Packt), cap. 1, seções
"Setting up a Raspberry Pi" (Figuras 1.19–1.32, p. 24–31) e "SSH" (Figura 1.33, p. 29).
Alternativa: *Internet of Things from Scratch* (Packt), cap. 2, Figuras 2.15–2.17 (p. 61–63).

## 5. Sincronizando seu diretório de trabalho (`~/sis-emb/`) com o GitHub da bancada

> Isto é o passo a passo prático do que o Lab 1 (Parte C) pede: cada bancada cria um
> repositório próprio (`sis-emb-2026-bancada-XX`) para guardar relatórios e códigos. Essa
> seção explica **como ligar** a pasta local onde vocês já vêm trabalhando (ex.: `~/sis-emb/`)
> a esse repositório no GitHub.

> ⚠️ **Não confunda as duas pastas**: `~/sis-emb-2026-2` é o **clone oficial da disciplina**
> (só leitura na prática — vocês sincronizam com `git fetch` + `git reset --hard`, que
> **sobrescreve** qualquer coisa local). `~/sis-emb/` (ou o nome que sua bancada escolheu) é a
> pasta **de vocês**, com os relatórios e códigos que vocês produzem — é essa que vai para o
> repositório da bancada. Nunca rode `git reset --hard` dentro dela.

### 5.1 Configuração única (faça uma vez por computador)

```bash
git config --global user.name "Seu Nome"
git config --global user.email "seu-email@exemplo.com"
```

O GitHub não aceita mais login por senha em `git push` — você precisa de um **token** ou de
uma **chave SSH**. O mais simples para a maioria é instalar o **GitHub CLI** e autenticar
uma vez:

```bash
sudo apt install -y gh
gh auth login          # escolha GitHub.com → HTTPS → Login with a web browser
```

Siga as instruções na tela (vai abrir o navegador para você confirmar o login). Depois disso,
`git push`/`git pull` funcionam sem pedir senha de novo neste computador.

### 5.2 Cenário A — sua bancada **ainda não criou** o repositório no GitHub

1. No GitHub, crie o repositório **vazio** (sem README, sem `.gitignore`, sem licença — é
   importante deixar em branco, para não conflitar com o que já existe na sua pasta local):
   `sis-emb-2026-bancada-XX` → **Create repository**.
2. No terminal, dentro da sua pasta de trabalho:

```bash
cd ~/sis-emb
git init
git add .
git commit -m "primeiro commit: trabalhos até aqui"
git branch -M main
git remote add origin https://github.com/SEU-USUARIO/sis-emb-2026-bancada-XX.git
git push -u origin main
```

Pronto — dali em diante, a cada aula, basta `git add .`, `git commit -m "..."` e `git push`.

### 5.3 Cenário B — sua bancada **já criou** o repositório (com ou sem README inicial)

A diferença para o Cenário A é que o GitHub já tem pelo menos um commit (o README que ele
mesmo sugere criar), enquanto sua pasta local também já tem arquivos — são **dois históricos
que nunca se falaram**. Verifique primeiro se sua pasta já é um repositório Git:

```bash
cd ~/sis-emb
git status
```

- Se aparecer `fatal: not a git repository` → rode `git init` antes de continuar.
- Se já mostrar o status normalmente → pode pular o `git init`.

Agora conecte ao repositório remoto e junte os dois históricos:

```bash
git add .
git commit -m "primeiro commit: trabalhos até aqui"
git remote add origin https://github.com/SEU-USUARIO/sis-emb-2026-bancada-XX.git
git branch -M main
git pull origin main --allow-unrelated-histories
```

O `--allow-unrelated-histories` é necessário porque o Git, por padrão, recusa juntar dois
projetos que "nasceram" separados (o seu, local, e o README do GitHub) — é uma proteção
contra misturar coisas por engano, não um erro seu.

- Se o `pull` abrir um editor de texto pedindo uma mensagem de merge, apenas salve e feche
  (`Ctrl+O`, `Enter`, `Ctrl+X` no nano; `:wq` no vim).
- Se aparecer conflito só no `README.md` (comum, já que os dois lados criaram um), abra o
  arquivo, apague os marcadores `<<<<<<<`, `=======`, `>>>>>>>` deixando o conteúdo que
  fizer sentido, depois `git add README.md` e `git commit`.

Por fim:

```bash
git push -u origin main
```

### 🛠️ Problemas comuns

| Sintoma | Causa provável | Remédio |
|---|---|---|
| `fatal: remote origin already exists` | você já rodou `git remote add origin` antes | use `git remote set-url origin <url>` em vez de `add` |
| `fatal: refusing to merge unrelated histories` | esqueceu a flag no `pull` | repita com `--allow-unrelated-histories` |
| `Support for password authentication was removed` | tentou logar com senha da conta | configure `gh auth login` (seção 5.1) ou uma chave SSH |
| `! [rejected] ... (fetch first)` ao dar `push` | o remoto tem commits que você não tem localmente | rode `git pull origin main` (com `--allow-unrelated-histories` se for a primeira vez) antes do `push` |
| `nothing to commit` mas os arquivos aparecem no GitHub errado | rodou os comandos dentro de `~/sis-emb-2026-2` por engano | `cd ~/sis-emb` (sua pasta, não o clone oficial) e repita |

## Acervo complementar da turma

- Molloy (*Exploring Raspberry Pi*);
- Upton & Halfacree (*Raspberry Pi User Guide*)
- Upton & Duntemann (*Learning Computer Architecture with Raspberry Pi*)
- Monk (*Hacking Electronics*)
- Smedley (*Conquer the Command Line*, 3. ed.)
- King (*Simple Electronics with GPIO Zero*, 2. ed.)
- *The Official Raspberry Pi Projects Book*
- Smart (*Practical Python Programming for IoT*)
- Dalmaris (*Raspberry Pi Full Stack*)
- Packt:
   - *Raspberry Pi and MQTT Essentials*
   - *Internet of Things Programming Projects* (2. ed.
   - *Internet of Things from Scratch*
   - *Building Smart Home Automation Solutions with Home Assistant*.
