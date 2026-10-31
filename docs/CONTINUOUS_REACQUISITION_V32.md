# Readquisición continua — firmware32 / Nicla7

Proyecto: C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware

## Error concreto

En Nicla6, `expired=True` dirigía todos los frames posteriores a `recover()`, pero las condiciones de aceptación dejaban de cumplirse al superar5s paraFULL o10s para recortes. Resultado: no había ninguna vía para reconocer de nuevo, aunque el globo volviera, salvo resetR/reinicio. Era un bloqueo del reconocimiento, no una desconexiónUART. El bucle seguía transmitiendo frames vacíos y sample cambiante; por eso podía verse `nicla_fresh=1` junto con estadoLOST.

La interrupción completa de F1/F3/F7 observada en el banco es otro fenómeno. Esta corrección no demuestra resolver un corte de alimentación, radio o Ground.

## Nicla7

Fuente: `vision/n03_panel/nicla_balloon_mission_main.py`, VERSION `N03-panel-v7-reacquire`.

- Sustituye el concepto de expiración terminal por recuperación activa. Tras2s sin observación informa SEARCH; mientras evalúa un candidato informa CONFIRM. LOST6 queda en la tabla solo por compatibilidad y ya no es un estado emitido por esta lógica de producción.
- UnFULL compatible o un recorte compatible con la semillaFULL previa puede volver a confirmarse sin plazo máximo de bloqueo. Después de10s se exige1s de confirmación, frente a500ms para recuperación reciente. No se reutilizan coordenadas viejas como observaciones actuales.
- Si cambia la posición o escala y ya no encaja con el anchor, un candidato único que cumpla las reglas originales de adquisición puede reconocerse tras1s estable. No selecciona arbitrariamente entre dos. Una región roja casi completa no obtiene adquisición inicial nueva.
- Una nueva asociación pierde la pruebaFULL/semilla cercana anterior e incrementa su generación de asociación. Una recuperación compatible del mismo track conserva esa generación. Esto es asociación geométrica, no identidad física garantizada.
- Se mantiene el envíoUART20Hz mientras no haya globo. La conexión no depende de FULL/PARTIAL.

## Separación entre reconocer y aceptar un waypoint

El canal sample existente tiene16bits:8 inferiores de contador de frames y8 superiores de generación de asociación. El rollover del contador no cambia la generación. Protocolo0x4E03,32bytes, flags65/66, geometría, distancia9999 y resetACK explícito no se alteran. No se finge un ACK de reset al readquirir.

ESP32 detecta una nueva asociación. Conserva rumbo/altura y número de waypoint; cancela avance/confirmación de llegada y exigeFULL dentro de±20° del rumbo esperado antes de aceptar esa asociación como el objetivo actual. Un PARTIAL nuevo no hereda la confirmaciónFULL del globo anterior. La recuperación compatible de un recorte conocido no necesitaFULL de nuevo. Se aplica la puerta de reasociación tanto a P08/P09 como a las rutas antiguas de varios waypoints; esto no supone integrar todo el control experimental en P10/P11.

El paso al SIGUIENTE waypoint sigue siendo una decisión de la misión, después de llegada/giro, no una consecuencia de que la cámara reconozca otra región.

## Límites que siguen existiendo

El detector sigue buscando indefinidamente, también con la misión parada. La misión mantiene su plazo de15s sin objetivo y la vigilancia de comunicación1,5s; estas paradas de actuadores no bloquean el reconocimiento ni lo rearman automáticamente. Se requiere un inicio de misión explícito después de una parada. No se mide contacto físico y la llegada sigue provisional.

No se cambian pines, potencia máxima, servos, ganancias de yaw, umbralLAB ni filtrado barométrico30. F11 identifica control32; F14/F15 continúan siendo versiones28/30 de adquisición/evidencia vertical.

## Comprobaciones

19 pruebas Python de producción: las13 previas más retornoFULL tras60s, retorno del recorte tras60s, nueva posición/escala, PARTIAL nuevo sin herenciaFULL, ambigüedad persistente y empaquetado de generación/contador sin falsificar resetACK.

Prueba C++ real de misión: rechaza asociación fuera del rumbo, bloquea PARTIAL nuevo, aceptaFULL en rumbo esperado, ignora rollover de frame y cancela llegada al cambiar asociación. También cubre la puerta en el camino de varios waypoints. Se mantienen regresiones de cercanía, misión, impulsos, PARTIAL y yaw.

El ensayo `groundstation/check_near_tracking.py` registra las generaciones al verFULL, recorte y recuperación: no da por válida continuidad si el detector cambió de asociación. Usar el modo normal sin reset; el reinicio manual no debe ser necesario para que Nicla7 vuelva a reconocer.

Reproducir Python: `vision/n03_panel/test_production_acquisition.py`, `test_production_near.py`, `test_production_reacquire.py`. WSL: `bash groundstation/reacquire_v32_tests.sh`.

Estado: corregido y guardado en Competencia,19 pruebas Python y regresiones C++ aprobadas. Compilación PlatformIO SUCCESS16,68s; RAM51000bytes/Flash842281bytes. Firmware32 cargado el28 de octubre de2026 en COM4/MACdc:b4:d9:39:b3:b4:842640bytes escritos, hash de escritura verificado, SUCCESS18,24s y reinicioRTS. SHA256 firmware.bin:708F6030C6C81A27796B19C47E5A0640044CBBD0172B28CDB421A27A5E3D232B.

Nicla USB COM10 identificada; Nicla7 guardada enE:\main.py,18263bytes, fsync y lectura idéntica al fuente. SHA256:3E930D6806CEDE7FF655853296C7BC9D36BA26C9EC16F31E655CA317419A2DD5. Copia anterior en `vision/backups/nicla_main_pre_reacquire_20261028_190847_817070.py`; recibo en `docs/nicla_v7_upload_receipt.json`. Requiere reiniciar Nicla para ejecutar el nuevo archivo. Validación física pendiente; no se enviaron órdenes de armado.
