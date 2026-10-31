"""Pruebas de banco simulado; no accede a puertos ni actuadores."""
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parent
ns = {"time": SimpleNamespace(ticks_diff=lambda a, b: a-b)}
tree = ast.parse((ROOT / "n02_core.py").read_text(encoding="utf-8"))
tree.body = [n for n in tree.body if not isinstance(n, (ast.Import, ast.ImportFrom))]
exec(compile(tree, "n02_core.py", "exec"), ns)

def row(cx=120, cy=80, w=26, h=26, state="F"):
    return (cx-w//2, cy-h//2, w, h, cx, cy, 400, int(state=="P"),
            w/h, 0.6, 0.8, 0.5, int(state=="F"), "OK", state, 1)

def locked():
    tracker = ns["Tracker"]()
    for now in range(0, 351, 50):
        tracker.update([row()], now)
    assert tracker.state == "FULL"
    return tracker

class RecoveryTests(unittest.TestCase):
    def test_local_jump_requires_confirmation(self):
        t = locked()
        for now in range(400, 900, 50):
            t.update([row(cx=160)], now)
            self.assertIsNone(t.current)
        t.update([row(cx=160)], 900)
        self.assertEqual(t.state, "FULL")
        self.assertEqual(t.current[4], 160)

    def test_lost_can_recover_locally(self):
        t = locked()
        t.update([], 2400)
        self.assertEqual(t.state, "LOST")
        for now in range(2450, 3001, 50):
            t.update([row(cx=160)], now)
        self.assertEqual(t.state, "FULL")
        self.assertFalse(t.expired)

    def test_two_candidates_never_reacquired(self):
        t = locked()
        for now in range(2400, 4001, 50):
            t.update([row(cx=155), row(cx=170)], now)
            self.assertIsNone(t.current)

    def test_far_or_partial_or_wrong_scale_never_reacquired(self):
        for candidate in (row(cx=200), row(cx=160, state="P"), row(cx=160, w=50)):
            t = locked()
            for now in range(2400, 4001, 50):
                t.update([candidate], now)
                self.assertIsNone(t.current)

    def test_expired_window_requires_external_reset(self):
        t = locked()
        for now in range(5400, 6401, 50):
            t.update([row()], now)
            self.assertEqual(t.state, "LOST")
            self.assertIsNone(t.current)

    def test_central_initial_acquisition_and_partial_tracking(self):
        t = ns["Tracker"]()
        for now in range(0, 1001, 50):
            t.update([row(cx=170)], now)
        self.assertFalse(t.locked)
        t = locked()
        t.update([row(state="P")], 400)
        self.assertEqual(t.state, "PARTIAL")

    def test_wire_protocol_unchanged(self):
        wire = {}
        tree = ast.parse((ROOT / "nicla_n03_uart.py").read_text(encoding="utf-8"))
        tree.body = [n for n in tree.body if isinstance(n, ast.Assign) or
                     isinstance(n, ast.FunctionDef) and n.name in
                     ("checksum", "ibus_frame", "send_observation")]
        exec(compile(tree, "wire", "exec"), wire)
        frames = []
        uart = SimpleNamespace(write=lambda packet: frames.append(packet))
        for state, observation, expected in (("FULL", row(), 0x41),
                                            ("PARTIAL", row(state="P"), 0x42),
                                            ("LOST", None, 0)):
            wire["send_observation"](uart, observation, 19, state, 7)
            packet = frames[-1]
            values = [int.from_bytes(packet[2+2*i:4+2*i], "little") for i in range(14)]
            self.assertEqual(len(packet), 32)
            self.assertEqual(values[0], expected)
            self.assertEqual(values[10:], [19, wire["STATES"][state], 0x4E03, 7])
            self.assertEqual(int.from_bytes(packet[30:], "little"), 65535-sum(packet[:30]))

    def test_close_partial_can_acquire_after_confirmation(self):
        t = ns['Tracker']()
        candidate = row(w=170, h=145, state='P')
        for now in range(0, 300, 50):
            t.update([candidate], now)
            self.assertIsNone(t.current)
        t.update([candidate], 300)
        self.assertEqual(t.state, 'PARTIAL')
        self.assertTrue(t.locked)

    def test_partial_background_and_ambiguous_initial_not_acquired(self):
        for candidates in ([row(w=240, h=160, state='P')],
                           [row(w=170, h=145, state='P'), row(cx=125)]):
            t = ns['Tracker']()
            for now in range(0, 1001, 50):
                t.update(candidates, now)
                self.assertIsNone(t.current)
                self.assertFalse(t.locked)

if __name__ == "__main__":
    unittest.main()
