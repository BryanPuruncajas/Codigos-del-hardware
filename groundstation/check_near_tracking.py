"""Voiced, viewer-only FULL -> cropped -> missing -> cropped recovery test."""
import argparse,asyncio,json,math,sys,time
from datetime import datetime
from pathlib import Path
import websockets
from check_visual_support import Speaker

ROOT=Path(__file__).resolve().parent.parent
STATES={0:'SEARCH',1:'CONFIRM',2:'FULL',3:'PARTIAL',4:'MISSING',5:'AMBIG',6:'LOST'}

async def prepare_tracker(record):
    """Only disarmed P12 reset + SAFE_STOP; never ARM or motor commands."""
    fields={};seen={};requested=False
    async with websockets.connect('ws://127.0.0.1:8765',open_timeout=4,close_timeout=1) as ws:
        await ws.send(json.dumps(dict(auth=dict(role='dev',token='changeme'))))
        async def until(predicate,seconds=6):
            end=time.monotonic()+seconds
            while time.monotonic()<end:
                if predicate():return
                try:v=json.loads(await asyncio.wait_for(ws.recv(),.5))
                except asyncio.TimeoutError:continue
                record(dict(kind='preparation_rx',monotonic=time.monotonic(),value=v))
                if v.get('cmd_ok') is False or v.get('auth_ok') is False:raise RuntimeError(v)
                if v.get('fields'):fields[v['flag']]=v['fields'];seen[v['flag']]=time.monotonic()
            raise RuntimeError('Preparacion sin confirmacion reciente')
        await until(lambda:all(time.monotonic()-seen.get(k,0)<1 for k in (3,7)))
        if fields[3].get('armed')!=0 or fields[3].get('mode')!=0:
            raise RuntimeError('Preparacion requiere modo0/desarmado')
        if (fields[7].get('nicla_fresh')==1 and fields[7].get('nicla_reset_pending')==0 and
                fields[7].get('nicla_state') in (2,3)):
            record(dict(kind='preparation_skip_reset',reason='Tracker already has a current confirmed observation'))
            print('TRACKER_ACTIVO; sin reinicio ni cambio de modo.',flush=True)
            return
        before=fields[7].get('nicla_reset_ack')
        try:
            requested=True;sent=time.monotonic()
            record(dict(kind='preparation_tx',value=dict(cmd='vision_bench_start')))
            await ws.send(json.dumps(dict(cmd='vision_bench_start')))
            await until(lambda:seen.get(3,0)>sent and seen.get(7,0)>sent and
                fields[3].get('mode')==22 and fields[3].get('armed')==0 and
                (fields[7].get('nicla_reset_ack')!=before or fields[7].get('nicla_state') in (2,3)) and
                fields[7].get('nicla_reset_pending')==0)
        finally:
            if requested:
                sent=time.monotonic();record(dict(kind='preparation_tx',value=dict(cmd='stop')))
                await ws.send(json.dumps(dict(cmd='stop')))
                await until(lambda:seen.get(3,0)>sent and fields[3].get('mode')==0 and fields[3].get('armed')==0)
        print('PREPARACION_CONFIRMADA; SAFE_STOP. Reset ACK cambiado:',fields[7].get('nicla_reset_ack')!=before,flush=True)

async def main(reset_tracker=False):
    folder=ROOT/'logs'/'near_tracking';folder.mkdir(parents=True,exist_ok=True)
    path=folder/(datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.jsonl')
    latest={};seen={};history=[];phase='preflight';abort=asyncio.Event();errors=[]
    speaker=Speaker();firstRecovery=None;missingAt=None;seedEpoch=None;cropEpoch=None
    fullProof=None;cropProof=None
    with path.open('x',encoding='utf-8') as log:
        def record(v):log.write(json.dumps(v,ensure_ascii=False)+'\n');log.flush()
        print('LOG:',path,flush=True)
        record(dict(kind='conditions',actuation=False,scope='Nicla tracking and UART only; not live ESP mission control'))
        if reset_tracker:
            try:await prepare_tracker(record)
            except Exception as e:
                record(dict(kind='failure',phase='preparation',error=str(e) or type(e).__name__))
                raise
        async with websockets.connect('ws://127.0.0.1:8765',open_timeout=4,close_timeout=1) as ws:
            await ws.send(json.dumps(dict(auth=dict(role='viewer'))))
            def fresh(flag,age=1):return time.monotonic()-seen.get(flag,0)<age
            def observed(state=None):
                f=latest.get(7,{});o=latest.get(2,{})
                return (fresh(7) and fresh(2) and f.get('nicla_fresh')==1 and
                        f.get('nicla_reset_pending')==0 and f.get('nicla_protocol')==0x4e03 and
                        (state is None or f.get('nicla_state')==state) and
                        o.get('nicla_flag') in (65,66))
            def seed():
                o=latest.get(2,{})
                # F2/F7 are published separately. The confirmed FULL flag in
                # F2 is the camera's proof; do not demand simultaneous F7FULL
                # or a second one-second confirmation on the computer.
                return (observed() and o.get('nicla_flag')==65 and
                        (o.get('nicla_w',0)>=80 or o.get('nicla_h',0)>=64) and
                        30<=o.get('nicla_x',-1)<=210 and 20<=o.get('nicla_y',-1)<=140)
            def crop():
                o=latest.get(2,{})
                return observed() and o.get('nicla_flag')==66 and (o.get('nicla_w',0)>=120 or o.get('nicla_h',0)>=112)
            def has_proof(proof):
                return proof is not None and observed() and proof['epoch']==int(latest[7]['nicla_sample'])>>8
            async def read():
                nonlocal firstRecovery,missingAt,fullProof,cropProof
                try:
                    while True:
                        try:v=json.loads(await asyncio.wait_for(ws.recv(),.5))
                        except asyncio.TimeoutError:
                            if phase!='preflight' and any(not fresh(k,1.5) for k in (2,3,7)):
                                raise RuntimeError('Telemetria interrumpida')
                            continue
                        now=time.monotonic();record(dict(kind='rx',monotonic=now,phase=phase,value=v))
                        f=v.get('flag');d=v.get('fields',{})
                        if not d:continue
                        latest[f]=d;seen[f]=now
                        if f==3 and (d.get('armed')!=0 or d.get('mode')!=0):
                            raise RuntimeError('Requiere modo0/desarmado; el ensayo no modifica el estado')
                        if f==7:
                            state=d.get('nicla_state');history.append((now,phase,state))
                            if phase!='preflight' and (d.get('nicla_fresh')!=1 or d.get('nicla_protocol')!=0x4e03 or d.get('nicla_reset_pending')!=0):
                                raise RuntimeError('Nicla sin enlace reciente/compatible')
                            if phase=='loss' and state in (4,6) and missingAt is None:missingAt=now
                            if missingAt is not None and state in (2,3) and firstRecovery is None:
                                firstRecovery=(now,int(state),int(d.get('nicla_sample',0))>>8)
                        if phase in ('preflight','full') and seed():
                            epoch=int(latest[7]['nicla_sample'])>>8
                            if fullProof is None or fullProof['epoch']!=epoch:
                                fullProof=dict(epoch=epoch,monotonic=now,geometry=dict(latest[2]))
                                record(dict(kind='full_proof',value=fullProof))
                        if phase in ('approach','crop_hold') and crop():
                            epoch=int(latest[7]['nicla_sample'])>>8
                            if cropProof is None or cropProof['epoch']!=epoch:
                                cropProof=dict(epoch=epoch,monotonic=now,geometry=dict(latest[2]))
                                record(dict(kind='crop_proof',value=cropProof))
                except Exception as e:errors.append(str(e) or type(e).__name__);abort.set()
            async def wait_for(predicate,timeout,hold=0):
                until=time.monotonic()+timeout;since=None
                while time.monotonic()<until:
                    if abort.is_set():raise RuntimeError(errors)
                    if predicate():
                        if since is None:since=time.monotonic()
                        if time.monotonic()-since>=hold:return
                    else:since=None
                    await asyncio.sleep(.04)
                raise RuntimeError('Condicion no alcanzada: '+phase)
            async def cue(name,text):
                nonlocal phase
                phase=name;record(dict(kind='phase',name=name,monotonic=time.monotonic()))
                await speaker.say(text,abort)
            async def delay(seconds):
                try:await asyncio.wait_for(abort.wait(),seconds)
                except asyncio.TimeoutError:return
                raise RuntimeError(errors)
            task=asyncio.create_task(read())
            try:
                await wait_for(lambda:all(fresh(k) for k in (2,3,7)),8)
                await cue('full','Muestra el globo completo y centrado, de tamaño mediano. Espera la confirmación.')
                await wait_for(lambda:has_proof(fullProof),35)
                seedEpoch=fullProof['epoch']
                await cue('approach','Globo completo confirmado. Acércate despacio hasta recortarlo. Mantén la cámara hacia el globo.')
                await wait_for(lambda:has_proof(cropProof),30)
                cropEpoch=cropProof['epoch']
                await cue('crop_hold','Recorte confirmado. Mantén esa posición cinco segundos.')
                await delay(5)
                await wait_for(lambda:has_proof(cropProof),8)
                await cue('loss','Desvía la cámara hasta perder el globo. Espera el aviso para volver.')
                await wait_for(lambda:missingAt is not None,12)
                await delay(2.2) # exceed tracker LOST2s; old FULL-only recovery cannot pass
                await cue('recovery','Vuelve al mismo globo, todavía recortado.')
                await wait_for(lambda:firstRecovery is not None,10)
                # The first recovered state must be PARTIAL. FULL first would
                # also pass the old code and cannot validate this correction.
                sameAssociation=seedEpoch==cropEpoch==firstRecovery[2]
                result=dict(log=str(path),actuation=False,
                    first_recovered_state=STATES[firstRecovery[1]],
                    association_epochs=dict(full=seedEpoch,crop=cropEpoch,recovered=firstRecovery[2]),
                    same_association=sameAssociation,
                    loss_to_first_recovery_s=round(firstRecovery[0]-missingAt,3),
                    partial_recovery_without_full=firstRecovery[1]==3 and sameAssociation,
                    note='Tests actual Nicla/UART. Does not validate live ESP31 near control, contact or flight.')
                record(dict(kind='summary',value=result))
                message=('Prueba terminada. Recorte recuperado.' if firstRecovery[1]==3 and sameAssociation else
                         'Prueba terminada. Se confirmó otra asociación. La continuidad queda pendiente.' if not sameAssociation else
                         'Prueba terminada. Volvió completo. La recuperación parcial queda pendiente.')
                await cue('done',message)
                print(json.dumps(result,indent=2,ensure_ascii=False),flush=True)
            except Exception as e:
                record(dict(kind='failure',phase=phase,error=str(e) or type(e).__name__))
                if not abort.is_set():
                    await speaker.say('Prueba detenida. No se alcanzó la condición esperada.',abort)
                raise
            finally:
                task.cancel();await asyncio.gather(task,return_exceptions=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reset-tracker',action='store_true',help='Reinicia solo seguimiento via P12 desarmado y vuelve a modo0 antes del ensayo')
    args=parser.parse_args()
    try:asyncio.run(main(args.reset_tracker))
    except Exception as e:raise SystemExit('PRUEBA NO VALIDADA: '+str(e))
