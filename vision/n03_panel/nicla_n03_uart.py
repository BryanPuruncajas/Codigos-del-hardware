"""N03 UART/iBUS bridge for the isolated Nicla test."""
import sensor
import time
import gc
from pyb import UART, LED

exec(open("n02_core.py").read())
UART_ID = "LP1"
UART_BAUD = 115200
FRAME_MS = 50
DEBUG_USB = False
PROTOCOL = 0x4E03
STATES = {"SEARCH": 0, "CONFIRM": 1, "FULL": 2, "PARTIAL": 3,
          "MISSING": 4, "AMBIG": 5, "LOST": 6}

def checksum(data):
    total = 0
    for value in data:
        total += value
    return 0xFFFF - total

def ibus_frame(values):
    frame = bytearray(32)
    frame[0] = 0x20
    frame[1] = 0x40
    for i in range(14):
        value = int(values[i])
        if value < 0: value = 0
        if value > 65535: value = 65535
        frame[2 + 2*i] = value & 0xFF
        frame[3 + 2*i] = (value >> 8) & 0xFF
    check = checksum(frame[:-2])
    frame[30] = check & 0xFF
    frame[31] = (check >> 8) & 0xFF
    return frame

def send_observation(uart, row, sample, state="SEARCH", reset_ack=0):
    if row is None:
        values = [0, 0, 0, 0, 0, 0, 0, 0, 0, 9999, sample, 0, 0, 0]
    else:
        x, y, w, h, cx, cy = row[:6]
        flag = 0x41 if state == "FULL" else 0x42
        values = [flag, x, y, w, h, cx, cy, w, h, 9999, sample, 0, 0, 0]
    values[11] = STATES[state]
    values[12] = PROTOCOL
    values[13] = reset_ack
    uart.write(ibus_frame(values))

def main():
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.HQVGA)
    sensor.skip_frames(time=1500)
    sensor.set_auto_whitebal(False)
    sensor.skip_frames(time=300)
    uart = UART(UART_ID, baudrate=UART_BAUD, timeout_char=10)
    tracker = Tracker()
    sample = 0
    last_send = time.ticks_ms()
    frame = 0
    reset_ack = 0
    last_gc = last_led = last_send
    red, green = LED(1), LED(2)
    pulse = False
    while True:
        if uart.any():
            command = uart.read(min(uart.any(), 32))
            if command and b'R' in command:
                tracker = Tracker()
                reset_ack = (reset_ack + 1) & 65535
        img = sensor.snapshot()
        now = time.ticks_ms()
        rows = candidates(img)
        tracker.update(rows, now)
        if time.ticks_diff(now, last_send) >= FRAME_MS:
            row = tracker.current
            send_observation(uart, row, sample, tracker.state, reset_ack)
            if DEBUG_USB and sample % 20 == 0:
                print("N03_UART,%d,%d,%s,%d,%d,%d,%d" %
                  (frame, sample, tracker.state, int(row is not None),
                   row[4] if row else -1, row[5] if row else -1,
                   row[2] if row else 0))
            sample = (sample + 1) & 255
            last_send = now
        frame += 1
        if time.ticks_diff(now, last_led) >= 250:
            pulse = not pulse
            red.off()
            green.off()
            if pulse:
                if tracker.current is not None: green.on()
                else: red.on()
            last_led = now
        # Recoger temporales regularmente; evita acumular hasta agotar heap.
        # El diagnostico mide la pausa real; no se presupone su duracion.
        if time.ticks_diff(now, last_gc) >= 1000:
            gc.collect()
            last_gc = time.ticks_ms()

main()
