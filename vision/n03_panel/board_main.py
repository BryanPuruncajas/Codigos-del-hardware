"""Se concatena en RAM tras core y mission. No instalar como main.py."""
import pyb
import ujson

def send_all(usb, data):
    sent = 0
    begin = time.ticks_ms()
    while sent < len(data):
        n = usb.send(memoryview(data)[sent:], timeout=100)
        sent += n or 0
        if time.ticks_diff(time.ticks_ms(), begin) > 2000:
            raise OSError('USB sin respuesta')

def panel_main():
    usb = pyb.USB_VCP()
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.HQVGA)
    sensor.skip_frames(time=1500)
    sensor.set_auto_whitebal(False)
    sensor.skip_frames(time=300)
    mission = Mission()
    incoming = b''
    last_send = time.ticks_ms()
    heartbeat = last_send
    frame = 0
    while True:
        img = sensor.snapshot()
        now = time.ticks_ms()
        rows = candidates(img)
        mission.update(rows, now)
        chunk = usb.read(128)
        if chunk:
            incoming += chunk
            if len(incoming) > 512:
                raise ValueError('Comando demasiado largo')
            while b'\n' in incoming:
                line, incoming = incoming.split(b'\n', 1)
                if line == b'PING':
                    heartbeat = now
                elif line == b'STOP':
                    return
                elif line.startswith(b'1 '):
                    try:
                        seq = int(line[2:])
                        if seq > 0:
                            mission.command(seq)
                    except ValueError:
                        pass
        if time.ticks_diff(now, heartbeat) > 8000:
            return  # Sin panel no continuar la prueba.
        if time.ticks_diff(now, last_send) >= 200:
            annotate(img, rows)
            if mission.mode != 'TURN' and not mission.tracker.locked:
                img.draw_line((ACQUIRE_MIN_X, 12, ACQUIRE_MIN_X, 145), color=(255,255,255))
                img.draw_line((ACQUIRE_MAX_X, 12, ACQUIRE_MAX_X, 145), color=(255,255,255))
            if mission.tracker.current is not None:
                r = mission.tracker.current
                img.draw_cross((r[4], r[5]), color=(255,255,255), size=8)
            state = mission.status(now)
            state['frame'] = frame
            state['detections'] = rows
            img.draw_string((0,148), 'T%d %s' % (mission.target,state['state']), color=(255,255,255))
            jpeg = img.compress(quality=50).bytearray()
            meta = ujson.dumps(state).encode()
            send_all(usb, ('N03 %d %d\n' % (len(meta),len(jpeg))).encode())
            send_all(usb, meta)
            send_all(usb, jpeg)
            last_send = now
        frame += 1

panel_main()
