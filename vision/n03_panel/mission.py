"""Se concatena tras n02_core.py. Sin UART, GPIO ni actuadores."""
class Mission:
    def __init__(self):
        self.tracker = Tracker()
        self.target = 1
        self.mode = 'TRACK'
        self.near_since = None
        self.ready = False
        self.seq = 0
        self.reply = 'INICIO'

    def update(self, rows, now):
        if self.mode == 'TURN':
            return
        self.tracker.update(rows, now)
        r = self.tracker.current
        near = r is not None and r[2] >= 180 and r[3] >= 150
        if not near:
            self.near_since = None
        elif self.near_since is None:
            self.near_since = now
        self.ready = near and time.ticks_diff(now, self.near_since) >= 2000

    def command(self, seq):
        # Retentar el mismo numero nunca avanza otra etapa.
        if seq <= self.seq:
            return
        self.seq = seq
        if self.mode == 'TURN':
            self.target += 1
            self.tracker = Tracker()
            self.mode = 'TRACK'
            self.reply = 'BUSCAR_SIGUIENTE'
        elif self.ready and self.tracker.current is not None:
            self.mode = 'TURN'
            self.tracker.current = None
            self.reply = 'GIRO_AUTORIZADO_MANUAL'
        else:
            self.reply = 'IGNORADO_SIN_LLEGADA'
        self.ready = False
        self.near_since = None

    def status(self, now):
        r = self.tracker.current
        return dict(version=VERSION, acquire_x=[ACQUIRE_MIN_X, ACQUIRE_MAX_X], ms=now, target=self.target,
                    mode=self.mode, state='GIRO' if self.mode == 'TURN' else self.tracker.state,
                    observed=r is not None, rect=list(r[:6]) if r else [-1]*6,
                    ready=self.ready, seq=self.seq, reply=self.reply)
