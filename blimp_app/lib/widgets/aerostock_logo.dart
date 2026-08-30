import 'package:flutter/material.dart';
import '../core/theme/app_theme.dart';

/// Marca de AeroStock: no es un blimp generico de stock-art, es la silueta
/// del vehiculo real de la tesis -- globo + gondola + los dos motores
/// brushless gemelos (ver README, seccion de hardware).
class AeroStockMark extends StatelessWidget {
  final double size;
  final Color envelopeColor;
  final Color accentColor;
  const AeroStockMark({
    super.key,
    this.size = 40,
    this.envelopeColor = AppTheme.blue600,
    this.accentColor = AppTheme.celeste,
  });

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: size,
      height: size * 0.66,
      child: CustomPaint(
        painter: _AeroStockPainter(envelopeColor: envelopeColor, accentColor: accentColor),
      ),
    );
  }
}

class _AeroStockPainter extends CustomPainter {
  final Color envelopeColor;
  final Color accentColor;
  const _AeroStockPainter({required this.envelopeColor, required this.accentColor});

  @override
  void paint(Canvas canvas, Size size) {
    final w = size.width;
    final h = size.height;

    // Globo (envelope): capsula horizontal, achatada como el bicoptero real.
    final envelope = RRect.fromRectAndRadius(
      Rect.fromLTWH(w * 0.02, h * 0.06, w * 0.80, h * 0.50),
      Radius.circular(h * 0.26),
    );
    canvas.drawRRect(envelope, Paint()..color = envelopeColor);

    // Brillo superior sutil, para no dejarlo plano.
    final highlight = RRect.fromRectAndRadius(
      Rect.fromLTWH(w * 0.10, h * 0.10, w * 0.40, h * 0.14),
      Radius.circular(h * 0.08),
    );
    canvas.drawRRect(highlight, Paint()..color = Colors.white.withValues(alpha: 0.22));

    // Aleta de cola.
    final fin = Path()
      ..moveTo(w * 0.80, h * 0.12)
      ..lineTo(w * 0.96, h * 0.20)
      ..lineTo(w * 0.80, h * 0.42)
      ..close();
    canvas.drawPath(fin, Paint()..color = envelopeColor);

    // Tirantes hacia la gondola.
    final strutPaint = Paint()
      ..color = AppTheme.navy900
      ..strokeWidth = w * 0.018
      ..strokeCap = StrokeCap.round;
    canvas.drawLine(Offset(w * 0.20, h * 0.55), Offset(w * 0.22, h * 0.72), strutPaint);
    canvas.drawLine(Offset(w * 0.62, h * 0.55), Offset(w * 0.60, h * 0.72), strutPaint);

    // Gondola.
    final gondola = RRect.fromRectAndRadius(
      Rect.fromLTWH(w * 0.14, h * 0.72, w * 0.54, h * 0.14),
      Radius.circular(h * 0.06),
    );
    canvas.drawRRect(gondola, Paint()..color = AppTheme.navy900);

    // Los dos motores brushless gemelos -- la firma del bicoptero.
    _motor(canvas, Offset(w * 0.14, h * 0.79), h * 0.11, accentColor);
    _motor(canvas, Offset(w * 0.68, h * 0.79), h * 0.11, accentColor);
  }

  void _motor(Canvas canvas, Offset center, double r, Color color) {
    canvas.drawCircle(center, r, Paint()..color = color);
    final bladePaint = Paint()
      ..color = Colors.white
      ..strokeWidth = r * 0.22
      ..strokeCap = StrokeCap.round;
    canvas.drawLine(center.translate(-r * 0.5, 0), center.translate(r * 0.5, 0), bladePaint);
    canvas.drawLine(center.translate(0, -r * 0.5), center.translate(0, r * 0.5), bladePaint);
  }

  @override
  bool shouldRepaint(covariant _AeroStockPainter oldDelegate) =>
      oldDelegate.envelopeColor != envelopeColor || oldDelegate.accentColor != accentColor;
}

/// Isotipo + wordmark bicolor, para el header del home y las pantallas
/// principales.
class AeroStockWordmark extends StatelessWidget {
  final double markSize;
  final double fontSize;
  final Color aeroColor;
  final Color stockColor;
  const AeroStockWordmark({
    super.key,
    this.markSize = 44,
    this.fontSize = 26,
    this.aeroColor = Colors.white,
    this.stockColor = AppTheme.celeste,
  });

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        AeroStockMark(size: markSize, envelopeColor: aeroColor == Colors.white ? Colors.white.withValues(alpha: 0.92) : AppTheme.blue600),
        SizedBox(width: markSize * 0.18),
        Text.rich(
          TextSpan(children: [
            TextSpan(
              text: 'Aero',
              style: TextStyle(
                  color: aeroColor, fontSize: fontSize, fontWeight: FontWeight.w800, letterSpacing: -0.5),
            ),
            TextSpan(
              text: 'Stock',
              style: TextStyle(
                  color: stockColor, fontSize: fontSize, fontWeight: FontWeight.w800, letterSpacing: -0.5),
            ),
          ]),
        ),
      ],
    );
  }
}
