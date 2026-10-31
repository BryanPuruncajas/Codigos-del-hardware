"""AUX4 extension; preserves the existing 13 float control channels."""
import math

def pack_height_options(*, released, advance=True, offset_cm=0, deadband=24, level_pitch_deg=0):
    if not math.isfinite(offset_cm) or not -50 <= offset_cm <= 50:
        raise ValueError('height offset must be -50..50 cm')
    if not math.isfinite(level_pitch_deg) or not -30 <= level_pitch_deg <= 30:
        raise ValueError('level pitch must be -30..30 degrees')
    if not 1 <= deadband <= 100:
        raise ValueError('deadband must be 1..100 pixels')
    return float((1 << 23) | int(bool(released)) | (int(bool(advance)) << 1) |
                 ((round(offset_cm) + 50) << 2) | (round(deadband) << 9) |
                 (round((level_pitch_deg + 30) * 2) << 16))
