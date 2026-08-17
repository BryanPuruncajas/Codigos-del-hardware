import struct, time

def mac_to_bytes(mac: str) -> bytes:
    return bytes(int(x,16) for x in mac.split(':'))

class BlimpLink:
    def __init__(self, port, baud=115200):
        try:
            import serial
        except ImportError as e:
            raise RuntimeError("Falta pyserial. Ejecuta: pip install -r requirements.txt") from e
        self.ser=serial.Serial(port,baud,timeout=0.2)
        time.sleep(2)
        self.ser.reset_input_buffer()
    def close(self):
        if self.ser.is_open: self.ser.close()
    def _line(self, timeout=1.0):
        end=time.time()+timeout
        while time.time()<end:
            s=self.ser.readline().decode(errors='ignore').strip()
            if s: return s
        return ''
    def add_peer(self,mac): self.ser.write(b'A'+mac_to_bytes(mac)); return self._line()
    def register_ground(self,mac): self.ser.write(b'G'+mac_to_bytes(mac)); return self._line()
    def send(self,mac,params):
        if len(params)!=13: raise ValueError('ControlInput requiere 13 floats')
        self.ser.write(b'C'+mac_to_bytes(mac)+struct.pack('<13f',*params))
        return self._line(0.4)
    def control(self,mac,mode=0,fx=0,fz=0,tx=0,tz=0,arm=0,reload=0,reset=0,aux=None):
        p=[0.0]*13; p[0]=mode; p[1]=fx; p[2]=fz; p[3]=tx; p[4]=tz
        if aux:
            for idx,val in aux.items(): p[idx]=val
        p[10]=arm; p[11]=reload; p[12]=reset
        return self.send(mac,p)
    def arm(self,mac): return self.control(mac,mode=0,arm=1)
    def disarm(self,mac): return self.control(mac,mode=0,arm=-1)
    def stop(self,mac):
        # SAFE_STOP desarma también en firmware
        for _ in range(3): self.control(mac,mode=0,arm=-1); time.sleep(0.05)
    def set_preference(self,mac,key,value,dtype):
        kb=key.encode('ascii')
        if dtype==1: vb=struct.pack('<i',int(value))
        elif dtype==2: vb=struct.pack('<f',float(value))
        elif dtype==4: vb=struct.pack('<?',bool(value))
        else: raise ValueError('dtype soportado: 1 int, 2 float, 4 bool')
        payload=bytes([0x68,dtype,len(kb)])+kb+vb
        self.ser.write(b'D'+mac_to_bytes(mac)+payload)
        return self._line()
    def set_float(self,mac,key,value): return self.set_preference(mac,key,value,2)
    def set_bool(self,mac,key,value): return self.set_preference(mac,key,value,4)
    def lines(self,seconds=None):
        end=time.time()+seconds if seconds else None
        while end is None or time.time()<end:
            s=self.ser.readline().decode(errors='ignore').strip()
            if s: yield s
