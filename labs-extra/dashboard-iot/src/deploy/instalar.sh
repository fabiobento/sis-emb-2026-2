#!/usr/bin/env bash
# Instala o painel no Raspberry Pi OS. Execute como SEU usuário (não root), de qualquer pasta:
#   bash deploy/instalar.sh     (a partir de .../dashboard-iot/src)
set -euo pipefail
sudo apt-get update
sudo apt-get install -y python3-venv nginx mosquitto mosquitto-clients
cd "$(dirname "$0")/.."
SRC="$(pwd)"; VENV="$SRC/../venv"; USUARIO="$(id -un)"
python3 -m venv "$VENV"
"$VENV/bin/pip" install -r requirements.txt
sudo cp deploy/mosquitto-lab.conf /etc/mosquitto/conf.d/lab.conf
sudo systemctl restart mosquitto
[ -f /etc/default/dashboard ] || sed "s|@SRC@|$SRC|" deploy/dashboard.env | sudo tee /etc/default/dashboard >/dev/null
sed -e "s|@USUARIO@|$USUARIO|" -e "s|@SRC@|$SRC|" -e "s|@VENV@|$VENV|" deploy/dashboard.service | sudo tee /etc/systemd/system/dashboard.service >/dev/null
sudo cp deploy/nginx-dashboard.conf /etc/nginx/sites-available/dashboard
sudo ln -sf /etc/nginx/sites-available/dashboard /etc/nginx/sites-enabled/dashboard
sudo rm -f /etc/nginx/sites-enabled/default
sudo nginx -t
sudo systemctl daemon-reload
sudo systemctl enable --now dashboard
sudo systemctl restart nginx
echo "Pronto: abra http://$(hostname -I | awk '{print $1}')/"
