import asyncio
import contextlib
import io
import json
import time
import unittest
from types import SimpleNamespace
from unittest.mock import AsyncMock

from xbox_control import XboxPilot, arguments
from pilot_lease import PilotLease

class ManualEntryTests(unittest.IsolatedAsyncioTestCase):
    def pilot(self,armed=0,mode=0):
        class Socket:
            async def send(ws,text):
                packet=json.loads(text);ws.sent.append(packet)
                if packet.get('cmd')!='control':return
                if packet.get('arm',0)>0:ws.armed=1
                elif packet.get('arm',0)<0:ws.armed=0
                ws.pilot.latest[3]=(time.monotonic(),dict(mode=packet['mode'],armed=ws.armed,motor1=0,motor2=0))
        ws=Socket();ws.sent=[];ws.armed=armed
        p=XboxPilot(ws,None,SimpleNamespace(seconds=0,power=30,course='track1',start='manual'),io.StringIO())
        ws.pilot=p;p.latest[3]=(time.monotonic(),dict(mode=mode,armed=armed,motor1=0,motor2=0))
        p.say=AsyncMock()
        return p,ws

    async def test_y_arms_disarmed_manual_without_flow_or_auto(self):
        p,ws=self.pilot()
        await p.take_manual()
        arms=[c for c in ws.sent if c.get('arm')==1]
        self.assertEqual(len(arms),1)
        self.assertEqual(arms[0]['mode'],8)
        self.assertEqual(arms[0]['aux']['7'],0)
        self.assertEqual(arms[0]['aux']['8'],0)
        self.assertEqual(p.phase,'MANUAL');self.assertFalse(p.auto_started)
        self.assertTrue(p.manual_wait_release)
        self.assertEqual(ws.sent[-1]['mode'],11)
        self.assertTrue(all(c['mode'] in (8,11) for c in ws.sent))
        await p.take_manual()
        self.assertEqual(sum(c.get('arm')==1 for c in ws.sent),1)

    async def test_y_takes_armed_auto_without_rearming_or_reset(self):
        p,ws=self.pilot(armed=1,mode=20)
        p.auto_started=True
        await p.take_manual()
        self.assertEqual(p.phase,'MANUAL')
        self.assertTrue(all(c['mode']==11 and c['arm']==0 and c['reset']==0 for c in ws.sent))

    async def test_fault_fallback_does_not_arm(self):
        p,ws=self.pilot()
        await p.select_manual()
        self.assertTrue(all(c.get('arm',0)==0 for c in ws.sent))

    async def test_armed_claim_requires_explicit_takeover_and_fresh_status(self):
        stopped=[]
        async def stop(mac):stopped.append(mac)
        lease=PilotLease(stop);owner=object();mac='dc:b4:d9:39:b3:b4'
        for status in (None,(time.monotonic()-3,1),(time.monotonic(),2)):
            with self.assertRaises(ValueError):await lease.claim(owner,mac,status,allow_armed=True)
        with self.assertRaises(ValueError):await lease.claim(owner,mac,(time.monotonic(),1))
        await lease.claim(owner,mac,(time.monotonic(),1),allow_armed=True)
        lease.ping(owner,mac);await lease.release(owner)
        self.assertEqual(stopped,[mac])

class OptionsTests(unittest.TestCase):
    def test_default_manual_unlimited_track1(self):
        o=arguments(['--actuate'])
        self.assertEqual((o.start,o.seconds,o.course),('manual',0,'track1'))

    def test_explicit_auto_track2_remains_available(self):
        o=arguments(['--actuate','--start','auto','--course','track2','--seconds','600'])
        self.assertEqual((o.start,o.seconds,o.course),('auto',600,'track2'))

    def test_invalid_duration_is_rejected(self):
        for duration in ('-1','nan','inf','1'):
            with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
                arguments(['--actuate','--seconds',duration])

if __name__=='__main__':unittest.main()
