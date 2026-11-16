#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
(Opcional) Etapa 3, Parte B — Cubo 3D que gira conforme o acelerômetro.
Adaptado do roteiro de 2025-2. Requer:  pip install pygame  (dentro da venv)
Uso:  python3 cubo3d.py --porta /dev/ttyUSB0
"""
import argparse
import serial
import time
import threading
import math
import pygame

# --- Configurações da Janela e do Cubo ---
WIDTH, HEIGHT = 800, 600
CENTER_X, CENTER_Y = WIDTH // 2, HEIGHT // 2
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
# --- NOVAS CORES PARA OS EIXOS ---
RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
VIEW_DISTANCE = 150.0

# --- Vértices do Cubo ---
SCALE = 3
CUBE_VERTICES = [
    [-20 * SCALE, -20 * SCALE, 20 * SCALE], [ 20 * SCALE, -20 * SCALE, 20 * SCALE],
    [ 20 * SCALE,  20 * SCALE, 20 * SCALE], [-20 * SCALE,  20 * SCALE, 20 * SCALE],
    [-20 * SCALE, -20 * SCALE, -20 * SCALE],[ 20 * SCALE, -20 * SCALE, -20 * SCALE],
    [ 20 * SCALE,  20 * SCALE, -20 * SCALE], [-20 * SCALE,  20 * SCALE, -20 * SCALE]
]

# --- Arestas do Cubo ---
EDGES = [
    (0, 1), (1, 2), (2, 3), (3, 0), (4, 5), (5, 6), (6, 7), (7, 4),
    (0, 4), (1, 5), (2, 6), (3, 7)
]

# --- NOVO: DEFINIÇÃO DOS PONTOS DOS EIXOS ---
# O primeiro ponto é a origem (0,0,0). O segundo é a extremidade do eixo.
AXIS_LENGTH = 3.5 * SCALE * 2 # Um pouco maior que o cubo
AXES_VERTICES = [
    [0, 0, 0], [AXIS_LENGTH, 0, 0],   # Eixo X
    [0, 0, 0], [0, AXIS_LENGTH, 0],   # Eixo Y
    [0, 0, 0], [0, 0, AXIS_LENGTH]    # Eixo Z
]

# --- Variáveis Globais ---
accel_data = [0.0, 0.0, 9.8]
running = True

# --- Função para Ler os Dados Seriais (em uma Thread) ---
def read_serial_data(ser):
    """Lê a porta serial em segundo plano e atualiza a variável global."""
    global accel_data, running
    while running:
        try:
            line = ser.readline().decode("utf-8").strip()
            if line:
                parts = line.split(",")
                if len(parts) == 3:
                    accel_data = [float(p) for p in parts]
        except (serial.SerialException, ValueError, UnicodeDecodeError):
            pass
        except Exception as e:
            print(f"Erro inesperado na thread serial: {e}")
            running = False
            break
        time.sleep(1 / 100)

# --- Função de Projeção (Refatorada para ser reutilizável) ---
def project_point(point, angle_x, angle_y):
    """Aplica rotação e projeção de perspectiva a um único ponto 3D."""
    x, y, z = point[0], point[1], point[2]

    # Rotação em torno do eixo Y (Yaw)
    rot_x = z * math.sin(angle_y) + x * math.cos(angle_y)
    rot_z = z * math.cos(angle_y) - x * math.sin(angle_y)
    
    # Rotação em torno do eixo X (Pitch)
    rot_y = y * math.cos(angle_x) - rot_z * math.sin(angle_x)
    final_z = y * math.sin(angle_x) + rot_z * math.cos(angle_x)
    final_x = rot_x
    
    # Projeção de Perspectiva 3D -> 2D
    factor = VIEW_DISTANCE / (VIEW_DISTANCE + final_z) if VIEW_DISTANCE + final_z != 0 else 1
    
    projected_x = final_x * factor + CENTER_X
    projected_y = rot_y * factor + CENTER_Y
    
    return (projected_x, projected_y)


# --- Função Principal ---
def main():
    global running

    # --- Configuração e Inicialização da Porta Serial ---
    ap = argparse.ArgumentParser(description="Cubo 3D guiado pelo acelerômetro")
    ap.add_argument("--porta", default="/dev/ttyUSB0")
    port = ap.parse_args().porta
    baudrate = 115200
    ser = None
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"Conectado com sucesso à porta {port}")
        serial_thread = threading.Thread(target=read_serial_data, args=(ser,))
        serial_thread.daemon = True
        serial_thread.start()
    except serial.SerialException as e:
        print(f"⚠️ Erro ao abrir porta serial: {e}\nO programa continuará, mas o cubo ficará estático.")

    # --- Inicialização do Pygame ---
    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption("Cubo 3D com Acelerômetro e Eixos XYZ")
    clock = pygame.time.Clock()
    # --- NOVO: CONFIGURAÇÃO DA FONTE PARA OS RÓTULOS DOS EIXOS ---
    font = pygame.font.SysFont('arial', 24)

    # --- Loop Principal de Renderização ---
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        screen.fill(BLACK)

        # --- Mapeamento dos Dados do Acelerômetro para Ângulos ---
        acc_x, acc_y, acc_z = accel_data
        angle_x = math.atan2(acc_y, math.sqrt(acc_x**2 + acc_z**2))
        angle_y = -math.atan2(acc_x, math.sqrt(acc_y**2 + acc_z**2))
        
        # --- Lógica de Projeção ---
        # Projeta os pontos do cubo
        projected_cube_points = [project_point(v, angle_x, angle_y) for v in CUBE_VERTICES]
        # --- NOVO: Projeta os pontos dos eixos ---
        projected_axes_points = [project_point(v, angle_x, angle_y) for v in AXES_VERTICES]

        # --- Desenhar os Eixos ---
        # Eixo X (Vermelho)
        pygame.draw.line(screen, RED, projected_axes_points[0], projected_axes_points[1], 2)
        # Eixo Y (Verde)
        pygame.draw.line(screen, GREEN, projected_axes_points[2], projected_axes_points[3], 2)
        # Eixo Z (Azul)
        pygame.draw.line(screen, BLUE, projected_axes_points[4], projected_axes_points[5], 2)

        # --- Desenhar as Arestas do Cubo ---
        for edge in EDGES:
            p1 = projected_cube_points[edge[0]]
            p2 = projected_cube_points[edge[1]]
            pygame.draw.line(screen, WHITE, p1, p2, 2)
            
        # --- NOVO: Desenhar os rótulos dos eixos ---
        label_x = font.render('X', True, RED)
        label_y = font.render('Y', True, GREEN)
        label_z = font.render('Z', True, BLUE)
        
        # O ponto [1] é a extremidade do eixo X, [3] do Y, e [5] do Z
        screen.blit(label_x, (projected_axes_points[1][0] + 5, projected_axes_points[1][1]))
        screen.blit(label_y, (projected_axes_points[3][0] + 5, projected_axes_points[3][1]))
        screen.blit(label_z, (projected_axes_points[5][0] + 5, projected_axes_points[5][1]))

        # --- Atualizar a Tela ---
        pygame.display.flip()
        clock.tick(60)

    # --- Finalização ---
    if ser and ser.is_open:
        ser.close()
        print("Porta serial fechada.")
    pygame.quit()

if __name__ == '__main__':
    main()