"""Five-second fixed-pose thrust direction check; no vision/PID, no firmware upload."""
import argparse
import asyncio
import json
import math
from pathlib import Path
import sys
import time
from datetime import datetime

ROOT=Path(r'C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware')
sys.path.insert(0,str(ROOT/'groundstation'))
import websockets
from check_visual_support import Trial, TrialError

TRIM_DEG=4.0

def angles(trim):
    if not math.isfinite(trim) or not 0<=trim<=12:
        raise ValueError('Trim debe estar entre0 y12grados hacia delante')
    return 48.5+trim,68.5-trim

def packet(power=0,arm=0):
    s1,s2=angles(TRIM_DEG)
    return dict(cmd='control',mode=8,arm=arm,reset=0,
                aux={5:s1,6:s2,7:power,8:power})


class Neutral(Trial):
    async def guard(self):
        while True:
            await asyncio.sleep(.05)
            if not self.guard_enabled:
                continue
            reason=''
            if time.monotonic()-self.power_started>18:
                reason='Limite total de ensayo'
            elif not all(self.fresh(k) for k in (1,3)):
                reason='Telemetria atrasada'
            else:
                a,b=self.fields(1),self.fields(3)
                if not math.isfinite(a.get('battery',math.nan)) or a['battery']<3.5:
                    reason='Bateria no valida'
                elif not math.isfinite(a.get('yaw',math.nan)):
                    reason='IMU no valida'
                elif b.get('armed')!=1 or b.get('mode')!=8:
                    reason='Desarmado/cambio de modo'
                elif any(not math.isfinite(b.get(k,math.nan)) or not 0<=b[k]<=.1501
                         for k in ('motor1','motor2')):
                    reason='Potencia fuera de limite'
            if reason:
                self.reason=reason
                self.abort.set()
                return

    async def run(self):
        reader=asyncio.create_task(self.read())
        guard=asyncio.create_task(self.guard())
        try:
            await self.wait(lambda: self.fresh(1) and self.fresh(3))
            a,b=self.fields(1),self.fields(3)
            if b.get('armed')!=0 or b.get('mode')!=0:
                raise TrialError('Pulsa STOP primero; no tomar otra sesion')
            if not math.isfinite(a.get('yaw',math.nan)) or not math.isfinite(a.get('battery',math.nan)) or a['battery']<3.5:
                raise TrialError('Bateria o IMU no validas')
            self.owned=True
            self.stage='posicionar desarmado'
            await self.send(packet(0,-1))
            await self.wait(lambda: self.fresh(3) and self.fields(3).get('mode')==8 and
                            self.fields(3).get('armed')==0)
            await self.say('Prueba de direccion del empuje, cinco segundos. Mantengalo sujeto. Al soltar observe si avanza, queda quieto, retrocede o se desplaza de lado. Espere la cuenta.')
            self.stage='armar a cero'
            await self.send(packet(0,1))
            await self.wait(lambda: self.fresh(3) and self.fields(3).get('armed')==1 and
                max(abs(self.fields(3)['motor1']),abs(self.fields(3)['motor2']))<.001,10)
            await self.say('Preparando sustentacion. Mantengalo sujeto.')
            self.stage='preparar neutral'
            self.power_started=time.monotonic()
            self.guard_enabled=True
            old=self.rx_counts.get(3,0)
            await self.send(packet(.15))
            await self.wait(lambda: self.fresh(3) and self.rx_counts.get(3,0)>old and
                all(abs(self.fields(3)[k]-.15)<.0001 for k in ('motor1','motor2')) and
                abs(self.fields(3)['servo1']-angles(TRIM_DEG)[0])<.1 and
                abs(self.fields(3)['servo2']-angles(TRIM_DEG)[1])<.1,3)
            await self.say('Tres, dos, uno. Suelte ahora.')
            self.stage='referencia libre'
            started=time.monotonic()
            while time.monotonic()-started<5:
                if self.abort.is_set():
                    raise TrialError(self.reason)
                print(json.dumps(dict(t=round(time.monotonic()-started,2),
                    imu=self.fields(1),actuators=self.fields(3)),ensure_ascii=False),flush=True)
                await asyncio.sleep(.25)
            if self.abort.is_set():
                raise TrialError(self.reason)
        finally:
            self.guard_enabled=False
            if self.owned:
                await asyncio.shield(self.stop())
            reader.cancel();guard.cancel()
            await asyncio.gather(reader,guard,return_exceptions=True)


async def main():
    path=ROOT/'logs'/'thrust_direction'
    path.mkdir(parents=True,exist_ok=True)
    file=path/(datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'_trim'+str(TRIM_DEG)+'.jsonl')
    print('LOG:',file,flush=True)
    print('ANGULOS:',angles(TRIM_DEG),'POTENCIA:',.15,'TIEMPO_LIBRE:',5,flush=True)
    async with websockets.connect('ws://127.0.0.1:8765',open_timeout=5) as ws:
        await ws.send(json.dumps(dict(auth=dict(role='dev',token='changeme'))))
        with file.open('x',encoding='utf-8') as log:
            trial=Neutral(ws,5,log)
            try:
                await trial.run()
            except (Exception,asyncio.CancelledError) as error:
                print('DETENIDO:',error,flush=True)
                trial.record('failure',str(error))
            if trial.owned:
                await trial.speaker.say('Prueba finalizada. Motores desarmados.' if trial.stop_confirmed else
                    'Atencion. No se confirmo el desarmado.',asyncio.Event())


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--trim',type=float,default=4,help='Adicion de avance sobre48.5/68.5:0..12grados')
    parser.add_argument('--actuate',action='store_true')
    args=parser.parse_args()
    try:angles(args.trim)
    except ValueError as error:parser.error(str(error))
    if not args.actuate:parser.error('Para probar motores se requiere --actuate')
    TRIM_DEG=args.trim
    asyncio.run(main())
