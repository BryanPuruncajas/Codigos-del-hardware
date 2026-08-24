import 'package:flutter/material.dart';
import '../core/theme/app_theme.dart';
import '../services/telemetry_service.dart';
import '../widgets/telemetry_widgets.dart';

class ViewerScreen extends StatefulWidget {
  const ViewerScreen({super.key});

  @override
  State<ViewerScreen> createState() => _ViewerScreenState();
}

class _ViewerScreenState extends State<ViewerScreen> {
  final TelemetryService service = TelemetryService();
  final TextEditingController ipController = TextEditingController();

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
        title: const Text('Conectar (solo lectura)'),
        content: TextField(
          controller: ipController,
          decoration: const InputDecoration(labelText: 'IP de la laptop'),
          keyboardType: TextInputType.number,
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context), child: const Text('Cancelar')),
          FilledButton(
            onPressed: () {
              service.connect(ipController.text.trim());
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
        title: const Text('Blimp Monitor'),
        actions: [
          ListenableBuilder(
            listenable: service,
            builder: (context, _) => Padding(
              padding: const EdgeInsets.symmetric(horizontal: 8),
              child: Chip(
                avatar: Icon(service.connected ? Icons.wifi : Icons.wifi_off,
                    color: Colors.white, size: 18),
                label: Text(service.connected ? 'Conectado' : 'Desconectado'),
                backgroundColor:
                    service.connected ? AppTheme.success : Theme.of(context).colorScheme.error,
                labelStyle: const TextStyle(color: Colors.white),
              ),
            ),
          ),
          IconButton(
              icon: const Icon(Icons.settings_ethernet),
              onPressed: _showConnectDialog,
              tooltip: 'Configurar conexión'),
        ],
      ),
      body: ListenableBuilder(
        listenable: service,
        builder: (context, _) {
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