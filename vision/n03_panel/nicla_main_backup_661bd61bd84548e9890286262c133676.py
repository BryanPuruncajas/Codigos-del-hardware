import sensor
import time
from pyb import UART
import sensor
import time
VERSION = "N03-panel-v2-central"
ACQUIRE_MIN_X = 90
ACQUIRE_MAX_X = 150
RED_LAB = (2, 80, 20, 80, -5, 70)
MIN_PIXELS = 25
MIN_BOX_AREA = 36
MIN_SIDE = 4
MIN_TARGET_SIDE = 20
MIN_FILL = 0.15
MIN_ASPECT = 0.65
MAX_ASPECT = 1.45
MIN_ROUNDNESS = 0.45
MIN_CIRCULARITY = 0.35
MAX_DISPLAY = 8
LOG_INTERVAL_MS = 200
VIEW_PERIOD_MS = 2000
def read_field(obj, name):
	value = getattr(obj, name)
	return value() if callable(value) else value
def metric(blob, name):
	try:
		value = float(read_field(blob, name))
		return value if 0 <= value < 1000000 else -1.0
	except (AttributeError, TypeError, ValueError, NotImplementedError):
		return -1.0
def candidates(img):
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
		label = "%d%s%s" % (i + 1, item[14], "!" if "C" in item[13].split("+") else "")
		tx = max(0, min(x, read_field(img, "width") - len(label) * 8))
		ty = max(12, min(y, read_field(img, "height") - 10))
		img.draw_string((tx, ty), label, color=color)
	f, p, d, x = counts(detections)
	img.draw_string((0, 0), "V3 F%d P%d D%d X%d S%d" %
					(f, p, d, x, min(len(detections), MAX_DISPLAY)),
					color=(255, 255, 255))
def render_view(img, detections, phase):
	if phase:
		img.binary([RED_LAB])
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
			full = [r for r in rows if r[14] == "F"
					and ACQUIRE_MIN_X <= r[4] <= ACQUIRE_MAX_X]
			if not full:
				self.pending = None
				self.hits = 0
				self.state = "SEARCH"
				return
			candidate = full[0]
			if len(full) > 1:
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
main()