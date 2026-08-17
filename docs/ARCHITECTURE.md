# Arquitectura refactorizada

`firmware/src/main.cpp` solo inicializa, recibe comandos y despacha pruebas.

- `app/`: pinout, seguridad, mapa de sensores y telemetría.
- `tests/`: lógica P01–P11.
- `mission/`: máquina reutilizable para 1/2/4 globos idénticos.
- `lib/BlimpSwarm/`: hardware/control heredado con saneamiento mínimo.
- `base_station/`: puente genérico Serial↔ESP-NOW.
- `groundstation/`: ejecutor de pruebas y logger CSV.
- `vision/`: percepción Nicla/OpenMV original, manteniendo detección roja calibrada.
- `legacy/`: snapshot de los archivos principales antes del refactor.

## Mapa de SensorSuite/Nicla

0 temp, 1 altura, 2 vz, 3 roll, 4 pitch, 5 yaw, 6 rollrate, 7 pitchrate, 8 yawrate, 9 reservado, 10 bateria, **11 flag Nicla**, 12 x, 13 y, 14 w, 15 h, 16–20 datos adicionales. No hay ultrasonico instalado.
