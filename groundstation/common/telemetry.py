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
  18:['flow_fused_height','flow_fused_vz','flow_height_source','flow_height_filtered','flow_height_vz','flow_height_flags'],
  19:['flow_rate_x','flow_rate_y','flow_motion_score','flow_control_age_ms','flow_quality_used','flow_source_sequence'],
  17:['fixed_height_target','fixed_height_estimate','height_reference_bias','fixed_height_offset','fixed_height_flags','camera_level_pitch_deg'],
  16:['height_anchor','height_anchor_error','height_anchor_age_ms','height_drift_rate','height_noise_band','height_reference_flags'],
  15:['baro_motion_rate','baro_motion_confidence','baro_motion_flags','baro_motion_displacement','baro_motion_events','baro_motion_version'],
  14:['height_period_ms','height_window_ms','height_scale','height_source_vz','height_sample_age_ms','vertical_profile_version'],
  13:['inertia_vz','inertia_predicted_vz','inertia_target_vz','inertia_motor_floor','inertia_flags','inertia_velocity_confidence'],
  12:['impulse_phase','impulse_elapsed_ms','impulse_forward','impulse_yaw','impulse_count','impulse_max_ms'],
  11:['support_visual_x','support_yaw_reference','support_yaw_error','support_visual_bearing','support_version','support_forward_request'],
  10:['support_visual_y','support_visual_error','support_trim','support_vertical_demand','support_yaw_cmd','support_phase'],
  9:['baro_raw_height','baro_filtered_height','baro_raw_vz','baro_filtered_vz','baro_age_ms','baro_healthy'],
  8:['mission_motion_reason','mission_forward_request','mission_yaw_error_deg','mission_waypoint','mission_stable_ms','mission_bench'],
  1:['height','yaw','roll','pitch','battery','vertical_velocity'],
  2:['nicla_flag','nicla_x','nicla_y','nicla_w','nicla_h','nicla_distance'],
  3:['servo1','servo2','motor1','motor2','armed','mode'],
  4:['rollrate','pitchrate','yawrate','yaw_ref','height_ref','fx_cmd'],
  5:['mission_state','visited','target_count','search_height','visit_score','elapsed_s'],
  6:['flow_dx','flow_dy','flow_range_mm','flow_quality','flow_age_ms','flow_flags'],
  7:['nicla_fresh','nicla_state','nicla_sample','nicla_protocol','nicla_reset_pending','nicla_reset_ack'],
}
