import argparse, sys, time, math
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from user_parameters import SERIAL_PORT, ROBOT_MACS
from common.link import BlimpLink
from common.telemetry import parse, LABELS
from common.csv_logger import CsvLogger
from tests import modes

NAME_TO_MODE={
 'p00m':modes.P00_MOTOR_POWER_TEST,'p00':modes.P00_SERVO_CALIBRATION,'p01':modes.P01_SENSOR_INTEGRATION,'p02':modes.P02_MANUAL_CONTROL,
 'p03':modes.P03_YAW_CONTROL,'p04':modes.P04_ALTITUDE_CONTROL,
 'p05':modes.P05_YAW_ALTITUDE,'p06':modes.P06_NICLA_RAW,
 'p07':modes.P07_VISUAL_CENTER,'p08':modes.P08_ONE_BALLOON,
 'p09':modes.P09_VISIT_ESCAPE,'p10':modes.P10_TWO_BALLOONS,'p11':modes.P11_FOUR_BALLOONS,
}
ACTUATED={'p02','p03','p04','p05','p07','p08','p09','p10','p11'}



def _alt_values(args):
    return (
        args.alt_kp if args.alt_kp is not None else args.kp,
        args.alt_ki if args.alt_ki is not None else args.ki,
        args.alt_kd if args.alt_kd is not None else args.kd,
        args.alt_max_power if args.alt_max_power is not None else args.max_power,
        args.alt_slew if args.alt_slew is not None else args.slew,
    )


def build_control_payload(args):
    """Empaqueta referencias y tuning segun el modo sin cambiar ControlInput."""
    fx=0.0; fz=args.height or 0.0; tx=0.0
    tz=math.radians(args.yaw_deg) if args.yaw_deg is not None else 0.0
    aux={}
    desc=[]
    akp,aki,akd,amax,aslew=_alt_values(args)

    if args.test=='p03':
        aux={5:args.yaw_kp,6:args.yaw_kd,7:args.yaw_min_power/100.0,
             8:args.yaw_max_power/100.0,9:math.radians(args.yaw_deadband_deg)}
        desc.append(f'P03 YAW PD: Kp={args.yaw_kp:.4f} Kd={args.yaw_kd:.4f} '
                    f'Min={args.yaw_min_power:.1f}% Max={args.yaw_max_power:.1f}% '
                    f'DB={args.yaw_deadband_deg:.1f}deg')

    elif args.test=='p04':
        aux={5:akp,6:aki,7:akd,8:amax/100.0,9:aslew}
        desc.append(f'P04 ALT PID: Kp={akp:.4f} Ki={aki:.4f} Kd={akd:.4f} '
                    f'Max={amax:.1f}% Slew={aslew:.3f}/s')

    elif args.test=='p05':
        # P05 v3: FZ=altura, TZ=yaw. El ESP32 corre:
        # ALTITUDE_ACQUIRE -> YAW_ACQUIRE -> HOLD.
        # El control de altura queda activo durante yaw y un unico mixer escribe
        # los actuadores.
        fx=akp; tx=akd
        marker=1<<23

        # AUX1 ALTURA:
        # [0..6] min % | [7..13] max % | [14..17] slew/0.02 |
        # [18..22] success cm | [23] marker
        alt_min_pct=int(round(args.alt_min_power))
        alt_max_pct=int(round(amax))
        slew_units=int(round(aslew/0.02))
        success_cm=int(round(args.alt_success_cm))
        alt_pack=float(marker |
            (alt_min_pct & 0x7F) |
            ((alt_max_pct & 0x7F)<<7) |
            ((slew_units & 0x0F)<<14) |
            ((success_cm & 0x1F)<<18))

        # AUX4 YAW:
        # min/max conservan DECIMAS de porcentaje: 8.5%% -> 85.
        # [0..6] min x10 | [7..13] max x10 | [14..18] success/0.5deg |
        # [19..22] servo authority/10%% | [23] marker
        yaw_min_tenth=int(round(args.yaw_min_power*10.0))
        yaw_max_tenth=int(round(args.yaw_max_power*10.0))
        yaw_success_half=int(round(args.yaw_success_deg*2.0))
        authority_units=int(round(args.yaw_servo_authority/10.0))
        yaw_pack=float(marker |
            (yaw_min_tenth & 0x7F) |
            ((yaw_max_tenth & 0x7F)<<7) |
            ((yaw_success_half & 0x1F)<<14) |
            ((authority_units & 0x0F)<<19))

        aux={5:aki,6:alt_pack,7:args.yaw_kp,8:args.yaw_kd,9:yaw_pack}
        desc.append(f'P05 STATE MACHINE: ALTITUDE_ACQUIRE -> YAW_ACQUIRE -> HOLD')
        desc.append(f'P05 ALT PID: Kp={akp:.4f} Ki={aki:.4f} Kd={akd:.4f} '
                    f'Min={args.alt_min_power:.1f}% Max={amax:.1f}% Slew={aslew:.3f}/s '
                    f'Z-band=+/-{args.alt_success_cm:.0f}cm (2s)')
        desc.append(f'P05 YAW PD: Kp={args.yaw_kp:.4f} Kd={args.yaw_kd:.4f} '
                    f'Min={args.yaw_min_power:.1f}% Max={args.yaw_max_power:.1f}% '
                    f'band=+/-{args.yaw_success_deg:.1f}deg servoAuthority={args.yaw_servo_authority:.0f}%')

    elif args.test in {'p07','p08','p09'}:
        aux={5:args.vis_kp,6:args.vis_kd,7:args.vis_min_power/100.0,
             8:args.vis_max_power/100.0,9:args.vis_deadband_px}
        desc.append(f'VIS PD: Kp={args.vis_kp:.4f} Kd={args.vis_kd:.4f} '
                    f'Min={args.vis_min_power:.1f}% Max={args.vis_max_power:.1f}% '
                    f'DB=+/-{args.vis_deadband_px:.1f}px')

    elif args.test in {'p10','p11'}:
        # P10/P11 necesitan altura + vision simultaneamente.
        # FZ=height; FX=altKp; TX=altKd; TZ=altKi
        # AUX0=altMax; AUX1=altSlew; AUX2=visKp; AUX3=visKd; AUX4=visMax
        fx=akp; tx=akd; tz=aki
        aux={5:amax/100.0,6:aslew,7:args.vis_kp,8:args.vis_kd,
             9:args.vis_max_power/100.0}
        desc.append(f'MISION ALT PID: ref={fz:.2f}m Kp={akp:.4f} Ki={aki:.4f} Kd={akd:.4f} '
                    f'Max={amax:.1f}% Slew={aslew:.3f}/s')
        desc.append(f'MISION VIS PD: Kp={args.vis_kp:.4f} Kd={args.vis_kd:.4f} '
                    f'Max giro={args.vis_max_power:.1f}%')

    return fx,fz,tx,tz,aux,desc


def listen(link,seconds,logger):
    end=time.time()+seconds if seconds else None
    for line in link.lines(seconds):
        t=parse(line)
        if t:
            logger.add(t)
            labels=LABELS.get(t.flag,[f'v{i}' for i in range(6)])
            print(f"F{t.flag} " + ' '.join(f'{n}={v:.3f}' for n,v in zip(labels,t.values)))
        elif line:
            print(line)


def main():
    ap=argparse.ArgumentParser(description='Banco de pruebas del blimp')
    ap.add_argument('test',choices=sorted(NAME_TO_MODE))
    ap.add_argument('--seconds',type=float,default=30)
    ap.add_argument('--yaw-deg',type=float,default=None,help='P03/P05: referencia absoluta en grados')
    ap.add_argument('--height',type=float,default=None,help='P04/P05/P10/P11: referencia de altura del barometro en metros')
    # Altura: se conservan los nombres cortos usados hasta ahora.
    ap.add_argument('--kp',type=float,default=0.30,help='Altura: Kp (alias simple); default 0.30')
    ap.add_argument('--ki',type=float,default=0.025,help='Altura: Ki (alias simple); default 0.025')
    ap.add_argument('--kd',type=float,default=0.18,help='Altura: Kd (alias simple); default 0.18')
    ap.add_argument('--max-power',type=float,default=12.0,help='Altura: potencia maxima en %%; default 12')
    ap.add_argument('--slew',type=float,default=0.18,help='Altura: rampa maxima 0..1/s; default 0.18')
    ap.add_argument('--alt-kp',type=float,default=None,help='Altura: Kp explicito; sobreescribe --kp')
    ap.add_argument('--alt-ki',type=float,default=None,help='Altura: Ki explicito; sobreescribe --ki')
    ap.add_argument('--alt-kd',type=float,default=None,help='Altura: Kd explicito; sobreescribe --kd')
    ap.add_argument('--alt-max-power',type=float,default=None,help='Altura: max %% explicito; sobreescribe --max-power')
    ap.add_argument('--alt-slew',type=float,default=None,help='Altura: slew explicito; sobreescribe --slew')
    ap.add_argument('--alt-min-power',type=float,default=0.0,help='P05: piso de potencia de altura en %; default 0')
    ap.add_argument('--alt-success-cm',type=float,default=10.0,help='P05: banda para declarar Z adquirido; default +/-10 cm')

    # Yaw PD: P03 y P05.
    ap.add_argument('--yaw-kp',type=float,default=0.12,help='Yaw PD: Kp; default 0.12')
    ap.add_argument('--yaw-kd',type=float,default=0.035,help='Yaw PD: Kd; default 0.035')
    ap.add_argument('--yaw-min-power',type=float,default=6.0,help='Yaw PD: potencia minima de giro %% ; default 6')
    ap.add_argument('--yaw-max-power',type=float,default=12.0,help='Yaw PD: potencia maxima de giro %% ; default 12')
    ap.add_argument('--yaw-deadband-deg',type=float,default=5.0,help='P03: zona muerta de yaw en grados; default 5')

    # P05 v3: maquina de estados ALTITUDE_ACQUIRE -> YAW_ACQUIRE -> HOLD.
    ap.add_argument('--yaw-success-deg',type=float,default=10.0,help='P05: yaw se considera adquirido dentro de +/- este valor; default 10 deg')
    ap.add_argument('--yaw-servo-authority',type=float,default=30.0,help='P05: cuanto se alejan los servos del vector Z durante yaw, 0..100%%; default 30')

    # Flags antiguos aceptados por compatibilidad; P05 v3 ya no los usa.
    ap.add_argument('--yaw-enter-deg',type=float,default=None,help=argparse.SUPPRESS)
    ap.add_argument('--yaw-exit-deg',type=float,default=None,help=argparse.SUPPRESS)
    ap.add_argument('--yaw-emergency-deg',type=float,default=None,help=argparse.SUPPRESS)
    ap.add_argument('--yaw-settle-ms',type=float,default=None,help=argparse.SUPPRESS)
    ap.add_argument('--alt-min-dwell-ms',type=float,default=None,help=argparse.SUPPRESS)

    # Vision PD: P07 y misiones P08-P11.
    ap.add_argument('--vis-kp',type=float,default=0.10,help='Vision PD: Kp sobre error normalizado; default 0.10')
    ap.add_argument('--vis-kd',type=float,default=0.015,help='Vision PD: Kd; default 0.015')
    ap.add_argument('--vis-min-power',type=float,default=6.0,help='Vision: potencia minima de giro %% ; default 6')
    ap.add_argument('--vis-max-power',type=float,default=10.0,help='Vision: potencia maxima de giro %% ; default 10')
    ap.add_argument('--vis-deadband-px',type=float,default=15.0,help='Vision: semiancho de zona centrada en px; default 15')

    ap.add_argument('--servo',choices=['1','2','both'],default='1',help='P00: servo a mover; default 1')
    ap.add_argument('--servo-step',type=int,default=2,help='P00: paso en grados (1..10); default 2')
    ap.add_argument('--servo-delay',type=float,default=0.06,help='P00: pausa entre pasos; default 0.06 s')
    ap.add_argument('--servo-angle',type=float,default=None,help='P00: fija SOLO el servo seleccionado; P00M: angulo comun para ambos')
    ap.add_argument('--servo1-angle',type=float,default=None,help='P00M: angulo Servo 1')
    ap.add_argument('--servo2-angle',type=float,default=None,help='P00M: angulo Servo 2')

    ap.add_argument('--motor',choices=['1','2','both'],default=None,help='P00M: brushless a probar')
    ap.add_argument('--power',type=float,default=None,help='P00M: potencia 0..100%%; 100%% = misma escala maxima del firmware viejo')
    ap.add_argument('--motor-seconds',type=float,default=3.0,help='P00M: tiempo con potencia aplicada; max 10 s')
    args=ap.parse_args()

    if not ROBOT_MACS:
        raise SystemExit('Configura ROBOT_MACS en user_parameters.py')

    if args.test in {'p03','p05'} and args.yaw_deg is None:
        raise SystemExit('P03/P05 requieren --yaw-deg.')
    if args.test in {'p04','p05','p10','p11'} and args.height is None:
        raise SystemExit('P04/P05/P10/P11 requieren --height.')
    akp,aki,akd,amax,aslew=_alt_values(args)
    if args.test in {'p04','p05','p10','p11'}:
        if akp < 0.0 or aki < 0.0 or akd < 0.0:
            raise SystemExit('Control de altura requiere Kp, Ki y Kd >= 0.')
        if not (0.1 <= amax <= 100.0):
            raise SystemExit('Potencia maxima de altura debe estar entre 0.1 y 100%.')
        if aslew <= 0.0:
            raise SystemExit('Slew de altura debe ser > 0.')
    if args.test in {'p03','p05'}:
        if args.yaw_kp < 0 or args.yaw_kd < 0:
            raise SystemExit('Yaw requiere --yaw-kp y --yaw-kd >= 0.')
        if not (0 <= args.yaw_min_power <= args.yaw_max_power <= 100):
            raise SystemExit('Yaw requiere 0 <= min-power <= max-power <= 100.')
        if not (0.1 <= args.yaw_deadband_deg <= 90.0):
            raise SystemExit('--yaw-deadband-deg fuera de rango.')

    if args.test=='p05':
        if not (0.0 <= args.alt_min_power <= amax <= 100.0):
            raise SystemExit('P05 requiere 0 <= alt-min-power <= alt-max-power <= 100%.')
        if not (1.0 <= args.alt_success_cm <= 31.0):
            raise SystemExit('P05: --alt-success-cm debe estar entre 1 y 31 cm.')
        if not (0.02 <= aslew <= 0.30):
            raise SystemExit('P05: --slew/--alt-slew debe estar entre 0.02 y 0.30 /s.')
        # Nuevo paquete conserva decimas y admite hasta 12.7% de yaw.
        if not (0.0 <= args.yaw_min_power <= args.yaw_max_power <= 12.7):
            raise SystemExit('P05 requiere 0 <= yaw-min-power <= yaw-max-power <= 12.7%.')
        if not (0.5 <= args.yaw_success_deg <= 15.5):
            raise SystemExit('P05: --yaw-success-deg debe estar entre 0.5 y 15.5 grados.')
        if not (0.0 <= args.yaw_servo_authority <= 100.0):
            raise SystemExit('P05: --yaw-servo-authority debe estar entre 0 y 100%.')
    if args.test in {'p07','p08','p09','p10','p11'}:
        if args.vis_kp < 0 or args.vis_kd < 0:
            raise SystemExit('Vision requiere --vis-kp y --vis-kd >= 0.')
        if not (0 <= args.vis_min_power <= args.vis_max_power <= 100):
            raise SystemExit('Vision requiere 0 <= min-power <= max-power <= 100.')
        if not (1 <= args.vis_deadband_px <= 100):
            raise SystemExit('--vis-deadband-px debe estar entre 1 y 100.')

    if args.test=='p00m':
        if args.motor is None:
            raise SystemExit('P00M requiere --motor 1, --motor 2 o --motor both.')
        if args.power is None or not (0.0 < args.power <= 100.0):
            raise SystemExit('P00M requiere --power entre >0 y 100.')
        if not (0.2 <= args.motor_seconds <= 10.0):
            raise SystemExit('--motor-seconds debe estar entre 0.2 y 10 s.')
        s1=args.servo1_angle if args.servo1_angle is not None else args.servo_angle
        s2=args.servo2_angle if args.servo2_angle is not None else args.servo_angle
        if s1 is None or s2 is None:
            raise SystemExit('P00M requiere --servo-angle, o ambos --servo1-angle y --servo2-angle.')
        if not (0.0 <= s1 <= 120.0 and 0.0 <= s2 <= 120.0):
            raise SystemExit('Los angulos P00M deben estar entre 0 y 120 grados.')

    if args.test=='p00':
        if not (1 <= args.servo_step <= 10):
            raise SystemExit('--servo-step debe estar entre 1 y 10 grados.')
        if args.servo_angle is not None and not (0.0 <= args.servo_angle <= 120.0):
            raise SystemExit('--servo-angle debe estar entre 0 y 120 grados.')

    mac=ROBOT_MACS[0]
    link=BlimpLink(SERIAL_PORT)
    log=CsvLogger(Path(__file__).resolve().parent.parent/'logs')

    try:
        print(link.add_peer(mac))
        print(link.register_ground(mac))
        link.control(mac,mode=0,reload=1)
        mode=NAME_TO_MODE[args.test]

        # ------------------------------------------------------------------
        # P00: SOLO el servo seleccionado. El otro queda detach y no recibe
        # comandos. AUX2 indica selector: 1=S1, 2=S2, 3=ambos.
        # ------------------------------------------------------------------
        if args.test=='p00':
            selection={'1':1,'2':2,'both':3}[args.servo]
            print('\nP00 SERVO CALIBRATION')
            print('Brushless BLOQUEADOS.')
            print(f'Servo seleccionado: {args.servo}. El otro no recibira PWM.')
            print('P0025 nominal: 0..120° = 900..2100 us; 60° = 1500 us.')
            accion='la posicion fija' if args.servo_angle is not None else 'el barrido'
            if input(f'Escribe SERVO para iniciar {accion}: ').strip().upper()!='SERVO':
                raise SystemExit('Cancelado.')

            s1=s2=60.0

            def send_selected(a1,a2):
                return link.control(mac,mode=mode,aux={5:float(a1),6:float(a2),7:float(selection)})

            if args.servo_angle is not None:
                angle=float(args.servo_angle)
                if selection==1:
                    s1=angle
                elif selection==2:
                    s2=angle
                else:
                    s1=s2=angle
                print(f'Comando: S1={s1:.1f}°  S2={s2:.1f}°  selector={selection}')
                print(send_selected(s1,s2))
                print('Solo el servo seleccionado debe moverse.')
                print('Usa --seconds 0 para mantenerlo hasta Ctrl+C.')
                listen(link,args.seconds,log)
            else:
                def sweep_one(which):
                    nonlocal s1,s2
                    print(f'\nServo {which}: 60° -> 0°')
                    for a in range(60,-1,-args.servo_step):
                        if which==1: s1=a
                        else: s2=a
                        send_selected(s1,s2)
                        print(f'  S{which}={a:3d}°',end='\r',flush=True)
                        time.sleep(args.servo_delay)
                    time.sleep(0.5)

                    print(f'\nServo {which}: 0° -> 120°')
                    for a in range(0,121,args.servo_step):
                        if which==1: s1=a
                        else: s2=a
                        send_selected(s1,s2)
                        print(f'  S{which}={a:3d}°',end='\r',flush=True)
                        time.sleep(args.servo_delay)
                    time.sleep(0.5)

                    print(f'\nServo {which}: vuelve a 60°')
                    for a in range(120,59,-args.servo_step):
                        if which==1: s1=a
                        else: s2=a
                        send_selected(s1,s2)
                        time.sleep(args.servo_delay)
                    if which==1: s1=60.0
                    else: s2=60.0
                    print(send_selected(s1,s2))
                    print(f'Servo {which} en 60° / 1500 us.')

                if selection in {1,3}: sweep_one(1)
                if selection in {2,3}: sweep_one(2)
                listen(link,2.0,log)

        # ------------------------------------------------------------------
        # P00M: primero posiciona ambos servos SIN motores usando P00. Luego
        # usa la secuencia de ARM ORIGINAL del firmware viejo y permite 0..100%.
        # ------------------------------------------------------------------
        elif args.test=='p00m':
            s1=args.servo1_angle if args.servo1_angle is not None else args.servo_angle
            s2=args.servo2_angle if args.servo2_angle is not None else args.servo_angle
            power=args.power/100.0
            m1=power if args.motor in {'1','both'} else 0.0
            m2=power if args.motor in {'2','both'} else 0.0

            print('\nP00M MOTOR POWER TEST')
            print('Control DIRECTO: sin PID ni mixer.')
            print('100% corresponde a la misma escala maxima 0..1 del firmware viejo.')
            print(f'Posicionando primero S1={s1:.1f}° y S2={s2:.1f}° con motores deshabilitados...')

            # P00 + selector 3: posiciona ambos servos sin habilitar brushless.
            print(link.control(mac,mode=modes.P00_SERVO_CALIBRATION,
                               aux={5:s1,6:s2,7:3.0},reset=1))
            time.sleep(1.0)

            print('\nATENCION: al ARMAR se ejecutara la MISMA secuencia de armado del firmware viejo.')
            print('Esa secuencia hace un barrido simultaneo en ambos brushless antes de volver a cero.')
            if input('Escribe ARMAR para ejecutar esa secuencia: ').strip().upper()!='ARMAR':
                raise SystemExit('Cancelado antes de armar motores.')

            # IMPORTANTE: ARM con mode=P00M. No usar link.arm(), porque ese helper
            # usa mode=0 y SAFE_STOP volveria a desarmar inmediatamente.
            print(link.control(mac,mode=mode,arm=1,
                               aux={5:s1,6:s2,7:0.0,8:0.0}))
            print('Secuencia de armado terminada; ambos brushless regresaron a 0%.')
            time.sleep(0.5)

            print(f'Objetivo: M1={m1*100:.1f}%  M2={m2*100:.1f}%')
            if input('Escribe MOTOR para aplicar esa potencia: ').strip().upper()!='MOTOR':
                raise SystemExit('Cancelado antes de aplicar potencia.')

            print(link.control(mac,mode=mode,aux={5:s1,6:s2,7:m1,8:m2}))
            listen(link,args.motor_seconds,log)

            print(link.control(mac,mode=mode,aux={5:s1,6:s2,7:0.0,8:0.0}))
            print('Brushless a 0%.')
            listen(link,0.8,log)

        else:
            fx,fz,tx,tz,aux_cfg,descriptions=build_control_payload(args)

            # Para cualquier prueba actuada, el paquete ARM lleva YA las
            # referencias y ganancias. Despues esperamos la secuencia ESC.
            if args.test in ACTUATED:
                print('\n⚠ Esta prueba puede mover actuadores.')
                for d in descriptions: print(d)
                if input('Escribe ARMAR para habilitar actuadores: ').strip().upper()!='ARMAR':
                    raise SystemExit('Cancelado; actuadores siguen desarmados.')
                print(link.control(mac,mode=mode,fx=fx,fz=fz,tx=tx,tz=tz,
                                   aux=aux_cfg,arm=1))
                print('Armando ESC... esperando 4.2 s antes del siguiente comando.')
                time.sleep(4.2)

            # RESET conserva exactamente las mismas referencias/tuning.
            print(link.control(mac,mode=mode,fx=fx,fz=fz,tx=tx,tz=tz,
                               aux=aux_cfg,reset=1))

            if args.test=='p02':
                print('Manual P02: w=avance, s=retroceso, a=izquierda, d=derecha, r=Z, f=Z pasivo, x=STOP.')
                print('Cada tecla queda activa hasta enviar otra tecla o x. Enter despues de cada tecla.')
                while True:
                    k=input('> ').strip().lower()
                    if k=='x': break
                    fx0=fz0=tx0=tz0=0.0
                    if k=='w': fx0=0.12
                    elif k=='s': fx0=-0.12
                    elif k=='a': tz0=-0.05
                    elif k=='d': tz0=0.05
                    elif k=='r': fz0=0.12
                    elif k=='f': fz0=-0.12
                    else:
                        print('Tecla invalida. Usa w/s/a/d/r/f/x.')
                        continue
                    print(link.control(mac,mode=mode,fx=fx0,fz=fz0,tx=tx0,tz=tz0))
            else:
                listen(link,args.seconds,log)

    except KeyboardInterrupt:
        pass
    finally:
        print('STOP + DISARM')
        try:
            link.stop(mac)
        except Exception:
            pass
        link.close()
        log.close()
        print('CSV:',log.path)


if __name__=='__main__':
    main()