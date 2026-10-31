# Altura exclusivamente Flowdeck — v39

Fecha: 30/10/2026.

Proyecto: `C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware`

Estado: **v39 cargada correctamente el 30/10/2026 en la ESP del blimp COM4, MAC DC:B4:D9:39:B3:B4; hash verificado y reinicio por RTS.** La ESP estaba fuera de la PCB, sin batería y alimentada solo por USB, según confirmación del usuario. Los cambios de app están en sus fuentes, pendientes de instalar en el celular. No se ejecutaron motores ni se validó el comportamiento físico de esta versión.

## Qué cambia

- SensorSuite no crea, inicializa, configura ni lee el BMP390. Tampoco obtiene su temperatura. Los controladores antiguos quedan archivados en la biblioteca; no hay rutinas del barómetro enlazadas en el firmware compilado.
- La altura de F1, la velocidad vertical, los datos verticales de SensorSuite y la observación vertical de BalloonMission proceden exclusivamente de Flowdeck.
- La altura es la distancia vertical desde Flowdeck al suelo, en metros. No se pone a cero al encender ni se alinea con una altura barométrica. Hay que considerar la posición física del sensor respecto al frame.
- Vz se calcula a partir del cambio de rango filtrado en el tiempo: positiva al subir y negativa al bajar. No se recibe una lectura `dz` en el paquete actual.
- No hay mezcla, sesgo ni respaldo barométrico. IMU, Nicla y batería conservan sus funciones; pines, reparto de motores y calibraciones de actuadores no cambian.
- X/Y siguen siendo diagnóstico en cuentas/s, pendientes de calibración para m/s; todavía no mandan correcciones de desplazamiento lateral.

## Validez y pérdida de datos

El estimador acepta rango de 0.04 a 3.0 m, inclinación hasta 20° y muestras de hasta 250 ms. Aplica mediana de tres, filtro de altura y una ventana de al menos cinco muestras y 160 ms para Vz. Comprueba secuencia, duplicados, saltos y reinicios del emisor.

Ante una interrupción breve conserva exclusivamente la última muestra Flowdeck cualificada, como máximo hasta que cumpla 250 ms. No actualiza artificialmente su fecha. El bit 8192 identifica esa retención; una inclinación inválida la cancela inmediatamente. Una interrupción mayor deja la altura inválida.

Sin altura válida, los modos automáticos que aplican sustentación se desarman y no ejecutan su control. Recuperar rango no vuelve a armar motores automáticamente. Las pruebas manuales y de lectura siguen disponibles. **No existe un respaldo de otro sensor.**

F1 conserva ceros como valores de transporte cuando falta altura; esos ceros no son una medida válida. La validez real está en F18. La app v39 muestra «—» en altura y Vz cuando falta una muestra de control válida y reciente.

## Dónde modificar y revisar

Todas las rutas parten de la carpeta de proyecto indicada arriba.

| Archivo | Función |
|---|---|
| `firmware\src\mission\FlowHeight.h` | Rango, filtros, frescura, altura y Vz exclusivamente Flowdeck. |
| `firmware\src\main.cpp` | Recibe el paquete ESP-NOW y publica los datos verticales en SensorSuite/F1. |
| `firmware\lib\BlimpSwarm\src\sense\SensorSuite.h` y `.cpp` | IMU y batería; no adquiere barómetro. Recibe altura y Vz de Flowdeck. |
| `firmware\src\mission\BalloonMission.cpp` | Usa la observación vertical de Flowdeck durante la misión. |
| `firmware\src\app\FlowSafety.h` y `firmware\src\tests\TestModes.cpp` | Exigen rango válido en los modos automáticos de altura. |
| `firmware\src\app\Telemetry.cpp` | F1, F14, F18 y F19; ya no emite F9/F15 barométricos. |
| `groundstation\common\telemetry.py` | Nombres de los campos que el bridge envía a la app. Reiniciar el bridge tras actualizar. |
| `blimp_app\lib\widgets\telemetry_widgets.dart` | Tarjetas de altura Flowdeck y validez. |
| `groundstation\check_flow_height.py` | Lectura y registro de altura sin armar ni enviar órdenes de motores. |

La referencia del waypoint sigue capturándose con un FULL centrado y cámara nivelada. Una vez capturada, queda fija en la escala de altura Flowdeck; aproximarse al globo no redefine esa altura. La llegada estimada conserva el umbral provisional de 120 px. Esta modificación no cambia la lógica del recorrido ni aumenta el empuje disponible.

## Comprobación inicial sin motores

El emisor actual es compatible con el paquete FlowProtocol v1 de 16 bytes. Que tenga `quality=0` no impide validar altura; sí impide considerar calibrado/válido el diagnóstico óptico X/Y. En la última lectura física anterior todavía no llegaban muestras Flowdeck; la compilación no prueba el enlace real.

v39 ya está cargada; falta actualizar la app cuando se decida hacerlo. Para futuras cargas, la ESP debe estar fuera de la PCB, sin batería y alimentada solo por USB. Para leer el blimp montado, alimentación por batería y únicamente Ground por USB a la computadora.

En CMD, iniciar Ground si no está ya ejecutándose:

```bat
cd /d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation"
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -X utf8 telemetry_bridge.py COM9
```

En otra ventana de CMD:

```bat
cd /d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation"
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -X utf8 check_flow_height.py --seconds 60
```

Se espera `fuente=FLOWDECK`, altura próxima a la medida física desde el sensor al suelo y Vz cerca de cero en reposo. Al subir/bajar, comprobar signo y amplitud; al interrumpir el emisor, comprobar altura inválida tras 250 ms. Los registros se guardan en `groundstation\logs\flow_height`. No iniciar vuelo hasta verificar la recepción real.

## Validación de software

- Compilación PlatformIO: correcta; RAM 50 936 bytes, flash 839 193 bytes.
- Ocho programas nativos: filtros, misión real, SensorSuite sin cabecera/objeto BMP, bloqueo de control sin Flowdeck, reparto de motores, yaw, inercia y parser del emisor.
- 25 pruebas Python: guardas de misión, opciones y lector sin órdenes a actuadores.
- Cinco pruebas Flutter: datos vigentes, datos vencidos, altura inválida y compatibilidad de visualización anterior.
- Inspección del ELF: sin símbolos `Barometer::`, `Bmp390` ni `Adafruit_BMP`.

Estos resultados verifican el código. La precisión de altura, el enlace y la respuesta de vuelo quedan por comprobar físicamente.
