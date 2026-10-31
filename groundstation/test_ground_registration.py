"""A rebooted telemetry route is restored without any control packet."""
import asyncio,sys
from pathlib import Path
from types import SimpleNamespace

project=Path(r'C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware')
sys.path.insert(0,str(project/'groundstation'))
sys.path.insert(0,str(Path(__file__).resolve().parent))
import telemetry_bridge as bridge

class Ground:
    def __init__(self):self.peers=set();self.registered=False;self.controls=[]
    def write(self,data):
        # Fake Ground accepts peer/routing records; any actuator record fails.
        for offset in range(0,len(data),7):
            cmd=data[offset:offset+1];address=data[offset+1:offset+7]
            assert len(address)==6 and cmd in (b'A',b'G')
            if cmd==b'A':self.peers.add(address)
            if cmd==b'G':self.registered=address in self.peers
        return len(data)

async def main():
    ground=Ground();link=SimpleNamespace(ser=ground)
    assert bridge.registration_due(10,None,0)
    assert not bridge.registration_due(10,(9.9,0),0)
    assert not bridge.registration_due(10,None,8)
    assert bridge.registration_due(10,(0,1),0) # recovery never changes the old ARM state
    await bridge.refresh_registration(link,'dc:b4:d9:39:b3:b4')
    assert ground.registered and not ground.controls
    print('PASS reboot routing recovery uses only A/G; fresh data/cooldown suppress retry; no control frames')
asyncio.run(main())
