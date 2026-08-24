import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import '../models/telemetry_message.dart';

class TelemetryService extends ChangeNotifier {
  WebSocketChannel? _channel;

  final Map<String, double> latest = {};
  final List<TelemetryMessage> log = [];
  final List<FlSpotLike> heightHistory = [];
  DateTime? _historyStart;

  bool connected = false;
  String? lastError;

  /// 'viewer' o 'dev' -- lo que el bridge confirmo tras el auth.
  String role = 'viewer';

  void connect(String host, {int port = 8765, String? devToken}) {
    disconnect();
    lastError = null;
    heightHistory.clear();
    _historyStart = null;

    try {
      _channel = WebSocketChannel.connect(Uri.parse('ws://$host:$port'));
    } catch (e) {
      lastError = 'No se pudo conectar: $e';
      notifyListeners();
      return;
    }

    final auth = devToken != null && devToken.isNotEmpty
        ? {'role': 'dev', 'token': devToken}
        : {'role': 'viewer'};
    _channel!.sink.add(jsonEncode({'auth': auth}));

    _channel!.stream.listen(
      _onMessage,
      onError: (e) {
        lastError = 'Error de conexion: $e';
        connected = false;
        notifyListeners();
      },
      onDone: () {
        connected = false;
        notifyListeners();
      },
    );
  }

  void _onMessage(dynamic raw) {
    Map<String, dynamic> json;
    try {
      json = jsonDecode(raw as String) as Map<String, dynamic>;
    } catch (_) {
      return;
    }

    if (json.containsKey('auth_ok')) {
      connected = json['auth_ok'] == true;
      role = json['role'] as String? ?? 'viewer';
      notifyListeners();
      return;
    }

    // Respuesta a un comando (solo relevante en modo dev).
    if (json.containsKey('cmd_ok')) {
      if (json['cmd_ok'] != true) {
        lastError = 'Comando "${json['cmd']}" fallo: ${json['error']}';
        notifyListeners();
      }
      return;
    }

    if (json.containsKey('flag')) {
      final msg = TelemetryMessage.fromJson(json);
      latest.addAll(msg.fields);

      if (msg.fields.containsKey('height')) {
        _historyStart ??= DateTime.now();
        final elapsed =
            DateTime.now().difference(_historyStart!).inMilliseconds / 1000.0;
        heightHistory.add(FlSpotLike(elapsed, msg.fields['height']!));
        if (heightHistory.length > 300) heightHistory.removeAt(0);
      }

      log.insert(0, msg);
      if (log.length > 200) log.removeLast();

      notifyListeners();
    }
  }

  // -------------------------------------------------------------------
  // COMANDOS -- el bridge los ignora silenciosamente si tu rol es viewer.
  // -------------------------------------------------------------------

  void _send(Map<String, dynamic> payload) {
    _channel?.sink.add(jsonEncode(payload));
  }

  void arm() => _send({'cmd': 'arm'});
  void disarm() => _send({'cmd': 'disarm'});
  void stop() => _send({'cmd': 'stop'});

  /// Control completo, igual forma que BlimpLink.control() del lado Python.
  /// aux usa los mismos indices que ControlInput.params[] del firmware
  /// (ver AppConfig.h: PARAM_AUX0..PARAM_AUX4, etc.)
  void sendControl({
    required int mode,
    double fx = 0,
    double fz = 0,
    double tx = 0,
    double tz = 0,
    int arm = 0,
    int reload = 0,
    int reset = 0,
    Map<String, double>? aux,
  }) {
    _send({
      'cmd': 'control',
      'mode': mode,
      'fx': fx,
      'fz': fz,
      'tx': tx,
      'tz': tz,
      'arm': arm,
      'reload': reload,
      'reset': reset,
      if (aux != null) 'aux': aux,
    });
  }

  /// Secuencia completa para arrancar una mision actuada (P08, P09, etc),
  /// identica a la que hace run_test.py por consola:
  ///   1) manda TODO el paquete (mode+referencias+ganancias) con arm=1
  ///   2) espera 4.2s (tiempo real que tarda el ESC en armar y pitar)
  ///   3) manda el mismo paquete con reset=1 (arranca la maquina de estados
  ///      limpia, conservando las referencias/ganancias ya enviadas)
  ///
  /// onStatus se llama en cada paso para que la UI pueda mostrar progreso.
  Future<void> startActuatedMission({
    required int mode,
    required double fz,
    required double fx,
    required double tx,
    required double tz,
    required Map<String, double> aux,
    void Function(String status)? onStatus,
  }) async {
    onStatus?.call('Armando...');
    sendControl(mode: mode, fz: fz, fx: fx, tx: tx, tz: tz, aux: aux, arm: 1);

    onStatus?.call('Esperando 4.2s a que arme el ESC...');
    await Future.delayed(const Duration(milliseconds: 4200));

    onStatus?.call('Iniciando misión...');
    sendControl(mode: mode, fz: fz, fx: fx, tx: tx, tz: tz, aux: aux, reset: 1);

    onStatus?.call('Misión corriendo');
  }

  void disconnect() {
    _channel?.sink.close();
    _channel = null;
    connected = false;
  }

  @override
  void dispose() {
    disconnect();
    super.dispose();
  }
}

class FlSpotLike {
  final double x;
  final double y;
  FlSpotLike(this.x, this.y);
}