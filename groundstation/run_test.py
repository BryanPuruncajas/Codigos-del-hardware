import argparse, sys, time, math
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from user_parameters import SERIAL_PORT, ROBOT_MACS
from common.link import BlimpLink
from common.telemetry import parse, LABELS
from common.csv_logger import CsvLogger
from common.control_pack import (
    NAME_TO_MODE, ACTUATED, build_control_payload, _alt_values, _yaw_deadband,
)
from tests import modes


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


EPILOG = """\
SECUENCIA RECOMENDADA DE CARACTERIZACION

  1. p00m   potencia de hover en lazo abierto (servos 35/95, sin controlador).
             Es el numero que ancla todo lo demas.
  2. p04     altura aislada. Mismo lazo que P05: las ganancias transfieren.
  3. p03     yaw aislado. Misma arquitectura y unidades que P05.
             Usa --yaw-base-power con la potencia de hover de p00m para
             ensayar cerca del punto de operacion real.
  4. p05     los dos juntos, para ver el acoplamiento.

TRUCOS PARA AISLAR LAZOS DENTRO DE P05

  Yaw puro     : --height -2   (el lazo Z se satura en 0 y el integral se
                                congela; solo actua el yaw)
  Altura pura  : --yaw-deg <tu rumbo actual>  (el error de yaw cae en la zona
                                muerta y el yaw no actua)

UNIDADES DEL YAW  (cambiaron en v6; los valores viejos tipo 0.08 NO sirven)

  --yaw-kp  mando por radian.  2.0 satura el mando a ~29 grados de error.
  --yaw-kd  mando por rad/s.
  --yaw-ki  mando por radian-segundo. Se ajusta al valor mas cercano de
            {0.00, 0.05, 0.10, 0.20, 0.35, 0.50, 0.75, 1.00}.

VALORES CUANTIZADOS

  Todo lo que va empaquetado se ajusta a una rejilla. El script imprime los
  valores EFECTIVOS antes de armar: revisalos, son los que va a aplicar el
  firmware.
"""


def main():
    ap=argparse.ArgumentParser(description='Banco de pruebas del blimp',
                               epilog=EPILOG,
                               formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('test',choices=sorted(NAME_TO_MODE))
    ap.add_argument('--seconds',type=float,default=30)
    ap.add_argument('--yaw-deg',type=float,default=None,help='P03/P05: referencia absoluta en grados')
    ap.add_argument('--height',type=float,default=None,help='P04/P05/P10/P11: referencia de altura del barometro en metros')

    # ---------------------------- ALTURA ----------------------------
    ap.add_argument('--kp',type=float,default=0.30,help='Altura: Kp (alias simple); default 0.30')
    ap.add_argument('--ki',type=float,default=0.025,help='Altura: Ki (alias simple); default 0.025')
    ap.add_argument('--kd',type=float,default=0.10,help='Altura: Kd (alias simple); default 0.10')
    ap.add_argument('--max-power',type=float,default=15.0,help='Altura: potencia maxima en %%; default 15')
    ap.add_argument('--slew',type=float,default=0.18,help='Altura: rampa maxima 0..1/s; default 0.18')
    ap.add_argument('--alt-kp',type=float,default=None,help='Altura: Kp explicito; sobreescribe --kp')
    ap.add_argument('--alt-ki',type=float,default=None,help='Altura: Ki explicito; sobreescribe --ki')
    ap.add_argument('--alt-kd',type=float,default=None,help='Altura: Kd explicito; sobreescribe --kd')
    ap.add_argument('--alt-max-power',type=float,default=None,help='Altura: max %% explicito; sobreescribe --max-power')
    ap.add_argument('--alt-slew',type=float,default=None,help='Altura: slew explicito; sobreescribe --slew')
    ap.add_argument('--alt-min-power',type=float,default=0.0,
                    help='P03/P04/P05: piso de potencia de altura en %%; default 0. '
                         'Dejalo en 0 mientras sintonizas: un piso alto se traga el '
                         'termino proporcional y convierte el lazo en un rele.')
    ap.add_argument('--alt-success-cm',type=float,default=10.0,
                    help='P04/P05: banda para declarar Z adquirido, 2..30 cm en pasos de 2; default 10')

    # ----------------------------- YAW ------------------------------
    # OJO: unidades v6. Kp esta en MANDO POR RADIAN, no en fraccion de
    # potencia. Los valores de la version vieja (0.08 .. 0.12) dejan el yaw
    # practicamente muerto.
    ap.add_argument('--yaw-kp',type=float,default=0.40,
                    help='Yaw lazo EXTERNO: velocidad deseada por radian de error [1/s]; '
                         'default 0.40. El error se trunca a 36 grados antes de aplicarlo.')
    ap.add_argument('--yaw-ki',type=float,default=1.00,
                    help='Yaw lazo INTERNO: integral del error de VELOCIDAD; default 1.00. '
                         'Se ajusta a {0, 0.25, 0.5, 1, 2, 3, 5, 8}. Es el que anula el '
                         'par perturbador constante.')
    ap.add_argument('--yaw-kd',type=float,default=3.00,
                    help='Yaw lazo INTERNO: mando por (rad/s) de error de velocidad; '
                         'default 3.00')
    ap.add_argument('--yaw-min-power',type=float,default=4.5,
                    help='Yaw: potencia con mando minimo, 0..31.5%% en pasos de 0.5; default 4.5')
    ap.add_argument('--yaw-max-power',type=float,default=11.0,
                    help='Yaw: potencia con mando saturado, 0..31.5%% en pasos de 0.5; default 11. '
                         'Separalo bien del minimo: la potencia se interpola entre los dos.')
    ap.add_argument('--yaw-deadband-deg',type=float,default=3.0,
                    help='Yaw: zona muerta en grados enteros, 1..15; default 3. '
                         'Dentro de esta banda el yaw no actua y el integral se congela.')
    ap.add_argument('--yaw-servo-authority',type=float,default=75.0,
                    help='Yaw: cuanto se alejan los servos del vector Z, '
                         '12.5..100%% en pasos de 12.5; default 75')
    ap.add_argument('--yaw-base-power',type=float,default=0.0,
                    help='P03: empuje vertical constante en %% durante el ensayo de yaw. '
                         'Pon aqui la potencia de hover medida con p00m; default 0')

    # Alias historico: en v6 la banda de exito del yaw ES la zona muerta.
    ap.add_argument('--yaw-success-deg',type=float,default=None,
                    help='Alias historico de --yaw-deadband-deg')

    # Flags antiguos aceptados por compatibilidad; ya no se usan.
    ap.add_argument('--yaw-enter-deg',type=float,default=None,help=argparse.SUPPRESS)
    ap.add_argument('--yaw-exit-deg',type=float,default=None,help=argparse.SUPPRESS)
    ap.add_argument('--yaw-emergency-deg',type=float,default=None,help=argparse.SUPPRESS)
    ap.add_argument('--yaw-settle-ms',type=float,default=None,help=argparse.SUPPRESS)
    ap.add_argument('--alt-min-dwell-ms',type=float,default=None,help=argparse.SUPPRESS)

    # ---------------------------- VISION ----------------------------
    ap.add_argument('--vis-kp',type=float,default=0.10,help='Vision PD: Kp sobre error normalizado; default 0.10')
    ap.add_argument('--vis-kd',type=float,default=0.015,help='Vision PD: Kd; default 0.015')
    ap.add_argument('--vis-min-power',type=float,default=6.0,help='Vision: potencia minima de giro %%; default 6')
    ap.add_argument('--vis-max-power',type=float,default=10.0,help='Vision: potencia maxima de giro %%; default 10')
    ap.add_argument('--vis-deadband-px',type=float,default=20.0,
                    help='Vision: semiancho de zona centrada en px; default 20. '
                         'El ruido medido de nicla_x es ~5 px.')
    ap.add_argument('--vis-fov-deg',type=float,default=90.0,
                    help='P07: campo de vision horizontal de la camara en grados; '
                         'default 90. Convierte pixeles a grados de giro. Es una '
                         'propiedad de la LENTE, no una ganancia: ajustalo si el '
                         'centrado se pasa o se queda corto de forma sistematica.')

    # ----------------------------- P00 ------------------------------
    ap.add_argument('--servo',choices=['1','2','both'],default='1',help='P00: servo a mover; default 1')
    ap.add_argument('--servo-step',type=int,default=2,help='P00: paso en grados (1..10); default 2')
    ap.add_argument('--servo-delay',type=float,default=0.06,help='P00: pausa entre pasos; default 0.06 s')
    ap.add_argument('--servo-angle',type=float,default=None,help='P00: fija SOLO el servo seleccionado; P00M: angulo comun para ambos')
    ap.add_argument('--servo1-angle',type=float,default=None,help='P00M: angulo Servo 1')
    ap.add_argument('--servo2-angle',type=float,default=None,help='P00M: angulo Servo 2')

    ap.add_argument('--motor',choices=['1','2','both'],default=None,help='P00M: brushless a probar')
    ap.add_argument('--power',type=float,default=None,help='P00M: potencia 0..100%%; 100%% = misma escala maxima del firmware viejo')
    ap.add_argument('--motor-seconds',type=float,default=3.0,
                    help='P00M: tiempo con cada potencia aplicada, 0.2..300 s; default 3. '
                         'Para buscar la flotabilidad neutra necesitas 30..60 s por nivel: '
                         'la deriva del blimp es lenta.')
    ap.add_argument('--power-sweep',default=None,metavar='LOW:HIGH:STEP',
                    help='P00M: escalera automatica de potencia, p.ej. 5:12:1 recorre '
                         '5%%,6%%,...,12%% manteniendo cada nivel --motor-seconds. '
                         'Todo en UN SOLO ciclo de armado, sin repetir la secuencia del ESC. '
                         'Sustituye a --power.')
    ap.add_argument('--sweep-updown',action='store_true',
                    help='P00M: recorre la escalera subiendo y VOLVIENDO A BAJAR. '
                         'Cada nivel se mide dos veces en instantes distintos, lo que '
                         'permite separar el efecto de la POTENCIA del efecto del '
                         'TIEMPO (bateria, helio, temperatura). Sin esto, un barrido '
                         'ascendente confunde ambos y puede dar ganancia negativa.')
    args=ap.parse_args()

    if not ROBOT_MACS:
        raise SystemExit('Configura ROBOT_MACS en user_parameters.py')

    if args.test in {'p03','p05'} and args.yaw_deg is None:
        raise SystemExit('P03/P05 requieren --yaw-deg.')
    if args.test in {'p04','p05','p08','p09','p10','p11'} and args.height is None:
        raise SystemExit('P04/P05 y las misiones P08..P11 requieren --height.')

    akp,aki,akd,amax,aslew=_alt_values(args)
    ydb=_yaw_deadband(args)

    if args.yaw_success_deg is not None:
        print('NOTA: --yaw-success-deg es alias historico de --yaw-deadband-deg. '
              f'Se usara {ydb:.0f} grados como zona muerta.')

    # ------------------------- VALIDACION ALTURA -------------------------
    if args.test in {'p04','p05','p08','p09','p10','p11'}:
        if akp < 0.0 or aki < 0.0 or akd < 0.0:
            raise SystemExit('Control de altura requiere Kp, Ki y Kd >= 0.')
        if not (0.1 <= amax <= 100.0):
            raise SystemExit('Potencia maxima de altura debe estar entre 0.1 y 100%.')
        if aslew <= 0.0:
            raise SystemExit('Slew de altura debe ser > 0.')

    if args.test in {'p04','p05'}:
        if not (0.0 <= args.alt_min_power <= amax <= 100.0):
            raise SystemExit('Se requiere 0 <= --alt-min-power <= max-power <= 100%.')
        if not (0.02 <= aslew <= 0.30):
            raise SystemExit('--slew/--alt-slew debe estar entre 0.02 y 0.30 /s.')
        if not (2.0 <= args.alt_success_cm <= 30.0):
            raise SystemExit('--alt-success-cm debe estar entre 2 y 30 cm.')

    # -------------------------- VALIDACION YAW ---------------------------
    if args.test in {'p03','p05'}:
        if args.yaw_kp <= 0:
            raise SystemExit('--yaw-kp debe ser > 0. En v6 esta en mando por radian '
                             '(default 2.00), no en fraccion de potencia.')
        if args.yaw_kd < 0 or args.yaw_ki < 0:
            raise SystemExit('--yaw-kd y --yaw-ki deben ser >= 0.')
        if not (0.0 <= args.yaw_min_power <= args.yaw_max_power <= 31.5):
            raise SystemExit('Se requiere 0 <= yaw-min-power <= yaw-max-power <= 31.5%.')
        if not (1.0 <= ydb <= 15.0):
            raise SystemExit('--yaw-deadband-deg debe estar entre 1 y 15 grados.')
        if not (12.5 <= args.yaw_servo_authority <= 100.0):
            raise SystemExit('--yaw-servo-authority debe estar entre 12.5 y 100%.')
        if not (0.0 <= args.yaw_ki <= 8.0):
            raise SystemExit('--yaw-ki debe estar entre 0 y 8.0 (unidades v7: '
                             'integral del error de VELOCIDAD).')
        # El valor correcto de yaw-kp depende del RETARDO del vehiculo, no de
        # una regla fija. Un dirigible con mucha inercia rotacional necesita
        # kp bajo y kd alto (kd/kp del orden del retardo en segundos). Solo se
        # avisa cuando el mando no puede saturar ni con media vuelta de error,
        # que si indica un valor equivocado de unidades.
        wmax = math.degrees(args.yaw_kp * math.radians(36.0))
        if wmax > 60.0:
            print(f'AVISO: --yaw-kp {args.yaw_kp:.2f} pide hasta {wmax:.0f} deg/s. '
                  f'Tu vehiculo no alcanza esas velocidades y el lazo interno '
                  f'quedara saturado todo el tiempo. Baja a 0.3-0.6.')
        else:
            print(f'INFO: velocidad maxima pedida {wmax:.1f} deg/s '
                  f'(error truncado a 36 grados).')

    if args.test=='p03':
        if not (0.0 <= args.yaw_base_power <= 30.0):
            raise SystemExit('--yaw-base-power debe estar entre 0 y 30%.')

    # ------------------------ VALIDACION VISION --------------------------
    if args.test=='p07':
        if args.yaw_kp <= 0:
            raise SystemExit('P07 usa la cascada de yaw: --yaw-kp debe ser > 0.')
        if not (0.0 <= args.yaw_min_power <= args.yaw_max_power <= 31.5):
            raise SystemExit('Se requiere 0 <= yaw-min-power <= yaw-max-power <= 31.5%.')
        if not (20.0 <= args.vis_fov_deg <= 180.0):
            raise SystemExit('--vis-fov-deg debe estar entre 20 y 180 grados.')
        if not (1 <= args.vis_deadband_px <= 100):
            raise SystemExit('--vis-deadband-px debe estar entre 1 y 100.')

    if args.test in {'p08','p09','p10','p11'}:
        if args.vis_kp < 0 or args.vis_kd < 0:
            raise SystemExit('Vision requiere --vis-kp y --vis-kd >= 0.')
        if not (0 <= args.vis_min_power <= args.vis_max_power <= 100):
            raise SystemExit('Vision requiere 0 <= min-power <= max-power <= 100.')
        if not (1 <= args.vis_deadband_px <= 100):
            raise SystemExit('--vis-deadband-px debe estar entre 1 y 100.')

    # -------------------------- VALIDACION P00 ---------------------------
    if args.test=='p00m':
        if args.motor is None:
            raise SystemExit('P00M requiere --motor 1, --motor 2 o --motor both.')
        if args.power_sweep is None:
            if args.power is None or not (0.0 < args.power <= 100.0):
                raise SystemExit('P00M requiere --power entre >0 y 100, '
                                 'o bien --power-sweep LOW:HIGH:STEP.')
        else:
            try:
                lo,hi,st=[float(x) for x in args.power_sweep.split(':')]
            except ValueError:
                raise SystemExit('--power-sweep usa el formato LOW:HIGH:STEP, p.ej. 5:12:1')
            if not (0.0 < lo <= hi <= 100.0):
                raise SystemExit('--power-sweep requiere 0 < LOW <= HIGH <= 100.')
            if st <= 0:
                raise SystemExit('--power-sweep requiere STEP > 0.')
            n=int(round((hi-lo)/st))+1
            if args.sweep_updown:
                n=2*n-1
            if n > 40:
                raise SystemExit(f'--power-sweep generaria {n} niveles. Maximo 40; '
                                 'usa un STEP mas grande o un rango mas corto.')
        if not (0.2 <= args.motor_seconds <= 300.0):
            raise SystemExit('--motor-seconds debe estar entre 0.2 y 300 s.')
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
            if args.power_sweep:
                lo,hi,st=[float(x) for x in args.power_sweep.split(':')]
                n=int(round((hi-lo)/st))+1
                levels=[round(lo+i*st,3) for i in range(n)]
                if args.sweep_updown and n>=2:
                    # sube y vuelve a bajar sin repetir el tope
                    levels=levels+levels[-2::-1]
            else:
                levels=[args.power]

            print('\nP00M MOTOR POWER TEST')
            print('Control DIRECTO: sin PID ni mixer.')
            print('100% corresponde a la misma escala maxima 0..1 del firmware viejo.')
            print('Para medir la POTENCIA DE HOVER: servos en 35/95 y sube la potencia')
            print('hasta que el blimp no suba ni baje. Ese numero es el que ancla')
            print('--alt-min-power, --max-power y --yaw-base-power.')
            print('')
            print('ATENCION - SIN FAILSAFE DE ENLACE: el firmware mantiene la ultima')
            print('potencia recibida indefinidamente. Si se corta el USB o se cierra')
            print('esta ventana a la fuerza, los motores NO se detienen solos.')
            print('Manten el blimp a la vista y Ctrl+C a la mano: eso si desarma.')
            print(f'Posicionando primero S1={s1:.1f}° y S2={s2:.1f}° con motores deshabilitados...')

            # P00 + selector 3: posiciona ambos servos sin habilitar brushless.
            print(link.control(mac,mode=modes.P00_SERVO_CALIBRATION,
                               aux={5:s1,6:s2,7:3.0},reset=1))
            time.sleep(1.0)

            print('\nATENCION: al ARMAR se ejecutara la MISMA secuencia de armado del firmware viejo.')
            print('Esa secuencia mantiene throttle minimo durante ~3.7 s antes de volver a cero.')
            if input('Escribe ARMAR para ejecutar esa secuencia: ').strip().upper()!='ARMAR':
                raise SystemExit('Cancelado antes de armar motores.')

            # IMPORTANTE: ARM con mode=P00M. No usar link.arm(), porque ese helper
            # usa mode=0 y SAFE_STOP volveria a desarmar inmediatamente.
            print(link.control(mac,mode=mode,arm=1,
                               aux={5:s1,6:s2,7:0.0,8:0.0}))
            print('Secuencia de armado terminada; ambos brushless regresaron a 0%.')
            time.sleep(0.5)

            total=len(levels)*args.motor_seconds
            if len(levels)>1:
                orden='subiendo y bajando' if args.sweep_updown else 'ascendente'
                print(f'\nESCALERA {orden}: {len(levels)} tramos '
                      f'({min(levels):.1f}% .. {max(levels):.1f}%), '
                      f'{args.motor_seconds:.0f}s cada uno = {total/60:.1f} min en total.')
                print('Todo en este mismo ciclo de armado; no se repite la secuencia del ESC.')
                if args.sweep_updown:
                    print('Cada nivel se mide dos veces: eso separa el efecto de la')
                    print('potencia del efecto del tiempo (bateria, helio, temperatura).')
            else:
                print(f'\nObjetivo: {levels[0]:.1f}% durante {args.motor_seconds:.0f}s.')

            if input('Escribe MOTOR para aplicar potencia: ').strip().upper()!='MOTOR':
                raise SystemExit('Cancelado antes de aplicar potencia.')

            for i,lvl in enumerate(levels,1):
                p=lvl/100.0
                m1=p if args.motor in {'1','both'} else 0.0
                m2=p if args.motor in {'2','both'} else 0.0
                print(f'\n--- nivel {i}/{len(levels)}: {lvl:.1f}%  '
                      f'({args.motor_seconds:.0f}s) ---')
                print(link.control(mac,mode=mode,aux={5:s1,6:s2,7:m1,8:m2}))
                listen(link,args.motor_seconds,log)

            print(link.control(mac,mode=mode,aux={5:s1,6:s2,7:0.0,8:0.0}))
            print('\nBrushless a 0%.')
            listen(link,0.8,log)
            if len(levels)>1:
                print('Grafica la corrida para ver en que escalon la altura deja de derivar:')
                print(f'  python analyze.py {log.path} --no-trim')

        else:
            fx,fz,tx,tz,aux_cfg,descriptions=build_control_payload(args)

            # Para cualquier prueba actuada, el paquete ARM lleva YA las
            # referencias y ganancias. Despues esperamos la secuencia ESC.
            if args.test in ACTUATED:
                print('\n⚠ Esta prueba puede mover actuadores.')
                print('VALORES EFECTIVOS (ya cuantizados; esto es lo que aplicara el firmware):')
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