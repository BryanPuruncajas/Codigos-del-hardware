"""XInput reader and the user's button map. Importing never opens hardware."""
import ctypes
from dataclasses import dataclass
import os

UP, DOWN, LEFT, RIGHT = 0x0001, 0x0002, 0x0004, 0x0008
START, BACK, RB = 0x0010, 0x0020, 0x0200
A, B, X, Y = 0x1000, 0x2000, 0x4000, 0x8000
MOVEMENT = A | UP | DOWN | LEFT | RIGHT
NAMES = {A:'A subir', B:'B +5%', X:'X -5%', Y:'Y manual/armar',
         UP:'arriba avanzar', DOWN:'abajo retroceder', LEFT:'izquierda',
         RIGHT:'derecha', RB:'RB waypoint', BACK:'View STOP', START:'Menu automatico'}

class GamepadDisconnected(RuntimeError):
    pass

class _Gamepad(ctypes.Structure):
    _fields_ = [('buttons',ctypes.c_uint16),('lt',ctypes.c_uint8),('rt',ctypes.c_uint8),
                ('lx',ctypes.c_int16),('ly',ctypes.c_int16),('rx',ctypes.c_int16),('ry',ctypes.c_int16)]

class _State(ctypes.Structure):
    _fields_ = [('packet',ctypes.c_uint32),('pad',_Gamepad)]

class XInput:
    def __init__(self, index=None):
        if os.name!='nt':raise OSError('XInput requiere Windows')
        self.dll=None
        for name in ('xinput1_4.dll','xinput1_3.dll','xinput9_1_0.dll'):
            try:self.dll=ctypes.WinDLL(name);break
            except OSError:continue
        if self.dll is None:raise OSError('Windows no dispone de XInput')
        self.get=self.dll.XInputGetState
        self.get.argtypes=[ctypes.c_uint32,ctypes.POINTER(_State)]
        self.get.restype=ctypes.c_uint32
        self.index=index
        if index is None:
            for candidate in range(4):
                state=_State()
                if self.get(candidate,ctypes.byref(state))==0:self.index=candidate;break
        if self.index is None:raise GamepadDisconnected('No se detecta un mando Xbox/XInput')

    def read(self):
        state=_State();error=self.get(self.index,ctypes.byref(state))
        if error:raise GamepadDisconnected(f'Mando desconectado (XInput {error})')
        return state.pad.buttons

@dataclass
class ButtonMap:
    power: int=30
    previous: int=0

    def update(self, buttons):
        pressed=buttons & ~self.previous;self.previous=buttons
        events=[]
        if pressed & BACK:return ['stop']
        increase=bool(pressed&B);decrease=bool(pressed&X)
        if increase!=decrease:
            self.power=max(0,min(50,self.power+(5 if increase else -5)))
            events.append('power')
        if pressed&Y:events.append('toggle')
        if pressed&RB:events.append('waypoint')
        if pressed&START:events.append('start')
        return events

    def manual(self, buttons):
        fx=fz=tz=0
        if self.power>0:
            # Existing P02 moves one axis at a time. Lift has priority if A
            # and a direction are held together; no unvalidated mixed vector.
            if buttons&A:fz=1
            else:
                horizontal=buttons&(UP|DOWN|LEFT|RIGHT)
                if horizontal==UP:fx=1
                elif horizontal==DOWN:fx=-1
                elif horizontal==RIGHT:tz=1
                elif horizontal==LEFT:tz=-1
        return dict(cmd='control',mode=11,fx=fx,fz=fz,tx=0,tz=tz,
                    arm=0,reload=0,reset=0,aux={5:self.power})

def button_names(buttons):
    return ', '.join(name for mask,name in NAMES.items() if buttons&mask) or 'neutral'
