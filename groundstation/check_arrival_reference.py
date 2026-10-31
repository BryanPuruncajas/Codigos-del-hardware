"""Viewer-only, voiced far/near/far calibration of the provisional width threshold.

Physical distances are operator measured, from camera to balloon surface.
This does NOT run the ESP arrival state machine or confirm physical contact.
"""
import argparse
import asyncio
import json
import math
import statistics
import time
from datetime import datetime
from pathlib import Path

import websockets
from check_visual_support import Speaker

ROOT = Path(__file__).resolve().parent.parent


class Reference:
    """Visual width evidence only; never claims the missing yaw/height gates."""
    def __init__(self, threshold_px=120):
        self.threshold_px = threshold_px
        self.epoch = None
        self.had_full = False
        self.since = None
        self.count = 0
        self.last_sample = None
        self.last_time = None

    def update(self, now, geometry, sample, valid):
        epoch = sample >> 8
        if epoch != self.epoch:
            self.epoch = epoch
            self.had_full = False
            self.since = None
            self.count = 0
            self.last_sample = None
            self.last_time = None
        confirmed = valid and geometry.get('nicla_flag') in (65, 66)
        if confirmed and geometry.get('nicla_flag') == 65:
            self.had_full = True
        width = geometry.get('nicla_w', 0)
        qualifies = confirmed and self.had_full and math.isfinite(width) and self.threshold_px <= width <= 240
        if not qualifies or (self.last_time is not None and now-self.last_time > .7):
            self.since = None
            self.count = 0
        if not qualifies:
            self.last_sample = None
            self.last_time = None
            return False
        if sample != self.last_sample:
            if self.since is None:
                self.since = now
            self.count += 1
            self.last_sample = sample
            self.last_time = now
        return self.count >= 3 and now-self.since >= .5 and now-self.last_time < .7


async def main(move_seconds=20, near_cm=80, far_cm=160, threshold_px=120):
    folder = ROOT/'logs'/'arrival_reference'
    folder.mkdir(parents=True, exist_ok=True)
    path = folder/(datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.jsonl')
    latest, seen, rows, errors = {}, {}, [], []
    phase = 'preflight'
    abort = asyncio.Event()
    reference = Reference(threshold_px)
    speaker = Speaker()
    with path.open('x', encoding='utf-8') as log:
        def record(value):
            log.write(json.dumps(value, ensure_ascii=False)+'\n')
            log.flush()
        print('LOG:', path, flush=True)
        record(dict(kind='conditions', actuation=False, threshold_px=threshold_px,
                    surface_distances_cm=[far_cm, near_cm, far_cm], board_mission_executed=False,
                    scope='Visual calibration only. Physical distances operator measured; no contact, yaw or height validation.'))
        async with websockets.connect('ws://127.0.0.1:8765', open_timeout=4, close_timeout=1) as ws:
            # This is the only outgoing message: no control, mode or reset.
            await ws.send(json.dumps(dict(auth=dict(role='viewer'))))

            def fresh(flag, age=1):
                return time.monotonic()-seen.get(flag, -math.inf) < age

            def observed():
                f = latest.get(7, {})
                return (fresh(2) and fresh(7) and f.get('nicla_fresh') == 1 and
                        f.get('nicla_reset_pending') == 0 and f.get('nicla_protocol') == 0x4e03)

            async def read():
                try:
                    while True:
                        try:
                            value = json.loads(await asyncio.wait_for(ws.recv(), .5))
                        except asyncio.TimeoutError:
                            if phase != 'preflight' and not all(fresh(k, 1.5) for k in (2, 3, 7)):
                                raise RuntimeError('Telemetria interrumpida')
                            continue
                        now = time.monotonic()
                        record(dict(kind='rx', monotonic=now, phase=phase, value=value))
                        if value.get('auth_ok') is False:
                            raise RuntimeError('Ground rechazo el acceso de lectura')
                        flag, fields = value.get('flag'), value.get('fields', {})
                        if not fields:
                            continue
                        latest[flag], seen[flag] = fields, now
                        if flag == 3 and (fields.get('mode') != 0 or fields.get('armed') != 0):
                            raise RuntimeError('Requiere modo 0 y desarmado; no se toma control')
                        if phase != 'preflight' and not all(fresh(k, 1.5) for k in (2, 3, 7)):
                            raise RuntimeError('Telemetria atrasada')
                        if flag == 2:
                            qualified = reference.update(now, fields,
                                int(latest.get(7, {}).get('nicla_sample', 0)), observed())
                            row = dict(kind='visual_sample', monotonic=now, phase=phase,
                                geometry=fields, observed=observed(), epoch=reference.epoch,
                                historical_full=reference.had_full, width_reference_met=qualified)
                            rows.append(row)
                            record(row)
                except Exception as error:
                    errors.append(str(error) or type(error).__name__)
                    abort.set()

            async def wait(predicate, seconds):
                deadline = time.monotonic()+seconds
                while time.monotonic() < deadline:
                    if abort.is_set():
                        raise RuntimeError(errors)
                    if predicate():
                        return
                    await asyncio.sleep(.05)
                raise RuntimeError('Condicion pendiente: '+phase)

            async def cue(name, message, seconds=0):
                nonlocal phase
                phase = name+'_instruction'
                await speaker.say(message, abort)
                phase = name
                record(dict(kind='phase', name=phase, monotonic=time.monotonic(), seconds=seconds))
                if seconds:
                    try:
                        await asyncio.wait_for(abort.wait(), seconds)
                    except asyncio.TimeoutError:
                        pass
                    if abort.is_set():
                        raise RuntimeError(errors)

            task = asyncio.create_task(read())
            try:
                await wait(lambda: all(fresh(k) for k in (2, 3, 7)) and observed(), 8)
                await cue('position_far', f'Prueba sin motores. Mide {far_cm} centímetros desde la cámara a la superficie del globo. Tienes veinte segundos.', 20)
                await cue('full', 'Muestra el globo completo. Espera la confirmación.')
                await wait(lambda: reference.had_full and observed() and latest[2].get('nicla_flag') in (65, 66), 35)
                await cue('far', f'Objetivo fijado. Mantén los {far_cm} centímetros. Ocho segundos.', 8)
                await cue('move_near', f'Acércate a {near_cm} centímetros de la superficie. Mantén el mismo globo delante. Tienes {move_seconds} segundos.', move_seconds)
                await cue('near', f'Mantén los {near_cm} centímetros. Ocho segundos.', 8)
                await cue('move_back', f'Vuelve a {far_cm} centímetros. Tienes {move_seconds} segundos.', move_seconds)
                await cue('return', f'Mantén los {far_cm} centímetros. Ocho segundos.', 8)

                def plateau(name):
                    samples = [r for r in rows if r['phase'] == name]
                    visible = [r for r in samples if r['observed'] and r['geometry'].get('nicla_flag') in (65, 66)]
                    widths = [r['geometry']['nicla_w'] for r in visible]
                    return dict(samples=len(samples), confirmed_samples=len(visible),
                        width_min=min(widths) if widths else None,
                        width_median=statistics.median(widths) if widths else None,
                        width_max=max(widths) if widths else None,
                        reference_met=any(r['width_reference_met'] for r in samples),
                        epochs=sorted(set(r['epoch'] for r in samples)))

                levels = {name: plateau(name) for name in ('far', 'near', 'return')}
                epochs = set(e for p in levels.values() for e in p['epochs'])
                enough = all(p['confirmed_samples'] >= 10 for p in levels.values())
                separates = (enough and len(epochs) == 1 and levels['near']['reference_met'] and
                    not levels['far']['reference_met'] and not levels['return']['reference_met'])
                result = dict(log=str(path), actuation=False, plateaus=levels,
                    enough_observations=enough, same_association=len(epochs) == 1,
                    visual_reference_separated=separates, esp_waypoint_cleared_validated=False,
                    note='Only sampled visual threshold calibration; does not execute ESP height/yaw/ROI arrival gates. Distances operator measured.')
                record(dict(kind='summary', value=result))
                print(json.dumps(result, indent=2, ensure_ascii=False), flush=True)
                await cue('done', 'Prueba terminada. La referencia visual distingue las dos distancias.' if separates else
                          'Prueba terminada. La referencia de llegada queda pendiente. Revisaremos el registro.')
            except Exception as error:
                record(dict(kind='failure', phase=phase, error=str(error)))
                raise
            finally:
                task.cancel()
                await asyncio.gather(task, return_exceptions=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--move-seconds', type=int, default=20)
    parser.add_argument('--near-cm', type=int, default=80)
    parser.add_argument('--far-cm', type=int, default=160)
    parser.add_argument('--threshold-px', type=int, default=120)
    args = parser.parse_args()
    if not 10 <= args.move_seconds <= 60:
        parser.error('--move-seconds: 10..60')
    if not 1 <= args.near_cm < args.far_cm <= 500 or not 1 <= args.threshold_px <= 240:
        parser.error('Requiere 1 <= near-cm < far-cm <= 500 y threshold-px 1..240')
    try:
        asyncio.run(main(args.move_seconds, args.near_cm, args.far_cm, args.threshold_px))
    except Exception as error:
        raise SystemExit('PRUEBA NO VALIDADA: '+str(error))
