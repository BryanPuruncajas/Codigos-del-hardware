import 'package:flutter/material.dart';

/// Paleta y tema de la app, centralizados aca. Cualquier cambio de color,
/// tipografia, forma de las tarjetas, etc. se edita SOLO en este archivo.
class AppTheme {
  AppTheme._(); // no instanciable, es solo un contenedor de constantes/metodos

  // ---------------------------------------------------------------------
  // PALETA
  // Azul acero apagado, inspirado en el globo del blimp. Nada de colores
  // saturados -- todo pasa por esta paleta antes de llegar a un widget.
  // ---------------------------------------------------------------------
  static const primary = Color(0xFF5B7C99);
  static const primaryContainer = Color(0xFFD9E4EC);
  static const secondary = Color(0xFF7A93A8);
  static const background = Color(0xFFF3F6F8);
  static const surfaceVariant = Color(0xFFE7EDF1);
  static const onSurface = Color(0xFF2B3A45);
  static const error = Color(0xFFB3564A);

  /// Verde salvia apagado para estados "conectado"/"ok" -- reemplaza el
  /// Colors.green de fabrica en cualquier indicador de estado.
  static const success = Color(0xFF6B9080);

  // ---------------------------------------------------------------------
  // TEMA COMPLETO
  // ---------------------------------------------------------------------
  static ThemeData get light {
    final colorScheme = ColorScheme.light(
      primary: primary,
      onPrimary: Colors.white,
      primaryContainer: primaryContainer,
      onPrimaryContainer: onSurface,
      secondary: secondary,
      onSecondary: Colors.white,
      surface: Colors.white,
      onSurface: onSurface,
      surfaceContainerHighest: surfaceVariant,
      error: error,
      onError: Colors.white,
    );

    return ThemeData(
      useMaterial3: true,
      colorScheme: colorScheme,
      scaffoldBackgroundColor: background,
      appBarTheme: AppBarTheme(
        backgroundColor: primary,
        foregroundColor: Colors.white,
        elevation: 0,
      ),
      cardTheme: CardThemeData(
        elevation: 0,
        color: Colors.white,
        surfaceTintColor: Colors.transparent,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
          side: BorderSide(color: surfaceVariant, width: 1),
        ),
        margin: const EdgeInsets.only(bottom: 12),
      ),
      dividerTheme: DividerThemeData(color: surfaceVariant, thickness: 1),
      textTheme: const TextTheme(
        bodyMedium: TextStyle(color: onSurface),
      ),
    );
  }
}