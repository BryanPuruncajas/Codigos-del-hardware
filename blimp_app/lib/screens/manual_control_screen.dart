import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import '../core/theme/app_theme.dart';
import '../services/telemetry_service.dart';

const double _kManualFx = 0.12;
const double _kManualFz = 0.12;
const double _kManualTz = 0.05;
const int _kP02Mode = 11; // AppConfig::P02_MANUAL_CONTROL

/// Pantalla dedicada de control manual (P02): se abre en su propia ruta,
/// bloqueada en horizontal (como un mando de RC real, sostenido con los dos
/// pulgares), para que el mando y la palanca siempre esten en el mismo
/// lugar exacto -- nada de esto conviene como una tarjeta mas dentro de una
/// lista con scroll.
class ManualControlScreen extends StatefulWidget {
  final TelemetryService service;
  const ManualControlScreen({super.key, required this.service});

  @override
  State<ManualControlScreen> createState() => _ManualControlScreenState();
}

class _ManualControlScreenState extends State<ManualControlScreen> {
  double powerLevel = 0.5;
  bool _exiting = false;

  @override
  void initState() {
    super.initState();
    SystemChrome.setPreferredOrientations(
        [DeviceOrientation.landscapeLeft, DeviceOrientation.landscapeRight]);
  }

  @override
  void dispose() {
    SystemChrome.setPreferredOrientations(const []);
    super.dispose();
  }

  void _send({double fx = 0, double fz = 0, double tz = 0}) {
    widget.service.sendControl(mode: _kP02Mode, fx: fx, fz: fz, tz: tz);
  }

  void _neutral() => _send();

  void _onJoystick(Offset stick) {
    _send(fx: stick.dy * _kManualFx * powerLevel, tz: stick.dx * _kManualTz * powerLevel);
  }

  Future<void> _exit() async {
    if (_exiting) return;
    _exiting = true;
    widget.service.stop();
    if (mounted) Navigator.of(context).pop();
  }

  @override
  Widget build(BuildContext context) {
    return PopScope(
      canPop: false,
      onPopInvokedWithResult: (didPop, _) {
        if (!didPop) _exit();
      },
      child: Scaffold(
        backgroundColor: AppTheme.navy900,
        body: SafeArea(
          child: ListenableBuilder(
            listenable: widget.service,
            builder: (context, _) {
              final latest = widget.service.latest;
              final height = latest['height'];
              final battery = latest['battery'];
              return Stack(
                children: [
                  // Controles principales: centrados, usan todo el ancho
                  // (esto es lo que importa en horizontal, no la altura).
                  Center(
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                      crossAxisAlignment: CrossAxisAlignment.center,
                      children: [
                        Column(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            _GamepadButton(
                              icon: Icons.keyboard_double_arrow_up,
                              label: 'Subir',
                              onPress: () => _send(fz: _kManualFz * powerLevel),
                              onRelease: _neutral,
                            ),
                            const SizedBox(height: 22),
                            _GamepadButton(
                              icon: Icons.keyboard_double_arrow_down,
                              label: 'Bajar',
                              onPress: () => _send(fz: -_kManualFz * powerLevel),
                              onRelease: _neutral,
                            ),
                          ],
                        ),
                        _Joystick(size: 210, onChanged: _onJoystick, onEnd: _neutral),
                        _PowerLever(
                          value: powerLevel,
                          onChanged: (v) => setState(() => powerLevel = v),
                        ),
                      ],
                    ),
                  ),
                  // Salir / STOP, arriba a la izquierda.
                  Positioned(
                    top: 4,
                    left: 4,
                    child: Row(
                      children: [
                        IconButton(
                          onPressed: _exit,
                          icon: const Icon(Icons.arrow_back, color: Colors.white),
                          tooltip: 'Salir (manda STOP)',
                        ),
                        const Icon(Icons.gamepad, color: Colors.white54, size: 18),
                        const SizedBox(width: 6),
                        const Text('Control manual (P02)',
                            style: TextStyle(color: Colors.white70, fontWeight: FontWeight.w600, fontSize: 13)),
                      ],
                    ),
                  ),
                  // Telemetria en vivo, arriba a la derecha.
                  Positioned(
                    top: 12,
                    right: 12,
                    child: Row(
                      children: [
                        _StatChip(
                            icon: Icons.height,
                            text: height != null ? '${height.toStringAsFixed(2)} m' : '--'),
                        const SizedBox(width: 8),
                        _StatChip(
                            icon: Icons.battery_std,
                            text: battery != null ? '${battery.toStringAsFixed(1)} V' : '--'),
                      ],
                    ),
                  ),
                  // STOP, abajo a la derecha -- chico pero siempre a mano.
                  Positioned(
                    bottom: 10,
                    right: 12,
                    child: FilledButton.icon(
                      style: FilledButton.styleFrom(
                        backgroundColor: AppTheme.danger,
                        padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
                      ),
                      onPressed: _exit,
                      icon: const Icon(Icons.stop_circle_outlined),
                      label: const Text('STOP', style: TextStyle(letterSpacing: 0.5)),
                    ),
                  ),
                ],
              );
            },
          ),
        ),
      ),
    );
  }
}

class _StatChip extends StatelessWidget {
  final IconData icon;
  final String text;
  const _StatChip({required this.icon, required this.text});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(20),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, size: 14, color: AppTheme.celeste),
          const SizedBox(width: 4),
          Text(text, style: const TextStyle(color: Colors.white, fontSize: 12, fontWeight: FontWeight.w600)),
        ],
      ),
    );
  }
}

/// Boton estilo gamepad: circular, con relieve (gradiente + sombra) para que
/// se sienta "apretable" en vez de un rectangulo plano.
class _GamepadButton extends StatefulWidget {
  final IconData icon;
  final String label;
  final VoidCallback onPress;
  final VoidCallback onRelease;

  const _GamepadButton({
    required this.icon,
    required this.label,
    required this.onPress,
    required this.onRelease,
  });

  @override
  State<_GamepadButton> createState() => _GamepadButtonState();
}

class _GamepadButtonState extends State<_GamepadButton> {
  bool _pressed = false;

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) {
        setState(() => _pressed = true);
        widget.onPress();
      },
      onTapUp: (_) {
        setState(() => _pressed = false);
        widget.onRelease();
      },
      onTapCancel: () {
        setState(() => _pressed = false);
        widget.onRelease();
      },
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 90),
        width: 76,
        height: 76,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: _pressed
                ? [AppTheme.blue700, AppTheme.blue600]
                : [AppTheme.blue500, AppTheme.blue700],
          ),
          boxShadow: _pressed
              ? []
              : [BoxShadow(color: Colors.black.withValues(alpha: 0.35), blurRadius: 10, offset: const Offset(0, 4))],
          border: Border.all(color: AppTheme.sky300.withValues(alpha: 0.5), width: 1.5),
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(widget.icon, color: Colors.white, size: 26),
            Text(widget.label, style: const TextStyle(color: Colors.white70, fontSize: 10, fontWeight: FontWeight.w600)),
          ],
        ),
      ),
    );
  }
}

/// Mando direccional estilo gamepad: bisel oscuro con anillos guia, y un
/// knob con relieve que brilla (glow celeste) mientras se arrastra.
class _Joystick extends StatefulWidget {
  final double size;
  final ValueChanged<Offset> onChanged;
  final VoidCallback onEnd;

  const _Joystick({required this.size, required this.onChanged, required this.onEnd});

  @override
  State<_Joystick> createState() => _JoystickState();
}

class _JoystickState extends State<_Joystick> {
  Offset _knob = Offset.zero;
  bool _active = false;

  void _updateFromLocal(Offset local) {
    final radius = widget.size / 2;
    final center = Offset(radius, radius);
    var delta = local - center;
    final dist = delta.distance;
    if (dist > radius && dist > 0) {
      delta = delta / dist * radius;
    }
    final normalized = Offset(delta.dx / radius, -delta.dy / radius);
    setState(() {
      _knob = normalized;
      _active = true;
    });
    widget.onChanged(normalized);
  }

  void _reset() {
    setState(() {
      _knob = Offset.zero;
      _active = false;
    });
    widget.onEnd();
  }

  @override
  Widget build(BuildContext context) {
    final radius = widget.size / 2;
    final knobSize = widget.size * 0.34;
    final travel = radius - knobSize / 2;

    return GestureDetector(
      onPanStart: (d) => _updateFromLocal(d.localPosition),
      onPanUpdate: (d) => _updateFromLocal(d.localPosition),
      onPanEnd: (_) => _reset(),
      onPanCancel: _reset,
      child: SizedBox(
        width: widget.size,
        height: widget.size,
        child: Stack(
          alignment: Alignment.center,
          children: [
            // Bisel exterior.
            Container(
              width: widget.size,
              height: widget.size,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                gradient: const LinearGradient(
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                  colors: [AppTheme.navy800, Color(0xFF0F1A33)],
                ),
                border: Border.all(color: AppTheme.sky300.withValues(alpha: 0.35), width: 2),
                boxShadow: [
                  BoxShadow(color: Colors.black.withValues(alpha: 0.45), blurRadius: 16, offset: const Offset(0, 6)),
                ],
              ),
            ),
            // Anillos guia.
            Container(
              width: widget.size * 0.72,
              height: widget.size * 0.72,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                border: Border.all(color: Colors.white.withValues(alpha: 0.08), width: 1),
              ),
            ),
            Container(
              width: widget.size * 0.42,
              height: widget.size * 0.42,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                border: Border.all(color: Colors.white.withValues(alpha: 0.10), width: 1),
              ),
            ),
            // Flechas de referencia (avanza/retrocede/gira).
            Positioned(top: 14, child: _hint(Icons.keyboard_arrow_up)),
            Positioned(bottom: 14, child: _hint(Icons.keyboard_arrow_down)),
            Positioned(left: 14, child: _hint(Icons.keyboard_arrow_left)),
            Positioned(right: 14, child: _hint(Icons.keyboard_arrow_right)),
            // Knob.
            Positioned(
              left: radius + _knob.dx * travel - knobSize / 2,
              top: radius - _knob.dy * travel - knobSize / 2,
              child: Container(
                width: knobSize,
                height: knobSize,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  gradient: const LinearGradient(
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                    colors: [AppTheme.blue400, AppTheme.blue700],
                  ),
                  boxShadow: [
                    if (_active)
                      BoxShadow(color: AppTheme.celeste.withValues(alpha: 0.55), blurRadius: 20, spreadRadius: 2),
                    BoxShadow(color: Colors.black.withValues(alpha: 0.4), blurRadius: 8, offset: const Offset(0, 3)),
                  ],
                  border: Border.all(color: Colors.white.withValues(alpha: 0.5), width: 1.5),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _hint(IconData icon) => Icon(icon, color: Colors.white.withValues(alpha: 0.25), size: 18);
}

/// Palanca de potencia: fader vertical con gradiente y marcas, al estilo de
/// un acelerador de radiocontrol real -- se queda donde la sueltes.
class _PowerLever extends StatefulWidget {
  final double value; // 0..1
  final ValueChanged<double> onChanged;

  const _PowerLever({required this.value, required this.onChanged});

  @override
  State<_PowerLever> createState() => _PowerLeverState();
}

class _PowerLeverState extends State<_PowerLever> {
  static const double _trackHeight = 210;
  static const double _trackWidth = 56;
  static const double _thumbHeight = 34;

  void _updateFromLocalY(double localY) {
    final usable = _trackHeight - _thumbHeight;
    final clampedY = localY.clamp(_thumbHeight / 2, _trackHeight - _thumbHeight / 2);
    final value = 1.0 - ((clampedY - _thumbHeight / 2) / usable);
    widget.onChanged(value.clamp(0.0, 1.0));
  }

  @override
  Widget build(BuildContext context) {
    final thumbTop = (1.0 - widget.value) * (_trackHeight - _thumbHeight);

    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        const Text('POTENCIA',
            style: TextStyle(color: Colors.white70, fontSize: 11, fontWeight: FontWeight.w700, letterSpacing: 1)),
        const SizedBox(height: 8),
        GestureDetector(
          onVerticalDragUpdate: (d) => _updateFromLocalY(d.localPosition.dy),
          onTapDown: (d) => _updateFromLocalY(d.localPosition.dy),
          child: Container(
            width: _trackWidth,
            height: _trackHeight,
            padding: const EdgeInsets.symmetric(horizontal: 8),
            decoration: BoxDecoration(
              borderRadius: BorderRadius.circular(16),
              gradient: const LinearGradient(
                begin: Alignment.topCenter,
                end: Alignment.bottomCenter,
                colors: [Color(0xFF0F1A33), AppTheme.navy800],
              ),
              border: Border.all(color: Colors.white.withValues(alpha: 0.12)),
              boxShadow: [BoxShadow(color: Colors.black.withValues(alpha: 0.35), blurRadius: 10, offset: const Offset(0, 4))],
            ),
            child: Stack(
              alignment: Alignment.center,
              children: [
                // Marcas de escala.
                Column(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: List.generate(5, (i) => Container(
                        width: 14,
                        height: 2,
                        color: Colors.white.withValues(alpha: 0.18),
                      )),
                ),
                // Relleno segun el valor actual.
                Align(
                  alignment: Alignment.bottomCenter,
                  child: Container(
                    width: 10,
                    height: (_trackHeight - _thumbHeight / 2) * widget.value,
                    decoration: BoxDecoration(
                      borderRadius: BorderRadius.circular(6),
                      gradient: const LinearGradient(
                        begin: Alignment.bottomCenter,
                        end: Alignment.topCenter,
                        colors: [AppTheme.celeste, AppTheme.blue500],
                      ),
                    ),
                  ),
                ),
                // Thumb.
                Positioned(
                  top: thumbTop,
                  child: Container(
                    width: _trackWidth - 8,
                    height: _thumbHeight,
                    decoration: BoxDecoration(
                      borderRadius: BorderRadius.circular(10),
                      gradient: const LinearGradient(
                        colors: [AppTheme.blue400, AppTheme.blue700],
                      ),
                      border: Border.all(color: Colors.white.withValues(alpha: 0.6), width: 1.2),
                      boxShadow: [BoxShadow(color: Colors.black.withValues(alpha: 0.4), blurRadius: 6, offset: const Offset(0, 2))],
                    ),
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: List.generate(3, (i) => Padding(
                            padding: const EdgeInsets.symmetric(vertical: 1.5),
                            child: Container(width: 20, height: 1.5, color: Colors.white.withValues(alpha: 0.6)),
                          )),
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 8),
        Text('${(widget.value * 100).round()}%',
            style: const TextStyle(color: Colors.white, fontWeight: FontWeight.bold, fontSize: 15)),
      ],
    );
  }
}
