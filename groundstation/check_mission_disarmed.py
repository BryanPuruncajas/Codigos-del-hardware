"""Mision breve: calcula ordenes DESARMADO. Nunca envia arm=1; termina en STOP.

Mantener actuadores fisicamente desconectados. Ejecutar sin otro lector COM.
"""
import argparse
import math
import struct
import time
import sys
import serial
import base64
import queue
import shutil
import subprocess
import threading
from common.control_pack import make_args, build_control_payload, NAME_TO_MODE
from common.telemetry import parse, LABELS

class Voice:
    def __init__(self, enabled=True):
        self.enabled = enabled
        self.failures = 0
        self.messages = queue.Queue(maxsize=8)
        if enabled:
            threading.Thread(target=self.worker, daemon=True).start()

    def say(self, text):
        print('VOZ:', text, flush=True)
        if self.enabled:
            critical = ('Girar ' in text or text.startswith(('Faltan ', 'Detenga ',
                        'Giro completado', 'Prueba finalizada', 'Giro no completado')))
            if critical:
                while True:
                    try:
                        self.messages.get_nowait()
                        self.messages.task_done()
                    except queue.Empty:
                        break
            try:
                self.messages.put_nowait((time.monotonic(), text, critical))
            except queue.Full:
                print('VOZ: cola llena; aviso omitido', flush=True)

    def worker(self):
        shell = shutil.which('powershell.exe')
        if not shell:
            self.failures += 1
            print('VOZ: PowerShell no disponible', flush=True)
            return
        while True:
            stamp, text, critical = self.messages.get()
            if not critical and time.monotonic()-stamp > 8:
                self.messages.task_done()
                continue
            script = ("Add-Type -AssemblyName System.Speech; "
                      "$speaker = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
                      "$spanish = $speaker.GetInstalledVoices() | Where-Object { $_.VoiceInfo.Culture.TwoLetterISOLanguageName -eq 'es' } | Select-Object -First 1; "
                      "if ($spanish) { $speaker.SelectVoice($spanish.VoiceInfo.Name) }; "
                      "$speaker.Speak('" + text.replace("'", "''") + "'); $speaker.Dispose()")
            encoded = base64.b64encode(script.encode('utf-16-le')).decode('ascii')
            try:
                subprocess.run([shell, '-NoProfile', '-NonInteractive', '-EncodedCommand', encoded],
                               creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0),
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                               timeout=20, check=True)
                print('VOZ_REPRODUCIDA:', text, flush=True)
            except (OSError, subprocess.SubprocessError) as error:
                self.failures += 1
                print('VOZ ERROR:', str(error), flush=True)
            finally:
                self.messages.task_done()

class MissionAnnouncements:
    def __init__(self, say, track2=False):
        self.say = say
        self.waypoints = (1, 3, 4, 2, 1) if track2 else (1, 2, 3, 4, 1)
        self.visited = 0
        self.state = None
        self.turn_pending = False
        self.lost = False
        self.bad_since = None
        self.good_since = None
        self.last_loss_notice = -100.0
        self.yaw = None
        self.yaw_at = 0
        self.waypoint_notice = ''
        self.last_turn_notice = -100.0
        self.in_turn_range = False

    def update(self, packet):
        now = time.monotonic()
        values = packet.values
        if packet.flag == 1:
            self.yaw, self.yaw_at = values[1], now
        elif packet.flag == 5:
            state, visited = int(values[0]), int(values[1])
            if visited > self.visited:
                waypoint = self.waypoints[min(visited-1, len(self.waypoints)-1)]
                notice = 'Waypoint %d confirmado. ' % waypoint
                if state == 3:
                    self.waypoint_notice = notice
                else:
                    self.say(notice)
            self.visited = visited
            if state != self.state:
                if state == 3:
                    self.turn_pending = True
                    self.in_turn_range = False
                    self.last_turn_notice = -100.0
                elif state == 4:
                    self.say('Giro completado. Buscando siguiente objetivo.' if self.state == 3
                             else 'Buscando recuperar el objetivo.')
                elif state == 1:
                    self.say('Nuevo objetivo adquirido. Puede acercarse.' if visited else
                             'Objetivo adquirido. Puede acercarse.')
                elif state == 5:
                    self.say('Recorrido terminado.')
                elif state == 6:
                    self.say('Giro no completado. Prueba detenida por la mision.')
                if state != 3:
                    self.turn_pending = False
                self.state = state
        elif packet.flag == 4 and self.state == 3 and self.yaw is not None:
            if now-self.yaw_at <= 0.5 and math.isfinite(self.yaw) and math.isfinite(values[3]):
                delta = (values[3]-self.yaw+math.pi) % (2*math.pi)-math.pi
                degrees = round(abs(math.degrees(delta)))
                in_range = abs(math.degrees(delta)) <= 5
                side = 'izquierda' if delta > 0 else 'derecha'
                if in_range and (not self.in_turn_range or self.turn_pending):
                    self.say(self.waypoint_notice + 'Detenga el giro. Mantenga quieto.')
                    self.last_turn_notice = now
                    self.in_turn_range = True
                elif not in_range and (self.turn_pending or
                        now-self.last_turn_notice >= 5):
                    # Convencion usada por VISION_YAW_SIGN=-1: yaw positivo izquierda.
                    instruction = ('Girar %d grados a la %s.' if self.turn_pending
                                   else 'Faltan %d grados a la %s.')
                    self.say(self.waypoint_notice + instruction % (degrees, side))
                    self.last_turn_notice = now
                    self.in_turn_range = False
                self.waypoint_notice = ''
                self.turn_pending = False
        elif packet.flag == 7:
            if self.state == 3 and values[0] != 0:
                # Dejar atras el globo es normal durante el giro de ruta.
                self.bad_since = None
                return
            lost = values[0] == 0 or int(values[1]) in (4, 6)
            if lost:
                self.good_since = None
                if self.bad_since is None:
                    self.bad_since = now
                if now-self.bad_since >= 0.6 and not self.lost:
                    if now-self.last_loss_notice >= 5:
                        self.say('Objetivo perdido.' if values[0] else 'Sin datos recientes de vision.')
                        self.last_loss_notice = now
                    self.lost = True
            else:
                self.bad_since = None
                if self.good_since is None:
                    self.good_since = now
                if now-self.good_since >= 0.7:
                    if self.lost and self.state == 1:
                        self.say('Objetivo recuperado. Puede acercarse.')
                    self.lost = False

def main():
    sys.stdout.reconfigure(errors='backslashreplace')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='COM9')
    parser.add_argument('--seconds', type=float, default=10)
    parser.add_argument('--test', choices=('p08', 'p10', 'p11', 'p11t2'), default='p08')
    parser.add_argument('--mute', action='store_true', help='Desactivar avisos hablados')
    parser.add_argument('--vision-bench', action='store_true', help='Modo 22: Track 2 desarmado sin supervisor barometrico')
    args = parser.parse_args()
    if args.vision_bench and args.test != 'p11t2':
        parser.error('--vision-bench requiere --test p11t2')
    voice = Voice(not args.mute)
    announcements = MissionAnnouncements(voice.say, track2=args.test == 'p11t2')
    announcing = False
    if not 1 <= args.seconds <= 240:
        parser.error('Duracion permitida: 1..240 segundos')
    destination = bytes.fromhex('dcb4d939b3b4')
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = args.port, 115200, 0.15
    ser.dtr = ser.rts = False
    ser.open()
    latest, counts = {}, {}
    def read():
        line = ser.readline().decode(errors='replace').strip()
        packet = parse(line)
        if packet:
            latest[packet.flag] = packet.values
            counts[packet.flag] = counts.get(packet.flag, 0)+1
            if announcing:
                announcements.update(packet)
        elif line:
            print('GROUND:', line, flush=True)
        return packet
    def send(mode=0, payload=None, reset=0):
        values = list(payload) if payload is not None else [0.0]*13
        values[0], values[10], values[11], values[12] = mode, -1, 0, reset
        ser.write(b'C'+destination+struct.pack('<13f', *values))
    try:
        end = time.monotonic()+3
        while time.monotonic() < end:
            read()
        if 3 not in latest or latest[3][4] != 0:
            raise RuntimeError('No hay confirmacion reciente de DESARMADO; no se inicia P08')
        if 1 not in latest or not all(math.isfinite(v) for v in latest[1][:4]):
            raise RuntimeError('No hay orientacion/altura valida; no se inicia P08')
        # Recibir telemetria no implica tener peer registrado para ENVIAR.
        ser.write(b'A'+destination)
        end = time.monotonic()+0.5
        while time.monotonic() < end:
            read()
        # Fijar la referencia solo DESPUES de escuchar la cuenta regresiva.
        # Continuar leyendo sensores mientras habla; no capturar un yaw antiguo.
        voice.say('Mantenga la orientacion de uno hacia dos. Tres. Dos. Uno.')
        countdown_end = time.monotonic() + (20 if voice.enabled else 3)
        yaw_received = False
        while time.monotonic() < countdown_end:
            packet = read()
            if packet and packet.flag == 1:
                yaw_received = math.isfinite(packet.values[1])
            if packet and packet.flag == 3 and packet.values[4] != 0:
                raise RuntimeError('ARMADO durante preparacion; se solicita STOP')
            if voice.enabled and voice.messages.unfinished_tasks == 0:
                break
        if voice.enabled and (voice.messages.unfinished_tasks or voice.failures):
            raise RuntimeError('Cuenta regresiva no reproducida correctamente; prueba cancelada')
        if not yaw_received:
            raise RuntimeError('Sin yaw reciente durante la preparacion; prueba cancelada')
        height = latest[1][0]
        mode = 22 if args.vision_bench else NAME_TO_MODE[args.test]
        fx, fz, tx, tz, aux, _ = build_control_payload(make_args(args.test, height=height))
        payload = [0.0]*13
        payload[1:5] = [fx, fz, tx, tz]
        for index, value in aux.items():
            payload[index] = value
        print('%s DESARMADO: altura de referencia=%.3f; NO se envia armado' % (args.test.upper(), height), flush=True)
        print('YAW_INICIAL_DEG=%.2f' % math.degrees(latest[1][1]), flush=True)
        send(mode, payload, reset=1)
        # Confirmar recepcion de modo y referencia por telemetria de ESP.
        ack_end = time.monotonic()+3
        mode_ack = reference_ack = False
        last_start_send = time.monotonic()
        while time.monotonic() < ack_end:
            packet = read()
            if packet and packet.flag == 3:
                if packet.values[4] != 0:
                    raise RuntimeError('ARMADO inesperado; se solicita STOP')
                mode_ack = int(packet.values[5]) == mode
            if packet and packet.flag == 4 and mode_ack:
                reference_ack = math.isfinite(packet.values[3])
                if reference_ack:
                    print('REFERENCIA_ESP_DEG=%.2f' % math.degrees(packet.values[3]), flush=True)
                    break
            if time.monotonic()-last_start_send >= 0.5:
                send(mode, payload)
                last_start_send = time.monotonic()
        if not (mode_ack and reference_ack):
            raise RuntimeError('Inicio y referencia no confirmados por ESP; se solicita STOP')
        announcing = True
        voice.say('Referencia guardada. Prueba iniciada. Sistema desarmado.')
        end = time.monotonic()+args.seconds
        last_send = last_print = time.monotonic()
        while time.monotonic() < end:
            packet = read()
            if packet and packet.flag == 3 and packet.values[4] != 0:
                raise RuntimeError('ARMADO detectado: se solicita STOP y se aborta')
            if packet and packet.flag == 5 and int(packet.values[0]) in (5, 6):
                print('Mision terminada o giro fallido: regreso a STOP', flush=True)
                break
            now = time.monotonic()
            if now-last_send >= 0.5:
                send(mode, payload)
                last_send = now
            if now-last_print >= 1:
                for flag in (1, 2, 3, 4, 5, 7, 8):
                    if flag in latest:
                        print('F%d %s' % (flag, dict(zip(LABELS[flag], latest[flag]))), flush=True)
                last_print = now
    finally:
        announcing = False
        try:
            for _ in range(3):
                send()
                time.sleep(0.05)
            end = time.monotonic()+2
            while time.monotonic() < end:
                read()
            print('FINAL F3:', latest.get(3), 'paquetes:', counts, flush=True)
            if 3 not in latest or latest[3][4:] != [0.0, 0.0]:
                print('ATENCION: STOP/desarmado no confirmado por telemetria', flush=True)
                voice.say('Atencion. Parada no confirmada por telemetria.')
            else:
                voice.say('Prueba finalizada. Sistema desarmado y en stop.')
        finally:
            ser.close()
            # El control y STOP ya terminaron; permitir que salga el ultimo aviso.
            deadline = time.monotonic()+10
            while voice.enabled and voice.messages.unfinished_tasks and time.monotonic()<deadline:
                time.sleep(0.05)

if __name__ == '__main__':
    main()
