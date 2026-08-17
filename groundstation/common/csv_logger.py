import csv, time
from pathlib import Path
from .telemetry import LABELS

class CsvLogger:
    def __init__(self, folder='logs'):
        Path(folder).mkdir(parents=True,exist_ok=True)
        stamp=time.strftime('%Y%m%d_%H%M%S')
        self.path=Path(folder)/f'blimp_{stamp}.csv'
        self.f=self.path.open('w',newline='',encoding='utf-8')
        self.w=csv.writer(self.f)
        self.w.writerow(['pc_time','flag','name0','v0','name1','v1','name2','v2','name3','v3','name4','v4','name5','v5'])
    def add(self,tel):
        labels=LABELS.get(tel.flag,[f'v{i}' for i in range(6)])
        row=[time.time(),tel.flag]
        for n,v in zip(labels,tel.values): row += [n,v]
        self.w.writerow(row); self.f.flush()
    def close(self): self.f.close()
