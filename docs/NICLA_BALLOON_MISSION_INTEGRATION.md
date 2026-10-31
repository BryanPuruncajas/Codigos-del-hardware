# Nicla N03 -> Balloon Mission (2026-10-16)

## Source and build

The integrated firmware remains the full firmware with P00-P11. Active desktop
project: `C:/Users/stefa/Desktop/BLIMP/Competencia/Blimp/Blimp/Codigos-del-hardware`.
The changed firmware files and the Nicla sources are also present in this worktree.
No board was flashed during this integration.

Build the desktop project's firmware with:

```powershell
platformio run -d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\firmware" -e seeed_xiao_esp32s3
```

## Connections and Nicla program

Use `vision/n03_panel/nicla_n03_uart.py` together with `n02_core.py` on the Nicla.
The wrapper loads the core; either provide that file on the device or concatenate
both in RAM as the host test does. `nicla_n02_tracking.py` is USB-only diagnostics.

UART is 115200, 8N1. ESP uses explicit D7/RX and D6/TX pins. Connect Nicla TX
to ESP D7, ESP D6 to Nicla RX, and GND to GND. On the PCB select TX/RX paths.
For the existing isolated USB protoboard test, do not interconnect VCC/VBAT.
The return wire is required for reset acknowledgement before another acquisition.

## Protocol

First ten iBUS channels retain the legacy layout; F2 telemetry and P06/P07 mappings
are unchanged. Geometry is in the 240x160 image. X/Y VALUE are centroid coordinates,
W/H VALUE are bounding-box dimensions. Distance 9999 means unavailable, not metres.

| Channel | Meaning |
| --- | --- |
| 0 | 0=no observation, 65=FULL, 66=PARTIAL |
| 1-4 | bounding box x,y,w,h |
| 5-8 | centroid cx,cy and box w,h |
| 9 | 9999, no distance measurement |
| 10 | sample sequence, wraps at 255 |
| 11 | SEARCH=0, CONFIRM=1, FULL=2, PARTIAL=3, MISSING=4, AMBIG=5, LOST=6 |
| 12 | N03 protocol marker 0x4E03 |
| 13 | tracker reset acknowledgement, wraps at 65535 |

ESP command byte `R` resets the tracker; the Nicla increments acknowledgement.
ESP waits for that acknowledgement before using further observations.
USB prints and image annotations are off in the UART wrapper by default.

## Mission behaviour

- Acquisition accepts FULL or temporally confirmed PARTIAL. Initial PARTIAL
  requires a compact central candidate (or a large box spanning the image center),
  fill >=0.35, roundness >=0.45, aspect 0.4..2.5 and box area <90% of the frame.
  Two eligible initial candidates remain ambiguous. Nearness alone is not contact.
- Missing/ambiguous observations stop planar approach and keep altitude control.
- LOST requests a tracker reset and returns to SEARCH, awaiting acknowledgement.
- In ACQUIRE_NEXT, a new fresh LOST sample also requests reset; route yaw and
  next-target geometry filters remain active. No additional reset is sent while
  resetPending waits for acknowledgement. Bench verification of this fix remains
  pending after the interrupted five-contact trial (one visit completed).
- Missing packets or a stalled N03 sample counter invalidate feedback after 500 ms.
  Planar mission motion pauses; existing altitude control continues.
- Visit confirmation and target-loss counts advance on new camera samples, not
  repeated 250 Hz control iterations. Yaw reference updates also require new samples.
- P00-P07 and their motor/servo calibration and controllers are retained. Transport
  timeout applies to all vision consumers. Legacy senders remain supported.

## Arrival and limitations

**Current source setting, 2026-10-28:** the user selected approximately **80 cm
camera-to-balloon-surface, 120 px**, replacing the provisional 44 cm / 220 px
reference below. `N03_VISIT_WIDTH_PX` now defaults to **120**. Historical FULL,
association continuity, alignment, >=3 new observations and >=500 ms confirmation
remain required; near crops retain the height-anchor/yaw/ROI coverage gates.
This is estimated proximity only, not contact. The latest manual capture
`logs/arrival_reference/20261028_204443_034426.jsonl` had 121..134 px at the first
requested 80 cm plateau and 93..101 px on return: 120 px does not provide exact
or repeatable metric ranging. This source change requires ESP upload; it does
not require changing the Nicla, pinout or app.

### Historical 44 cm reference (superseded)

The historical score threshold 38 is NOT applied to the N03 bounding-box width.
`N03_VISIT_WIDTH_PX` previously defaulted to 220 after the provisional 2026-10-22 bench
calibration. At user-reported 80 cm, observed width was 151..215 px (median 196),
all PARTIAL. In a separate continuous approach, width rose from ~155 to 229..240
px at the user-confirmed final distance of 44 cm. A repeat ending at confirmed
44 cm reached about 220..222 px without confirming arrival at the original
225 px threshold. At the user's request, the provisional threshold is now 220.
These are different trials,
not a calibrated ranging model; frame clipping saturates the width at 240.
Require fresh centered observations above threshold, >=3 new samples, and
>=500 ms in VISIT_CONFIRM. A failed condition returns to APPROACH or holds
on missing feedback; no stale geometry can confirm arrival.
The 5 px separation from the observed 80 cm maximum is small: false arrival
and repeatability must be checked in a disarmed near/far trial before flight.
Define N03_VISIT_WIDTH_PX=0 to disable arrival. Box size or PARTIAL alone does
not prove physical balloon contact. This setting means estimated arrival only.

The threshold is compiled locally but must be uploaded and physically validated
before treating visits as reliable. No unattended flight or contact validation
has been established by this calibration.
Updated 2026-10-21: ESCAPE and reverse mission movement have been removed.
P11 (`p11`, mode 20) selects Track 1: 1-2-3-4-1.
P11 Track 2 (`p11t2`, mode 21) selects 1-3-4-2-1. A closed loop is five
confirmed contacts including initial 1 and return to 1. P08/P09 stop after one
contact; P10 exercises the first two contacts of Track 1 without reversing.

At mission RESET align the blimp parallel to the arena direction 1 -> 2.
Its measured yaw is latched as the course reference; this is required calibration,
not absolute localization. Relative leg headings for Track 1 are 0/90/180/-90 deg;
Track 2 uses 26.565/180/-26.565/180 deg. Camera-visible target selection alone
cannot establish physical waypoint identity.

After a visit, turn using the existing yaw cascade while maintaining height.
Require yaw error <=5 deg and absolute yaw rate <=0.10 rad/s for 300 ms.
Only then reset the Nicla tracker and await acknowledgement and a fresh FULL
candidate along the intended heading. The next bounding-box area must be less
than half of the last contact-sized area; this conservative heuristic rejects
nearby old targets but is not proof of identity. Occlusion can prevent acquisition;
the vehicle waits holding the planned heading, rather than advancing blindly.
If yaw does not settle within 20 s of active mission time, state 6 holds altitude
without further planar turn commands. There is no automatic retry/escape.

F5 numeric states: 0 SEARCH, 1 APPROACH, 2 VISIT_CONFIRM, 3 TURN_TO_NEXT,
4 ACQUIRE_NEXT, 5 DONE, 6 TURN_FAILED. Dashboard labels/selector and Python
command packing were updated. Existing F1-F6 packet formats are retained.
Rebuild/restart the dashboard and bridge when selecting Track 2. No pin changes.
Updated 2026-10-22: F7 adds nicla_fresh, nicla_state, nicla_sample,
nicla_protocol, nicla_reset_pending and nicla_reset_ack at <=5 Hz.
F1-F6 and UART layout are unchanged. Restart the ground Python bridge to load
the new F7 labels; an existing app may ignore the additional packet.
Heading tracking, contact calibration, target rejection and actual motor response
still require bench and supervised physical validation before an autonomous loop.

## Verification

### Manual vision bench, 2026-10-23

Mode 22 (P12_VISION_BENCH) exercises all five Track 2 contacts (1-3-4-2-1) with
altitude demand and barometric supervisor disabled. It explicitly disarms
on entry and update, and rejects ARM commands in processCommands. This is
not a flight mode. Modes 17..21 retain their altitude supervisor.
Use the existing check_mission_disarmed.py with --test p11t2 --vision-bench.
The script continues to send arm=-1 and returns to SAFE_STOP in finally.
Voice announcements remain enabled. ESP firmware upload is required; no Nicla
or PCB pin changes are needed. Physical bench validation remains pending.

- Full ESP firmware compilation, including all P modes.
- Host tests check actual Nicla transmitter packing, checksum, FULL/PARTIAL flags,
  missing-data zeros, sequence rollover and reset acknowledgement fields.
- Host tests do not establish in-flight behaviour, physical UART reliability,
  sensor health, or correct motor response.

Next bench test: both boards on the current protoboard, motors disabled; verify
protocol=19971, increasing frames/sample, reset_pending returning to zero,
matching centroid/size and flag=65/66. Stop Nicla transmission and verify fresh=0
and zero geometry after 500 ms. Then validate mission commands with the actuators
disarmed before an explicitly supervised motor test.

## 2026-10-23: forward permission and dashboard diagnostic (not yet deployed)

- APPROACH now requires a centered, usable detection, finite yaw rate <=0.10 rad/s,
  at least five new samples and 300 ms of stability before requesting forward thrust.
  A sample gap >200 ms, off-center target, excess rotation, loss or altitude
  supervisor interruption resets this permission. These timings are provisional.
- VISIT_CONFIRM retains vertical altitude demand instead of setting all motors to
  zero. P12 remains forcibly disarmed and has zero altitude demand.
- No active braking, reverse/escape, horizontal position hold, servo range changes,
  pin changes or Nicla changes. Removing forward thrust does not remove inertia.
- F1-F7 remain unchanged. New F8 (5 Hz during mission): motion reason,
  requested forward thrust (before allocation/saturation, not measured motion),
  wrapped yaw reference error in degrees, actual course waypoint, stable ms, bench.
  Reasons: 0 none, 1 vision wait, 2 altitude priority, 3 turn, 4 acquisition/search,
  5 visual centering, 6 stability wait, 7 forward request, 8 arrival confirmation,
  9 done, 10 turn failed.
- Dashboard shows the reason, waypoint, yaw error and disarmed simulation label.
  Missing/old (>1.5 s) F8 is displayed as unavailable on telemetry/UI refresh.
- Rebuild/upload ESP and update app plus host bridge to deploy. No board was flashed
  by this change. Do NOT connect board USB while mounted on powered PCB.
- Bench: run existing p11t2 --vision-bench, hold target centered, rotate faster
  than the allowed rate, stop, remove target and restore it. Verify F8 reason 6
  before 7, immediate withdrawal on excess rotation/loss, and F3 armed=0 throughout.
- Host regression: groundstation/test_advance_gate.cpp exercises the actual C++ gate
  (frame count, dwell, sample gap, revocation, reset and millis rollover).
  Flight/floatability, actual thrust symmetry and altitude tuning remain unvalidated.

### P12 from the dashboard

In Competencia, the mission panel now has dedicated buttons **Iniciar P12
desarmado** and **Detener P12 / STOP**, separate from actuated mission selection.
The start button requires a connected Dev session and armed=0; demo is excluded.
Bridge command `vision_bench_start` requires F3 disarmed received within 2 seconds,
uses existing p11t2 parameter packing but mode=22 and arm=-1 only, and resets once.
The existing ESP mode22 independently enforces disarming. This is a manual
vision/route test without altitude actuation. Align the initial heading 1->2 before
pressing start. The bridge sends STOP after 60 seconds, on session disconnection,
or before replacing that session's P12 with another command. STOP transmission
is not proof of reception: verify mode=0 and armed=0 on F3.

Deployment: restart the Python bridge and reinstall/reload the updated Flutter
app. No additional ESP or Nicla upload is needed for this button.

### Follow-up: calmer guidance and acquisition diagnostics

Supersedes the 60-second P12 duration above: the bridge now limits P12 to
240 seconds. Existing STOP/disconnect handling is unchanged. Restart Ground
and reload the app to apply this duration.

The dashboard groups transient centering/stability messages into one instruction.
Non-urgent guidance must persist for 600 ms; forward guidance also requires
600 ms of continuous ESP permission. Withdrawal of forward permission, lost
vision, altitude priority and failure are not delayed. This is display-only;
the ESP control gate, gains, servo limits and pin mapping are unchanged.
Turn instructions use CCW-positive yaw (left positive), approximate remaining
angle rounded to 5 degrees, and ask the operator to stop within 5 degrees.
This does not prove that physical rotation has settled; the ESP retains its
existing rate/dwell checks. Data expires after 1.5 seconds, including when no
new packets arrive (UI timer).

F8 reason codes extended without altering packet layout: 11 route yaw not aligned,
12 next target is PARTIAL (FULL required for initial acquisition), 13 area rejected
by the existing smaller-next-target rule, 14 pending reset or missing fresh camera
reply. Area rejections are latched for 1.5 s for visibility across camera resets.
No filter was relaxed: a failure to acquire waypoint3 must be diagnosed rather
than bypassed. These extra reasons require a new ESP upload; no Nicla change.

Software verification includes message-chatter/permission-revocation unit tests,
widget tests for left/right/stop and partial/size blockers, and firmware compile.
Actual acquisition, direction sign, display timing and four-minute run remain
pending physical bench validation after deployment, with motors disconnected.
