import 'package:flutter/material.dart';

/// Paleta y tema de la app, centralizados aca. Cualquier cambio de color,
/// tipografia, forma de las tarjetas, etc. se edita SOLO en este archivo.
///
/// PALETA: espectro azul -> celeste, de mas oscuro a mas claro. Se usa en
/// TODO (fondos, texto, botones, graficas, iconos de estado) salvo tres
/// excepciones deliberadas que rompen la paleta a proposito porque son
/// seguridad fisica, no branding:
///   - ARMADO (motores/helices con potencia real)
///   - STOP de emergencia
///   - bateria critica
/// Esos tres usan ambar/rojo porque son el color que cualquier persona
/// reconoce como "peligro" sin tener que leer texto -- mantenerlos en la
/// paleta azul les quitaria esa señal instantanea.
class AppTheme {
  AppTheme._(); // no instanciable, es solo un contenedor de constantes/metodos

  // ---------------------------------------------------------------------
  // RAMPA AZUL -> CELESTE (oscuro a claro)
  // Ancladas en los dos azules reales de ESPOL/FIMCP: navy900 ~ el navy del
  // isotipo ESPOL, blue600 ~ el azul del lockup de la FIMCP. El resto de la
  // rampa (mas clara) se extiende desde ahi para el resto de la interfaz.
  // ---------------------------------------------------------------------
  static const navy900 = Color(0xFF1B2A55); // texto de alto enfasis, fondos hero
  static const navy800 = Color(0xFF1F3A6E); // app bar / superficies oscuras
  static const blue700 = Color(0xFF1A4A82); // contenedores oscuros, sombras de marca
  static const blue600 = Color(0xFF1568B5); // marca principal (azul FIMCP), botones llenos
  static const blue500 = Color(0xFF1E7BC4); // primario interactivo (mas usado)
  static const blue400 = Color(0xFF3E93D6); // hover / acentos secundarios
  static const sky300 = Color(0xFF6FC0E8); // apoyo, bordes activos
  static const celeste = Color(0xFF1FAEDB); // acento celeste saturado: "activo/ahora"
  static const celeste200 = Color(0xFFAEE0F5); // contenedores celestes claros
  static const celeste100 = Color(0xFFD8EFFA); // tinte muy claro (chips, fondos suaves)
  static const background = Color(0xFFF1F8FC); // fondo general de la app
  static const surface = Color(0xFFFFFFFF); // tarjetas
  static const outline = Color(0xFFD3E4EE); // bordes / divisores

  static const onSurface = navy900; // texto principal
  static const onSurfaceMuted = Color(0xFF4C6072); // texto secundario

  // ---------------------------------------------------------------------
  // COLORES DE ESTADO (reservados; nunca se reusan como color de serie)
  // ---------------------------------------------------------------------
  /// Peligro fisico real: motores armados, STOP, desconexion de comando.
  static const danger = Color(0xFFC1443A);
  /// Advertencia: bateria baja, token invalido, avisos no criticos.
  static const warning = Color(0xFFD98E2B);
  /// Neutro/inactivo: pasos de mision aun no alcanzados, "sin dato".
  static const neutral = Color(0xFF9AAAB8);

  /// Se mantiene por compatibilidad semantica ("todo ok"); en esta paleta
  /// "ok" se expresa con celeste/azul, no con verde.
  static const success = celeste;

  // ---------------------------------------------------------------------
  // GRADIENTES
  // ---------------------------------------------------------------------
  static const heroGradient = LinearGradient(
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
    colors: [navy800, blue600, celeste],
  );

  static const appBarGradient = LinearGradient(
    begin: Alignment.centerLeft,
    end: Alignment.centerRight,
    colors: [navy800, blue600],
  );

  // ---------------------------------------------------------------------
  // TEMA COMPLETO
  // ---------------------------------------------------------------------
  static ThemeData get light {
    final colorScheme = ColorScheme.light(
      primary: blue500,
      onPrimary: Colors.white,
      primaryContainer: celeste100,
      onPrimaryContainer: navy900,
      secondary: celeste,
      onSecondary: Colors.white,
      secondaryContainer: celeste200,
      onSecondaryContainer: navy900,
      surface: surface,
      onSurface: onSurface,
      surfaceContainerHighest: celeste100,
      outline: outline,
      error: danger,
      onError: Colors.white,
    );

    return ThemeData(
      useMaterial3: true,
      colorScheme: colorScheme,
      scaffoldBackgroundColor: background,
      splashFactory: InkRipple.splashFactory,
      appBarTheme: const AppBarTheme(
        backgroundColor: navy800,
        foregroundColor: Colors.white,
        elevation: 0,
        centerTitle: false,
        titleTextStyle: TextStyle(
          color: Colors.white,
          fontSize: 20,
          fontWeight: FontWeight.w700,
        ),
      ),
      cardTheme: CardThemeData(
        elevation: 0,
        color: surface,
        surfaceTintColor: Colors.transparent,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(18),
          side: const BorderSide(color: outline, width: 1),
        ),
        margin: const EdgeInsets.only(bottom: 12),
      ),
      chipTheme: ChipThemeData(
        backgroundColor: celeste100,
        labelStyle: const TextStyle(color: navy900, fontWeight: FontWeight.w600),
        side: BorderSide.none,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(20)),
        padding: const EdgeInsets.symmetric(horizontal: 4),
      ),
      filledButtonTheme: FilledButtonThemeData(
        style: FilledButton.styleFrom(
          backgroundColor: blue600,
          foregroundColor: Colors.white,
          padding: const EdgeInsets.symmetric(vertical: 14, horizontal: 20),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(14)),
          textStyle: const TextStyle(fontWeight: FontWeight.w700),
        ),
      ),
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: OutlinedButton.styleFrom(
          foregroundColor: blue600,
          side: const BorderSide(color: sky300, width: 1.4),
          padding: const EdgeInsets.symmetric(vertical: 14, horizontal: 20),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(14)),
          textStyle: const TextStyle(fontWeight: FontWeight.w700),
        ),
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: celeste100.withValues(alpha: 0.5),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: const BorderSide(color: outline),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: const BorderSide(color: outline),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: const BorderSide(color: blue500, width: 1.6),
        ),
        labelStyle: const TextStyle(color: onSurfaceMuted),
      ),
      dividerTheme: const DividerThemeData(color: outline, thickness: 1),
      textTheme: const TextTheme(
        bodyMedium: TextStyle(color: onSurface),
        bodySmall: TextStyle(color: onSurfaceMuted),
      ),
      progressIndicatorTheme: const ProgressIndicatorThemeData(
        color: blue500,
        linearTrackColor: celeste100,
      ),
    );
  }
}
