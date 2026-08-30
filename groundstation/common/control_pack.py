"""
control_pack.py - Empaquetado de parametros de control (formato v2/v6/v7).

Logica movida ACA desde run_test.py para que telemetry_bridge.py (y por lo
tanto la app Flutter) arme exactamente el mismo paquete de bits que la CLI,
en vez de reimplementarlo en Dart. Si cambias algo aca, run_test.py y la app
cambian juntos automaticamente.

ESTE FORMATO DEBE COINCIDIR CON firmware/src/app/ControlCommon.h.
Ver comentarios extendidos en el propio ControlCommon.h y en el uso de
build_control_payload() para el porque del empaquetado.
"""

import math
from types import SimpleNamespace

from tests import modes

NAME_TO_MODE = {
    'p00m': modes.P00_MOTOR_POWER_TEST, 'p00': modes.P00_SERVO_CALIBRATION,
    'p01': modes.P01_SENSOR_INTEGRATION, 'p02': modes.P02_MANUAL_CONTROL,
    'p03': modes.P03_YAW_CONTROL, 'p04': modes.P04_ALTITUDE_CONTROL,
    'p05': modes.P05_YAW_ALTITUDE, 'p06': modes.P06_NICLA_RAW,
    'p07': modes.P07_VISUAL_CENTER, 'p08': modes.P08_ONE_BALLOON,
    'p09': modes.P09_VISIT_ESCAPE, 'p10': modes.P10_TWO_BALLOONS,
    'p11': modes.P11_FOUR_BALLOONS,
}
ACTUATED = {'p02', 'p03', 'p04', 'p05', 'p07', 'p08', 'p09', 'p10', 'p11'}

PACK_MARKER_V2 = 1 << 24

# Debe coincidir con YAW_KI_TABLE de ControlCommon.h
YAW_KI_TABLE = [0.00, 0.25, 0.50, 1.00, 2.00, 3.00, 5.00, 8.00]

# Defaults identicos a los --flags de run_test.py, para que un cliente
# (la app) solo tenga que mandar lo que quiere cambiar.
DEFAULT_ARGS = dict(
    yaw_deg=None, height=None,
    kp=0.30, ki=0.025, kd=0.10, max_power=15.0, slew=0.18,
    alt_kp=None, alt_ki=None, alt_kd=None, alt_max_power=None, alt_slew=None,
    alt_min_power=0.0, alt_success_cm=10.0,
    yaw_kp=0.40, yaw_ki=1.00, yaw_kd=3.00,
    yaw_min_power=4.5, yaw_max_power=11.0, yaw_deadband_deg=3.0,
    yaw_servo_authority=75.0, yaw_base_power=0.0, yaw_success_deg=None,
    vis_kp=0.10, vis_kd=0.015, vis_min_power=6.0, vis_max_power=10.0,
    vis_deadband_px=20.0, vis_fov_deg=90.0,
)


def make_args(test: str, **overrides) -> SimpleNamespace:
    """Namespace con los mismos defaults que la CLI, mas overrides del caller."""
    data = dict(DEFAULT_ARGS)
    data['test'] = test
    data.update({k: v for k, v in overrides.items() if v is not None})
    return SimpleNamespace(**data)


def _wrap_payload(payload):
    """Envuelve 22 bits de payload en el formato v2 (valor par, >= 2^24)."""
    return float(PACK_MARKER_V2 | ((payload & 0x3FFFFF) << 1))


def _clamp(v, lo, hi):
    return max(lo, min(hi, v))


def pack_alt(min_pct, max_pct, slew, success_cm):
    """ALT_PACK v2.

    bits  0.. 6 (7) : potencia minima, % entero
    bits  7..13 (7) : potencia maxima, % entero
    bits 14..17 (4) : slew / 0.02        -> 0.02 .. 0.30 /s
    bits 18..21 (4) : banda exito / 2 cm -> 2 .. 30 cm
    """
    min_u = _clamp(int(round(min_pct)), 0, 100)
    max_u = _clamp(int(round(max_pct)), 1, 100)
    slew_u = _clamp(int(round(slew / 0.02)), 1, 15)
    ok_u = _clamp(int(round(success_cm / 2.0)), 1, 15)

    packed = _wrap_payload((min_u & 0x7F) |
                            ((max_u & 0x7F) << 7) |
                            ((slew_u & 0x0F) << 14) |
                            ((ok_u & 0x0F) << 18))

    effective = {
        'min_pct': float(min_u),
        'max_pct': float(max_u),
        'slew': slew_u * 0.02,
        'success_cm': ok_u * 2.0,
    }
    return packed, effective


def pack_yaw(min_pct, max_pct, deadband_deg, authority_pct, ki):
    """YAW_PACK v2.

    bits  0.. 5 (6) : potencia minima / 0.5 %  -> 0 .. 31.5 %
    bits  6..11 (6) : potencia maxima / 0.5 %  -> 0 .. 31.5 %
    bits 12..15 (4) : zona muerta, grados enteros 1 .. 15
    bits 16..18 (3) : autoridad de servo, (i+1) * 12.5 %
    bits 19..21 (3) : indice en YAW_KI_TABLE
    """
    min_u = _clamp(int(round(min_pct / 0.5)), 0, 63)
    max_u = _clamp(int(round(max_pct / 0.5)), 1, 63)
    db_u = _clamp(int(round(deadband_deg)), 1, 15)
    auth_u = _clamp(int(round(authority_pct / 12.5)) - 1, 0, 7)

    ki_u = min(range(len(YAW_KI_TABLE)),
               key=lambda i: abs(YAW_KI_TABLE[i] - ki))

    packed = _wrap_payload((min_u & 0x3F) |
                            ((max_u & 0x3F) << 6) |
                            ((db_u & 0x0F) << 12) |
                            ((auth_u & 0x07) << 16) |
                            ((ki_u & 0x07) << 19))

    effective = {
        'min_pct': min_u * 0.5,
        'max_pct': max_u * 0.5,
        'deadband_deg': float(db_u),
        'authority_pct': (auth_u + 1) * 12.5,
        'ki': YAW_KI_TABLE[ki_u],
    }
    return packed, effective


def _alt_values(args):
    return (
        args.alt_kp if args.alt_kp is not None else args.kp,
        args.alt_ki if args.alt_ki is not None else args.ki,
        args.alt_kd if args.alt_kd is not None else args.kd,
        args.alt_max_power if args.alt_max_power is not None else args.max_power,
        args.alt_slew if args.alt_slew is not None else args.slew,
    )


def _yaw_deadband(args):
    """--yaw-success-deg queda como alias historico de --yaw-deadband-deg."""
    if args.yaw_success_deg is not None:
        return args.yaw_success_deg
    return args.yaw_deadband_deg


def build_control_payload(args):
    """Empaqueta referencias y tuning segun el modo sin cambiar ControlInput.

    `args` puede ser un argparse.Namespace (run_test.py) o un
    types.SimpleNamespace armado con make_args() (telemetry_bridge.py):
    solo se usan sus atributos, nunca metodos de argparse.
    """
    fx = 0.0; fz = args.height or 0.0; tx = 0.0
    tz = math.radians(args.yaw_deg) if args.yaw_deg is not None else 0.0
    aux = {}
    desc = []
    akp, aki, akd, amax, aslew = _alt_values(args)
    ydb = _yaw_deadband(args)

    if args.test == 'p03':
        yaw_pack, yeff = pack_yaw(args.yaw_min_power, args.yaw_max_power,
                                   ydb, args.yaw_servo_authority, args.yaw_ki)
        aux = {5: args.yaw_kp, 6: args.yaw_kd, 7: yaw_pack,
               8: args.yaw_base_power / 100.0}
        desc.append('P03 YAW CASCADA v7 (transfiere a P05)')
        desc.append(f'  externo: Kp={args.yaw_kp:.3f} (rad/s por rad)  '
                     f'error truncado a +/-36deg')
        desc.append(f'  interno: Kd={args.yaw_kd:.3f} (mando por rad/s)  '
                     f'Ki={yeff["ki"]:.2f}   sobre el giroscopio')
        desc.append(f'  Potencia {yeff["min_pct"]:.1f}% .. {yeff["max_pct"]:.1f}%   '
                     f'ZonaMuerta=+/-{yeff["deadband_deg"]:.0f}deg   '
                     f'Autoridad={yeff["authority_pct"]:.1f}%')

    elif args.test == 'p04':
        alt_pack, aeff = pack_alt(args.alt_min_power, amax, aslew, args.alt_success_cm)
        aux = {5: akp, 6: aki, 7: akd, 8: alt_pack}
        desc.append('P04 ALT PID (mismo lazo que P05; las ganancias transfieren)')
        desc.append(f'  ref={fz:.2f}m   Kp={akp:.3f}   Ki={aki:.4f}   Kd={akd:.3f}')
        desc.append(f'  Potencia {aeff["min_pct"]:.0f}% .. {aeff["max_pct"]:.0f}%   '
                     f'Slew={aeff["slew"]:.2f}/s   '
                     f'Banda=+/-{aeff["success_cm"]:.0f}cm')

    elif args.test == 'p05':
        fx = akp; tx = akd
        alt_pack, aeff = pack_alt(args.alt_min_power, amax, aslew, args.alt_success_cm)
        yaw_pack, yeff = pack_yaw(args.yaw_min_power, args.yaw_max_power,
                                   ydb, args.yaw_servo_authority, args.yaw_ki)
        aux = {5: aki, 6: alt_pack, 7: args.yaw_kp, 8: args.yaw_kd, 9: yaw_pack}
        desc.append('P05 v6: los dos lazos activos siempre; mixer power=max(pAlt,pYaw)')
        desc.append(f'  ALTURA ref={fz:.2f}m   Kp={akp:.3f}   Ki={aki:.4f}   Kd={akd:.3f}')
        desc.append(f'  Potencia {aeff["min_pct"]:.0f}% .. {aeff["max_pct"]:.0f}%   '
                     f'Slew={aeff["slew"]:.2f}/s   Banda=+/-{aeff["success_cm"]:.0f}cm')
        desc.append(f'  YAW ref={args.yaw_deg:.1f}deg  externo Kp={args.yaw_kp:.3f}  '
                     f'interno Kd={args.yaw_kd:.3f} Ki={yeff["ki"]:.2f}')
        desc.append(f'  Potencia {yeff["min_pct"]:.1f}% .. {yeff["max_pct"]:.1f}%   '
                     f'ZonaMuerta=+/-{yeff["deadband_deg"]:.0f}deg   '
                     f'Autoridad={yeff["authority_pct"]:.1f}%')

    elif args.test == 'p07':
        yaw_pack, yeff = pack_yaw(args.yaw_min_power, args.yaw_max_power,
                                   ydb, args.yaw_servo_authority, args.yaw_ki)
        aux = {5: args.yaw_kp, 6: args.yaw_kd, 7: yaw_pack,
               8: args.vis_fov_deg, 9: args.vis_deadband_px}
        desc.append('P07 v2: vision -> referencia de yaw -> CASCADA')
        desc.append(f'  cascada: externo Kp={args.yaw_kp:.3f}  '
                     f'interno Kd={args.yaw_kd:.3f} Ki={yeff["ki"]:.2f}')
        desc.append(f'  vision: FOV={args.vis_fov_deg:.0f}deg  '
                     f'zona muerta={args.vis_deadband_px:.0f}px')

    elif args.test in {'p08', 'p09', 'p10', 'p11'}:
        # Mapa de slots UNIFICADO para todas las misiones.
        #   FZ=altura | FX=altKp | TX=altKd | TZ=altKi
        #   AUX0(5)=altMax | AUX1(6)=altSlew
        #   AUX2(7)=visMin | AUX3(8)=visMax | AUX4(9)=visDeadbandPx
        fx = akp; tx = akd; tz = aki
        aux = {5: amax / 100.0, 6: aslew,
               7: args.vis_min_power / 100.0, 8: args.vis_max_power / 100.0,
               9: args.vis_deadband_px}
        desc.append(f'MISION {args.test.upper()}: altura + vision sobre la cascada')
        desc.append(f'  ALTURA ref={fz:.2f}m  Kp={akp:.3f} Ki={aki:.4f} Kd={akd:.3f}  '
                     f'Max={amax:.1f}%  Slew={aslew:.2f}/s')
        desc.append(f'  VISION giro {args.vis_min_power:.1f}%..{args.vis_max_power:.1f}%  '
                     f'centrado +/-{args.vis_deadband_px:.0f}px')

    return fx, fz, tx, tz, aux, [d for d in desc if d]
