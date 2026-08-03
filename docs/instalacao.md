# Preparação do ambiente de laboratório (Ubuntu 22.04)

## 1. ESP-IDF (Bloco 1 — ESP32)

```bash
sudo apt update
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
mkdir -p ~/esp && cd ~/esp
git clone -b v5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32
echo "alias get_idf='. ~/esp/esp-idf/export.sh'" >> ~/.bashrc
# permissão da porta serial (relogar depois):
sudo usermod -aG dialout $USER
```

Teste: `get_idf && idf.py --version`.

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
