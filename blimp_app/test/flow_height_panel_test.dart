import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:blimp_app/widgets/telemetry_widgets.dart';

Widget panel(Map<String, double> values) => MaterialApp(
      home: Scaffold(body: SingleChildScrollView(child: FlowTelemetryPanel(latest: values, history: const []))),
    );

void main() {
  testWidgets('shows Flowdeck height, vertical speed and counts/s separately', (tester) async {
    await tester.pumpWidget(panel({
      'flow_age_ms': 20, 'flow_flags': 3,
      'flow_received_at_ms': DateTime.now().millisecondsSinceEpoch.toDouble(),
      'flow_height_source': 2, 'flow_height_flags': 2 + 8 + 128,
      'flow_height_filtered': .8, 'flow_height_vz': -.03,
      'flow_fused_height': .2, 'flow_fused_vz': -.03,
      'flow_rate_x': 12, 'flow_rate_y': -9, 'flow_quality': 80,
    }));
    expect(find.text('Altura: Flowdeck'), findsOneWidget);
    expect(find.text('Altura al suelo'), findsOneWidget);
    expect(find.text('Vz Flowdeck'), findsOneWidget);
    expect(find.text('cuentas/s'), findsNWidgets(2));
    expect(find.text('FRESCO'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

  testWidgets('shows fallback and does not display a false fresh radio link', (tester) async {
    await tester.pumpWidget(panel({
      'flow_age_ms': 20, 'flow_flags': 3,
      'flow_received_at_ms': DateTime.now().millisecondsSinceEpoch.toDouble() - 2000,
      'flow_height_source': 3, 'flow_height_flags': 8 + 16,
      'flow_quality': 0,
    }));
    expect(find.text('Altura: Perfil anterior: respaldo'), findsOneWidget);
    expect(find.text('VENCIDO'), findsOneWidget);
    expect(find.text('FRESCO'), findsNothing);
    expect(tester.takeException(), isNull);
  });
  testWidgets('flow-only overview hides invalid height instead of showing a stale zero or pressure value', (tester) async {
    await tester.pumpWidget(MaterialApp(home: Scaffold(body: OverviewRow(latest: {
      'vertical_profile_version': 39, 'height': 9.99, 'vertical_velocity': .77,
      'flow_height_source': 0, 'flow_height_flags': 4096,
    }))));
    expect(find.text('Altura Flowdeck'), findsOneWidget);
    expect(find.textContaining('9.99'), findsNothing);
    expect(find.textContaining('0.77'), findsNothing);
    expect(tester.takeException(), isNull);
  });

  testWidgets('flow-only panel hides expired control samples', (tester) async {
    await tester.pumpWidget(panel({
      'vertical_profile_version': 39,
      'flow_height_source': 2, 'flow_height_flags': 4096 + 2 + 8,
      'flow_control_received_at_ms': DateTime.now().millisecondsSinceEpoch.toDouble() - 2000,
      'flow_height_filtered': 9.99, 'flow_fused_height': 9.99,
      'flow_height_vz': .77, 'flow_fused_vz': .77,
    }));
    expect(find.text('Altura: Sin altura Flowdeck'), findsOneWidget);
    expect(find.textContaining('9.99'), findsNothing);
    expect(find.textContaining('0.770'), findsNothing);
    expect(tester.takeException(), isNull);
  });

  testWidgets('flow-only panel displays a current qualified range', (tester) async {
    await tester.pumpWidget(panel({
      'vertical_profile_version': 39,
      'flow_height_source': 2, 'flow_height_flags': 4096 + 2 + 8,
      'flow_control_received_at_ms': DateTime.now().millisecondsSinceEpoch.toDouble(),
      'flow_height_filtered': .8, 'flow_fused_height': .8,
      'flow_height_vz': -.03, 'flow_fused_vz': -.03,
    }));
    expect(find.text('Altura: Flowdeck'), findsOneWidget);
    expect(find.text('0.80'), findsNWidgets(2));
    expect(find.text('-0.030'), findsNWidgets(2));
    expect(tester.takeException(), isNull);
  });

}
