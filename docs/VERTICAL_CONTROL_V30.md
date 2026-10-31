# Control vertical30: desplazamiento breve y referencia visual

Proyecto: C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware

## Correcciones

`firmware/src/mission/VerticalMotionEvidence.h` conserva la vía lenta de29 (consistencia de velocidad, sin zona muerta que borre−0,029m/s) y añade una vía para desplazamiento breve. Con muestras nuevas de altura filtrada y velocidad, usa una ventana<=1,2s, mínimo5 muestras y600ms; compara medianas de los extremos de la ventana y ajusta una recta para estimar su dispersión. Requiere desplazamiento>=0,12m, coherencia>=0,65, sentido compatible con velocidad y pendiente, tasa<=0,6m/s y200ms continuos de evidencia. No exige velocidad constante ni400ms extra del criterio lento. La confianza es una regla heurística, no una probabilidad. Altura y velocidad provienen del MISMO barómetro: no son sensores independientes.

`InertialHeight.h` usa la tasa de la vía aceptada; conserva la vía visual inmediata y el veto cuando visión fiable contradice presión. La vía rápida no enseña por sí sola un nuevo trim de sustentación. `VisualSearch.h` y `VisualSupport.h` reciben altura y timestamp reales, manteniendo límites y esquema de potencia existentes.

`firmware/src/app/VisualSupport.h` refresca la referencia de CONTROL, sin poner a cero ni reescalar el sensor. Captura inicial tras500ms y>=5 muestras con globoFULL, x±24px/y±10px, bordes verticales libres, pitch<=10°, yawRate<=0,08rad/s, derivada visual<=0,08/s y dispersión visual x<=8px/y<=6px. Mediana de hasta16 alturas; después ajusta lentamente (tau2s, máximo0,02m/s). Refresco bloqueado durante impulsos (learningAllowed=false), pérdida, PARTIAL, giro, inclinación excesiva, centramiento inestable o muestras cacheadas. Al recuperar visión vuelve a exigir estabilidad; dt de actualización limitado a250ms para evitar un salto.

La actualización se integra en el camino experimental P08/P09 y consumidores de InertialHeight/VisualSupport. No implica resintonizar las rutas antiguas P04/P05 o toda la misión de pista. Escala1,0, Nicla, pines, geometría/ángulos de servos, yaw, máximos de potencia y llegada provisional por tamaño se conservan.

## Evidencia y límites

Reproducción C++ de tres capturas estáticas:0/150 confirmaciones en cada una, tanto29 como30. En los cuatro movimientos manuales declarados válidos se acepta subida y bajada en30;29 había omitido al menos una dirección en tres de ellos y ambas en el último. Detalle en `docs/vertical_replay_v30.json`.

Último80cm `20261028_173305_870815.jsonl`:29 acepta0 enup y0 endown;30 acepta2 enup y6 endown, además de3 muestras de transición al empezar la pausa alta. No hay aceptaciones en los últimos4s de las pausas. Son muestrasF9 reducidas a~5Hz y fases que incluyen la voz; no medir la demora física a partir del inicio del aviso. No certifican decisiones idénticas a25Hz ni estabilidad de vuelo.

Ensayo nativo con VerticalEstimate real a25Hz, subida0,8m/2s y bajada0,8m/2s: vía rápida acepta ambos sentidos a1160ms desde el inicio real del movimiento. Caída sintética−0,32/11m/s aceptada a1040ms por la vía lenta. Ruido oscilante y pico aislado no confirman movimiento. Son simulaciones, no mediciones nuevas del blimp.

Pruebas de referencia: sigue deriva lenta únicamente con imagen centrada estable; se congela durante pérdida, PARTIAL, giro, pitch, impulso y frames repetidos; no salta al recuperar visión. Pasaron regresiones de sustentación, control vertical, prioridad visual, misión real P08, PARTIAL, predicción yaw, impulsos/llegada y P03. Se corrigieron expectativas obsoletas de `test_visual_support.cpp` que ya fallaban en29: trim fijo pese a aprendizaje visual, freno retenido indefinidamente con presión inválida, y aprendizaje sin límite pese a saturación. Se conservan comprobaciones de límites y respuesta al movimiento sostenido.

Una deriva coherente aún puede parecer movimiento físico. Esta versión no mide suelo/techo, contacto, flotabilidad ni velocidad independiente. El ruido de la lectura de altura puede seguir presente. El anclaje visual solo ayuda mientras haya una referencia visual fiable; durante pérdida prolongada no puede cancelar la deriva sin otra medición.

## Diagnóstico a bordo sin motores

F11 identifica control30; F14 sigue indicando adquisición28, cuyo driver no cambió. F13 conserva sus6 campos; bit256 añade vía rápida. F15 nuevo muestra el estado de una instancia de SOLO diagnóstico de VerticalMotionEvidence, alimentada desde sendCore en cada bucle y solo con timestamp nuevo (cadencia del sensor~25Hz): tasa aceptada, confianza, flags, desplazamiento de ventana, contador de eventos de confirmación y versión30. FlagsF15:1=lenta,2=rápida,4=historial quieto,8=dato válido. Este diagnóstico no toca control, modo o actuadores. Se publica cada200ms; el contador registra confirmaciones breves que podrían ocurrir entre paquetes.

Reiniciar Ground para cargar etiquetasF15. `groundstation/check_vertical_motion.py --height-cm 80` sigue siendo viewer/desarmado; ahora añade `onboard_motion_v30` al resumen, por fase: muestras observadas, confirmaciones, eventos y tasas. No declara automáticamente que el movimiento físico fue correcto. La app sigue usando paquetes anteriores; no hace falta recompilarla para este banco. El lanzador de vuelo `check_single_waypoint.py` exige>=30 para evitar ensayar por accidente el comportamiento29.

Pruebas reproducibles: `bash groundstation/vertical_v30_tests.sh` en WSL/g++. Reproducción auxiliar C++: `groundstation/replay_vertical_v30.cpp`, stdin `elapsed_us,phase,height,vz` (sin puertos ni motores).

Estado: cambios guardados y verificados por hash en Competencia. Compilación PlatformIO SUCCESS18,59s; RAM50968bytes, Flash840441bytes. Las11 pruebas del runner final pasan. Carga realizada el28 de octubre de2026 en COM4, ESP32-S3 MACdc:b4:d9:39:b3:b4:840800bytes escritos, hash de escritura verificado, PlatformIO SUCCESS17,13s y reinicioRTS. No se activaron motores. Falta confirmar arranque30/F15 con Ground reiniciado y repetir el movimiento manual de80cm, antes de validar vuelo. Nicla y app no requieren carga para este ensayo.
