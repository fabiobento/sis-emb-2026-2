#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Teste de integração dos scripts do projeto guiado (sem hardware, sem conta EI):
simulador_imu.py (porta virtual) + gabarito 01..04 + fake_eim.py.

Uso:  python3 tests/testa_scripts.py      (requer pyserial, numpy, six, edge_impulse_linux)
"""
import os, re, signal, subprocess, sys, time, shutil, tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
PY = sys.executable
ENV = dict(os.environ, PYTHONPATH=os.path.join(AQUI, "stubs"), PYTHONUNBUFFERED="1")
falhas = 0

def simulador(classe):
    p = subprocess.Popen([PY, os.path.join(RAIZ, "src/python/simulador_imu.py"), "--classe", classe],
                         stdout=subprocess.PIPE, text=True, env=ENV)
    linha = p.stdout.readline()
    porta = re.search(r"(/dev/pts/\d+)", linha).group(1)
    return p, porta

def roda(script, args, dur):
    p = subprocess.Popen([PY, script] + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         text=True, env=ENV)
    time.sleep(dur)
    p.send_signal(signal.SIGINT)
    out, _ = p.communicate(timeout=10)
    return out

def checa(nome, cond, info=""):
    global falhas
    print(("  OK   " if cond else "  FALHA ") + nome + (f"  [{info}]" if info else ""))
    if not cond:
        falhas += 1

# ---------------- 01: leitura e taxa ----------------
print("01_leitor_serial.py")
sim, porta = simulador("idle")
out = roda(os.path.join(RAIZ, "src/python/01_leitor_serial.py"), ["--porta", porta], 3.5)
taxas = [float(x) for x in re.findall(r"taxa medida: ([\d.]+)", out)]
checa("recebe linhas", out.count("Recebido:") > 100, f"{out.count('Recebido:')} linhas")
checa("taxa ~62,5 Hz", any(abs(t - 62.5) < 3 for t in taxas[1:]), str(taxas))
checa("encerra limpo com Ctrl+C", "Finalizando o programa." in out)
sim.send_signal(signal.SIGINT); sim.wait()

# ---------------- 02: features ----------------
print("02_extrai_features.py (gabarito)")
sim, porta = simulador("lift")
out = roda(os.path.join(RAIZ, "gabarito/02_extrai_features.py"), ["--porta", porta], 2.5)
zs = [float(z) for z in re.findall(r"Z:\s+([-\d.]+)", out)]
checa("extrai X/Y/Z", len(zs) > 80, f"{len(zs)} amostras")
checa("Z oscila em torno de g (lift)", zs and min(zs) < 7 and max(zs) > 12, f"min {min(zs):.1f} max {max(zs):.1f}")
sim.send_signal(signal.SIGINT); sim.wait()

# ---------------- 03: janela ----------------
print("03_janela_deslizante.py (gabarito)")
sim, porta = simulador("terrestrial")
out = roda(os.path.join(RAIZ, "gabarito/03_janela_deslizante.py"), ["--porta", porta], 5.0)
jan = re.findall(r"Janela (\d+): (\d+) amostras \((\d+) valores\), ([\d.]+) s", out)
checa("primeira janela após ~2 s e janelas de 125 amostras", jan and all(j[1] == "125" for j in jan), f"{len(jan)} janelas")
inter = [float(j[3]) for j in jan[1:]]
checa("intervalo entre janelas ≈ 12/62,5 = 0,19 s", inter and abs(sum(inter)/len(inter) - 0.192) < 0.03,
      f"média {sum(inter)/max(len(inter),1):.3f} s")
sim.send_signal(signal.SIGINT); sim.wait()

# ---------------- 04: classificação ----------------
tmp = tempfile.mkdtemp()
eim = os.path.join(tmp, "modelfile.eim")
with open(eim, "w") as f:
    f.write(f"#!{PY}\n" + open(os.path.join(AQUI, "fake_eim.py")).read())
os.chmod(eim, 0o755)
for classe in ["idle", "lift", "maritime", "terrestrial"]:
    print(f"04_classifica.py (gabarito) — simulando {classe}")
    sim, porta = simulador(classe)
    csv = os.path.join(tmp, f"{classe}.csv")
    out = roda(os.path.join(RAIZ, "gabarito/04_classifica.py"), [eim, "--porta", porta, "--csv", csv], 5.0)
    checa("lê o contrato do modelo", "janela de 125 amostras (2.00 s)" in out)
    preds = re.findall(r"\]\s+(\w+)\s+conf", out)
    acerto = preds.count(classe) / max(len(preds), 1)
    checa(f"classifica '{classe}'", len(preds) > 5 and acerto > 0.8, f"{len(preds)} inferências, {acerto:.0%} corretas")
    checa("grava CSV", os.path.exists(csv) and sum(1 for _ in open(csv)) > 5)
    checa("encerra o runner", "Finalizando o programa." in out and "Traceback" not in out, out[-300:] if "Traceback" in out else "")
    sim.send_signal(signal.SIGINT); sim.wait()
shutil.rmtree(tmp)

print("\nRESULTADO:", "TODOS OS TESTES PASSARAM" if falhas == 0 else f"{falhas} FALHA(S)")
sys.exit(1 if falhas else 0)
