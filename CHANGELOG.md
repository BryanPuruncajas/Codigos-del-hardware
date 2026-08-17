# v2 - hardware actual sin ultrasonico

- SensorSuite reducido a BMP390 + BNO085/BNO086 + monitor de bateria D8.
- Nicla Vision pasa a offset 11; arreglo total de sensores = 21.
- Telemetria core usa velocidad vertical en lugar de distancia ultrasonica.
- Codigo GY-US42V2 movido a `legacy/unused_sensors/` y no forma parte del build activo.
- La evitacion de pared heredada en `LevyWalk` queda deshabilitada para evitar interpretar `NICLA_FLAG` como distancia.

# Cambios de esta entrega

- Refactor P01–P11 con un solo firmware y módulos de prueba separados.
- Safe boot: sin auto-arm ni auto-calibración; PWM de actuadores deshabilitado al iniciar.
- ARM/DISARM explícito por `ControlInput.params[10]`.
- Límite duro de brushless: 50% del comando original.
- D2 reservado/HIGH como RST del BNO.
- Nicla offset ajustado a 11 para el hardware actual sin ultrasonico; protocolo balloon 0x40 conservado.
- GY-US42V2 retirado de `SensorSuite`; la evitacion de pared heredada que dependia de `sensors[11]` queda deshabilitada.
- Telemetría independiente por flag a 10 Hz y base genérica `TEL,...`.
- Nueva misión para globos idénticos: SEARCH→APPROACH→VISIT→ESCAPE→WAIT_LOST→SEARCH.
- Snapshot de archivos principales originales en `legacy/`.
