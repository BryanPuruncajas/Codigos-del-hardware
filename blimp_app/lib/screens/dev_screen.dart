import 'dart:async';
import 'package:flutter/material.dart';
import '../core/app_config.dart';
import '../core/prefs.dart';
import '../core/theme/app_theme.dart';
import '../services/telemetry_service.dart';
import '../widgets/aerostock_logo.dart';
import '../widgets/telemetry_widgets.dart';
import 'manual_control_screen.dart';

class DevScreen extends StatefulWidget {
  final bool startInDemo;
  const DevScreen({super.key, this.startInDemo = false});

  @override
  State<DevScreen> createState() => _DevScreenState();
}

class _DevScreenState extends State<DevScreen> {
  final TelemetryService service = TelemetryService();
  final ipController = persistentController('conn_host', '');

  @override
  void initState() {
    super.initState();
    if (widget.startInDemo) service.startDemo();
  }

  @override
  void dispose() {
    service.dispose();
    ipController.dispose();
    super.dispose();
  }

  void _showConnectDialog() {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Conectar como Dev'),
        content: TextField(
          controller: ipController,
          decoration: const InputDecoration(
            labelText: 'IP de la laptop o URL del tunel',
            hintText: '192.168.1.5  o  https://algo.trycloudflare.com',
          ),
          keyboardType: TextInputType.url,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancelar'),
          ),
          FilledButton(
            onPressed: () {
              service.connect(
                ipController.text.trim(),
                devToken: AppConfig.devToken,
              );
              Navigator.pop(context);
            },
            child: const Text('Conectar'),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        flexibleSpace: const DecoratedBox(decoration: BoxDecoration(gradient: AppTheme.appBarGradient)),
        title: Row(
          mainAxisSize: MainAxisSize.min,
          children: const [
            AeroStockMark(size: 26, envelopeColor: Colors.white70),
            SizedBox(width: 10),
            Text('Control (Dev)'),
          ],
        ),
        actions: [
          ListenableBuilder(
            listenable: service,
            builder: (context, _) => Padding(
              padding: const EdgeInsets.symmetric(horizontal: 6),
              child: ConnectionChip(service: service),
            ),
          ),
          IconButton(
            icon: Icon(service.demoMode ? Icons.science : Icons.science_outlined),
            tooltip: 'Modo demostración (sin hardware)',
            onPressed: () => service.demoMode ? service.stopDemo() : service.startDemo(),
          ),
          IconButton(
            icon: const Icon(Icons.settings_ethernet),
            onPressed: _showConnectDialog,
            tooltip: 'Configurar conexión',
          ),
        ],
      ),
      body: ListenableBuilder(
        listenable: service,
        builder: (context, _) {
          final isDev = service.connected && service.role == 'dev';

          if (service.lastError != null) {
            WidgetsBinding.instance.addPostFrameCallback((_) {
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(
                  content: Text(service.lastError!),
                  backgroundColor: Theme.of(context).colorScheme.error,
                ),
              );
              service.lastError = null;
            });
          }

          if (service.connected && !isDev) {
            return Padding(
              padding: const EdgeInsets.all(24),
              child: Center(
                child: Text(
                  'Conectado, pero como "viewer" -- el token no coincidió '
                  'con el que le pasaste a telemetry_bridge.py (--dev-token). '
                  'Revisá AppConfig.devToken y el comando del bridge, tienen '
                  'que ser exactamente iguales.',
                  textAlign: TextAlign.center,
                  style: TextStyle(color: Theme.of(context).colorScheme.error),
                ),
              ),
            );
          }

          if (service.latest.isEmpty) {
            return Center(
              child: Padding(
                padding: const EdgeInsets.all(24),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(Icons.settings_input_antenna, size: 56, color: AppTheme.sky300),
                    const SizedBox(height: 16),
                    Text(
                      'Sin datos todavía.\nConectate con el ícono de red, o probá el modo demo (ícono de matraz) para ver todo el panel sin hardware.',
                      textAlign: TextAlign.center,
                      style: TextStyle(color: AppTheme.onSurfaceMuted.withValues(alpha: 0.9)),
                    ),
                  ],
                ),
              ),
            );
          }

          final armed = (service.latest['armed'] ?? 0) > 0.5;

          return ListView(
            padding: const EdgeInsets.all(12),
            children: [
              if (service.demoMode) const _DemoBanner(),
              MissionOverviewCard(latest: service.latest),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(child: OverviewRow(latest: service.latest)),
                  const SizedBox(width: 8),
                  ArmedChip(armed: armed),
                ],
              ),
              const SizedBox(height: 12),
              _SensorDiagnosticPanel(service: service),
              const SizedBox(height: 12),
              _MissionPanel(service: service),
              const SizedBox(height: 12),
              _ControlPanel(service: service),
              const SizedBox(height: 12),
              _ManualControlPanel(service: service),
              const SizedBox(height: 12),
              ControlChart(
                title: 'Altura en vivo',
                icon: Icons.height,
                unit: 'm',
                history: service.history,
                actualOf: (s) => s.height,
                refOf: (s) => s.heightRef,
                actualColor: AppTheme.blue500,
              ),
              const SizedBox(height: 12),
              ControlChart(
                title: 'Yaw en vivo',
                icon: Icons.explore,
                unit: '°',
                history: service.history,
                actualOf: (s) => s.yawDeg,
                refOf: (s) => s.yawRefDeg,
                actualColor: AppTheme.celeste,
              ),
              const SizedBox(height: 12),
              _BenchTestPanel(service: service),
              const SizedBox(height: 12),
              for (final group in kTelemetryGroups.entries)
                GroupCard(
                  title: group.key,
                  icon: kTelemetryGroupIcons[group.key]!,
                  fields: group.value,
                  latest: service.latest,
                ),
            ],
          );
        },
      ),
    );
  }
}

class _DemoBanner extends StatelessWidget {
  const _DemoBanner();

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.only(bottom: 12),
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: AppTheme.celeste100,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: AppTheme.celeste.withValues(alpha: 0.5)),
      ),
      child: const Row(
        children: [
          Icon(Icons.science_outlined, color: AppTheme.blue600, size: 18),
          SizedBox(width: 10),
          Expanded(
            child: Text(
              'MODO DEMO — datos simulados. Los comandos no llegan a ningún hardware real.',
              style: TextStyle(color: AppTheme.navy900, fontWeight: FontWeight.w600, fontSize: 12.5),
            ),
          ),
        ],
      ),
    );
  }
}

// ============================================================================
// PANEL DE DIAGNOSTICO DE SENSORES -- equivalente a correr:
//   python run_test.py p01 --seconds N
// P01 no toca motores ni servos (ver P01SensorIntegration.cpp): solo sirve
// para mirar la telemetria de sensores en el tiempo, p.ej. para ver si la
// altura del barometro deriva sola con el blimp quieto.
// ============================================================================
class _SensorDiagnosticPanel extends StatefulWidget {
  final TelemetryService service;
  const _SensorDiagnosticPanel({required this.service});

  @override
  State<_SensorDiagnosticPanel> createState() => _SensorDiagnosticPanelState();
}

class _SensorDiagnosticPanelState extends State<_SensorDiagnosticPanel>
    with AutomaticKeepAliveClientMixin {
  @override
  bool get wantKeepAlive => true;

  final durationController = TextEditingController(text: '900');

  bool running = false;
  DateTime? startedAt;
  Timer? _autoStopTimer;
  String status = 'Detenido';

  @override
  void dispose() {
    durationController.dispose();
    _autoStopTimer?.cancel();
    super.dispose();
  }

  Future<void> _start() async {
    final seconds = double.tryParse(durationController.text) ?? 900;
    _autoStopTimer?.cancel();
    setState(() {
      running = true;
      startedAt = DateTime.now();
      status = 'Corriendo';
    });
    // P01 no arma actuadores (ver ACTUATED en common/control_pack.py), asi
    // que no hace falta pasarle parametros: solo activa el modo en el firmware.
    await widget.service.startMission(test: 'p01', params: const {});
    _autoStopTimer = Timer(Duration(seconds: seconds.round()), () {
      if (mounted) _stop(auto: true);
    });
  }

  void _stop({bool auto = false}) {
    _autoStopTimer?.cancel();
    widget.service.stop();
    setState(() {
      running = false;
      status = auto ? 'Terminado (duración cumplida) — revisá el CSV en logs/' : 'Detenido (STOP enviado)';
    });
  }

  @override
  Widget build(BuildContext context) {
    super.build(context);

    final elapsed = running && startedAt != null
        ? DateTime.now().difference(startedAt!).inSeconds
        : 0;
    final target = double.tryParse(durationController.text)?.round() ?? 900;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.sensors, size: 20, color: AppTheme.blue600),
                const SizedBox(width: 8),
                const Text(
                  'Diagnóstico de sensores (P01)',
                  style: TextStyle(fontWeight: FontWeight.bold, color: AppTheme.onSurface),
                ),
              ],
            ),
            const Divider(),
            const Text(
              'Sin motores ni servos. Dejá el blimp completamente quieto y '
              'mirá el gráfico "Altura en vivo" más abajo para ver si deriva '
              'sola con el tiempo.',
              style: TextStyle(color: AppTheme.onSurfaceMuted, fontSize: 12.5),
            ),
            const SizedBox(height: 8),
            Row(
              children: [
                SizedBox(
                  width: 100,
                  child: TextField(
                    controller: durationController,
                    enabled: !running,
                    keyboardType: TextInputType.number,
                    decoration: const InputDecoration(labelText: 'Duración (s)', isDense: true),
                  ),
                ),
                const SizedBox(width: 12),
                FilledButton.icon(
                  onPressed: running ? null : _start,
                  icon: const Icon(Icons.play_arrow),
                  label: const Text('Iniciar P01'),
                ),
                const SizedBox(width: 8),
                OutlinedButton.icon(
                  onPressed: running ? () => _stop() : null,
                  icon: const Icon(Icons.stop),
                  label: const Text('Detener'),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Text(
              running ? '$status  ·  $elapsed s / $target s' : status,
              style: TextStyle(color: AppTheme.onSurface.withValues(alpha: 0.7), fontSize: 13),
            ),
          ],
        ),
      ),
    );
  }
}

// ============================================================================
// PANEL DE MISION -- equivalente a correr:
//   python run_test.py p08 --height H --kp .. --ki .. --kd .. --max-power ..
//     --slew .. --vis-min-power .. --vis-max-power .. --vis-deadband-px ..
// ============================================================================
class _MissionPanel extends StatefulWidget {
  final TelemetryService service;
  const _MissionPanel({required this.service});

  @override
  State<_MissionPanel> createState() => _MissionPanelState();
}

class _MissionPanelState extends State<_MissionPanel> with AutomaticKeepAliveClientMixin {
  // Sin esto, al scrollear lo suficiente el ListView descarta este panel
  // (esta fuera del "cache extent") y lo reconstruye desde cero cuando
  // vuelve a aparecer -- perdiendo todo lo que se haya escrito en los
  // TextEditingController. wantKeepAlive=true le pide al ListView que NO
  // lo destruya mientras el usuario siga en esta pantalla.
  @override
  bool get wantKeepAlive => true;

  String test = Prefs.getString('mission_test', 'p08'); // p08 = un globo, p09 = visita + escape

  final heightController = persistentController('mission_height', '0.7');
  final kpController = persistentController('mission_kp', '0.30');
  final kiController = persistentController('mission_ki', '0.025');
  final kdController = persistentController('mission_kd', '0.10');
  final maxPowerController = persistentController('mission_max_power', '15');
  final slewController = persistentController('mission_slew', '0.18');
  final visMinController = persistentController('mission_vis_min', '6');
  final visMaxController = persistentController('mission_vis_max', '10');
  final visDeadbandController = persistentController('mission_vis_deadband', '20');

  String status = 'Detenida';
  bool busy = false;

  @override
  void dispose() {
    heightController.dispose();
    kpController.dispose();
    kiController.dispose();
    kdController.dispose();
    maxPowerController.dispose();
    slewController.dispose();
    visMinController.dispose();
    visMaxController.dispose();
    visDeadbandController.dispose();
    super.dispose();
  }

  double _num(TextEditingController c, double fallback) =>
      double.tryParse(c.text) ?? fallback;

  /// Mismos nombres que los --flags de run_test.py (sin guiones): el bridge
  /// arma fx/fz/tx/tz/aux con common/control_pack.py, igual que la CLI.
  Map<String, dynamic> _params() => {
        'height': _num(heightController, 0.7),
        'kp': _num(kpController, 0.30),
        'ki': _num(kiController, 0.025),
        'kd': _num(kdController, 0.10),
        'max_power': _num(maxPowerController, 15),
        'slew': _num(slewController, 0.18),
        'vis_min_power': _num(visMinController, 6),
        'vis_max_power': _num(visMaxController, 10),
        'vis_deadband_px': _num(visDeadbandController, 20),
      };

  Future<void> _start() async {
    setState(() => busy = true);
    await widget.service.startMission(
      test: test,
      params: _params(),
      onStatus: (s) {
        if (mounted) setState(() => status = s);
      },
    );
    if (mounted) setState(() => busy = false);
  }

  void _update() {
    widget.service.updateMission(test: test, params: _params());
    setState(() => status = 'Ganancias actualizadas (sin rearmar)');
  }

  void _stop() {
    widget.service.stop();
    setState(() => status = 'Detenida (STOP enviado)');
  }

  @override
  Widget build(BuildContext context) {
    super.build(context); // requerido por AutomaticKeepAliveClientMixin

    final missionState = widget.service.latest['mission_state'];
    final missionRunning = missionState != null;

    final armedVal = widget.service.latest['armed'];
    final isArmed = armedVal != null && armedVal > 0.5;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.rocket_launch, size: 20, color: AppTheme.blue600),
                const SizedBox(width: 8),
                const Text(
                  'Misión (P08–P11)',
                  style: TextStyle(fontWeight: FontWeight.bold, color: AppTheme.onSurface),
                ),
                const Spacer(),
                ArmedChip(armed: isArmed),
              ],
            ),
            const Divider(),
            DropdownButton<String>(
              value: test,
              items: const [
                DropdownMenuItem(value: 'p08', child: Text('P08 - Un globo')),
                DropdownMenuItem(value: 'p09', child: Text('P09 - Visita + escape')),
                DropdownMenuItem(value: 'p10', child: Text('P10 - Dos globos')),
                DropdownMenuItem(value: 'p11', child: Text('P11 - Cuatro globos')),
              ],
              onChanged: busy
                  ? null
                  : (v) => setState(() {
                        test = v!;
                        Prefs.setString('mission_test', test);
                      }),
            ),
            const SizedBox(height: 8),
            Wrap(
              spacing: 12,
              runSpacing: 12,
              children: [
                _f('Altura (m)', heightController),
                _f('Kp', kpController),
                _f('Ki', kiController),
                _f('Kd', kdController),
                _f('Max power (%)', maxPowerController),
                _f('Slew (/s)', slewController),
                _f('Vis min (%)', visMinController),
                _f('Vis max (%)', visMaxController),
                _f('Deadband (px)', visDeadbandController),
              ],
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                FilledButton.icon(
                  onPressed: busy ? null : _start,
                  icon: busy
                      ? const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                        )
                      : const Icon(Icons.play_arrow),
                  label: const Text('Iniciar misión'),
                ),
                const SizedBox(width: 8),
                OutlinedButton.icon(
                  onPressed: busy ? null : _update,
                  icon: const Icon(Icons.tune),
                  label: const Text('Actualizar'),
                ),
                const SizedBox(width: 8),
                FilledButton.icon(
                  style: FilledButton.styleFrom(
                    backgroundColor: Theme.of(context).colorScheme.error,
                  ),
                  onPressed: _stop,
                  icon: const Icon(Icons.stop),
                  label: const Text('Detener'),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Text(
              missionRunning
                  ? '$status  ·  estado real: ${kMissionStateNames[missionState.toInt().clamp(0, kMissionStateNames.length - 1)]}'
                  : status,
              style: TextStyle(color: AppTheme.onSurface.withValues(alpha: 0.7), fontSize: 13),
            ),
          ],
        ),
      ),
    );
  }

  Widget _f(String label, TextEditingController c) {
    return SizedBox(
      width: 100,
      child: TextField(
        controller: c,
        keyboardType: const TextInputType.numberWithOptions(decimal: true, signed: true),
        decoration: InputDecoration(labelText: label, isDense: true),
      ),
    );
  }
}

// ============================================================================
// PANEL DE CONTROL SIMPLE -- P04/P05, para seguir afinando el PID de altura.
// ============================================================================
class _ControlPanel extends StatefulWidget {
  final TelemetryService service;
  const _ControlPanel({required this.service});

  @override
  State<_ControlPanel> createState() => _ControlPanelState();
}

class _ControlPanelState extends State<_ControlPanel> with AutomaticKeepAliveClientMixin {
  // Ver el comentario equivalente en _MissionPanelState: sin esto, scrollear
  // lo suficiente hace que el ListView destruya y recree este panel,
  // perdiendo los valores escritos en los TextEditingController.
  @override
  bool get wantKeepAlive => true;

  String test = Prefs.getString('control_test', 'p04'); // p04 = altura sola, p05 = yaw + altura

  final fzController = persistentController('control_fz', '0.7');
  final kpController = persistentController('control_kp', '0.30');
  final kiController = persistentController('control_ki', '0.025');
  final kdController = persistentController('control_kd', '0.10');
  final maxPowerController = persistentController('control_max_power', '15');
  final slewController = persistentController('control_slew', '0.18');
  final yawDegController = persistentController('control_yaw_deg', '0');

  String status = '';
  bool busy = false;

  @override
  void dispose() {
    fzController.dispose();
    kpController.dispose();
    kiController.dispose();
    kdController.dispose();
    maxPowerController.dispose();
    slewController.dispose();
    yawDegController.dispose();
    super.dispose();
  }

  double _num(TextEditingController c, double fallback) =>
      double.tryParse(c.text) ?? fallback;

  /// Mismos nombres que los --flags de run_test.py (sin guiones).
  Map<String, dynamic> _params() => {
        'height': _num(fzController, 0.7),
        'kp': _num(kpController, 0.30),
        'ki': _num(kiController, 0.025),
        'kd': _num(kdController, 0.10),
        'max_power': _num(maxPowerController, 15),
        'slew': _num(slewController, 0.18),
        if (test == 'p05') 'yaw_deg': _num(yawDegController, 0),
      };

  Future<void> _start() async {
    setState(() => busy = true);
    await widget.service.startMission(
      test: test,
      params: _params(),
      onStatus: (s) {
        if (mounted) setState(() => status = s);
      },
    );
    if (mounted) setState(() => busy = false);
  }

  void _update() {
    widget.service.updateMission(test: test, params: _params());
    setState(() => status = 'Ganancias actualizadas (sin rearmar)');
  }

  Widget _paramField(String label, TextEditingController c, {required double width}) {
    return SizedBox(
      width: width,
      child: TextField(
        controller: c,
        keyboardType: const TextInputType.numberWithOptions(decimal: true, signed: true),
        decoration: InputDecoration(labelText: label, isDense: true),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    super.build(context); // requerido por AutomaticKeepAliveClientMixin

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.tune, size: 20, color: AppTheme.blue600),
                const SizedBox(width: 8),
                const Text(
                  'Panel de control (P04/P05)',
                  style: TextStyle(fontWeight: FontWeight.bold, color: AppTheme.onSurface),
                ),
              ],
            ),
            const Divider(),
            DropdownButton<String>(
              value: test,
              items: const [
                DropdownMenuItem(value: 'p04', child: Text('P04 - Altura sola')),
                DropdownMenuItem(value: 'p05', child: Text('P05 - Yaw + altura')),
              ],
              onChanged: busy
                  ? null
                  : (v) => setState(() {
                        test = v!;
                        Prefs.setString('control_test', test);
                      }),
            ),
            const SizedBox(height: 8),
            Wrap(
              spacing: 12,
              runSpacing: 12,
              children: [
                _paramField('Altura (m)', fzController, width: 100),
                _paramField('Kp', kpController, width: 90),
                _paramField('Ki', kiController, width: 90),
                _paramField('Kd', kdController, width: 90),
                _paramField('Max power (%)', maxPowerController, width: 120),
                _paramField('Slew (/s)', slewController, width: 100),
                if (test == 'p05') _paramField('Yaw ref (°)', yawDegController, width: 100),
              ],
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                FilledButton.icon(
                  onPressed: busy ? null : _start,
                  icon: busy
                      ? const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                        )
                      : const Icon(Icons.play_arrow),
                  label: const Text('Iniciar'),
                ),
                const SizedBox(width: 8),
                OutlinedButton.icon(
                  onPressed: busy ? null : _update,
                  icon: const Icon(Icons.tune),
                  label: const Text('Actualizar'),
                ),
                const Spacer(),
                FilledButton.icon(
                  style: FilledButton.styleFrom(
                    backgroundColor: Theme.of(context).colorScheme.error,
                  ),
                  onPressed: widget.service.stop,
                  icon: const Icon(Icons.stop),
                  label: const Text('STOP'),
                ),
              ],
            ),
            if (status.isNotEmpty) ...[
              const SizedBox(height: 8),
              Text(
                status,
                style: TextStyle(color: AppTheme.onSurface.withValues(alpha: 0.7), fontSize: 13),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

// ============================================================================
// CONTROL MANUAL -- equivalente de run_test.py p02.
//
// Arma P02 desde aca y abre ManualControlScreen (pantalla propia, bloqueada
// en vertical) con el mando y la palanca de potencia -- ver
// screens/manual_control_screen.dart. No conviene como una tarjeta mas
// dentro de esta lista con scroll: el mando necesita pantalla completa y
// que nada se reordene por un giro accidental del telefono a mitad de vuelo.
// ============================================================================
class _ManualControlPanel extends StatefulWidget {
  final TelemetryService service;
  const _ManualControlPanel({required this.service});

  @override
  State<_ManualControlPanel> createState() => _ManualControlPanelState();
}

class _ManualControlPanelState extends State<_ManualControlPanel>
    with AutomaticKeepAliveClientMixin {
  @override
  bool get wantKeepAlive => true;

  bool arming = false;
  String status = '';

  Future<void> _confirmAndArm() async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        icon: const Icon(Icons.warning_amber_rounded, color: AppTheme.danger, size: 32),
        title: const Text('¿Armar control manual?'),
        content: const Text(
          'P02 mueve los brushless de verdad con el mando de la pantalla '
          'siguiente. Asegurate de tener espacio despejado antes de continuar.',
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancelar')),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: AppTheme.danger),
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Armar'),
          ),
        ],
      ),
    );
    if (ok != true) return;

    setState(() => arming = true);
    await widget.service.startMission(
      test: 'p02',
      params: const {},
      onStatus: (s) {
        if (mounted) setState(() => status = s);
      },
    );
    if (!mounted) return;
    setState(() => arming = false);

    await Navigator.of(context).push(
      MaterialPageRoute(builder: (_) => ManualControlScreen(service: widget.service)),
    );

    if (mounted) setState(() => status = 'Detenido (STOP enviado)');
  }

  @override
  Widget build(BuildContext context) {
    super.build(context); // requerido por AutomaticKeepAliveClientMixin

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                const Icon(Icons.gamepad_outlined, size: 20, color: AppTheme.blue600),
                const SizedBox(width: 8),
                const Text('Control manual (P02)',
                    style: TextStyle(fontWeight: FontWeight.bold, color: AppTheme.onSurface)),
              ],
            ),
            const Divider(),
            const Text(
              'Se abre en una pantalla aparte, bloqueada en vertical, con el '
              'mando y la palanca de potencia.',
              style: TextStyle(fontSize: 11.5, color: AppTheme.onSurfaceMuted),
            ),
            const SizedBox(height: 10),
            FilledButton.icon(
              style: FilledButton.styleFrom(backgroundColor: AppTheme.danger),
              onPressed: arming ? null : _confirmAndArm,
              icon: arming
                  ? const SizedBox(
                      width: 16,
                      height: 16,
                      child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                    )
                  : const Icon(Icons.warning_amber_rounded),
              label: Text(arming ? 'Armando...' : 'Armar y abrir mando'),
            ),
            if (status.isNotEmpty) ...[
              const SizedBox(height: 8),
              Text(status,
                  style: const TextStyle(fontSize: 12.5, color: AppTheme.onSurfaceMuted)),
            ],
          ],
        ),
      ),
    );
  }
}

// ============================================================================
// BANCO DE PRUEBAS -- equivalente de run_test.py p00 / p00m.
//
// Colapsado por default a proposito: P00M SI gira helices. No queremos que
// el panel mas peligroso de toda la app sea tambien el primero que se ve
// al entrar a Dev, ni algo con lo que se pueda interactuar sin querer
// mientras se scrollea de paso.
// ============================================================================
class _BenchTestPanel extends StatefulWidget {
  final TelemetryService service;
  const _BenchTestPanel({required this.service});

  @override
  State<_BenchTestPanel> createState() => _BenchTestPanelState();
}

class _BenchTestPanelState extends State<_BenchTestPanel> with AutomaticKeepAliveClientMixin {
  @override
  bool get wantKeepAlive => true;

  // P00 -- servos. Con el Tower Pro (13/09, rango 0-180) el vertical de
  // arranque es simetrico: 90/90, avance 180/0, retroceso 0/180 -- ver
  // SERVO*_Z_DEG/FORWARD_DEG/BACK_DEG en firmware/src/app/ControlCommon.h.
  // Sin re-validar en vuelo todavia. Si esas constantes cambian, estos
  // presets tambien hay que actualizarlos.
  String servoSel = Prefs.getString('bench_servo_sel', 'both');
  double servo1Angle = double.tryParse(Prefs.getString('bench_servo1_angle', '90')) ?? 90;
  double servo2Angle = double.tryParse(Prefs.getString('bench_servo2_angle', '90')) ?? 90;

  // P00M -- motores. servo1/servo2 son el vector que se usa TANTO para
  // posicionar antes de armar COMO en cada paquete de potencia (el firmware
  // no los recuerda entre mensajes de este modo).
  late double servo1 = double.tryParse(servo1Controller.text) ?? 90;
  late double servo2 = double.tryParse(servo2Controller.text) ?? 90;
  final servo1Controller = persistentController('bench_motor_servo1', '90');
  final servo2Controller = persistentController('bench_motor_servo2', '90');
  String motorSel = Prefs.getString('bench_motor_sel', 'both');
  double motorPower = 0;
  bool motorsReady = false;
  bool arming = false;
  String status = '';

  @override
  void dispose() {
    servo1Controller.dispose();
    servo2Controller.dispose();
    super.dispose();
  }

  Future<void> _confirmAndArm() async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        icon: const Icon(Icons.warning_amber_rounded, color: AppTheme.danger, size: 32),
        title: const Text('¿Armar los brushless?'),
        content: const Text(
          'Esto va a activar los motores brushless de verdad. Asegurate de que '
          'el blimp esté sujeto y despejado antes de continuar.\n\n'
          'Sin failsafe de enlace: si se corta la conexión, el firmware mantiene '
          'la última potencia recibida. El botón STOP de abajo sí corta todo.',
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancelar')),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: AppTheme.danger),
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Armar'),
          ),
        ],
      ),
    );
    if (ok != true) return;

    setState(() => arming = true);
    await widget.service.armMotors(
      servo1: servo1,
      servo2: servo2,
      onStatus: (s) {
        if (mounted) setState(() => status = s);
      },
    );
    if (mounted) {
      setState(() {
        arming = false;
        motorsReady = true;
        motorPower = 0;
      });
    }
  }

  void _stop() {
    widget.service.stop();
    setState(() {
      motorsReady = false;
      motorPower = 0;
      status = 'STOP enviado -- desarmado';
    });
  }

  Widget _servoSelector(String value, ValueChanged<String> onChanged) {
    return SegmentedButton<String>(
      segments: const [
        ButtonSegment(value: '1', label: Text('S1')),
        ButtonSegment(value: '2', label: Text('S2')),
        ButtonSegment(value: 'both', label: Text('Ambos')),
      ],
      selected: {value},
      onSelectionChanged: (s) => onChanged(s.first),
    );
  }

  void _applyServoPreset(double s1, double s2) {
    setState(() {
      servoSel = 'both';
      servo1Angle = s1;
      servo2Angle = s2;
    });
    _sendServoAngles();
  }

  void _sendServoAngles() {
    Prefs.setString('bench_servo1_angle', servo1Angle.toString());
    Prefs.setString('bench_servo2_angle', servo2Angle.toString());
    widget.service.setServo(servo: servoSel, angle1: servo1Angle, angle2: servo2Angle);
  }

  Widget _servoAngleSlider({
    required String label,
    required double value,
    required ValueChanged<double> onChanged,
    required ValueChanged<double> onChangeEnd,
  }) {
    return Row(
      children: [
        SizedBox(width: 56, child: Text(label, style: const TextStyle(fontSize: 12.5))),
        Expanded(
          child: Slider(
            value: value,
            min: 0,
            max: 180,
            divisions: 180,
            label: '${value.round()}°',
            onChanged: onChanged,
            onChangeEnd: onChangeEnd,
          ),
        ),
        SizedBox(width: 40, child: Text('${value.round()}°', textAlign: TextAlign.end)),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    super.build(context); // requerido por AutomaticKeepAliveClientMixin

    return Card(
      clipBehavior: Clip.antiAlias,
      child: ExpansionTile(
        leading: const Icon(Icons.build_circle_outlined, color: AppTheme.danger),
        title: const Text('Banco de pruebas (P00 / P00M)',
            style: TextStyle(fontWeight: FontWeight.bold, color: AppTheme.onSurface)),
        subtitle: const Text('Servos y motores en crudo -- sin PID, sin misión',
            style: TextStyle(fontSize: 12)),
        childrenPadding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
        children: [
          // ------------------------------------------------------------
          // P00 -- SERVOS (brushless bloqueados en este modo, sin riesgo)
          // ------------------------------------------------------------
          const Align(
            alignment: Alignment.centerLeft,
            child: Text('Servos (P00)', style: TextStyle(fontWeight: FontWeight.w700)),
          ),
          const SizedBox(height: 8),
          _servoSelector(servoSel, (v) => setState(() {
                servoSel = v;
                Prefs.setString('bench_servo_sel', v);
              })),
          const SizedBox(height: 4),
          Text(
            servoSel == 'both'
                ? 'Ambos: manda los DOS ángulos de abajo a la vez (en espejo, como un vector real).'
                : 'Solo S$servoSel: el otro servo queda desconectado.',
            style: const TextStyle(fontSize: 11.5, color: AppTheme.onSurfaceMuted),
          ),
          const SizedBox(height: 10),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              OutlinedButton(
                onPressed: () => _applyServoPreset(90, 90),
                child: const Text('Vertical (90/90)'),
              ),
              OutlinedButton(
                onPressed: () => _applyServoPreset(180, 0),
                child: const Text('Avance (180/0)'),
              ),
              OutlinedButton(
                onPressed: () => _applyServoPreset(0, 180),
                child: const Text('Retroceso (0/180)'),
              ),
            ],
          ),
          const SizedBox(height: 12),
          _servoAngleSlider(
            label: 'Servo 1',
            value: servo1Angle,
            onChanged: (v) => setState(() => servo1Angle = v),
            onChangeEnd: (_) => _sendServoAngles(),
          ),
          _servoAngleSlider(
            label: 'Servo 2',
            value: servo2Angle,
            onChanged: (v) => setState(() => servo2Angle = v),
            onChangeEnd: (_) => _sendServoAngles(),
          ),
          const Text(
            'Los presets usan los mismos vectores que la misión real. Con el '
            'servo actual el vertical de arranque es simétrico (90°/90°), '
            'pero todavía sin validar en vuelo -- si al recalibrar resulta '
            'que hace falta un vector asimétrico (como pasaba con el '
            'servo viejo), hay que actualizar estos presets.',
            style: TextStyle(fontSize: 11.5, color: AppTheme.onSurfaceMuted),
          ),

          const Divider(height: 32),

          // ------------------------------------------------------------
          // P00M -- MOTORES (esto SI gira helices)
          // ------------------------------------------------------------
          const Row(
            children: [
              Icon(Icons.warning_amber_rounded, size: 16, color: AppTheme.danger),
              SizedBox(width: 6),
              Text('Motores (P00M)', style: TextStyle(fontWeight: FontWeight.w700)),
            ],
          ),
          const SizedBox(height: 8),
          Wrap(
            spacing: 16,
            runSpacing: 8,
            children: [
              SizedBox(
                width: 130,
                child: TextField(
                  enabled: !motorsReady && !arming,
                  keyboardType: const TextInputType.numberWithOptions(decimal: true),
                  decoration: const InputDecoration(labelText: 'Servo 1 (°)', isDense: true),
                  controller: servo1Controller,
                  onChanged: (v) => servo1 = double.tryParse(v) ?? servo1,
                ),
              ),
              SizedBox(
                width: 130,
                child: TextField(
                  enabled: !motorsReady && !arming,
                  keyboardType: const TextInputType.numberWithOptions(decimal: true),
                  decoration: const InputDecoration(labelText: 'Servo 2 (°)', isDense: true),
                  controller: servo2Controller,
                  onChanged: (v) => servo2 = double.tryParse(v) ?? servo2,
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          if (!motorsReady)
            FilledButton.icon(
              style: FilledButton.styleFrom(backgroundColor: AppTheme.danger),
              onPressed: arming ? null : _confirmAndArm,
              icon: arming
                  ? const SizedBox(
                      width: 16,
                      height: 16,
                      child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                    )
                  : const Icon(Icons.warning_amber_rounded),
              label: Text(arming ? 'Armando...' : 'Posicionar + Armar'),
            )
          else ...[
            _servoSelector(motorSel, (v) => setState(() {
                  motorSel = v;
                  Prefs.setString('bench_motor_sel', v);
                })),
            const SizedBox(height: 8),
            Row(
              children: [
                Expanded(
                  child: Slider(
                    value: motorPower,
                    min: 0,
                    max: 100,
                    divisions: 100,
                    label: '${motorPower.round()}%',
                    activeColor: AppTheme.danger,
                    onChanged: (v) => setState(() => motorPower = v),
                    onChangeEnd: (v) => widget.service.setMotorPower(
                      motor: motorSel,
                      power: v,
                      servo1: servo1,
                      servo2: servo2,
                    ),
                  ),
                ),
                SizedBox(
                  width: 48,
                  child: Text('${motorPower.round()}%', textAlign: TextAlign.end),
                ),
              ],
            ),
            const SizedBox(height: 8),
            SizedBox(
              width: double.infinity,
              child: FilledButton.icon(
                style: FilledButton.styleFrom(backgroundColor: AppTheme.danger),
                onPressed: _stop,
                icon: const Icon(Icons.stop),
                label: const Text('STOP'),
              ),
            ),
            const SizedBox(height: 4),
            const Text(
              'Sin failsafe de enlace: si se corta la conexión, el firmware '
              'mantiene la última potencia. No empieces con potencias altas.',
              style: TextStyle(fontSize: 11.5, color: AppTheme.danger),
            ),
          ],
          if (status.isNotEmpty) ...[
            const SizedBox(height: 8),
            Text(status,
                style: const TextStyle(fontSize: 12.5, color: AppTheme.onSurfaceMuted)),
          ],
        ],
      ),
    );
  }
}