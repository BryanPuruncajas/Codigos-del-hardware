import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// Envoltorio sincrono sobre SharedPreferences.
///
/// Prefs.init() se llama UNA vez en main() antes de runApp(); a partir de
/// ahi los valores ya estan en memoria y get()/set() no necesitan await.
/// Sin esto, cada panel de dev_screen.dart perdia lo que el usuario habia
/// escrito (angulos, ganancias de PID, IP de conexion) cada vez que se
/// cerraba la app -- los TextEditingController siempre arrancaban con el
/// valor hardcodeado del codigo.
class Prefs {
  static late SharedPreferences _sp;

  static Future<void> init() async {
    _sp = await SharedPreferences.getInstance();
  }

  static String getString(String key, String fallback) =>
      _sp.getString(key) ?? fallback;

  static void setString(String key, String value) => _sp.setString(key, value);
}

/// TextEditingController que se guarda solo en cada cambio y arranca
/// releyendo el ultimo valor guardado (o `fallback` si es la primera vez).
TextEditingController persistentController(String key, String fallback) {
  final controller = TextEditingController(text: Prefs.getString(key, fallback));
  controller.addListener(() => Prefs.setString(key, controller.text));
  return controller;
}
