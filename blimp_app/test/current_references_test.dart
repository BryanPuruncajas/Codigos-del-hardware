import 'package:flutter_test/flutter_test.dart';
import 'package:blimp_app/services/telemetry_service.dart';

void main() {
  test('Copy references requires fresh disarmed Dev telemetry', () {
    final service = TelemetryService();
    service.connected = true;
    service.role = 'dev';
    final now = DateTime.now().millisecondsSinceEpoch.toDouble();
    service.latest.addAll({'height': 2.8, 'yaw': 1.57079632679,
      'armed': 0, 'received_f1_ms': now, 'received_f3_ms': now});
    expect(service.currentReferences!['height'], 2.8);
    expect(service.currentReferences!['yaw_deg'], closeTo(90, .001));
    service.latest['armed'] = 1;
    expect(service.currentReferences, isNull);
    service.latest['armed'] = 0;
    service.latest['received_f1_ms'] = now - 2000;
    expect(service.currentReferences, isNull);
    service.latest['received_f1_ms'] = now;
    service.role = 'viewer';
    expect(service.currentReferences, isNull);
    service.dispose();
  });
}
