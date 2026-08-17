from dataclasses import dataclass
from typing import Optional

@dataclass
class Telemetry:
    flag: int
    values: list[float]

def parse(line: str) -> Optional[Telemetry]:
    if not line.startswith('TEL,'): return None
    parts=line.split(',')
    if len(parts)!=8: return None
    try: return Telemetry(int(parts[1]),[float(x) for x in parts[2:]])
    except ValueError: return None

LABELS={
  1:['height','yaw','roll','pitch','battery','vertical_velocity'],
  2:['nicla_flag','nicla_x','nicla_y','nicla_w','nicla_h','nicla_distance'],
  3:['servo1','servo2','motor1','motor2','armed','mode'],
  4:['rollrate','pitchrate','yawrate','yaw_ref','height_ref','fx_cmd'],
  5:['mission_state','visited','target_count','search_height','visit_score','elapsed_s'],
}
