#!/usr/bin/env bash
# =============================================================================
# Projeto guiado — Etapa 4: preparação do Raspberry Pi (Raspberry Pi OS Lite 64-bit)
#
#   Básico (sempre):  pacotes Python do apt + venv ~/venv-ei + SDK edge_impulse_linux
#                     + permissão na porta serial
#   Opcional:         bash instalar_rpi.sh --com-cli
#                     instala também Node.js 22 + Edge Impulse CLI/Linux CLI no RPi
#                     (demora 20–40 min num RPi 3 — rode em paralelo com outra tarefa)
#
# Uso (dentro do RPi, via ssh):  bash instalar_rpi.sh [--com-cli]
# =============================================================================
set -e

echo "== Sistema: $(uname -m), $(. /etc/os-release; echo "$PRETTY_NAME") =="
if [ "$(uname -m)" != "aarch64" ]; then
    echo "AVISO: arquitetura $(uname -m). Baixe o modelo 'Linux (ARMv7)' em vez de 'Linux (AARCH64)'."
fi

echo "== 1) Pacotes do sistema =================================================="
sudo apt update
sudo apt install -y python3-venv python3-pip python3-numpy python3-serial \
    python3-six python3-pyaudio python3-opencv

echo "== 2) venv + SDK do Edge Impulse ==========================================="
# O Raspberry Pi OS (Bookworm/Trixie) protege o Python do sistema (PEP 668):
# 'pip install' fora de uma venv dá "externally-managed-environment".
python3 -m venv --system-site-packages ~/venv-ei
~/venv-ei/bin/pip install edge_impulse_linux
~/venv-ei/bin/python -c "from edge_impulse_linux.runner import ImpulseRunner; import serial; print('SDK OK')"

echo "== 3) Porta serial ========================================================="
sudo usermod -a -G dialout "$USER"

if [ "$1" = "--com-cli" ]; then
    echo "== 4) (opcional) Node.js 22 + Edge Impulse CLI ============================"
    curl -fsSL https://deb.nodesource.com/setup_22.x | sudo -E bash -
    sudo apt install -y nodejs gcc g++ make build-essential
    mkdir -p ~/.npm-global
    npm config set prefix "$HOME/.npm-global"
    grep -q 'npm-global/bin' ~/.bashrc || echo 'export PATH=$HOME/.npm-global/bin:$PATH' >> ~/.bashrc
    export PATH=$HOME/.npm-global/bin:$PATH
    npm install -g edge-impulse-cli --force
    npm install -g edge-impulse-linux --unsafe-perm
fi

cat <<'FIM'

=============================================================================
 Pronto. Agora:  sudo reboot      (para valer o grupo dialout)
 Depois, em cada sessão ssh:  source ~/venv-ei/bin/activate
=============================================================================
FIM
