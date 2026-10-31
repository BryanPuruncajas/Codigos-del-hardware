import unittest
from common.control_pack import make_args,build_control_payload
class SupportPack(unittest.TestCase):
    def test_legacy_unchanged(self):
        self.assertNotIn(9,build_control_payload(make_args('p03'))[4])
    def test_support_opt_in(self):
        fx,fz,tx,tz,aux,desc=build_control_payload(make_args('p03',yaw_deg=0,
            yaw_support_hold=True,yaw_base_power=16,yaw_max_power=16,
            yaw_min_power=0,yaw_servo_authority=25,yaw_kp=.4,yaw_kd=2,yaw_ki=.25))
        self.assertEqual(aux[9],1)
        self.assertEqual(aux[8],.16)
        self.assertEqual((fx,fz,tx,tz),(0,0,0,0))
        self.assertTrue(any('sin barometro/avance' in d for d in desc))
    def test_cannot_silently_enable_a_forward_mission(self):
        with self.assertRaises(ValueError):
            build_control_payload(make_args('p08',yaw_support_hold=True))
    def test_visual_prepare_and_release(self):
        args=dict(yaw_visual_hold=True,yaw_base_power=15,yaw_max_power=16,yaw_deg=0)
        prepared=build_control_payload(make_args('p03',yaw_support_prepare=True,**args))
        released=build_control_payload(make_args('p03',**args))
        self.assertEqual(prepared[0],1)
        self.assertEqual(released[0],0)
        self.assertEqual(prepared[4][9],2)
        self.assertEqual(prepared[4][8],.15)
        self.assertEqual(prepared[4],released[4])
        payload=(int(prepared[4][7])>>1)&0x3fffff
        self.assertEqual((payload&63)*.5,14)
    def test_visual_floor_configurable(self):
        packed=build_control_payload(make_args('p03',yaw_visual_hold=True,
            yaw_base_power=15,yaw_max_power=16,yaw_support_min_power=13))[4][7]
        self.assertEqual(((int(packed)>>1)&63)*.5,13)
    def test_visual_bad_floor_rejected(self):
        for floor in (float('nan'),-1,17):
            with self.assertRaises(ValueError):
                build_control_payload(make_args('p03',yaw_visual_hold=True,
                    yaw_base_power=15,yaw_max_power=16,yaw_support_min_power=floor))
        with self.assertRaises(ValueError):
            build_control_payload(make_args('p03',yaw_visual_hold=True,
                yaw_base_power=13,yaw_max_power=16,yaw_support_min_power=14))
    def test_visual_wrong_mode_rejected(self):
        with self.assertRaises(ValueError):
            build_control_payload(make_args('p08',yaw_visual_hold=True))
        with self.assertRaises(ValueError):
            build_control_payload(make_args('p03',yaw_support_prepare=True))
if __name__=='__main__':unittest.main()
