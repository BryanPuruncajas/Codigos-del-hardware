import 'package:flutter/material.dart';
import '../core/theme/app_theme.dart';
import '../services/telemetry_service.dart';
import '../widgets/telemetry_widgets.dart';

class ViewerScreen extends StatefulWidget {
  final bool startInDemo;
  const ViewerScreen({super.key, this.startInDemo = false});

  @override
  State<ViewerScreen> createState() => _ViewerScreenState();
}

class _ViewerScreenState extends State<ViewerScreen> {
  final TelemetryService service = TelemetryService();
  final TextEditingController ipController = TextEditingController();

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

  void _showAppMenu(String action) {
    if (action == 'demo') {
      service.demoMode ? service.stopDemo() : service.startDemo();
    } else if (action == 'connect') {
      _showConnectDialog();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        flexibleSpace: const DecoratedBox(decoration: BoxDecoration(gradient: AppTheme.appBarGradient)),
        title: const Text(
          'Competencia',
          overflow: TextOverflow.ellipsis,
        ),
        actions: [
          ListenableBuilder(
            listenable: service,
            builder: (context, _) => Padding(
              padding: const EdgeInsets.symmetric(horizontal: 6),
              child: ConnectionChip(service: service),
            ),
          ),
          PopupMenuButton<String>(
            tooltip: 'Opciones de conexión',
            icon: const Icon(Icons.more_vert),
            onSelected: _showAppMenu,
            itemBuilder: (context) => [
              PopupMenuItem(
                value: 'demo',
                child: ListTile(
                  leading: Icon(service.demoMode ? Icons.stop_circle_outlined : Icons.science_outlined),
                  title: Text(service.demoMode ? 'Detener demo' : 'Modo demostración'),
                ),
              ),
              const PopupMenuItem(
                value: 'connect',
                child: ListTile(
                  leading: Icon(Icons.settings_ethernet),
                  title: Text('Configurar conexión'),
                ),
              ),
            ],
          ),
        ],
      ),
      body: ListenableBuilder(
        listenable: service,
        builder: (context, _) {
          if (service.latest.isEmpty) {
            return Center(
              child: Padding(
                padding: const EdgeInsets.all(24),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(Icons.satellite_alt, size: 56, color: AppTheme.sky300),
                    const SizedBox(height: 16),
                    Text(
                      'Sin datos todavía.\nConectate con el ícono de red, o probá el modo demo (ícono de matraz) para ver la interfaz sin hardware.',
                      textAlign: TextAlign.center,
                      style: TextStyle(color: AppTheme.onSurfaceMuted.withValues(alpha: 0.9)),
                    ),
                  ],
                ),
              ),
            );
          }
          return ListView(
            padding: const EdgeInsets.all(12),
            children: [
              if (service.demoMode) const _DemoBanner(),
              MissionOverviewCard(latest: service.latest),
              const SizedBox(height: 12),
              OverviewRow(latest: service.latest),
              const SizedBox(height: 12),
              FlowTelemetryPanel(latest: service.latest, history: service.flowHistory),
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
              'MODO DEMO — datos simulados, sin conexión real al blimp.',
              style: TextStyle(color: AppTheme.navy900, fontWeight: FontWeight.w600, fontSize: 12.5),
            ),
          ),
        ],
      ),
    );
  }
}
