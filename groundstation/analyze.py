"""
analyze.py  -  Graficas y metricas de los logs del blimp

USO
---
    python analyze.py logs/blimp_20260823_101500.csv
    python analyze.py logs/blimp_*.csv                    # superpone corridas
    python analyze.py logs/*.csv --band-cm 5 --band-deg 5
    python analyze.py logs/*.csv --save figs/             # guarda PNG
    python analyze.py logs/*.csv --no-show                # solo metricas

POR QUE HACE FALTA PIVOTAR
--------------------------
El CSV esta en FORMATO LARGO: una fila por trama de telemetria, y la columna
`flag` decide que significan las 6 parejas nombre/valor de esa fila.

    F1  height, yaw, roll, pitch, battery, vertical_velocity
    F3  servo1, servo2, motor1, motor2, armed, mode
    F4  rollrate, pitchrate, yawrate, yaw_ref, height_ref, fx_cmd

Cada flag llega a 10 Hz de forma INDEPENDIENTE, asi que `height` (F1) y
`height_ref` (F4) nunca comparten fila ni instante. Para graficar uno contra
otro hay que reconstruir una base de tiempo comun. Aqui se hace con retencion
de orden cero (zero-order hold), que es lo correcto para telemetria muestreada:
cada senal conserva su ultimo valor conocido hasta que llega uno nuevo.

QUE SE PUEDE Y QUE NO
---------------------
La telemetria NO lleva zDemand, el integral, blend ni etaZ: esos solo salen por
el serial. Pero si lleva servo1/servo2, asi que este script RECONSTRUYE la
geometria con las mismas formulas del firmware y calcula el empuje vertical
real (motor * etaZ), que de otra forma no puedes ver estando el blimp en vuelo.
"""

import argparse
import csv
import glob
import math
import sys
from pathlib import Path

import numpy as np

# ============================================================================
# GEOMETRIA  -  debe coincidir con firmware/src/app/ControlCommon.h
# ============================================================================

SERVO1_Z_DEG = 35.0
SERVO2_Z_DEG = 95.0
# Corregido el 24/08: 85.0 era el valor OBSOLETO de la gondola izquierda
# desalineada (ver BalloonMission.cpp). Con 85 aca la reconstruccion de
# etaZ/empuje vertical quedaba mal para cualquier corrida posterior a esa
# calibracion, aunque la altura cruda se graficara bien.

MODE_NAMES = {
    0: 'SAFE_STOP', 8: 'P00M', 9: 'P00', 10: 'P01', 11: 'P02',
    12: 'P03 yaw', 13: 'P04 altura', 14: 'P05 yaw+altura',
    15: 'P06', 16: 'P07', 17: 'P08', 18: 'P09', 19: 'P10', 20: 'P11',
}

# P08/P09 sumados el 24/08: desde el fix de BalloonMission.cpp que confia
# siempre en altitudeDemand, P08/P09 tienen lazo de altura real igual que
# P10/P11 (antes no lo tenian, por eso no estaban aca).
ALT_MODES = {13, 14, 17, 18, 19, 20}
YAW_MODES = {12, 14}
# P08/P09/P10/P11 tambien cierran yaw, pero su yaw_ref NO es un setpoint fijo:
# se recalcula todo el tiempo (nueva deteccion, nuevo SEARCH, etc.), asi que
# las metricas de escalon (sobrepico, t. establecimiento) no tienen sentido
# ahi -- salen numeros basura si se las calcula igual que en P03/P05. Estos
# modos solo se grafican (linea cruda), sin la tabla de metricas de step.
YAW_PLOT_ONLY_MODES = {17, 18, 19, 20}
OPENLOOP_MODES = {8, 11}      # P00M y P02: sin lazo cerrado de altura


def efficiencies(servo1_deg, servo2_deg):
    """etaZ, etaYaw a partir de los angulos de servo. Igual que el firmware."""
    phi1 = np.radians(servo1_deg - SERVO1_Z_DEG)
    phi2 = np.radians(SERVO2_Z_DEG - servo2_deg)
    eta_z = 0.5 * (np.cos(phi1) + np.cos(phi2))
    eta_yaw = 0.5 * (np.abs(np.sin(phi1)) + np.abs(np.sin(phi2)))
    return np.clip(eta_z, 0.0, 1.0), np.clip(eta_yaw, 0.0, 1.0)


# ============================================================================
# CARGA Y PIVOTE
# ============================================================================

def load_csv(path):
    """Devuelve {nombre_senal: (t_absoluto, valores)} en formato ancho."""
    series = {}

    with open(path, newline='', encoding='utf-8', errors='replace') as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if header is None:
            return series

        for row in reader:
            if len(row) < 2:
                continue
            try:
                t = float(row[0])
            except ValueError:
                continue

            # pares nombre/valor a partir de la columna 2
            for i in range(2, len(row) - 1, 2):
                name = row[i].strip()
                if not name:
                    continue
                try:
                    v = float(row[i + 1])
                except (ValueError, IndexError):
                    continue
                series.setdefault(name, ([], []))
                series[name][0].append(t)
                series[name][1].append(v)

    return {k: (np.asarray(a), np.asarray(b)) for k, (a, b) in series.items()}


def zoh(t_src, v_src, grid):
    """Retencion de orden cero sobre la malla temporal comun."""
    if len(t_src) == 0:
        return np.full(len(grid), np.nan)
    idx = np.searchsorted(t_src, grid, side='right') - 1
    out = np.full(len(grid), np.nan)
    valid = idx >= 0
    out[valid] = v_src[idx[valid]]
    return out


def resample(series, dt=0.05):
    """Alinea todas las senales en una malla comun. Devuelve (t_rel, dict)."""
    all_t = [t for t, _ in series.values() if len(t)]
    if not all_t:
        return np.array([]), {}

    t0 = min(t.min() for t in all_t)
    t1 = max(t.max() for t in all_t)
    if t1 <= t0:
        return np.array([]), {}

    grid = np.arange(t0, t1, dt)
    wide = {name: zoh(t, v, grid) for name, (t, v) in series.items()}
    return grid - t0, wide


def trim_armed(t, wide):
    """Recorta al tramo con actuadores armados. El resto es ruido."""
    armed = wide.get('armed')
    if armed is None or not np.any(armed > 0.5):
        return t, wide, False

    on = np.where(armed > 0.5)[0]
    a, b = on[0], on[-1] + 1
    return t[a:b] - t[a], {k: v[a:b] for k, v in wide.items()}, True


# ============================================================================
# METRICAS DE RESPUESTA AL ESCALON
# ============================================================================

def find_peaks(y, min_prominence):
    """Maximos locales simples. Evita depender de scipy."""
    peaks = []
    for i in range(1, len(y) - 1):
        if y[i] >= y[i - 1] and y[i] > y[i + 1]:
            lo = max(0, i - 20)
            hi = min(len(y), i + 21)
            if y[i] - min(y[lo:hi].min(), y[i]) >= min_prominence:
                peaks.append(i)
    return np.asarray(peaks, dtype=int)


def step_metrics(t, y, ref, band, label, unit):
    """Metricas clasicas + ciclo limite. `band` en las mismas unidades que y."""
    m = {}
    good = np.isfinite(y) & np.isfinite(ref)
    if good.sum() < 10:
        return None

    t, y, ref = t[good], y[good], ref[good]

    target = float(np.median(ref))
    y0 = float(np.median(y[:max(1, len(y) // 50)]))
    span = target - y0

    m['label'] = label
    m['unit'] = unit
    m['setpoint'] = target
    m['inicio'] = y0
    m['salto'] = span
    m['duracion'] = float(t[-1] - t[0])

    # --- tiempo de subida 10-90% ---
    m['t_subida'] = None
    if abs(span) > 3 * band:
        lo = y0 + 0.1 * span
        hi = y0 + 0.9 * span
        cross = (lambda lvl: np.where(
            (y >= lvl) if span > 0 else (y <= lvl))[0])
        i_lo, i_hi = cross(lo), cross(hi)
        if len(i_lo) and len(i_hi) and i_hi[0] >= i_lo[0]:
            m['t_subida'] = float(t[i_hi[0]] - t[i_lo[0]])

    # --- sobrepico ---
    m['sobrepico'] = None
    if abs(span) > 3 * band:
        peak = y.max() if span > 0 else y.min()
        m['sobrepico'] = float((peak - target) / span * 100.0)
        m['pico'] = float(peak)

    # --- tiempo de establecimiento dentro de +/- band ---
    m['t_establec'] = None
    inside = np.abs(y - target) <= band
    if inside.any():
        # ultimo instante en que SALE de la banda
        outside = np.where(~inside)[0]
        if len(outside) == 0:
            m['t_establec'] = 0.0
        elif outside[-1] + 1 < len(t):
            m['t_establec'] = float(t[outside[-1] + 1] - t[0])

    # --- error permanente: ultimo 20% ---
    tail = slice(int(len(y) * 0.8), None)
    m['err_perm'] = float(np.mean(y[tail] - target))
    m['desv_perm'] = float(np.std(y[tail]))

    # --- ciclo limite ---
    #
    # Dos correcciones respecto de versiones anteriores:
    #
    #  1) Buscar picos locales sobre la senal cruda daba periodos absurdos
    #     (0.1-0.5 s) cuando la oscilacion real era de decenas de segundos:
    #     el detector se enganchaba con el ruido del sensor.
    #
    #  2) Mirar solo la segunda mitad y exigir 1.5 ciclos descartaba
    #     justamente las oscilaciones lentas, que son las que importan en un
    #     vehiculo con varios segundos de retardo. Una oscilacion de 58 s en
    #     una ventana de 60 s se reportaba como 30 s.
    #
    # Ahora se usa el ultimo 70% del registro, se exige solo ~1.2 ciclos, y la
    # amplitud se mide como pico-a-pico de la senal suavizada, que es lo que
    # se ve en la grafica, en vez del valor de un bin de la FFT.
    tail_i = int(len(y) * 0.30)
    yh, th = y[tail_i:], t[tail_i:]
    m['ciclo_amp'] = None
    m['ciclo_per'] = None

    if len(yh) > 60:
        det = yh - np.polyval(np.polyfit(th, yh, 1), th)
        span = float(th[-1] - th[0])
        dt_mean = max(float(np.mean(np.diff(th))), 1e-6)

        spec = np.abs(np.fft.rfft(det * np.hanning(len(det))))
        freq = np.fft.rfftfreq(len(det), dt_mean)

        lo = 1.2 / span          # al menos ~1.2 ciclos en la ventana
        hi = 1.0 / 1.5           # nada mas rapido que 1.5 s: eso es ruido
        band_ok = (freq >= lo) & (freq <= hi)

        if band_ok.any():
            idx = np.where(band_ok)[0]
            k = int(idx[np.argmax(spec[idx])])

            # Interpolacion parabolica sobre el pico. Sin esto, una oscilacion
            # de 58 s en una ventana de 84 s se reportaba como 42 s: con pocas
            # ciclos los bines de la FFT quedan muy separados y el maximo cae
            # entre dos.
            f0 = float(freq[k])
            if 0 < k < len(spec) - 1:
                a, b, c = spec[k - 1], spec[k], spec[k + 1]
                den = a - 2.0 * b + c
                if abs(den) > 1e-12:
                    delta = 0.5 * (a - c) / den
                    if abs(delta) < 1.0:
                        f0 = float(freq[k] + delta * (freq[1] - freq[0]))

            if f0 > 0:
                per = 1.0 / f0
                m['ciclo_per'] = per
                # Con menos de 2 ciclos en la ventana el periodo no es
                # estimable con fiabilidad: se marca como cota inferior en
                # vez de dar un numero que parece exacto y no lo es.
                m['ciclo_incierto'] = (per > span / 2.0)
                # suaviza con ventana = periodo/8 para quitar ruido sin
                # aplanar la oscilacion, y mide pico a pico de verdad
                w = max(3, int(per / 8.0 / dt_mean))
                if w < len(det):
                    sm = np.convolve(det, np.ones(w) / w, mode='valid')
                    m['ciclo_amp'] = float(np.ptp(sm))
                else:
                    m['ciclo_amp'] = float(np.ptp(det))

    return m


def print_metrics(name, m):
    if m is None:
        return
    u = m['unit']
    print(f"\n  [{m['label']}]  {name}")
    print(f"    setpoint            {m['setpoint']:+8.3f} {u}"
          f"   (arranca en {m['inicio']:+.3f}, salto {m['salto']:+.3f})")
    if m.get('t_subida') is not None:
        print(f"    tiempo de subida    {m['t_subida']:8.2f} s   (10-90%)")
    else:
        print(f"    tiempo de subida         --       (salto muy pequeno)")
    if m.get('sobrepico') is not None:
        print(f"    sobrepico           {m['sobrepico']:8.1f} %   "
              f"(pico {m['pico']:+.3f} {u})")
    else:
        print(f"    sobrepico                --")
    if m.get('t_establec') is not None:
        print(f"    t. establecimiento  {m['t_establec']:8.2f} s")
    else:
        print(f"    t. establecimiento       --       (nunca entra en banda)")
    print(f"    error permanente    {m['err_perm']:+8.3f} {u}"
          f"   (desv {m['desv_perm']:.3f})")
    if m.get('ciclo_amp') is not None:
        print(f"    ciclo limite        {m['ciclo_amp']:8.3f} {u} pico-a-pico", end='')
        if m.get('ciclo_per'):
            if m.get('ciclo_incierto'):
                print(f", periodo > {m['ciclo_per']/1.5:.0f} s")
                print(f"    {'':20}(menos de 2 ciclos en el registro: alarga "
                      f"--seconds para medirlo bien)")
            else:
                print(f", periodo {m['ciclo_per']:.1f} s")
        else:
            print()
    else:
        print(f"    ciclo limite             --       (sin oscilacion clara)")


def print_power(name, t, wide):
    m1 = wide.get('motor1')
    if m1 is None or not np.any(np.isfinite(m1)):
        return

    s1, s2 = wide.get('servo1'), wide.get('servo2')
    tail = slice(int(len(m1) * 0.6), None)
    seg = m1[tail]
    seg = seg[np.isfinite(seg)]
    if len(seg) == 0:
        return

    print(f"    potencia motor      {np.mean(seg)*100:8.2f} %   "
          f"(media ultimo 40%, pico {np.nanmax(m1)*100:.2f}%)")

    if s1 is not None and s2 is not None:
        eta_z, _ = efficiencies(s1, s2)
        fz = m1 * eta_z
        fzt = fz[tail]
        fzt = fzt[np.isfinite(fzt)]
        if len(fzt):
            print(f"    empuje vertical     {np.mean(fzt)*100:8.2f} %   "
                  f"(motor x etaZ, reconstruido)")


# ============================================================================
# LAZO ABIERTO  (P00M)  -  FLOTABILIDAD Y GANANCIA DE PLANTA
#
# En P00M no hay setpoint, asi que las metricas de escalon no aplican. Lo que
# importa es otra cosa: a potencia constante, ¿el blimp sube, baja o se queda?
#
# La DERIVA (pendiente de altura contra tiempo) es la medida directa. Donde
# cruza cero esta la flotabilidad neutra.
#
# Y la pendiente de (deriva contra potencia) es la GANANCIA DE PLANTA: cuantos
# m/s de ascenso ganas por cada unidad de potencia. Ese numero es el que
# determina tu kp de altura.
# ============================================================================

def segment_by_power(t, p, min_dur=5.0, tol=0.002):
    """Bloques contiguos de potencia constante y no nula."""
    segs = []
    if p is None:
        return segs
    n = len(p)
    i = 0
    while i < n:
        if not np.isfinite(p[i]) or p[i] <= 1e-6:
            i += 1
            continue
        j = i
        while j < n and np.isfinite(p[j]) and abs(p[j] - p[i]) <= tol:
            j += 1
        if t[j - 1] - t[i] >= min_dur:
            segs.append((i, j, float(np.median(p[i:j]))))
        i = j
    return segs


def analyze_openloop(name, t, wide, skip_frac=0.30, min_dur=5.0):
    """Deriva por nivel de potencia + estimacion de hover y ganancia."""
    h = wide.get('height')
    p = wide.get('motor1')
    vz = wide.get('vertical_velocity')
    bat = wide.get('battery')

    if h is None or p is None:
        print('    (falta height o motor1: no se puede analizar)')
        return []

    segs = segment_by_power(t, p, min_dur=min_dur)
    if not segs:
        print(f'    (ningun tramo de potencia constante de mas de {min_dur:.0f}s)')
        return []

    print(f'\n  [LAZO ABIERTO]  {name}')
    print(f'    {"potencia":>9}  {"deriva":>12}  {"altura ini->fin":>17}  '
          f'{"bateria":>8}   veredicto')

    rows = []
    heights = []
    for (a, b, lvl) in segs:
        k = a + int(skip_frac * (b - a))     # descarta el transitorio inicial
        if b - k < 5:
            continue
        tt, hh = t[k:b], h[k:b]
        ok = np.isfinite(tt) & np.isfinite(hh)
        if ok.sum() < 5:
            continue
        drift = float(np.polyfit(tt[ok], hh[ok], 1)[0])

        hh_ok = hh[ok]
        h_ini, h_fin = float(hh_ok[0]), float(hh_ok[-1])

        if abs(drift) < 0.005:
            verdict = 'deriva ~0'
        elif drift > 0:
            verdict = 'sube'
        else:
            verdict = 'baja'

        bv = np.nan
        if bat is not None:
            bs = bat[k:b]
            bs = bs[np.isfinite(bs)]
            if len(bs):
                bv = float(np.mean(bs))

        bstr = f'{bv:7.2f}V' if np.isfinite(bv) else '     --'
        print(f'    {lvl*100:8.2f}%  {drift:+9.4f} m/s  '
              f'{h_ini:+7.2f} ->{h_fin:+7.2f} m  {bstr}   {verdict}')
        rows.append((lvl, drift, float(t[b-1]-t[a]), name,
                     float(0.5*(t[a]+t[b-1])), bv))
        heights.append((lvl, h_ini, h_fin))

    if len(rows) < 2:
        return rows

    # ------------------------------------------------------------------
    # AVISO DE SUELO / TECHO
    #
    # Un tramo con deriva ~0 A LA MISMA ALTURA que otro tramo de potencia
    # distinta casi siempre significa que el vehiculo estaba APOYADO, no
    # flotando. Se ve como un hover falso y arruina la estimacion.
    # ------------------------------------------------------------------
    flat = [(lvl, hi, hf) for (lvl, hi, hf) in heights if abs(hf - hi) < 0.06]
    if len(flat) >= 2:
        alturas = [0.5 * (hi + hf) for _, hi, hf in flat]
        if max(alturas) - min(alturas) < 0.10:
            niveles = ', '.join(f'{l*100:.1f}%' for l, _, _ in flat)
            print(f'\n    AVISO: los niveles {niveles} quedaron quietos a la MISMA')
            print(f'    altura (~{np.mean(alturas):+.2f} m). Con potencias distintas eso')
            print(f'    normalmente significa que el blimp estaba APOYADO en el suelo')
            print(f'    (o pegado al techo), no flotando. Repite soltandolo desde')
            print(f'    media altura para que tenga espacio hacia ARRIBA y hacia ABAJO.')

    # ------------------------------------------------------------------
    # GANANCIAS LOCALES: revelan no linealidad que el ajuste global esconde
    # ------------------------------------------------------------------
    if len(rows) >= 3:
        rs = sorted(rows)
        locs = []
        print(f'\n    ganancia local entre niveles consecutivos:')
        for i in range(len(rs) - 1):
            dP = rs[i+1][0] - rs[i][0]
            if dP <= 0:
                continue
            g = (rs[i+1][1] - rs[i][1]) / dP
            locs.append(g)
            print(f'      {rs[i][0]*100:.1f}% -> {rs[i+1][0]*100:.1f}%   '
                  f'{g:+7.2f} (m/s por unidad de potencia)')
        if locs and max(locs) > 0 and (min(locs) < 0.25 * max(locs)):
            print(f'\n      Las ganancias locales difieren mucho entre si. El ajuste')
            print(f'      global promedia tramos que no son comparables: fiate mas de')
            print(f'      la pendiente CERCA del hover que del numero global.')

    estimate_buoyancy(rows, prefix='    ')
    return rows


# ----------------------------------------------------------------------------
# ESTIMACION DE FLOTABILIDAD  -  sirve para un archivo o para varios juntos
# ----------------------------------------------------------------------------

def time_drift_estimate(rows):
    """Deriva atribuible al TIEMPO, no a la potencia.

    Solo es separable si algun nivel de potencia se repite en instantes
    distintos: la diferencia entre esas dos medidas del MISMO nivel es, por
    definicion, efecto del tiempo. De ahi la utilidad de barrer subiendo y
    volviendo a bajar.
    """
    from collections import defaultdict
    by = defaultdict(list)
    for r in rows:
        if len(r) >= 5:
            by[round(r[0], 5)].append(r)

    slopes = []
    for lvl, rs in by.items():
        if len(rs) < 2:
            continue
        rs = sorted(rs, key=lambda r: r[4])
        for i in range(len(rs) - 1):
            dt = rs[i + 1][4] - rs[i][4]
            if dt > 1.0:
                slopes.append((rs[i + 1][1] - rs[i][1]) / dt)
    return float(np.mean(slopes)) if slopes else None


def estimate_buoyancy(rows, prefix='  ', cross_run=False):
    if len(rows) < 2:
        return

    pad = prefix
    rows = sorted(rows, key=lambda r: r[0])

    # ------------------------------------------------------------------
    # CORRECCION POR DERIVA TEMPORAL
    #
    # Si algun nivel se repite en dos instantes, la diferencia entre ambos
    # mide el efecto del TIEMPO (bateria cayendo, helio, temperatura). Se
    # descuenta antes de estimar la ganancia.
    # ------------------------------------------------------------------
    tslope = time_drift_estimate(rows)
    if tslope is not None and abs(tslope) > 1e-6 and len(rows[0]) >= 5:
        tref = float(np.mean([r[4] for r in rows]))
        print(f'\n{pad}DERIVA TEMPORAL detectada: {tslope*3600:+.2f} (cm/s) por hora'
              .replace('cm/s', 'm/s'))
        print(f'{pad}  medida repitiendo un mismo nivel de potencia en dos instantes.')
        print(f'{pad}  Se descuenta antes de calcular la ganancia.')
        rows = [(r[0], r[1] - tslope * (r[4] - tref)) + tuple(r[2:]) for r in rows]

    P = np.array([r[0] for r in rows])
    D = np.array([r[1] for r in rows])

    gain = float(np.polyfit(P, D, 1)[0])
    pad = prefix

    # ------------------------------------------------------------------
    # GANANCIA NEGATIVA = IMPOSIBLE FISICAMENTE
    #
    # El empuje apunta hacia arriba: mas potencia NO puede hacer descender.
    # Si sale negativa, la flotabilidad cambio ENTRE las corridas (fuga de
    # helio, temperatura, lastre movido), no es un problema de medicion.
    # ------------------------------------------------------------------
    if gain <= 1e-6:
        print(f'\n{pad}GANANCIA DE PLANTA  {gain:8.3f}  <-- NEGATIVA O NULA')
        print(f'{pad}  Fisicamente imposible: mas potencia no puede hacer bajar')
        print(f'{pad}  el blimp. Causa mas comun con diferencia: el vehiculo TOCO')
        print(f'{pad}  el techo o el suelo y lo que sigue es el rebote, no vuelo')
        print(f'{pad}  libre. Revisa la columna de altura tramo por tramo.')
        print(f'{pad}  Otras causas: bateria cayendo, helio, temperatura.')
        if tslope is None:
            print(f'\n{pad}  En un barrido ascendente la potencia y el tiempo suben')
            print(f'{pad}  JUNTOS, asi que los dos efectos son indistinguibles.')
            print(f'{pad}  SOLUCION: barre subiendo y volviendo a bajar, para que')
            print(f'{pad}  cada nivel se mida dos veces en instantes distintos:')
            print(f'{pad}    --power-sweep 9.4:10:0.2 --sweep-updown --motor-seconds 45')
            print(f'{pad}  Revisa tambien la columna de bateria: si cae a lo largo')
            print(f'{pad}  del ensayo, el mismo PWM entrega menos empuje.')
        span = float(np.max(np.abs(D)))
        print(f'\n{pad}  Aun asi, TODAS las derivas son menores a {span*100:.1f} cm/s.')
        print(f'{pad}  Tu hover esta entre {P.min()*100:.1f}% y {P.max()*100:.1f}%,')
        print(f'{pad}  y la flotabilidad se mueve sola en ese mismo margen.')
        print(f'{pad}  Eso es exactamente para lo que sirve el termino integral:')
        print(f'{pad}  NO pongas --ki 0 en P04/P05.')
        return

    # --- hover: donde la deriva cruza cero ---
    hover = None
    extrap = ''
    for i in range(len(rows) - 1):
        if D[i] <= 0.0 <= D[i + 1] and D[i + 1] != D[i]:
            hover = P[i] + (P[i + 1] - P[i]) * (-D[i]) / (D[i + 1] - D[i])
            break

    if hover is None:
        cross = -float(np.polyfit(P, D, 1)[1]) / gain
        if P.min() - 0.03 <= cross <= P.max() + 0.03:
            hover, extrap = cross, ' (extrapolado)'

    print()
    if hover is not None:
        print(f'{pad}POTENCIA DE HOVER   {hover*100:8.2f} %{extrap}')
        print(f'{pad}  -> usala en  --yaw-base-power {hover*100:.1f}')
        print(f'{pad}  -> el integral de altura debe poder llegar ahi: '
              f'--max-power >= {hover*100+5:.0f}')
    else:
        print(f'{pad}POTENCIA DE HOVER        --   (la deriva no cruza cero)')
        if D.min() > 0:
            print(f'{pad}  todos los niveles SUBEN: prueba potencias mas bajas')
        elif D.max() < 0:
            print(f'{pad}  todos los niveles BAJAN: prueba potencias mas altas')

    print(f'\n{pad}GANANCIA DE PLANTA  {gain:8.3f} (m/s) por unidad de potencia')
    print(f'{pad}  = {gain/100.0:.4f} m/s por cada 1% de potencia extra')

    kp = 0.20 / gain
    print(f'\n{pad}  kp sugerido de partida: {kp:.3f}')
    print(f'{pad}  (kp = velocidad_deseada / ganancia, por metro de error)')
    print(f'{pad}  El integral aporta el hover; kp solo aporta el EXCESO.')


# ============================================================================
# GRAFICAS
# ============================================================================

def plot_altitude(runs, band_m, save, show):
    import matplotlib.pyplot as plt

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(11, 7), sharex=True,
                                   gridspec_kw={'height_ratios': [2, 1]})

    ref_drawn = False
    for name, t, wide, color in runs:
        h = wide.get('height')
        if h is None:
            continue
        ax1.plot(t, h, color=color, lw=1.4, label=name)

        ref = wide.get('height_ref')
        if ref is not None and not ref_drawn:
            ax1.plot(t, ref, 'k--', lw=1.2, label='referencia')
            target = float(np.nanmedian(ref))
            ax1.axhspan(target - band_m, target + band_m,
                        color='k', alpha=0.07, lw=0)
            ref_drawn = True

        m1 = wide.get('motor1')
        if m1 is not None:
            ax2.plot(t, m1 * 100.0, color=color, lw=1.2, label=f'{name} motor')
            s1, s2 = wide.get('servo1'), wide.get('servo2')
            if s1 is not None and s2 is not None:
                eta_z, _ = efficiencies(s1, s2)
                ax2.plot(t, m1 * eta_z * 100.0, color=color, lw=1.0,
                         ls=':', alpha=0.8)

    ax1.set_ylabel('altura [m]')
    ax1.set_title('Respuesta de altura'
                  f'   (banda sombreada = ±{band_m*100:.0f} cm)')
    ax1.grid(alpha=0.3)
    ax1.legend(fontsize=8, ncol=2)

    ax2.set_ylabel('potencia [%]')
    ax2.set_xlabel('tiempo desde ARM [s]')
    ax2.grid(alpha=0.3)
    ax2.set_title('Potencia de motor (linea) y empuje vertical motor×etaZ (punteada)',
                  fontsize=9)

    fig.tight_layout()
    _finish(fig, 'altura', save, show)


def plot_yaw(runs, band_deg, save, show):
    import matplotlib.pyplot as plt

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(11, 7), sharex=True,
                                   gridspec_kw={'height_ratios': [2, 1]})

    ref_drawn = False
    for name, t, wide, color in runs:
        y = wide.get('yaw')
        if y is None:
            continue
        ydeg = np.degrees(np.unwrap(y))
        ax1.plot(t, ydeg, color=color, lw=1.4, label=name)

        ref = wide.get('yaw_ref')
        if ref is not None and not ref_drawn:
            rdeg = np.degrees(ref)
            ax1.plot(t, rdeg, 'k--', lw=1.2, label='referencia')
            target = float(np.nanmedian(rdeg))
            ax1.axhspan(target - band_deg, target + band_deg,
                        color='k', alpha=0.07, lw=0)
            ref_drawn = True

        s1, s2 = wide.get('servo1'), wide.get('servo2')
        if s1 is not None:
            ax2.plot(t, s1, color=color, lw=1.2, alpha=0.9)
        if s2 is not None:
            ax2.plot(t, s2, color=color, lw=1.2, ls='--', alpha=0.9)

    ax1.set_ylabel('yaw [grados]')
    ax1.set_title(f'Respuesta de yaw   (banda sombreada = ±{band_deg:.0f}°)')
    ax1.grid(alpha=0.3)
    ax1.legend(fontsize=8, ncol=2)

    ax2.axhline(SERVO1_Z_DEG, color='gray', lw=0.8, alpha=0.6)
    ax2.axhline(SERVO2_Z_DEG, color='gray', lw=0.8, alpha=0.6)
    ax2.set_ylabel('servo [grados]')
    ax2.set_xlabel('tiempo desde ARM [s]')
    ax2.grid(alpha=0.3)
    ax2.set_title('Servo 1 (solida) y Servo 2 (punteada); '
                  'lineas grises = vector vertical 35/85', fontsize=9)

    fig.tight_layout()
    _finish(fig, 'yaw', save, show)


def _finish(fig, stem, save, show):
    import matplotlib.pyplot as plt
    if save:
        out = Path(save)
        out.mkdir(parents=True, exist_ok=True)
        p = out / f'{stem}.png'
        fig.savefig(p, dpi=140)
        print(f"\n  Guardado: {p}")
    if not show:
        plt.close(fig)


# ============================================================================
# MAIN
# ============================================================================

def main():
    ap = argparse.ArgumentParser(
        description='Graficas y metricas de los logs del blimp',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
EJEMPLOS

  python analyze.py logs/blimp_20260823_101500.csv
  python analyze.py logs/blimp_2026082*.csv --save figs/
  python analyze.py logs/*.csv --no-show          (solo metricas en consola)

Para comparar sintonias, corre la misma prueba varias veces y pasa todos los
CSV juntos: se superponen con un color por corrida.
""")
    ap.add_argument('csv', nargs='+', help='uno o mas CSV (admite comodines)')
    ap.add_argument('--band-cm', type=float, default=5.0,
                    help='banda de establecimiento de altura en cm; default 5')
    ap.add_argument('--band-deg', type=float, default=5.0,
                    help='banda de establecimiento de yaw en grados; default 5')
    ap.add_argument('--dt', type=float, default=0.05,
                    help='paso de la malla temporal comun en s; default 0.05')
    ap.add_argument('--save', default=None, help='carpeta donde guardar los PNG')
    ap.add_argument('--no-show', action='store_true',
                    help='no abrir ventanas; util para procesar en lote')
    ap.add_argument('--no-trim', action='store_true',
                    help='no recortar al tramo armado')
    args = ap.parse_args()

    # run_test.py guarda en <raiz_del_proyecto>/logs, que esta un nivel ARRIBA
    # de groundstation/. Como analyze.py normalmente se ejecuta desde
    # groundstation/, 'logs\\*.csv' no resuelve. Se intenta tambien relativo a
    # la raiz del proyecto y a la carpeta del script, para que funcione desde
    # cualquiera de los dos sitios.
    here = Path(__file__).resolve().parent
    roots = [Path.cwd(), here, here.parent]

    paths = []
    for pat in args.csv:
        hits = []
        for root in roots:
            hits = sorted(glob.glob(str(root / pat)))
            if hits:
                break
        paths.extend(hits if hits else [pat])

    paths = [p for p in paths if Path(p).is_file()]
    if not paths:
        tried = '\n'.join(f'    {r}' % () for r in roots)
        raise SystemExit(
            'No se encontro ningun CSV.\n'
            f'  Patron: {" ".join(args.csv)}\n'
            '  Se busco en:\n' + tried + '\n\n'
            '  run_test.py guarda los logs en <proyecto>/logs, un nivel arriba\n'
            '  de groundstation/. Prueba con:  python analyze.py ..\\logs\\blimp_*.csv')

    try:
        import matplotlib
        if args.no_show:
            matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        raise SystemExit(
            'Falta matplotlib.  Instala con:  pip install matplotlib numpy')

    cmap = plt.get_cmap('tab10')
    runs = []
    modes = set()
    openloop_rows = []

    print('=' * 74)
    print('METRICAS')
    print('=' * 74)

    for i, p in enumerate(paths):
        series = load_csv(p)
        if not series:
            print(f'\n  {Path(p).name}: vacio o ilegible, se omite.')
            continue

        t, wide = resample(series, args.dt)
        if len(t) == 0:
            print(f'\n  {Path(p).name}: sin muestras utiles, se omite.')
            continue

        trimmed = False
        if not args.no_trim:
            t, wide, trimmed = trim_armed(t, wide)

        mode = None
        if 'mode' in wide and np.any(np.isfinite(wide['mode'])):
            vals = wide['mode'][np.isfinite(wide['mode'])]
            if len(vals):
                mode = int(round(float(np.median(vals))))
                modes.add(mode)

        name = Path(p).stem.replace('blimp_', '')
        mname = MODE_NAMES.get(mode, f'modo {mode}' if mode is not None else '?')

        print(f'\n{"-"*74}\n  {Path(p).name}   [{mname}]   '
              f'{t[-1]-t[0]:.1f} s'
              f'{"  (recortado a ARM)" if trimmed else "  (sin recorte)"}')

        if mode in ALT_MODES or ('height_ref' in wide and mode is None):
            m = step_metrics(t, wide.get('height'), wide.get('height_ref'),
                             args.band_cm / 100.0, 'ALTURA', 'm')
            print_metrics(name, m)
            print_power(name, t, wide)

        if mode in YAW_MODES:
            y = wide.get('yaw')
            r = wide.get('yaw_ref')
            if y is not None and r is not None:
                m = step_metrics(t, np.degrees(np.unwrap(y)), np.degrees(r),
                                 args.band_deg, 'YAW', 'deg')
                print_metrics(name, m)
                if mode == 12:
                    print_power(name, t, wide)
        elif mode in YAW_PLOT_ONLY_MODES:
            print('    [YAW] referencia dinamica de mision (se recalcula '
                  'constantemente): ver grafico, metricas de escalon no aplican aqui.')

        if mode in OPENLOOP_MODES:
            openloop_rows.extend(analyze_openloop(name, t, wide) or [])

        if mode not in ALT_MODES and mode not in YAW_MODES \
                and mode not in OPENLOOP_MODES:
            print('    (modo sin lazo cerrado: no hay metricas de escalon)')

        runs.append((name, t, wide, cmap(i % 10)))

    if not runs:
        raise SystemExit('\nNada que graficar.')

    # ------------------------------------------------------------------
    # Combinado: niveles de potencia de TODOS los archivos juntos.
    # Sin esto, dos corridas de un nivel cada una nunca se comparan.
    # ------------------------------------------------------------------
    levels = sorted({round(r[0], 5) for r in openloop_rows})
    if len(levels) >= 2:
        print('\n' + '=' * 74)
        print('FLOTABILIDAD  -  todos los niveles juntos')
        print('=' * 74)
        print(f'  {"potencia":>9}  {"t":>7}  {"deriva":>12}   origen')
        for r in sorted(openloop_rows):
            tm = r[4] if len(r) >= 5 else float('nan')
            print(f'  {r[0]*100:8.2f}%  {tm:6.0f}s  {r[1]:+9.4f} m/s   {r[3]}')
        estimate_buoyancy(openloop_rows, prefix='  ', cross_run=True)

    print('\n' + '=' * 74)

    want_alt = any(m in (ALT_MODES | OPENLOOP_MODES) for m in modes) or not modes
    want_yaw = any(m in YAW_MODES or m in YAW_PLOT_ONLY_MODES for m in modes)

    if want_alt and any('height' in w for _, _, w, _ in runs):
        plot_altitude(runs, args.band_cm / 100.0, args.save, not args.no_show)
    if want_yaw and any('yaw' in w for _, _, w, _ in runs):
        plot_yaw(runs, args.band_deg, args.save, not args.no_show)

    if not args.no_show:
        plt.show()


if __name__ == '__main__':
    main()