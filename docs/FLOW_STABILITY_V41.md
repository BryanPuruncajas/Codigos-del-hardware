# Estabilidad de altura Flowdeck - v41

Fecha: 2026-10-31. Proyecto de Competencia:
`C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware`

## Problema observado

En el ensayo `logs\single_waypoint\20261031_101223_011120.jsonl` hubo paquetes recientes (hasta 26 ms) con rango marcado válido, pero saltos abruptos de distancia. La inclinación registrada estuvo entre 6,67 y 13,25 grados. El usuario confirmó vista libre al suelo. No está demostrada la causa de esos saltos.

El filtro v40 guardaba valores crudos antes de decidir si rechazaba su mediana. Un pico podía contaminar muestras posteriores. Además, al superar 250 ms sin una altura calificada, reiniciaba su ventana y la misión se detenía durante la recuperación.

## Cambios

- `firmware\src\mission\FlowHeight.h`: descartar un salto crudo antes de incorporarlo al historial. Mantener el margen existente de 8 cm más 1 m/s por el tiempo desde la última muestra aceptada.
- Conservar brevemente la última altura calificada hasta 500 ms únicamente si siguen llegando muestras con secuencia nueva y antigüedad no mayor de 250 ms, y la inclinación sigue permitida. No se crea una altura nueva ni se renueva el tiempo de la medida real.
- `holding` y bit 8192 identifican altura conservada; `recoveryHold` y bit 16384 identifican la parte de recuperación posterior a 250 ms.
- `firmware\src\mission\BalloonMission.cpp`: pausar avance y confirmación de llegada mientras la altura está conservada; congelar aprendizaje de sustentación durante ese intervalo. El sostén y yaw siguen dentro de los límites existentes.
- `groundstation\check_single_waypoint.py`: informar altura conservada y recuperación, y detener si aparece avance durante conservación.
- `firmware\src\app\VisualBearing.h`: identificador de firmware 41.

## Umbrales

| Parámetro | Valor | Comportamiento |
|---|---:|---|
| `FRESH_MS` | 250 ms | Caducidad de comunicación/muestras repetidas. |
| `RECOVERY_MS` | 500 ms | Máxima conservación con paquetes nuevos pero rango sin calificar. |
| `RANGE_JUMP_BASE_M` | 0,08 m | Margen base de cambio de rango. |
| `RANGE_JUMP_RATE_MPS` | 1 m/s | Margen adicional según tiempo transcurrido. |
| Rango de control | 0,04 a 3 m | Sin cambios. |
| Inclinación de medida | 20 grados | Sin cambios; corrección existente por roll/pitch del IMU. |
| Potencia automática | Hasta 25% | Sin cambios. |

Una ausencia sostenida de rango, pérdida de comunicación o inclinación fuera de límite siguen invalidando la altura. Un `65535` del productor sigue siendo inválido. Esta mejora no repara bloqueos del productor Crazyflie ni pérdidas UART.

## Validación

- Pruebas nativas del filtro y misión: picos, retorno al suelo original, falso suelo persistente, descenso real, balanceo con IMU, muestras repetidas, caducidad y ausencia de barómetro. Pasan las pruebas de rutas de motores y límites.
- Comparación sintética: 18 actualizaciones inválidas en el filtro previo frente a 0 con el candidato ante un intervalo de retornos falsos de 320 ms.
- Reproducción del log muestreado tras calificación: 3 registros inválidos con el filtro previo frente a 0 con el candidato. Es una reproducción de telemetría reducida; no reconstruye todas las adquisiciones ni demuestra el comportamiento de vuelo.
- Suite Python: protecciones de waypoint y puentes, incluida pausa de avance durante altura conservada.
- Compilación PlatformIO correcta: RAM 50936 bytes, flash 839449 bytes.

No se realizó vuelo con v41 ni se cargó todavía en la ESP al preparar este documento. La prueba física es obligatoria para confirmar la mejora.

## Alcance y carga pendiente

Se mantiene exclusivamente Flowdeck como fuente vertical. No cambia el pinout de las PCB, geometría de servos, control yaw ni firmware de Nicla, Ground o Crazyflie.

El binario usa el código actual del proyecto, que también contiene el ajuste P02 manual preparado anteriormente: 30% por defecto y máximo ajustable de 50%; la misión automática continúa limitada al 25%.

Para cargar v41, retirar la ESP principal de la PCB, desconectar batería y usar únicamente USB. Identificar MAC `DC:B4:D9:39:B3:B4`; no confundirla con la emisora Flowdeck `7C:4F:AD:1D:CA:D4`.

Después de la carga, montar sin USB y alimentar por batería. Ground permanece por USB. El mismo comando `check_single_waypoint.py --actuate --seconds 120` usa las nuevas indicaciones y protecciones; no necesita recompilar la app para este cambio de altura.
