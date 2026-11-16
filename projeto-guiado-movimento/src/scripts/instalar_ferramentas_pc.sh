#!/usr/bin/env bash
# =============================================================================
# Projeto guiado — instalação das ferramentas no PC do laboratório (Ubuntu 24.04/22.04)
#
#   1) Node.js 22 (LTS) + Edge Impulse CLI (edge-impulse-data-forwarder, ...)
#      + Edge Impulse Linux CLI (edge-impulse-linux-runner)
#   2) Ambiente Python (venv ~/venv-ei) com pyserial, numpy e o SDK
#      edge_impulse_linux
#   3) Permissão na porta serial (grupo dialout) e remoção do brltty
#
# Uso:  bash instalar_ferramentas_pc.sh        (NÃO use sudo na frente: ele pede quando precisa)
# Depois: FECHE a sessão do Ubuntu e entre de novo (por causa do grupo dialout).
# =============================================================================
set -e

echo "== 1/5 Pacotes do sistema =================================================="
sudo apt update
# python3-pyaudio e python3-opencv: o SDK edge_impulse_linux importa os módulos
# de áudio e de imagem ao ser carregado — mesmo que só usemos acelerômetro.
# Instalar pelo apt evita compilar o PyAudio (que exige portaudio19-dev).
sudo apt install -y curl git build-essential python3-venv python3-pip \
    python3-numpy python3-serial python3-six python3-pyaudio python3-opencv

echo "== 2/5 Node.js 22 (NodeSource) ============================================="
if ! command -v node >/dev/null || [ "$(node -v | cut -c2- | cut -d. -f1)" -lt 20 ]; then
    curl -fsSL https://deb.nodesource.com/setup_22.x | sudo -E bash -
    sudo apt install -y nodejs
fi
node -v && npm -v

echo "== 3/5 Edge Impulse CLI (sem sudo, em ~/.npm-global) ======================="
mkdir -p ~/.npm-global
npm config set prefix "$HOME/.npm-global"
grep -q 'npm-global/bin' ~/.bashrc || echo 'export PATH=$HOME/.npm-global/bin:$PATH' >> ~/.bashrc
export PATH=$HOME/.npm-global/bin:$PATH
npm install -g edge-impulse-cli --force          # data forwarder, uploader, daemon...
npm install -g edge-impulse-linux                 # edge-impulse-linux-runner (.eim)

echo "== 4/5 Ambiente Python (venv) =============================================="
# --system-site-packages: a venv enxerga numpy/pyserial/pyaudio/opencv do apt
python3 -m venv --system-site-packages ~/venv-ei
~/venv-ei/bin/pip install --upgrade pip
~/venv-ei/bin/pip install edge_impulse_linux
~/venv-ei/bin/python -c "from edge_impulse_linux.runner import ImpulseRunner; import serial; print('SDK OK, pyserial', serial.__version__)"

echo "== 5/5 Porta serial ========================================================"
sudo usermod -a -G dialout "$USER"
# O brltty (leitor de Braille) "sequestra" conversores USB-serial CH340/CP210x.
sudo apt remove -y brltty || true

cat <<'FIM'

=============================================================================
 Instalação concluída.
 1) FAÇA LOGOUT E LOGIN (ou reinicie) para valer o grupo dialout.
 2) Em todo terminal novo do projeto:   source ~/venv-ei/bin/activate
 3) Teste:   edge-impulse-data-forwarder --version
             edge-impulse-linux-runner --version
=============================================================================
FIM
