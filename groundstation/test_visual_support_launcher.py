import asyncio
import json
import unittest
from types import SimpleNamespace
from unittest.mock import patch, AsyncMock
import check_visual_support as launcher


class Speaker:
    def __init__(self): self.spoken=[]
    async def say(self,text,abort): self.spoken.append(text)


class Socket:
    def __init__(self,version=12):
        self.version=version;self.sent=[];self.queue=asyncio.Queue()
        self.data={1:dict(yaw=0,battery=3.9),
            2:dict(nicla_flag=65,nicla_x=120,nicla_y=80,nicla_w=30,nicla_h=30),
            3:dict(mode=0,armed=0,motor1=0,motor2=0),
            7:dict(nicla_fresh=1,nicla_state=2,nicla_reset_pending=0)}
        self.emit()
    def emit(self):
        for flag,fields in self.data.items():
            self.queue.put_nowait(json.dumps(dict(flag=flag,fields=fields.copy())))
    def __aiter__(self): return self
    async def __anext__(self):
        if self.queue.empty():
            await asyncio.sleep(.05)
            self.emit()
        return self.queue.get_nowait()
    async def send(self,raw):
        p=json.loads(raw);self.sent.append(p)
        if p.get('cmd')=='stop':
            self.data[3]=dict(mode=0,armed=0,motor1=0,motor2=0)
        elif p.get('cmd')=='vision_bench_start':
            self.data[3]=dict(mode=22,armed=0,motor1=0,motor2=0)
            self.data[2]['nicla_flag']=65
            self.data[7]['nicla_state']=2
        elif p.get('cmd')=='control':
            mode=p['mode'];arm=self.data[3]['armed']
            if p.get('arm',0)>0:arm=1
            if p.get('arm',0)<0:arm=0
            motor=.15 if mode==12 and arm else 0
            self.data[3]=dict(mode=mode,armed=arm,motor1=motor,motor2=motor)
            if mode==12:
                self.data[10]=dict(support_phase=(0 if p['fx']==1 else 1) if arm else 4)
                if self.version is not None:
                    self.data[11]=dict(support_version=self.version,support_forward_request=0,
                        support_visual_bearing=1 if arm else 0)
        self.emit()


class FastTrial(launcher.Trial):
    async def wait(self,predicate,timeout=6):
        return await super().wait(predicate,min(timeout,.3))
    async def stable_target(self,timeout=6,stable_seconds=.5):
        return await super().stable_target(.3,.01)


class LauncherTests(unittest.IsolatedAsyncioTestCase):
    async def test_missing_during_countdown_does_not_cancel_release(self):
        ws=Socket()
        class CountSpeaker(Speaker):
            async def say(self,text,abort):
                await super().say(text,abort)
                if text.startswith('Tres, dos'):
                    ws.data[2]['nicla_flag']=0
                    ws.data[7]['nicla_state']=4
                    ws.emit()
                    await asyncio.sleep(.06)
        trial=FastTrial(ws,.02,speaker=CountSpeaker())
        await trial.run()
        self.assertTrue(any(p.get('mode')==12 and p.get('arm')==0 and p.get('fx')==0 for p in ws.sent))
        self.assertTrue(trial.stop_confirmed)

    async def test_brief_missing_is_recoverable_but_stale_link_is_not(self):
        ws=Socket();trial=FastTrial(ws,.02,speaker=Speaker())
        now=launcher.time.monotonic()
        trial.latest={k:(now,v.copy()) for k,v in ws.data.items()}
        trial.latest[2][1]['nicla_flag']=0
        trial.latest[7][1]['nicla_state']=4
        trial.last_target_at=now-1
        self.assertTrue(trial.target_or_recovering())
        trial.last_target_at=now-4
        self.assertFalse(trial.target_or_recovering())
        trial.last_target_at=now-1
        trial.latest[7][1]['nicla_fresh']=0
        self.assertFalse(trial.target_or_recovering())

    async def test_confirmed_partial_or_edge_pose_can_start(self):
        for flag,state,y,h in [(66,3,17,44),(65,2,20,50)]:
            ws=Socket();trial=FastTrial(ws,.02,speaker=Speaker())
            trial.latest={k:(launcher.time.monotonic(),v.copy()) for k,v in ws.data.items()}
            trial.latest[2][1].update(nicla_flag=flag,nicla_y=y,nicla_h=h)
            trial.latest[7][1]['nicla_state']=state
            await trial.stable_target()
            self.assertEqual(ws.sent,[])

    async def test_search_and_missing_cannot_start(self):
        for state in (0,1,4,5,6):
            ws=Socket();trial=FastTrial(ws,.02,speaker=Speaker())
            trial.latest={k:(launcher.time.monotonic(),v.copy()) for k,v in ws.data.items()}
            trial.latest[7][1]['nicla_state']=state
            with self.assertRaises(launcher.TrialError):await trial.stable_target()
            self.assertEqual(ws.sent,[])

    async def test_height_reference_is_centered_full_record_only(self):
        ws=Socket();trial=FastTrial(ws,.02,speaker=Speaker())
        trial.stage='regular XY'
        now=launcher.time.monotonic()
        trial.latest={k:(now,v.copy()) for k,v in ws.data.items()}
        trial.latest[1][1]['height']=.8
        trial.latest[7][1]['nicla_state']=3
        trial.capture_height_reference(now)
        self.assertIsNone(trial.height_reference)
        trial.latest[7][1]['nicla_state']=2
        trial.capture_height_reference(now)
        later=now+.51
        trial.latest={k:(later,v) for k,(_,v) in trial.latest.items()}
        trial.capture_height_reference(later)
        self.assertEqual(trial.height_reference['height'],.8)
        self.assertTrue(trial.height_reference['diagnostic_only'])
        trial.latest[1][1]['height']=1.5
        trial.capture_height_reference(later)
        self.assertEqual(trial.height_reference['height'],.8)
        self.assertEqual(ws.sent,[])
    async def test_forward_request_is_explicit_in_control_packets(self):
        ws=Socket();trial=FastTrial(ws,.02,speaker=Speaker(),forward=.01)
        await trial.run()
        active=[p for p in ws.sent if p.get('cmd')=='control' and p.get('mode')==12]
        self.assertTrue(active and all(p['tx']==.01 for p in active))
        self.assertTrue(trial.stop_confirmed)
    async def test_pose_loss_reacquires_before_arming(self):
        ws=Socket()
        class PoseSpeaker(Speaker):
            async def say(self,text,abort):
                await super().say(text,abort)
                if text.startswith('Sostenga'):
                    ws.data[2]['nicla_flag']=0
                    ws.data[7]['nicla_state']=4
                    ws.emit()
                    await asyncio.sleep(.01)
        trial=FastTrial(ws,.02,speaker=PoseSpeaker())
        await trial.run()
        reacquire=next(i for i,p in enumerate(ws.sent) if p.get('cmd')=='vision_bench_start')
        arm=next(i for i,p in enumerate(ws.sent) if p.get('arm')==1)
        self.assertLess(reacquire,arm)
        self.assertTrue(trial.stop_confirmed)

    async def test_unstable_pose_never_arms(self):
        class MissingTrial(FastTrial):
            async def stable_target(self,timeout=6,stable_seconds=.5):
                raise launcher.TrialError('Sin objetivo estable')
        ws=Socket();trial=MissingTrial(ws,.02,speaker=Speaker())
        with self.assertRaises(launcher.TrialError):await trial.run()
        self.assertFalse(any(p.get('arm')==1 for p in ws.sent))
        self.assertTrue(trial.stop_confirmed)

    async def test_missing_or_old_firmware_never_arms(self):
        for version in (None,1,2,3,4,5,6,7,8,9,10,11):
            ws=Socket(version);voice=Speaker();trial=FastTrial(ws,.02,speaker=voice)
            with self.assertRaises(launcher.TrialError): await trial.run()
            self.assertFalse(any(p.get('arm')==1 for p in ws.sent))
            self.assertTrue(trial.stop_confirmed)
            self.assertEqual(voice.spoken,[])
    async def test_success_prepares_then_releases_without_reset(self):
        ws=Socket();voice=Speaker();trial=FastTrial(ws,.02,speaker=voice)
        await trial.run()
        self.assertTrue(trial.stop_confirmed)
        active=[p for p in ws.sent if p.get('cmd')=='control' and p.get('mode')==12]
        prepare=[p for p in active if p['arm']==0 and p['fx']==1][0]
        release=[p for p in active if p['arm']==0 and p['fx']==0][0]
        self.assertEqual(prepare['reset'],1)
        self.assertEqual(release['reset'],0)
        self.assertEqual(prepare['aux'],release['aux'])
        self.assertTrue(any('Suelte ahora' in s for s in voice.spoken))
    async def test_existing_armed_session_is_not_taken_over(self):
        ws=Socket();ws.data[3]['armed']=1;ws.queue=asyncio.Queue();ws.emit()
        trial=FastTrial(ws,.02,speaker=Speaker())
        with self.assertRaises(launcher.TrialError):await trial.run()
        self.assertFalse(trial.owned)
        self.assertEqual(ws.sent,[])
    async def test_dry_run_never_connects(self):
        args=SimpleNamespace(dry_run=True)
        with patch.object(launcher.websockets,'connect',side_effect=AssertionError('no network')):
            self.assertEqual(await launcher.execute(args),0)
    async def test_voice_is_cancelled_on_fault(self):
        class Process:
            def __init__(self):self.returncode=None;self.event=asyncio.Event();self.killed=False
            async def wait(self):await self.event.wait();return self.returncode
            def kill(self):self.killed=True;self.returncode=-9;self.event.set()
        proc=Process();abort=asyncio.Event()
        async def trigger():await asyncio.sleep(.01);abort.set()
        with patch.object(launcher.shutil,'which',return_value='fake'),patch.object(
                launcher.asyncio,'create_subprocess_exec',new=AsyncMock(return_value=proc)):
            event=asyncio.create_task(trigger())
            with self.assertRaises(launcher.TrialError):await launcher.Speaker().say('Cuenta de prueba',abort)
            await event
        self.assertTrue(proc.killed)


class PackTests(unittest.TestCase):
    def test_forward_opt_in_and_limit(self):
        self.assertEqual(launcher.control(False,0)['tx'],0)
        self.assertEqual(launcher.control(False,0,forward=.01)['tx'],.01)
        self.assertEqual(launcher.control(False,0,forward=.03)['tx'],.03)
        for value in (-.01,.04,float('nan')):
            with self.assertRaises(ValueError):launcher.control(False,0,forward=value)
    def test_limits_and_no_implicit_arm(self):
        p=launcher.control(True,0)
        self.assertEqual((p['mode'],p['arm'],p['aux'][9]),(12,0,2))
        packed=(int(p['aux'][7])>>1)&0x3fffff
        self.assertEqual((packed&63)*.5,10)
        self.assertEqual(((packed>>6)&63)*.5,16)


if __name__=='__main__':unittest.main()
