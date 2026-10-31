# Referencia fija de altura — v37

Proyecto: `C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware`

Estado: implementado para P08/P09, compilación y pruebas de software; **pendiente de cargar en ESP y validar físicamente**. No se inició ningún ensayo actuado con esta actualización. La ESP conserva la versión instalada anteriormente hasta cargar este firmware.

## Funcionamiento

1. SEARCH busca lentamente. En APPROACH, antes del avance, exige un FULL centrado, cámara horizontal respecto a su montaje y barómetro reciente y coherente. Captura la mediana de hasta 16 muestras: al menos 5 adquisiciones diferentes y 500 ms de estabilidad. Se exige x=120±24 px, y=80±10 px, sin recorte vertical, pitch a ±3° del nivel configurado y velocidad de yaw ≤0.08 rad/s.
2. Guarda una sola altura base relativa por ensayo. Una aproximación, cambio de tamaño, PARTIAL, pérdida temporal o nuevo identificador de cámara no sustituye esa base. RESET de misión sí permite capturar una nueva.
3. Mantiene aparte el sesgo barométrico: `altura_estimada = altura_barometrica - sesgo`; `error = objetivo_fijo - altura_estimada`. La altura es relativa al origen del barómetro, no distancia absoluta al suelo.
4. Con referencia capturada, el error de posición vertical se calcula en metros. El centro y=80 deja de mandar continuamente la posición vertical. La velocidad y evidencia de movimiento siguen ayudando a reducir empuje antes de sobrepasarse y aumentarlo durante una caída. No existe empuje hacia abajo.
5. La actualización de sesgo requiere geometría y pitch estables. Corrección de ancla limitada a 5 mm/s; proyección de deriva limitada a 4 s y luego congelada, conservando la base. Con un ajuste de altura deliberado distinto de cero, se congela el aprendizaje del sesgo para no absorber ese desplazamiento real.
6. El avance y la llegada estimada requieren referencia válida y liberación tras la voz. Se inicia P08 con permiso de avance bloqueado; al terminar la cuenta, se envían los mismos parámetros con `reset=0`, sin borrar referencia, trim ni yaw. La potencia vertical y el yaw sí pueden actuar durante la preparación mientras se sostiene el frame.
7. Si pasan 20 s desde la liberación en APPROACH sin capturar referencia, falla y desarma. Se conservan los límites existentes: 15 s de recuperación visual, 1.5 s sin comunicación de cámara y 180 s máximos a bordo. El ensayo de PC dura hasta 120 s tras la cuenta.

La banda mínima de error es 3 cm y aumenta con el ruido observado. Es una banda de control, **no una garantía de precisión**. La deriva lenta y el movimiento real no siempre son distinguibles sin una medida independiente. La cámara debe tener conocido su pitch cuando apunta horizontalmente; no se infiere automáticamente su montaje. El waypoint debe estar fijo.

## Qué se conserva

- Pines PCB, UART, servos neutros 52.5°/64.5°, inversión física M1→D10 y M2→D9.
- Ataque de avance hasta 28°, solicitud horizontal 0.12 y techo de potencia 25% del perfil anterior; no son valores de empuje físico medido.
- Llegada **estimada** a 120 px: referencia provisional del ensayo de aproximadamente 80 cm, no prueba de contacto.
- Nicla v7 y aplicación móvil sin cambios. Ground necesita reiniciarse para etiquetar F17.
- El corte de batería del lanzador sigue suspendido por decisión previa del usuario; se conserva voltaje en el log y se puede reactivar con `--battery-cutoff`.

La referencia fija está integrada en la aproximación de un waypoint P08/P09. **P10/P11 y la conservación del mismo plano durante el recorrido completo siguen usando la lógica previa**; no se consideran validados por esta actualización. En la app, P08/P09 usa el nuevo requisito de referencia; los ajustes y la cuenta de este documento se manejan desde el lanzador de PC.

## Comandos en CMD

Subir ESP, únicamente fuera de la PCB, sin batería y alimentada por USB. El comando siguiente no se ejecutó como parte de esta implementación:

```bat
"%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" run -d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\firmware" -e seeed_xiao_esp32s3 -t upload --upload-port COM4
```

Después de montar, blimp por batería y solo Ground por USB. En una ventana CMD:

```bat
cd /d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation"
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u telemetry_bridge.py COM9
```

En otra ventana CMD, primera prueba sin avance:

```bat
cd /d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation"
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u check_single_waypoint.py --actuate --seconds 60 --vertical-only
```

Esperar la voz **«Altura fijada. Solo control vertical»**. Debe mantener el mismo `altura_objetivo_fija` mientras cambia la distancia al globo. Revisar físicamente la altura y comparar estimación/velocidad/potencia; no concluir éxito solo por la telemetría.

Una vez comprobada la regulación vertical, aproximación de un waypoint:

```bat
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u check_single_waypoint.py --actuate --seconds 120
```

Ajuste opcional +10 cm respecto al plano capturado, aplicado gradualmente a 2 cm/s:

```bat
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u check_single_waypoint.py --actuate --seconds 60 --vertical-only --height-offset-cm 10
```

Rango de ajuste −50..50 cm, resolución 1 cm. `--camera-level-pitch-deg 0` es el valor predeterminado; sustituirlo por el pitch de IMU medido con la cámara horizontal si su montaje está inclinado (−30..30°, resolución 0.5°). Sin referencia válida no hay avance; el log muestra `esperando_referencia`.

## Telemetría y resultados

F17 a 5 Hz contiene objetivo fijo, altura estimada, sesgo, ajuste aplicado, flags y pitch horizontal configurado. Flags: 1 capturado; 2 control barométrico válido; 4 cuenta liberada; 8 avance habilitado; 16 ajuste solicitado; 32 ancla de sesgo reciente; 64 esperando captura. Para impulsar se exigen los cuatro primeros bits y los controles existentes de alineación/altura/velocidad. F16 conserva error, ruido y deriva del modelo de referencia.

Logs: `C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\logs\single_waypoint\*.jsonl`.

El protocolo conserva 13 floats: AUX4 permite una extensión entera exacta en float32 con marcador bit23, permisos de liberación/avance, ajuste, deadband y pitch. Valores antiguos 1..100 siguen siendo el deadband; en clientes antiguos la liberación se considera inmediata. Solo el lanzador actualizado proporciona el bloqueo de la cuenta por voz. Paquetes extendidos inválidos fallan y desarman.

## Archivos principales

- `firmware\src\app\VisualSupport.h`: captura, base fija, sesgo y ajuste.
- `firmware\src\mission\InertialHeight.h`: posición en metros y frenado vertical.
- `firmware\src\mission\BalloonMission.cpp`: integración P08/P09 y permisos de avance/llegada.
- `firmware\src\mission\MissionHeightOptions.h`: decodificación AUX4.
- `groundstation\check_single_waypoint.py`: cuenta, prueba vertical y ajustes.
- `groundstation\common\height_options.py`: codificación de opciones.
- `firmware\src\app\Telemetry.cpp` y `groundstation\common\telemetry.py`: F17.
- `groundstation\fixed_height_v37_tests.sh`: regresiones C++ sobre implementación real, sin motores.
- `groundstation\test_single_waypoint_guard.py` y `test_fixed_height_options.py`: verificaciones del lanzador y protocolo.

Todos los paths anteriores se resuelven desde la carpeta absoluta del proyecto indicada al inicio. Respaldo de las fuentes de control anteriores: `docs\backup_before_fixed37`.
