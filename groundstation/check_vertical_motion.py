"""Voiced manual height test; viewer authentication only, no hardware commands."""
import argparse,asyncio,json,math,statistics,sys,time
from datetime import datetime
from pathlib import Path
import websockets

ROOT=Path(r'C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware')
sys.path.insert(0,str(ROOT/'groundstation'))
from check_visual_support import Speaker

async def main(height_cm=66):
    folder=ROOT/'logs'/'vertical_motion';folder.mkdir(parents=True,exist_ok=True)
    path=folder/(datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.jsonl')
    latest={};seen={};samples=[];evidence=[];phases={};abort=asyncio.Event();error=[];phase='preflight'
    speaker=Speaker()
    with path.open('x',encoding='utf-8') as log:
        def record(v):log.write(json.dumps(v,ensure_ascii=False)+'\n');log.flush()
        print('LOG:',path,flush=True)
        record(dict(kind='conditions',manual_expected_rise_m=height_cm/100.,actuation=False))
        async with websockets.connect('ws://127.0.0.1:8765',open_timeout=4,close_timeout=1) as ws:
            await ws.send(json.dumps(dict(auth=dict(role='viewer'))))
            async def read():
                try:
                    while True:
                        v=json.loads(await asyncio.wait_for(ws.recv(),1.5));now=time.monotonic()
                        record(dict(kind='rx',monotonic=now,phase=phase,value=v))
                        f=v.get('flag');d=v.get('fields',{})
                        if not d:continue
                        latest[f]=d;seen[f]=now
                        if f==3 and (d.get('mode')!=0 or d.get('armed')!=0):
                            raise RuntimeError('Requiere modo0/desarmado. No se modifica el estado.')
                        if f==9:
                            if d.get('baro_healthy')!=1 or not all(math.isfinite(d.get(k,math.nan)) for k in ('baro_filtered_height','baro_filtered_vz')):
                                raise RuntimeError('Barometro sin muestra valida')
                            samples.append((now,phase,d))
                        if f==15 and d.get('baro_motion_version')==30:
                            evidence.append((now,phase,d))
                        if phase!='preflight' and any(now-seen.get(k,0)>1.5 for k in (3,9,14)):
                            raise RuntimeError('Telemetria atrasada')
                except Exception as e:error.append(str(e) or type(e).__name__);abort.set()
            task=asyncio.create_task(read())
            try:
                until=time.monotonic()+8
                while not all(time.monotonic()-seen.get(k,0)<1 for k in (3,9,14)):
                    if abort.is_set() or time.monotonic()>until:raise RuntimeError(error or 'Falta telemetria')
                    await asyncio.sleep(.05)
                if latest[14].get('vertical_profile_version')!=28:raise RuntimeError('Perfil de adquisicion incompatible')
                for name,voice,seconds in (
                    ('zero','Mantén la posición inicial. Diez segundos.',10),
                    ('up',f'Sube {height_cm:g} centímetros en cinco segundos.',5),
                    ('high','Mantén esa altura. Diez segundos.',10),
                    ('down','Baja a la posición inicial en cinco segundos.',5),
                    ('return','Mantén la posición inicial. Diez segundos.',10)):
                    phase=name;phases[name]=time.monotonic()
                    record(dict(kind='phase',name=name,monotonic=phases[name],hold_seconds_after_voice=seconds))
                    await speaker.say(voice,abort)
                    try:await asyncio.wait_for(abort.wait(),seconds)
                    except asyncio.TimeoutError:pass
                    if abort.is_set():raise RuntimeError(error)
                await speaker.say('Prueba terminada. Sin motores.',abort)
            finally:
                task.cancel();await asyncio.gather(task,return_exceptions=True)
        def plateau(name):
            rows=[r for r in samples if r[1]==name]
            if len(rows)<10:raise RuntimeError('Meseta insuficiente: '+name)
            tail=[r[2]['baro_filtered_height'] for r in rows if r[0]>=rows[-1][0]-4]
            return dict(median_m=statistics.median(tail),range_m=max(tail)-min(tail),samples=len(tail))
        levels={name:plateau(name) for name in ('zero','high','return')}
        result=dict(log=str(path),expected_rise_m=height_cm/100.,plateaus=levels,
            measured_rise_m=levels['high']['median_m']-levels['zero']['median_m'],
            return_offset_m=levels['return']['median_m']-levels['zero']['median_m'],
            note='Physical positions operator-controlled, not independently measured. No automatic scale fit. No motors.')
        result['onboard_motion_v30']={}
        for name in ('zero','up','high','down','return'):
            rows=[r for r in evidence if r[1]==name]
            if not rows:continue
            accepted=[r for r in rows if int(r[2]['baro_motion_flags'])&3]
            tail=[r for r in rows if r[0]>=rows[-1][0]-4]
            prior=[r for r in evidence if r[0]<rows[0][0]]
            eventStart=prior[-1][2]['baro_motion_events'] if prior else rows[0][2]['baro_motion_events']
            result['onboard_motion_v30'][name]=dict(samples=len(rows),accepted=len(accepted),
                confirmed_events=int(rows[-1][2]['baro_motion_events']-eventStart),
                accepted_last4s=sum(bool(int(r[2]['baro_motion_flags'])&3) for r in tail),
                first_accepted_after_cue_s=round(accepted[0][0]-phases[name],3) if accepted else None,
                accepted_rate_range_m_s=[min(r[2]['baro_motion_rate'] for r in accepted),max(r[2]['baro_motion_rate'] for r in accepted)] if accepted else None)
        record(dict(kind='summary',value=result));print(json.dumps(result,indent=2,ensure_ascii=False),flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--height-cm',type=float,default=66,help='Subida respecto al punto inicial,1..200cm')
    args=parser.parse_args()
    if not math.isfinite(args.height_cm) or not 1<=args.height_cm<=200:parser.error('Altura1..200cm')
    try:asyncio.run(main(args.height_cm))
    except Exception as e:raise SystemExit('PRUEBA NO VALIDA: '+str(e))
