"""No network or hardware: check the interlock for impulse/yaw telemetry."""
import asyncio
import math
import time
import unittest
import check_single_waypoint as launcher

class GuardTests(unittest.IsolatedAsyncioTestCase):
    async def test_held_height_pauses_advance_without_stopping_support(self):
        t=self.trial(0,0)
        t.latest[18]=(time.monotonic(),dict(flow_height_source=2,flow_height_flags=8192|16384|4104))
        task=asyncio.create_task(t.guard())
        try:
            await asyncio.sleep(.12)
            self.assertFalse(t.abort.is_set())
        finally:
            task.cancel();await asyncio.gather(task,return_exceptions=True)

    async def test_advance_on_held_height_aborts(self):
        t=self.trial(.03,0)
        t.latest[18]=(time.monotonic(),dict(flow_height_source=2,flow_height_flags=8192|16384|4104))
        await asyncio.wait_for(t.guard(),.5)
        self.assertTrue(t.abort.is_set())
        self.assertIn('altura conservada',t.reason)

    async def test_flow_after_arm_waits_for_new_qualified_window(self):
        t=self.trial(0,0);t.rx_counts[18]=3
        task=asyncio.create_task(t.wait_flow_after_arm(timeout=.6))
        try:
            await asyncio.sleep(.06)
            self.assertFalse(task.done(), 'A pre-arm sample must not authorize flight')
            t.rx_counts[18]=4
            t.latest[18]=(time.monotonic(),dict(flow_height_source=0,flow_height_flags=4096))
            await asyncio.sleep(.06)
            self.assertFalse(task.done(), 'A new invalid sample must not authorize flight')
            t.rx_counts[18]=5
            t.latest[18]=(time.monotonic(),dict(flow_height_source=2,flow_height_flags=4106))
            await asyncio.wait_for(task,.3)
        finally:
            task.cancel();await asyncio.gather(task,return_exceptions=True)

    async def test_flow_after_arm_missing_range_still_times_out(self):
        t=self.trial(0,0);t.rx_counts[18]=3
        t.latest[18]=(time.monotonic(),dict(flow_height_source=0,flow_height_flags=4096))
        with self.assertRaises(launcher.TrialError):
            await t.wait_flow_after_arm(timeout=.1)

    def trial(self,forward,yaw,done=False):
        t=launcher.WaypointTrial(None,120)
        data={1:dict(battery=3.8,yaw=0),
              3:dict(mode=17,armed=0 if done else 1,motor1=.15,motor2=.15),
              5:dict(mission_state=5 if done else 1,visited=1 if done else 0),
              7:dict(nicla_fresh=1),11:dict(support_version=39),
              12:dict(impulse_forward=forward,impulse_yaw=yaw),17:dict(fixed_height_flags=15),18:dict(flow_height_source=2,flow_height_flags=4106)}
        t.latest={k:(time.monotonic(),v) for k,v in data.items()}
        t.run_released=True
        t.guard_enabled=True
        return t
    async def test_required_flow_missing_aborts(self):
        t=self.trial(0,0);t.require_flow=True;t.latest.pop(18,None)
        await asyncio.wait_for(t.guard(),.5)
        self.assertIn('Flowdeck',t.reason)

    async def test_required_flow_range_allowed(self):
        for source in (2,):
            t=self.trial(0,0);t.require_flow=True
            t.latest[18]=(time.monotonic(),dict(flow_height_source=source,flow_height_flags=10))
            self.assertEqual(t.flow_ready(),source==2)
            task=asyncio.create_task(t.guard())
            try:
                await asyncio.sleep(.12);self.assertFalse(t.abort.is_set())
            finally:
                task.cancel();await asyncio.gather(task,return_exceptions=True)

    async def test_required_flow_expired_bridge_aborts(self):
        t=self.trial(0,0);t.require_flow=True
        t.latest[18]=(time.monotonic(),dict(flow_height_source=3,flow_height_flags=520))
        await asyncio.wait_for(t.guard(),.5)
        self.assertIn('Flowdeck',t.reason)

    async def test_advance_before_release_aborts(self):
        t=self.trial(.12,0);t.run_released=False
        await asyncio.wait_for(t.guard(),.5)
        self.assertIn('liberacion',t.reason)

    async def test_vertical_only_blocks_advance(self):
        t=self.trial(.12,0);t.vertical_only=True
        await asyncio.wait_for(t.guard(),.5)
        self.assertIn('liberacion',t.reason)

    async def test_advance_without_fixed_reference_aborts(self):
        t=self.trial(.12,0);t.latest[17]=(time.monotonic(),dict(fixed_height_flags=12))
        await asyncio.wait_for(t.guard(),.5)
        self.assertIn('referencia',t.reason)

    async def test_release_follows_completed_voice_without_reset(self):
        t=self.trial(0,0);t.run_released=False;events=[]
        async def say(words):
            self.assertFalse(t.run_released);events.append('voice_finished')
        async def send(command):
            self.assertTrue(t.run_released);events.append(command)
        t.say=say;t.send=send
        command=dict(cmd='control',mode=17,arm=0,aux={5:.25,9:24})
        await t.release_after_countdown(command)
        self.assertEqual(events[0],'voice_finished')
        self.assertEqual(events[1]['reset'],0)
        self.assertEqual(int(events[1]['aux'][9])&1,1)
        self.assertEqual(command['aux'][9],24)

    async def test_simultaneous_push_and_yaw_aborts(self):
        t=self.trial(.03,.1)
        await asyncio.wait_for(t.guard(),.5)
        self.assertTrue(t.abort.is_set())
        self.assertIn('simultaneos',t.reason)

    async def test_power_above_twenty_five_percent_aborts(self):
        t=self.trial(0,0)
        t.latest[3]=(time.monotonic(),dict(mode=17,armed=1,motor1=.26,motor2=.26))
        await asyncio.wait_for(t.guard(),.5)
        self.assertTrue(t.abort.is_set())
        self.assertIn('Potencia',t.reason)

    async def test_old_full_gate_firmware_refused(self):
        t=self.trial(0,0)
        t.latest[11]=(time.monotonic(),dict(support_version=25))
        await asyncio.wait_for(t.guard(),.5)
        self.assertTrue(t.abort.is_set())
        self.assertIn('Firmware',t.reason)

    async def test_previous_approach_firmware_refused(self):
        t=self.trial(0,0)
        t.latest[11]=(time.monotonic(),dict(support_version=38))
        await asyncio.wait_for(t.guard(),.5)
        self.assertTrue(t.abort.is_set())
        self.assertIn('Firmware',t.reason)

    async def test_twenty_five_percent_supported(self):
        t=self.trial(0,0)
        t.latest[3]=(time.monotonic(),dict(mode=17,armed=1,motor1=.25,motor2=.25))
        task=asyncio.create_task(t.guard())
        try:
            await asyncio.sleep(.12)
            self.assertFalse(t.abort.is_set())
        finally:
            task.cancel();await asyncio.gather(task,return_exceptions=True)

    async def test_low_or_invalid_battery_does_not_stop_temporary_trial(self):
        for voltage in (3.49,3.0,float('nan')):
            t=self.trial(0,0)
            t.latest[1]=(time.monotonic(),dict(battery=voltage,yaw=0))
            self.assertFalse(t.battery_blocked())
            task=asyncio.create_task(t.guard())
            try:
                await asyncio.sleep(.12)
                self.assertFalse(t.abort.is_set())
                if math.isnan(voltage):self.assertTrue(math.isnan(t.fields(1)['battery']))
                else:self.assertEqual(t.fields(1)['battery'],voltage)
            finally:
                task.cancel();await asyncio.gather(task,return_exceptions=True)

    async def test_battery_cutoff_can_be_restored(self):
        t=self.trial(0,0);t.battery_cutoff=True
        t.latest[1]=(time.monotonic(),dict(battery=3.49,yaw=0))
        await asyncio.wait_for(t.guard(),.5)
        self.assertTrue(t.abort.is_set())
        self.assertIn('Bateria',t.reason)

    async def test_invalid_yaw_still_aborts_without_battery_cutoff(self):
        t=self.trial(0,0)
        t.latest[1]=(time.monotonic(),dict(battery=3.0,yaw=float('nan')))
        await asyncio.wait_for(t.guard(),.5)
        self.assertTrue(t.abort.is_set())
        self.assertIn('Yaw',t.reason)

    async def test_stale_telemetry_still_aborts_without_battery_cutoff(self):
        t=self.trial(0,0)
        t.latest[1]=(time.monotonic()-2,dict(battery=3.0,yaw=0))
        await asyncio.wait_for(t.guard(),.5)
        self.assertTrue(t.abort.is_set())
        self.assertIn('Telemetria',t.reason)
    async def test_separate_push_or_correction_stays_active(self):
        for forward,yaw in ((.03,0),(0,.25),(0,0)):
            t=self.trial(forward,yaw)
            task=asyncio.create_task(t.guard())
            try:
                await asyncio.sleep(.12)
                self.assertFalse(t.abort.is_set())
            finally:
                task.cancel();await asyncio.gather(task,return_exceptions=True)
    async def test_done_disarm_is_completion_not_a_fault(self):
        t=self.trial(0,0,True)
        await asyncio.wait_for(t.guard(),.5)
        self.assertFalse(t.abort.is_set())

    async def test_live_camera_without_target_can_prepare_search(self):
        t=self.trial(0,0)
        sent=[]
        t.latest[2]=(time.monotonic(),dict(nicla_flag=0))
        t.latest[7]=(time.monotonic(),dict(nicla_fresh=1,nicla_reset_pending=0,
            nicla_protocol=0x4e03,nicla_state=0))
        async def send(command):
            sent.append(command)
            if command.get('cmd')=='vision_bench_start':
                fields=dict(mode=22,armed=0,motor1=0,motor2=0)
                t.rx_counts[7]=t.rx_counts.get(7,0)+1
            else:fields=dict(mode=8,armed=1 if command['arm']==1 else 0,motor1=0,motor2=0)
            t.latest[3]=(time.monotonic(),fields)
        async def wait(predicate,timeout=6):
            # Model arrival of the first requalified range after the ESC pause.
            if not predicate():t.rx_counts[18]=t.rx_counts.get(18,0)+1
            self.assertTrue(predicate())
        t.send=send;t.wait=wait
        self.assertFalse(t.target())
        await t.prepare_acquisition(dict(cmd='control',mode=8,arm=-1,aux={7:0,8:0}))
        self.assertEqual(sent[-1]['arm'],1)
        self.assertFalse(t.target())

    async def test_missing_camera_refused_before_arming(self):
        t=self.trial(0,0);sent=[]
        t.latest[7]=(time.monotonic(),dict(nicla_fresh=0,nicla_reset_pending=1))
        async def send(command):
            sent.append(command)
            t.latest[3]=(time.monotonic(),dict(mode=22,armed=0))
            t.rx_counts[7]=1
        async def wait(predicate,timeout=6):
            if not predicate():raise launcher.TrialError('Camara sin enlace')
        t.send=send;t.wait=wait
        with self.assertRaises(launcher.TrialError):
            await t.prepare_acquisition(dict(cmd='control',mode=8,arm=-1))
        self.assertFalse(any(c.get('arm')==1 for c in sent))

    async def test_search_without_target_is_not_a_guard_fault(self):
        t=self.trial(0,.2)
        t.latest[5]=(time.monotonic(),dict(mission_state=0,visited=0))
        t.latest[2]=(time.monotonic(),dict(nicla_flag=0))
        t.latest[12]=(time.monotonic(),dict(impulse_phase=5,impulse_forward=0,impulse_yaw=.2))
        task=asyncio.create_task(t.guard())
        try:
            await asyncio.sleep(.12)
            self.assertFalse(t.abort.is_set())
        finally:
            task.cancel();await asyncio.gather(task,return_exceptions=True)

if __name__=='__main__':unittest.main()
