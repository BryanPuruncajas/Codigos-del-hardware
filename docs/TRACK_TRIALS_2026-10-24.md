# Short actuated trials before course runs

Changes only in Competencia app and Ground. No new ESP/Nicla upload, pin edits,
servo range expansion, active reverse/braking, flow integration or relative-zero
feature. Barometer drift remains unresolved. Course completion/contact is still
an estimate from vision width, not a contact sensor.

## Evidence before this change

Two operator-observed P04 trials maintained approximately physical height. This
is not proof of sustained altitude hold. First monitored trial: ref2.8 m,
observed estimate ~2.75..2.97 m and requested motor0..5.4%; STOP/disarm confirmed.
Second trial had already run ~30s when monitoring started; STOP was sent immediately,
ref4.1, final height4.05. No complete second-run trace was collected by the observer.
User reported yaw rotation and horizontal motion during P04: yaw and position
are not regulated by that mode.

## Improvements

- App buttons copy current height and yaw (for P04/P05) or height (mission) only
  with Dev connection, disarmed F3 and F1/F3 received within1.5s. This is a snapshot,
  not continuous reference tracking. Capture at intended physical height. Align
  course start along1->2 before mission reset; yaw route reference remains in ESP.
- P04/P05 default duration20s; mission panel default240s. Ground validates5..240s
  and creates an automatic STOP deadline beginning before the ARM request; arming
  time is INCLUDED. Updates do not extend that deadline. A new command cancels the
  owning session's old deadline and stops it before replacement. Disconnect sends
  STOP; STOP acknowledgement still requires mode0/armed0 telemetry.
- Timed starts require F3 disarmed within2s. Gain updates require an active timed
  trial in that session. Generic ARM is refused in actuated test/mission modes;
  use Iniciar. Low-level raw-control APIs remain available and are not a timed
  flight supervisor. Use a single Dev control session.
- Height max default changed15%->10% in mission and P04/P05 app panels. Existing
  parameter fields remain editable; inspect before starting. P05 sends existing
  mission yaw Kp0.4(default), Ki1(default), Kd9, min6%, max10%, authority75%(default),
  yaw deadband5deg matching route-hold tolerance. This is parameter alignment, not
  a newly validated flight tuning. Check P05 physically before a course run.
- Status says request sent rather than falsely claiming running after4.3s.

## Important limits

STOP timeout is HOST-SIDE ONLY. Ground termination, lost radio or serial failure
can prevent STOP reception. No new on-board watchdog is claimed. Remain in range,
keep manual STOP accessible and do not leave the blimp unattended. Never connect
ESP/Nicla USB while mounted on powered PCB. This change does not relax acquisition,
arrival or altitude-supervisor thresholds. No horizontal position hold.

## Operator order

1. Reload app and restart Ground. Do not flash ESP again for this change.
2. P05: set physical height with clear margins, release hands, copy references,
   check20s/maxheight10%; start deliberately. Observe physical yaw and height,
   not only the barometer. Timer includes arming. Confirm STOP and disarmed.
3. If yaw hold is acceptable, P08 single target; only then select P11 Track1
   rectangle or P11t2 infinity, initial direction1->2. Capture current height,
   inspect duration and power. Monitor physical attitude/height and stop early on
   sustained drift or unintended motion. Full course not yet flight validated.

Software tests cover timer STOP, cancellation without delayed STOP, duration
validation, start/update/stop packing, no rearming on updates, P12 disarmed path,
Viewer restrictions, and fresh-reference capture. Physical trials remain pending.
