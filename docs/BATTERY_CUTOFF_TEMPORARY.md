# Corte por estimación de batería suspendido durante las pruebas de pistas

Fecha: 2026-10-29. Solicitado por el usuario después de dos pruebas interrumpidas
al cruzar la lectura estimada de 3.5 V. La política temporal corresponde al
lanzador `groundstation/check_single_waypoint.py`.

## Cambio

El lanzador ya no bloquea el inicio ni interrumpe una prueba por el valor de
`battery` en F1. También permite una estimación no finita: este dato temporalmente
no decide el control. F1, registros y dashboard conservan la lectura original.
Cada ejecución registra `battery_policy`, con `cutoff_enabled=false` y
`telemetry_retained=true`; no se sustituye el voltaje por un valor artificial.

Se mantienen el límite de potencia 20%, el plazo máximo de 120 segundos,
verificación de firmware, yaw válido, telemetría reciente, fallos de misión,
interbloqueo entre impulso y giro, y STOP con confirmación de desarmado.
El registro de error ahora conserva la causa real del supervisor cuando una
locución es interrumpida, en lugar de ocultarla con el mensaje genérico de voz.

No se modificaron firmware, sensores, pines, Nicla ni aplicación. La ESP sigue
con la referencia de llegada 120 px. Otros lanzadores de calibración, como
`check_visual_support.py` y `check_thrust_direction.py`, conservan su política
propia; el cambio se aplica a la prueba de waypoint que se estaba ejecutando.

## Uso desde CMD

Con Ground abierto, ejecutar en otra ventana:

```bat
cd /d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation"
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u check_single_waypoint.py --actuate --seconds 120
```

Para restaurar el corte de 3.5 V en cualquier ejecución, añadir
`--battery-cutoff`. Al terminar la validación de las pistas corresponde restaurar
la política por defecto; no hay una detección automática de «todas las pistas
completas» que la reactive.

La modificación no inicia una prueba ni arma motores por sí misma.
