import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:blimp_app/services/telemetry_service.dart';
import 'package:blimp_app/widgets/manual_control_panel.dart';

class FakeService extends TelemetryService {
  final List<Map<String, dynamic>> controls = [];
  Map<String, dynamic>? startParams;
  int stops = 0;
  FakeService() { connected = true; role = 'dev'; report(0, 0); }
  void report(int mode, int armed, {int age = 0}) {
    latest.addAll({'mode': mode.toDouble(), 'armed': armed.toDouble(),
      'received_f3_ms': (DateTime.now().millisecondsSinceEpoch-age).toDouble()});
    notifyListeners();
  }
  @override
  Future<void> startMission({required String test, required Map<String, dynamic> params,
      void Function(String status)? onStatus}) async { startParams = {'test': test, ...params}; }
  @override
  void sendControl({required int mode, double fx=0, double fz=0, double tx=0, double tz=0,
      int arm=0, int reload=0, int reset=0, Map<String, double>? aux}) {
    controls.add({'mode': mode, 'fx': fx, 'fz': fz, 'tz': tz, 'aux': aux});
  }
  @override
  void stop() { stops++; }
}

Future<void> panel(WidgetTester tester, FakeService service) async {
  tester.view.physicalSize = const Size(800, 1100);
  tester.view.devicePixelRatio = 1;
  addTearDown(() { tester.view.resetPhysicalSize(); tester.view.resetDevicePixelRatio(); service.dispose(); });
  await tester.pumpWidget(MaterialApp(home: Scaffold(body: SingleChildScrollView(child: ManualControlPanel(service: service)))));
}

void main() {
  testWidgets('arming request alone never displays armed controls', (tester) async {
    final service = FakeService();
    await panel(tester, service);
    await tester.tap(find.text('Armar P02')); await tester.pumpAndSettle();
    await tester.tap(find.widgetWithText(FilledButton, 'Armar')); await tester.pumpAndSettle();
    expect(service.startParams, {'test': 'p02', 'duration_s': 120});
    expect(find.text('Subir'), findsNothing);
    service.report(11, 1); await tester.pump();
    expect(find.text('Subir'), findsOneWidget);
  });

  testWidgets('selected percent reaches P02 and release sends neutral', (tester) async {
    final service = FakeService()..report(11, 1);
    await panel(tester, service);
    final slider = tester.widget<Slider>(find.byKey(const Key('p02-power')));
    expect(slider.max, 50);
    slider.onChanged!(50);
    await tester.pump();
    final gesture = await tester.startGesture(tester.getCenter(find.text('Subir')));
    await tester.pump();
    expect(service.controls.last['fz'], .12);
    expect(service.controls.last['aux'], {'5': 50.0});
    expect(tester.widget<Slider>(find.byKey(const Key('p02-power'))).onChanged, isNull);
    await gesture.up(); await tester.pump();
    expect(service.controls.last['fz'], 0);
    expect(service.controls.last['fx'], 0);
    expect(service.controls.last['aux'], {'5': 50.0});
    await tester.tap(find.text('STOP')); await tester.pump();
    expect(service.stops, 1);
  });

  testWidgets('a timed disarm returns to arm button without a reconnect', (tester) async {
    final service = FakeService()..report(11, 1);
    await panel(tester, service);
    expect(find.text('Subir'), findsOneWidget);
    service.report(0, 0); await tester.pump();
    expect(find.text('Subir'), findsNothing);
    expect(find.text('Armar P02'), findsOneWidget);
  });

  testWidgets('stale F3 never presents an armed manual session', (tester) async {
    final service = FakeService()..report(11, 1, age: 2000);
    await panel(tester, service);
    expect(find.text('Subir'), findsNothing);
    expect(find.text('Sin confirmación reciente de actuadores.'), findsOneWidget);
    final button = tester.widget<FilledButton>(find.widgetWithText(FilledButton, 'Armar P02'));
    expect(button.onPressed, isNull);
  });
}
