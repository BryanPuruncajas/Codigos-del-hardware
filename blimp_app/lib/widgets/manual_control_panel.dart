import 'package:flutter/material.dart';
import '../core/theme/app_theme.dart';
import '../services/telemetry_service.dart';
import 'telemetry_widgets.dart';

class ManualControlPanel extends StatefulWidget {
  final TelemetryService service;
  const ManualControlPanel({super.key, required this.service});
  @override
  State<ManualControlPanel> createState() => _ManualControlPanelState();
}

class _ManualControlPanelState extends State<ManualControlPanel> {
  static const mode = 11;
  double powerPercent = 30;
  bool arming = false;
  bool held = false;
  String status = '';

  bool get fresh {
    final at = widget.service.latest['received_f3_ms'];
    if (at == null) return false;
    final age = DateTime.now().millisecondsSinceEpoch - at;
    return age >= 0 && age < 1500;
  }
  bool get canSend => widget.service.connected && widget.service.role == 'dev' && !widget.service.demoMode;
  bool get armed => canSend && fresh && widget.service.latest['armed'] == 1 && widget.service.latest['mode'] == mode;

  Future<void> arm() async {
    if (!canSend || !fresh || widget.service.latest['armed'] != 0) return;
    final ok = await showDialog<bool>(context: context, builder: (context) => AlertDialog(
      title: const Text('¿Armar control manual?'),
      content: Text('P02 aplicará $powerPercent% al mantener una dirección. La sesión dura 120 segundos. Ten el área despejada.'),
      actions: [
        TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancelar')),
        FilledButton(onPressed: () => Navigator.pop(context, true), child: const Text('Armar')),
      ],
    ));
    if (ok != true || !mounted) return;
    setState(() => arming = true);
    try {
      await widget.service.startMission(test: 'p02', params: const {'duration_s': 120}, onStatus: (value) {
        if (mounted) setState(() => status = value);
      });
    } catch (error) {
      if (mounted) setState(() => status = 'No se pudo armar: $error');
    } finally {
      if (mounted) setState(() => arming = false);
    }
  }

  void send({double fx = 0, double fz = 0, double tz = 0}) {
    if (!armed) return;
    widget.service.sendControl(mode: mode, fx: fx, fz: fz, tz: tz, aux: {'5': powerPercent});
  }
  void neutral() {
    if (mounted) setState(() => held = false);
    send();
  }
  void stop() {
    widget.service.stop();
    setState(() { held = false; status = 'STOP solicitado; comprobar desarmado en telemetría.'; });
  }
  Widget direction(IconData icon, String label, VoidCallback press) => GestureDetector(
    onTapDown: (_) {
      if (!armed || arming) return;
      setState(() => held = true);
      press();
    },
    onTapUp: (_) => neutral(),
    onTapCancel: neutral,
    child: Container(
      width: 76, height: 66,
      decoration: BoxDecoration(color: AppTheme.celeste100, borderRadius: BorderRadius.circular(14), border: Border.all(color: AppTheme.outline)),
      child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
        Icon(icon, color: AppTheme.blue600),
        Text(label, style: const TextStyle(fontSize: 11)),
      ]),
    ),
  );

  @override
  Widget build(BuildContext context) => ListenableBuilder(
    listenable: widget.service,
    builder: (context, _) => Card(child: Padding(
      padding: const EdgeInsets.all(12),
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Row(children: [
          const Icon(Icons.gamepad_outlined, color: AppTheme.blue600),
          const SizedBox(width: 8),
          const Expanded(child: Text('Control manual (P02)', style: TextStyle(fontWeight: FontWeight.bold))),
          ArmedChip(armed: armed),
        ]),
        const Divider(),
        Text('Potencia manual: ${powerPercent.round()}%', key: const Key('p02-power-label')),
        Slider(key: const Key('p02-power'), value: powerPercent, min: 10, max: 50, divisions: 8,
          label: '${powerPercent.round()}%',
          onChanged: held || arming ? null : (value) => setState(() => powerPercent = value)),
        const Text('10–50%. Se aplica en la siguiente pulsación. Sesión de 120 s; al terminar, vuelve a armar.', style: TextStyle(fontSize: 12)),
        const SizedBox(height: 10),
        if (!armed)
          FilledButton.icon(
            onPressed: arming || !canSend || !fresh || widget.service.latest['armed'] != 0 ? null : arm,
            icon: const Icon(Icons.power_settings_new),
            label: Text(arming ? 'Armando...' : 'Armar P02'),
          )
        else ...[
          const Text('Mantén presionado para mover; al soltar, potencia cero.'),
          const SizedBox(height: 10),
          Center(child: Column(children: [
            direction(Icons.arrow_upward, 'Avanzar', () => send(fx: .12)),
            const SizedBox(height: 8),
            Row(mainAxisAlignment: MainAxisAlignment.center, children: [
              direction(Icons.rotate_left, 'Izquierda', () => send(tz: -.05)),
              const SizedBox(width: 44),
              direction(Icons.rotate_right, 'Derecha', () => send(tz: .05)),
            ]),
            const SizedBox(height: 8),
            direction(Icons.arrow_downward, 'Retroceder', () => send(fx: -.12)),
            const SizedBox(height: 12),
            Row(mainAxisAlignment: MainAxisAlignment.center, children: [
              direction(Icons.keyboard_double_arrow_up, 'Subir', () => send(fz: .12)),
              const SizedBox(width: 24),
              direction(Icons.keyboard_double_arrow_down, 'Bajar', () => send(fz: -.12)),
            ]),
          ])),
        ],
        const SizedBox(height: 12),
        SizedBox(width: double.infinity, child: FilledButton.icon(onPressed: stop,
          style: FilledButton.styleFrom(backgroundColor: AppTheme.danger),
          icon: const Icon(Icons.stop), label: const Text('STOP'))),
        if (!fresh) const Text('Sin confirmación reciente de actuadores.', style: TextStyle(color: AppTheme.warning)),
        if (status.isNotEmpty) Text(status, style: const TextStyle(fontSize: 12)),
        if (widget.service.lastError != null) Text(widget.service.lastError!, style: const TextStyle(color: AppTheme.danger)),
      ]),
    )),
  );
}
