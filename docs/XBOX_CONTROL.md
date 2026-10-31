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
| Y | Entrar en manual desde cualquier modo. Si está desarmado, ejecuta el armado con potencia cero. No vuelve a automático. |
| RB, en manual | Confirmar por decisión del operador el waypoint pendiente. Permanece en manual. |
| View/Back | STOP y desarmado; termina la sesión. |
| Menu/Start, en manual | Retomar una misión pausada; si no existe, preparar un recorrido nuevo estando desarmado y con sensores válidos. |

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

Abrir `groundstation\05_XBOX_CONTROL.cmd`: empieza en manual desarmado, sin exigir Flowdeck ni una misión previa. Pulsar Y para entrar y armar manual con potencia cero; después soltar A/cruceta y dar una orden nueva. Y también toma control desde automático sin borrar el progreso. La sesión no tiene corte de tiempo por defecto (`--seconds 0`). Para preparar automático al iniciar, usar `--start auto`.

CMD, para iniciar track 2 automático sin límite de sesión:

```bat
"C:\Users\stefa\.platformio\penv\Scripts\python.exe" -B -X utf8 -u "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation\xbox_control.py" --actuate --start auto --course track2 --seconds 0
```

Opciones de recorrido: `--course waypoint`, `--course track1` (defecto), `--course track2`. `--seconds 0` mantiene la sesión hasta STOP, desconexión o fin del recorrido; también se puede limitar a 5..3600 segundos. Esto elimina el corte global de sesión, no las protecciones de enlace ni los límites internos de búsqueda/giro del firmware. Si el automático falla, el mando queda disponible en manual.

En manual, A/cruceta solo actúan si F3 confirma armado. Pulsar Y estando desarmado ejecuta un armado explícito a potencia cero, sin requerir Flowdeck ni iniciar automático. Mantener un botón de movimiento durante la transición no conserva esa orden: soltarlo y volverlo a pulsar. Los cambios automáticos por fallo no rearman por sí solos. Retomar con Menu requiere Flowdeck y Nicla válidas, misión pendiente y F3 armado; conserva recorrido, progreso y referencia de rumbo.

El Xbox toma control exclusivo en Ground. La app puede seguir observando y STOP permanece disponible desde cualquier cliente. Para volver a controlar desde la app, terminar la sesión Xbox.

## Protección y registro

La computadora envía latidos/comandos cada 100 ms. Si el mando se desconecta, se intenta STOP. Si la aplicación deja de enviar latidos, Ground solicita STOP al vencer 750 ms. La ESP principal también vigila comandos del mando con 750 ms, opt-in mediante marcador 22594 en el campo reload; las tramas habituales de la app y CLI no activan ese marcador. No se cambió el pinout ni el formato de 13 floats.

El marcador no rearma, reinicia ni convierte medidas antiguas en recientes. Un STOP borra la sesión pausada. La llegada automática sigue usando su validación existente; RB se registra aparte como confirmación manual.

Registros: `logs\xbox\<fecha_hora>.jsonl`. Incluyen comandos, telemetría y confirmaciones manuales.

## Validación realizada

2026-10-31: compilación ESP32 correcta; 49 pruebas Python correctas; prueba nativa del código real de misión para pausa/reanudación, RB idempotente, rumbo track 2, fin del recorrido y watchdog. Pasan también las regresiones de altura Flowdeck, ausencia de barómetro, rutas de motores y control de yaw/inercia.

La v42 se cargó correctamente en la ESP principal por COM4 (MAC DC:B4:D9:39:B3:B4), con hash verificado. Se reinició el proceso Ground para cargar el soporte Xbox. Tras cambiar el cable USB, XInput detectó el mando y la prueba física en modo monitor registró B (30 a 35%), X (35 a 30%), A, las cuatro direcciones de cruceta, Y, RB y View/Back. Menu/Start todavía no se comprobó físicamente. El modo monitor no conecta a Ground ni activa motores. También se comprobó el enlace Xbox -> Ground -> ESP con el blimp por batería: P02 desarmado devolvió en F4 las cinco órdenes (subir, avanzar, retroceder, izquierda y derecha). La prueba registró STOP confirmado, cero comandos de armado y todos los F3 con motores en cero. Registro local: logs/xbox/20261031_114457_disarmed_echo.jsonl. La respuesta física de actuadores y la transición auto/manual en vuelo siguen pendientes. En esta prueba Nicla comunicaba, pero Flowdeck no entregaba muestras; por ello todavía no se puede iniciar el automático.




Actualización de inicio manual: 56 pruebas Python correctas. Toma de control armado requiere petición explícita takeover y F3 reciente; las conexiones normales conservan el requisito de inicio desarmado. La ESP v42, Nicla, pines y filtros de sensores no se modificaron en este ajuste. No se hizo push.

