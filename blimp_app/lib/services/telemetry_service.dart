import 'dart:async';
import 'dart:convert';
import 'dart:math';
import 'package:flutter/foundation.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import '../models/telemetry_message.dart';

/// Un punto de la linea de tiempo combinada que usan los graficos.
///
/// Se agrega uno con CADA mensaje de telemetria (de cualquier flag),
/// llevando hacia adelante el ultimo valor conocido de los campos que no
/// vinieron en ese paquete puntual. height/height_ref viajan en flags
/// distintos (F1 y F4) a ritmos independientes; sin esto, graficar ambos
/// contra el mismo eje de tiempo los dejaria desalineados.
class TelemetrySample {
  final double t;
  final double height;
  final double heightRef;
  final double yawDeg;
  final double yawRefDeg;
  const TelemetrySample(this.t, this.height, this.heightRef, this.yawDeg, this.yawRefDeg);
}

class TelemetryService extends ChangeNotifier {
  WebSocketChannel? _channel;

  final Map<String, double> latest = {};
  final List<TelemetryMessage> log = [];
  final List<TelemetrySample> history = [];
  DateTime? _historyStart;

  bool connected = false;
  String? lastError;

  /// 'viewer' o 'dev' -- lo que el bridge confirmo tras el auth (o lo que
  /// asume el modo demo, que siempre se comporta como 'dev').
  String role = 'viewer';

  /// true cuando la pantalla muestra datos SIMULADOS (sin bridge ni
  /// hardware real). Se usa para no intentar escribir en un socket que no
  /// existe y para que la UI avise que lo que se ve no es un vuelo real.
  bool demoMode = false;
  Timer? _demoTimer;
  final Random _rng = Random();

  void connect(String host, {int port = 8765, String? devToken}) {
    stopDemo();
    _closeChannel();
    lastError = null;
    _resetHistory();

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

  void _resetHistory() {
    history.clear();
    log.clear();
    latest.clear();
    _historyStart = null;
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
      _applyTelemetry(json);
    }
  }

  /// Unico camino que tocan tanto el WebSocket real como el generador del
  /// modo demo -- la UI se comporta identico venga de donde venga el dato.
  void _applyTelemetry(Map<String, dynamic> json) {
    final msg = TelemetryMessage.fromJson(json);
    latest.addAll(msg.fields);

    _historyStart ??= DateTime.now();
    final elapsed = DateTime.now().difference(_historyStart!).inMilliseconds / 1000.0;

    history.add(TelemetrySample(
      elapsed,
      latest['height'] ?? 0,
      latest['height_ref'] ?? 0,
      (latest['yaw'] ?? 0) * 180.0 / pi,
      (latest['yaw_ref'] ?? 0) * 180.0 / pi,
    ));
    if (history.length > 400) history.removeAt(0);

    log.insert(0, msg);
    if (log.length > 200) log.removeLast();

    notifyListeners();
  }

  // -------------------------------------------------------------------
  // COMANDOS -- el bridge los ignora silenciosamente si tu rol es viewer.
  // En modo demo no hay bridge del otro lado: el envio simplemente no
  // hace nada (no hay hardware que mover), la simulacion sigue su curso.
  // -------------------------------------------------------------------

  void _send(Map<String, dynamic> payload) {
    if (demoMode) return;
    _channel?.sink.add(jsonEncode(payload));
  }

  void arm() => _send({'cmd': 'arm'});
  void disarm() => _send({'cmd': 'disarm'});
  void stop() => _send({'cmd': 'stop'});

  /// Control de bajo nivel (fx/fz/tx/tz/aux crudos, indices de ControlInput
  /// directos). Casi nunca lo necesitas: para P03-P11 usa startMission() /
  /// updateMission(), que arman el paquete del lado del bridge con el mismo
  /// codigo que usa run_test.py (ver common/control_pack.py) en vez de
  /// duplicar aca el empaquetado de bits ALT_PACK/YAW_PACK.
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

  /// Arranca una prueba/mision actuada (p03..p11, mismo nombre que el
  /// argumento `test` de run_test.py) mandando UN SOLO comando de alto
  /// nivel. `params` usa los mismos nombres que los --flags de run_test.py
  /// sin guiones (p.ej. {'height': 0.7, 'kp': 0.30, 'vis_min_power': 6});
  /// lo que no mandes usa el mismo default que la CLI.
  ///
  /// El bridge hace la secuencia completa (arm=1, espera 4.2s a que arme
  /// el ESC, reset=1) del lado del servidor -- por eso este metodo no
  /// necesita esperar nada por su cuenta. onStatus es solo para que la UI
  /// muestre progreso mientras tanto.
  Future<void> startMission({
    required String test,
    required Map<String, dynamic> params,
    void Function(String status)? onStatus,
  }) async {
    onStatus?.call(demoMode
        ? 'Modo demo: no hay hardware conectado, esto no hace nada'
        : 'Armando (el ESC tarda ~4.2s en pitar)...');
    _send({'cmd': 'mission_start', 'test': test, ...params});
    if (!demoMode) {
      await Future.delayed(const Duration(milliseconds: 4300));
      onStatus?.call('Misión $test corriendo');
    }
  }

  /// Actualiza ganancias/referencias de una mision YA armada, sin repetir
  /// la secuencia de armado del ESC (reset=1 solamente). Mismos `params`
  /// que startMission().
  void updateMission({required String test, required Map<String, dynamic> params}) {
    _send({'cmd': 'mission_update', 'test': test, ...params});
  }

  // -------------------------------------------------------------------
  // BANCO DE PRUEBAS -- P00 (servos) y P00M (motores). Brushless real:
  // pedile confirmacion al usuario ANTES de llamar armMotors()/setMotorPower(),
  // esta clase no la pide por si sola.
  // -------------------------------------------------------------------

  /// P00: si servo es '1' o '2', mueve SOLO ese servo (el otro queda
  /// detach); si es 'both', mueve los DOS a la vez con SUS PROPIOS angulos
  /// -- angle1/angle2 casi nunca deben ser iguales, porque los servos van
  /// en espejo: el vector vertical real es 35/95, no 35/35.
  void setServo({required String servo, required double angle1, required double angle2}) {
    _send({'cmd': 'servo_set', 'servo': servo, 'angle1': angle1, 'angle2': angle2});
  }

  /// P00M paso 1: posiciona ambos servos con brushless BLOQUEADOS y ejecuta
  /// la secuencia de ARM del firmware viejo. Tarda ~5s en volver porque el
  /// bridge espera de verdad a que termine esa secuencia -- no llames a
  /// setMotorPower() antes de que este Future termine, no va a hacer nada
  /// (los actuadores todavia estan desarmados).
  Future<void> armMotors({
    required double servo1,
    required double servo2,
    void Function(String status)? onStatus,
  }) async {
    onStatus?.call(demoMode
        ? 'Modo demo: no hay hardware conectado, esto no hace nada'
        : 'Posicionando servos y armando ESC (~5s)...');
    _send({'cmd': 'motor_arm', 'servo1': servo1, 'servo2': servo2});
    if (!demoMode) {
      await Future.delayed(const Duration(milliseconds: 5200));
      onStatus?.call('ESC armado -- listo para mandar potencia');
    }
  }

  /// P00M paso 2: potencia (0..100%) sobre los servos ya armados. Hay que
  /// mandar los MISMOS servo1/servo2 que se usaron en armMotors() -- el
  /// firmware no los recuerda entre paquetes de este modo.
  void setMotorPower({
    required String motor,
    required double power,
    required double servo1,
    required double servo2,
  }) {
    _send({
      'cmd': 'motor_power',
      'motor': motor,
      'power': power,
      'servo1': servo1,
      'servo2': servo2,
    });
  }

  // -------------------------------------------------------------------
  // MODO DEMO -- recorre una mision simulada (P08: un globo, repetida en
  // bucle) para poder revisar TODA la interfaz -- graficas, globos
  // visitados, estados -- sin la base station ni el blimp conectados.
  // -------------------------------------------------------------------

  void startDemo() {
    _closeChannel();
    _resetHistory();
    demoMode = true;
    connected = true;
    role = 'dev';
    lastError = null;
    notifyListeners();

    double simT = 0;
    double height = 0.05;
    double heightRef = 0.7;
    double yawDeg = 0;
    double yawRefDeg = 0;
    double battery = 8.2; // 2S nominal, como en la telemetria real
    int missionState = 0; // SEARCH
    int visited = 0;
    const targetCount = 4;
    double phaseTimer = 0;

    _demoTimer = Timer.periodic(const Duration(milliseconds: 100), (_) {
      simT += 0.1;
      phaseTimer += 0.1;

      // Altura: converge a heightRef como un lazo real asentando, con
      // ruido de sensor pequeño.
      height += (heightRef - height) * 0.10 + (_rng.nextDouble() - 0.5) * 0.01;

      // Yaw: gira hacia la referencia de la fase actual (error envuelto a
      // +/-180 grados, igual que wrapPi() en el firmware real).
      final yawErr = _wrapDeg(yawRefDeg - yawDeg);
      yawDeg = _wrapDeg(yawDeg + yawErr * 0.06 + (_rng.nextDouble() - 0.5) * 0.4);

      // Misma maquina de estados que BalloonMission.cpp (SEARCH ->
      // APPROACH -> VISIT_CONFIRM -> ESCAPE -> WAIT_TARGET_LOST -> ...)
      // para que el stepper y los globos avancen de forma creible.
      switch (missionState) {
        case 0: // SEARCH: gira buscando
          yawRefDeg = _wrapDeg(yawRefDeg + 7);
          if (phaseTimer > 3) {
            missionState = 1;
            phaseTimer = 0;
          }
          break;
        case 1: // APPROACH: centra y avanza
          if (phaseTimer > 4) {
            missionState = 2;
            phaseTimer = 0;
          }
          break;
        case 2: // VISIT_CONFIRM
          if (phaseTimer > 1.2) {
            visited += 1;
            missionState = 3;
            phaseTimer = 0;
          }
          break;
        case 3: // ESCAPE
          heightRef = 0.6 + _rng.nextDouble() * 0.3;
          if (phaseTimer > 1.8) {
            missionState = 4;
            phaseTimer = 0;
          }
          break;
        case 4: // WAIT_TARGET_LOST
          if (phaseTimer > 1.5) {
            missionState = visited >= targetCount ? 5 : 0;
            phaseTimer = 0;
          }
          break;
        case 5: // DONE: pausa y reinicia el reel de demo
          if (phaseTimer > 4) {
            visited = 0;
            missionState = 0;
            phaseTimer = 0;
          }
          break;
      }

      final niclaVisible = missionState == 1 || missionState == 2;
      battery = max(6.6, battery - 0.00004);

      _applyTelemetry({'t': simT, 'flag': 1, 'fields': {
        'height': height,
        'yaw': yawDeg * pi / 180.0,
        'roll': (_rng.nextDouble() - 0.5) * 2,
        'pitch': (_rng.nextDouble() - 0.5) * 2,
        'battery': battery,
        'vertical_velocity': (heightRef - height) * 0.5,
      }});
      _applyTelemetry({'t': simT, 'flag': 2, 'fields': {
        'nicla_flag': (niclaVisible ? 65 : 64).toDouble(),
        'nicla_x': niclaVisible ? 120 + (_rng.nextDouble() - 0.5) * 30 : 0,
        'nicla_y': 80.0,
        'nicla_w': missionState == 2 ? 62.0 : (niclaVisible ? 32.0 : 0.0),
        'nicla_h': niclaVisible ? 40.0 : 0.0,
        'nicla_distance': 9999.0,
      }});
      _applyTelemetry({'t': simT, 'flag': 3, 'fields': {
        'servo1': 35.0, 'servo2': 95.0, 'motor1': 8.0, 'motor2': 8.0,
        'armed': 1.0, 'mode': 17.0,
      }});
      _applyTelemetry({'t': simT, 'flag': 4, 'fields': {
        'rollrate': 0.0, 'pitchrate': 0.0, 'yawrate': yawErr * 0.06,
        'yaw_ref': yawRefDeg * pi / 180.0, 'height_ref': heightRef,
        'fx_cmd': (yawErr / 90.0).clamp(-1.0, 1.0),
      }});
      _applyTelemetry({'t': simT, 'flag': 5, 'fields': {
        'mission_state': missionState.toDouble(),
        'visited': visited.toDouble(),
        'target_count': targetCount.toDouble(),
        'search_height': heightRef,
        'visit_score': 0.0,
        'elapsed_s': simT,
      }});
    });
  }

  void stopDemo() {
    _demoTimer?.cancel();
    _demoTimer = null;
    if (demoMode) {
      demoMode = false;
      connected = false;
      _resetHistory();
      notifyListeners();
    }
  }

  /// Envuelve un angulo en grados a (-180, 180], igual que wrapPi() en el
  /// firmware real (ver README, seccion de convencion de yaw).
  double _wrapDeg(double deg) {
    var d = deg % 360.0;
    if (d > 180.0) d -= 360.0;
    if (d <= -180.0) d += 360.0;
    return d;
  }

  void _closeChannel() {
    _channel?.sink.close();
    _channel = null;
  }

  void disconnect() {
    stopDemo();
    _closeChannel();
    connected = false;
  }

  @override
  void dispose() {
    stopDemo();
    _closeChannel();
    super.dispose();
  }
}
