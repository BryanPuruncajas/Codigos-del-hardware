import 'package:flutter_test/flutter_test.dart';
import 'package:blimp_app/models/mission_guidance.dart';

void main() {
  test('Short chatter never grants advance; withdrawal is immediate', () {
    final g = MissionGuidance();
    expect(g.update(5, 1, 0), 6);
    expect(g.update(7, 1, 100), 6);
    expect(g.update(6, 1, 300), 6);
    expect(g.update(7, 1, 500), 6);
    expect(g.update(7, 1, 1100), 7);
    expect(g.update(5, 1, 1101), 6);
    expect(g.update(1, 1, 1102), 1);
    expect(g.update(7, 3, 1200), 6); // new waypoint does not inherit advance
    expect(g.update(7, 3, 1800), 7);
    expect(g.update(10, 3, 1801), 10);
    g.reset();
    expect(g.update(7, 3, 2000), 6);
  });
  test('Acquisition details require persistence', () {
    final g = MissionGuidance();
    expect(g.update(4, 3, 0), 4);
    expect(g.update(12, 3, 100), 4);
    expect(g.update(14, 3, 200), 4);
    expect(g.update(13, 3, 300), 4);
    expect(g.update(13, 3, 900), 13);
  });
}
