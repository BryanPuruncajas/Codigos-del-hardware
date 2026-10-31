"""Ensayo XY visual + IMU via Ground WebSocket, avance optativo.

Sin --actuate solo lee. --actuate prepara empuje mientras se sujeta y usa voz.
Requiere firmware version12 en F11, recuperacion breve y referencia relativa.
Nunca abre COM9: Ground sigue siendo su lector.
"""
import argparse
import asyncio
import base64
import json
import math
from pathlib import Path
import shutil
import subprocess
import time
from datetime import datetime, timezone, timedelta

import websockets
from common.control_pack import make_args, build_control_payload


class TrialError(RuntimeError):
    pass


def control(preparing, yaw, reset=0, arm=0, forward=0):
    if not math.isfinite(forward) or not 0<=forward<=.03:
        raise ValueError('Avance experimental fuera de limite0..0.03')
    values = build_control_payload(make_args('p03', yaw_deg=math.degrees(yaw),
        yaw_visual_hold=True, yaw_support_prepare=preparing,
        yaw_base_power=15, yaw_support_min_power=10, yaw_max_power=16,
        yaw_servo_authority=25, yaw_kp=.4, yaw_ki=.25, yaw_kd=2,
        yaw_deadband_deg=1))
    fx, fz, tx, tz, aux, _ = values
    return dict(cmd='control', mode=12, arm=arm, reset=reset,
                fx=fx, fz=fz, tx=forward, tz=tz, aux=aux)


class Speaker:
    async def say(self, text, abort):
        print('VOZ:', text, flush=True)
        shell = shutil.which('powershell.exe')
        if not shell:
            raise TrialError('No hay voz PowerShell; no ejecutar el ensayo actuado.')
        script = ("$ErrorActionPreference='Stop'; Add-Type -AssemblyName System.Speech; "
                  "$s=New-Object System.Speech.Synthesis.SpeechSynthesizer; "
                  "$v=$s.GetInstalledVoices() | Where-Object { $_.VoiceInfo.Culture.TwoLetterISOLanguageName -eq 'es' } | Select-Object -First 1; "
                  "if($v){$s.SelectVoice($v.VoiceInfo.Name)}; "
                  "$s.Speak('" + text.replace("'", "''") + "');$s.Dispose()")
        encoded = base64.b64encode(script.encode('utf-16-le')).decode('ascii')
        proc = await asyncio.create_subprocess_exec(shell, '-NoProfile', '-NonInteractive',
            '-EncodedCommand', encoded, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0),
            stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
        done = asyncio.create_task(proc.wait())
        failed = asyncio.create_task(abort.wait())
        try:
            finished, _ = await asyncio.wait((done, failed), timeout=20,
                                             return_when=asyncio.FIRST_COMPLETED)
            if failed in finished or abort.is_set():
                raise TrialError('Preparacion interrumpida; cancelar la cuenta de voz.')
            if done not in finished or done.result() != 0:
                raise TrialError('Fallo o tiempo agotado en la voz.')
        finally:
            if proc.returncode is None:
                try:
                    proc.kill()
                except ProcessLookupError:
                    pass
                await proc.wait()
            for task in (done, failed):
                task.cancel()
            await asyncio.gather(done, failed, return_exceptions=True)


class Trial:
    def __init__(self, ws, seconds, log=None, speaker=None, forward=0):
        self.ws, self.seconds, self.log = ws, seconds, log
        self.forward=forward
        self.speaker = speaker or Speaker()
        self.latest = {}
        self.rx_counts = {}
        self.abort = asyncio.Event()
        self.reason = ''
        self.stage = 'preflight'
        self.owned = False
        self.guard_enabled = False
        self.stop_confirmed = False
        self.centered_since = None
        self.height_reference = None
        self.last_target_at = None

    def target_or_recovering(self):
        if self.target():
            return True
        return (self.last_target_at is not None and
                time.monotonic()-self.last_target_at < 3.5 and self.fresh(7) and
                self.fields(7).get('nicla_fresh') == 1 and
                self.fields(7).get('nicla_reset_pending') == 0)

    def capture_height_reference(self, now):
        # Record only. This does not enable barometric height control or imply
        # that a centred image establishes an absolute physical altitude.
        if self.height_reference is not None:
            return
        valid = self.stage == 'regular XY' and self.target() and self.fresh(1)
        if valid:
            a, b = self.fields(2), self.fields(1)
            valid = (self.fields(7).get('nicla_state') == 2 and
                     a.get('nicla_flag') == 65 and
                     abs(a.get('nicla_x', -999)-120) <= 24 and
                     abs(a.get('nicla_y', -999)-80) <= 10 and
                     a.get('nicla_y', 0)-a.get('nicla_h', 160)*.5 > 4 and
                     a.get('nicla_y', 160)+a.get('nicla_h', 160)*.5 < 156 and
                     math.isfinite(b.get('height', math.nan)) and
                     now-self.latest[1][0] < .4 and now-self.latest[7][0] < .4)
        if not valid:
            self.centered_since = None
            return
        if self.centered_since is None:
            self.centered_since = now
        elif now-self.centered_since >= .5:
            self.height_reference = dict(height=b['height'], visual_y=a['nicla_y'],
                source='barometer_at_visual_center', diagnostic_only=True)
            self.record('height_reference', self.height_reference)
            print('REFERENCIA_ALTURA_DIAGNOSTICA:', self.height_reference, flush=True)

    def record(self, kind, value):
        if self.log:
            self.log.write(json.dumps(dict(kind=kind, stage=self.stage,
                monotonic=time.monotonic(), value=value), ensure_ascii=False) + '\n')
            self.log.flush()

    def fresh(self, flag):
        return flag in self.latest and time.monotonic()-self.latest[flag][0] < 1.5

    def fields(self, flag):
        return self.latest[flag][1]

    def target(self):
        if not self.fresh(2) or not self.fresh(7):
            return False
        a, b = self.fields(2), self.fields(7)
        return (b.get('nicla_fresh') == 1 and b.get('nicla_reset_pending') == 0 and
                b.get('nicla_state') in (2, 3) and a.get('nicla_flag') in (65, 66))

    async def read(self):
        try:
            async for raw in self.ws:
                packet = json.loads(raw)
                self.record('rx', packet)
                if 'flag' in packet and isinstance(packet.get('fields'), dict):
                    flag = int(packet['flag'])
                    self.latest[flag] = (time.monotonic(), packet['fields'])
                    self.rx_counts[flag] = self.rx_counts.get(flag, 0)+1
                    if flag in (2,7) and self.target() and time.monotonic()-self.latest[2][0]<.3:
                        self.last_target_at=time.monotonic()
                    if flag == 2:
                        self.capture_height_reference(time.monotonic())
                if packet.get('cmd_ok') is False:
                    self.reason = str(packet.get('error', 'Ground rechazo un comando'))
                    self.abort.set()
        except asyncio.CancelledError:
            raise
        except Exception as error:
            self.reason = 'Enlace: ' + str(error)
            self.abort.set()
        finally:
            if self.guard_enabled:
                self.reason = self.reason or 'Ground desconectado'
                self.abort.set()

    async def send(self, command):
        self.record('tx', command)
        await self.ws.send(json.dumps(command))

    async def wait(self, predicate, timeout=6):
        end = time.monotonic()+timeout
        while time.monotonic() < end:
            if self.abort.is_set():
                raise TrialError(self.reason or 'Ensayo interrumpido')
            if predicate():
                return
            await asyncio.sleep(.05)
        raise TrialError('No se confirma estado en ' + self.stage)

    async def say(self, text):
        await self.speaker.say(text, self.abort)

    async def stable_target(self, timeout=6, stable_seconds=.5):
        end = time.monotonic()+timeout
        since = None
        while time.monotonic() < end:
            if self.abort.is_set():
                raise TrialError(self.reason or 'Ensayo interrumpido')
            if not self.fresh(3) or self.fields(3).get('armed') != 0:
                raise TrialError('No se confirma desarmado al verificar posicion.')
            now = time.monotonic()
            valid = (self.target() and now-self.latest[2][0] < .3 and
                     now-self.latest[7][0] < .4)
            if valid:
                if since is None:
                    since = now
                if now-since >= stable_seconds:
                    return
            else:
                since = None
            await asyncio.sleep(.05)
        raise TrialError('Se requiere objetivo FULL o PARTIAL confirmado y con datos recientes antes de armar.')

    async def guard(self):
        while True:
            await asyncio.sleep(.05)
            if not self.guard_enabled:
                continue
            reason = ''
            if not all(self.fresh(k) for k in (1, 3, 7, 10, 11)):
                reason = 'Telemetria atrasada'
            else:
                a, b, phase = self.fields(1), self.fields(3), self.fields(10).get('support_phase')
                power = max(b.get('motor1', math.inf), b.get('motor2', math.inf))
                if not math.isfinite(power) or power > .165:
                    reason = 'Potencia fuera de limite'
                elif not math.isfinite(a.get('battery', math.nan)) or a['battery'] < 3.5:
                    reason = 'Bateria bajo limite de ensayo'
                elif b.get('armed') != 1 or b.get('mode') != 12 or phase == 3:
                    reason = 'Fallo/desarmado a bordo'
                elif self.fields(11).get('support_version', 0) < 12:
                    reason = 'Firmware incompatible'
                elif not 0<=self.fields(11).get('support_forward_request',math.inf)<=self.forward+.00001:
                    reason = 'Avance fuera del limite solicitado'
            if reason:
                self.reason = reason
                self.abort.set()
                return

    async def stop(self):
        self.guard_enabled = False
        self.stage = 'stop'
        previous_f3 = self.rx_counts.get(3, 0)
        for _ in range(3):
            try:
                await asyncio.wait_for(self.send(dict(cmd='stop')), 1)
            except Exception:
                pass
            await asyncio.sleep(.05)
        # Ignore the abort event here: verify NEW disarmed telemetry, never cache.
        end = time.monotonic()+5
        while time.monotonic() < end:
            if (self.fresh(3) and self.rx_counts.get(3, 0) > previous_f3 and
                    self.fields(3).get('armed') == 0 and self.fields(3).get('mode') == 0):
                self.stop_confirmed = True
                break
            await asyncio.sleep(.05)
        print('STOP_CONFIRMADO:', self.stop_confirmed, flush=True)
        self.record('stop_confirmed', self.stop_confirmed)

    async def run(self):
        reader = asyncio.create_task(self.read())
        guard = asyncio.create_task(self.guard())
        try:
            await self.wait(lambda: all(self.fresh(k) for k in (1, 2, 3, 7)))
            if self.fields(3).get('armed') != 0 or self.fields(3).get('mode') != 0:
                raise TrialError('Pulsa STOP primero; no se toma control de otro ensayo.')
            core = self.fields(1)
            if not math.isfinite(core.get('yaw', math.nan)) or not math.isfinite(core.get('battery', math.nan)) or core['battery'] < 3.5:
                raise TrialError('Bateria o IMU no validas; no armar.')
            self.owned = True
            if not self.target():
                self.stage = 'recuperar objetivo desarmado'
                await self.send(dict(cmd='vision_bench_start'))
                await self.wait(self.target, 6)
            self.stage = 'compatibilidad desarmada'
            previous_f11 = self.rx_counts.get(11, 0)
            await self.send(control(True, self.fields(1)['yaw'], reset=1, arm=-1,forward=self.forward))
            try:
                await self.wait(lambda: self.fresh(11) and self.rx_counts.get(11, 0) > previous_f11 and
                    self.fields(11).get('support_version', 0) >= 12 and self.fresh(10) and
                    'support_phase' in self.fields(10) and self.fields(3).get('armed') == 0, 4)
            except TrialError as error:
                raise TrialError('Falta firmware XY version12 con recuperacion; no se armo.') from error
            zero = dict(cmd='control', mode=8, arm=-1, reset=0,
                        aux={5: 52.5, 6: 64.5, 7: 0, 8: 0})
            await self.send(zero)
            await self.wait(lambda: self.fresh(3) and self.fields(3).get('mode') == 8 and
                self.fields(3).get('armed') == 0 and max(abs(self.fields(3)['motor1']), abs(self.fields(3)['motor2'])) < .001)
            await asyncio.sleep(.4)
            await self.say('Sostenga el blimp con el waypoint enfrente y espacio libre. Se admite objetivo parcial para empezar desde abajo. No suelte hasta la cuenta.')
            self.stage = 'confirmar pose desarmado'
            if not self.target():
                await self.say('No veo el waypoint. Vuelva a apuntar hacia el globo. Aun sin motores.')
            await self.send(dict(cmd='vision_bench_start'))
            await self.stable_target()
            await self.send(zero)
            await self.wait(lambda: self.fresh(3) and self.fields(3).get('mode') == 8 and
                self.fields(3).get('armed') == 0 and max(abs(self.fields(3)['motor1']), abs(self.fields(3)['motor2'])) < .001)
            if not self.target_or_recovering():
                raise TrialError('Objetivo perdido en posicion de prueba; no se armo.')
            self.stage = 'armar a cero'
            if not self.fresh(1) or self.fields(1)['battery'] < 3.5:
                raise TrialError('Bateria no valida antes de armar.')
            await self.send(dict(zero, arm=1))
            await self.wait(lambda: self.fresh(3) and self.fields(3).get('mode') == 8 and
                self.fields(3).get('armed') == 1 and max(abs(self.fields(3)['motor1']), abs(self.fields(3)['motor2'])) < .001, 10)
            await asyncio.sleep(.6)
            if not self.target_or_recovering():
                raise TrialError('Objetivo perdido antes de preparar; no soltar.')
            await self.say('Preparando motores. Mantengalo sujeto.')
            # At the initial power reset the firmware needs one real target;
            # wait for a brief occlusion to clear instead of cancelling it.
            await self.wait(self.target, 3)
            if not self.fresh(1) or self.fields(1)['battery'] < 3.5 or not self.target_or_recovering():
                raise TrialError('Bateria/objetivo no validos antes de preparar.')
            self.stage = 'preparar empuje'
            await self.send(control(True, self.fields(1)['yaw'], reset=1,forward=self.forward))
            await self.wait(lambda: all(self.fresh(k) for k in (3, 10, 11)) and
                self.fields(3).get('armed') == 1 and self.fields(3).get('mode') == 12 and
                self.fields(10).get('support_phase') in (0,2) and
                .10 <= self.fields(3)['motor1'] <= .16001 and
                .10 <= self.fields(3)['motor2'] <= .16001 and
                self.fields(11).get('support_visual_bearing') == 1, 4)
            self.guard_enabled = True
            await asyncio.sleep(.4)
            if not self.target_or_recovering():
                raise TrialError('Objetivo perdido: no soltar.')
            await self.say('Tres, dos, uno. Suelte ahora.')
            if self.abort.is_set() or not self.target_or_recovering():
                raise TrialError(self.reason or 'Recuperacion agotada durante preparacion.')
            self.stage = 'regular XY'
            released_at = time.monotonic()
            previous_f10 = self.rx_counts.get(10, 0)
            previous_f11 = self.rx_counts.get(11, 0)
            await self.send(control(False, self.fields(1)['yaw'],forward=self.forward))  # no reset: retain spool
            await self.wait(lambda: self.fresh(10) and self.rx_counts.get(10, 0) > previous_f10 and
                self.fields(10).get('support_phase') in (1, 2) and self.fresh(11) and
                self.rx_counts.get(11, 0) > previous_f11 and self.fields(11).get('support_visual_bearing') == 1, 1)
            start = released_at
            while time.monotonic()-start < self.seconds:
                if self.abort.is_set():
                    raise TrialError(self.reason or 'Ensayo interrumpido')
                if self.fresh(10) and self.fresh(11) and self.fresh(3):
                    print(json.dumps(dict(t=round(time.monotonic()-start, 1),
                        visual=self.fields(10), bearing=self.fields(11),
                        actuators=self.fields(3)), ensure_ascii=False), flush=True)
                await asyncio.sleep(min(.5, max(0, self.seconds-(time.monotonic()-start))))
            if self.abort.is_set():
                raise TrialError(self.reason or 'Ensayo interrumpido')
        finally:
            self.guard_enabled = False
            if self.owned:
                await asyncio.shield(self.stop())
            for task in (guard, reader):
                task.cancel()
            await asyncio.gather(guard, reader, return_exceptions=True)


async def execute(args):
    forward=.03 if getattr(args,'forward',False) else 0
    if args.dry_run:
        print(json.dumps(dict(prepare=control(True, 0, reset=1,forward=forward), release=control(False, 0,forward=forward)), indent=2))
        return 0
    async with websockets.connect(args.url, open_timeout=5) as ws:
        await ws.send(json.dumps(dict(auth=dict(role='dev' if args.actuate else 'viewer', token=args.token))))
        if not args.actuate:
            end = time.monotonic()+3
            while time.monotonic() < end:
                try:
                    packet = await asyncio.wait_for(ws.recv(), end-time.monotonic())
                except asyncio.TimeoutError:
                    break
                print(packet)
            return 0
        path = Path(__file__).resolve().parents[1]/'logs'/'visual_support'
        path.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now(timezone(timedelta(hours=-5))).strftime('%Y%m%d_%H%M%S_%f')
        with (path/(stamp+'.jsonl')).open('x', encoding='utf-8') as log:
            print('LOG:', log.name, flush=True)
            trial = Trial(ws, args.seconds, log,forward=forward)
            failed = False
            try:
                await trial.run()
            except (Exception, asyncio.CancelledError) as error:
                failed = True
                print('ENSAYO DETENIDO:', str(error), flush=True)
                trial.record('failure', str(error))
            if trial.owned:
                text = ('Prueba finalizada. Motores desarmados.' if trial.stop_confirmed else
                        'Atencion. No se confirmo el desarmado.')
                try:
                    await trial.speaker.say(text, asyncio.Event())
                except Exception:
                    pass
            return 1 if failed or (trial.owned and not trial.stop_confirmed) else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='ws://127.0.0.1:8765')
    parser.add_argument('--token', default='changeme')
    parser.add_argument('--seconds', type=float, default=8)
    parser.add_argument('--actuate', action='store_true', help='Autoriza ensayo con motores; sin esto solo lee.')
    parser.add_argument('--forward',action='store_true',help='Opt-in: demanda de avance0.03, max12grados, condicionada por vision.')
    parser.add_argument('--dry-run', action='store_true', help='Muestra paquetes sin red ni hardware.')
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or not 2 <= args.seconds <= 12:
        parser.error('--seconds debe estar entre 2 y 12 (limite a bordo15s).')
    try:
        return asyncio.run(execute(args))
    except KeyboardInterrupt:
        return 130
    except Exception as error:
        print('ERROR:', str(error), flush=True)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
