"""N02 v1 - Nicla Vision / OpenMV, diagnostico USB sin UART ni actuadores.

Muestra regiones rojas independientes. C1/C2 son indices DEL FOTOGRAMA,
no identidades de globos. Las dimensiones son del segmento de color, no
distancias ni garantia del contorno completo del globo. NO usar en vuelo.
Ejecutar desde OpenMV IDE antes de considerar guardar como main.py.
"""
import sensor
import time

VERSION = "N02-v1-fluido"
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
LOG_INTERVAL_MS = 1000
VERBOSE_BLOBS = False
# COLOR, MASK o AUTO. MASK muestra la mascara cruda usada para calibrar.
VIEW_MODE = "COLOR"
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



# N02: asociacion geometrica experimental, sin IDs de competencia.
TRACK_CONFIRM_MS = 300
TRACK_CONFIRM_FRAMES = 5
TRACK_LOST_MS = 2000

class Tracker:
    def __init__(self):
        self.anchor = None
        self.pending = None
        self.pending_ms = 0
        self.hits = 0
        self.last_seen = 0
        self.current = None
        self.state = "SEARCH"
        self.locked = False
        self.expired = False

    def compatible(self, old, new):
        gate = min(50, max(25, max(old[2], old[3]) * 0.5))
        if (new[4]-old[4])**2 + (new[5]-old[5])**2 > gate**2:
            return False
        low = 0.25 if new[14] == "P" or old[14] == "P" else 0.5
        return all(low <= new[k] / old[k] <= 2.0 for k in (2, 3))

    def update(self, rows, now):
        self.current = None
        if self.expired:
            return
        if not self.locked:
            full = sorted((r for r in rows if r[14] == "F"),
                          key=lambda r: r[2]*r[3], reverse=True)
            if not full:
                self.pending = None
                self.hits = 0
                self.state = "SEARCH"
                return
            candidate = full[0]
            # Tamano casi igual: no decidir arbitrariamente entre dos.
            if len(full) > 1 and full[1][2]*full[1][3] >= candidate[2]*candidate[3]*0.9:
                self.pending = None
                self.hits = 0
                self.state = "AMBIG"
                return
            if self.pending is None or not self.compatible(self.pending, candidate):
                self.pending_ms = now
                self.hits = 0
            self.pending = candidate
            self.hits += 1
            self.state = "CONFIRM"
            if self.hits >= TRACK_CONFIRM_FRAMES and time.ticks_diff(now, self.pending_ms) >= TRACK_CONFIRM_MS:
                self.locked = True
                self.anchor = candidate
                self.last_seen = now
                self.current = candidate
                self.state = "FULL"
            return
        # No recuperar automaticamente una identidad despues del timeout.
        if time.ticks_diff(now, self.last_seen) > TRACK_LOST_MS:
            self.expired = True
            self.state = "LOST"
            return
        matches = [r for r in rows if r[14] in ("F", "P") and self.compatible(self.anchor, r)]
        if len(matches) != 1:
            self.state = "AMBIG" if matches else "MISSING"
            return
        self.current = matches[0]
        self.anchor = self.current
        self.last_seen = now
        self.state = "FULL" if self.current[14] == "F" else "PARTIAL"

    def log(self, frame, now):
        r = self.current
        # -1: sin medida actual; nunca reutilizar coordenadas antiguas.
        values = r[:6] if r is not None else (-1,)*6
        age = time.ticks_diff(now, self.last_seen) if self.locked else -1
        print("N02_TRACK,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d" %
              ((frame, now, int(self.locked), self.state, int(r is not None)) + values + (age,)))

    def draw(self, img):
        img.draw_string((0, 148), "T%d %s" % (int(self.locked), self.state),
                        color=(255, 255, 255))
        if self.current is not None:
            r = self.current
            img.draw_cross((r[4], r[5]), color=(255, 255, 255), size=8)


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
    print("N02_TRACK,frame,uptime_ms,track_id,state,observed,x,y,w,h,cx,cy,age_ms")
    print("N02: mayor F inicial; confirmacion 300ms/5frames; LOST>%dms requiere reiniciar. Solo USB." % TRACK_LOST_MS)
    tracker = Tracker()
    frame = 0
    last_log = time.ticks_ms()
    last_view = last_log
    phase = 0
    previous_view = None
    last_state = None
    perf_start = time.ticks_ms()
    perf_frames = 0
    perf_capture = 0
    perf_detect = 0
    perf_render = 0
    previous_loop = time.ticks_ms()
    max_loop_ms = 0
    while True:
        loop_start = time.ticks_ms()
        max_loop_ms = max(max_loop_ms, time.ticks_diff(loop_start, previous_loop))
        previous_loop = loop_start
        img = sensor.snapshot()
        timestamp = time.ticks_ms()
        detections = candidates(img)
        tracker.update(detections, timestamp)
        detected_at = time.ticks_ms()
        if time.ticks_diff(timestamp, last_view) >= VIEW_PERIOD_MS:
            phase = 1 - phase
            last_view = timestamp
        selected_phase = phase if VIEW_MODE == "AUTO" else int(VIEW_MODE == "MASK")
        view = render_view(img, detections, selected_phase)
        if view == "COLOR":
            tracker.draw(img)
        if view != previous_view:
            print("N01_VIEW,%d,%s" % (timestamp, view))
            previous_view = view
        rendered_at = time.ticks_ms()
        perf_frames += 1
        perf_capture += time.ticks_diff(timestamp, loop_start)
        perf_detect += time.ticks_diff(detected_at, timestamp)
        perf_render += time.ticks_diff(rendered_at, detected_at)
        if frame == 0 or time.ticks_diff(timestamp, last_log) >= LOG_INTERVAL_MS:
            if VERBOSE_BLOBS:
                log_frame(frame, timestamp, detections, view)
            tracker.log(frame, timestamp)
            elapsed = max(1, time.ticks_diff(rendered_at, perf_start))
            print("N02_PERF,fps=%.1f,capture_ms=%.1f,detect_ms=%.1f,render_ms=%.1f,max_loop_ms=%d" %
                  (perf_frames * 1000 / elapsed, perf_capture / perf_frames,
                   perf_detect / perf_frames, perf_render / perf_frames, max_loop_ms))
            perf_start = rendered_at
            perf_frames = perf_capture = perf_detect = perf_render = 0
            max_loop_ms = 0
            last_log = timestamp
        elif tracker.state != last_state:
            tracker.log(frame, timestamp)
        last_state = tracker.state
        frame += 1


if __name__ == "__main__":
    main()
