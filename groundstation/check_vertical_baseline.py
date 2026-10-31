"""Read-only BMP390 comparison. No commands, resets, arming or motor requests."""
import argparse
import asyncio
from datetime import datetime
import json
import math
from pathlib import Path
import statistics
import time
import websockets
from websockets.exceptions import WebSocketException

ROOT=Path(__file__).resolve().parent.parent

async def capture(seconds,stationary):
    folder=ROOT/'logs'/'vertical_baseline';folder.mkdir(parents=True,exist_ok=True)
    path=folder/(datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.jsonl')
    print('LOG:',path,flush=True)
    latest={};seen={};samples=[];periods=[];windows=[]
    with path.open('x',encoding='utf-8') as log:
        log.write(json.dumps(dict(kind='conditions',stationary_declared=stationary,seconds=seconds))+'\n')
        async with websockets.connect('ws://127.0.0.1:8765',open_timeout=4,close_timeout=1) as ws:
            await ws.send(json.dumps(dict(auth=dict(role='viewer'))))
            print('SOLO LECTURA. Esperando perfil vertical28 y estado desarmado.',flush=True)
            started=None;deadline=time.monotonic()+8;lastPrint=-1
            while True:
                now=time.monotonic()
                if started is not None and now-started>=seconds:break
                if started is None and now>=deadline:
                    raise RuntimeError('Faltan F3/F9/F14 recientes. Cargar ESP28 y reiniciar Ground para las etiquetas nuevas.')
                try:value=json.loads(await asyncio.wait_for(ws.recv(),.5))
                except asyncio.TimeoutError:
                    if started is not None and any(now-seen.get(k,0)>1.5 for k in (3,9,14)):
                        raise RuntimeError('Telemetria interrumpida; captura no valida')
                    continue
                now=time.monotonic()
                log.write(json.dumps(dict(kind='rx',monotonic=now,value=value))+'\n');log.flush()
                flag=value.get('flag');fields=value.get('fields',{})
                if not fields:continue
                latest[flag]=fields;seen[flag]=now
                if flag==3 and (fields.get('armed')!=0 or fields.get('mode')!=0):
                    raise RuntimeError('Requiere modo0 y desarmado; no se modifica el estado del vehiculo.')
                ready=(all(now-seen.get(k,0)<1 for k in (3,9,14)) and
                       latest[3].get('armed')==0 and latest[3].get('mode')==0 and
                       latest[9].get('baro_healthy')==1 and
                       latest[14].get('vertical_profile_version')==28)
                if started is None:
                    if not ready:continue
                    started=now;print(f'CAPTURA INICIADA: {seconds:g}s. Mantener inmovil si se declaro estatico.',flush=True)
                if any(now-seen.get(k,0)>1.5 for k in (3,9,14)):
                    raise RuntimeError('Datos atrasados; no evaluar esta captura')
                if flag==9:
                    vals=[fields.get(k,math.nan) for k in ('baro_raw_height','baro_filtered_height','baro_filtered_vz')]
                    if fields.get('baro_healthy')!=1 or not all(math.isfinite(x) for x in vals):
                        raise RuntimeError('Barometro sin muestra valida')
                    samples.append((now-started,*vals))
                if flag==14:
                    if fields.get('vertical_profile_version')!=28:raise RuntimeError('Perfil vertical incompatible')
                    periods.append(fields['baro_period_ms']);windows.append(fields['baro_window_ms'])
                elapsed=int(now-started)
                if elapsed//5!=lastPrint:lastPrint=elapsed//5;print(f'{elapsed}s / {seconds:g}s',flush=True)
        if len(samples)<10 or not periods:raise RuntimeError('Captura insuficiente')
        raw=[x[1] for x in samples];height=[x[2] for x in samples];vz=[x[3] for x in samples]
        first=[x[2] for x in samples if x[0]<5];last=[x[2] for x in samples if x[0]>seconds-5]
        result=dict(log=str(path),stationary_declared=stationary,samples=len(samples),
            raw_height_range_m=max(raw)-min(raw),filtered_height_range_m=max(height)-min(height),
            filtered_height_std_m=statistics.pstdev(height),velocity_std_m_s=statistics.pstdev(vz),
            apparent_velocity_range_m_s=[min(vz),max(vz)],
            median_height_shift_m=statistics.median(last)-statistics.median(first),
            median_sample_period_ms=statistics.median(periods),max_window_ms=max(windows),
            note='No mide altura fisica ni separa deriva de movimiento; no estima escala con una captura estatica.')
        log.write(json.dumps(dict(kind='summary',value=result))+'\n')
        print(json.dumps(result,indent=2,ensure_ascii=False),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--seconds',type=float,default=30)
    p.add_argument('--stationary',action='store_true',help='Operador declara frame inmovil; no lo detecta el programa')
    args=p.parse_args()
    if not math.isfinite(args.seconds) or not 10<=args.seconds<=120:p.error('Duracion10..120s')
    try:asyncio.run(capture(args.seconds,args.stationary))
    except (OSError,RuntimeError,WebSocketException) as error:raise SystemExit(str(error))
