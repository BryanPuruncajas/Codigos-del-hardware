import 'package:flutter/material.dart';
import 'core/prefs.dart';
import 'core/theme/app_theme.dart';
import 'screens/home_screen.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Prefs.init();
  runApp(const BlimpApp());
}

class BlimpApp extends StatelessWidget {
  const BlimpApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'AeroStock',
      theme: AppTheme.light,
      home: const HomeScreen(),
    );
  }
}