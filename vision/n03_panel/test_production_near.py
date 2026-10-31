"""Near continuity of the actual standalone main, without OpenMV hardware."""
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest

source=Path(__file__).with_name('nicla_balloon_mission_main.py')
tree=ast.parse(source.read_text(encoding='utf-8'))
tree.body=[n for n in tree.body if isinstance(n,(ast.Assign,ast.ClassDef,ast.FunctionDef))]
ns={'time':SimpleNamespace(ticks_diff=lambda a,b:a-b)}
exec(compile(tree,str(source),'exec'),ns)

def region(x=70,y=40,w=100,h=80,state='F',cx=None,cy=None,fill=.65):
    return (x,y,w,h,x+w//2 if cx is None else cx,y+h//2 if cy is None else cy,
            int(fill*w*h),int(state!='F'),w/h,fill,.8,.5,int(state=='F'),'EDGE' if state!='F' else 'OK',state,0)

def seeded():
    tracker=ns['Tracker']()
    for now in (0,80,160,240,320):tracker.update([region()],now)
    assert tracker.locked and tracker.near_seed is not None
    return tracker

class NearTests(unittest.TestCase):
    def test_grow_clip_and_fill_frame_keeps_confirmed_track(self):
        t=seeded();t.update([region(0,20,170,140,'P')],400)
        self.assertEqual(t.state,'PARTIAL');self.assertTrue(t.near_locked)
        filled=region(0,0,240,160,'D',cx=70,cy=110)
        t.update([filled],480)
        self.assertEqual(t.current,filled);self.assertEqual(t.state,'PARTIAL')
        # A new target can NEVER be acquired from this shape/full-frame red.
        for state in ('P','D'):
            new=ns['Tracker']()
            for now in (0,80,160,240,320):new.update([region(0,0,240,160,state)],now)
            self.assertFalse(new.locked)
    def test_recovers_unique_partial_after_loss_without_requiring_full(self):
        t=seeded();crop=region(0,20,170,140,'P')
        t.update([crop],400);t.update([],500)
        self.assertIsNone(t.current)
        for now in (1100,1200,1300,1400,1500):t.update([crop],now)
        self.assertIsNone(t.current) #500ms still not complete
        t.update([crop],1600)
        self.assertEqual(t.current,crop);self.assertEqual(t.state,'PARTIAL')
    def test_late_near_recovery_still_works_with_longer_confirmation(self):
        crop=region(0,20,170,140,'P')
        t=seeded();t.update([crop],400);t.update([],600)
        for now in (7000,7100,7200,7300,7400,7500):t.update([crop],now)
        self.assertEqual(t.current,crop)
        expired=seeded();expired.update([crop],400)
        for now in (11000,11100,11200,11300,11400,11500):expired.update([crop],now)
        self.assertIsNone(expired.current)
        for now in (11600,11700,11800,11900,12000):expired.update([crop],now)
        self.assertEqual(expired.current,crop)
    def test_ambiguous_or_displaced_regions_do_not_replace_target(self):
        t=seeded();crop=region(0,20,170,140,'P')
        t.update([crop,region(80,30,130,100,'P')],400)
        self.assertEqual(t.state,'AMBIG');self.assertIsNone(t.current)
        t=seeded();t.update([region(220,0,20,120,'P')],400)
        self.assertIsNone(t.current)
    def test_partial_start_has_no_full_seed_and_cannot_promote_doubtful_crop(self):
        t=ns['Tracker']();crop=region(0,20,170,140,'P')
        for now in (0,80,160,240,320):t.update([crop],now)
        self.assertTrue(t.locked);self.assertIsNone(t.near_seed)
        t.update([region(0,0,240,160,'D')],400)
        self.assertIsNone(t.current)
    def test_wire_stays_partial_and_missing_has_no_old_coordinates(self):
        frames=[];uart=SimpleNamespace(write=frames.append)
        t=seeded();crop=region(0,20,170,140,'P');t.update([crop],400)
        ns['send_observation'](uart,t.current,5,t.state,3)
        values=[int.from_bytes(frames[-1][2+2*i:4+2*i],'little') for i in range(14)]
        self.assertEqual(values[0],0x42);self.assertEqual(values[11],ns['STATES']['PARTIAL'])
        self.assertEqual(values[12],0x4e03)
        t.update([],500);ns['send_observation'](uart,t.current,6,t.state,3)
        self.assertEqual(int.from_bytes(frames[-1][2:4],'little'),0)
        self.assertEqual(frames[-1][4:20],bytes(16))

if __name__=='__main__':unittest.main()
