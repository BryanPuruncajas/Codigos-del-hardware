# Vertical estimate: bench evidence and experimental filter

Work is in Competencia, not BlimpV4. No pins, actuator geometry, ESC limits,
PID gains or Nicla software were changed. No hardware deployment/arming yet.

## Stationary measurements before the change

Two 30-second Viewer-only runs through Ground, 288 F1 samples each. F3 stayed
mode=0/armed=0, motor commands zero; max F1 gap ~125 ms. User confirmed frame
fully blocked, no translation or rocking on the second run.

- Run 1: height peak-to-peak 0.1184 m, standard deviation 0.0218 m,
  first/last five-second median shift -0.0168 m; apparent vz -0.163..+0.188 m/s.
- Blocked run: height peak-to-peak 0.1036 m, standard deviation 0.0198 m,
  first/last five-second median shift -0.0229 m; apparent vz -0.126..+0.132 m/s.
- These are stationary baseline observations, not raw BMP pressure captures.
  They demonstrate estimate fluctuations; they do not identify pressure,
  temperature, electrical or library failure as the sole physical cause.

## Code findings

Active driver is Barometer390.cpp (not legacy Barometer.cpp). Existing update
period is 20,000 us = up to 50 Hz, despite its incorrect old comment. Velocity
was the one-sample altitude difference / dt, then SensorSuite applied a .9/.1
sample-count-based low-pass; the control layer also filters velocity.
Calibration called performReading and then readAltitude, whose installed
Adafruit implementation performs another pressure read. Update also lacked an
explicit successful-read check.

## Experimental change

Use one checked performReading and convert that pressure with the same altitude
equation as the locally installed Adafruit library. Keep the startup relative
ground reference (no in-flight auto-zero and no claim of distance to ground).
Use median-of-three, a time-based 0.20 s altitude low-pass, a 25-sample linear
regression (~0.5 s at 50 Hz) for slope, then 0.15 s velocity low-pass. Remove the
extra velocity low-pass in SensorSuite; controller filters/gains remain unchanged.
Fixed storage only, no dynamic allocation. Invalid readings are skipped; sample
age >=300 ms or initialization failure makes height/vz unavailable. A gap >0.5 s
primes the filter without a derivative spike. Mission with invalid height cannot
request forward thrust; F8 reason15 reports invalid vertical data (except P12,
which remains explicitly non-actuated and bypasses altitude supervision).

F1-F8 packet layouts unchanged; F1 height/vz now contain the filtered estimate.
New F9 at 5 Hz: raw relative height, filtered height, raw finite-difference vz,
filtered vz, sample age ms, healthy flag. Bridge LABELS and app diagnostic group
are updated. Raw means no NEW software filter, not bypass of BMP hardware IIR.

## Software checks and limitations

groundstation/test_vertical_estimate.cpp exercises the actual portable C++
estimator: stationary synthetic noise, ramps, step, single-sample spike, invalid
input, duplicate timestamp, long gap, micros rollover, irregular sampling and
slow drift. For the particular synthetic 2+7 Hz noise signal, RMS ratios were
0.337 height/raw and 0.122 velocity/old filtered derivative. These are NOT measured
hardware improvements. A synthetic 50 cm step took 500 ms to reach 90% height.
Velocity window and remaining controller filters add latency too.

No drift compensation: a true slow movement and pressure drift are indistinguishable
with this sensor alone. No gains or supervisor thresholds were widened to hide
noise. Flight tuning, fall detection delay and motor-wash effects remain unvalidated.

## Next test (no balloon required)

1. Program ESP outside PCB, USB-only, battery disconnected. Never backfeed powered
   PCB through board USB. Reinstall with power off; keep motors disconnected.
2. Restart Ground for F9 labels. Stationary blocked run >=30 seconds: capture
   raw and filtered values simultaneously, healthy=1, age<300 ms and armed=0.
3. Move vertically a measured ~30-50 cm, stop, lower again; compare sign, settled
   height change, velocity return to zero and timing. Do not confuse a changed
   startup reference after power cycling with reduced drift.
4. Only after stationary AND moving validation consider supervised flotation
   altitude-control tests. Filter smoothness alone is not flight readiness.

## Subsequent actuated bench evidence (2026-10-24)

All runs ended with mode0/armed0 confirmed. Software observers are temporary
WebSocket clients, not a replacement for on-board failsafe or manual STOP.
Early attempts were inconclusive: one observer incorrectly treated expected
3.7s firmware arming silence as telemetry loss; another saw cached motor power
on the first F3 after mode transition (F3 is sent before runner updates caches).
Corrected protocol first selects P00M zero DISARMED, waits multiple zero frames,
arms at zero while operator supports frame, waits for current sensor data, then
announces release and transitions to the test. No change to arming firmware.

- Clean P04 zero-integral start: ref1.030523, first~3s almost zero power,
  ~3.8% at4.25s, ~5.1% at5.33s,10% at6.39s. Estimated loss>30cm caused STOP.
- Fixed10% vertical test (no PID/yaw),6s: initial estimate1.616317,
  minimum around1.446, final~1.500; final vz~+0.032m/s. Operator confirmed it
  remained suspended without ground contact. This is a provisional preload
  candidate for CURRENT ballast/envelope, NOT a settled hover calibration.

### Shared startup-integral preload, awaiting deployment/physical validation

ControlCommon DEF_ALT_START_INTEGRAL=0.10 initializes the EXISTING altitude I
state at reset in P04/P05/BalloonMission, clamped to each mode's configured
maxPower. lastPower still starts at zero; existing slew remains. The I term may
decrease or grow through the original control law; this is not a10% power floor.
No max-power increase, gain retuning or automatic calibration. P12 remains
non-actuated. Future weight changes require revalidation; configurable automatic
calibration is explicitly deferred by user.

The native test test_altitude_startup.cpp exercises the actual ControlCommon
implementation:10% demand at zero error, existing slew reaches10% in556ms,
lower maximum clamps preload, above-target error reduces output/I, invalid
inputs yield zero and saturation remains bounded. Full firmware compiles.
Mission also now honors any finite explicit barometric height (including
zero/negative), instead of silently replacing targets<=0.05 with current height.

Next: upload ESP only with battery off/outside PCB, then repeat P04 with the
same ballast and10% ceiling. No new app/Nicla upload for this preload change.
P05/course flight remains pending; yaw tilt reduces vertical authority and no
extra reserve has been authorized. Do not infer full-course readiness from the
six-second fixed-power run. Drift is still unresolved.
