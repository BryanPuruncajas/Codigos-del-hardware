"""Regression checks for stale/cached observations and association changes."""
import ast
from pathlib import Path
import unittest
from check_arrival_reference import Reference


class EvidenceTests(unittest.TestCase):
    def image(self, flag=66, width=230):
        return dict(nicla_flag=flag, nicla_w=width)

    def seed(self, ref):
        ref.update(0, self.image(65, 100), 256, True)

    def test_crop_alone_never_proves_arrival(self):
        ref = Reference()
        for i in range(6):
            self.assertFalse(ref.update(i*.2, self.image(), 256+i, True))

    def test_seed_and_new_samples_can_confirm(self):
        ref = Reference(); self.seed(ref)
        results = [ref.update(i*.2, self.image(), 257+i, True) for i in range(4)]
        self.assertEqual(results, [False, False, False, True])

    def test_width119_remains_below_reference(self):
        ref = Reference(); self.seed(ref)
        for i in range(10):
            self.assertFalse(ref.update(i*.2, self.image(width=119), 257+i, True))

    def test_exact120_confirms_after_required_samples(self):
        ref = Reference(); self.seed(ref)
        results = [ref.update(i*.2, self.image(width=120), 257+i, True) for i in range(4)]
        self.assertEqual(results, [False, False, False, True])

    def test_historical220_can_be_replayed_explicitly(self):
        ref = Reference(220); self.seed(ref)
        for i in range(8):
            self.assertFalse(ref.update(i*.2, self.image(width=215), 257+i, True))

    def test_repeated_sample_does_not_confirm(self):
        ref = Reference(); self.seed(ref)
        for i in range(10):
            self.assertFalse(ref.update(i*.2, self.image(), 257, True))

    def test_missing_breaks_confirmation(self):
        ref = Reference(); self.seed(ref)
        for i in range(3): ref.update(i*.2, self.image(), 257+i, True)
        self.assertFalse(ref.update(.6, self.image(), 260, False))
        self.assertFalse(ref.update(.7, self.image(), 261, True))

    def test_new_association_cannot_inherit_full(self):
        ref = Reference(); self.seed(ref)
        for i in range(8):
            self.assertFalse(ref.update(i*.2, self.image(), 512+i, True))
        self.assertFalse(ref.had_full)

    def test_network_output_is_viewer_auth_only(self):
        tree = ast.parse(Path(__file__).with_name('check_arrival_reference.py').read_text(encoding='utf-8'))
        sends = [n for n in ast.walk(tree) if isinstance(n, ast.Call) and
                 isinstance(n.func, ast.Attribute) and n.func.attr == 'send']
        self.assertEqual(len(sends), 1)
        self.assertIn("role='viewer'", ast.unparse(sends[0]))


if __name__ == '__main__':
    unittest.main()
