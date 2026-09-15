"""
telemetry_bridge.py  -  Puente WebSocket bidireccional para la app Flutter

USO
---
    python telemetry_bridge.py COM5
    python telemetry_bridge.py /dev/ttyUSB0 --port 8765 --dev-token MICLAVESECRETA

Que hace
--------
Abre el mismo BlimpLink que usan run_test.py / simple_control.py para leer
el puerto serial de la base station, reenvia la telemetria a todos los
clientes conectados, permite que clientes autenticados como "dev" manden
comandos de vuelta (armar/desarmar/control directo), y guarda la misma
telemetria en logs/blimp_<timestamp>.csv con el mismo formato que usa
run_test.py (o sea, analyze.py funciona igual con vuelos hechos desde la
app).

No reemplaza ni modifica run_test.py: es un lector/escritor alternativo del
mismo puerto serial. No lo corras al mismo tiempo que run_test.py -- dos
procesos no pueden abrir el mismo puerto serial a la vez.

ROLES: VIEWER vs DEV
---------------------
El PRIMER mensaje que manda cada cliente, apenas conecta, tiene que ser un
mensaje de auth:

    {"auth": {"role": "viewer"}}
    {"auth": {"role": "dev", "token": "MICLAVESECRETA"}}

- Sin mandar auth, o con role="viewer": el cliente SOLO recibe telemetria.
  Cualquier otro mensaje que mande se ignora.
- Con role="dev" y el token correcto (--dev-token al arrancar el bridge,
  default 'changeme' -- CAMBIALO): el cliente puede ademas mandar comandos.
  Si el token no coincide, se lo trata como viewer igual (no se cae la
  conexion, simplemente no tiene permiso de mandar comandos).

Si el auth no coincide, el bridge responde:
    {"auth_ok": true,  "role": "dev"}
    {"auth_ok": true,  "role": "viewer"}   # token invalido -> degradado
    {"auth_ok": false, "error": "primer mensaje debe ser auth"}

TELEMETRIA (servidor -> todos los clientes)
--------------------------------------------
    {
        "t": 1735000000.123,
        "flag": 1,
        "fields": {"height": 1.23, "yaw": 0.41, ...}
    }
Ver LABELS en common/telemetry.py para el set de campos por flag.

COMANDOS (cliente dev -> servidor)
------------------------------------
    {"cmd": "arm"}
    {"cmd": "disarm"}
    {"cmd": "stop"}                      # SAFE_STOP, desarma
    {"cmd": "control",
     "mode": 0, "fx": 0.0, "fz": 0.0, "tx": 0.0, "tz": 0.0,
     "arm": 0, "reload": 0, "reset": 0,
     "aux": {"5": 0.12, "7": 0.08}}       # indices de aux como string

"control" es de bajo nivel: el cliente arma fx/fz/tx/tz/aux a mano, con los
indices crudos que espera ControlInput. Para P03-P11 eso significa conocer
el empaquetado de bits ALT_PACK/YAW_PACK (ver common/control_pack.py) y el
mapa de slots por modo -- exactamente lo que hace run_test.py. En vez de
reimplementar eso en la app, usa los comandos de alto nivel:

    {"cmd": "mission_start", "test": "p08", "height": 0.7,
     "kp": 0.30, "ki": 0.025, "kd": 0.10, "max_power": 15, "slew": 0.18,
     "vis_min_power": 6, "vis_max_power": 10, "vis_deadband_px": 20}

    {"cmd": "mission_update", "test": "p08", ...mismos campos...}

"test" es el mismo string que usarias como argumento de run_test.py
(p03..p11). Los demas campos son EXACTAMENTE los mismos nombres que los
--flags de run_test.py sin guiones (--vis-min-power -> "vis_min_power"),
y los que se omiten usan el mismo default que la CLI (ver
common/control_pack.py:DEFAULT_ARGS). El bridge arma fx/fz/tx/tz/aux con
common.control_pack.build_control_payload(), el MISMO codigo que usa
run_test.py, asi que el resultado es identico bit a bit.

- "mission_start": hace la secuencia completa de armado (igual que
  run_test.py): manda el paquete con arm=1, espera 4.2s a que arme el ESC,
  y despues manda reset=1 con las mismas referencias/ganancias. Uso: boton
  "Iniciar mision".
- "mission_update": NO rearma. Solo reenvia fx/fz/tx/tz/aux con reset=1 --
  para ajustar ganancias en caliente con la mision ya corriendo/armada.

Ambos responden con:
    {"cmd_ok": true, "cmd": "mission_start", "effective": ["...", "..."]}
"effective" son las mismas lineas de texto que run_test.py imprime como
"VALORES EFECTIVOS" (los parametros YA cuantizados que se van a aplicar).

BANCO DE PRUEBAS: P00 (servos) y P00M (motores)
------------------------------------------------
Equivalente de alto nivel a `run_test.py p00` / `p00m`, para el panel de
banco de pruebas de la app. A diferencia de mission_start/update, esta
logica NO pasa por common/control_pack.py (P00/P00M no usan el empaquetado
ALT_PACK/YAW_PACK ni el mapa de slots de P03-P11): son modos crudos con
selector de servo / potencia directa, asi que se arman aca mismo.

    {"cmd": "servo_set", "servo": "both", "angle1": 90, "angle2": 90}
                                                        # servo: '1'|'2'|'both'

Si "servo" es '1' o '2', mueve SOLO ese servo (el otro queda detach, igual
que la CLI). Si es 'both', mueve LOS DOS con sus propios angle1/angle2 --
con el SG90 (rango 0..180) el vertical de arranque es 90/90 (centro
simetrico, sin re-validar en vuelo todavia), avance 180/0, retroceso 0/180
(ver SERVO*_Z_DEG/FORWARD_DEG/BACK_DEG en firmware/src/app/ControlCommon.h).
Sirve para chequear a mano si el vector de empuje resultante apunta donde
deberia.

    {"cmd": "motor_arm", "servo1": 35, "servo2": 95}

Hace la secuencia completa de P00M: posiciona AMBOS servos con brushless
BLOQUEADOS, espera 1s, y ejecuta la secuencia de ARM del firmware viejo
(throttle minimo ~3.7s y vuelve a 0). Este comando tarda ~5s en responder
a proposito -- recien cuando responde cmd_ok es seguro mandar potencia.
SIN ESTO, motor_power no hace nada (los actuadores siguen desarmados).

    {"cmd": "motor_power", "motor": "both", "power": 10,
     "servo1": 35, "servo2": 95}

Aplica potencia (0..100%) en caliente sobre los servos ya armados por
motor_arm. Hay que seguir mandando los mismos servo1/servo2 en cada
llamada (el firmware no los recuerda entre paquetes de este modo). Para
cortar YA: mandar power=0, o directamente {"cmd": "stop"} (SAFE_STOP,
desarma todo).

Opcional en cualquier comando: "mac": "DC:B4:D9:39:B3:B4" para apuntar a un
robot especifico. Si no se manda, usa ROBOT_MACS[0] de user_parameters.py.

Cada comando recibe una confirmacion LOCAL del bridge (que el byte salio
por el puerto serial), no una confirmacion del firmware -- esa la vas a
ver aparte, en la telemetria normal que sigue llegando (por ejemplo, el
campo "armed" del flag 3 cambiando a 1):
    {"cmd_ok": true, "cmd": "arm"}
    {"cmd_ok": false, "cmd": "arm", "error": "sin permiso (rol viewer)"}
"""

import argparse
import asyncio
import json
import struct
import time
from pathlib import Path

import websockets

from common.link import BlimpLink, mac_to_bytes
from common.telemetry import LABELS, parse
from common.control_pack import NAME_TO_MODE, build_control_payload, make_args
from common.csv_logger import CsvLogger
from user_parameters import ROBOT_MACS

clients: dict[websockets.WebSocketServerProtocol, str] = {}  # ws -> role
serial_write_lock = asyncio.Lock()

# El firmware reporta su modo actual en cada trama del flag 3 (ver
# Telemetry::sendActuators). Lo guardamos aca para que arm()/disarm() no
# tengan que adivinar un modo -- si mandaramos mode=0 a ciegas mientras
# una mision (P08, etc.) esta corriendo, eso fuerza una transicion a
# SAFE_STOP y mata la mision, aunque la intencion era solo armar/desarmar.
last_known_mode = 0


# ============================================================================
# ESCRITURA SIN ESPERAR RESPUESTA
#
# BlimpLink.send()/control() escriben Y ADEMAS leen una linea de vuelta
# (_line(0.4)) para mostrar el ack en consola. Si hacemos eso mientras el
# read_loop de mas abajo tambien esta leyendo el mismo puerto en continuo,
# se pisan: el ack de un comando puede terminar consumido por el lector de
# telemetria (o viceversa), y ninguno de los dos lados ve lo que esperaba.
#
# Por eso aca escribimos DIRECTO al serial, sin leer respuesta. El efecto
# del comando se ve igual en la telemetria normal (por ejemplo "armed"
# cambiando), que ya viaja por el unico lector continuo (read_loop).
# ============================================================================

def _pack_control(mode=0, fx=0, fz=0, tx=0, tz=0, arm=0, reload_=0, reset=0,
                   aux=None):
    p = [0.0] * 13
    p[0] = mode; p[1] = fx; p[2] = fz; p[3] = tx; p[4] = tz
    if aux:
        for idx, val in aux.items():
            p[int(idx)] = float(val)
    p[10] = arm; p[11] = reload_; p[12] = reset
    return p


async def write_control(link: BlimpLink, mac: str, **kwargs) -> None:
    params = _pack_control(**kwargs)
    payload = b'C' + mac_to_bytes(mac) + struct.pack('<13f', *params)
    async with serial_write_lock:
        link.ser.write(payload)


# ============================================================================
# WEBSOCKET: TELEMETRIA (broadcast) + COMANDOS (solo dev)
# ============================================================================

async def broadcast(msg: str) -> None:
    if not clients:
        return
    await asyncio.gather(
        *(c.send(msg) for c in clients),
        return_exceptions=True,
    )


def make_handler(link: BlimpLink, dev_token: str):

    async def handler(ws: websockets.WebSocketServerProtocol) -> None:
        role = None
        try:
            # Primer mensaje OBLIGATORIO: auth.
            first = await ws.recv()
            try:
                msg = json.loads(first)
            except (json.JSONDecodeError, TypeError):
                msg = {}

            auth = msg.get('auth') if isinstance(msg, dict) else None
            if not isinstance(auth, dict):
                await ws.send(json.dumps({
                    'auth_ok': False,
                    'error': 'primer mensaje debe ser auth',
                }))
                await ws.close()
                return

            requested_role = auth.get('role', 'viewer')
            if requested_role == 'dev' and auth.get('token') == dev_token:
                role = 'dev'
            else:
                role = 'viewer'

            clients[ws] = role
            await ws.send(json.dumps({'auth_ok': True, 'role': role}))
            print(f"[bridge] cliente conectado como '{role}' "
                  f"({len(clients)} activos)")

            async for raw in ws:
                if role != 'dev':
                    continue  # viewers no pueden mandar comandos, se ignora

                try:
                    msg = json.loads(raw)
                except (json.JSONDecodeError, TypeError):
                    continue

                cmd = msg.get('cmd')
                mac = msg.get('mac') or (ROBOT_MACS[0] if ROBOT_MACS else None)
                if not mac:
                    await ws.send(json.dumps({
                        'cmd_ok': False, 'cmd': cmd,
                        'error': 'sin MAC (configura ROBOT_MACS)',
                    }))
                    continue

                try:
                    effective = None

                    if cmd == 'arm':
                        await write_control(link, mac, mode=last_known_mode, arm=1)
                    elif cmd == 'disarm':
                        await write_control(link, mac, mode=last_known_mode, arm=-1)
                    elif cmd == 'stop':
                        # STOP si es a proposito un cambio de modo: fuerza
                        # SAFE_STOP y desarma, para cortar cualquier mision
                        # en curso de forma segura.
                        for _ in range(3):
                            await write_control(link, mac, mode=0, arm=-1)
                            await asyncio.sleep(0.05)
                    elif cmd == 'control':
                        await write_control(
                            link, mac,
                            mode=msg.get('mode', 0),
                            fx=msg.get('fx', 0), fz=msg.get('fz', 0),
                            tx=msg.get('tx', 0), tz=msg.get('tz', 0),
                            arm=msg.get('arm', 0),
                            reload_=msg.get('reload', 0),
                            reset=msg.get('reset', 0),
                            aux=msg.get('aux'),
                        )
                    elif cmd in ('mission_start', 'mission_update'):
                        test = msg.get('test')
                        if test not in NAME_TO_MODE:
                            await ws.send(json.dumps({
                                'cmd_ok': False, 'cmd': cmd,
                                'error': f'test desconocido: {test!r} '
                                         f'(usa p03..p11, igual que run_test.py)',
                            }))
                            continue

                        overrides = {k: v for k, v in msg.items()
                                     if k not in ('cmd', 'test', 'mac')}
                        try:
                            control_args = make_args(test, **overrides)
                            fx, fz, tx, tz, aux, effective = build_control_payload(control_args)
                        except (TypeError, ValueError) as e:
                            await ws.send(json.dumps({
                                'cmd_ok': False, 'cmd': cmd,
                                'error': f'parametros invalidos: {e}',
                            }))
                            continue

                        mode = NAME_TO_MODE[test]
                        if cmd == 'mission_start':
                            # Misma secuencia que run_test.py: ARM con el
                            # paquete completo, esperar al ESC, y recien
                            # despues RESET para arrancar limpio.
                            await write_control(link, mac, mode=mode,
                                                 fx=fx, fz=fz, tx=tx, tz=tz,
                                                 aux=aux, arm=1)
                            await asyncio.sleep(4.2)
                        await write_control(link, mac, mode=mode,
                                             fx=fx, fz=fz, tx=tx, tz=tz,
                                             aux=aux, reset=1)

                    elif cmd == 'servo_set':
                        # P00: si servo es '1'/'2', mueve SOLO ese servo (el
                        # otro queda detach). Si es 'both', mueve los DOS a
                        # la vez con SUS PROPIOS angulos. Con el SG90 (04/09,
                        # rango 0..180) el default es 90/90 -- centro
                        # simetrico sin re-validar en vuelo todavia; ver
                        # SERVO*_Z_DEG en ControlCommon.h.
                        servo = str(msg.get('servo', 'both'))
                        angle1 = float(msg.get('angle1', 90.0))
                        angle2 = float(msg.get('angle2', 90.0))
                        if not (0.0 <= angle1 <= 180.0 and 0.0 <= angle2 <= 180.0):
                            await ws.send(json.dumps({
                                'cmd_ok': False, 'cmd': cmd,
                                'error': 'angle1/angle2 deben estar entre 0 y 180 grados',
                            }))
                            continue
                        selector = {'1': 1.0, '2': 2.0, 'both': 3.0}.get(servo)
                        if selector is None:
                            await ws.send(json.dumps({
                                'cmd_ok': False, 'cmd': cmd,
                                'error': f"servo debe ser '1', '2' o 'both', "
                                         f"recibido {servo!r}",
                            }))
                            continue
                        await write_control(link, mac, mode=NAME_TO_MODE['p00'],
                                             aux={5: angle1, 6: angle2, 7: selector},
                                             reset=1)
                        effective = [f'Servo {servo}: S1={angle1:.0f}° S2={angle2:.0f}°']

                    elif cmd == 'motor_arm':
                        # P00M, paso a paso, igual que run_test.py:
                        #  1) posiciona AMBOS servos con brushless bloqueados
                        #  2) secuencia de ARM del firmware viejo: throttle
                        #     minimo ~3.7s y vuelve a 0 -- ahi si ya se puede
                        #     mandar potencia con motor_power.
                        # A diferencia de la CLI (que usa el tiempo que tarda
                        # el humano en escribir "ARMAR" como colchon), aca
                        # esperamos explicito antes de responder cmd_ok.
                        s1 = float(msg.get('servo1', 90.0))
                        s2 = float(msg.get('servo2', 90.0))
                        if not (0.0 <= s1 <= 180.0 and 0.0 <= s2 <= 180.0):
                            await ws.send(json.dumps({
                                'cmd_ok': False, 'cmd': cmd,
                                'error': 'servo1/servo2 deben estar entre 0 y 180 grados',
                            }))
                            continue
                        await write_control(link, mac, mode=NAME_TO_MODE['p00'],
                                             aux={5: s1, 6: s2, 7: 3.0}, reset=1)
                        await asyncio.sleep(1.0)
                        await write_control(link, mac, mode=NAME_TO_MODE['p00m'],
                                             arm=1, aux={5: s1, 6: s2, 7: 0.0, 8: 0.0})
                        await asyncio.sleep(4.0)
                        effective = [f'Servos en S1={s1:.0f}° S2={s2:.0f}°, ESC armado']

                    elif cmd == 'motor_power':
                        # Actualiza potencia en caliente (mode ya armado por
                        # motor_arm). Sin ARM previo, el firmware ignora la
                        # potencia porque los actuadores siguen desarmados.
                        motor = str(msg.get('motor', 'both'))
                        power = float(msg.get('power', 0.0))
                        s1 = float(msg.get('servo1', 35.0))
                        s2 = float(msg.get('servo2', 95.0))
                        if motor not in ('1', '2', 'both'):
                            await ws.send(json.dumps({
                                'cmd_ok': False, 'cmd': cmd,
                                'error': f"motor debe ser '1', '2' o 'both', "
                                         f"recibido {motor!r}",
                            }))
                            continue
                        if not (0.0 <= power <= 100.0):
                            await ws.send(json.dumps({
                                'cmd_ok': False, 'cmd': cmd,
                                'error': 'power debe estar entre 0 y 100',
                            }))
                            continue
                        m1 = power / 100.0 if motor in ('1', 'both') else 0.0
                        m2 = power / 100.0 if motor in ('2', 'both') else 0.0
                        await write_control(link, mac, mode=NAME_TO_MODE['p00m'],
                                             aux={5: s1, 6: s2, 7: m1, 8: m2})
                        effective = [f'Motor {motor}: {power:.0f}%']

                    else:
                        await ws.send(json.dumps({
                            'cmd_ok': False, 'cmd': cmd,
                            'error': f'comando desconocido: {cmd}',
                        }))
                        continue

                    ok_msg = {'cmd_ok': True, 'cmd': cmd}
                    if effective is not None:
                        ok_msg['effective'] = effective
                    await ws.send(json.dumps(ok_msg))

                except (websockets.exceptions.ConnectionClosed, OSError):
                    # El cliente se corto justo al mandar/recibir la
                    # confirmacion del comando (tipico de celular con la
                    # pantalla apagada o cambiando de app). No es un bug
                    # del bridge, no hace falta traceback.
                    break

                except Exception as e:  # noqa: BLE001 - avisamos cualquier otro fallo al cliente
                    try:
                        await ws.send(json.dumps({
                            'cmd_ok': False, 'cmd': cmd, 'error': str(e),
                        }))
                    except (websockets.exceptions.ConnectionClosed, OSError):
                        break

        except (websockets.exceptions.ConnectionClosed, OSError):
            # Conexion cortada de golpe (Android matando la app en segundo
            # plano, WiFi inestable, etc.) -- normal en un celular, no
            # ensuciamos la terminal con el traceback completo.
            pass

        finally:
            clients.pop(ws, None)
            print(f"[bridge] cliente desconectado ({len(clients)} activos)")

    return handler


# ============================================================================
# LECTURA CONTINUA DEL SERIAL -> BROADCAST
# ============================================================================

async def read_loop(link: BlimpLink, logger: CsvLogger) -> None:
    global last_known_mode
    loop = asyncio.get_event_loop()
    gen = link.lines()

    def next_line():
        try:
            return next(gen)
        except StopIteration:
            return None

    while True:
        line = await loop.run_in_executor(None, next_line)
        if line is None:
            break

        tel = parse(line)
        if tel is None:
            continue

        logger.add(tel)

        labels = LABELS.get(tel.flag, [])
        fields = dict(zip(labels, tel.values))

        if tel.flag == 3 and 'mode' in fields:
            last_known_mode = int(fields['mode'])

        payload = {
            't': time.time(),
            'flag': tel.flag,
            'fields': fields,
        }
        await broadcast(json.dumps(payload))


async def main(port_name: str, ws_port: int, dev_token: str) -> None:
    link = BlimpLink(port_name)
    print(f"[bridge] serial abierto en {port_name}")

    # Mismo CsvLogger que usa run_test.py: cada linea de telemetria que pasa
    # por el bridge (venga la conexion de la app o no) queda en logs/, con
    # el mismo formato que ya lee analyze.py.
    logger = CsvLogger(Path(__file__).resolve().parent.parent / 'logs')
    print(f"[bridge] guardando telemetria en {logger.path}")

    # HANDSHAKE INICIAL -- sin esto, la base station puede RECIBIR
    # telemetria del robot sin problema, pero no tiene registrado el peer
    # de ESP-NOW para poder ENVIARLE comandos de vuelta. run_test.py hace
    # exactamente esto mismo antes de mandar cualquier control (ver
    # run_test.py, justo despues de abrir el BlimpLink).
    if ROBOT_MACS:
        mac = ROBOT_MACS[0]
        print(f"[bridge] registrando peer ESP-NOW con {mac}...")
        print("  " + str(link.add_peer(mac)))
        print("  " + str(link.register_ground(mac)))
    else:
        print("[bridge] AVISO: ROBOT_MACS esta vacio en user_parameters.py -- "
              "no se pudo hacer el handshake inicial, los comandos de armar/"
              "control probablemente no van a llegar al robot.")

    print(f"[bridge] escuchando WebSocket en ws://0.0.0.0:{ws_port}")
    print("[bridge] en la app Flutter, conecta a ws://<IP-de-esta-laptop>:"
          f"{ws_port}  (misma red WiFi)")
    if dev_token == 'changeme':
        print("[bridge] AVISO: estas usando el --dev-token por defecto "
              "('changeme'). Cambialo antes de usar esto fuera de tu red "
              "de confianza.")

    handler = make_handler(link, dev_token)

    async with websockets.serve(handler, '0.0.0.0', ws_port):
        try:
            await read_loop(link, logger)
        finally:
            link.close()
            logger.close()
            print(f"[bridge] CSV cerrado: {logger.path}")


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('serial_port', help='Puerto de la base station, '
                                         'ej. COM5 o /dev/ttyUSB0')
    ap.add_argument('--port', type=int, default=8765,
                     help='Puerto WebSocket a exponer (default: 8765)')
    ap.add_argument('--dev-token', default='changeme',
                     help="Token que un cliente debe mandar para operar "
                          "como 'dev' (poder mandar comandos). Cambialo del "
                          "default antes de usar esto en una red que no "
                          "controles vos.")
    args = ap.parse_args()

    try:
        asyncio.run(main(args.serial_port, args.port, args.dev_token))
    except KeyboardInterrupt:
        print('\n[bridge] cerrado')