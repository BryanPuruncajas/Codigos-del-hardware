# Avance v34: mayor inclinación y continuidad breve del waypoint

2026-10-29. Perfil de aproximación N03 de un waypoint (P08/P09).

## Parámetros modificables

Archivo principal: `firmware/src/mission/ApproachProfile.h`.

| Parámetro | Valor v34 | Antes |
|---|---:|---:|
| Orden horizontal máxima | 0.08 | 0.03 |
| Inclinación adicional máxima en impulso | 22° | 12° normal |
| Preparar vector | 300 ms | 500 ms |
| Impulso | 1800 ms | 1200 ms |
| Corrección durante inercia | 900 ms | 2000 ms |
| Ventana de confirmación para iniciar | 3 muestras / 150 ms | 5 / 300 ms |
| Error de yaw para iniciar / continuar | 12° / 18° | 8° / 12° |
| Error de imagen horizontal para iniciar / continuar | 40 / 50 px | 24 / 40 px |
| Error vertical de imagen para iniciar / continuar | 68 / 74 px | 50 / 65 px |

La inclinación es adicional a la pose calibrada 52.5° / 64.5°. Se aproxima a
74.5° / 42.5° al alcanzar 22° sin orden de giro. Es un máximo: la geometría,
demanda vertical y rampa determinan la inclinación ordenada en cada instante.
La magnitud 0.08 no es una velocidad medida ni un porcentaje extra de motor.

El mezclador habilita ese mayor recorrido solo al solicitarlo explícitamente
y sin orden de giro simultánea. Los clientes que no lo soliciten mantienen
su envolvente previa de 12° y demanda horizontal 0.03. Los límites mecánicos
0..120° y rampa de servo 30°/s se conservan. El regreso espera la pose neutral
ordenada antes de volver a corregir yaw; el presupuesto de retorno es 800 ms.

## Referencia durante el avance

Cada observación confirmada nueva sigue actualizando el rumbo del waypoint y
el control visual de altura. La referencia de altura barométrica del waypoint
se conserva cuando ya ha podido validarse; no se crea una referencia precisa
a partir del centro de una imagen recortada.

Si desaparece brevemente la detección durante un impulso ya activo, puede
continuar hasta 800 ms desde la última observación real. Para ello requiere:

- misma asociación, UART reciente, sin reset ni ambigüedad;
- rumbo dentro de la predicción y velocidad de giro permitidas;
- barómetro reciente y válido; variación de altura <=25 cm desde la última
  observación y velocidad vertical estimada <=0.30 m/s;
- impulso todavía dentro de su duración máxima de 1.8 s.

No inicia un nuevo impulso sin observación confirmada. Al vencer el margen,
retira avance, conserva el rumbo y continúa recuperación según el controlador
existente. Una nueva asociación invalida la memoria. El tiempo máximo de
recuperación visual a bordo sigue siendo 15 s.

La memoria contiene rumbo y altura relativos, no una posición 3D ni velocidad
horizontal medida. No hay Flowdeck. No se modifican F2, FULL/PARTIAL ni los
datos reales de cámara para simular una observación. La continuación temporal
se identifica con el bit 1024 de `inertia_flags` (F13); el lanzador la muestra
como `avance_con_referencia_temporal`.

## Llegada y sustentación

Se conserva la llegada estimada de 120 px, aproximadamente 80 cm por elección
del usuario, con sus condiciones previas de FULL histórico, alineación y
confirmación. La memoria por sí sola nunca confirma llegada ni contacto.

La semilla vertical sigue siendo 18%, máximo 25%, con adaptación y reducción
por sobrealtura. Las ganancias de yaw y los pines no cambian. El corte por
estimación de batería continúa suspendido en el lanzador del waypoint.

## Integración y ejecución

`firmware/src/app/VisualBearing.h` anuncia versión 34 en F11. El lanzador
`groundstation/check_single_waypoint.py` exige >=34 antes de armar. Nicla y
app no requieren carga nueva. El perfil dinámico se integra en P08/P09; las
rutas completas P10/P11 aún conservan su controlador de aproximación previo.

Compilar/cargar desde CMD, con la ESP fuera de PCB, sin batería, solo USB:

```bat
"%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" run -d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\firmware" -e seeed_xiao_esp32s3 -t upload --upload-port COM4
```

Luego montar sin USB en las placas del blimp, alimentar por batería y conectar
solo Ground al ordenador. Con Ground abierto en COM9, en otra CMD:

```bat
cd /d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation"
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u check_single_waypoint.py --actuate --seconds 120
```

v34 cargado el 2026-10-29 en COM4, ESP MAC DC:B4:D9:39:B3:B4. PlatformIO SUCCESS (19.39 s), 843248 bytes escritos, hash verificado y reinicio RTS. No se ejecutaron motores durante la carga; la validación física del avance sigue pendiente.
Comprobaciones: `groundstation/approach_v34_tests.sh` y
`python -B -m unittest discover -s groundstation -p test_single_waypoint_guard.py -v`.

Resultado: compilación PlatformIO SUCCESS en 53.73 s, RAM 51016 bytes y flash
842881 bytes; 14 comprobaciones Python correctas. La suite nativa completa pasó
incluyendo inclinación real del mezclador >15°/hasta22°, preservación del perfil
legado, pérdida breve, cancelación por pérdida prolongada/ambigüedad/cambio de
asociación/fallo UART/altura inválida, llegada independiente, sustentación,
barómetro y recuperación. No se midió velocidad física ni contacto.

