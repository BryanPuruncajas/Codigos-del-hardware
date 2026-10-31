import math
import struct
import unittest
from common.height_options import pack_height_options
from common.control_pack import build_control_payload, make_args

class HeightOptionsTests(unittest.TestCase):
    def test_float32_round_trip_at_limits(self):
        for cm in (-50,0,50):
            for pitch in (-30,0,30):
                for release in (False,True):
                    for advance in (False,True):
                        value=pack_height_options(released=release,advance=advance,
                            offset_cm=cm,level_pitch_deg=pitch)
                        self.assertEqual(struct.unpack('<f',struct.pack('<f',value))[0],value)
                        bits=int(value)
                        self.assertEqual(bool(bits&1),release)
                        self.assertEqual(bool(bits&2),advance)
                        self.assertEqual(((bits>>2)&127)-50,cm)
                        self.assertEqual((bits>>16)&127,round((pitch+30)*2))

    def test_keeps_existing_channels_and_height_parameter(self):
        fx,fz,tx,tz,aux,_=build_control_payload(make_args('p08',height=.8,
            alt_max_power=25,vis_min_power=10,vis_max_power=20,vis_deadband_px=24))
        aux[9]=pack_height_options(released=False)
        payload=[0.0]*13
        payload[:5]=[17,fx,fz,tx,tz]
        for index,value in aux.items():payload[index]=value
        self.assertEqual(len(struct.pack('<13f',*payload)),52)
        self.assertEqual(fz,.8)
        self.assertEqual(aux[5],.25)
        self.assertEqual(set(aux),{5,6,7,8,9})

    def test_invalid_adjustments_rejected_before_connection(self):
        for value in (-51,51,math.nan,math.inf):
            with self.assertRaises(ValueError):pack_height_options(released=True,offset_cm=value)
        for value in (-31,31,math.nan,math.inf):
            with self.assertRaises(ValueError):pack_height_options(released=True,level_pitch_deg=value)

if __name__=='__main__':unittest.main()
