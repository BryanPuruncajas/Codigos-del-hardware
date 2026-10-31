import unittest
from test_panel import ns, row

def at(cx, w=40):
    r=list(row(w,40));r[0]=cx-w//2;r[4]=cx
    return tuple(r)

def feed(t, rows):
    for now in (0,80,160,240,320):t.update(rows,now)

class CentralTests(unittest.TestCase):
    def test_outside(self):
        t=ns['Tracker']();feed(t,[at(89),at(151)])
        self.assertEqual(t.state,'SEARCH');self.assertFalse(t.locked)
    def test_boundaries(self):
        for cx in (90,150):
            t=ns['Tracker']();feed(t,[at(cx)])
            self.assertTrue(t.locked)
    def test_larger_outside_ignored(self):
        t=ns['Tracker']();feed(t,[at(110),at(200,80)])
        self.assertEqual(t.current[4],110)
    def test_two_central_ambiguous(self):
        t=ns['Tracker']();feed(t,[at(100),at(140,80)])
        self.assertEqual(t.state,'AMBIG');self.assertFalse(t.locked)
    def test_track_outside_after_acquire(self):
        t=ns['Tracker']();feed(t,[at(145)])
        t.update([at(160)],400);self.assertEqual(t.state,'FULL')
        self.assertEqual(t.current[4],160)
    def test_partial_not_acquired(self):
        r=list(at(120));r[14]='P';t=ns['Tracker']();feed(t,[tuple(r)])
        self.assertFalse(t.locked)
    def test_exit_resets_confirmation(self):
        t=ns['Tracker']();t.update([at(100)],0);t.update([at(80)],100)
        t.update([at(100)],400);self.assertFalse(t.locked);self.assertEqual(t.hits,1)

if __name__=='__main__':unittest.main()
