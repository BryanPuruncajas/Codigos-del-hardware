"""Read-only Flowdeck-only height and vertical velocity through Ground; never sends motor commands."""
import argparse
import asyncio
from datetime import datetime
import json
from pathlib import Path
import time
import websockets

ROOT=Path(__file__).resolve().parent.parent
NAMES={0:'SIN_ALTURA_VALIDA',2:'FLOWDECK'}

async def main(seconds):
    folder=ROOT/'logs'/'flow_height';folder.mkdir(parents=True,exist_ok=True)
    path=folder/(datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.jsonl')
    print('LOG:',path,flush=True)
    print('LECTURA SOLAMENTE: no arma ni envia comandos. Comparar altura fisica con altura_suelo.',flush=True)
    latest={};last_print=0
    async with websockets.connect('ws://127.0.0.1:8765',open_timeout=5,close_timeout=1) as ws:
        await ws.send(json.dumps({'auth':{'role':'viewer'}}))
        end=time.monotonic()+seconds
        with path.open('x',encoding='utf-8') as log:
            while time.monotonic()<end:
                try:raw=await asyncio.wait_for(ws.recv(),min(1,max(.01,end-time.monotonic())))
                except asyncio.TimeoutError:raw=None
                now=time.monotonic()
                if raw is not None:
                    data=json.loads(raw)
                    log.write(json.dumps(dict(kind='rx',monotonic=now,value=data))+'\n');log.flush()
                    if 'flag' in data and 'fields' in data:latest[data['flag']]=(now,data['fields'])
                if now-last_print>=1:
                    last_print=now
                    def fields(flag):
                        entry=latest.get(flag)
                        return entry[1] if entry and now-entry[0]<1.5 else {}
                    v=fields(18);baro=fields(1);motion=fields(19)
                    print(json.dumps(dict(fuente=NAMES.get(v.get('flow_height_source'),'ESPERANDO_F18'),
                        altura_suelo=v.get('flow_height_filtered'),altura_local=v.get('flow_fused_height'),
                        vz_flow=v.get('flow_height_vz'),vz_utilizada=v.get('flow_fused_vz'),
                        altura_publicada=baro.get('height'),vz_publicada=baro.get('vertical_velocity'),
                        flags=v.get('flow_height_flags'),calidad=motion.get('flow_quality_used'),
                        flujo_x=motion.get('flow_rate_x'),flujo_y=motion.get('flow_rate_y'),
                        armado=fields(3).get('armed'))),flush=True)
    print('FIN DE LECTURA:',path,flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds',type=float,default=60)
    args=parser.parse_args()
    if not 5<=args.seconds<=900:parser.error('--seconds: 5..900')
    try:asyncio.run(main(args.seconds))
    except KeyboardInterrupt:print('Lectura terminada.')
