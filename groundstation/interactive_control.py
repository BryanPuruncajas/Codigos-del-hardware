"""
interactive_control.py

Panel de control interactivo: aprieta una tecla, pasa algo. No hay que
editar el script ni volver a correrlo para cada prueba.

Requiere Windows (usa msvcrt, que viene incluido con Python, nada que instalar
aparte de pyserial).

Corre:
    python3 interactive_control.py
"""

import sys
import time
import msvcrt
import csv
import re
from datetime import datetime

from simple_control import BlimpLink
from user_parameters import ROBOT_MACS

# Estado local de los feedback loops (para poder mostrar ON/OFF y alternar)
state = {
    "yawEn": False,
    "pitchEn": False,
    "rollEn": False,
    "zEn": False,
    "servo2Mirror": True,
    "yawInvert": False,
    "servo1Trim": 0.0,
    "servo2Trim": 0.0,
    "swapFxTz": False,
    "mode": 0,       # 0 = manual/neutro, 2 = autonomo (Nicla)
    "show_telemetry": True,
}

MENU = """
================= PANEL DE CONTROL DEL BLIMP =================
  [2]  Activar modo AUTONOMO (persigue el globo, camara)
  [3]  Activar BUSQUEDA AUTONOMA (busca y cuenta hasta 4 globos)
  [0]  APAGADO (motores a cero, ignora fx/fz/tz por completo)
  [5]  STOP de emergencia (motores a cero)
  [c]  Encerar/centrar servos (misma accion que [0], mas explicito)

  --- Movimiento manual (empujon corto de 0.5s con flag=1, luego vuelve a [0]) ---
  [i]  Avanzar (fx)          [k]  Retroceder (fx)      [requiere S=ON]
  [j]  Girar izquierda (tz)  [l]  Girar derecha (tz)   [requiere S=ON]
  [u]  Subir (fz)            [o]  Bajar (fz)
  [U]  SUBIR SOSTENIDO (no se apaga solo, usa [0] o [5] para parar)
  [H]  Setear ALTURA OBJETIVO especifica (activa zEn, pide el valor, sostenido)

  [y]  Toggle yawEn   (feedback de yaw)      -> {yawEn}
  [p]  Toggle pitchEn (feedback de pitch)    -> {pitchEn}
  [r]  Toggle rollEn  (feedback de roll)     -> {rollEn}
  [z]  Toggle zEn     (feedback de altura)   -> {zEn}
  [M]  Toggle servo2Mirror (espejo del servo2) -> {servo2Mirror}
  [N]  Toggle yawInvert (invierte giro izq/der) -> {yawInvert}
  [ [ ] / [ ] ]  Servo1 trim -5 / +5 grados   -> {servo1Trim}
  [ - ] / [ = ]  Servo2 trim -5 / +5 grados   -> {servo2Trim}
  [S]  Toggle swapFxTz (intercambia avance/giro) -> {swapFxTz}
  [P]  Setear CUALQUIER parametro (pide nombre, tipo, valor)
  [R]  Reset del contador de busqueda (globos encontrados + memoria)

  [t]  Toggle mostrar telemetria en vivo     -> {show_telemetry}
  [m]  Mostrar este menu de nuevo
  [q]  Salir (manda stop antes de cerrar)
================================================================
"""

NUDGE_MAGNITUDE = 0.3      # que tan fuerte es el empujon (0 a 1)
NUDGE_DURATION = 0.5       # duracion por defecto (giro, retroceder, bajar)
NUDGE_DURATION_LONG = 1.5  # duracion mas larga (avanzar, subir)


def nudge(link, mac, fx=0.0, fz=0.0, tz=0.0, duration=NUDGE_DURATION):
    """Manda un empujon corto en modo manual y vuelve a apagado (flag=0) despues.
    OJO: flag=0 en el firmware significa 'APAGADO' (ignora fx/fz/tz por completo),
    no 'manual con mis valores'. Para que el empujon realmente mueva algo hay
    que usar flag=1 (cualquier valor != 0 y != 2 hace pasar tus valores directo).
    """
    link.send_control(mac, [1, fx, fz, 0, tz, 0, 0, 0, 0, 0, 0, 0, 0])
    time.sleep(duration)
    link.send_control(mac, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])


METRICS_CSV_PATH = "busqueda_metricas.csv"
METRICS_CSV_HEADER = ["timestamp", "globo", "t_buscando_s", "t_acoplando_s",
                      "t_total_s", "altura", "yaw", "nicla_w"]


def init_metrics_csv():
    """Crea el CSV con encabezado si todavia no existe."""
    try:
        with open(METRICS_CSV_PATH, "x", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(METRICS_CSV_HEADER)
        print(f">> Creado {METRICS_CSV_PATH} para guardar metricas de captura.")
    except FileExistsError:
        pass  # ya existe, se le sigue agregando (append)


def log_metrics_line(line: str):
    """Parsea una linea 'Telemetry-Metricas globo=N t_buscando=X ...' y la
    agrega como fila nueva al CSV, para analizarla despues (Excel/pandas).
    """
    nums = dict(re.findall(r"(\w+)=(-?[\d.]+)", line))
    try:
        globo = int(float(nums.get("globo", -1)))
        t_buscando = float(nums.get("t_buscando", 0))
        t_acoplando = float(nums.get("t_acoplando", 0))
        altura = float(nums.get("altura", 0))
        yaw = float(nums.get("yaw", 0))
        nicla_w = float(nums.get("nicla_w", 0))
    except ValueError:
        print(">> No se pudo parsear la linea de metricas, se ignora.")
        return

    t_total = t_buscando + t_acoplando
    with open(METRICS_CSV_PATH, "a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            datetime.now().isoformat(timespec="seconds"),
            globo, t_buscando, t_acoplando, t_total, altura, yaw, nicla_w,
        ])
    print(f">> Metrica guardada en {METRICS_CSV_PATH}: globo {globo}, "
          f"total {t_total:.1f}s")


def print_menu():
    print(MENU.format(
        yawEn="ON" if state["yawEn"] else "off",
        pitchEn="ON" if state["pitchEn"] else "off",
        rollEn="ON" if state["rollEn"] else "off",
        zEn="ON" if state["zEn"] else "off",
        servo2Mirror="ON" if state["servo2Mirror"] else "off",
        yawInvert="ON" if state["yawInvert"] else "off",
        servo1Trim=state["servo1Trim"],
        servo2Trim=state["servo2Trim"],
        swapFxTz="ON" if state["swapFxTz"] else "off",
        show_telemetry="ON" if state["show_telemetry"] else "off",
    ))


def reload_prefs(link, mac):
    link.send_control(mac, [state["mode"], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0])


def main():
    if not ROBOT_MACS:
        raise SystemExit("Pon la MAC de tu blimp en user_parameters.py (ROBOT_MACS).")
    mac = ROBOT_MACS[0]

    init_metrics_csv()

    link = BlimpLink()

    print(f"Agregando peer {mac}...")
    link.add_peer(mac)
    print("Registrando esta base para telemetria...")
    link.register_ground_station(mac)
    time.sleep(0.3)
    print("Recargando preferencias...")
    reload_prefs(link, mac)
    time.sleep(0.3)

    print_menu()

    try:
        while True:
            # --- revisa si el usuario aprieto una tecla, sin bloquear ---
            if msvcrt.kbhit():
                key_raw = msvcrt.getch().decode(errors="ignore")
                key = key_raw.lower()

                if key_raw == "U":
                    print(">> SUBIENDO SOSTENIDO (presiona 0 o 5 para parar)...")
                    link.send_control(mac, [1, 0, NUDGE_MAGNITUDE + 0.2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])

                elif key_raw == "H":
                    if not state["zEn"]:
                        print(">> zEn estaba apagado, lo activo primero...")
                        state["zEn"] = True
                        link.set_bool(mac, "zEn", True)
                        reload_prefs(link, mac)
                        time.sleep(0.3)
                    try:
                        target = float(input(">> Altura objetivo (mismo valor que ves en 'height=' de la telemetria): "))
                    except ValueError:
                        print(">> Valor invalido, cancelado.")
                    else:
                        print(f">> Yendo a altura objetivo = {target} (sostenido, usa 0 o 5 para parar)...")
                        link.send_control(mac, [1, 0, target, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])

                elif key_raw == "M":
                    state["servo2Mirror"] = not state["servo2Mirror"]
                    link.set_bool(mac, "servo2Mirror", state["servo2Mirror"])
                    reload_prefs(link, mac)
                    print(f">> servo2Mirror = {state['servo2Mirror']}")

                elif key_raw == "N":
                    state["yawInvert"] = not state["yawInvert"]
                    link.set_float(mac, "yawInvert", -1.0 if state["yawInvert"] else 1.0)
                    reload_prefs(link, mac)
                    print(f">> yawInvert = {state['yawInvert']}")

                elif key_raw == "[":
                    state["servo1Trim"] -= 5.0
                    link.set_float(mac, "servo1Trim", state["servo1Trim"])
                    reload_prefs(link, mac)
                    print(f">> servo1Trim = {state['servo1Trim']}")

                elif key_raw == "]":
                    state["servo1Trim"] += 5.0
                    link.set_float(mac, "servo1Trim", state["servo1Trim"])
                    reload_prefs(link, mac)
                    print(f">> servo1Trim = {state['servo1Trim']}")

                elif key_raw == "-":
                    state["servo2Trim"] -= 5.0
                    link.set_float(mac, "servo2Trim", state["servo2Trim"])
                    reload_prefs(link, mac)
                    print(f">> servo2Trim = {state['servo2Trim']}")

                elif key_raw == "=":
                    state["servo2Trim"] += 5.0
                    link.set_float(mac, "servo2Trim", state["servo2Trim"])
                    reload_prefs(link, mac)
                    print(f">> servo2Trim = {state['servo2Trim']}")

                elif key_raw == "S":
                    state["swapFxTz"] = not state["swapFxTz"]
                    link.set_bool(mac, "swapFxTz", state["swapFxTz"])
                    reload_prefs(link, mac)
                    print(f">> swapFxTz = {state['swapFxTz']}")

                elif key_raw == "P":
                    name = input(">> Nombre del parametro (ej. kpz, kdyaw, x_strength): ").strip()
                    tipo = input(">> Tipo (f=float, b=bool): ").strip().lower()
                    if tipo == "b":
                        val_str = input(">> Valor (true/false, 1/0): ").strip().lower()
                        val = val_str in ("true", "1", "on", "si", "yes")
                        link.set_bool(mac, name, val)
                    else:
                        try:
                            val = float(input(">> Valor (numero): "))
                            link.set_float(mac, name, val)
                        except ValueError:
                            print(">> Valor invalido, cancelado.")
                            val = None
                    if val is not None:
                        reload_prefs(link, mac)
                        print(f">> {name} = {val}")

                elif key == "2":
                    state["mode"] = 2
                    link.send_control(mac, [2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
                    print(">> Modo AUTONOMO activado.")

                elif key == "3":
                    state["mode"] = 3
                    link.send_control(mac, [3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
                    print(">> Modo BUSQUEDA AUTONOMA activado (busca 4 globos).")

                elif key_raw == "R":
                    link.send_control(mac, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1])
                    print(">> Reset de busqueda enviado (contador y memoria a cero).")

                elif key == "0":
                    state["mode"] = 0
                    link.send_control_repeated(mac, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
                    print(">> Modo MANUAL activado (APAGADO).")

                elif key == "5":
                    link.stop(mac)
                    state["mode"] = 0
                    print(">> STOP enviado.")

                elif key == "c":
                    link.send_control_repeated(mac, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
                    print(">> Servos centrados (flag=0, motores a cero).")

                elif key == "i":
                    print(">> Avanzando (empujon largo)... [requiere S activado]")
                    nudge(link, mac, fx=NUDGE_MAGNITUDE, duration=NUDGE_DURATION_LONG)

                elif key == "k":
                    print(">> Retrocediendo (empujon corto)... [requiere S activado]")
                    nudge(link, mac, fx=-NUDGE_MAGNITUDE)

                elif key == "j":
                    print(">> Girando izquierda (empujon corto)... [requiere S activado]")
                    nudge(link, mac, tz=-NUDGE_MAGNITUDE)

                elif key == "l":
                    print(">> Girando derecha (empujon corto)... [requiere S activado]")
                    nudge(link, mac, tz=NUDGE_MAGNITUDE)

                elif key == "u":
                    print(">> Subiendo (empujon largo)...")
                    nudge(link, mac, fz=NUDGE_MAGNITUDE, duration=NUDGE_DURATION_LONG)

                elif key == "o":
                    print(">> Bajando (empujon corto)...")
                    nudge(link, mac, fz=-NUDGE_MAGNITUDE)

                elif key == "y":
                    state["yawEn"] = not state["yawEn"]
                    link.set_bool(mac, "yawEn", state["yawEn"])
                    reload_prefs(link, mac)
                    print(f">> yawEn = {state['yawEn']}")

                elif key == "p":
                    state["pitchEn"] = not state["pitchEn"]
                    link.set_bool(mac, "pitchEn", state["pitchEn"])
                    reload_prefs(link, mac)
                    print(f">> pitchEn = {state['pitchEn']}")

                elif key == "r":
                    state["rollEn"] = not state["rollEn"]
                    link.set_bool(mac, "rollEn", state["rollEn"])
                    reload_prefs(link, mac)
                    print(f">> rollEn = {state['rollEn']}")

                elif key == "z":
                    state["zEn"] = not state["zEn"]
                    link.set_bool(mac, "zEn", state["zEn"])
                    reload_prefs(link, mac)
                    print(f">> zEn = {state['zEn']}")

                elif key == "t":
                    state["show_telemetry"] = not state["show_telemetry"]
                    print(f">> Mostrar telemetria = {state['show_telemetry']}")

                elif key == "m":
                    print_menu()

                elif key == "q":
                    print("Saliendo, mandando stop...")
                    link.stop(mac)
                    break

            # --- mientras tanto, muestra telemetria si llega algo (no bloqueante) ---
            if link.ser.in_waiting:
                line = link.ser.readline().decode(errors="ignore").strip()
                if line.startswith("Telemetry-Metricas"):
                    log_metrics_line(line)  # siempre se guarda, aunque telemetria este oculta
                if state["show_telemetry"] and (line.startswith("Telemetry") or line.startswith("I2C SCAN")):
                    print(line)

            time.sleep(0.02)

    except KeyboardInterrupt:
        print("Interrumpido, mandando stop...")
        link.stop(mac)
    finally:
        link.close()


if __name__ == "__main__":
    main()