# Llegada estimada: referencia elegida de 80 cm / 120 px

Fecha: 2026-10-28. El usuario sustituye la referencia de 44 cm / 220 px por
**80 cm aproximadamente entre cámara y superficie, ancho de detección 120 px**.

Único cambio de comportamiento a bordo: en
`firmware/src/mission/BalloonMission.cpp`, `N03_VISIT_WIDTH_PX` pasa de 220 a 120.
Se mantienen las condiciones existentes: FULL previo, misma asociación,
observación reciente, alineación, mínimo 3 observaciones nuevas y 500 ms.
En el controlador P08/P09, un recorte grande también requiere ancla barométrica
válida, diferencia de altura <=20 cm, cobertura del centro de imagen, error de
yaw <=8 grados y velocidad de giro <=0.12 rad/s.

No mide contacto. No se modifican Nicla, pines, ángulos, potencia ni ganancias
de yaw. El umbral también lo consume la ruta de misión multiwaypoint, conservando
sus propias condiciones existentes de llegada.

## Evidencia y límite

La captura `logs/arrival_reference/20261028_204443_034426.jsonl` registró
121..134 px en la primera posición solicitada de 80 cm y 93..101 px al regresar.
Por tanto, 120 px se adopta por elección del usuario como referencia operacional
aproximada; no garantiza 80 cm exactos en diferentes ángulos o segmentaciones.
Los registros de las pruebas previas conservan su umbral histórico de 220 px.

## Repetir calibración visual sin motores

Con Ground abierto, en otra ventana CMD:

```bat
cd /d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation"
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u check_arrival_reference.py
```

Por defecto la voz solicita 160 -> 80 -> 160 cm y compara con 120 px. Esta prueba
solo recibe telemetría: no ejecuta ni valida `waypoint_cleared` a bordo y no
activa motores. La distancia física la mide el operador. No se ha ejecutado esta
nueva secuencia en el momento de preparar el cambio.

Para reproducir explícitamente la secuencia anterior:

```bat
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u check_arrival_reference.py --near-cm 44 --far-cm 80 --threshold-px 220
```

Verificación: compilación PlatformIO correcta; pruebas de misión, recuperación,
avance, yaw y llegada simulada correctas; 9 pruebas del script de referencia
correctas. La llegada simulada confirma a 120 px y no a 119 px; datos repetidos,
altura inválida o alineación incorrecta no confirman.

El código debe cargarse en la ESP antes de que el cambio afecte la misión.
Carga completada el 2026-10-28 en COM4, ESP32-S3 MAC DC:B4:D9:39:B3:B4.
PlatformIO finalizó SUCCESS en 17.64 s; 842640 bytes escritos, hash de datos
verificado y reinicio por RTS. No se ejecutó una prueba de motores durante
la carga. El resultado físico de llegada con este umbral sigue pendiente.
