#!/usr/bin/env bash
# Semana 11 — inventario do sistema (rode no RPi):  bash inventario_sistema.sh
out=relatorio_$(hostname).txt
{
  echo "== $(date) =="
  echo "--- kernel ---";      uname -a
  echo "--- cpu ---";         grep -E "Model|model name" /proc/cpuinfo | sort -u
  echo "--- memoria ---";     free -h
  echo "--- disco ---";       df -h /
  echo "--- temperatura ---"; vcgencmd measure_temp
  echo "--- gpio ---";        gpioinfo 2>/dev/null | head -20 || echo "instale gpiod"
  echo "--- rede ---";        ip -brief addr
  echo "--- servicos ssh ---";systemctl is-active ssh
} > "$out"
echo "gerado: $out"
