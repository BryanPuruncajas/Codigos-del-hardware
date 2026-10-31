"""USB Xbox -> existing Ground WebSocket. --monitor never sends controls."""
import argparse
import asyncio
from datetime import datetime
import json
import math
import os
from pathlib import Path
import time

import websockets
from check_single_waypoint import WaypointTrial
from check_visual_support import TrialError, control
from common.control_pack import build_control_payload, make_args, NAME_TO_MODE
from xbox_input import ButtonMap, XInput, GamepadDisconnected, button_names, MOVEMENT

ROOT=Path(__file__).resolve().parent.parent
TOKEN=22594
COURSES={'waypoint':'p08','track1':'p11','track2':'p11t2'}

class XboxPilot(WaypointTrial):
    def __init__(self,ws,pad,options,log):
        super().__init__(ws,options.seconds,log,forward=.12)
        self.pad,self.options=pad,options
        self.mapping=ButtonMap(options.power)
        self.buttons=0;self.events=asyncio.Queue()
        self.phase='INICIO';self.mode=NAME_TO_MODE[COURSES[options.course]]
        self.auto_payload=None;self.auto_started=False
        self.lease=False;self.closing=False;self.ack_counts={};self.last_ack={}
        self.heartbeat_payload=None;self.operation=None;self.rb_id=0
        self.manual_wait_release=False

    async def send(self,command):
        command=dict(command)
        if command.get('cmd')=='control':
            command['reload']=TOKEN
            # Repeated packets are a deadman heartbeat, never another ARM,
            # reset, resume or manual waypoint acknowledgement.
            self.heartbeat_payload=dict(command,arm=0,reset=0)
        await super().send(command)

    async def read(self):
        try:
            async for raw in self.ws:
                message=json.loads(raw);self.record('rx',message)
                if 'flag' in message and isinstance(message.get('fields'),dict):
                    flag=int(message['flag']);self.latest[flag]=(time.monotonic(),message['fields'])
                    self.rx_counts[flag]=self.rx_counts.get(flag,0)+1
                if 'cmd_ok' in message:
                    cmd=message.get('cmd');self.ack_counts[cmd]=self.ack_counts.get(cmd,0)+1
                    self.last_ack[cmd]=message
                    if message['cmd_ok'] is False and not self.closing:
                        self.reason=str(message.get('error','Ground rechazo el mando'));self.abort.set()
        except asyncio.CancelledError:raise
        except Exception as error:
            self.reason='Ground: '+str(error);self.abort.set()
        finally:
            if not self.closing:self.reason=self.reason or 'Ground desconectado';self.abort.set()

    async def request(self,command):
        cmd=command['cmd'];before=self.ack_counts.get(cmd,0)
        await self.send(command)
        await self.wait(lambda:self.ack_counts.get(cmd,0)>before,2)
        return self.last_ack[cmd]

    async def poll(self):
        try:
            # Starting with held buttons cannot trigger an edge action.
            self.buttons=self.pad.read();self.mapping.previous=self.buttons
            while not self.abort.is_set():
                self.buttons=self.pad.read()
                for event in self.mapping.update(self.buttons):
                    if event=='stop':
                        self.reason='STOP del mando';self.abort.set();return
                    self.events.put_nowait(event)
                await asyncio.sleep(.02)
        except GamepadDisconnected as error:
            self.reason=str(error);self.abort.set()
        except Exception as error:
            self.reason='Lectura Xbox: '+str(error);self.abort.set()

    async def heartbeat(self):
        while not self.abort.is_set():
            if self.lease and not self.closing:
                await self.send(dict(cmd='pilot_ping'))
                if self.heartbeat_payload is not None:
                    await self.send(self.heartbeat_payload)
            await asyncio.sleep(.1)

    async def cancel_operation(self):
        if self.operation is not None:
            self.operation.cancel();await asyncio.gather(self.operation,return_exceptions=True)
            self.operation=None

    async def select_manual(self):
        self.phase='CAMBIO_MANUAL';self.stage=self.phase
        await self.send(self.mapping.manual(0))
        await self.wait(lambda:self.fresh(3) and self.fields(3).get('mode')==11,2)
        self.phase='MANUAL';self.stage=self.phase
        print(f'MANUAL | potencia {self.mapping.power}% | A subir, cruceta mover, RB confirmar waypoint',flush=True)
        if self.fields(3).get('armed')!=1:
            print('DESARMADO: pulse Y para entrar y armar manual, sin iniciar automatico.',flush=True)

    async def arm_manual(self):
        if self.fresh(3) and self.fields(3).get('armed')==1:return
        self.phase='ARMANDO_MANUAL';self.stage=self.phase
        await self.say('Armando manual. Mantenga el blimp sujeto.')
        zero=dict(cmd='control',mode=8,arm=0,reset=0,aux={5:52.5,6:64.5,7:0,8:0})
        await self.send(zero)
        await self.wait(lambda:self.fresh(3) and self.fields(3).get('mode')==8,2)
        await self.send(dict(zero,arm=1))
        await self.wait(lambda:self.fresh(3) and self.fields(3).get('armed')==1,10)
        await self.select_manual()
        await self.say('Manual listo.')

    async def take_manual(self):
        # Only an explicit Y press requests arming. Automatic fallbacks use
        # select_manual(), which never silently rearms after a fault.
        self.manual_wait_release=True
        await self.select_manual()
        if self.fields(3).get('armed')!=1:
            await self.arm_manual()
        print('MANUAL LISTO | suelte A/cruceta antes de dar una orden nueva.',flush=True)

    async def start_auto(self):
        self.phase='PREPARANDO_AUTO';self.stage=self.phase
        await self.wait(lambda:all(self.fresh(k) for k in (1,3,7,18)),6)
        if self.fields(3).get('armed')!=0:
            raise TrialError('Inicio automatico requiere desarmado; use Y para retomar una mision pausada')
        if not self.flow_ready():
            print('ESPERANDO FLOWDECK: altura reciente y valida. Y permite tomar manual.',flush=True)
        while not self.flow_ready():
            if self.abort.is_set():raise TrialError(self.reason or 'Inicio cancelado')
            if not all(self.fresh(k) for k in (1,3,7,18)):
                raise TrialError('Telemetria atrasada mientras se espera Flowdeck')
            if self.fields(3).get('armed')!=0:
                raise TrialError('La preparacion automatica debe permanecer desarmada')
            await asyncio.sleep(.05)
        await self.send(control(True,self.fields(1)['yaw'],reset=1,arm=-1))
        await self.wait(lambda:self.fresh(11),3)
        if self.fields(11).get('support_version',0)<42:
            raise TrialError('Xbox requiere firmware 42; no se armaron motores')
        course_name={'waypoint':'un waypoint','track1':'pista uno, rectangulo','track2':'pista dos, diagonales'}[self.options.course]
        await self.say('Prueba automatica de '+course_name+'. Comience apuntando de uno hacia dos. Mantenga sujeto hasta la cuenta. Y permite tomar control manual.')
        zero=dict(cmd='control',mode=8,arm=-1,reset=0,aux={5:52.5,6:64.5,7:0,8:0})
        await self.prepare_acquisition(zero)
        fx,fz,tx,tz,aux,_=build_control_payload(make_args(COURSES[self.options.course],
            height=self.fields(1).get('height',0),alt_max_power=25,
            vis_min_power=10,vis_max_power=20,vis_deadband_px=24))
        command=dict(cmd='control',mode=self.mode,arm=0,fx=fx,fz=fz,tx=tx,tz=tz,aux=aux)
        self.auto_payload=self.height_command(command,released=True,reset=0)
        await self.send(self.height_command(command,released=False,reset=1))
        await self.wait(lambda:self.fresh(3) and self.fields(3).get('mode')==self.mode,2)
        self.auto_started=True
        await self.say('Tres, dos, uno. Suelte ahora.')
        await self.send(self.auto_payload)
        self.phase='AUTO';self.stage=self.phase
        print('AUTOMATICO | Y tomar manual | B/X ajustan solo la potencia manual',flush=True)

    async def resume_auto(self):
        if not self.auto_started or self.auto_payload is None:
            raise TrialError('No hay mision pausada; Menu inicia automatico estando desarmado')
        if not self.fresh(3) or self.fields(3).get('armed')!=1:
            raise TrialError('Manual desarmado: Y arma manual antes de retomar con Menu')
        if not self.flow_ready() or not self.camera_ready():
            raise TrialError('Para retomar automatico se requiere Flowdeck y Nicla recientes')
        if self.fresh(5) and self.fields(5).get('visited',0)>=self.fields(5).get('target_count',1):
            raise TrialError('Recorrido completado; continua manual o pulsa View STOP')
        self.phase='RETOMANDO_AUTO';self.stage=self.phase
        await self.send(dict(self.auto_payload,reset=2))
        await self.wait(lambda:self.fresh(3) and self.fields(3).get('mode')==self.mode and self.fields(3).get('armed')==1,2)
        self.phase='AUTO';self.stage=self.phase
        print('AUTOMATICO: retomando waypoint pendiente, sin borrar progreso.',flush=True)

    async def mark_waypoint(self):
        if self.phase!='MANUAL' or not self.auto_started or not self.fresh(5):
            print('RB: requiere manual (Y) y una mision pausada.',flush=True);return
        before=int(self.fields(5).get('visited',0))
        if before>=self.fields(5).get('target_count',1):
            print('RB: todos los waypoints ya estan confirmados.',flush=True);return
        self.rb_id=self.rb_id%65535+1
        packet=self.mapping.manual(0);packet['reset']=3;packet['aux'][6]=self.rb_id
        # A retransmission of this acknowledgement is idempotent onboard.
        for _ in range(3):
            await self.send(packet)
            try:
                await self.wait(lambda:self.fresh(5) and self.fields(5).get('visited',0)>before,.35)
                print(f'WAYPOINT CONFIRMADO POR OPERADOR: {before+1} / {int(self.fields(5)["target_count"])}',flush=True)
                self.record('manual_waypoint_confirmed',dict(visited=before+1,event_id=self.rb_id))
                return
            except TrialError:
                if self.abort.is_set():raise
        raise TrialError('RB sin confirmacion F5; no se supone completado')

    async def run(self):
        self.pad.read() # no lease or arm without a connected controller
        reader=asyncio.create_task(self.read());poll=asyncio.create_task(self.poll())
        heartbeat=asyncio.create_task(self.heartbeat())
        started=time.monotonic();last_manual=0.;last_status=0.
        try:
            await self.wait(lambda:self.fresh(3),6)
            reply=await self.request(dict(cmd='pilot_claim',takeover=True))
            if reply.get('pilot_version')!=1:raise TrialError('Ground no tiene soporte Xbox actualizado')
            self.lease=self.owned=True
            self.operation=asyncio.create_task(self.start_auto() if self.options.start=='auto' else self.select_manual())
            print('SESION SIN LIMITE: View/Back STOP; Y manual y armado; Menu automatico.' if self.seconds==0 else
                  f'SESION {self.seconds:g}s: View/Back STOP; Y manual y armado; Menu automatico.',flush=True)
            while not self.abort.is_set() and (self.seconds==0 or time.monotonic()-started<self.seconds):
                if self.operation is not None and self.operation.done():
                    task=self.operation;self.operation=None
                    error=None if task.cancelled() else task.exception()
                    if error:
                        print('OPERACION NO COMPLETADA:',error,flush=True)
                        self.record('operation_failed',str(error))
                        # Always neutralize the old command. Keep the gamepad
                        # available for manual recovery; never silently rearm.
                        await self.send(self.mapping.manual(0))
                        self.phase='MANUAL';self.stage=self.phase
                try:event=await asyncio.wait_for(self.events.get(),.02)
                except asyncio.TimeoutError:event=None
                if event=='power':print(f'POTENCIA MANUAL: {self.mapping.power}%',flush=True)
                elif event=='toggle':
                    await self.cancel_operation()
                    self.operation=asyncio.create_task(self.take_manual())
                elif event=='waypoint' and self.operation is None:
                    self.operation=asyncio.create_task(self.mark_waypoint())
                elif event=='start' and self.operation is None:
                    if self.phase=='MANUAL':
                        self.operation=asyncio.create_task(self.resume_auto() if self.auto_started else self.start_auto())
                preparing=self.phase in ('PREPARANDO_AUTO','ARMANDO_MANUAL','CAMBIO_MANUAL','RETOMANDO_AUTO')
                # The firmware's ESC arming pauses telemetry ~3.7 s. The
                # bounded preparation operation verifies a new F3 afterwards.
                if not preparing and not self.fresh(3):raise TrialError('F3 atrasada')
                if not preparing and not self.fresh(1):raise TrialError('Telemetria IMU atrasada')
                now=time.monotonic()
                if self.phase=='MANUAL' and self.operation is None and now-last_manual>=.08:
                    if not self.buttons&MOVEMENT:self.manual_wait_release=False
                    await self.send(self.mapping.manual(0 if self.manual_wait_release else self.buttons));last_manual=now
                if self.phase=='AUTO':
                    if self.fields(3).get('armed')!=1:
                        if self.fresh(5) and self.fields(5).get('mission_state')==5:
                            print('RECORRIDO COMPLETADO',flush=True);return
                        await self.select_manual()
                    elif not self.fresh(18) or not int(self.fields(18).get('flow_height_flags',0))&8:
                        await self.select_manual()
                if now-last_status>=1:
                    last_status=now
                    progress=self.fields(5) if self.fresh(5) else {}
                    print(f'{self.phase} | manual={self.mapping.power}% | armado={int(self.fields(3).get("armed",0))} | completados={progress.get("visited","?")}/{progress.get("target_count","?")}',flush=True)
            if self.abort.is_set():raise TrialError(self.reason or 'Mando detenido')
            print('Tiempo de sesion agotado.',flush=True)
        finally:
            self.closing=True;await self.cancel_operation()
            if self.owned:await asyncio.shield(self.stop())
            for task in (heartbeat,poll,reader):task.cancel()
            await asyncio.gather(heartbeat,poll,reader,return_exceptions=True)

async def main(options):
    pad=XInput(options.controller)
    if options.monitor:
        mapping=ButtonMap(options.power);previous=None
        print('SOLO MANDO: no conecta a Ground ni activa motores. Ctrl+C termina.',flush=True)
        end=time.monotonic()+options.seconds
        while options.seconds==0 or time.monotonic()<end:
            buttons=pad.read();events=mapping.update(buttons)
            if buttons!=previous:
                print(f'{button_names(buttons)} | potencia={mapping.power}% | eventos={events}',flush=True)
                previous=buttons
            await asyncio.sleep(.02)
        return
    folder=ROOT/'logs'/'xbox';folder.mkdir(parents=True,exist_ok=True)
    path=folder/(datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.jsonl')
    print('LOG:',path,flush=True)
    async with websockets.connect(options.url,open_timeout=5,close_timeout=1) as ws:
        await ws.send(json.dumps({'auth':{'role':'dev','token':options.token}}))
        with path.open('x',encoding='utf-8') as log:
            pilot=XboxPilot(ws,pad,options,log)
            try:await pilot.run()
            except TrialError as error:print('DETENIDO:',error,flush=True)

def arguments(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--monitor',action='store_true')
    parser.add_argument('--actuate',action='store_true')
    parser.add_argument('--course',choices=COURSES,default='track1')
    parser.add_argument('--start',choices=('manual','auto'),default='manual',
                        help='Inicio manual sin armar (defecto), o preparar automatico')
    parser.add_argument('--seconds',type=float,default=0,
                        help='0: sin limite de sesion (defecto); o 5..3600 segundos')
    parser.add_argument('--power',type=int,choices=range(0,51,5),default=30,metavar='0..50')
    parser.add_argument('--controller',type=int,choices=range(4))
    parser.add_argument('--url',default='ws://127.0.0.1:8765')
    parser.add_argument('--token',default=os.environ.get('BLIMP_DEV_TOKEN','changeme'))
    options=parser.parse_args(argv)
    if not math.isfinite(options.seconds) or not (options.seconds==0 or 5<=options.seconds<=3600):
        parser.error('--seconds: 0 sin limite, o 5..3600')
    if not options.monitor and not options.actuate:parser.error('Usa --monitor para botones o --actuate para operar')
    return options

if __name__=='__main__':
    try:asyncio.run(main(arguments()))
    except (KeyboardInterrupt,GamepadDisconnected,OSError) as error:print('DETENIDO:',error,flush=True)
