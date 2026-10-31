# Subida manual66cm — 28 de octubre de2026

Registro: logs/vertical_motion/20261028_154827_764414.jsonl. Autenticaciónviewer; modo0/armed0 durante captura. Sin comandos de control, reset o motores. Avisos de voz para pausa inicial10s, subida5s, pausa alta10s, bajada5s, pausa final10s. Cada plazo empieza después de terminar su aviso; fases del log incluyen el tiempo del aviso. La posición física la controla el operador; no hay referencia externa instrumentada.

Medianas de los últimos4s de cada pausa:

- Inicial: −0,056544m; rango0,115577m.
- Alta: +0,596229m; rango0,133705m.
- Retorno: −0,115171m; rango0,135264m.
- Subida relativa:0,652773m frente a0,66m declarados; diferencia−0,007227m.
- Diferencia al retornar:−0,058627m respecto al inicio.

El signo de velocidad presenta máximo+0,304807m/s al subir y mínimo−0,339712m/s al bajar. Las fases de movimiento incluyen las pausas asociadas al aviso: sus medianas no son la velocidad física media del recorrido.

Reproducción del controlador29 con F9(~5Hz), sin visión ni aprendizaje: confianza aceptada0/48 muestras de faseup y1/45 de phasedown; primera aceptación de bajada a7,813s del inicio del aviso de bajada. No certifica comportamiento idéntico a25Hz ni mide fuerzas. Sí advierte que el criterio barométrico puede ser demasiado restrictivo para un cambio real breve; no considerar validada la recuperación barométrica de vuelo por la rampa sintética limpia.

Conclusión: diferencia de altura consistente con la subida declarada en esta prueba; precisión estática y retorno siguen limitados. No ajustar la escala con una sola subida. Antes de aceptar29 para vuelo, revisar la detección de movimientos reales breves y comprobarla con capturas estáticas y este movimiento, idealmente con adquisición completa. No se modificó firmware en esta prueba.
