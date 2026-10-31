# Confianza vertical — versión29

Proyecto: C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware

## Problema medido

Capturas desarmadas de30s del28 de octubre de2026:

| Registro en logs/vertical_baseline | Rango altura filtrada | Sigma altura | Sigma velocidad | Cambio medianas inicio/final |
|---|---:|---:|---:|---:|
| 20261028_143801_276688.jsonl | 0,3244m | 0,0655m | 0,0969m/s | −0,1562m |
| 20261028_143922_032905.jsonl | 0,2431m | 0,0404m | 0,0959m/s | +0,0100m |

Se declaró frame inmóvil en el programa; no se verificó físicamente mediante otro sensor. La adquisición permaneció cerca de40ms y saludable. Saludable significa muestra válida/reciente, no movimiento demostrado. En28, casi cualquier velocidad fresca de magnitud<=0,6m/s entraba al control; la condición de frenado comenzaba cerca de0,035m/s.

## Corrección aplicada

`firmware/src/mission/VerticalMotionEvidence.h` separa validez de evidencia de movimiento:

- Memoria fija40 muestras; ventana máxima1,2s, mínimo5 muestras y600ms de cobertura. Se procesa una sola vez cada timestamp de adquisición. Huecos>300ms/invalidación reinician evidencia.
- Mediana y dispersión absoluta mediana(MAD) de la velocidad que ya entrega el sensor. Reduce la influencia de excursiones aisladas en la corrección; no vuelve a convertir presión ni redefine la altura.
- Score = abs(mediana)/(abs(mediana)+3*MAD+0,002), umbral0,65, al menos80% de signos coincidentes. Es una regla de coherencia, NO una probabilidad estadística: las muestras están correlacionadas por la regresión500ms previa.
- La condición debe persistir400ms adicionales. Un cambio de signo en la muestra nueva revoca inmediatamente la evidencia anterior. Una velocidad constante se acepta a1040ms con adquisición40ms. Esto agrega demora deliberada al barómetro solo.
- No hay zona muerta de±0,24m/s. La caída sintética−0,32/11m/s sigue generando incremento de sustentación tras confirmación.

`firmware/src/mission/InertialHeight.h`:

- Conserva `rate` como velocidad de entrada para diagnóstico. `controlRate` usa la mediana cuando hay evidencia solo barométrica; pondera la corrección con el score.
- La aceleración barométrica no anticipa potencia por sí sola. Se conserva para anticipación rápida únicamente cuando hay movimiento visual fiable en el mismo sentido. Una contradicción visual deshabilita la corrección barométrica; sigue disponible la corrección visual.
- La posición visual y su ajuste de sustentación persistente siguen activos aunque el barómetro no confirme movimiento.
- Aprendizaje de sustentación solo barométrico requiere evidencia sostenida o un historial de baja dispersión; se bloquea durante ruido/invalidez. Con pérdida visual y sin tendencia de presión, conserva el trim aprendido sin mantener indefinidamente una corrección negativa antigua. No constituye mantenimiento garantizado de altura.

Ámbito: control experimental P08/P09 y módulos que usan InertialHeight. No se resintonizaron todos los PID de P04/P05 ni las rutas antiguas de misión completa. No se cambiaron pines, Nicla, umbrales visuales, potencia máxima, geometría de servos, yaw ni criterio provisional de llegada.

## Telemetría

Versión de control enF11:29. F13 conserva6 campos y usa el sexto antes reservado como `inertia_velocity_confidence`,0..1. Flags anteriores se conservan; bit64=tendencia de presión confirmada, bit128=movimiento visual corroborado. `inertia_vz` sigue siendo entrada; `inertia_predicted_vz` refleja la tendencia realmente usada. Reiniciar Ground para importar las nuevas etiquetas; la app anterior no necesita recibir un paquete distinto.

F14 continúa indicando perfil de adquisición28: el driver/estimador del sensor no cambió en29. La altura filtrada puede seguir variando en el banco; esta corrección mejora su uso en control, no la precisión física del barómetro.

## Comprobaciones

Pruebas nativas: ruido oscilante/aislado, caída lenta−0,0291m/s, respuesta visual inmediata, contradicción visual, muestras repetidas, huecos/invalidez, reversión y rollover. Regresiones del controlador vertical y misión P08/PARTIAL/yaw pasan. Modelo simplificado de sobrepaso: posición sola0,236m; nuevo control0,092m (28 daba0,038m). El aumento respecto a28 ilustra el costo de confirmar presión durante~1s; NO es predicción física ni resultado de vuelo.

Reproducción de ambos logs por el controlador C++ real, error visual0, sin aprendizaje de trim:28 acepta150/150 muestras por registro y produce correcciones RMS de0,0354/0,0359 de potencia;29 confirma0/150 y no produce corrección barométrica en ese escenario. Es telemetría F9 reducida a~5Hz; NO reconstruye las25Hz originales, no demuestra rechazo idéntico a bordo y no certifica ausencia de movimiento real. Comparación guardada en `docs/vertical_confidence_replay_v29.json`.

Ejecutar en WSL: `bash groundstation/vertical_host_run_confidence_tests.sh`. El comparador compilado admite por stdin filas `elapsed_us,filtered_vz`; no abre puertos ni envía comandos.

## Límites y siguiente comprobación

Una deriva lenta y coherente de presión todavía puede confundirse con desplazamiento. No hay medición de suelo, techo, contacto o flotabilidad. Una caída real lenta mezclada con ruido grande puede tardar más o no superar el score; la rampa limpia no prueba detección bajo todo ruido. No ajustar escala ni sustraer deriva usando estas capturas.

Antes de vuelo: subir/bajar físicamente una distancia conocida con pausas, verificar signo, magnitud y demora; luego observar el control con imagen fiable. Los logs F9 de5Hz no permiten elegir con rigor un filtro nuevo para presión a25Hz. Los parámetros de coherencia están concentrados en VerticalMotionEvidence.h para iteración; no aumentar umbrales arbitrariamente para que una captura pase.

Estado: compilado y comprobado desde Competencia. PlatformIO SUCCESS64,23s; RAM49656bytes, Flash837221bytes. Pasaron las siete pruebas nativas del runner y se reprodujeron ambos registros con el código final. Advertencias heredadas de redefinición PI; sin errores de compilación. Cargado el28 de octubre de2026 en ESP COM4, MAC dc:b4:d9:39:b3:b4:837584bytes escritos, hash de escritura verificado, PlatformIO SUCCESS15,87s y reinicio RTS. SHA256 del firmware.bin local: B5A5CDE2E20D5E4497494000739AE2A9BE30893921E20B9AEA34A6D43DEB63CB. No se activaron motores. Falta comprobar arranque/telemetría29 y movimiento físico medido desarmado antes de validar vuelo. Nicla no cambió. Reiniciar Ground para las nuevas etiquetas F13.
