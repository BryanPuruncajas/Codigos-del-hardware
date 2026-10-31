# Flowdeck para altura y diagnóstico de movimiento — v38

Proyecto: `C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware`

Estado: versión implementada para comparar sensores y para P08/P09. **Firmware v38 cargado correctamente en la ESP del blimp COM4, MAC DC:B4:D9:39:B3:B4, el 30/10/2026; hash verificado.** La app y el emisor Flowdeck no se cargaron en esta operación. La validación física queda pendiente; no se inició ningún ensayo de motores.

## Cambio respecto a v37

Con rango reciente y coherente, la altura y velocidad vertical de Flowdeck alimentan el regulador usado durante búsqueda y aproximación de P08/P09. El controlador ya no depende de la deriva del barómetro mientras dispone de ese rango. El barómetro bruto de F1/F9 se conserva para comparación.

La referencia del waypoint se sigue capturando con un FULL centrado y cámara nivelada. Una vez capturada, permanece fija. Con Flowdeck se congela el aprendizaje visual del sesgo barométrico: el movimiento real medido por rango no se absorbe como deriva. Si el vehículo está sobre el plano del waypoint pero está cayendo rápidamente, una velocidad vertical confiable de Flowdeck permite aumentar sustentación para frenar la caída, aunque la vista del globo esté recortada.

**No cambia la fuerza disponible:** conserva techo de potencia 25%, ángulos, pines, reparto de motores, yaw, interbloqueo de cuenta por voz y llegada estimada a 120 px. La pérdida de helio sigue siendo una limitación física. Esta versión no resuelve por software una sustentación insuficiente.

P10/P11, P03/P04/P05 y sus reguladores previos conservan su comportamiento. El nuevo estimador se publica en todos los modos para poder comprobarlo sin motores; su uso en el control está integrado en P08/P09.

## Qué significa cada dato

| Dato | Tratamiento en esta versión |
| --- | --- |
| `range_mm` | Distancia de Flowdeck a la superficie bajo el sensor, corregida por inclinación para obtener altura vertical. |
| `vz` / velocidad Z | Pendiente de la altura filtrada sobre una ventana de muestras; positiva al subir, negativa al bajar, en m/s. No se recibe `dz` en el paquete. |
| `dx`, `dy` | Lecturas incrementales reenviadas por UART. El formato enviado no demuestra una escala en m/s. Se conservan como cuentas. |
| Flujo X/Y | Cuentas/s calculadas entre muestras consecutivas recibidas, condicionadas a calidad y baja velocidad angular. No son velocidades métricas ni están referidas al mundo. |
| Calidad | `squal` del mensaje UART. El código que compartiste lo imprimía, pero no lo copiaba a `packet.quality`. |
| `motion` | Se valida el campo numérico en el emisor; no se interpreta como un estado físico porque su semántica no está documentada en el código compartido. |

El sensor óptico observa movimiento relativo al suelo y el ToF mide rango. Se requiere una superficie fija y aproximadamente plana, con Flowdeck orientada hacia abajo. [Descripción de Bitcraze](https://www.bitcraze.io/flow-deck-v2/), [condiciones de uso y suelo](https://www.bitcraze.io/documentation/tutorials/getting-started-with-flow-deck/).

Para convertir X/Y a m/s faltan la escala de las lecturas, su intervalo real de adquisición, la orientación de ejes y compensación de rotaciones. Esta versión no inventa esa calibración ni usa X/Y para mandar correcciones laterales.

## Filtrado y selección de altura

Parámetros centralizados en:

`C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\firmware\src\mission\FlowHeight.h`

- Rango aceptado para estas pruebas: 0.04–3.0 m, con `RANGE_VALID`. El emisor puede informar hasta 4 m, pero el controlador usa el intervalo anterior, configurable.
- Antigüedad máxima: 250 ms. Secuencias repetidas no renuevan la validez de control aunque sigan llegando por radio. Se gestionan vuelta del contador y reinicio del emisor.
- Inclinación máxima: 20°. Se usa `abs(cos(roll) * cos(pitch))`; esto contempla que la IMU de este montaje informa roll cerca de π cuando está nivelada. La Flowdeck debe mirar al suelo; el signo de roll no sustituye esa comprobación de montaje.
- Mediana de tres rangos, filtro de altura con constante 0.08 s y estimación de pendiente con al menos cinco muestras y 160 ms de ventana; filtro de velocidad con constante 0.10 s.
- Saltos incompatibles con el movimiento configurado se rechazan. Tras una interrupción se exige nuevamente una ventana coherente.
- Al utilizar Flowdeck por primera vez se alinea y congela el origen local. Tras un respaldo, se recupera ese mismo origen a un máximo de 5 cm/s: no se hereda permanentemente la deriva del barómetro. Un regreso que contradice la altura local en más de 30 cm se rechaza, en lugar de redefinir silenciosamente el plano del suelo.
- Si se pierde un rango ya utilizado, hay un respaldo barométrico alineado de **hasta 2 s desde el último rango cualificado**. Después, o si tampoco hay barómetro válido, P08/P09 falla y desarma. El respaldo no se prolonga con paquetes duplicados o muestras sin cualificar.
- Si nunca hubo Flowdeck válida, el modo compatible conserva el comportamiento barométrico anterior. Para evitar iniciar una prueba creyendo que ya se está usando Flowdeck, el lanzador incorpora `--require-flow`.

La altura local mantiene el origen relativo de la prueba. **No confundirla con “altura al suelo”**, que es la medición vertical de Flowdeck. El montaje del sensor respecto al frame también afecta qué distancia física se está comparando.

## Emisor: compatibilidad con el código que compartiste

El receptor del blimp acepta tu broadcast actual, con el mismo paquete de 16 bytes y protocolo v1. **Para probar solo altura, tu emisor actual es compatible.**

Para transmitir también la calidad, añadir antes de `esp_now_send`:

```cpp
packet.quality = static_cast<uint8_t>(squal > 255u ? 255u : squal);
```

También quedó actualizado el proyecto emisor:

`C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\flowdeck_sender\FlowdeckEspNow\FlowdeckEspNow.ino`

Ahora acepta `FLOW,secuencia,dx,dy,distanciaMm,squal,motion`, conserva broadcast y envía `squal`. Sigue aceptando el formato antiguo `dx,dy,z` con calidad desconocida. Mantiene D7 RX y D6 TX sin uso para esta entrada. El canal configurable es 1 y debe coincidir con blimp/Ground. Rechaza líneas truncadas y valores fuera del formato, en lugar de transmitir datos envueltos por una conversión de tipo.

Calidad 0 no desactiva una altura ToF válida. X/Y cualificados requieren `squal >= 20`, secuencia consecutiva, intervalos de recepción entre 10 y 200 ms y velocidad angular ≤0.15 rad/s. Son umbrales provisionales para diagnóstico. No constituyen una validación del sensor original.

## Vista en la app

En la tarjeta Flowdeck se añaden:

- Fuente: **Flowdeck / Respaldo barométrico / Barómetro / Sin altura válida**.
- Altura local y altura al suelo.
- Velocidad vertical Flowdeck y velocidad utilizada por el estimador.
- Flujo X/Y en cuentas/s, cuando cumple los criterios de calidad.

La tarjeta indica “Estimador P08/P09” para no atribuir ese control a P04/P05. La altura general de la app y F1 continúan siendo el barómetro bruto. La calidad desconocida se muestra como “—”. La recepción F6 deja de mostrarse fresca cuando los mensajes de Ground han quedado atrasados.

## Carga y primera comparación, sin motores

La carga de la ESP del blimp ya se realizó correctamente. Para repetirla, usar USB solo fuera de la PCB y sin batería, como en las pruebas anteriores. En CMD:

```bat
"%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" run -d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\firmware" -e seeed_xiao_esp32s3 -t upload --upload-port COM4
```

La Nicla y el firmware de Ground no cambian. Reiniciar el bridge de Ground para cargar las etiquetas F18/F19. Para ver la nueva tarjeta en el teléfono, actualizar la app desde:

```bat
cd /d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\blimp_app"
C:\Users\stefa\flutter\bin\flutter.bat run
```

Con la electrónica montada, alimentación de batería y solo Ground por USB, en una ventana CMD:

```bat
cd /d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation"
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u telemetry_bridge.py COM9
```

En otra ventana CMD, **solo lectura**, sin órdenes de armado ni motores:

```bat
cd /d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation"
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u check_flow_height.py --seconds 60
```

Mantener quieto el sensor unos 15 s, elevarlo lentamente 20–40 cm, sostener otros 15 s y bajarlo. Comparar **altura al suelo** con la medida física del sensor; buscar Vz cerca de cero quieto, positiva al subir y negativa al bajar. Comparar a la vez con el barómetro. No hace falta helio ni vuelo para esta comprobación.

Logs de lectura:

`C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\logs\flow_height\*.jsonl`

## Ensayo actuado posterior

Una vez validado el rango y recuperado margen de sustentación, se puede comprobar altura/yaw de un waypoint sin avance:

```bat
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u check_single_waypoint.py --actuate --seconds 60 --vertical-only --require-flow
```

`--require-flow` exige rango cualificado antes de armar y vigila el respaldo durante la ejecución. La llegada y el avance siguen bloqueados en `--vertical-only`. No se ha ejecutado este ensayo como parte de la implementación.

## Telemetría y archivos

F18: `flow_fused_height`, `flow_fused_vz`, `flow_height_source`, `flow_height_filtered`, `flow_height_vz`, `flow_height_flags`.

Fuente: 0 ninguna; 1 barómetro previo a cualificar Flowdeck; 2 rango Flowdeck; 3 respaldo barométrico alineado.

Flags: 1 paquete reciente; 2 rango reciente cualificado; 4 ventana de rango lista; 8 altura utilizable; 16 respaldo; 32 barómetro sano; 64 salto rechazado; 128 flujo óptico cualificado; 256 calidad no disponible; 512 Flowdeck utilizada alguna vez; 1024 actitud dentro del límite; 2048 recuperación gradual del origen tras respaldo.

F19: flujo X/Y en cuentas/s, indicador de movimiento dependiente de altura (no m/s), edad del paquete, calidad recibida y secuencia. La secuencia completa se comprueba a bordo como uint32; su representación en telemetría float puede perder precisión después de 2²⁴.

Archivos principales, bajo la ruta absoluta del proyecto indicada al inicio:

- `firmware\src\mission\FlowHeight.h`: filtros, pendiente, secuencia y selección de fuente.
- `firmware\src\main.cpp`: adquisición sin alterar F1 ni los sensores originales.
- `firmware\src\mission\BalloonMission.cpp`: alimentación del regulador de P08/P09.
- `firmware\src\app\VisualSupport.h` y `firmware\src\mission\InertialHeight.h`: referencia fija y frenado.
- `firmware\src\app\Telemetry.cpp` y `groundstation\common\telemetry.py`: F18/F19.
- `groundstation\check_flow_height.py`: lectura sin motores.
- `groundstation\check_single_waypoint.py`: ensayo opcional `--require-flow`.
- `blimp_app\lib\widgets\telemetry_widgets.dart`: tarjeta en el teléfono.
- `groundstation\flow_v38_tests.sh`: pruebas nativas del filtro, integración real y regresiones.

Respaldo de fuentes anteriores: `docs\backup_before_flow38`.
