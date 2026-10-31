import asyncio
import ctypes
import json
import struct
import time
import unittest
from types import SimpleNamespace
import io
from unittest.mock import patch

import xbox_input as pad
from pilot_lease import PilotLease
import telemetry_bridge as bridge
from xbox_control import XboxPilot, TOKEN

MAC='DC:B4:D9:39:B3:B4'

class MappingTests(unittest.TestCase):
    def test_xinput_wire_layout(self):
        self.assertEqual(ctypes.sizeof(pad._Gamepad),12)
        self.assertEqual(ctypes.sizeof(pad._State),16)

    def test_power_is_one_step_per_press(self):
        m=pad.ButtonMap()
        for _ in range(50):m.update(pad.B)
        self.assertEqual(m.power,35)
        m.update(0);m.update(pad.B);self.assertEqual(m.power,40)
        for _ in range(20):m.update(0);m.update(pad.B)
        self.assertEqual(m.power,50)
        for _ in range(20):m.update(0);m.update(pad.X)
        self.assertEqual(m.power,0)
        m.update(0);m.update(pad.B|pad.X);self.assertEqual(m.power,0)

    def test_requested_directions_and_neutral_release(self):
        m=pad.ButtonMap(45)
        for button,axis,value in ((pad.A,'fz',1),(pad.UP,'fx',1),(pad.DOWN,'fx',-1),
                                  (pad.RIGHT,'tz',1),(pad.LEFT,'tz',-1)):
            c=m.manual(button)
            self.assertEqual(c[axis],value)
            self.assertEqual(c['aux'][5],45)
            self.assertEqual(sum(abs(c[k]) for k in ('fx','fz','tz')),1)
            self.assertEqual(c['arm'],0)
        self.assertEqual(sum(abs(m.manual(0)[k]) for k in ('fx','fz','tz')),0)
        self.assertEqual(m.manual(pad.A|pad.UP)['fz'],1)
        self.assertEqual(m.manual(pad.A|pad.UP)['fx'],0)
        self.assertEqual(m.manual(pad.UP|pad.DOWN)['fx'],0)

    def test_zero_power_cannot_trigger_legacy_default_thrust(self):
        c=pad.ButtonMap(0).manual(pad.A|pad.UP)
        self.assertEqual([c[k] for k in ('fx','fz','tx','tz')],[0,0,0,0])

    def test_mode_and_waypoint_are_edges_stop_has_priority(self):
        m=pad.ButtonMap()
        self.assertEqual(m.update(pad.Y),['toggle']);self.assertEqual(m.update(pad.Y),[])
        m.update(0);self.assertEqual(m.update(pad.RB),['waypoint'])
        self.assertEqual(m.update(pad.RB),[])
        self.assertEqual(m.update(pad.BACK|pad.B|pad.Y),['stop'])

    def test_disconnected_controller_is_not_a_neutral_connected_pad(self):
        reader=object.__new__(pad.XInput);reader.index=0;reader.get=lambda *_:1167
        with self.assertRaises(pad.GamepadDisconnected):reader.read()

class LeaseTests(unittest.IsolatedAsyncioTestCase):
    async def test_expiration_stops_once_and_revokes_owner(self):
        stopped=[]
        async def stop(mac):stopped.append(mac)
        lease=PilotLease(stop,.02);owner=object()
        await lease.claim(owner,MAC,(time.monotonic(),0))
        await asyncio.sleep(.1)
        self.assertEqual(stopped,[MAC.lower()]);self.assertIsNone(lease.owner)
        with self.assertRaises(ValueError):lease.ping(owner,MAC)
        self.assertFalse(await lease.release(owner))

    async def test_exclusive_owner_and_fresh_disarmed_claim(self):
        async def stop(mac):pass
        lease=PilotLease(stop);owner=object()
        for status in (None,(time.monotonic()-3,0),(time.monotonic(),1)):
            with self.assertRaises(ValueError):await lease.claim(owner,MAC,status)
        await lease.claim(owner,MAC,(time.monotonic(),0))
        lease.ping(owner,MAC);lease.check(owner,MAC)
        with self.assertRaises(ValueError):lease.check(object(),MAC)
        with self.assertRaises(ValueError):await lease.claim(object(),MAC,(time.monotonic(),0))
        await lease.release(owner)

    async def test_real_bridge_releases_on_disconnect(self):
        class Serial:
            def __init__(self):self.packets=[]
            def write(self,data):self.packets.append(data)
        class Link:
            def __init__(self):self.ser=Serial()
        class Socket:
            def __init__(self):
                self.sent=[];self.commands=iter([{'cmd':'pilot_claim'},{'cmd':'pilot_ping'},
                    {'cmd':'control','mode':11,'fz':1,'aux':{'5':35},'reload':22594}])
            async def recv(self):return json.dumps({'auth':{'role':'dev','token':'test'}})
            async def send(self,msg):self.sent.append(json.loads(msg))
            def __aiter__(self):return self
            async def __anext__(self):
                try:return json.dumps(next(self.commands))
                except StopIteration:raise StopAsyncIteration
        bridge.last_actuator_status=(time.monotonic(),0)
        ws=Socket();link=Link();await bridge.make_handler(link,'test')(ws)
        packets=[struct.unpack('<13f',p[7:]) for p in link.ser.packets]
        self.assertEqual(packets[0][0],11);self.assertEqual(packets[0][5],35)
        self.assertEqual(packets[0][11],22594)
        self.assertTrue(all(p[0]==0 and p[10]==-1 for p in packets[-3:]))
        self.assertTrue(all(m.get('cmd_ok',True) for m in ws.sent))

    async def test_expired_pilot_cannot_send_late_controls(self):
        class Serial:
            def __init__(self):self.packets=[]
            def write(self,data):self.packets.append(data)
        class Socket:
            def __init__(self):self.sent=[];self.step=0
            async def recv(self):return json.dumps({'auth':{'role':'dev','token':'test'}})
            async def send(self,msg):self.sent.append(json.loads(msg))
            def __aiter__(self):return self
            async def __anext__(self):
                self.step+=1
                if self.step==1:return json.dumps({'cmd':'pilot_claim'})
                if self.step==2:
                    await asyncio.sleep(.12)
                    return json.dumps({'cmd':'control','mode':11,'fz':1})
                raise StopAsyncIteration
        class Link:
            def __init__(self):self.ser=Serial()
        bridge.last_actuator_status=(time.monotonic(),0);link=Link();ws=Socket()
        with patch.object(bridge,'PilotLease',lambda stop:PilotLease(stop,.02)):
            await bridge.make_handler(link,'test')(ws)
        packets=[struct.unpack('<13f',p[7:]) for p in link.ser.packets]
        self.assertTrue(all(p[0]==0 for p in packets))
        self.assertTrue(any(m.get('cmd_ok') is False for m in ws.sent))

class PilotTests(unittest.IsolatedAsyncioTestCase):
    def pilot(self):
        class Socket:
            def __init__(self):self.sent=[];self.pilot=None;self.ids=set()
            async def send(self,text):
                c=json.loads(text);self.sent.append(c)
                if c.get('cmd')!='control':return
                p=self.pilot
                p.latest[3]=(time.monotonic(),dict(mode=c['mode'],armed=1,motor1=0,motor2=0))
                if c.get('reset')==3:
                    event=c['aux']['6']
                    if event not in self.ids:
                        self.ids.add(event)
                        p.latest[5]=(time.monotonic(),dict(visited=p.fields(5)['visited']+1,target_count=5))
        ws=Socket()
        p=XboxPilot(ws,None,SimpleNamespace(seconds=240,power=30,course='track2'),io.StringIO())
        ws.pilot=p
        p.latest={3:(time.monotonic(),dict(mode=21,armed=1)),
                  5:(time.monotonic(),dict(visited=1,target_count=5)),
                  7:(time.monotonic(),dict(nicla_fresh=1,nicla_reset_pending=0,nicla_protocol=0x4e03)),
                  18:(time.monotonic(),dict(flow_height_source=2,flow_height_flags=5647))}
        p.auto_started=True;p.auto_payload=dict(cmd='control',mode=21,arm=0,reset=0,aux={5:.25})
        return p,ws

    async def test_y_manual_rb_and_y_resume_do_not_rearm_or_reset_progress(self):
        p,ws=self.pilot()
        await p.select_manual()
        self.assertEqual(p.phase,'MANUAL');self.assertEqual(ws.sent[-1]['mode'],11)
        await p.mark_waypoint()
        self.assertEqual(p.fields(5)['visited'],2)
        mark=next(c for c in ws.sent if c.get('reset')==3)
        self.assertEqual(mark['aux']['6'],1)
        await p.resume_auto()
        self.assertEqual(p.phase,'AUTO');self.assertEqual(ws.sent[-1]['reset'],2)
        self.assertEqual(p.fields(5)['visited'],2)
        self.assertTrue(all(c.get('arm',0)==0 for c in ws.sent))
        self.assertTrue(all(c['reload']==TOKEN for c in ws.sent))

    async def test_heartbeat_never_repeats_one_shot_arm_or_reset(self):
        p,ws=self.pilot()
        await p.send(dict(cmd='control',mode=8,arm=1,reset=1,aux={7:0,8:0}))
        self.assertEqual(ws.sent[-1]['arm'],1)
        self.assertEqual(p.heartbeat_payload['arm'],0)
        self.assertEqual(p.heartbeat_payload['reset'],0)

    async def test_rb_without_manual_does_not_send(self):
        p,ws=self.pilot();p.phase='AUTO'
        await p.mark_waypoint();self.assertEqual(ws.sent,[])

if __name__=='__main__':unittest.main()
