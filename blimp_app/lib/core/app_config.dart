/// Configuracion fija de la app. El token de dev vive aca, no se tipea
/// cada vez -- tiene que coincidir EXACTO con el --dev-token que le pasas
/// a telemetry_bridge.py al arrancarlo.
class AppConfig {
  AppConfig._();

  static const devToken = 'changeme'; // coincide con el default del bridge
}