/// Un mensaje de telemetria tal como lo manda telemetry_bridge.py:
///   { "t": 173..., "flag": 1, "fields": {"height": 1.23, "yaw": 0.4, ...} }
class TelemetryMessage {
  final double t;
  final int flag;
  final Map<String, double> fields;

  TelemetryMessage({required this.t, required this.flag, required this.fields});

  factory TelemetryMessage.fromJson(Map<String, dynamic> json) {
    final rawFields = json['fields'] as Map<String, dynamic>? ?? {};
    return TelemetryMessage(
      t: (json['t'] as num).toDouble(),
      flag: json['flag'] as int,
      fields: rawFields.map(
        (key, value) => MapEntry(key, (value as num).toDouble()),
      ),
    );
  }
}