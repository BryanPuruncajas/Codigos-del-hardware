import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import '../core/theme/app_theme.dart';
import '../services/telemetry_service.dart';

const Map<String, List<String>> kTelemetryGroups = {
  'Altura y movimiento': ['height', 'vertical_velocity', 'yaw', 'roll', 'pitch'],
  'Batería': ['battery'],
  'Visión': ['nicla_x', 'nicla_y', 'nicla_w', 'nicla_h', 'nicla_distance', 'nicla_flag'],
  'Actuadores': ['servo1', 'servo2', 'motor1', 'motor2', 'armed', 'mode'],
  'Misión': ['mission_state', 'visited', 'target_count', 'search_height', 'visit_score', 'elapsed_s'],
};

const Map<String, IconData> kTelemetryGroupIcons = {
  'Altura y movimiento': Icons.height,
  'Batería': Icons.battery_full,
  'Visión': Icons.camera_alt,
  'Actuadores': Icons.settings_input_component,
  'Misión': Icons.flag,
};

const Map<int, String> kMissionStateNames = {
  0: 'SEARCH', 1: 'APPROACH', 2: 'VISIT_CONFIRM',
  3: 'ESCAPE', 4: 'WAIT_TARGET_LOST', 5: 'DONE',
};

class MissionBadge extends StatelessWidget {
  final Map<String, double> latest;
  const MissionBadge({super.key, required this.latest});

  @override
  Widget build(BuildContext context) {
    final stateVal = latest['mission_state'];
    final stateName = stateVal != null
        ? (kMissionStateNames[stateVal.toInt()] ?? 'modo ${stateVal.toInt()}')
        : '—';
    final visited = latest['visited']?.toInt() ?? 0;
    final target = latest['target_count']?.toInt() ?? 0;

    return Card(
      color: AppTheme.primaryContainer,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(stateName,
                style: const TextStyle(
                    fontSize: 22, fontWeight: FontWeight.bold, color: AppTheme.onSurface)),
            Text('Visitados: $visited / $target',
                style: const TextStyle(fontSize: 16, color: AppTheme.onSurface)),
          ],
        ),
      ),
    );
  }
}

class HeightChart extends StatelessWidget {
  final TelemetryService service;
  const HeightChart({super.key, required this.service});

  @override
  Widget build(BuildContext context) {
    final spots = service.heightHistory.map((p) => FlSpot(p.x, p.y)).toList();

    return Card(
      child: Padding(
        padding: const EdgeInsets.fromLTRB(12, 16, 16, 12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Altura en vivo',
                style: TextStyle(fontWeight: FontWeight.bold, color: AppTheme.onSurface)),
            const SizedBox(height: 8),
            SizedBox(
              height: 160,
              child: LineChart(
                LineChartData(
                  gridData: FlGridData(
                    show: true,
                    getDrawingHorizontalLine: (_) =>
                        FlLine(color: AppTheme.surfaceVariant, strokeWidth: 1),
                    getDrawingVerticalLine: (_) =>
                        FlLine(color: AppTheme.surfaceVariant, strokeWidth: 1),
                  ),
                  titlesData: const FlTitlesData(
                    topTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                    rightTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                  ),
                  borderData: FlBorderData(
                      show: true, border: Border.all(color: AppTheme.surfaceVariant)),
                  lineBarsData: [
                    LineChartBarData(
                      spots: spots,
                      isCurved: true,
                      color: AppTheme.primary,
                      barWidth: 2,
                      dotData: const FlDotData(show: false),
                      belowBarData: BarAreaData(
                        show: true,
                        color: AppTheme.primary.withValues(alpha: 0.1),
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
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(icon, size: 20, color: AppTheme.primary),
                const SizedBox(width: 8),
                Text(title,
                    style: const TextStyle(fontWeight: FontWeight.bold, color: AppTheme.onSurface)),
              ],
            ),
            const Divider(),
            Wrap(
              spacing: 16,
              runSpacing: 8,
              children: available.map((f) {
                final value = latest[f]!;
                return SizedBox(
                  width: 140,
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(f,
                          style: TextStyle(
                              fontSize: 12, color: AppTheme.onSurface.withValues(alpha: 0.6))),
                      Text(
                        value.toStringAsFixed(value.abs() < 10 ? 3 : 1),
                        style: const TextStyle(
                            fontSize: 16, fontWeight: FontWeight.w600, color: AppTheme.onSurface),
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