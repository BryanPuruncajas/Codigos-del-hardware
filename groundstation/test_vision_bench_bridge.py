"""No serial device or network required. Exercise actual bridge packing."""
import asyncio
import json
import struct
import time
import unittest
import telemetry_bridge as bridge


class Serial:
    def __init__(self):
        self.packets = []

    def write(self, data):
        self.packets.append(data)


class Link:
    def __init__(self):
        self.ser = Serial()


class VisionBenchTest(unittest.IsolatedAsyncioTestCase):
    async def test_actuated_start_update_stop_path_uses_one_arm(self):
        class Socket:
            def __init__(self):
                self.sent = []
                base = {'test':'p05','height':2.8,'yaw_deg':45,'max_power':10,'duration_s':20}
                self.commands = iter([{'cmd':'mission_start',**base},
                                      {'cmd':'mission_update',**base}, {'cmd':'stop'}])
            async def recv(self):
                return json.dumps({'auth':{'role':'dev','token':'test'}})
            async def send(self, msg):
                self.sent.append(json.loads(msg))
            def __aiter__(self): return self
            async def __anext__(self):
                try: return json.dumps(next(self.commands))
                except StopIteration: raise StopAsyncIteration
        bridge.last_actuator_status = (time.monotonic(), 0)
        link = Link(); ws = Socket()
        await bridge.make_handler(link, 'test')(ws)
        packets = [struct.unpack('<13f', p[7:]) for p in link.ser.packets]
        self.assertEqual(sum(p[10] == 1 for p in packets), 1)
        self.assertEqual(sum(p[0] == 14 and p[12] == 1 for p in packets), 2)
        self.assertEqual((packets[-1][0], packets[-1][10]), (0, -1))
        self.assertTrue(all(m.get('cmd_ok', True) for m in ws.sent))

    async def test_trial_duration_and_deadline(self):
        self.assertEqual(bridge.trial_duration({}), 20)
        self.assertEqual(bridge.trial_duration({'duration_s': 240}), 240)
        for value in (0, -1, 241, float('nan'), float('inf')):
            with self.assertRaises(ValueError):
                bridge.trial_duration({'duration_s': value})
        link = Link()
        deadline = bridge.TrialDeadline(link, 'DC:B4:D9:39:B3:B4', .01)
        deadline.start()
        await deadline.task
        self.assertEqual(len(link.ser.packets), 3)
        for packet in link.ser.packets:
            p = struct.unpack('<13f', packet[7:])
            self.assertEqual((p[0], p[10]), (0, -1))

    async def test_cancelled_deadline_cannot_stop_later_trial(self):
        link = Link()
        deadline = bridge.TrialDeadline(link, 'DC:B4:D9:39:B3:B4', .25)
        deadline.start()
        await deadline.close()
        count = len(link.ser.packets)
        await asyncio.sleep(.3)
        self.assertEqual(count, 3)
        self.assertEqual(len(link.ser.packets), count)

    async def test_handler_disconnect_stops_and_viewer_cannot_start(self):
        class Socket:
            def __init__(self, role):
                self.role = role
                self.sent = []
                self.pending = True
            async def recv(self):
                return json.dumps({'auth': {'role': self.role, 'token': 'test'}})
            async def send(self, msg):
                self.sent.append(json.loads(msg))
            def __aiter__(self):
                return self
            async def __anext__(self):
                if not self.pending:
                    raise StopAsyncIteration
                self.pending = False
                return json.dumps({'cmd': 'vision_bench_start', 'mac': 'DC:B4:D9:39:B3:B4'})
        for role in ('dev', 'viewer'):
            bridge.last_actuator_status = (time.monotonic(), 0)
            link = Link()
            ws = Socket(role)
            await bridge.make_handler(link, 'test')(ws)
            if role == 'viewer':
                self.assertEqual(link.ser.packets, [])
            else:
                packets = [struct.unpack('<13f', p[7:]) for p in link.ser.packets]
                self.assertEqual(len(packets), 6)
                self.assertTrue(all(p[10] == -1 for p in packets))
                self.assertEqual(packets[-1][0], 0)
                self.assertTrue(any(m.get('cmd_ok') is True for m in ws.sent))

    async def test_start_and_stop_never_arm(self):
        link = Link()
        bridge.last_actuator_status = (time.monotonic(), 0)
        await bridge.start_vision_bench(link, 'DC:B4:D9:39:B3:B4')
        packets = [struct.unpack('<13f', p[7:]) for p in link.ser.packets]
        self.assertEqual(len(packets), 3)
        self.assertTrue(all(p[0] == 22 and p[10] == -1 for p in packets))
        self.assertEqual([p[12] for p in packets], [1, 0, 0])
        await bridge.stop_vision_bench(link, 'DC:B4:D9:39:B3:B4')
        for p in link.ser.packets[3:]:
            values = struct.unpack('<13f', p[7:])
            self.assertEqual((values[0], values[10]), (0, -1))

    async def test_refuse_missing_stale_or_armed_status(self):
        for status in (None, (time.monotonic() - 3, 0), (time.monotonic(), 1)):
            bridge.last_actuator_status = status
            link = Link()
            with self.assertRaises(ValueError):
                await bridge.start_vision_bench(link, 'DC:B4:D9:39:B3:B4')
            self.assertEqual(link.ser.packets, [])


if __name__ == '__main__':
    unittest.main()
