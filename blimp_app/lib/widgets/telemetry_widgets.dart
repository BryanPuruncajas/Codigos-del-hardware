import 'dart:math';
import 'dart:async';
import '../models/mission_guidance.dart';
import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import '../core/theme/app_theme.dart';
import '../services/telemetry_service.dart';

/// Redondea el paso entre lineas de grilla a un numero "lindo" (1/2/5 x
/// potencia de 10). Sin esto, fl_chart puede elegir un paso que hace que
/// dos valores distintos (p.ej. 0.5 y 1.0) redondeen a la misma etiqueta.
double _niceGridStep(double span) {
  if (span <= 0) return 1;
  final raw = span / 4;
  final mag = pow(10, (log(raw) / ln10).floor()).toDouble();
  final norm = raw / mag;
  final step = norm < 1.5 ? 1.0 : (norm < 3 ? 2.0 : (norm < 7 ? 5.0 : 10.0));
  return step * mag;
}

const Map<String, List<String>> kTelemetryGroups = {
  'Altura Flowdeck': ['flow_height_filtered', 'flow_height_vz', 'flow_control_age_ms', 'flow_height_flags'],
  'Movimiento': ['roll', 'pitch', 'vertical_velocity'],
  'Visión': ['nicla_x', 'nicla_y', 'nicla_w', 'nicla_h', 'nicla_distance', 'nicla_flag'],
  'Actuadores': ['servo1', 'servo2', 'motor1', 'motor2', 'mode'],
  'Control': ['rollrate', 'pitchrate', 'yawrate', 'fx_cmd'],
};

const Map<String, IconData> kTelemetryGroupIcons = {
  'Altura Flowdeck': Icons.height,
  'Movimiento': Icons.threed_rotation,
  'Visión': Icons.camera_alt,
  'Actuadores': Icons.settings_input_component,
  'Control': Icons.tune,
};

const List<String> kMissionStateNames = [
  'BUSCANDO', 'ACERCANDO', 'CONFIRMANDO', 'GIRANDO', 'ADQUIRIENDO', 'LISTO', 'GIRO FALLIDO',
];
const List<String> kMissionStateShort = [
  'Buscar', 'Acercar', 'Confirmar', 'Girar', 'Adquirir', 'Listo', 'Fallo',
];
const List<IconData> kMissionStateIcons = [
  Icons.travel_explore, Icons.center_focus_strong, Icons.check_circle_outline,
  Icons.rotate_right, Icons.center_focus_weak, Icons.flag, Icons.error_outline,
];

// ============================================================================
// CHIPS DE ESTADO -- reusados en Viewer y Dev. El color de "armado" y
// "desconectado" rompe a proposito la paleta azul/celeste: son las unicas
// señales de seguridad fisica de toda la interfaz.
// ============================================================================

class ConnectionChip extends StatelessWidget {
  final TelemetryService service;
  const ConnectionChip({super.key, required this.service});

  @override
  Widget build(BuildContext context) {
    final isDev = service.connected && service.role == 'dev';
    String label;
    Color color;
    IconData icon;
    if (service.demoMode) {
      label = 'Demo';
      color = AppTheme.celeste;
      icon = Icons.science_outlined;
    } else if (!service.connected) {
      label = 'Desconectado';
      color = AppTheme.neutral;
      icon = Icons.wifi_off;
    } else if (isDev) {
      label = 'Dev';
      color = AppTheme.blue600;
      icon = Icons.verified_user;
    } else {
      label = 'Viewer';
      color = AppTheme.sky300;
      icon = Icons.wifi;
    }
    return Chip(
      avatar: Icon(icon, color: Colors.white, size: 18),
      label: Text(label),
      backgroundColor: color,
      labelStyle: const TextStyle(color: Colors.white, fontWeight: FontWeight.w700),
      visualDensity: VisualDensity.compact,
    );
  }
}

class ArmedChip extends StatelessWidget {
  final bool armed;
  const ArmedChip({super.key, required this.armed});

  @override
  Widget build(BuildContext context) {
    return Chip(
      avatar: Icon(armed ? Icons.warning_amber_rounded : Icons.lock_outline,
          size: 16, color: Colors.white),
      label: Text(armed ? 'ARMADO' : 'Desarmado'),
      backgroundColor: armed ? AppTheme.danger : AppTheme.neutral,
      labelStyle: const TextStyle(color: Colors.white, fontSize: 12, fontWeight: FontWeight.w700),
      visualDensity: VisualDensity.compact,
      materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
    );
  }
}

// ============================================================================
// FILA DE ESTADISTICAS -- lectura rapida sin tener que buscar en tarjetas.
// ============================================================================

class StatTile extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;
  final String unit;
  final Color color;
  const StatTile({
    super.key,
    required this.icon,
    required this.label,
    required this.value,
    this.unit = '',
    this.color = AppTheme.blue500,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 128,
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      decoration: BoxDecoration(
        color: AppTheme.surface,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: AppTheme.outline),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(children: [
            Icon(icon, size: 16, color: color),
            const SizedBox(width: 6),
            Expanded(
              child: Text(label,
                  style: const TextStyle(fontSize: 11, color: AppTheme.onSurfaceMuted),
                  overflow: TextOverflow.ellipsis),
            ),
          ]),
          const SizedBox(height: 6),
          Text.rich(
            TextSpan(children: [
              TextSpan(
                  text: value,
                  style: const TextStyle(
                      fontSize: 20, fontWeight: FontWeight.w800, color: AppTheme.navy900)),
              if (unit.isNotEmpty)
                TextSpan(
                    text: ' $unit',
                    style: const TextStyle(fontSize: 12, color: AppTheme.onSurfaceMuted)),
            ]),
          ),
        ],
      ),
    );
  }
}

class OverviewRow extends StatelessWidget {
  final Map<String, double> latest;
  const OverviewRow({super.key, required this.latest});

  @override
  Widget build(BuildContext context) {
    final flowOnly = (latest['vertical_profile_version'] ?? 0) >= 39;
    final rangeValid = latest['flow_height_source'] == 2 && ((latest['flow_height_flags'] ?? 0).round() & 8) != 0;
    final received = latest['flow_control_received_at_ms'];
    final fresh = received != null && DateTime.now().millisecondsSinceEpoch - received < 1500;
    final height = !flowOnly || (rangeValid && fresh) ? latest['height'] : null;
    final yawDeg = latest['yaw'] != null ? latest['yaw']! * 180.0 / 3.14159265 : null;
    final battery = latest['battery'];
    final batteryPct = battery == null ? null : (((battery - 6.6) / (8.4 - 6.6)) * 100).clamp(0, 100);
    final batteryColor = batteryPct == null
        ? AppTheme.neutral
        : (batteryPct < 20 ? AppTheme.danger : (batteryPct < 40 ? AppTheme.warning : AppTheme.blue500));

    return SingleChildScrollView(
      scrollDirection: Axis.horizontal,
      child: Row(
        children: [
          StatTile(
            icon: Icons.height,
            label: flowOnly ? 'Altura Flowdeck' : 'Altura',
            value: height != null ? height.toStringAsFixed(2) : '—',
            unit: 'm',
            color: AppTheme.blue500,
          ),
          const SizedBox(width: 10),
          StatTile(
            icon: Icons.explore,
            label: 'Yaw',
            value: yawDeg != null ? yawDeg.toStringAsFixed(0) : '—',
            unit: '°',
            color: AppTheme.celeste,
          ),
          const SizedBox(width: 10),
          StatTile(
            icon: Icons.battery_charging_full,
            label: 'Batería',
            value: battery != null ? battery.toStringAsFixed(2) : '—',
            unit: 'V',
            color: batteryColor,
          ),
          const SizedBox(width: 10),
          StatTile(
            icon: Icons.speed,
            label: 'Vel. vertical',
            value: (!flowOnly || (rangeValid && fresh)) && latest['vertical_velocity'] != null
                ? latest['vertical_velocity']!.toStringAsFixed(2)
                : '—',
            unit: 'm/s',
            color: AppTheme.sky300,
          ),
        ],
      ),
    );
  }
}

// ============================================================================
// FLOWDECK -- F6 llega desde el ESP del blimp. Esta tarjeta separa los datos
// de navegacion del resto de diagnosticos para que se puedan leer en vuelo.
// ============================================================================

class FlowTelemetryPanel extends StatelessWidget {
  final Map<String, double> latest;
  final List<FlowSample> history;
  const FlowTelemetryPanel({super.key, required this.latest, required this.history});

  @override
  Widget build(BuildContext context) {
    final ageMs = latest['flow_age_ms'];
    final flags = (latest['flow_flags'] ?? 0).round();
    final hasPacket = ageMs != null && ageMs >= 0;
    final flowValid = (flags & 1) != 0;
    final rangeValid = (flags & 2) != 0;
    final receivedAt = latest['flow_received_at_ms'];
    final linkAge = receivedAt == null ? double.infinity : DateTime.now().millisecondsSinceEpoch - receivedAt;
    final fresh = hasPacket && ageMs <= 500 && flowValid && linkAge <= 1500;
    final statusColor = fresh ? AppTheme.success : (hasPacket ? AppTheme.warning : AppTheme.neutral);
    final status = fresh ? 'FRESCO' : (hasPacket ? 'VENCIDO' : 'ESPERANDO F6');
    final dx = latest['flow_dx'] ?? 0;
    final dy = latest['flow_dy'] ?? 0;
    final rangeMm = latest['flow_range_mm'];
    final quality = latest['flow_quality'];
    final source = latest['flow_height_source']?.round();
    const sourceNames = {0: 'Sin altura Flowdeck', 1: 'Perfil anterior: barometro', 2: 'Flowdeck', 3: 'Perfil anterior: respaldo'};
    final estimatorFlags = (latest['flow_height_flags'] ?? 0).round();
    final flowOnly = (latest['vertical_profile_version'] ?? 0) >= 39;
    final controlReceived = latest['flow_control_received_at_ms'];
    final controlFresh = controlReceived != null && DateTime.now().millisecondsSinceEpoch - controlReceived <= 1500;
    final heightValid = (estimatorFlags & 8) != 0 && (!flowOnly || (source == 2 && controlFresh));
    final rangeFresh = (estimatorFlags & 2) != 0 && (!flowOnly || controlFresh);
    final opticalReady = (estimatorFlags & 128) != 0 && (!flowOnly || controlFresh);
    final sourceLabel = flowOnly && !heightValid ? 'Sin altura Flowdeck' : (sourceNames[source] ?? 'Desconocido');

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(7),
                  decoration: BoxDecoration(
                    color: statusColor.withValues(alpha: 0.15),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: Icon(Icons.sensors, color: statusColor, size: 19),
                ),
                const SizedBox(width: 10),
                const Expanded(
                  child: Text('Flowdeck',
                      style: TextStyle(fontWeight: FontWeight.w800, fontSize: 17, color: AppTheme.navy900)),
                ),
                Chip(
                  avatar: Icon(fresh ? Icons.wifi : Icons.wifi_off, color: Colors.white, size: 15),
                  label: Text(status),
                  backgroundColor: statusColor,
                  labelStyle: const TextStyle(color: Colors.white, fontSize: 11, fontWeight: FontWeight.w800),
                  visualDensity: VisualDensity.compact,
                ),
              ],
            ),
            const SizedBox(height: 12),
            if (source != null) ...[
              Text('Altura: $sourceLabel',
                  style: const TextStyle(fontWeight: FontWeight.w700)),
              const SizedBox(height: 8),
              Wrap(spacing: 18, runSpacing: 12, children: [
                _FlowMetric(label: flowOnly ? 'Altura de control' : 'Altura local', value: heightValid ? (latest['flow_fused_height']?.toStringAsFixed(2) ?? '—') : '—', unit: 'm'),
                _FlowMetric(label: 'Altura al suelo', value: rangeFresh ? (latest['flow_height_filtered']?.toStringAsFixed(2) ?? '—') : '—', unit: 'm'),
                _FlowMetric(label: 'Vz Flowdeck', value: rangeFresh ? (latest['flow_height_vz']?.toStringAsFixed(3) ?? '—') : '—', unit: 'm/s'),
                _FlowMetric(label: 'Vz estimada', value: heightValid ? (latest['flow_fused_vz']?.toStringAsFixed(3) ?? '—') : '—', unit: 'm/s'),
                _FlowMetric(label: 'Flujo X', value: opticalReady ? (latest['flow_rate_x']?.toStringAsFixed(1) ?? '—') : '—', unit: 'cuentas/s'),
                _FlowMetric(label: 'Flujo Y', value: opticalReady ? (latest['flow_rate_y']?.toStringAsFixed(1) ?? '—') : '—', unit: 'cuentas/s'),
              ]),
              const SizedBox(height: 8),
              const Text('En v39 la altura y Vz provienen solo de Flowdeck. X/Y requieren calibracion para expresarse en m/s.',
                  style: TextStyle(fontSize: 12, color: AppTheme.onSurfaceMuted)),
              if ((flowOnly && !heightValid) || source == 0 || source == 3)
                const Text('Rango no disponible: comprobar enlace, inclinacion y suelo.',
                    style: TextStyle(color: AppTheme.warning)),
              const Divider(height: 24),
            ],
            if (!hasPacket)
              const Padding(
                padding: EdgeInsets.symmetric(vertical: 10),
                child: Text('Esperando telemetría F6 desde el ESP del blimp.',
                    style: TextStyle(color: AppTheme.onSurfaceMuted)),
              )
            else ...[
              Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  SizedBox(
                    width: 118,
                    height: 112,
                    child: CustomPaint(
                      painter: _FlowVectorPainter(dx: dx, dy: dy, color: statusColor),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Wrap(
                      spacing: 18,
                      runSpacing: 12,
                      children: [
                        _FlowMetric(label: 'Delta X', value: dx.toStringAsFixed(1), unit: 'cuentas'),
                        _FlowMetric(label: 'Delta Y', value: dy.toStringAsFixed(1), unit: 'cuentas'),
                        _FlowMetric(
                            label: 'Distancia',
                            value: rangeMm != null ? (rangeMm / 1000).toStringAsFixed(2) : '—',
                            unit: 'm'),
                        _FlowMetric(label: 'Calidad', value: quality == null || quality == 0 ? '—' : quality.toStringAsFixed(0), unit: ''),
                        _FlowMetric(label: 'Edad', value: ageMs.toStringAsFixed(0), unit: 'ms'),
                        _FlowMetric(
                            label: 'Sensores',
                            value: rangeValid ? 'Flow + rango' : 'Solo Flow',
                            unit: ''),
                      ],
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 10),
              const Text('Historial de movimiento',
                  style: TextStyle(fontWeight: FontWeight.w700, color: AppTheme.navy900)),
              const SizedBox(height: 6),
              SizedBox(height: 150, child: _FlowHistoryChart(history: history)),
            ],
          ],
        ),
      ),
    );
  }
}

class _FlowMetric extends StatelessWidget {
  final String label;
  final String value;
  final String unit;
  const _FlowMetric({required this.label, required this.value, required this.unit});

  @override
  Widget build(BuildContext context) => SizedBox(
        width: 88,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(label, style: const TextStyle(fontSize: 11, color: AppTheme.onSurfaceMuted)),
            const SizedBox(height: 2),
            Text(value,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(fontWeight: FontWeight.w800, fontSize: 14, color: AppTheme.navy900)),
            if (unit.isNotEmpty)
              Text(unit, style: const TextStyle(fontSize: 10, color: AppTheme.onSurfaceMuted)),
          ],
        ),
      );
}

class _FlowHistoryChart extends StatelessWidget {
  final List<FlowSample> history;
  const _FlowHistoryChart({required this.history});

  @override
  Widget build(BuildContext context) {
    if (history.length < 2) {
      return const Center(
        child: Text('Esperando muestras Flow...', style: TextStyle(color: AppTheme.onSurfaceMuted)),
      );
    }
    final samples = history.length > 60 ? history.sublist(history.length - 60) : history;
    final x0 = samples.first.t;
    final x1 = samples.last.t == x0 ? x0 + 1 : samples.last.t;
    final values = [...samples.map((s) => s.dx), ...samples.map((s) => s.dy)];
    final peak = values.map((v) => v.abs()).reduce(max).clamp(1.0, 20.0) + 0.8;

    return Column(
      children: [
        const Row(
          children: [
            _FlowLegend(color: AppTheme.blue500, label: 'dx'),
            SizedBox(width: 14),
            _FlowLegend(color: AppTheme.celeste, label: 'dy'),
          ],
        ),
        const SizedBox(height: 4),
        Expanded(
          child: LineChart(
            LineChartData(
              minX: x0,
              maxX: x1,
              minY: -peak,
              maxY: peak,
              gridData: FlGridData(show: true, drawVerticalLine: false,
                  horizontalInterval: max(1.0, peak / 2),
                  getDrawingHorizontalLine: (_) => const FlLine(color: AppTheme.outline, strokeWidth: 1)),
              titlesData: const FlTitlesData(
                topTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                rightTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                bottomTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                leftTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
              ),
              borderData: FlBorderData(show: false),
              lineTouchData: LineTouchData(enabled: false),
              lineBarsData: [
                LineChartBarData(
                  spots: samples.map((s) => FlSpot(s.t, s.dx)).toList(),
                  isCurved: true,
                  color: AppTheme.blue500,
                  barWidth: 2.5,
                  dotData: const FlDotData(show: false),
                ),
                LineChartBarData(
                  spots: samples.map((s) => FlSpot(s.t, s.dy)).toList(),
                  isCurved: true,
                  color: AppTheme.celeste,
                  barWidth: 2.5,
                  dotData: const FlDotData(show: false),
                ),
              ],
            ),
          ),
        ),
      ],
    );
  }
}

class _FlowLegend extends StatelessWidget {
  final Color color;
  final String label;
  const _FlowLegend({required this.color, required this.label});

  @override
  Widget build(BuildContext context) => Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(width: 10, height: 3, color: color),
          const SizedBox(width: 5),
          Text(label, style: const TextStyle(fontSize: 11, color: AppTheme.onSurfaceMuted)),
        ],
      );
}

class _FlowVectorPainter extends CustomPainter {
  final double dx;
  final double dy;
  final Color color;
  const _FlowVectorPainter({required this.dx, required this.dy, required this.color});

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final axis = Paint()..color = AppTheme.outline..strokeWidth = 1;
    canvas.drawLine(Offset(10, center.dy), Offset(size.width - 10, center.dy), axis);
    canvas.drawLine(Offset(center.dx, 10), Offset(center.dx, size.height - 10), axis);
    final scale = min(size.width, size.height) / 12;
    final tip = Offset(
      center.dx + dx.clamp(-5.0, 5.0) * scale,
      center.dy - dy.clamp(-5.0, 5.0) * scale,
    );
    final arrow = Paint()
      ..color = color
      ..strokeWidth = 3
      ..strokeCap = StrokeCap.round;
    canvas.drawLine(center, tip, arrow);
    final direction = atan2(tip.dy - center.dy, tip.dx - center.dx);
    final head = Path()
      ..moveTo(tip.dx, tip.dy)
      ..lineTo(tip.dx - 10 * cos(direction - 0.5), tip.dy - 10 * sin(direction - 0.5))
      ..lineTo(tip.dx - 10 * cos(direction + 0.5), tip.dy - 10 * sin(direction + 0.5))
      ..close();
    canvas.drawPath(head, Paint()..color = color);
    final style = TextStyle(fontSize: 10, color: AppTheme.onSurfaceMuted);
    final x = TextPainter(text: TextSpan(text: 'X', style: style), textDirection: TextDirection.ltr)..layout();
    final y = TextPainter(text: TextSpan(text: 'Y', style: style), textDirection: TextDirection.ltr)..layout();
    x.paint(canvas, Offset(size.width - 16, center.dy + 4));
    y.paint(canvas, Offset(center.dx + 4, 4));
  }

  @override
  bool shouldRepaint(covariant _FlowVectorPainter oldDelegate) =>
      oldDelegate.dx != dx || oldDelegate.dy != dy || oldDelegate.color != color;
}

// ============================================================================
// MISION -- globos visitados + fase actual. Es el widget "vistoso": el
// tema de toda la tesis es visitar globos, asi que se dibujan como globos
// de verdad en vez de iconos genericos.
// ============================================================================

class MissionOverviewCard extends StatelessWidget {
  final Map<String, double> latest;
  const MissionOverviewCard({super.key, required this.latest});

  @override
  Widget build(BuildContext context) {
    final stateVal = latest['mission_state'];
    final stateIdx = stateVal != null ? stateVal.toInt().clamp(0, 6) : null;
    final visited = latest['visited']?.toInt() ?? 0;
    final target = latest['target_count']?.toInt().clamp(1, 8) ?? 1;
    final elapsed = latest['elapsed_s'];

    return Container(
      decoration: BoxDecoration(
        gradient: AppTheme.heroGradient,
        borderRadius: BorderRadius.circular(20),
      ),
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(
                stateIdx != null ? kMissionStateNames[stateIdx] : 'SIN MISIÓN ACTIVA',
                style: const TextStyle(
                    fontSize: 20, fontWeight: FontWeight.w800, color: Colors.white),
              ),
              if (elapsed != null)
                Text(_formatElapsed(elapsed),
                    style: const TextStyle(
                        color: Colors.white70, fontWeight: FontWeight.w600, fontSize: 13)),
            ],
          ),
          const SizedBox(height: 4),
          Text('Contactos confirmados: $visited / $target',
              style: const TextStyle(color: Colors.white, fontSize: 14)),
          const SizedBox(height: 8),
          _motionFeedback(),
          const SizedBox(height: 16),
          BalloonTrack(visited: visited, target: target, missionState: stateIdx),
          const SizedBox(height: 18),
          MissionPhaseStepper(currentIndex: stateIdx),
        ],
      ),
    );
  }

  String _formatElapsed(double s) {
    final total = s.round();
    final m = total ~/ 60;
    final sec = total % 60;
    return '${m.toString().padLeft(2, '0')}:${sec.toString().padLeft(2, '0')}';
  }

  Widget _motionFeedback() {
    return MissionGuidanceView(latest: latest);
  }
}

class MissionGuidanceView extends StatefulWidget {
  final Map<String, double> latest;
  const MissionGuidanceView({super.key, required this.latest});
  @override
  State<MissionGuidanceView> createState() => _MissionGuidanceViewState();
}

class _MissionGuidanceViewState extends State<MissionGuidanceView> {
  final guidance = MissionGuidance();
  Timer? timer;
  double? lastReceived;
  int displayCode = 6;
  @override
  void initState() {
    super.initState();
    timer = Timer.periodic(const Duration(milliseconds: 200), (_) {
      if (mounted) setState(() {}); // also expire stale data without new packets
    });
  }
  @override
  void dispose() { timer?.cancel(); super.dispose(); }

  @override
  Widget build(BuildContext context) {
    final latest = widget.latest;
    final received = latest['mission_decision_received_ms'];
    final age = received == null ? double.infinity :
        DateTime.now().millisecondsSinceEpoch - received;
    final mode = latest['mode']?.round() ?? 0;
    final valid = age >= 0 && age < 1500 && mode >= 17 && mode <= 22;
    final reason = latest['mission_motion_reason']?.round();
    final waypoint = latest['mission_waypoint']?.round();
    final yawError = latest['mission_yaw_error_deg'];
    final bench = latest['mission_bench'] == 1;
    final armed = latest['armed'] == 1;
    if (!valid) {
      guidance.reset();
      lastReceived = null;
    } else if (reason != null && received != lastReceived) {
      displayCode = guidance.update(reason, waypoint ?? 0,
          DateTime.now().millisecondsSinceEpoch);
      lastReceived = received;
    }
    const instructions = {
      0: 'Espere · sin orden de avance',
      1: 'No avance · vuelva a encuadrar el globo',
      2: 'No avance · estabilizando altura',
      4: 'Mantenga el rumbo · buscando el siguiente globo',
      6: 'Centre el globo y mantenga el giro quieto',
      7: 'Puede acercarse lentamente',
      8: 'No avance · mantenga la posición para confirmar llegada',
      9: 'Recorrido terminado',
      10: 'Giro no completado · detenga y reinicie la prueba',
      12: 'Mantenga el rumbo · encuadre el globo completo',
      13: 'Objetivo rechazado por tamaño · encuadre el siguiente globo',
      14: 'No avance · esperando respuesta nueva de la cámara',
      15: 'No avance · altura o velocidad vertical no válidas',
    };
    String status = 'Sin diagnóstico reciente de avance (F8)';
    if (valid) {
      status = instructions[displayCode] ?? 'Espere confirmación';
      if (displayCode == 3 || displayCode == 11) {
        if (yawError == null || !yawError.isFinite) {
          status = 'No avance · esperando referencia de giro';
        } else if (yawError.abs() <= 5) {
          status = 'Detenga el giro y mantenga quieto';
        } else {
          final degrees = max(5, (yawError.abs() / 5).round() * 5);
          status = 'Gire despacio a la ${yawError > 0 ? 'izquierda' : 'derecha'} · faltan ≈$degrees°';
        }
      }
    }
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(status, style: const TextStyle(color: Colors.white, fontSize: 13)),
        if (valid && bench && latest['elapsed_s'] != null)
          Text('P12 · máximo 4 minutos · STOP automático al finalizar',
              style: const TextStyle(color: Colors.white70, fontSize: 12)),
        if (valid)
          Text([
            if (waypoint != null && waypoint > 0) 'Waypoint $waypoint',
            if (bench || !armed) 'DESARMADO · órdenes simuladas',
          ].join(' · '), style: const TextStyle(color: Colors.white70, fontSize: 12)),
        if (valid)
          const Text('Guía visual suavizada; la ESP conserva sus decisiones inmediatas.',
              style: TextStyle(color: Colors.white70, fontSize: 11)),
        if (valid)
          const Text('Sin avance solicitado no significa inmóvil ni frenado.',
              style: TextStyle(color: Colors.white70, fontSize: 11)),
      ],
    );
  }
}

enum _BalloonState { visited, current, pending }

class BalloonTrack extends StatelessWidget {
  final int visited;
  final int target;
  final int? missionState;
  const BalloonTrack({super.key, required this.visited, required this.target, this.missionState});

  @override
  Widget build(BuildContext context) {
    final done = missionState == 5 && visited >= target;
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
      children: List.generate(target, (i) {
        final state = (i < visited || done)
            ? _BalloonState.visited
            : (i == visited ? _BalloonState.current : _BalloonState.pending);
        return _Balloon(state: state);
      }),
    );
  }
}

class _Balloon extends StatelessWidget {
  final _BalloonState state;
  const _Balloon({required this.state});

  @override
  Widget build(BuildContext context) {
    final Color fill;
    final Color stroke;
    switch (state) {
      case _BalloonState.visited:
        fill = AppTheme.blue600;
        stroke = Colors.white;
        break;
      case _BalloonState.current:
        fill = AppTheme.celeste;
        stroke = Colors.white;
        break;
      case _BalloonState.pending:
        fill = Colors.white.withValues(alpha: 0.12);
        stroke = Colors.white.withValues(alpha: 0.55);
        break;
    }

    final shape = SizedBox(
      width: 34,
      height: 46,
      child: CustomPaint(
        painter: _BalloonPainter(
          fill: fill,
          stroke: stroke,
          showCheck: state == _BalloonState.visited,
        ),
      ),
    );

    return state == _BalloonState.current ? _PulsingBalloon(child: shape) : shape;
  }
}

class _PulsingBalloon extends StatefulWidget {
  final Widget child;
  const _PulsingBalloon({required this.child});

  @override
  State<_PulsingBalloon> createState() => _PulsingBalloonState();
}

class _PulsingBalloonState extends State<_PulsingBalloon> with SingleTickerProviderStateMixin {
  late final AnimationController _c =
      AnimationController(vsync: this, duration: const Duration(milliseconds: 1100))
        ..repeat(reverse: true);

  @override
  void dispose() {
    _c.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: _c,
      builder: (context, child) {
        final scale = 0.95 + _c.value * 0.14;
        return Transform.scale(scale: scale, child: child);
      },
      child: widget.child,
    );
  }
}

class _BalloonPainter extends CustomPainter {
  final Color fill;
  final Color stroke;
  final bool showCheck;
  const _BalloonPainter({required this.fill, required this.stroke, this.showCheck = false});

  @override
  void paint(Canvas canvas, Size size) {
    final w = size.width;
    final h = size.height;
    final body = Rect.fromLTWH(w * 0.04, 0, w * 0.92, h * 0.76);
    final bodyPath = Path()..addOval(body);

    canvas.drawPath(bodyPath, Paint()..color = fill..style = PaintingStyle.fill);
    canvas.drawPath(
        bodyPath, Paint()..color = stroke..style = PaintingStyle.stroke..strokeWidth = 2);

    final knot = Path()
      ..moveTo(w * 0.45, h * 0.74)
      ..lineTo(w * 0.55, h * 0.74)
      ..lineTo(w * 0.50, h * 0.83)
      ..close();
    canvas.drawPath(knot, Paint()..color = stroke);

    final stringPaint = Paint()
      ..color = stroke.withValues(alpha: 0.7)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.3;
    final stringPath = Path()
      ..moveTo(w * 0.5, h * 0.83)
      ..quadraticBezierTo(w * 0.34, h * 0.92, w * 0.5, h);
    canvas.drawPath(stringPath, stringPaint);

    if (showCheck) {
      final checkPaint = Paint()
        ..color = Colors.white
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2.6
        ..strokeCap = StrokeCap.round
        ..strokeJoin = StrokeJoin.round;
      final checkPath = Path()
        ..moveTo(w * 0.30, h * 0.38)
        ..lineTo(w * 0.44, h * 0.52)
        ..lineTo(w * 0.68, h * 0.24);
      canvas.drawPath(checkPath, checkPaint);
    }
  }

  @override
  bool shouldRepaint(covariant _BalloonPainter oldDelegate) =>
      oldDelegate.fill != fill || oldDelegate.stroke != stroke || oldDelegate.showCheck != showCheck;
}

/// Stepper horizontal de las 6 fases de BalloonMission.cpp. Muestra en que
/// parte del ciclo esta la mision ACTUAL (no acumula entre globos: cada
/// globo nuevo vuelve a BUSCANDO).
class MissionPhaseStepper extends StatelessWidget {
  final int? currentIndex;
  const MissionPhaseStepper({super.key, this.currentIndex});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: List.generate(kMissionStateShort.length, (i) {
        final isDone = currentIndex != null && i < currentIndex!;
        final isCurrent = currentIndex == i;
        final color = isCurrent
            ? AppTheme.celeste
            : (isDone ? Colors.white : Colors.white.withValues(alpha: 0.35));

        final dot = Container(
          width: isCurrent ? 26 : 20,
          height: isCurrent ? 26 : 20,
          decoration: BoxDecoration(
            color: isCurrent || isDone ? color : Colors.transparent,
            shape: BoxShape.circle,
            border: Border.all(color: color, width: 1.6),
          ),
          child: Icon(
            isDone ? Icons.check : kMissionStateIcons[i],
            size: isCurrent ? 15 : 12,
            color: isCurrent || isDone ? AppTheme.navy900 : Colors.white70,
          ),
        );

        return Expanded(
          child: Column(
            children: [
              Row(
                children: [
                  if (i > 0)
                    Expanded(
                      child: Container(
                        height: 2,
                        color: isDone || isCurrent
                            ? Colors.white.withValues(alpha: 0.8)
                            : Colors.white.withValues(alpha: 0.25),
                      ),
                    ),
                  dot,
                  if (i < kMissionStateShort.length - 1)
                    Expanded(
                      child: Container(
                        height: 2,
                        color: isDone ? Colors.white.withValues(alpha: 0.8) : Colors.white.withValues(alpha: 0.25),
                      ),
                    ),
                ],
              ),
              const SizedBox(height: 4),
              Text(
                kMissionStateShort[i],
                style: TextStyle(
                  fontSize: 9.5,
                  fontWeight: isCurrent ? FontWeight.w800 : FontWeight.w500,
                  color: isCurrent ? Colors.white : Colors.white70,
                ),
                overflow: TextOverflow.ellipsis,
              ),
            ],
          ),
        );
      }),
    );
  }
}

// ============================================================================
// GRAFICAS EN VIVO -- altura y yaw, cada una con su referencia (linea
// discontinua) sobre el mismo eje: son la misma magnitud, no dos escalas.
// ============================================================================

class ControlChart extends StatelessWidget {
  final String title;
  final IconData icon;
  final String unit;
  final List<TelemetrySample> history;
  final double Function(TelemetrySample) actualOf;
  final double Function(TelemetrySample) refOf;
  final Color actualColor;

  const ControlChart({
    super.key,
    required this.title,
    required this.icon,
    required this.unit,
    required this.history,
    required this.actualOf,
    required this.refOf,
    this.actualColor = AppTheme.blue500,
  });

  @override
  Widget build(BuildContext context) {
    final actualSpots = history.map((p) => FlSpot(p.t, actualOf(p))).toList();
    final refSpots = history.map((p) => FlSpot(p.t, refOf(p))).toList();
    final lastActual = history.isNotEmpty ? actualOf(history.last) : null;
    final lastRef = history.isNotEmpty ? refOf(history.last) : null;

    double gridStep = 1;
    int decimals = 0;
    if (history.length >= 2) {
      final ys = [...actualSpots.map((s) => s.y), ...refSpots.map((s) => s.y)];
      final minY = ys.reduce(min);
      final maxY = ys.reduce(max);
      gridStep = _niceGridStep(maxY - minY);
      decimals = gridStep < 1 ? (gridStep < 0.15 ? 2 : 1) : 0;
    }

    return Card(
      child: Padding(
        padding: const EdgeInsets.fromLTRB(14, 14, 16, 10),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(icon, size: 18, color: actualColor),
                const SizedBox(width: 8),
                Text(title,
                    style: const TextStyle(fontWeight: FontWeight.w700, color: AppTheme.navy900)),
                const Spacer(),
                if (lastActual != null)
                  Text('${lastActual.toStringAsFixed(1)} $unit',
                      style: TextStyle(
                          fontWeight: FontWeight.w800, color: actualColor, fontSize: 16)),
              ],
            ),
            const SizedBox(height: 4),
            _Legend(actualColor: actualColor, showRef: lastRef != null),
            const SizedBox(height: 6),
            SizedBox(
              height: 150,
              child: history.length < 2
                  ? const Center(
                      child: Text('Esperando datos…',
                          style: TextStyle(color: AppTheme.onSurfaceMuted, fontSize: 12)),
                    )
                  : LineChart(
                      LineChartData(
                        minX: history.first.t,
                        maxX: history.last.t,
                        gridData: FlGridData(
                          show: true,
                          drawVerticalLine: false,
                          horizontalInterval: gridStep,
                          getDrawingHorizontalLine: (_) =>
                              const FlLine(color: AppTheme.outline, strokeWidth: 1),
                        ),
                        titlesData: FlTitlesData(
                          topTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                          rightTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                          bottomTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                          leftTitles: AxisTitles(
                            sideTitles: SideTitles(
                              showTitles: true,
                              reservedSize: 34,
                              interval: gridStep,
                              getTitlesWidget: (v, meta) => Text(
                                v.toStringAsFixed(decimals),
                                style: const TextStyle(fontSize: 10, color: AppTheme.onSurfaceMuted),
                              ),
                            ),
                          ),
                        ),
                        borderData: FlBorderData(show: false),
                        lineTouchData: LineTouchData(
                          touchTooltipData: LineTouchTooltipData(
                            getTooltipColor: (_) => AppTheme.navy800,
                            getTooltipItems: (spots) => spots
                                .map((s) => LineTooltipItem(
                                    '${s.y.toStringAsFixed(2)} $unit',
                                    const TextStyle(color: Colors.white, fontSize: 11)))
                                .toList(),
                          ),
                        ),
                        lineBarsData: [
                          LineChartBarData(
                            spots: refSpots,
                            isCurved: false,
                            color: AppTheme.neutral,
                            barWidth: 1.6,
                            dashArray: [6, 4],
                            dotData: const FlDotData(show: false),
                          ),
                          LineChartBarData(
                            spots: actualSpots,
                            isCurved: true,
                            curveSmoothness: 0.15,
                            color: actualColor,
                            barWidth: 2.4,
                            dotData: const FlDotData(show: false),
                            belowBarData: BarAreaData(
                              show: true,
                              color: actualColor.withValues(alpha: 0.12),
                            ),
                          ),
                        ],
                      ),
                    ),
            ),
          ],
        ),
      ),
    );
  }
}

class _Legend extends StatelessWidget {
  final Color actualColor;
  final bool showRef;
  const _Legend({required this.actualColor, required this.showRef});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        _swatch(actualColor, solid: true),
        const SizedBox(width: 4),
        const Text('Actual', style: TextStyle(fontSize: 11, color: AppTheme.onSurfaceMuted)),
        if (showRef) ...[
          const SizedBox(width: 12),
          _swatch(AppTheme.neutral, solid: false),
          const SizedBox(width: 4),
          const Text('Referencia', style: TextStyle(fontSize: 11, color: AppTheme.onSurfaceMuted)),
        ],
      ],
    );
  }

  Widget _swatch(Color color, {required bool solid}) {
    return SizedBox(
      width: 16,
      height: 2,
      child: solid
          ? ColoredBox(color: color)
          : CustomPaint(painter: _DashPainter(color: color)),
    );
  }
}

class _DashPainter extends CustomPainter {
  final Color color;
  const _DashPainter({required this.color});

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()..color = color..strokeWidth = 2;
    const dashWidth = 4.0, gap = 3.0;
    double x = 0;
    while (x < size.width) {
      canvas.drawLine(Offset(x, size.height / 2), Offset(x + dashWidth, size.height / 2), paint);
      x += dashWidth + gap;
    }
  }

  @override
  bool shouldRepaint(covariant _DashPainter oldDelegate) => oldDelegate.color != color;
}

// ============================================================================
// TARJETA GENERICA DE CAMPOS -- para todo lo que no tiene visual dedicado.
// ============================================================================

class GroupCard extends StatelessWidget {
  final String title;
  final IconData icon;
  final List<String> fields;
  final Map<String, double> latest;

  const GroupCard({
    super.key,
    required this.title,
    required this.icon,
    required this.fields,
    required this.latest,
  });

  @override
  Widget build(BuildContext context) {
    final available = fields.where((f) => latest.containsKey(f)).toList();
    if (available.isEmpty) return const SizedBox.shrink();

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(6),
                  decoration: BoxDecoration(
                    color: AppTheme.celeste100,
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: Icon(icon, size: 16, color: AppTheme.blue600),
                ),
                const SizedBox(width: 10),
                Text(title,
                    style: const TextStyle(fontWeight: FontWeight.w700, color: AppTheme.navy900)),
              ],
            ),
            const Divider(height: 20),
            Wrap(
              spacing: 18,
              runSpacing: 10,
              children: available.map((f) {
                final value = latest[f]!;
                return SizedBox(
                  width: 130,
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(f,
                          style: const TextStyle(fontSize: 11.5, color: AppTheme.onSurfaceMuted)),
                      Text(
                        value.toStringAsFixed(value.abs() < 10 ? 3 : 1),
                        style: const TextStyle(
                            fontSize: 15, fontWeight: FontWeight.w700, color: AppTheme.navy900),
                      ),
                    ],
                  ),
                );
              }).toList(),
            ),
          ],
        ),
      ),
    );
  }
}
