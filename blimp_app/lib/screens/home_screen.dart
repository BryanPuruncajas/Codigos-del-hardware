import 'package:flutter/material.dart';
import '../core/theme/app_theme.dart';
import 'viewer_screen.dart';
import 'dev_screen.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Blimp Monitor')),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.air, size: 72, color: AppTheme.primary),
            const SizedBox(height: 24),
            SizedBox(
              width: 240,
              child: FilledButton.icon(
                icon: const Icon(Icons.visibility),
                label: const Text('Ver telemetría'),
                onPressed: () => Navigator.push(
                  context,
                  MaterialPageRoute(builder: (_) => const ViewerScreen()),
                ),
              ),
            ),
            const SizedBox(height: 12),
            SizedBox(
              width: 240,
              child: OutlinedButton.icon(
                icon: const Icon(Icons.tune),
                label: const Text('Modo desarrollador'),
                onPressed: () => Navigator.push(
                  context,
                  MaterialPageRoute(builder: (_) => const DevScreen()),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}