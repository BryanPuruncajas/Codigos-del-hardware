import 'package:flutter/material.dart';
import '../core/theme/app_theme.dart';
import '../widgets/aerostock_logo.dart';
import '../widgets/institution_footer.dart';
import 'viewer_screen.dart';
import 'dev_screen.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppTheme.background,
      body: SafeArea(
        child: Column(
          children: [
            Container(
              width: double.infinity,
              padding: const EdgeInsets.fromLTRB(24, 40, 24, 36),
              decoration: const BoxDecoration(
                gradient: AppTheme.heroGradient,
                borderRadius: BorderRadius.only(
                  bottomLeft: Radius.circular(32),
                  bottomRight: Radius.circular(32),
                ),
              ),
              child: Column(
                children: [
                  const AeroStockWordmark(markSize: 52, fontSize: 30),
                  const SizedBox(height: 8),
                  Text(
                    'Estación de tierra del blimp de tesis',
                    style: TextStyle(color: Colors.white.withValues(alpha: 0.85), fontSize: 13),
                  ),
                ],
              ),
            ),
            Expanded(
              // SingleChildScrollView: en un viewport bajo (celular en
              // horizontal, ventanas chicas) el contenido de abajo no
              // entraba y desbordaba en vez de scrollear.
              child: SingleChildScrollView(
                padding: const EdgeInsets.fromLTRB(24, 28, 24, 24),
                child: Column(
                  children: [
                    _HomeCard(
                      icon: Icons.visibility,
                      iconColor: AppTheme.blue500,
                      title: 'Ver telemetría',
                      subtitle: 'Solo lectura: altura, yaw, batería, misión',
                      onTap: () => Navigator.push(
                        context,
                        MaterialPageRoute(builder: (_) => const ViewerScreen()),
                      ),
                    ),
                    const SizedBox(height: 14),
                    _HomeCard(
                      icon: Icons.tune,
                      iconColor: AppTheme.celeste,
                      title: 'Modo desarrollador',
                      subtitle: 'Armar, lanzar misiones y ajustar ganancias',
                      onTap: () => Navigator.push(
                        context,
                        MaterialPageRoute(builder: (_) => const DevScreen()),
                      ),
                    ),
                    const SizedBox(height: 28),
                    const Divider(),
                    const SizedBox(height: 8),
                    _HomeCard(
                      icon: Icons.science_outlined,
                      iconColor: AppTheme.sky300,
                      title: 'Vista previa (demo)',
                      subtitle: 'Recorre toda la interfaz con datos simulados, sin conectar nada',
                      onTap: () => Navigator.push(
                        context,
                        MaterialPageRoute(builder: (_) => const ViewerScreen(startInDemo: true)),
                      ),
                    ),
                    const SizedBox(height: 28),
                    const InstitutionFooter(),
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

class _HomeCard extends StatelessWidget {
  final IconData icon;
  final Color iconColor;
  final String title;
  final String subtitle;
  final VoidCallback onTap;

  const _HomeCard({
    required this.icon,
    required this.iconColor,
    required this.title,
    required this.subtitle,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return Material(
      color: AppTheme.surface,
      borderRadius: BorderRadius.circular(18),
      child: InkWell(
        borderRadius: BorderRadius.circular(18),
        onTap: onTap,
        child: Container(
          padding: const EdgeInsets.all(16),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(18),
            border: Border.all(color: AppTheme.outline),
          ),
          child: Row(
            children: [
              Container(
                width: 46,
                height: 46,
                decoration: BoxDecoration(
                  color: iconColor.withValues(alpha: 0.12),
                  borderRadius: BorderRadius.circular(14),
                ),
                child: Icon(icon, color: iconColor, size: 24),
              ),
              const SizedBox(width: 14),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(title,
                        style: const TextStyle(
                            fontWeight: FontWeight.w700, fontSize: 15.5, color: AppTheme.navy900)),
                    const SizedBox(height: 2),
                    Text(subtitle,
                        style: const TextStyle(fontSize: 12.5, color: AppTheme.onSurfaceMuted)),
                  ],
                ),
              ),
              const Icon(Icons.chevron_right, color: AppTheme.onSurfaceMuted),
            ],
          ),
        ),
      ),
    );
  }
}
