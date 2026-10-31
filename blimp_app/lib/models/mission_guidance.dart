/// Presentation only: never modifies ESP commands or grants motion permission.
class MissionGuidance {
  int? current, candidate, waypoint;
  int since = 0;
  void reset() { current = candidate = waypoint = null; }
  int update(int raw, int nextWaypoint, int now) {
    final code = raw == 5 || raw == 6 ? 6 : raw;
    if (waypoint != nextWaypoint) {
      reset();
      waypoint = nextWaypoint;
    }
    // Never retain an advance instruction when the ESP withdraws permission.
    if (current == 7 && code != 7) current = 6;
    if (candidate != code) { candidate = code; since = now; }
    final urgent = code == 1 || code == 2 || code == 9 || code == 10 || code == 15;
    if (urgent || (current == null && code != 7) || now - since >= 600) current = code;
    return current ?? 6;
  }
}
