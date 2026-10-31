# Adquisición y uso de validaciones - versión28

Proyecto: C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware.

## Evidencia utilizada y límites

- `docs/VERTICAL_ESTIMATE_VALIDATION.md`: frame bloqueado, rango0,1036m y sigma0,0198m de altura estimada; velocidades aparentes -0,126..+0,132m/s. Son mediciones del estimador anterior, no presión cruda ni calibración de escala del BMP390.
- Usuario: caída física0,50m->0,18m en11s, aproximadamente -0,029m/s. La regresión verifica que el estimador conserva una rampa de esa velocidad; no se introduce una zona muerta que la elimine.
- Se conservan detector Nicla v5, umbrales/seguimiento, protocolo, pines, pose de servos, límites de potencia y ganancias P08 v27. La llegada sigue provisional en220px; no se convierte una proximidad visual en contacto físico.

## Problema de adquisición identificado

La librería Adafruit instalada llama a `bmp3_set_sensor_settings`, fuerza modoFORCED y lee registros cada `performReading()`. El proyecto lo llamaba cada20ms con presión16x/temperatura8x y ODR100Hz. El tiempo de conversión es aproximadamente49ms: esa llamada no acredita una conversión NUEVA cada20ms y puede interrumpir/reiniciar adquisiciones. No se afirma que ello sea la única causa física de los fallos de vuelo.

Se usa el driver Bosch ya incluido con esa dependencia mediante `Bmp390Sampler`: configuración única en modoNORMAL, presión8x/temperatura1x,25Hz, IIR1. Conversión máxima publicada21,53ms, menor que el período40ms. Se consulta data-ready de presión y temperatura y sólo entonces se leen datos compensados. Sin espera de conversión en el bucle de vuelo. Errores de bus o muestras inválidas no actualizan la antigüedad de la última muestra buena. Se mantiene referencia inicial; no hay auto-cero durante el vuelo.

Fuente primaria: Bosch BMP390 Datasheet, secciones3.3 y3.9, https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp390-ds002.pdf . API usada: bmp3.h/bmp3.c de la versión realmente instalada en firmware/.pio/libdeps/seeed_xiao_esp32s3/Adafruit BMP3XX Library.

## Cadena vertical única para P08/P09

1. Barometer390 acepta sólo una conversión nueva y fecha su recepción.
2. VerticalEstimate aplica mediana3, filtro temporal de altura0,20s, regresión limitada a los últimos500ms y filtro de velocidad0,15s. La ventana ya no se alarga a1s o más cuando baja la frecuencia de adquisición.
3. SensorSuite/NiclaSuite copian los valores sin reescalar. Se atiende la IMU aunque haya una muestra barométrica nueva; ya no se omite ese servicio por el simple hecho de haber actualizado presión.
4. InertialHeight consume la velocidad ya filtrada y su marca de tiempo. Elimina su filtro adicional250ms de velocidad. La aceleración para anticipación se calcula sólo al llegar una muestra nueva, usando el tiempo entre adquisiciones; se conserva su límite y filtro propios.
5. P08 conserva las prioridades visuales y el control v27. P04/P05 mantienen sus controladores anteriores: esta mejora no es una resintonización de sus PID ni garantiza toda la misión.

## Escala y configuración

`firmware/lib/BlimpSwarm/src/sense/VerticalProfile.h` concentra unidades SI, escala de altura, tiempos de filtros/ventana, límites de antigüedad y cadencia de adquisición. HEIGHT_SCALE permanece1,0. No hay pares de alturas físicas y presiones suficientemente identificados para ajustar otro factor sin confundir offset, deriva y escala. Una lectura2,66m al encender frente a una altura física distinta no justifica multiplicar por una razón.

`SensorInterface::getPreferences()` tiene implementación vacía por defecto; elimina un hook virtual heredado declarado sin definición y no altera las implementaciones de las suites.

## Observación y comprobación sin motores

F1/F9 conservan su formato. F14 añade: baro_period_ms, baro_window_ms, baro_height_scale, baro_source_vz, baro_sample_age_ms, vertical_profile_version. Reiniciar Ground para cargar las etiquetas nuevas. Se espera un período cercano a40ms, ventana<=500ms y perfil28; esos valores deben medirse en el hardware.

Con modo0/armed0, Ground en COM9 y el frame realmente inmóvil, ejecutar `groundstation/03_VALIDAR_BAROMETRO.cmd`. Sólo envía autenticación viewer: no ordena reset, modo, armado ni potencia. Guarda30s de F9/F14 y calcula rango, dispersión, deriva aparente, velocidad y cadencia en logs/vertical_baseline. La condición inmóvil la declara el operador; no la adivina el programa.

Después se requiere subida/bajada física medida y pausa, para comprobar signo, magnitud y tiempo de respuesta antes de otro vuelo. No se afirma reducción de ruido ni mejora de escala física por pasar pruebas sintéticas.

## Pruebas de software

- Adaptador y Barometer390 reales con bus/Bosch simulados: configuración única, modo normal, data-ready, muestras repetidas, I2C fallido, invalidación por antigüedad y reinicio de derivada tras hueco.
- Estimador/control reales: caída lenta observada preservada en rampa sintética, ventana temporal a frecuencia irregular, muestra única fechada y sin segundo filtro de velocidad.
- Pasaron regresiones de misión P08, PARTIAL, prioridad visual de altura, anticipación vertical y predicción de yaw; las siete comprobaciones del detector de producción Nicla también pasan.
- El estimador conserva sus comprobaciones de ruido sintético, escalón, picos, timestamps repetidos, rollover, huecos y deriva. Las cifras sintéticas no se presentan como mejora medida del sensor real.

Estado actualizado: versión28 cargada el28 de octubre de2026 en ESP COM4, MAC dc:b4:d9:39:b3:b4. Primer intento interrumpido por desconexión USB; segundo intento a115200 completado: 834864bytes escritos, hash verificado, PlatformIO SUCCESS75,68s. RAM48520bytes, Flash834497bytes. Las pruebas reproducibles del adaptador, cadena y estimador pasan desde groundstation/vertical_host/run_tests.sh. No se armaron motores. Falta validación física del sensor: primero banco desarmado, después movimiento medido; no una prueba de vuelo directa. La última carga verificada ahora es28. Nicla no se modificó.
