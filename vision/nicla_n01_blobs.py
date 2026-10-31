"""N01 v3 - Nicla Vision / OpenMV, diagnostico USB sin UART ni actuadores.

Muestra regiones rojas independientes. C1/C2 son indices DEL FOTOGRAMA,
no identidades de globos. Las dimensiones son del segmento de color, no
distancias ni garantia del contorno completo del globo. NO usar en vuelo.
Ejecutar desde OpenMV IDE antes de considerar guardar como main.py.
"""
import sensor
import time

VERSION = "N01-v3.1"
# Umbral LAB INICIAL, no calibrado. No equivale al detector gaussiano anterior.
# Ajustar con Tools > Machine Vision > Threshold Editor bajo luz de prueba.
RED_LAB = (2, 80, 20, 80, -5, 70)
MIN_PIXELS = 25
MIN_BOX_AREA = 36
MIN_SIDE = 4
# Filtro experimental para candidatos actuales; referencia v2.1 intacta.
MIN_TARGET_SIDE = 20
MIN_FILL = 0.15
MIN_ASPECT = 0.65
MAX_ASPECT = 1.45
# Valores iniciales de ensayo: calibrar con globos reales y reflejos.
MIN_ROUNDNESS = 0.45
MIN_CIRCULARITY = 0.35
MAX_DISPLAY = 8
LOG_INTERVAL_MS = 200
# Alterna COLOR/MASK cada 2 s; 4 s por pareja. Misma deteccion RGB en ambos.
VIEW_PERIOD_MS = 2000


def read_field(obj, name):
    """OpenMV 5 usa atributos para Blob; admite tambien metodos antiguos."""
    value = getattr(obj, name)
    return value() if callable(value) else value


def metric(blob, name):
    """Compatible con metodos antiguos y atributos nuevos de OpenMV."""
    try:
        value = float(read_field(blob, name))
        return value if 0 <= value < 1000000 else -1.0
    except (AttributeError, TypeError, ValueError, NotImplementedError):
        return -1.0


def candidates(img):
    """Conserva tambien rechazados para diagnostico, sin memoria ni fusion."""
    blobs = img.find_blobs(
        [RED_LAB], x_stride=1, y_stride=1,
        pixels_threshold=MIN_PIXELS, area_threshold=MIN_BOX_AREA,
        merge=False,
    )
    result = []
    for blob in blobs:
        x, y, w, h = read_field(blob, "rect")
        pixels = read_field(blob, "pixels")
        if w <= 0 or h <= 0:
            continue
        aspect = w / h
        fill = pixels / (w * h)
        clipped = int(x <= 0 or y <= 0 or x + w >= read_field(img, "width")
                      or y + h >= read_field(img, "height"))
        roundness = metric(blob, "roundness")
        perimeter = metric(blob, "perimeter")
        # Area segmentada (pixels), NO el area rectangular w*h.
        circularity = (4 * 3.141592653589793 * pixels / (perimeter * perimeter)
                       if perimeter > 0 else -1.0)
        reasons = []
        if w < MIN_SIDE or h < MIN_SIDE:
            reasons.append("SIZE")
        if fill < MIN_FILL:
            reasons.append("FILL")
        if not MIN_ASPECT <= aspect <= MAX_ASPECT:
            reasons.append("AR")
        if clipped:
            reasons.append("EDGE")
        if roundness < 0 or roundness > 1 or circularity < 0:
            reasons.append("METRIC")
        else:
            if roundness < MIN_ROUNDNESS:
                reasons.append("R")
            if circularity < MIN_CIRCULARITY:
                reasons.append("C")
        reason = "+".join(reasons) if reasons else "OK"
        # F: completo plausible, P: parcial, D: dudoso, X: descarte basico.
        # C deja de ser veto SOLO para esta comparacion USB; no es un objetivo.
        if w < MIN_TARGET_SIDE or h < MIN_TARGET_SIDE:
            state = "X"
        elif "SIZE" in reasons or "FILL" in reasons:
            state = "X"
        elif "METRIC" in reasons:
            state = "D"
        elif clipped:
            state = "P"
        elif "AR" in reasons or "R" in reasons:
            state = "D"
        else:
            state = "F"
        result.append((x, y, w, h, read_field(blob, "cx"), read_field(blob, "cy"), pixels, clipped,
                       aspect, fill, roundness, circularity, int(state == "F"),
                       reason, state, int(not reasons)))
    # Indices solo por fotograma: NO seleccion ni seguimiento persistente.
    result.sort(key=lambda item: (item[0], item[1]))
    return result


def counts(detections):
    return tuple(sum(item[14] == state for item in detections)
                 for state in ("F", "P", "D", "X"))


def annotate(img, detections):
    colors = {"F": (0, 255, 0), "P": (0, 255, 255),
              "D": (255, 0, 255), "X": (255, 255, 0)}
    for i, item in enumerate(detections[:MAX_DISPLAY]):
        x, y, w, h, cx, cy = item[:6]
        color = colors[item[14]]
        img.draw_rectangle((x, y, w, h), color=color)
        img.draw_cross((cx, cy), color=color, size=4)
        # Etiquetas cortas; medidas y motivos completos quedan en terminal.
        label = "%d%s%s" % (i + 1, item[14], "!" if "C" in item[13].split("+") else "")
        tx = max(0, min(x, read_field(img, "width") - len(label) * 8))
        ty = max(12, min(y, read_field(img, "height") - 10))
        img.draw_string((tx, ty), label, color=color)
    f, p, d, x = counts(detections)
    img.draw_string((0, 0), "V3 F%d P%d D%d X%d S%d" %
                    (f, p, d, x, min(len(detections), MAX_DISPLAY)),
                    color=(255, 255, 255))


def render_view(img, detections, phase):
    # Detectar y medir ANTES de dibujar o binarizar, siempre sobre RGB.
    if phase:
        img.binary([RED_LAB])  # Mascara cruda: sin textos, filtros ni fusiones.
        return "MASK"
    annotate(img, detections)
    return "COLOR"


def log_frame(frame, timestamp, detections, view="COLOR"):
    shown = detections[:MAX_DISPLAY]
    f, p, d, x = counts(detections)
    strict = sum(item[15] for item in detections)
    print("N01V3_FRAME,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d" %
          (frame, timestamp, view, len(detections), f, p, d, x, strict, len(shown)))
    for i, item in enumerate(shown):
        print("N01V3_BLOB,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%d,%s,%s,%d" %
              ((frame, timestamp, i + 1) + item))


def main():
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.HQVGA)  # 240 x 160, igual resolucion que original.
    sensor.skip_frames(time=1500)
    # GC2145 de Nicla no soporta set_auto_gain: no llamar ni afirmar
    # que la ganancia queda fijada. Se mantiene exposicion automatica.
    sensor.set_auto_whitebal(False)
    sensor.skip_frames(time=300)
    print("N01_CONFIG,%s,LAB=%s,pixels=%d,area=%d,fill=%.2f,aspect=%.2f:%.2f" %
          (VERSION, str(RED_LAB), MIN_PIXELS, MIN_BOX_AREA, MIN_FILL,
           MIN_ASPECT, MAX_ASPECT))
    print("N01_SHAPE,roundness_min=%.2f,circularity_min=%.2f" %
          (MIN_ROUNDNESS, MIN_CIRCULARITY))
    print("N01: SOLO USB. Sin IBUS. C1/C2 NO son IDs persistentes.")
    print("N01_DIAG,circularity_veto=OFF,view_period_ms=%d" % VIEW_PERIOD_MS)
    print("N01_SIZE,min_width=%d,min_height=%d; menores=X; reason_v21 conserva referencia antigua" %
          (MIN_TARGET_SIDE, MIN_TARGET_SIDE))
    print("N01V3_FRAME,frame,uptime_ms,view,total,full,partial,doubt,rejected,v21_accepted,emitted")
    print("N01V3_BLOB,frame,uptime_ms,index,x,y,w,h,cx,cy,pixels,clipped,aspect,fill,roundness,circularity,full,reason_v21,state,v21_accepted")
    frame = 0
    last_log = time.ticks_ms()
    last_view = last_log
    phase = 0
    previous_view = None
    while True:
        img = sensor.snapshot()
        timestamp = time.ticks_ms()
        detections = candidates(img)
        if time.ticks_diff(timestamp, last_view) >= VIEW_PERIOD_MS:
            phase = 1 - phase
            last_view = timestamp
        view = render_view(img, detections, phase)
        if view != previous_view:
            print("N01_VIEW,%d,%s" % (timestamp, view))
            previous_view = view
        if frame == 0 or time.ticks_diff(timestamp, last_log) >= LOG_INTERVAL_MS:
            log_frame(frame, timestamp, detections, view)
            last_log = timestamp
        frame += 1


if __name__ == "__main__":
    main()
