"""Tests the standalone file actually copied to main.py, not the panel core."""
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest

HERE=Path(__file__).resolve().parent
source=HERE/'nicla_balloon_mission_main.py'
tree=ast.parse(source.read_text(encoding='utf-8'))
tree.body=[n for n in tree.body if isinstance(n,(ast.Assign,ast.ClassDef,ast.FunctionDef))]
ns={'time':SimpleNamespace(ticks_diff=lambda a,b:a-b)}
exec(compile(tree,str(source),'exec'),ns)

def row(cx=120,cy=80,w=26,h=26,state='F'):
    return (cx-w//2,cy-h//2,w,h,cx,cy,400,int(state=='P'),w/h,.6,.8,.5,int(state=='F'),'OK',state,1)

def feed(tracker,rows):
    for now in (0,80,160,240,320):tracker.update(rows,now)

class ProductionAcquisitionTests(unittest.TestCase):
    def test_offcentre_visible_target_is_acquired(self):
        for cx in (20,40,80,120,170,200,220):
            t=ns['Tracker']();feed(t,[row(cx=cx)])
            self.assertTrue(t.locked,cx)
            self.assertEqual(t.current[4],cx)
    def test_two_visible_targets_do_not_select_arbitrarily(self):
        t=ns['Tracker']();feed(t,[row(cx=100),row(cx=200)])
        self.assertEqual(t.state,'AMBIG');self.assertFalse(t.locked)
    def test_frame_edge_or_rejected_shape_not_promoted(self):
        for r in (row(cx=10),row(cx=230),row(state='D'),row(state='X')):
            t=ns['Tracker']();feed(t,[r]);self.assertFalse(t.locked)
    def test_confirmation_still_requires_time(self):
        t=ns['Tracker']();t.update([row(cx=200)],0);t.update([row(cx=200)],80)
        self.assertEqual(t.state,'CONFIRM');self.assertIsNone(t.current)
    def test_raw_candidate_wire_cannot_be_a_confirmed_target(self):
        frames=[];uart=SimpleNamespace(write=frames.append)
        for state in ('SEARCH','CONFIRM','AMBIG','LOST'):
            ns['send_observation'](uart,None,19,state,7,row(cx=200,state='D'))
            p=frames[-1];v=[int.from_bytes(p[2+2*i:4+2*i],'little') for i in range(14)]
            self.assertEqual(v[0],0)
            self.assertEqual(v[5:9],[200,80,26,26])
            self.assertEqual(v[10:],[19,ns['STATES'][state],0x4e03,7])
            self.assertEqual(int.from_bytes(p[30:],'little'),65535-sum(p[:30]))
    def test_confirmed_wire_and_geometry_unchanged(self):
        frames=[];uart=SimpleNamespace(write=frames.append)
        ns['send_observation'](uart,row(cx=200),19,'FULL',7)
        self.assertEqual(int.from_bytes(frames[-1][2:4],'little'),0x41)
        self.assertEqual(ns['RED_LAB'],(2,80,20,80,-5,70))
        self.assertEqual(ns['MIN_ROUNDNESS'],.45)
    def test_unique_rejected_region_is_visible_for_diagnostics(self):
        r=row(cx=200,state='D')
        self.assertEqual(ns['diagnostic_candidate']([r]),r)
        self.assertIsNone(ns['diagnostic_candidate']([]))

if __name__=='__main__':unittest.main()
