"""One Ground pilot at a time; host heartbeat loss sends STOP."""
import asyncio
import time

class PilotLease:
    def __init__(self, stop, timeout=.75):
        self.stop,self.timeout=stop,timeout
        self.owner=None;self.mac=None;self.last_ping=0.;self.task=None
        self.closing=False

    async def claim(self, owner, mac, actuator_status, allow_armed=False):
        if self.owner is not None or self.closing:raise ValueError('Ya hay un mando con control exclusivo')
        if actuator_status is None or time.monotonic()-actuator_status[0]>2 or actuator_status[1] not in ((0,1) if allow_armed else (0,)):
            raise ValueError('El mando requiere F3 reciente'+('' if allow_armed else ' desarmado')+' para iniciar')
        self.owner,self.mac=owner,mac.lower();self.last_ping=time.monotonic()
        self.task=asyncio.create_task(self.watch())

    def ping(self, owner, mac):
        if owner is not self.owner or mac.lower()!=self.mac:raise ValueError('Sesion de mando no activa')
        self.last_ping=time.monotonic()

    def check(self, owner, mac):
        if self.owner is not None and (owner is not self.owner or mac.lower()!=self.mac):
            raise ValueError('El mando Xbox tiene el control; STOP sigue disponible')

    async def watch(self):
        try:
            while self.owner is not None:
                await asyncio.sleep(.05)
                if time.monotonic()-self.last_ping>self.timeout:
                    await self.release(self.owner)
                    return
        except asyncio.CancelledError:
            raise

    async def release(self, owner):
        if owner is not self.owner:return False
        mac=self.mac;task=self.task
        self.closing=True
        self.owner=None;self.mac=None;self.task=None
        if task is not None and task is not asyncio.current_task():
            task.cancel();await asyncio.gather(task,return_exceptions=True)
        try:await self.stop(mac)
        finally:self.closing=False
        return True
