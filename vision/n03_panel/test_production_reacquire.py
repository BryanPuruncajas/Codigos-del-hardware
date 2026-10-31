"""Regression of the real production terminal-LOST bug, not a panel mock."""
import unittest
from test_production_near import ns,region,seeded

class ReacquisitionTests(unittest.TestCase):
    def test_same_full_returns_after_one_minute_without_reset(self):
        t=seeded()
        for now in (1000,4000,10000,60000):t.update([],now)
        self.assertEqual(t.state,'SEARCH');self.assertIsNone(t.current)
        for now in range(60100,61100,100):t.update([region()],now)
        self.assertIsNone(t.current)
        t.update([region()],61100)
        self.assertEqual(t.state,'FULL');self.assertEqual(t.association_epoch,0)
    def test_same_large_crop_returns_after_one_minute(self):
        t=seeded();crop=region(0,20,170,140,'P')
        t.update([crop],400);t.update([],60000)
        for now in range(60100,61200,100):t.update([crop],now)
        self.assertEqual(t.current,crop);self.assertEqual(t.state,'PARTIAL')
        self.assertEqual(t.association_epoch,0)
    def test_changed_location_and_scale_can_be_recognised_again(self):
        t=seeded();t.update([],60000)
        other=region(145,30,60,60)
        for now in range(60100,61200,100):t.update([other],now)
        self.assertEqual(t.current,other);self.assertEqual(t.association_epoch,1)
        self.assertIsNone(t.near_seed)
    def test_new_partial_does_not_inherit_old_full_proof(self):
        t=seeded();t.update([],60000);other=region(0,20,60,80,'P')
        for now in range(60100,61200,100):t.update([other],now)
        self.assertEqual(t.current,other);self.assertEqual(t.association_epoch,1)
        self.assertIsNone(t.near_seed);self.assertFalse(t.near_locked)
    def test_two_candidates_wait_even_after_long_outage(self):
        t=seeded();t.update([],60000)
        for now in range(60100,62000,100):t.update([region(),region(145,30,60,60)],now)
        self.assertIsNone(t.current);self.assertEqual(t.state,'AMBIG')
        self.assertEqual(t.association_epoch,0)
    def test_frame_wrap_does_not_forge_an_association_or_reset_ack(self):
        a=ns['wire_sample'](255,7);b=ns['wire_sample'](256,7)
        self.assertEqual(a>>8,7);self.assertEqual(b>>8,7)
        frames=[]
        class UART:
            def write(self,data):frames.append(data)
        ns['send_observation'](UART(),None,b,'SEARCH',3)
        values=[int.from_bytes(frames[-1][2+2*i:4+2*i],'little') for i in range(14)]
        self.assertEqual(values[0],0);self.assertEqual(values[10],b)
        self.assertEqual(values[11],0);self.assertEqual(values[12],0x4e03);self.assertEqual(values[13],3)
        self.assertEqual(int.from_bytes(frames[-1][30:],'little'),65535-sum(frames[-1][:30]))

if __name__=='__main__':unittest.main()
