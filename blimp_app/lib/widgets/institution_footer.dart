import 'package:flutter/material.dart';
import '../core/theme/app_theme.dart';

/// Pie de pantalla con el logo de ESPOL y de la facultad (FIMCP).
///
/// Los archivos van en `assets/logos/espol_logo.png` y
/// `assets/logos/fimcp_logo.png` (cualquier PNG/JPG/SVG-rasterizado con
/// fondo transparente sirve). Mientras esos archivos no existan todavia,
/// se muestra un rotulo de texto en su lugar en vez de romper el build.
class InstitutionFooter extends StatelessWidget {
  final bool light; // true = fondo oscuro (logos en blanco), false = fondo claro
  const InstitutionFooter({super.key, this.light = false});

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(
          'Proyecto de tesis',
          style: TextStyle(
            fontSize: 11,
            letterSpacing: 0.6,
            color: light ? Colors.white70 : AppTheme.onSurfaceMuted,
          ),
        ),
        const SizedBox(height: 10),
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
          decoration: BoxDecoration(
            color: Colors.white,
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: AppTheme.outline),
            boxShadow: [
              BoxShadow(
                color: AppTheme.navy900.withValues(alpha: 0.06),
                blurRadius: 10,
                offset: const Offset(0, 3),
              ),
            ],
          ),
          // FittedBox: el logo de la FIMCP es un banner MUY ancho (aspect
          // ratio ~8:1) comparado con el isotipo cuadrado de ESPOL. Con
          // fit: scaleDown, si en algun telefono angosto no entra a esta
          // altura "de diseño", se reduce proporcionalmente en vez de
          // desbordar la tarjeta.
          child: FittedBox(
            fit: BoxFit.scaleDown,
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                _LogoSlot(
                  assetPath: 'assets/logos/espol_logo.png',
                  fallbackLabel: 'ESPOL',
                  height: 28,
                ),
                const SizedBox(width: 14),
                Container(width: 1, height: 24, color: AppTheme.outline),
                const SizedBox(width: 14),
                _LogoSlot(
                  assetPath: 'assets/logos/fimcp_logo.png',
                  fallbackLabel: 'FIMCP',
                  height: 18,
                ),
              ],
            ),
          ),
        ),
      ],
    );
  }
}

/// Los logos institucionales siempre se muestran sobre la tarjeta blanca
/// de InstitutionFooter, nunca directo sobre un fondo oscuro -- así que no
/// necesitan variante clara/oscura propia.
class _LogoSlot extends StatelessWidget {
  final String assetPath;
  final String fallbackLabel;
  final double height;
  const _LogoSlot({
    required this.assetPath,
    required this.fallbackLabel,
    required this.height,
  });

  @override
  Widget build(BuildContext context) {
    return Image.asset(
      assetPath,
      height: height,
      filterQuality: FilterQuality.high,
      errorBuilder: (context, error, stackTrace) => SizedBox(
        height: height,
        child: Center(
          child: Text(
            fallbackLabel,
            style: const TextStyle(
              fontSize: 13,
              fontWeight: FontWeight.w700,
              letterSpacing: 0.5,
              color: AppTheme.onSurfaceMuted,
            ),
          ),
        ),
      ),
    );
  }
}
