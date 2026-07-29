#!/usr/bin/env python3
# Semana 12 — gpiozero: LED alterna a cada aperto (debounce ja incluso no gpiozero)
from gpiozero import LED, Button
from signal import pause

led = LED(17)            # BCM 17 (pino fisico 11)
btn = Button(27, bounce_time=0.02)   # BCM 27, debounce 20 ms

btn.when_pressed = led.toggle
print("Pressione o botao (Ctrl+C para sair)")
pause()
