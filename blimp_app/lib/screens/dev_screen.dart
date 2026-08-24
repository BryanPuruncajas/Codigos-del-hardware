import 'package:flutter/material.dart';
import '../core/app_config.dart';
import '../core/theme/app_theme.dart';
import '../services/telemetry_service.dart';
import '../widgets/telemetry_widgets.dart';

class DevScreen extends StatefulWidget {
  const DevScreen({super.key});

  @override
  State<DevScreen> createState() => _DevScreenState();
}

class _DevScreenState extends State<DevScreen> {
  final TelemetryService service = TelemetryService();
  final ipController = TextEditingController();

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
          decoration: const InputDecoration(labelText: 'IP de la laptop'),
          keyboardType: TextInputType.number,
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
        title: const Text('Blimp Control (Dev)'),
        actions: [
          ListenableBuilder(
            listenable: service,
            builder: (context, _) {
              final isDev = service.connected && service.role == 'dev';
              return Padding(
                padding: const EdgeInsets.symmetric(horizontal: 8),
                child: Chip(
                  avatar: Icon(
                    service.connected
                        ? (isDev ? Icons.verified_user : Icons.wifi)
                        : Icons.wifi_off,
                    color: Colors.white,
                    size: 18,
                  ),
                  label: Text(
                    !service.connected
                        ? 'Desconectado'
                        : (isDev ? 'Dev' : 'Conectado (solo viewer)'),
                  ),
                  backgroundColor: !service.connected
                      ? Theme.of(context).colorScheme.error
                      : (isDev ? AppTheme.success : AppTheme.secondary),
                  labelStyle: const TextStyle(color: Colors.white),
                ),
              );
            },
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
              child: Text(
                'Sin datos todavía.\nTocá el ícono de conexión arriba a la derecha.',
                textAlign: TextAlign.center,
                style: TextStyle(color: AppTheme.onSurface.withValues(alpha: 0.6)),
              ),
            );
          }

          return ListView(
            padding: const EdgeInsets.all(12),
            children: [
              MissionBadge(latest: service.latest),
              const SizedBox(height: 12),
              _MissionPanel(service: service),
              const SizedBox(height: 12),
              _ControlPanel(service: service),
              const SizedBox(height: 12),
              if (service.heightHistory.length > 1) HeightChart(service: service),
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

class _MissionPanelState extends State<_MissionPanel> {
  int mode = 17; // 17 = P08 (un globo)

  final heightController = TextEditingController(text: '0.7');
  final kpController = TextEditingController(text: '0.30');
  final kiController = TextEditingController(text: '0.025');
  final kdController = TextEditingController(text: '0.40');
  final maxPowerController = TextEditingController(text: '20');
  final slewController = TextEditingController(text: '0.18');
  final visMinController = TextEditingController(text: '5');
  final visMaxController = TextEditingController(text: '5');
  final visDeadbandController = TextEditingController(text: '30');

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

  Future<void> _start() async {
    setState(() => busy = true);
    await widget.service.startActuatedMission(
      mode: mode,
      fz: _num(heightController, 0.7),
      fx: _num(kpController, 0.30),
      tx: _num(kdController, 0.40),
      tz: _num(kiController, 0.025),
      aux: {
        '0': _num(maxPowerController, 20) / 100.0,
        '1': _num(slewController, 0.18),
        '2': _num(visMinController, 5) / 100.0,
        '3': _num(visMaxController, 5) / 100.0,
        '4': _num(visDeadbandController, 30),
      },
      onStatus: (s) {
        if (mounted) setState(() => status = s);
      },
    );
    if (mounted) setState(() => busy = false);
  }

  void _stop() {
    widget.service.stop();
    setState(() => status = 'Detenida (STOP enviado)');
  }

  @override
  Widget build(BuildContext context) {
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
                Icon(Icons.rocket_launch, size: 20, color: AppTheme.primary),
                const SizedBox(width: 8),
                const Text(
                  'Misión (P08 / P09)',
                  style: TextStyle(fontWeight: FontWeight.bold, color: AppTheme.onSurface),
                ),
                const Spacer(),
                Chip(
                  avatar: Icon(
                    isArmed ? Icons.lock_open : Icons.lock,
                    size: 16,
                    color: Colors.white,
                  ),
                  label: Text(isArmed ? 'Armado' : 'Desarmado'),
                  backgroundColor: isArmed
                      ? AppTheme.success
                      : AppTheme.onSurface.withValues(alpha: 0.4),
                  labelStyle: const TextStyle(color: Colors.white, fontSize: 12),
                  visualDensity: VisualDensity.compact,
                  materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
                ),
              ],
            ),
            const Divider(),
            DropdownButton<int>(
              value: mode,
              items: const [
                DropdownMenuItem(value: 17, child: Text('P08 - Un globo')),
                DropdownMenuItem(value: 18, child: Text('P09 - Visita + escape')),
              ],
              onChanged: busy ? null : (v) => setState(() => mode = v!),
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
                  ? '$status  ·  estado real: ${kMissionStateNames[missionState.toInt()] ?? missionState.toInt()}'
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

class _ControlPanelState extends State<_ControlPanel> {
  int mode = 13; // 13 = P04 altura, 14 = P05 yaw+altura

  final fzController = TextEditingController(text: '0.7');
  final kpController = TextEditingController(text: '0.30');
  final kiController = TextEditingController(text: '0.025');
  final kdController = TextEditingController(text: '0.40');
  final maxPowerController = TextEditingController(text: '20');
  final slewController = TextEditingController(text: '0.18');

  @override
  void dispose() {
    fzController.dispose();
    kpController.dispose();
    kiController.dispose();
    kdController.dispose();
    maxPowerController.dispose();
    slewController.dispose();
    super.dispose();
  }

  double _num(TextEditingController c, double fallback) =>
      double.tryParse(c.text) ?? fallback;

  void _sendControl() {
    widget.service.sendControl(
      mode: mode,
      fz: _num(fzController, 0),
      tx: _num(kdController, 0),
      tz: _num(kiController, 0),
      fx: _num(kpController, 0),
      aux: {
        '5': _num(maxPowerController, 20) / 100.0,
        '6': _num(slewController, 0.18),
      },
      reset: 1,
    );
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
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.tune, size: 20, color: AppTheme.primary),
                const SizedBox(width: 8),
                const Text(
                  'Panel de control (P04/P05)',
                  style: TextStyle(fontWeight: FontWeight.bold, color: AppTheme.onSurface),
                ),
              ],
            ),
            const Divider(),
            DropdownButton<int>(
              value: mode,
              items: const [
                DropdownMenuItem(value: 13, child: Text('P04 - Altura sola')),
                DropdownMenuItem(value: 14, child: Text('P05 - Yaw + altura')),
              ],
              onChanged: (v) => setState(() => mode = v!),
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
              ],
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                FilledButton.icon(
                  onPressed: _sendControl,
                  icon: const Icon(Icons.send),
                  label: const Text('Enviar'),
                ),
                const SizedBox(width: 8),
                OutlinedButton.icon(
                  onPressed: widget.service.arm,
                  icon: const Icon(Icons.play_arrow),
                  label: const Text('Armar'),
                ),
                const SizedBox(width: 8),
                OutlinedButton.icon(
                  onPressed: widget.service.disarm,
                  icon: const Icon(Icons.pause),
                  label: const Text('Desarmar'),
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
          ],
        ),
      ),
    );
  }
}