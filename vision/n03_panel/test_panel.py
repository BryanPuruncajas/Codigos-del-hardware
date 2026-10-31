import io
import json
import sys
import types
import unittest
from pathlib import Path

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
sys.modules['sensor']=types.ModuleType('sensor')
ns={'__name__':'test'}
exec((HERE/'n02_core.py').read_text(encoding='utf-8'),ns)
ns['time']=types.SimpleNamespace(ticks_diff=lambda a,b:((a-b+2**29)%2**30)-2**29)
exec((HERE/'mission.py').read_text(encoding='utf-8'),ns)
from panel import read_packet,read_exact

def row(w=190,h=155):
    return (10,0,w,h,105,80,1000,1,1,.6,.9,.5,1,'OK','F',1)

def acquired():
    m=ns['Mission']()
    for t in [0,80,160,240,320]:m.update([row()],t)
    return m

def ready():
    m=acquired()
    for t in range(400,2401,100):m.update([row()],t)
    return m

class Tests(unittest.TestCase):
    def test_confirm(self):self.assertEqual(acquired().tracker.state,'FULL')
    def test_not_early(self):
        m=acquired();m.command(1);self.assertEqual(m.mode,'TRACK');self.assertIn('IGNORADO',m.reply)
    def test_arrival_dwell(self):
        m=acquired();m.update([row()],2319);self.assertFalse(m.ready)
        m.update([row()],2320);self.assertTrue(m.ready)
    def test_missing_resets_arrival(self):
        m=ready();m.update([],2450);self.assertFalse(m.ready)
        m.update([row()],2500);self.assertFalse(m.ready)
    def test_small_resets(self):
        m=ready();m.update([row(179)],2450);self.assertFalse(m.ready)
    def test_turn_blocks_acquire(self):
        m=ready();m.command(1);m.update([row()],9999)
        self.assertEqual(m.mode,'TURN');self.assertIsNone(m.tracker.current)
    def test_duplicate_is_idempotent(self):
        m=ready();m.command(1);m.command(1);self.assertEqual(m.mode,'TURN')
    def test_second_command(self):
        m=ready();m.command(1);m.command(2)
        self.assertEqual(m.target,2);self.assertEqual(m.mode,'TRACK');self.assertFalse(m.tracker.locked)
    def test_lost_does_not_arrive(self):
        m=acquired();m.update([],2321);m.command(1)
        self.assertEqual(m.tracker.state,'LOST');self.assertEqual(m.mode,'TRACK')
    def test_geometry_unmodified(self):self.assertEqual(ns['TRACK_LOST_MS'],2000)
    def test_packet(self):
        b=json.dumps({'seq':1}).encode();p=io.BytesIO(b'N03 %d 3\n'%len(b)+b+b'abc')
        self.assertEqual(read_packet(p),({'seq':1},b'abc'))
    def test_packet_invalid(self):
        with self.assertRaises(ValueError):read_packet(io.BytesIO(b'N03 1 999999999\n'))
    def test_packet_error(self):
        with self.assertRaises(RuntimeError):read_packet(io.BytesIO(b'Traceback\n'))
    def test_read_timeout(self):
        with self.assertRaises(TimeoutError):read_exact(io.BytesIO(),3,deadline=.001)
    def test_wrap(self):
        m=acquired();m.near_since=2**30-1000;m.tracker.last_seen=0
        m.update([row()],1000);self.assertTrue(m.ready)

if __name__=='__main__':unittest.main()
