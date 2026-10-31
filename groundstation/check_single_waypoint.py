"""Bounded, voiced P08 trial through the existing Ground WebSocket."""
import argparse
import asyncio
import json
import math
from datetime import datetime
from pathlib import Path
import sys

ROOT=Path(r'C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware')
sys.path.insert(0,str(ROOT/'groundstation'))
from check_visual_support import Trial, TrialError, control
from common.height_options import pack_height_options
from common.control_pack import make_args, build_control_payload, NAME_TO_MODE
import websockets

class WaypointTrial(Trial):
    def __init__(self,*args,battery_cutoff=False,height_offset_cm=0,vertical_only=False,camera_level_pitch_deg=0,require_flow=True,**kwargs):
        super().__init__(*args,**kwargs)
        # Temporary user-selected policy while validating the courses.
        # Battery remains in F1/logs; all other guards stay active.
        self.battery_cutoff=battery_cutoff
        self.height_offset_cm=height_offset_cm
        self.vertical_only=vertical_only
        self.camera_level_pitch_deg=camera_level_pitch_deg
        self.run_released=False
        self.require_flow=True # Flowdeck is mandatory in the flow-only build

    def battery_blocked(self):
        voltage=self.fields(1).get('battery',math.nan)
        return self.battery_cutoff and (not math.isfinite(voltage) or voltage<3.5)

    def flow_ready(self):
        return (self.fresh(18) and self.fields(18).get('flow_height_source')==2 and
                int(self.fields(18).get('flow_height_flags',0))&10==10)

    def completed(self):
        return self.fresh(5) and self.fields(5).get('mission_state')==5 and self.fields(5).get('visited')==1

    def camera_ready(self):
        if not self.fresh(7):return False
        f=self.fields(7)
        return (f.get('nicla_fresh')==1 and f.get('nicla_reset_pending')==0 and
                f.get('nicla_protocol')==0x4e03)

    async def wait_flow_after_arm(self,timeout=5):
        # ESC arming pauses the onboard loop for ~3.7 s. That expires the
        # old range window. Keep zero throttle and require a NEW qualified
        # Flowdeck report after arming before entering automatic control.
        before=self.rx_counts.get(18,0)
        await self.wait(lambda:self.rx_counts.get(18,0)>before and self.flow_ready(),timeout)
        self.record('flow_requalified_after_arm',self.fields(18))

    async def prepare_acquisition(self,zero):
        # Live camera/ACK is required; a balloon in the image is not. Initial
        # SEARCH belongs to the onboard mission and stays bounded onboard.
        before=self.rx_counts.get(7,0)
        await self.send(dict(cmd='vision_bench_start'))
        await self.wait(lambda:self.fresh(3) and self.fields(3).get('mode')==22 and
            self.fields(3).get('armed')==0)
        await self.wait(lambda:self.camera_ready() and self.rx_counts.get(7,0)>before,6)
        await self.send(zero)
        await self.wait(lambda:self.fresh(3) and self.fields(3).get('mode')==8 and
            self.fields(3).get('armed')==0)
        await self.send(dict(zero,arm=1))
        await self.wait(lambda:self.fresh(3) and self.fields(3).get('armed')==1 and
            self.fields(3).get('mode')==8 and
            max(abs(self.fields(3).get(k,1)) for k in ('motor1','motor2'))<.001,10)
        await self.wait_flow_after_arm()
        if not self.camera_ready():raise TrialError('Camara sin comunicacion reciente')

    async def guard(self):
        disarmed_at=None
        while True:
            await asyncio.sleep(.05)
            if not self.guard_enabled:continue
            if self.completed():return
            reason=''
            if not all(self.fresh(k) for k in (1,3,7,5,11)):
                reason='Telemetria atrasada'
            else:
                a,b=self.fields(1),self.fields(3)
                if self.battery_blocked():
                    reason='Bateria baja'
                elif not math.isfinite(a.get('yaw',math.nan)):
                    reason='Yaw no valido'
                elif any(not math.isfinite(b.get(k,math.nan)) or not 0<=b[k]<=.2501 for k in ('motor1','motor2')):
                    reason='Potencia fuera de limite'
                elif b.get('mode')!=NAME_TO_MODE['p08'] or self.fields(5).get('mission_state')==6:
                    reason='Mision detenida/fallo a bordo'
                elif self.fields(11).get('support_version',0)<39:
                    reason='Firmware no compatible'
                elif (self.fresh(12) and self.fields(12).get('impulse_forward',0)>0 and
                      abs(self.fields(12).get('impulse_yaw',0))>.00001):
                    reason='Impulso y giro simultaneos: cancelar ensayo'
                elif (self.fresh(12) and self.fields(12).get('impulse_forward',0)>0 and
                      (not self.run_released or self.vertical_only or not self.fresh(17) or
                       int(self.fields(17).get('fixed_height_flags',0))&15!=15)):
                    reason='Avance sin liberacion o referencia fija valida'
                elif self.require_flow and (not self.fresh(18) or
                        self.fields(18).get('flow_height_source')!=2 or
                        not int(self.fields(18).get('flow_height_flags',0))&8):
                    reason='Flowdeck sin rango valido; no hay respaldo barometrico'
                elif (self.fresh(12) and self.fields(12).get('impulse_forward',0)>0 and
                      int(self.fields(18).get('flow_height_flags',0))&8192):
                    reason='Avance durante altura conservada'
                elif b.get('armed')!=1:
                    # F3 disarm may precede the F5 DONE packet by one cycle.
                    if disarmed_at is None:disarmed_at=asyncio.get_running_loop().time()
                    if asyncio.get_running_loop().time()-disarmed_at>.3:reason='Desarmado sin llegada'
                else:disarmed_at=None
            if reason:
                self.reason=reason;self.abort.set();return

    def height_command(self,command,*,released,reset):
        aux=dict(command['aux'])
        aux[9]=pack_height_options(released=released,advance=not self.vertical_only,
            offset_cm=self.height_offset_cm,level_pitch_deg=self.camera_level_pitch_deg)
        return dict(command,aux=aux,reset=reset)

    async def release_after_countdown(self,command):
        # The onboard bit blocks impulse AND arrival, even if a target locks
        # while held. Same mode, reset=0: retain reference, learned lift and yaw.
        await self.say('Tres, dos, uno. Suelte ahora.')
        if self.abort.is_set():raise TrialError(self.reason)
        self.run_released=True
        await self.send(self.height_command(command,released=True,reset=0))
        self.record('release',dict(height_offset_cm=self.height_offset_cm,vertical_only=self.vertical_only))

    async def run(self):
        self.record('battery_policy',dict(cutoff_enabled=self.battery_cutoff,
            threshold_v=3.5,telemetry_retained=True,temporary=True))
        print('CORTE_BATERIA:', 'ACTIVO 3.5 V' if self.battery_cutoff else
            'DESACTIVADO TEMPORALMENTE; voltaje conservado en telemetria y log',flush=True)
        reader=asyncio.create_task(self.read());guard=asyncio.create_task(self.guard())
        try:
            await self.wait(lambda: all(self.fresh(k) for k in (1,2,3,7)))
            if self.fields(3).get('armed')!=0 or self.fields(3).get('mode')!=0:
                raise TrialError('Otra prueba activa: no se toma control.')
            if not math.isfinite(self.fields(1).get('yaw',math.nan)):
                raise TrialError('Yaw no valido')
            if self.battery_blocked():
                raise TrialError('Bateria no valida')
            if self.require_flow:
                await self.wait(self.flow_ready,5)
            self.owned=True
            self.stage='compatibilidad desarmada'
            before=self.rx_counts.get(11,0)
            await self.send(control(True,self.fields(1)['yaw'],reset=1,arm=-1))
            await self.wait(lambda:self.fresh(11) and self.rx_counts.get(11,0)>before and self.fields(11).get('support_version',0)>=39,4)
            print('FIRMWARE_CONFIRMADO:',self.fields(11)['support_version'],flush=True)
            zero=dict(cmd='control',mode=8,arm=-1,reset=0,aux={5:52.5,6:64.5,7:0,8:0})
            await self.send(zero)
            await self.wait(lambda:self.fresh(3) and self.fields(3).get('mode')==8 and self.fields(3).get('armed')==0)
            await self.say('Sostenga el blimp. Prueba de un waypoint. No suelte hasta la cuenta.')
            await self.say('Preparando busqueda. Mantengalo sujeto.')
            self.stage='preparar busqueda'
            if self.require_flow and not self.flow_ready():raise TrialError('Flowdeck no lista antes de armar')
            await self.prepare_acquisition(zero)
            if self.require_flow and not self.flow_ready():raise TrialError('Flowdeck perdio referencia durante preparacion')
            if not self.fresh(1):raise TrialError('Telemetria atrasada antes de iniciar')
            if not math.isfinite(self.fields(1).get('yaw',math.nan)):raise TrialError('Yaw no valido')
            if self.battery_blocked():raise TrialError('Bateria baja antes de iniciar')
            fx,fz,tx,tz,aux,_=build_control_payload(make_args('p08',height=self.fields(1).get('height',0),
                alt_max_power=25,vis_min_power=10,vis_max_power=20,vis_deadband_px=24))
            self.stage='P08 busqueda y aproximacion'
            command=dict(cmd='control',mode=NAME_TO_MODE['p08'],arm=0,fx=fx,fz=fz,tx=tx,tz=tz,aux=aux)
            await self.send(self.height_command(command,released=False,reset=1))
            await self.wait(lambda:all(self.fresh(k) for k in (3,5,11)) and self.fields(3).get('mode')==NAME_TO_MODE['p08'] and self.fields(11).get('support_version',0)>=39,2)
            self.guard_enabled=True
            # Zero/low thrust can be the correct request above the target.
            # ESC arming/spool was confirmed in prepare_acquisition; a fixed
            #10% requirement here would reintroduce the rejected lift floor.
            await self.wait(lambda:self.fresh(3) and self.fields(3).get('armed')==1 and
                self.fields(3).get('mode')==NAME_TO_MODE['p08'] and
                all(0<=self.fields(3).get(k,-1)<=.2501 for k in ('motor1','motor2')),3)
            await self.release_after_countdown(command)
            self.record('duration_s',self.seconds)
            end=asyncio.get_running_loop().time()+self.seconds
            announced=False
            while asyncio.get_running_loop().time()<end:
                if self.completed():
                    self.reached=True
                    print('WAYPOINT_COMPLETADO:',json.dumps(self.fields(5)),flush=True)
                    return
                if self.abort.is_set():raise TrialError(self.reason)
                if not announced and self.fresh(17) and int(self.fields(17).get('fixed_height_flags',0))&3==3:
                    announced=True
                    await self.say('Altura fijada. Solo control vertical.' if self.vertical_only else 'Altura fijada. Aproximando.')
                phase=self.fields(12).get('impulse_phase') if self.fresh(12) else None
                inertia_flags=int(self.fields(13).get('inertia_flags',0)) if self.fresh(13) else 0
                names={0:'ALINEAR',1:'ACOMODAR',2:'IMPULSO',3:'RETIRAR_AVANCE',4:'CORREGIR_EN_INERCIA',5:'BUSCAR'}
                print(json.dumps(dict(fase=names.get(phase,'SIN_F12_REINICIAR_GROUND'),
                    x=self.fields(2).get('nicla_x'),y=self.fields(2).get('nicla_y'),
                    ancho=self.fields(2).get('nicla_w'),potencia=self.fields(3).get('motor1'),
                    objetivo_confirmado=self.target(),
                    candidato_sin_confirmar=(self.fields(2).get('nicla_flag')==0 and self.fields(2).get('nicla_w',0)>0),
                    trim=self.fields(10).get('support_trim') if self.fresh(10) else None,
                    potencia_al_limite=max(self.fields(3).get(k,0) for k in ('motor1','motor2'))>=.248,
                    freno_vertical='REDUCIR_POR_OBJETIVO_ABAJO' if inertia_flags&16 else
                        ('REDUCIR_SUBIDA' if inertia_flags&4 else ('RECUPERAR_CAIDA' if inertia_flags&8 else 'REGULAR')),
                    frenar_giro_antes_de_impulsar=bool(inertia_flags&32),
                    avance=self.fields(11).get('support_forward_request'),
                    avance_con_referencia_temporal=bool(inertia_flags&1024),
                    fuente_altura=self.fields(18).get('flow_height_source') if self.fresh(18) else None,
                    altura_flowdeck=self.fields(18).get('flow_height_filtered') if self.fresh(18) else None,
                    vz_flowdeck=self.fields(18).get('flow_height_vz') if self.fresh(18) else None,
                    altura_conservada=bool(int(self.fields(18).get('flow_height_flags',0))&8192) if self.fresh(18) else None,
                    recuperacion_altura=bool(int(self.fields(18).get('flow_height_flags',0))&16384) if self.fresh(18) else None,
                    altura_objetivo_fija=self.fields(17).get('fixed_height_target') if self.fresh(17) else None,
                    altura_estimada=self.fields(17).get('fixed_height_estimate') if self.fresh(17) else None,
                    sesgo_barometro=self.fields(17).get('baro_height_bias') if self.fresh(17) else None,
                    estado_altura=int(self.fields(17).get('fixed_height_flags',0)) if self.fresh(17) else None,
                    esperando_referencia=not (self.fresh(17) and bool(int(self.fields(17).get('fixed_height_flags',0))&1)),
                    referencia_altura_valida=bool(int(self.fields(16).get('height_reference_flags',0))&2) if self.fresh(16) else None,
                    error_altura_relativo=self.fields(16).get('height_anchor_error') if self.fresh(16) else None,
                    deriva_estimada=self.fields(16).get('height_drift_rate') if self.fresh(16) else None,
                    yaw_error=self.fields(8).get('mission_yaw_error_deg') if self.fresh(8) else None,
                    completados=self.fields(5).get('visited'))),flush=True)
                await asyncio.sleep(.5)
            if self.vertical_only:
                if not announced:raise TrialError('No se capturo referencia fija durante el ensayo vertical')
                self.record('vertical_trial_completed',True)
                return
            raise TrialError(f'Plazo de{self.seconds:g}s sin llegada confirmada')
        finally:
            self.guard_enabled=False
            if self.owned:await asyncio.shield(self.stop())
            for task in (reader,guard):task.cancel()
            await asyncio.gather(reader,guard,return_exceptions=True)

async def main(seconds,battery_cutoff=False,**options):
    async with websockets.connect('ws://127.0.0.1:8765',open_timeout=5) as ws:
        await ws.send(json.dumps(dict(auth=dict(role='dev',token='changeme'))))
        folder=ROOT/'logs'/'single_waypoint';folder.mkdir(parents=True,exist_ok=True)
        path=folder/(datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.jsonl')
        print('LOG:',path,flush=True)
        with path.open('x',encoding='utf-8') as log:
            trial=WaypointTrial(ws,seconds,log,forward=.12,battery_cutoff=battery_cutoff,**options)
            try:
                await trial.run()
            except Exception as error:
                reason=trial.reason or str(error)
                print('DETENIDO:',reason,flush=True);trial.record('failure',reason)
            if trial.stop_confirmed:
                await trial.speaker.say('Waypoint completado. Motores desarmados.' if getattr(trial,'reached',False) else
                    ('Prueba vertical terminada. Motores desarmados.' if trial.vertical_only else
                     'Prueba terminada sin llegada confirmada. Motores desarmados.'),asyncio.Event())

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--actuate',action='store_true')
    parser.add_argument('--battery-cutoff',action='store_true',
        help='Reactiva el corte por lectura de bateria inferior a 3.5 V; temporalmente desactivado por defecto')
    parser.add_argument('--seconds',type=float,default=120,help='Duracion tras la cuenta:5..120s, defecto120')
    parser.add_argument('--height-offset-cm',type=float,default=0,help='Ajuste respecto a la altura capturada: -50..50 cm, aplicado a 2 cm/s')
    parser.add_argument('--camera-level-pitch-deg',type=float,default=0,help='Pitch de IMU cuando la camara esta horizontal: -30..30 grados')
    parser.add_argument('--vertical-only',action='store_true',help='Regula altura/yaw; bloquea avance y llegada')
    parser.add_argument('--require-flow',action='store_true',default=True,help='Siempre requerido en v39: altura exclusivamente Flowdeck')
    args=parser.parse_args()
    try:pack_height_options(released=False,offset_cm=args.height_offset_cm,level_pitch_deg=args.camera_level_pitch_deg)
    except ValueError as error:parser.error(str(error))
    if not math.isfinite(args.seconds) or not 5<=args.seconds<=120:parser.error('--seconds debe estar entre5 y120')
    if not args.actuate:parser.error('Se requiere --actuate para activar motores')
    asyncio.run(main(args.seconds,args.battery_cutoff,height_offset_cm=args.height_offset_cm,
        vertical_only=args.vertical_only,camera_level_pitch_deg=args.camera_level_pitch_deg,require_flow=args.require_flow))
