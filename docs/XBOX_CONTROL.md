# Control Xbox por USB - Ground y firmware 42

Proyecto: `C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware`

El mando se conecta por USB a la computadora. Ground continúa enviando los comandos al blimp. No se modifica ni reinstala la app, Nicla, Crazyflie o el firmware de la ESP Ground para esta integración. Sí se necesita cargar la ESP principal con v42 y reiniciar el programa Python de Ground.

## Botones

| Botón | Acción |
|---|---|
| B, derecha | Aumenta la potencia manual 5 puntos porcentuales por pulsación. |
| X, izquierda | Reduce la potencia manual 5 puntos porcentuales por pulsación. |
| A, abajo, mantenido | Subir con la potencia manual seleccionada. |
| Cruceta arriba, mantenida | Avanzar. |
| Cruceta abajo, mantenida | Retroceder. |
| Cruceta derecha, mantenida | Girar a la derecha. |
| Cruceta izquierda, mantenida | Girar a la izquierda. |
| Y | Tomar manual desde automático. Una nueva pulsación permite retomar el waypoint pendiente si la misión y sensores lo permiten. |
| RB, en manual | Confirmar por decisión del operador el waypoint pendiente. Permanece en manual. |
| View/Back | STOP y desarmado; termina la sesión. |
| Menu/Start, en manual desarmado | Iniciar la secuencia de armado manual. No rearma silenciosamente con Y. |

Potencia manual inicial: 30%. Ajustable de 0 a 50%. Mantener B/X no repite incrementos. En automático, B/X preparan el valor manual y no alteran la potencia automática, limitada al 25%.

Se conserva P02, con un eje a la vez. Si se mantienen A y una flecha, A tiene prioridad. Combinaciones de varias flechas quedan neutrales. Al soltar A/cruceta se pide cero empuje; el modo manual no mantiene altura por sí solo.

RB es una confirmación del operador, no una prueba de contacto del sensor. El progreso se confirma por F5 antes de anunciarlo. Las retransmisiones con el mismo identificador no suman dos waypoints.

Los recorridos existentes conservan cinco contactos, contando el inicio y el regreso: track 1, 1-2-3-4-1; track 2, 1-3-4-2-1. Y/RB no recalibran la orientación inicial; comenzar alineado con la línea 1 a 2, como en las pruebas previas.

## Archivos principales

- `groundstation\xbox_input.py`: XInput de Windows y mapa de botones.
- `groundstation\xbox_control.py`: sesión, telemetría, auto/manual y confirmación RB.
- `groundstation\pilot_lease.py`: control exclusivo y vigilancia en Ground.
- `groundstation\telemetry_bridge.py`: comandos `pilot_claim`, `pilot_ping`, `pilot_release`; preserva la API existente.
- `firmware\src\tests\TestModes.cpp`: pausa y reanudación de la misión conservando progreso.
- `firmware\src\mission\BalloonMission.cpp`: confirmación manual y reanudación del waypoint pendiente.
- `firmware\src\app\PilotWatchdog.h`: caducidad de los comandos del mando.

## Primero: probar solo botones

Conectar el Xbox por USB y abrir `groundstation\04_XBOX_BOTONES.cmd`. No abre Ground ni activa motores. La consola debe mostrar cada botón y los cambios de potencia.

Alternativa desde CMD:

```bat
"C:\Users\stefa\.platformio\penv\Scripts\python.exe" -B -X utf8 -u "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation\xbox_control.py" --monitor
```

El lector usa XInput de Windows, con ranuras 0 a 3. `--controller 1`, por ejemplo, selecciona una ranura específica. Si no detecta un Xbox/XInput, el modo actuado no inicia.

Referencia de los botones y lectura: https://learn.microsoft.com/en-us/windows/win32/api/xinput/ns-xinput-xinput_gamepad y https://learn.microsoft.com/en-us/windows/win32/api/xinput/nf-xinput-xinputgetstate

## Cargar la ESP principal

Preparar la ESP principal fuera de la PCB, sin batería y alimentada solo por USB, según el circuito del usuario. Identificar MAC `DC:B4:D9:39:B3:B4`; no confundir con la emisora Flowdeck `7C:4F:AD:1D:CA:D4`.

CMD, usando COM4 solo si la identificación lo confirma:

```bat
"C:\Users\stefa\.platformio\penv\Scripts\platformio.exe" run -d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\firmware" -e seeed_xiao_esp32s3 -t upload --upload-port COM4
```

Después, desconectar USB antes de montar en la PCB y alimentar el blimp por batería. La v42 incluye la mejora de estabilidad Flowdeck v41 y el ajuste manual P02 de 30 a 50% ya solicitado. Los pines y rutas de motores/servos permanecen iguales.

## Ground y control

Reiniciar `telemetry_bridge.py` para que cargue el soporte Xbox. Ground queda por USB a la computadora. No ejecutar al mismo tiempo `check_single_waypoint.py`, `run_test.py` u otro controlador de motores.

Si Ground está en COM9, desde CMD:

```bat
"C:\Users\stefa\.platformio\penv\Scripts\python.exe" -B -X utf8 -u "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation\telemetry_bridge.py" COM9
```

Para un waypoint, abrir `groundstation\05_XBOX_CONTROL.cmd`. Empieza preparando automático y usa cuenta por voz antes de liberar el avance. Y puede tomar manual durante la preparación. La sesión dura 240 segundos en total por defecto, incluida la preparación.

CMD, para track 2 durante hasta 600 segundos de sesión:

```bat
"C:\Users\stefa\.platformio\penv\Scripts\python.exe" -B -X utf8 -u "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation\xbox_control.py" --actuate --course track2 --seconds 600
```

Opciones de recorrido: `--course waypoint` (defecto), `--course track1`, `--course track2`. El tiempo de sesión se configura entre 5 y 3600 segundos; los límites temporales internos de la misión continúan activos.

En manual, A/cruceta solo actúan si F3 confirma armado. Si el automático ya se desarmó por una falla, Y selecciona manual y Menu ejecuta un armado nuevo. Retomar con Y requiere Flowdeck y Nicla válidas, la misión pendiente y F3 armado; se conservan recorrido, contactos completados y referencia de rumbo, reconstruyendo controladores transitorios.

El Xbox toma control exclusivo en Ground. La app puede seguir observando y STOP permanece disponible desde cualquier cliente. Para volver a controlar desde la app, terminar la sesión Xbox.

## Protección y registro

La computadora envía latidos/comandos cada 100 ms. Si el mando se desconecta, se intenta STOP. Si la aplicación deja de enviar latidos, Ground solicita STOP al vencer 750 ms. La ESP principal también vigila comandos del mando con 750 ms, opt-in mediante marcador 22594 en el campo reload; las tramas habituales de la app y CLI no activan ese marcador. No se cambió el pinout ni el formato de 13 floats.

El marcador no rearma, reinicia ni convierte medidas antiguas en recientes. Un STOP borra la sesión pausada. La llegada automática sigue usando su validación existente; RB se registra aparte como confirmación manual.

Registros: `logs\xbox\<fecha_hora>.jsonl`. Incluyen comandos, telemetría y confirmaciones manuales.

## Validación realizada

2026-10-31: compilación ESP32 correcta; 49 pruebas Python correctas; prueba nativa del código real de misión para pausa/reanudación, RB idempotente, rumbo track 2, fin del recorrido y watchdog. Pasan también las regresiones de altura Flowdeck, ausencia de barómetro, rutas de motores y control de yaw/inercia.

La v42 se cargó correctamente en la ESP principal por COM4 (MAC DC:B4:D9:39:B3:B4), con hash verificado. Se reinició el proceso Ground para cargar el soporte Xbox. Windows enumera el mando USB como Xbox Gaming Device (VID 20D6, PID 2061), pero todavía no entrega lecturas por XInput ni GameInput y el operador informa luz apagada. La validación física de botones está pendiente. No se activaron motores como parte de esta integración.

