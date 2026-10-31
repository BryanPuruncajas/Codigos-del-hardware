# Seguimiento de recortes cercanos: ESP31 / Nicla6

Proyecto: C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware

## Problema y alcance

Nicla5 seguía parciales compatibles mientras no se perdiera la asociación, pero su recuperación exigía FULL. Un globo que ya excedía el campo de visión podía no volver a ser recuperado. Además, el centroide del recorte puede desplazarse dentro de la imagen sin representar el centro real del globo.

Se actualiza el standalone de producción `vision/n03_panel/nicla_balloon_mission_main.py` (VERSION N03-panel-v6-near), y el control experimental de aproximación P08/P09 (`BalloonMission::updateN03Approach`). La nueva interpretación de recortes en control NO se anuncia como integración completa de las rutas antiguas P10/P11. Nicla sigue ofreciendo geometría bruta PARTIAL con el mismo protocolo para todos sus consumidores. No se cambian pines, conexión UART115200, umbralLAB, ganancias de yaw, posiciones de servos ni máximos de potencia. Se conservan las dos vías barométricas30.

## Nicla: memoria de asociación

- Una confirmación FULL de tamaño mínimo80px de ancho o64px de alto, dentro de una zona central amplia, habilita una semilla de cercanía. No se obtiene de un PARTIAL inicial.
- Con esa semilla, un recorte puede continuar el track si toca el borde, tiene ancho>=40px/alto>=30px,>=160 píxeles rojos, relleno>=0,35, dimensiones entre0,4 y2,5 veces la observación anterior, suficiente solapamiento y al menos30% de sus píxeles. Puede aceptarse un recorte D por métricas de forma truncadas; no se promueve un D a objetivo nuevo.
- Tras perderlo, un recorte único compatible puede recuperarse durante un máximo10s, con5 frames y500ms de confirmación. Se corrigió un salto que permitía abandonar esa confirmación al frame siguiente volviendo a la asociación ordinaria.
- Dos candidatos compatibles producen AMBIG; no se elige el mayor. Un cuadro rojo casi completo sigue prohibido como adquisición inicial. Un resetR crea otro Tracker y borra la semilla.
- Sin observación actual se envía flag0; nunca se reutilizan coordenadas antiguas como si fueran una medida nueva. La memoria no demuestra identidad física: es continuidad geométrica/temporal de regiones del mismo color.

## ESP: aproximación cercana

- Activa el tratamiento cercano solo después de haber visto FULL en esta ejecución de misión, y con PARTIAL grande (ancho>=120px o alto>=112px). Los parciales pequeños conservan el comportamiento previo de aproximación.
- Usa los límites reales del blob que llegan en los canalesROI, separados de su centroide. Un recorte cercano no cambia la referencia de altura ni genera una derivada visual de su centroide.
- Si la región abarca el centro de imagen, conserva el rumbo del último objetivo y regula usando la referencia de altura ya capturada y el barómetro. Si la región queda completamente a un lado, corrige el rumbo solo por ese lado conocido, limitado a3°/s. Si queda completamente arriba/abajo, usa una corrección direccional pequeña; no inventa un centro ni una distancia.
- Para continuar los impulsos cerca exige referencia de altura válida/reciente, diferencia<=0,20m, región que abarque el centro de imagen y rumbo/velocidad angular dentro de las puertas existentes. Sin referencia suficiente, no da avance cercano por memoria solamente.
- Una pérdida no permite avance. Conserva rumbo/altura, sin barrido de yaw de la recuperación cercana. No resetea a los5,2s el Tracker que aún puede recuperar la semilla cercana; el plazo terminal de pérdida visual15s y la vigilancia de comunicación1,5s siguen activos.
- Una vez reseteado explícitamente un tracker normal se borra el históricoFULL de la ESP para evitar que una nueva adquisición se beneficie de la confirmación del objetivo anterior.

## Llegada y telemetría

El criterio de llegada sigue siendo PROVISIONAL/ESTIMADO: ancho>=220px, históricoFULL y confirmación de500ms con3 frames nuevos. En recorte cercano, el alineamiento se comprueba con cobertura de imagen y rumbo/altura retenidos, no con el centroide recortado. Esto permite terminar esa prueba sin exigir FULL de nuevo, pero NO mide contacto físico.

UART conserva32bytes, protocolo0x4E03 y flags0x41 FULL/0x42 PARTIAL. No hace falta nueva app para recibirlos. F11 identifica ESP31; F13 bit512 identifica recorte cercano en P08/P09. F15 sigue indicando algoritmo barométrico30 y F14 adquisición28: no son versiones contradictorias. El lanzador `check_single_waypoint.py` exige>=31 antes de permitir su ensayo actuado.

## Comprobación

13 pruebas de producción Nicla pasan: las7 anteriores y6 nuevas de crecimiento/recorte, pantalla casi llena, recuperación parcial tras pérdida, límite10s, ambigüedad/región desplazada, ausencia de semillaFULL y paquetes sin coordenadas antiguas.

Pruebas C++: centroide recortado no sesga altura/rumbo; corrección solo por borde; referencia conservada; P08 real continúa impulsos con recorte grande, conserva asociación tras6,2s sin observación y completa el criterio estimado de llegada. Con altura incorrecta, referencia ausente o región fuera del eje no completa ni avanza. Regresiones de P08/PARTIAL/yaw/impulsos pasan. Se actualiza el fixture de llegada antigua para proporcionar el anclaje válido que ahora requiere un recorte grande; la captura real de ese anclaje se prueba aparte.

Reproducir: Python `vision/n03_panel/test_production_acquisition.py` y `test_production_near.py`; WSL `bash groundstation/near_v31_tests.sh` y `bash groundstation/vertical_v30_tests.sh`. Son comprobaciones de software; no certifican continuidad física en cualquier iluminación ni contacto real.

Estado: actualizado en Competencia, comprobado y cargado el28 de octubre de2026. ESP31 en COM4/MACdc:b4:d9:39:b3:b4:842288bytes escritos, hash de escritura verificado, PlatformIO SUCCESS17,44s y resetRTS. Compilación SUCCESS17,11s, RAM51000bytes/Flash841921bytes. SHA256 firmware.bin:9E814FCD43B95214C054EB6551F36DEA5FC1A75142021204B0A319962E023C3D.

Nicla USB COM10 identificada por VID2341/PID045F y discoE:Arduino/OpenMV. Nicla6 guardada enE:\main.py,17169bytes, flush/fsync y lectura idéntica al fuente. SHA256:A0C80566974ADED1D75929C38FFC193930341E92CE065EDB972481A6875283F4. Copia del main5 anterior en `vision/backups/nicla_main_pre_near_20261028_183117_218084.py`; recibo en `docs/nicla_v6_upload_receipt.json`. Falta reiniciar Nicla para ejecutar el archivo nuevo y validar físicamente FULL→recorte→pérdida→recuperación. No se verificó en ejecución aún ni se activaron motores.
