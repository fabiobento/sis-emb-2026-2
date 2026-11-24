#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
(Opcional) Etapa 2, B.6 — A protoboard em 3D, inclinada conforme o acelerômetro.

Uso:  python3 cubo3d.py --porta /dev/ttyUSB0      (pip install pygame, dentro da venv)

O desenho é uma placa (a protoboard) com os eixos X (vermelho), Y (verde) e Z (azul) do
MPU-6050. A placa deitada na bancada aparece deitada, com o Z para CIMA, como no módulo
GY-521.

O que o acelerômetro consegue e o que NÃO consegue mostrar
-----------------------------------------------------------
Parado, o acelerômetro mede só a gravidade, um vetor "para cima" de 9,8 m/s² visto no
referencial do sensor. Desse vetor saem dois ângulos:

    roll  (rolagem, em torno de X) = atan2(ay, az)
    pitch (arfagem, em torno de Y) = atan2(-ax, sqrt(ay² + az²))

Girar a placa em torno do eixo VERTICAL (yaw, guinada) não muda o vetor gravidade no
referencial do sensor. Por isso o yaw NÃO aparece aqui: é um limite físico do acelerômetro,
e não um defeito do programa. Para medir o yaw é preciso o giroscópio (integrando a
velocidade angular, com deriva) ou um magnetômetro (bússola); o MPU-6050 tem o giroscópio,
mas o firmware deste projeto envia só accX, accY e accZ.

Durante um movimento, a aceleração do movimento se soma à gravidade e os ângulos oscilam:
a placa no desenho "treme" quando você sacode a protoboard. É esperado.
"""
import argparse
import math
import threading
import time

import serial

LARGURA, ALTURA = 900, 650
CENTRO_X, CENTRO_Y = LARGURA // 2, ALTURA // 2 + 40
DIST_CAMERA = 900.0          # distância da câmera (perspectiva suave)
AZIMUTE = math.radians(-35)  # câmera girada em torno do Z do mundo...
ELEVACAO = math.radians(25)  # ...e olhando de cima para baixo

# Placa com proporções de uma protoboard (meias-dimensões, em pixels do desenho)
PX, PY, PZ = 160, 100, 18
VERTICES = [(sx * PX, sy * PY, sz * PZ)
            for sz in (-1, 1) for sy in (-1, 1) for sx in (-1, 1)]
# faces: (índices dos vértices, cor). A face de cima (+Z) é destacada.
FACES = [
    ((4, 5, 7, 6), (70, 110, 170)),   # +Z (topo, onde estão os componentes)
    ((0, 1, 3, 2), (45, 45, 55)),     # -Z
    ((1, 3, 7, 5), (80, 80, 95)),     # +X
    ((0, 2, 6, 4), (80, 80, 95)),     # -X
    ((2, 3, 7, 6), (95, 95, 110)),    # +Y
    ((0, 1, 5, 4), (95, 95, 110)),    # -Y
]
EIXO = 230
EIXOS = [((EIXO, 0, 0), (230, 60, 60), "X"),
         ((0, EIXO, 0), (60, 200, 60), "Y"),
         ((0, 0, EIXO), (80, 140, 255), "Z")]

acc = [0.0, 0.0, 9.8]        # última amostra (m/s²), atualizada pela thread serial
rodando = True


# ----------------------------------------------------------------- matemática
def angulos_da_gravidade(ax, ay, az):
    """Roll e pitch (rad) a partir da gravidade medida pelo acelerômetro."""
    roll = math.atan2(ay, az)
    pitch = math.atan2(-ax, math.sqrt(ay * ay + az * az))
    return roll, pitch


def matriz_sensor_para_mundo(roll, pitch):
    """R = Ry(pitch) · Rx(roll): leva um vetor do referencial do sensor ao do mundo
    (Z do mundo para cima). O yaw é fixado em 0, pois o acelerômetro não o mede."""
    cr, sr = math.cos(roll), math.sin(roll)
    cp, sp = math.cos(pitch), math.sin(pitch)
    return [[cp, sp * sr, sp * cr],
            [0.0, cr, -sr],
            [-sp, cp * sr, cp * cr]]


def aplica(R, v):
    return tuple(R[i][0] * v[0] + R[i][1] * v[1] + R[i][2] * v[2] for i in range(3))


def para_tela(p):
    """Mundo (Z para cima) → tela. Devolve (x, y, profundidade)."""
    x, y, z = p
    # gira a cena em torno do Z do mundo (azimute da câmera)
    xa = x * math.cos(AZIMUTE) - y * math.sin(AZIMUTE)
    ya = x * math.sin(AZIMUTE) + y * math.cos(AZIMUTE)
    # inclina a câmera para olhar de cima (elevação): o que é mais alto fica mais perto
    prof = ya * math.cos(ELEVACAO) - z * math.sin(ELEVACAO)
    cima = ya * math.sin(ELEVACAO) + z * math.cos(ELEVACAO)
    f = DIST_CAMERA / (DIST_CAMERA + prof)
    return CENTRO_X + xa * f, CENTRO_Y - cima * f, prof   # tela: y cresce para baixo


# ----------------------------------------------------------------- serial
def le_serial(ser):
    """Lê linhas 'accX,accY,accZ' em segundo plano. Linhas 'ERR: ...' são ignoradas."""
    global acc, rodando
    while rodando:
        try:
            partes = ser.readline().decode("utf-8", errors="ignore").strip().split(",")
            if len(partes) == 3:
                acc = [float(p) for p in partes]
        except ValueError:
            pass
        except serial.SerialException as e:
            print(f"Porta serial perdida: {e}")
            break


# ----------------------------------------------------------------- desenho
def desenha(tela, fonte, fonte_p, roll, pitch, a):
    import pygame
    tela.fill((18, 18, 24))
    R = matriz_sensor_para_mundo(roll, pitch)

    # chão de referência (fixo no mundo) e a vertical do mundo
    for k in range(-4, 5):
        for (p, q) in (((k * 60, -240, -140), (k * 60, 240, -140)),
                       ((-240, k * 60, -140), (240, k * 60, -140))):
            pygame.draw.line(tela, (40, 40, 50), para_tela(p)[:2], para_tela(q)[:2], 1)
    base, topo = para_tela((-240, -240, -140)), para_tela((-240, -240, 120))
    pygame.draw.line(tela, (150, 150, 150), base[:2], topo[:2], 2)
    pygame.draw.polygon(tela, (150, 150, 150), [(topo[0], topo[1] - 10),
                        (topo[0] - 6, topo[1] + 4), (topo[0] + 6, topo[1] + 4)])
    tela.blit(fonte_p.render("vertical do mundo", True, (150, 150, 150)),
              (topo[0] - 70, topo[1] - 34))

    # placa: faces da mais distante para a mais próxima (algoritmo do pintor)
    pts = [para_tela(aplica(R, v)) for v in VERTICES]
    for idx, cor in sorted(FACES, key=lambda f: -sum(pts[i][2] for i in f[0])):
        pygame.draw.polygon(tela, cor, [pts[i][:2] for i in idx])
        pygame.draw.polygon(tela, (210, 210, 220), [pts[i][:2] for i in idx], 1)

    # eixos do sensor, presos à placa
    origem = para_tela(aplica(R, (0, 0, 0)))
    for v, cor, nome in EIXOS:
        ponta = para_tela(aplica(R, v))
        pygame.draw.line(tela, cor, origem[:2], ponta[:2], 4)
        pygame.draw.circle(tela, cor, (int(ponta[0]), int(ponta[1])), 6)
        tela.blit(fonte.render(nome, True, cor), (ponta[0] + 8, ponta[1] - 14))

    linhas = [
        f"acc = ({a[0]:+6.2f}, {a[1]:+6.2f}, {a[2]:+6.2f}) m/s²",
        f"roll (em X) = {math.degrees(roll):+6.1f}°    pitch (em Y) = {math.degrees(pitch):+6.1f}°",
        "yaw (giro em torno da vertical): o acelerômetro não mede — veja o cabeçalho do script",
    ]
    for i, txt in enumerate(linhas):
        tela.blit(fonte_p.render(txt, True, (220, 220, 220)), (16, 14 + 24 * i))


def main():
    global rodando
    ap = argparse.ArgumentParser(description="Protoboard em 3D guiada pelo acelerômetro")
    ap.add_argument("--porta", default="/dev/ttyUSB0")
    ap.add_argument("--suavizacao", type=float, default=0.2,
                    help="filtro passa-baixas (0 < alfa ≤ 1; 1 = sem filtro)")
    args = ap.parse_args()

    import pygame
    try:
        ser = serial.Serial(args.porta, 115200, timeout=1)
        threading.Thread(target=le_serial, args=(ser,), daemon=True).start()
        print(f"Conectado à porta {args.porta}")
    except serial.SerialException as e:
        ser = None
        print(f"⚠️ Não abri a porta serial ({e}). A placa ficará parada.")

    pygame.init()
    tela = pygame.display.set_mode((LARGURA, ALTURA))
    pygame.display.set_caption("Protoboard 3D — inclinação pelo acelerômetro")
    fonte = pygame.font.SysFont("dejavusans", 26, bold=True)
    fonte_p = pygame.font.SysFont("dejavusans", 17)
    relogio = pygame.time.Clock()
    f = list(acc)

    while rodando:
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                rodando = False
        a = acc
        f = [fi + args.suavizacao * (ai - fi) for fi, ai in zip(f, a)]
        roll, pitch = angulos_da_gravidade(*f)
        desenha(tela, fonte, fonte_p, roll, pitch, f)
        pygame.display.flip()
        relogio.tick(60)

    if ser and ser.is_open:
        ser.close()
    pygame.quit()


if __name__ == "__main__":
    main()
