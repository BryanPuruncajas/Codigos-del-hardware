"""Panel USB exclusivo. Carga en RAM; ninguna escritura en flash."""
import io
import json
import queue
import sys
import threading
import time
from datetime import datetime
from pathlib import Path
import tkinter as tk
from tkinter import ttk
from PIL import Image, ImageTk

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / '_deps'))

def read_exact(port, count, deadline=3):
    end = time.monotonic() + deadline
    data = bytearray()
    while len(data) < count:
        if time.monotonic() > end:
            raise TimeoutError('USB: paquete incompleto')
        data.extend(port.read(count-len(data)))
    return bytes(data)

def upload_ram(port, source):
    port.write(b'\r\x03\x03\x02')
    time.sleep(.3)
    port.reset_input_buffer()
    port.write(b'\r\x01')
    banner = bytearray()
    end = time.monotonic()+4
    while not (b'raw REPL' in banner and banner.endswith(b'>')) and time.monotonic() < end:
        banner.extend(port.read(1))
    if not (b'raw REPL' in banner and banner.endswith(b'>')):
        raise RuntimeError('No responde raw REPL. Respuesta: %r' % bytes(banner[-240:]))
    for off in range(0,len(source),128):
        port.write(source[off:off+128])
        time.sleep(.01)
    port.write(b'\x04')
    if read_exact(port,2) != b'OK':
        raise RuntimeError('La Nicla no acepto el programa en RAM')

def read_packet(port):
    header = bytearray()
    end = time.monotonic()+5
    while not header.endswith(b'\n'):
        if time.monotonic()>end:
            raise TimeoutError('Sin imagen durante 5 s')
        header.extend(port.read(1))
        if len(header)>512:
            raise RuntimeError('Respuesta inesperada: '+repr(bytes(header[:180])))
    if not header.startswith(b'N03 '):
        raise RuntimeError('Respuesta de la placa: '+repr(bytes(header)))
    parts=header.split()
    if len(parts)!=3:
        raise ValueError('Cabecera invalida')
    n,j=map(int,parts[1:])
    if not 0<n<=65536 or not 0<j<=200000:
        raise ValueError('Tamano de paquete invalido')
    return json.loads(read_exact(port,n)),read_exact(port,j)

class Panel:
    def __init__(self, root):
        self.root=root
        self.events=queue.Queue(maxsize=4)
        self.commands=queue.Queue()
        self.stop=threading.Event()
        self.worker=None
        self.pending=False
        self.held=False
        self.seq=0
        self.last_frame=0
        self.fresh=False
        root.title('N03 - PRUEBA SIN MOTORES')
        root.geometry('900x780')
        self.port=tk.StringVar(value='COM10')
        bar=ttk.Frame(root);bar.pack(pady=8)
        ttk.Label(bar,text='Puerto Nicla:').pack(side='left')
        ttk.Entry(bar,textvariable=self.port,width=12).pack(side='left')
        self.connect=ttk.Button(bar,text='Conectar e iniciar',command=self.start);self.connect.pack(side='left',padx=8)
        ttk.Button(bar,text='Detener (Esc)',command=self.halt).pack(side='left')
        self.state=tk.StringVar(value='Deten y desconecta OpenMV IDE antes de conectar aqui.')
        ttk.Label(root,textvariable=self.state,font=('Segoe UI',20,'bold'),wraplength=870).pack(pady=6)
        self.video=ttk.Label(root);self.video.pack()
        self.hint=tk.StringVar(value='1 confirma llegada / 1 termina giro. El panel debe tener el foco.')
        ttk.Label(root,textvariable=self.hint,font=('Segoe UI',13),wraplength=870).pack(pady=8)
        ttk.Label(root,text='Llegada experimental: 180 x 150 px durante 2 s. NO confirma contacto.').pack()
        self.log_label=tk.StringVar()
        ttk.Label(root,textvariable=self.log_label,wraplength=870).pack(pady=5)
        root.bind('<KeyPress-1>',self.key)
        root.bind('<KeyRelease-1>',lambda e:setattr(self,'held',False))
        root.bind('<Escape>',lambda e:self.halt())
        root.protocol('WM_DELETE_WINDOW',self.close)
        root.after(50,self.poll)

    def key(self,event):
        if self.held:return
        self.held=True
        if self.fresh and not self.pending and self.worker and self.worker.is_alive():
            self.seq+=1;self.pending=True
            self.commands.put(self.seq)
            self.hint.set('Esperando confirmacion de la Nicla...')

    def emit(self,item):
        try:self.events.put_nowait(item)
        except queue.Full:
            try:self.events.get_nowait()
            except queue.Empty:pass
            self.events.put_nowait(item)

    def start(self):
        if self.worker and self.worker.is_alive():return
        self.stop.clear();self.pending=False;self.seq=0;self.fresh=False
        while not self.commands.empty():self.commands.get_nowait()
        while not self.events.empty():self.events.get_nowait()
        self.connect.config(state='disabled')
        self.state.set('Conectando y cargando prueba en RAM...')
        self.root.focus_set()
        self.worker=threading.Thread(target=self.run,args=(self.port.get().strip(),),daemon=True)
        self.worker.start()

    def run(self,name):
        port=None
        try:
            import serial
            source='\n'.join((HERE/f).read_text(encoding='utf-8') for f in ['n02_core.py','mission.py','board_main.py']).encode()
            port=serial.Serial(port=None,baudrate=115200,timeout=.1,write_timeout=2)
            port.dtr=True;port.rts=False;port.port=name;port.open()
            upload_ram(port,source)
            logs=HERE/'logs';logs.mkdir(exist_ok=True)
            path=logs/(datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.jsonl')
            self.emit(('log',str(path)))
            pending=None;sent=0;attempts=0
            with path.open('x',encoding='utf-8') as out:
                while not self.stop.is_set():
                    port.write(b'PING\n')
                    if pending is None:
                        try:pending=self.commands.get_nowait();attempts=0;sent=0
                        except queue.Empty:pass
                    if pending is not None and time.monotonic()-sent>=1:
                        if attempts>=3:raise TimeoutError('Comando sin ACK; no se avanza otra etapa')
                        port.write(('1 %d\n'%pending).encode());sent=time.monotonic();attempts+=1
                        out.write(json.dumps(dict(event='command',seq=pending,host_time=time.time()))+'\n');out.flush()
                    meta,jpeg=read_packet(port)
                    meta['host_time']=time.time()
                    out.write(json.dumps(meta)+'\n');out.flush()
                    if pending is not None and meta['seq']>=pending:pending=None
                    self.emit(('frame',meta,jpeg))
        except Exception as exc:
            self.emit(('error',str(exc)))
        finally:
            if port is not None:
                try:
                    if port.is_open:port.write(b'STOP\n');port.write(b'\x03\x02')
                except Exception:pass
                port.close()
            self.emit(('done',))

    def poll(self):
        try:
            while True:
                e=self.events.get_nowait()
                if e[0]=='frame':
                    meta,jpeg=e[1:]
                    self.last_frame=time.monotonic();self.fresh=True
                    if meta['seq']>=self.seq:self.pending=False
                    self.photo=ImageTk.PhotoImage(Image.open(io.BytesIO(jpeg)).resize((720,480)))
                    self.video.config(image=self.photo)
                    label='LLEGADA CANDIDATA' if meta['ready'] else meta['state']
                    self.state.set('T%d - %s'%(meta['target'],label))
                    if not self.pending:
                        self.hint.set('Pulsa 1 al terminar el giro.' if meta['mode']=='TURN' else ('Pulsa 1 para confirmar llegada e iniciar giro.' if meta['ready'] else 'Sigue al objetivo. 1 solo se acepta con llegada candidata.'))
                elif e[0]=='log':self.log_label.set('Log: '+e[1])
                elif e[0]=='error':self.state.set('ERROR: '+e[1]);self.fresh=False;self.video.config(image='')
                elif e[0]=='done':
                    self.connect.config(state='normal');self.fresh=False;self.pending=False
                    self.video.config(image='');self.hint.set('Desconectado. Puedes volver a OpenMV IDE.')
                    if not self.state.get().startswith('ERROR:'):self.state.set('DETENIDO')
        except queue.Empty:pass
        except Exception as exc:
            self.state.set('ERROR de imagen: '+str(exc));self.halt()
        if self.fresh and time.monotonic()-self.last_frame>1.5:
            self.fresh=False;self.video.config(image='');self.state.set('SIN IMAGEN ACTUAL - ordenes deshabilitadas')
        self.root.after(50,self.poll)

    def halt(self):
        self.stop.set();self.fresh=False

    def close(self):
        self.halt()
        if self.worker and self.worker.is_alive():
            self.root.after(100,self.close)
        else:self.root.destroy()

if __name__=='__main__':
    root=tk.Tk();Panel(root);root.mainloop()
