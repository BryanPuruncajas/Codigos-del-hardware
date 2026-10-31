import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:blimp_app/widgets/telemetry_widgets.dart';

void main() {
  testWidgets('Advance diagnostic distinguishes requested motion and stale data', (tester) async {
    Future<void> showDecision(int reason, {bool stale = false}) => tester.pumpWidget(MaterialApp(
      home: Scaffold(body: MissionOverviewCard(latest: {
        'mission_state': 1, 'visited': 1, 'target_count': 5, 'mode': 22,
        'mission_motion_reason': reason.toDouble(), 'mission_waypoint': 3,
        'mission_yaw_error_deg': 2.5, 'mission_bench': 1, 'armed': 0,
        'mission_decision_received_ms':
            (DateTime.now().millisecondsSinceEpoch - (stale ? 2000 : 0)).toDouble(),
      })),
    ));
    await showDecision(6);
    expect(find.text('Centre el globo y mantenga el giro quieto'), findsOneWidget);
    expect(find.textContaining('Waypoint 3'), findsOneWidget);
    expect(find.textContaining('DESARMADO'), findsOneWidget);
    await showDecision(7);
    expect(find.text('Puede acercarse lentamente'), findsNothing);
    await showDecision(7, stale: true);
    expect(find.text('Puede acercarse lentamente'), findsNothing);
    expect(find.text('Sin diagnóstico reciente de avance (F8)'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets('Mission shows directed turn and timeout without escape', (tester) async {
    Future<void> showState(int state) => tester.pumpWidget(MaterialApp(
      home: Scaffold(body: MissionOverviewCard(latest: {
        'mission_state': state.toDouble(), 'visited': 1, 'target_count': 5,
      })),
    ));
    await showState(3);
    expect(find.text('GIRANDO'), findsOneWidget);
    expect(find.text('ESCAPANDO'), findsNothing);
    expect(find.text('Contactos confirmados: 1 / 5'), findsOneWidget);
    await showState(4);
    expect(find.text('ADQUIRIENDO'), findsOneWidget);
    await showState(6);
    expect(find.text('GIRO FALLIDO'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets('Turn guidance shows direction and blocker explicitly', (tester) async {
    Future<void> showReason(int reason, double error) => tester.pumpWidget(MaterialApp(
      home: Scaffold(body: MissionGuidanceView(key: ValueKey(reason), latest: {
        'mode': 22, 'mission_motion_reason': reason.toDouble(),
        'mission_waypoint': 3, 'mission_yaw_error_deg': error,
        'mission_decision_received_ms': DateTime.now().millisecondsSinceEpoch.toDouble(),
      })),
    ));
    await showReason(3, 26.5);
    expect(find.textContaining('izquierda'), findsOneWidget);
    await showReason(11, -12);
    expect(find.textContaining('derecha'), findsOneWidget);
    await showReason(3, 4);
    expect(find.text('Detenga el giro y mantenga quieto'), findsOneWidget);
    await showReason(12, 0);
    expect(find.textContaining('globo completo'), findsOneWidget);
    await showReason(13, 0);
    expect(find.textContaining('rechazado por tamaño'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
}
