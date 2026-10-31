# v36: referencia visual y ayuda local contra deriva barométrica

2026-10-29. El setpoint visual sigue siendo el centro vertical y=80 px en
imagen240x160. La referencia de llegada80cm/120px es un parámetro distinto.

## Qué cambia en el P08/P09

La altura y velocidad de SensorSuite no se reinician, escalan ni sustituyen.
La mejora opera solamente en el controlador de aproximación:

1. Sigue capturando referencia cuando el waypoint está FULL, centrado, estable,
   con pitch/giro bajos, >=500ms y >=5 muestras nuevas. La mediana de16 muestras
   como máximo conserva el mecanismo previo de actualización lenta.
2. Calcula una banda de incertidumbre a partir del MAD de esas alturas:
   `max(0.03m,min(0.12m,3*1.4826*MAD))`. No es una probabilidad ni una exactitud
   certificada. El error posicional dentro de la banda no solicita corrección.
3. Aprende una deriva pequeña solo entre observaciones visuales estables:
   duración>=2s, >=8 muestras, y varía<=2px, ancho<=5%, pitch<=0.015rad.
   Una pendiente de presión mayor de0.01m/s se rechaza como hipótesis de deriva;
   la corrección aprendida queda limitada a +/-0.005m/s (5mm/s).
4. Durante impulsos se suspende el aprendizaje. Puede comparar anclas estables
   antes/después si recupera la misma geometría en menos de8s. Pérdida visual,
   recorte, movimiento de imagen o ruido elevado descartan la ventana candidata.
5. Durante una recuperación breve proyecta la referencia con esa deriva y
   descuenta la deriva aprendida de la velocidad usada localmente por el
   gobernador. La proyección caduca en4s: después no persigue posicionalmente
   una altura de presión envejecida. La sustentación y la evaluación de
   velocidad siguen activas según sus reglas existentes.

La caída física medida por el usuario,32cm en11s (~0.029m/s), queda fuera de la
hipótesis de deriva. No se introduce una zona muerta de velocidad que borre
esa caída. La banda de3cm corresponde al error de POSICIÓN, no a velocidad.

La corrección depende de un waypoint inmóvil y una cámara estable. No elimina
la deriva cuando falta referencia visual ni garantiza altura absoluta respecto
al suelo. El pitch se controla como condición de aprendizaje; aún no hay una
compensación geométrica calibrada del movimiento de imagen por pitch.

## Archivos

- `firmware/src/mission/VisualAltitudeAid.h`: límites, estimador de deriva,
  banda de incertidumbre y envejecimiento.
- `firmware/src/app/VisualSupport.h`: captura de referencia e integración local.
- `firmware/src/mission/BalloonMission.cpp`: diagnósticoF16 para P08/P09.
- `firmware/src/app/Telemetry.{h,cpp}`: emisiónF16 a5Hz.
- `groundstation/common/telemetry.py`: nombres de los camposF16.
- `groundstation/check_single_waypoint.py`: impresión de referencia válida,
  error relativo y deriva estimada; exige firmware>=36 antes de armar.

## Datos nuevos F16

| Campo | Significado |
|---|---|
| height_anchor | Referencia barométrica local proyectada, no altura al suelo |
| height_anchor_error | Error tras banda de ruido;0 también cuando no es utilizable |
| height_anchor_age_ms | Tiempo desde la última referencia estable; -1 si no hay |
| baro_drift_rate | Deriva aprendida válida en m/s;0 si no hay estimación vigente |
| height_noise_band | Banda de incertidumbre en metros |
| height_reference_flags | Bits de validez descritos debajo |

Flags:1=capturada,2=utilizable para corrección posicional,4=deriva vigente,
8=ventana visual candidata activa,16=ruido elevado. Leer siempre la validez:
un error0 sin el bit2 NO confirma altura correcta. Una referencia puede haberse
capturado pero quedar envejecida. Sin ancla, no se fabrica altura del waypoint.

F1/F9/F14 y el shadowF15 mantienen los valores del sensor. F13 refleja la
velocidad usada localmente tras la compensación cuando existe una deriva
vigente. Flutter acepta campos nuevos sin cambiar su formato; no se añadió
una tarjeta de pantalla. El detalle está en Ground, consola y registros.

Para interpretarF16 hay que reiniciar Ground con el código actualizado. No
requiere reinstalar Nicla ni app. Se requiere cargar el firmwarev36 en la ESP.

## Verificación y alcance

`groundstation/test_altitude36.cpp` comprueba deriva simulada4mm/s con visión
estable, caída real de referencia29mm/s, caducidad, ruido, pitch, cambio de
escala de imagen, suspensión entre impulsos y rollover. Ejecutor:
`groundstation/altitude_v36_tests.sh`; incluye las suites previas de motor,
avance, llegada, recuperación y sensor. El supervisor Python tiene14 pruebas.

Se conservan el mapa lógico de motores v35, ataque invertido28°, máximo25%,
semilla18%, llegada estimada120px, memoria de avance0.8s y corte estimado de
batería suspendido. Las condiciones existentes de altura en llegada/avance
recortados no se sustituyeron por el diagnóstico de referencia nueva.

Cargado en COM4 el 2026-10-29, MAC DC:B4:D9:39:B3:B4; 844352 bytes escritos, hash verificado, reinicio RTS y PlatformIO SUCCESS en17.48s. Pendiente de validar físicamente. Es necesario reiniciar Ground para interpretar F16. La sincronización del primer impulso con la cuenta de voz
detectada en el ensayo anterior sigue pendiente; esta mejora no modifica esa
secuencia del lanzador.

Verificado: compilación final PlatformIO SUCCESS (16.54 s), RAM51128 y
flash843985 bytes;14 pruebas Python y la suite nativa completa correctas.
Los resultados de precisión son simulados; no se ha medido todavía el cambio
de error de altura con el blimp real.

