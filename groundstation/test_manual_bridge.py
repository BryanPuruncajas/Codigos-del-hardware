"""Exercise real bridge handling with fake serial; never touches hardware."""
import asyncio
import json
import struct
import time
import unittest
from types import SimpleNamespace
from unittest.mock import patch
import telemetry_bridge as bridge

MAC = 'dc:b4:d9:39:b3:b4'

class Serial:
    def __init__(self): self.packets = []
    def write(self, data): self.packets.append(data)

class Socket:
    def __init__(self, commands):
        self.commands = iter(commands)
        self.sent = []
    async def recv(self): return json.dumps({'auth': {'role': 'dev', 'token': 'test'}})
    async def send(self, message): self.sent.append(json.loads(message))
    def __aiter__(self): return self
    async def __anext__(self):
        try: return json.dumps(next(self.commands))
        except StopIteration: raise StopAsyncIteration

class ManualBridgeTests(unittest.IsolatedAsyncioTestCase):
    async def handle(self, commands):
        link = SimpleNamespace(ser=Serial())
        ws = Socket(commands)
        bridge.last_actuator_status = (time.monotonic(), 0)
        real_sleep = asyncio.sleep
        async def skip_esc_hardware_delay(seconds):
            await real_sleep(0 if seconds == 4.2 else seconds)
        with patch.object(bridge.asyncio, 'sleep', skip_esc_hardware_delay):
            await bridge.make_handler(link, 'test')(ws)
        return [struct.unpack('<13f', packet[7:]) for packet in link.ser.packets], ws

    async def test_button_press_and_release_do_not_disarm_manual_session(self):
        packets, ws = await self.handle([
            {'cmd': 'mission_start', 'test': 'p02', 'duration_s': 20},
            {'cmd': 'control', 'mode': 11, 'fx': .12},
            {'cmd': 'control', 'mode': 11},
            {'cmd': 'control', 'mode': 11, 'tz': .05},
            {'cmd': 'stop'},
        ])
        self.assertEqual(sum(p[10] == 1 for p in packets), 1)
        self.assertEqual([p[0] for p in packets[:5]], [11]*5)
        self.assertTrue(all(p[10] == 0 for p in packets[1:5]))
        self.assertAlmostEqual(packets[2][1], .12, places=6)
        self.assertEqual(packets[3][1:5], (0, 0, 0, 0))
        self.assertAlmostEqual(packets[4][4], .05, places=6)
        self.assertTrue(all(p[0] == 0 and p[10] == -1 for p in packets[5:]))
        self.assertTrue(all(m.get('cmd_ok', True) for m in ws.sent))

    async def test_manual_disconnect_still_stops(self):
        packets, _ = await self.handle([
            {'cmd': 'mission_start', 'test': 'p02'},
            {'cmd': 'control', 'mode': 11, 'fz': .12},
        ])
        self.assertEqual(packets[2][0], 11)
        self.assertAlmostEqual(packets[2][2], .12, places=6)
        self.assertTrue(all(p[0] == 0 and p[10] == -1 for p in packets[-3:]))

    async def test_manual_default_duration_is_120_and_explicit_duration_respected(self):
        for extra, expected in (({}, 120), ({'duration_s': 30}, 30)):
            _, ws = await self.handle([{'cmd': 'mission_start', 'test': 'p02', **extra}, {'cmd': 'stop'}])
            response = next(m for m in ws.sent if m.get('cmd') == 'mission_start')
            self.assertTrue(response['cmd_ok'])
            self.assertTrue(any(f'{expected} s' in line for line in response['effective']))

    async def test_mode_change_stops_previous_trial_before_new_command(self):
        packets, _ = await self.handle([
            {'cmd': 'mission_start', 'test': 'p02'},
            {'cmd': 'control', 'mode': 12},
        ])
        self.assertTrue(all(p[0] == 0 and p[10] == -1 for p in packets[2:5]))
        self.assertEqual(packets[5][0], 12)

    async def test_updates_keep_original_deadline_and_reject_other_sessions(self):
        link = SimpleNamespace(ser=Serial())
        deadline = bridge.TrialDeadline(link, MAC, .08, mode=11)
        deadline.start()
        task = deadline.task
        for bad in ({'mode': 12}, {'mode': 11, 'arm': 1},
                    {'mode': 11, 'reset': 1}, {'mode': 11, 'reload': 1}):
            self.assertFalse(bridge.continues_manual_trial(deadline, 'control', bad, MAC))
        self.assertFalse(bridge.continues_manual_trial(deadline, 'control', {'mode': 11}, '00:00:00:00:00:01'))
        self.assertFalse(bridge.continues_manual_trial(deadline, 'stop', {}, MAC))
        for _ in range(4):
            self.assertTrue(bridge.continues_manual_trial(deadline, 'control', {'mode': 11}, MAC.upper()))
            self.assertIs(deadline.task, task)
            await asyncio.sleep(.01)
        await asyncio.wait_for(task, .3)
        self.assertFalse(bridge.continues_manual_trial(deadline, 'control', {'mode': 11}, MAC))
        self.assertEqual(len(link.ser.packets), 3)
        self.assertTrue(all(struct.unpack('<13f', p[7:])[10] == -1 for p in link.ser.packets))

if __name__ == '__main__': unittest.main()
