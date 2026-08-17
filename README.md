# mi-blimp-mio — Manual técnico completo del proyecto de tesis

**Estado revisado:** 17 de agosto de 2026  
**Plataforma principal:** Seeed Studio XIAO ESP32-S3  
**Comunicación de tierra:** ESP32 Base Station por USB ↔ ESP-NOW ↔ XIAO ESP32-S3  
**Visión:** Arduino Nicla Vision con OpenMV/MicroPython  
**Sensores activos:** BNO085/BNO086 + BMP390 + batería + Nicla Vision  
**Actuadores:** 2 motores brushless con ESC + 2 servos K-Power P0025


---

## 1. Objetivo del proyecto

El proyecto controla un blimp/bicóptero de tesis capaz de:

1. medir altura, orientación y velocidad angular;
2. recibir detecciones visuales desde una Nicla Vision;
3. controlar dos motores brushless y dos servos tilt;
4. comunicarse con una estación base por ESP-NOW;
5. ejecutar pruebas incrementales P00–P11;
6. evolucionar hacia una misión autónoma de visita de **4 globos visualmente idénticos**.

La filosofía actual es validar primero cada subsistema de forma aislada y luego combinarlos.

---

# 2. Estructura del repositorio

```text
mi-blimp-mio/
├── README.md
├── CHANGELOG.md
├── base_station/
│   ├── platformio.ini
│   └── src/main.cpp
├── calibration/
│   ├── gaussian_manual.py
│   ├── gaussian_manual_multi.py
│   └── colors/
│       ├── red.txt
│       └── purple.txt
├── docs/
│   ├── ARCHITECTURE.md
│   ├── PROJECT_TREE.txt
│   └── TEST_SEQUENCE.md
├── firmware/
│   ├── platformio.ini
│   ├── src/
│   │   ├── main.cpp
│   │   ├── app/
│   │   ├── mission/
│   │   └── tests/
│   └── lib/BlimpSwarm/
├── groundstation/
│   ├── run_test.py
│   ├── user_parameters.py
│   ├── common/
│   └── tests/
├── legacy/
├── logs/
└── vision/
    ├── get_gains.py
    └── perception_subsystem.py
```

### Carpetas importantes

| Carpeta | Función |
|---|---|
| `firmware/` | Firmware del XIAO ESP32-S3 montado en el blimp. |
| `base_station/` | Firmware de la ESP32 conectada al PC por USB. |
| `groundstation/` | Scripts Python para mandar comandos, armar, ejecutar pruebas y guardar telemetría. |
| `vision/` | Código MicroPython/OpenMV de la Nicla Vision. |
| `calibration/` | Datos y scripts para calibración estadística de colores. |
| `logs/` | CSV generados automáticamente por `run_test.py`. |
| `mission/` | Máquina de estados P08–P11. |
| `legacy/` | Código heredado/no activo conservado como referencia. |

---

# 3. Hardware del blimp

## 3.1 XIAO ESP32-S3

El firmware principal utiliza:

```ini
[env:seeed_xiao_esp32s3]
platform = espressif32
board = seeed_xiao_esp32s3
framework = arduino
monitor_speed = 115200
upload_speed = 921600
```

Dependencias principales:

- ESP32Servo
- IBusBM
- Adafruit BMP3XX
- Adafruit BusIO
- Adafruit Unified Sensor
- SparkFun BNO08x Arduino Library

---

## 3.2 Pinout actual

Fuente: `firmware/src/app/AppConfig.h`.

| Función | Pin XIAO S3 |
|---|---:|
| Servo 1 | D0 |
| Servo 2 | D1 |
| RST BNO085/BNO086 | D2 |
| I2C SDA | D4 |
| I2C SCL | D5 |
| Nicla TX esperado | D6 |
| Nicla RX esperado | D7 |
| ADC batería | D8 |
| Brushless / ESC 1 | D9 |
| Brushless / ESC 2 | D10 |

### D2

`D2` está reservado para el reset del BNO.

Al arrancar:

```cpp
pinMode(D2, OUTPUT);
digitalWrite(D2, HIGH);
```

No reutilizar D2 para servos, motores u otra señal.

---

# 4. Calibración física de servos y vectores de empuje

Servos utilizados: **K-Power P0025**.

Configuración nominal:

| Ángulo lógico | PWM |
|---:|---:|
| 0° | 900 µs |
| 60° | 1500 µs |
| 120° | 2100 µs |

Conversión activa:

```text
0..120°  <=>  900..2100 µs
```

## 4.1 Orientaciones físicas validadas

Estas posiciones ya fueron probadas físicamente:

| Movimiento | Servo 1 | Servo 2 |
|---|---:|---:|
| Vector vertical +Z | 35° | 85° |
| Avance | 120° | 0° |
| Retroceso | 0° | 120° |
| Giro derecha / horario | 120° | 120° |
| Giro izquierda / antihorario | 0° | 0° |

Vista frontal usada durante las pruebas:

- Servo 1: lado izquierdo.
- Servo 2: lado derecho.

Convención de yaw observada físicamente:

- giro horario → yaw disminuye;
- giro antihorario → yaw aumenta;
- yaw trabaja en radianes dentro del firmware y envuelve en `[-π, +π]`.

---

# 5. Motores brushless y ESC

## 5.1 Escala de potencia actual

**Importante:** el código fuente actual tiene:

```cpp
ABSOLUTE_MOTOR_POWER_LIMIT = 1.00f;
motorPowerLimit = 1.00f;
```

Por tanto:

```text
0.00 = 0 %
0.10 = 10 %
0.20 = 20 %
1.00 = 100 % de la escala heredada
```

La documentación vieja que dice “límite duro de 50 %” está desactualizada.

## 5.2 Configuración de los ESC

En `RawBicopter::startup()`:

```cpp
motor1 = new BLMotor(1100, 2000, 0, D9, 55);
motor2 = new BLMotor(1100, 2000, 0, D10, 58);
```

La secuencia de ARM reproduce el comportamiento efectivo del firmware viejo:

1. habilita servos y ESC;
2. manda throttle mínimo;
3. mantiene aproximadamente throttle mínimo durante ~3.7 s;
4. termina con motores en 0 %;
5. recién después `actuatorsArmed = true`.

No existe auto-arm al encender.

---

# 6. Seguridad de arranque

Al boot:

- D2 se pone HIGH para liberar el BNO.
- pines de motores y servos comienzan en alta impedancia;
- los actuadores permanecen desarmados;
- no se ejecuta calibración automática de ESC;
- para mover motores se necesita un ARM explícito.

`SAFE_STOP` tiene modo `0`.

Cuando `run_test.py` termina o recibe `Ctrl+C`, intenta:

```text
STOP + DISARM
```

enviando varias veces el modo seguro.

> Seguridad eléctrica: el software no evita por sí mismo un posible backfeed desde USB/rail de alimentación. Verificar alimentación físicamente cuando se trabaja con batería desconectada.

---

# 7. Sensores del XIAO ESP32-S3

## 7.1 BMP390

El BMP390:

- usa I2C;
- oversampling temperatura: 8X;
- oversampling presión: 16X;
- filtro IIR: coeficiente 7;
- ODR configurado: 100 Hz;
- la rutina de actualización efectiva limita lecturas aproximadamente a 50 Hz (`20 ms`).

### Calibración de cero de altura al boot

Al iniciar:

1. descarta 5 lecturas;
2. toma 10 lecturas;
3. promedia la altitud barométrica;
4. guarda ese promedio como `groundLevel`.

La telemetría de altura es:

```text
altura = altitud barométrica - groundLevel
```

Por eso cada reinicio redefine el cero barométrico.

La velocidad vertical se calcula a partir del cambio de altitud y luego SensorSuite aplica suavizado.

---

## 7.2 BNO085/BNO086

Bus I2C:

```text
SDA = D4
SCL = D5
RST = D2 HIGH
I2C = 400 kHz después de conectar
```

### Dirección configurada actualmente

El código intenta:

```cpp
myIMU.begin(0x4A, Wire)
```

Por tanto el firmware actual espera **0x4A**.

Si el módulo físico está configurado a `0x4B`, este código debe modificarse o el sensor no será detectado.

### Datos utilizados

El firmware usa el reporte:

```text
Gyro Integrated Rotation Vector
```

y obtiene:

- roll;
- pitch;
- yaw;
- velocidad angular X;
- velocidad angular Y;
- velocidad angular Z.

---

# 8. Mapa completo de sensores

`SensorMap.h`:

| Índice | Variable |
|---:|---|
| 0 | temperature |
| 1 | altitude |
| 2 | vertical_velocity |
| 3 | roll |
| 4 | pitch |
| 5 | yaw |
| 6 | roll_rate |
| 7 | pitch_rate |
| 8 | yaw_rate |
| 9 | reservado |
| 10 | battery |
| 11 | nicla_flag |
| 12 | nicla_x |
| 13 | nicla_y |
| 14 | nicla_w |
| 15 | nicla_h |
| 16 | nicla_x_value |
| 17 | nicla_y_value |
| 18 | nicla_w_value |
| 19 | nicla_h_value |
| 20 | nicla_distance |

No existe ultrasonido en la ruta activa actual.

---

# 9. Nicla Vision

## 9.1 Qué hace realmente el “entrenamiento”

Actualmente **no se entrena una red neuronal**.

La detección de globos usa:

- imagen RGB565;
- espacio de color LAB;
- grilla 14 × 21;
- estadísticas de `A` y `B`;
- modelo gaussiano 2D;
- distancia de Mahalanobis;
- filtro probabilístico por celdas;
- seguimiento temporal.

El proceso llamado “entrenamiento” en este proyecto consiste en:

1. capturar muchos pares `(A, B)` del color del globo;
2. calcular media;
3. calcular matriz de covarianza;
4. invertir la covarianza;
5. copiar la media y matriz inversa a `COLOR_DATA`.

---

## 9.2 Configuración actual de imagen

En `vision/perception_subsystem.py`:

```python
FRAME_SIZE = sensor.HQVGA
```

Para Nicla:

```text
240 × 160 px
```

Centro horizontal usado por el control:

```text
x_center = 120 px
```

Grilla:

```text
N_ROWS = 14
N_COLS = 21
```

Objetivo actual:

```python
COLOR_TARGET = {"red": (255, 0, 0)}
```

Por tanto la misión actual busca **globos rojos**.

---

## 9.3 Parámetros actuales del detector rojo

Para Nicla:

```python
R_GAIN, G_GAIN, B_GAIN = [91, 64, 92]
```

Datos gaussianos activos para rojo:

```text
media [A, B]:
[50.173846153846156, 25.426923076923078]

matriz inversa de covarianza:
[
  [ 0.01470976874406989, -0.029273398296275985],
  [-0.029273398296275982, 0.08279564686512679]
]
```

Estos valores coinciden con los **2600 puntos válidos** presentes actualmente en:

```text
calibration/colors/red.txt
```

---

# 10. Cargar código en la Nicla Vision

El código está escrito para el firmware OpenMV/MicroPython de la Nicla Vision.

Archivo principal:

```text
vision/perception_subsystem.py
```

## Flujo recomendado

1. Instalar OpenMV IDE.
2. Conectar la Nicla Vision por USB.
3. Abrir OpenMV IDE.
4. Conectar la cámara desde el botón de conexión.
5. Si no entra correctamente al firmware OpenMV:
   - hacer doble pulsación rápida del botón RESET para entrar al bootloader/DFU;
   - permitir que OpenMV IDE actualice/cargue el firmware compatible.
6. Abrir:
   ```text
   vision/perception_subsystem.py
   ```
7. Ejecutarlo primero desde el IDE.
8. Verificar framebuffer y consola.
9. Cuando esté validado, guardar/copiar el script como:
   ```text
   /flash/main.py
   ```
   para que arranque automáticamente después de un reinicio completo.

La Nicla con OpenMV ejecuta `main.py` en un arranque frío.

---

# 11. Calibrar ganancias RGB de la Nicla

Archivo:

```text
vision/get_gains.py
```

Objetivo: obtener ganancias RGB estables para las condiciones reales de iluminación.

## Procedimiento

1. Conecta la Nicla a OpenMV IDE.
2. Coloca el globo bajo la iluminación de prueba.
3. Ejecuta:
   ```text
   vision/get_gains.py
   ```
4. Espera la estabilización automática.
5. En la consola aparecerá algo similar a:
   ```text
   RGB gain: [R, G, B]
   ```
6. Repite en condiciones representativas.
7. Si decides cambiar la calibración, actualiza en:
   ```python
   R_GAIN, G_GAIN, B_GAIN = [...]
   ```
   dentro de `perception_subsystem.py`.

Actualmente:

```python
[91, 64, 92]
```

---

# 12. Capturar datos de color para recalibrar un globo

En:

```text
vision/perception_subsystem.py
```

cambiar temporalmente:

```python
PRINT_CORNER = True
```

Como el código define:

```python
PLOT_METRIC = not PRINT_CORNER
```

esto activa el modo de recolección.

## Procedimiento recomendado

1. Deja el color objetivo visible bajo una iluminación realista.
2. Activa:
   ```python
   PRINT_CORNER = True
   ```
3. Ejecuta el script en OpenMV.
4. La imagen marca una pequeña región de recolección.
5. Coloca la superficie del globo dentro de esa región.
6. La consola imprime pares:
   ```text
   (A, B), (A, B), ...
   ```
7. Captura muestras en:
   - distintas distancias;
   - distintos ángulos;
   - varias zonas del globo;
   - iluminación clara;
   - iluminación tenue;
   - fondo real del experimento.
8. Copia los pares del globo rojo en:
   ```text
   calibration/colors/red.txt
   ```
9. Al terminar, vuelve a:
   ```python
   PRINT_CORNER = False
   ```

---

# 13. Calcular la nueva distribución de color en PC

Dependencias para calibración:

```powershell
pip install numpy matplotlib
```

Desde la carpeta:

```powershell
cd calibration
```

ejecutar:

```powershell
python gaussian_manual_multi.py
```

Para cada archivo existente imprime:

```text
mean and Inverted Covariance:
[media_A, media_B], [[...], [...]]
```

Los dos valores que interesan son:

1. media;
2. covarianza inversa.

Luego se copian en:

```python
COLOR_DATA = {
    "red": [
        [media_A, media_B],
        [[c00, c01], [c10, c11]]
    ]
}
```

dentro de `vision/perception_subsystem.py`.

### Nota del ZIP actual

`gaussian_manual_multi.py` intenta abrir:

```text
green.txt
purple.txt
blue.txt
red.txt
```

pero el ZIP actual solo contiene:

```text
purple.txt
red.txt
```

El rojo sí está sincronizado con `COLOR_DATA`.

El archivo `purple.txt` actual produce aproximadamente:

```text
media = [16.2885, -10.1674]
```

pero `COLOR_DATA["purple"]` contiene otros valores más antiguos. Actualmente no afecta la misión porque `COLOR_TARGET` solo usa rojo.

---

# 14. Comunicación Nicla ↔ XIAO

La Nicla transmite una trama tipo iBus de 32 bytes a:

```text
115200 baud
```

La Nicla usa:

```python
UART("LP1", baudrate=115200)
```

En el comentario del código:

```text
Nicla P1 = TX
Nicla P0 = RX
```

El diseño del XIAO reserva:

```text
D6 = TX
D7 = RX
```

Cableado esperado:

```text
Nicla TX (P1) -> XIAO RX (D7)
Nicla RX (P0) <- XIAO TX (D6)
GND            -> GND
```

### Nota de implementación

`AppConfig.h` define D6/D7, pero `Nicla.cpp` crea `HardwareSerial(0)` y llama `begin(..., -1, -1)`, por lo que no pasa explícitamente D6/D7 a `HardwareSerial`.

Como la comunicación ya fue observada funcionando en P06, el montaje actual está operativo, pero conviene limpiar esto después para que el pinout quede explícito en el código.

---

# 15. Formato de datos de la Nicla

Para globo:

```text
[flag, x_roi, y_roi, w_roi, h_roi,
 x_value, y_value, w_value, h_value, distance]
```

Actualmente `distance` se transmite como:

```text
9999
```

por tanto **no usar `nicla_distance` para navegación**.

## Flags observados

Modo globo:

```text
0x40 = 64
```

Sin detección activa:

```text
64
```

Detección válida alterna bits bajos y normalmente se observa:

```text
65
66
```

El firmware del XIAO considera detección válida cuando:

```cpp
(flag & 0x40) != 0
&&
(flag & 0x03) != 0
```

---

# 16. Firmware del XIAO ESP32-S3

## 16.1 Compilar

Desde:

```powershell
cd firmware
```

con PlatformIO CLI:

```powershell
pio run
```

## 16.2 Cargar

```powershell
pio run -t upload
```

Si existen varios puertos se puede definir temporalmente el puerto desde PlatformIO/VS Code o agregar `upload_port`.

## 16.3 Monitor serial

```powershell
pio device monitor -b 115200
```

Al arrancar debe aparecer información similar a:

```text
=== BLIMP TESIS / REFRACTOR DE PRUEBAS ===
SAFE BOOT...
ESP Board MAC Address: xx:xx:xx:xx:xx:xx
```

Guarda la MAC mostrada: esa es la **MAC del robot**, necesaria en `groundstation/user_parameters.py`.

---

# 17. Secuencia de inicialización del XIAO

El flujo actual de `setup()` es:

```text
Serial 115200
    ↓
HardwareSafety::preInit()
    ↓
D2 HIGH
motores/servos alta impedancia
    ↓
ESP-NOW
    ↓
FullBicopter::startup()
    ↓
actuadores DESARMADOS
    ↓
SensorSuite
 ├── BNO
 ├── BMP390
 └── batería
    ↓
Nicla modo 0x40 / balloon
    ↓
carga de preferencias
    ↓
SAFE_STOP
```

El loop intenta ejecutarse cada:

```text
4000 µs ≈ 250 Hz
```

aunque cada sensor/control tiene sus propias tasas efectivas.

Telemetría ESP-NOW está limitada a:

```text
10 Hz por flag
```

---

# 18. Base Station ESP32

La base **no ejecuta control de vuelo**.

Su trabajo es:

```text
PC/Python
   ↕ USB Serial 115200
ESP32 Base Station
   ↕ ESP-NOW
XIAO ESP32-S3
```

Archivo:

```text
base_station/src/main.cpp
```

## 18.1 Placa configurada actualmente

```ini
[env:esp32dev]
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
```

Esto corresponde a una ESP32 genérica tipo DevKit/NodeMCU.

Si la base fuera otro XIAO ESP32-S3:

```ini
board = seeed_xiao_esp32s3
```

---

# 19. Cargar la Base Station

Desde:

```powershell
cd base_station
```

Compilar:

```powershell
pio run
```

Cargar:

```powershell
pio run -t upload
```

Monitor:

```powershell
pio device monitor -b 115200
```

La base imprime:

```text
BASE_MAC,XX:XX:XX:XX:XX:XX
```

---

# 20. Configurar la Ground Station

Archivo:

```text
groundstation/user_parameters.py
```

Variables importantes:

```python
robot_macs = ["DC:B4:D9:39:B3:B4"]
SERIAL_PORT = "COM5"
```

Cambiar:

```text
robot_macs
```

por la MAC que imprime el XIAO.

Cambiar:

```text
SERIAL_PORT
```

por el COM de la **ESP32 Base Station**, no el COM del XIAO.

Ejemplo:

```python
robot_macs = ["AA:BB:CC:DD:EE:FF"]
SERIAL_PORT = "COM7"
```

El script convierte la MAC a minúsculas automáticamente.

---

# 21. Registro automático de la base en el robot

Cada ejecución normal de `run_test.py` hace:

1. agrega la MAC del robot como peer de la base;
2. envía al robot la MAC de la base;
3. el robot guarda esa MAC en NVS bajo:
   ```text
   GroundMac
   ```
4. se manda `reload=1`;
5. el XIAO vuelve a cargar `GroundMac`;
6. agrega la estación de tierra como peer;
7. comienza la telemetría.

Por eso normalmente no es necesario escribir manualmente la MAC de la base en el firmware.

---

# 22. Preparar Python

Desde:

```powershell
cd groundstation
```

Instalar:

```powershell
pip install -r requirements.txt
```

Actualmente:

```text
pyserial >= 3.5
```

Muy importante:

> Cierra el Serial Monitor de Arduino/PlatformIO antes de ejecutar Python o Windows puede devolver “Access is denied” al abrir el COM.

---

# 23. run_test.py

Forma general:

```powershell
python run_test.py TEST [opciones]
```

Tests disponibles:

```text
p00m
p00
p01
p02
p03
p04
p05
p06
p07
p08
p09
p10
p11
```

Modos internos:

| Test | Mode |
|---|---:|
| SAFE_STOP | 0 |
| P00M motores | 8 |
| P00 servos | 9 |
| P01 sensores | 10 |
| P02 manual | 11 |
| P03 yaw | 12 |
| P04 altura | 13 |
| P05 altura+yaw | 14 |
| P06 Nicla raw | 15 |
| P07 centrado visual | 16 |
| P08 un globo | 17 |
| P09 visita+escape | 18 |
| P10 dos globos | 19 |
| P11 cuatro globos | 20 |

---

# 24. Protocolo de seguridad de run_test.py

Las pruebas actuadas:

```text
P02
P03
P04
P05
P07
P08
P09
P10
P11
```

requieren escribir:

```text
ARMAR
```

El script manda el paquete de ARM incluyendo ya:

- modo;
- referencias;
- ganancias;
- parámetros auxiliares.

Después espera:

```text
4.2 s
```

para que termine la secuencia de armado de ESC.

Luego manda `reset=1` conservando las mismas referencias.

Esto evita que un paquete posterior reemplace el ARM antes de que el firmware lo procese.

---

# 25. Telemetría

La base imprime:

```text
TEL,flag,v0,v1,v2,v3,v4,v5
```

`run_test.py` lo convierte a nombres legibles.

## F1 — estado principal

```text
height
yaw
roll
pitch
battery
vertical_velocity
```

## F2 — Nicla

```text
nicla_flag
nicla_x
nicla_y
nicla_w
nicla_h
nicla_distance
```

## F3 — actuadores

```text
servo1
servo2
motor1
motor2
armed
mode
```

## F4 — control

```text
rollrate
pitchrate
yawrate
yaw_ref
height_ref
fx_cmd
```

El significado exacto de `fx_cmd` depende del test:

- P03: error de yaw;
- P04/P05: error de altura;
- misión: error visual normalizado durante APPROACH.

## F5 — misión

```text
mission_state
visited
target_count
search_height
visit_score
elapsed_s
```

---

# 26. Logs CSV

Cada ejecución crea automáticamente:

```text
logs/blimp_YYYYMMDD_HHMMSS.csv
```

Columnas:

```text
pc_time
flag
name0/v0
...
name5/v5
```

Sirven para:

- ajustar PID/PD;
- medir overshoot;
- medir tiempo de subida;
- observar potencia de sostenimiento;
- analizar yaw;
- validar visión;
- validar estados de misión.

---

# 27. P00 — Calibración de servos

P00 **no habilita brushless**.

El firmware desacopla el servo no seleccionado (`detach`) para evitar que al probar uno se mueva el otro.

## Servo 1 — barrido

```powershell
python run_test.py p00 --servo 1 --servo-step 2 --servo-delay 0.06 --seconds 2
```

Escribir:

```text
SERVO
```

Secuencia:

```text
60° -> 0° -> 120° -> 60°
```

## Servo 2

```powershell
python run_test.py p00 --servo 2 --servo-step 2 --servo-delay 0.06 --seconds 2
```

## Posición fija Servo 1

```powershell
python run_test.py p00 --servo 1 --servo-angle 35 --seconds 10
```

## Posición fija Servo 2

```powershell
python run_test.py p00 --servo 2 --servo-angle 85 --seconds 10
```

## Ambos

```powershell
python run_test.py p00 --servo both --servo-angle 60 --seconds 10
```

Para diagnosticar interferencias eléctricas/mecánicas es preferible probar primero **uno por uno**.

---

# 28. P00M — Prueba directa de brushless

P00M no usa:

- PID;
- mixer;
- yaw;
- altura;
- visión.

Permite mandar directamente potencia a uno o ambos motores.

## Motor 1, vector vertical, 10 %

```powershell
python run_test.py p00m --motor 1 --power 10 --servo1-angle 35 --servo2-angle 85 --motor-seconds 3
```

## Motor 2

```powershell
python run_test.py p00m --motor 2 --power 10 --servo1-angle 35 --servo2-angle 85 --motor-seconds 3
```

## Ambos a 10 %

```powershell
python run_test.py p00m --motor both --power 10 --servo1-angle 35 --servo2-angle 85 --motor-seconds 3
```

El script pide:

```text
ARMAR
```

y luego:

```text
MOTOR
```

No empezar con potencias altas.

---

# 29. P01 — Integración de sensores

No mueve actuadores.

```powershell
python run_test.py p01 --seconds 30
```

Revisar principalmente F1:

```text
height
yaw
roll
pitch
battery
vertical_velocity
```

Y verificar:

- altura cercana al cero después de boot;
- yaw responde al giro;
- roll/pitch cambian en los ejes esperados;
- batería tiene lectura razonable;
- Vz no explota con ruido.

---

# 30. P02 — Control manual físico

```powershell
python run_test.py p02
```

Escribir:

```text
ARMAR
```

Controles:

| Tecla | Acción |
|---|---|
| `w` | avance |
| `s` | retroceso |
| `a` | giro izquierda |
| `d` | giro derecha |
| `r` | empuje +Z |
| `f` | -Z solicitado → descenso pasivo, motores 0 |
| `x` | salir |

Cada tecla se escribe y luego Enter.

P02 usa potencia fija:

```text
20 %
```

y selecciona solo el comando de mayor magnitud si por accidente llegan varios ejes.

---

# 31. P03 — Control de yaw PD

P03 controla yaw de forma aislada.

Control:

```text
error = wrapPi(yaw_ref - yaw)
```

Esfuerzo:

```text
Kp * |error| - Kd * velocidad_hacia_objetivo
```

Si corrige:

```text
potencia >= yaw_min_power
potencia <= yaw_max_power
```

Zona muerta configurable.

## Ejemplo suave

```powershell
python run_test.py p03 --yaw-deg 0 --yaw-kp 0.10 --yaw-kd 0.03 --yaw-min-power 7 --yaw-max-power 8.5 --yaw-deadband-deg 5 --seconds 60
```

Parámetros:

| Parámetro | Significado |
|---|---|
| `--yaw-deg` | setpoint absoluto de yaw en grados |
| `--yaw-kp` | ganancia proporcional |
| `--yaw-kd` | amortiguación derivativa |
| `--yaw-min-power` | potencia mínima cuando hay corrección |
| `--yaw-max-power` | límite de potencia |
| `--yaw-deadband-deg` | banda donde deja de corregir |

---

# 32. P04 — PID de altura

P04 mantiene únicamente altura.

Servos siempre:

```text
S1 = 35°
S2 = 85°
```

Control:

```text
error = Zref - Z
P = Kp * error
I = integral
D = -Kd * Vz_filtrada
```

Salida:

```text
0 .. max_power
```

No existe empuje activo hacia -Z; si está alto, el sistema reduce/corta potencia y deja descenso pasivo.

Otros detalles:

```text
deadband P = ±2.5 cm
integral activa dentro de ±35 cm
aporte integral máximo = 6 %
filtro Vz alpha = 0.82
```

## Configuración que se venía probando

```powershell
python run_test.py p04 --height -1 --kp 0.30 --ki 0.025 --kd 0.01 --max-power 12 --slew 0.18 --seconds 60
```

Cambiar `--height` por el setpoint deseado.

### Parámetros

| Parámetro | Función |
|---|---|
| `--height` | altura objetivo relativa al cero barométrico del boot |
| `--kp` | respuesta al error |
| `--ki` | aprende/aporta potencia de sostenimiento |
| `--kd` | frena según velocidad vertical |
| `--max-power` | límite máximo en % |
| `--slew` | velocidad máxima de cambio de potencia, escala 0..1/s |

Aliases equivalentes:

```text
--alt-kp
--alt-ki
--alt-kd
--alt-max-power
--alt-slew
```

Los `--alt-*` explícitos tienen prioridad sobre los nombres cortos.

---

# 33. P05 v3 — Altura primero + yaw manteniendo altura

Este es el P05 nuevo del ZIP revisado.

Arquitectura:

```text
ALTITUDE_ACQUIRE
        ↓
YAW_ACQUIRE
        ↓
HOLD
```

## Fase 0 — ALTITUDE_ACQUIRE

- solo Z tiene autoridad;
- servos permanecen en:
  ```text
  35° / 85°
  ```
- yaw es ignorado;
- altura debe quedar dentro de la banda configurada;
- además:
  ```text
  |Vz filtrada| <= 0.10 m/s
  ```
- debe mantenerse así:
  ```text
  2 segundos
  ```

Entonces cambia a `YAW_ACQUIRE`.

## Fase 1 — YAW_ACQUIRE

El control de altura **no se apaga**.

Se calculan simultáneamente:

```text
altPower
yawPower
```

El único mixer manda:

```text
outputPower = max(altPower, yawPower)
```

Los servos se interpolan desde el vector vertical hacia el vector extremo de yaw.

`--yaw-servo-authority` controla cuánto se alejan de:

```text
35° / 85°
```

Ejemplo 30 %:

```text
usa como máximo 30 % del recorrido desde Z hacia 0/0 o 120/120
```

Yaw debe permanecer dentro de la banda de éxito durante:

```text
500 ms
```

para entrar a HOLD.

## Fase 2 — HOLD

Mantiene control de altura.

Si yaw se aleja más de:

```text
yaw_success + 3°
```

vuelve a `YAW_ACQUIRE`.

---

# 34. Comando recomendado actual P05

Ejemplo:

```powershell
python run_test.py p05 --height 0 --yaw-deg 0 --kp 0.30 --ki 0.025 --kd 0.01 --alt-min-power 6 --max-power 15 --slew 0.18 --alt-success-cm 10 --yaw-kp 0.10 --yaw-kd 0.03 --yaw-min-power 7 --yaw-max-power 8.5 --yaw-success-deg 10 --yaw-servo-authority 30 --seconds 120
```

### Altura

```text
Kp              = 0.30
Ki              = 0.025
Kd              = 0.01
mínimo           = 6 %
máximo           = 15 %
slew             = 0.18 /s
éxito Z          = ±10 cm
estabilidad Vz   = ±0.10 m/s
tiempo estable   = 2 s
```

### Yaw

```text
Kp              = 0.10
Kd              = 0.03
mínimo           = 7 %
máximo           = 8.5 %
éxito            = ±10°
lock             = 500 ms
re-adquisición   = >13°
servo authority  = 30 %
```

### Debug P05

Cada ~500 ms el XIAO imprime:

```text
[P05] phase=...
zErr=...
vz=...
altP=...
yawErrDeg=...
yawP=...
out=...
S1=...
S2=...
```

Para encontrar la potencia necesaria para sostener altura, observar:

```text
altP
```

Ejemplo:

```text
altP=0.083
```

equivale aproximadamente a:

```text
8.3 %
```

---

# 35. Cuantización de parámetros P05

P05 empaqueta varios parámetros dentro de floats para no cambiar `ControlInput`.

Por eso algunos valores no tienen resolución infinita.

## Altura

- `alt-min-power`: entero en %
- `alt-max-power`: entero en %
- `slew`: pasos de 0.02/s
- `alt-success-cm`: entero en cm

## Yaw

- min/max: décimas de %
- yaw success: pasos de 0.5°
- servo authority: pasos de 10 %

Por ejemplo:

```text
--yaw-max-power 8.5
```

sí conserva 8.5 %.

---

# 36. P06 — Nicla RAW

No mueve actuadores.

```powershell
python run_test.py p06 --seconds 60
```

Mirar F2:

```text
nicla_flag
nicla_x
nicla_y
nicla_w
nicla_h
nicla_distance
```

Resultados ya observados:

```text
imagen = 240 × 160
centro x ≈ 120
x aumenta izquierda -> derecha
distance = 9999
```

Usar P06 siempre que se recalibre la visión.

---

# 37. P07 — Centrado visual

P07 gira para colocar el globo cerca del centro de la imagen.

Error:

```text
error_px = nicla_x - 120
error_norm = error_px / 120
```

Con deadband por defecto:

```text
±15 px
```

Zona central:

```text
105 .. 135 px
```

Comando:

```powershell
python run_test.py p07 --vis-kp 0.10 --vis-kd 0.015 --vis-min-power 6 --vis-max-power 10 --vis-deadband-px 15 --seconds 60
```

Si no detecta objetivo:

```text
motores = 0
```

Si está centrado:

```text
motores = 0
```

---

# 38. Estados actuales de misión P08–P11

`BalloonMission` utiliza:

```text
SEARCH = 0
APPROACH = 1
VISIT_CONFIRM = 2
ESCAPE = 3
WAIT_TARGET_LOST = 4
DONE = 5
```

Constantes actuales:

```text
centro X                   = 120 px
deadband visual default    = ±15 px
potencia avance/escape     = 20 %
P08/P09 búsqueda           = 20 %
P10/P11 búsqueda default   = 10 %
umbral visita nicla_w      = 60
frames visita              = 3
frames pérdida             = 8
escape                     = 1.8 s
separación nuevo objetivo  = 45°
```

---

# 39. P08 — Un globo

Objetivo:

```text
SEARCH
  ↓ detecta
APPROACH
  ↓ centrado + w>=60
VISIT_CONFIRM
  ↓ 3 frames
DONE
```

P08 se desarma automáticamente al terminar.

Comando:

```powershell
python run_test.py p08 --vis-kp 0.10 --vis-kd 0.015 --vis-min-power 6 --vis-max-power 10 --vis-deadband-px 15 --seconds 120
```

### Importante

P08 **no tiene supervisor de altura**.

Durante:

- SEARCH;
- giro visual;
- avance;

usa la lógica de misión original.

---

# 40. P09 — Visita + escape

Flujo:

```text
SEARCH
  ↓
APPROACH
  ↓
VISIT_CONFIRM
  ↓
ESCAPE 1.8 s
  ↓
WAIT_TARGET_LOST
  ↓ 8 frames sin objetivo
DONE
```

Comando:

```powershell
python run_test.py p09 --vis-kp 0.10 --vis-kd 0.015 --vis-min-power 6 --vis-max-power 10 --vis-deadband-px 15 --seconds 180
```

P09 también se desarma al llegar a DONE.

---

# 41. P10 — Dos globos

Objetivo:

```text
target_count = 2
```

Después de visitar uno:

```text
ESCAPE
→ WAIT_TARGET_LOST
→ 8 frames sin globo
→ SEARCH
→ acumular al menos 45° de yaw
→ aceptar siguiente detección
```

Esto existe porque la Nicla no entrega ID individual del globo.

## Comando base

**Con el código actual usar una referencia positiva mayor a 0.05 m.**

```powershell
python run_test.py p10 --height 1.00 --kp 0.30 --ki 0.025 --kd 0.01 --max-power 15 --slew 0.18 --vis-kp 0.10 --vis-kd 0.015 --vis-max-power 10 --seconds 180
```

---

# 42. P11 — Cuatro globos

Igual que P10, pero:

```text
target_count = 4
```

Comando:

```powershell
python run_test.py p11 --height 1.00 --kp 0.30 --ki 0.025 --kd 0.01 --max-power 15 --slew 0.18 --vis-kp 0.10 --vis-kd 0.015 --vis-max-power 10 --seconds 600
```

Aunque `--seconds` sea largo, la misión entra a `DONE` y se desarma cuando:

```text
visited == 4
```

---

# 43. Control de altura actual dentro de P10/P11

**P10/P11 todavía NO utilizan la arquitectura P05 v3.**

El código actual conserva un supervisor anterior.

Entra a corrección de altura cuando:

```text
está >12 cm por debajo del setpoint
o
está >18 cm por encima del setpoint
```

Sale cuando vuelve a:

```text
±7 cm
```

Mientras corrige altura:

```text
la máquina de misión queda pausada
```

Si está bajo:

```text
PID Z + servos 35/85
```

Si está alto:

```text
motores 0 -> descenso pasivo
```

Esto significa que P10/P11 todavía usan filosofía:

```text
corrección de altura con prioridad
       ↓
reanudar misión
```

y **no todavía**:

```text
Z + yaw + avance simultáneos mediante mixer
```

Ese será el siguiente cambio arquitectónico después de terminar de validar P05.

---

# 44. Parámetros de visión en P10/P11

Por falta de campos libres en `ControlInput`, P10/P11 reciben:

```text
vis Kp
vis Kd
vis max power
```

pero actualmente mantienen fijos:

```text
vis min power = 6 %
deadband      = ±15 px
```

Aunque `run_test.py` acepte `--vis-min-power` y `--vis-deadband-px`, esos dos valores **no se transmiten a P10/P11** en la implementación actual.

Para P07/P08/P09 sí son configurables.

---

# 45. Secuencia recomendada de validación

No saltar directamente a P11.

Orden recomendado:

```text
P00  -> calibrar servos
P00M -> validar ESC/motores
P01  -> sensores
P02  -> movimientos físicos
P03  -> yaw aislado
P04  -> altura aislada
P05  -> altura + yaw mediante mixer
P06  -> Nicla RAW
P07  -> centrado visual
P08  -> un globo
P09  -> visita + escape
P10  -> dos globos
P11  -> cuatro globos
```

Con la nueva arquitectura prevista:

```text
P05 validado
   ↓
actualizar BalloonMission
   ↓
ALTITUDE_ACQUIRE
   ↓
SEARCH manteniendo Z
   ↓
ALIGN manteniendo Z
   ↓
APPROACH: Z + yaw/visión + avance
   ↓
VISIT
   ↓
ESCAPE
   ↓
RECOVER
   ↓
SEARCH siguiente globo
```

---

# 46. Parámetros principales — resumen rápido

## Altura

| Opción | Uso |
|---|---|
| `--height` | setpoint Z |
| `--kp` / `--alt-kp` | proporcional |
| `--ki` / `--alt-ki` | integral |
| `--kd` / `--alt-kd` | derivativa |
| `--max-power` / `--alt-max-power` | potencia máxima |
| `--slew` / `--alt-slew` | rampa de potencia |
| `--alt-min-power` | P05: piso de potencia |
| `--alt-success-cm` | P05: banda de adquisición |

## Yaw

| Opción | Uso |
|---|---|
| `--yaw-deg` | referencia absoluta |
| `--yaw-kp` | proporcional |
| `--yaw-kd` | derivativa |
| `--yaw-min-power` | mínimo de giro |
| `--yaw-max-power` | máximo de giro |
| `--yaw-deadband-deg` | P03 |
| `--yaw-success-deg` | P05 |
| `--yaw-servo-authority` | P05: cuánto inclina servos desde el vector Z |

## Visión

| Opción | Uso |
|---|---|
| `--vis-kp` | proporcional visual |
| `--vis-kd` | derivativa visual |
| `--vis-min-power` | mínimo de giro |
| `--vis-max-power` | máximo de giro |
| `--vis-deadband-px` | banda centrada |

---

# 47. Diagnóstico rápido

## No llega telemetría

Revisar:

1. COM correcto de la base;
2. Serial Monitor cerrado;
3. MAC del robot correcta;
4. base y robot encendidos;
5. ejecutar nuevamente `run_test.py` para registrar/reload GroundMac.

## `Access is denied` en Windows

Cerrar:

- Serial Monitor;
- Arduino IDE monitor;
- PlatformIO monitor;
- cualquier terminal que tenga abierto el COM.

## BNO no aparece

Buscar:

```text
Ooops, no BNO085 detected
```

Revisar:

- D2 HIGH;
- SDA D4;
- SCL D5;
- alimentación;
- dirección actual esperada `0x4A`.

## BMP390 no aparece

Buscar:

```text
Could not find a valid BMP390 sensor
```

Revisar I2C y alimentación.

## Nicla siempre da flag 64

Revisar:

- script `perception_subsystem.py` ejecutándose;
- target rojo;
- iluminación;
- UART cruzada TX↔RX;
- tierra común;
- `COLOR_DATA`;
- ganancias RGB;
- usar P06.

## `nicla_distance=9999`

Es normal en el modo globo actual.

## Motor pita pero no gira

Revisar:

- secuencia ARM completa;
- esperar 4.2 s;
- batería;
- ESC;
- potencia mínima física;
- P00M para aislar el problema.

---

# 48. Estado actual y pendientes detectados al revisar este ZIP

Esta sección es importante para no confundir “lo que queremos” con “lo que el código hace hoy”.

## 48.1 README viejo dice 50 %, código actual dice 100 %

Documentos antiguos contienen:

```text
brushless limitados a 50 %
```

pero el fuente actual usa:

```text
1.00 = 100 %
```

Este README usa el valor real del fuente actual.

---

## 48.2 P05 fuente actual es más nuevo que `firmware.bin`

Dentro de `.pio/build` existe un `firmware.bin`, pero su fecha es anterior al `P05YawAltitude.cpp` v3.

Por tanto:

> **No asumir que ese `firmware.bin` contiene P05 v3.**

Recompilar el proyecto antes de cargar la versión actual.

---

## 48.3 P10/P11 no están migrados a P05 v3

P05 ya usa:

```text
ALTITUDE_ACQUIRE -> YAW_ACQUIRE -> HOLD
```

con mixer.

P10/P11 todavía usan `BalloonMission.cpp` con supervisor de altura prioritario.

---

## 48.4 P10/P11 ignoran altura 0 o negativa

En `BalloonMission::reset()`:

```cpp
requestedHeight > 0.05f
```

es condición para aceptar `--height`.

Por tanto actualmente:

```text
--height 1.0  -> sí se usa
--height 0    -> se ignora
--height -1   -> se ignora
```

Si se ignora, usa la altura medida al iniciar la misión.

Esto debe corregirse antes de usar referencias cero/negativas en P10/P11.

P04 y P05 no tienen esta restricción.

---

## 48.5 `run_test.py --help` tiene un error de formato

La lógica de ejecución compila con Python, pero el `--help` actual puede fallar por un `%` sin escapar dentro de un texto de ayuda de argparse.

Los comandos normales siguen funcionando.

Pendiente: reemplazar `%` por `%%` en los textos de ayuda correspondientes.

---

## 48.6 Mensaje de P00M sobre el “barrido”

El texto de consola de `run_test.py` todavía menciona un “barrido simultáneo”.

Sin embargo el `RawBicopter::setActuatorsArmed()` actual reproduce el efecto real del firmware viejo:

```text
throttle mínimo durante ~3.7 s
```

No una rampa real de potencia.

---

## 48.7 D6/D7 de Nicla no están pasados explícitamente al UART

`AppConfig.h` los reserva, pero `Nicla.cpp` usa:

```cpp
MySerial0.begin(115200, SERIAL_8N1, -1, -1);
```

Como P06 ha funcionado físicamente, no es un bloqueo actual, pero conviene hacerlo explícito para robustez y documentación.

---

## 48.8 BNO solo intenta 0x4A

Si se cambia el módulo o strap de dirección, agregar intento a 0x4B o hacer dirección configurable.

---

## 48.9 Calibración de colores incompleta para multi-color

`gaussian_manual_multi.py` espera 4 archivos.

Solo existen:

```text
red.txt
purple.txt
```

Actualmente no importa porque el objetivo activo es solo rojo.

---

## 48.10 Purple no está sincronizado

El archivo `purple.txt` y el `COLOR_DATA["purple"]` actual no corresponden a la misma distribución.

No afecta al target rojo actual.

---

# 49. Qué archivos son “fuente de verdad” ahora

Para configuración de hardware:

```text
firmware/src/app/AppConfig.h
```

Para mapa de sensores:

```text
firmware/src/app/SensorMap.h
```

Para modos:

```text
firmware/src/tests/TestModes.cpp
groundstation/tests/modes.py
```

Para P03:

```text
firmware/src/tests/P03YawControl.cpp
```

Para P04:

```text
firmware/src/tests/P04AltitudeControl.cpp
```

Para P05 actual:

```text
firmware/src/tests/P05YawAltitude.cpp
```

Para visión:

```text
vision/perception_subsystem.py
```

Para P08–P11:

```text
firmware/src/mission/BalloonMission.cpp
firmware/src/mission/BalloonMission.h
```

Para comandos:

```text
groundstation/run_test.py
```

Para COM/MAC:

```text
groundstation/user_parameters.py
```

---

# 50. Checklist antes de una prueba de vuelo

```text
[ ] Firmware recompilado con los fuentes actuales
[ ] Base Station cargada
[ ] COM correcto
[ ] MAC correcta
[ ] Serial Monitor cerrado
[ ] Nicla ejecutando perception_subsystem.py
[ ] Nicla detecta rojo en P06
[ ] BNO responde en P01
[ ] BMP390 calibró cero al boot
[ ] Servos verificados con P00
[ ] ESC verificados con P00M
[ ] Orientaciones físicas no cambiaron
[ ] Batería segura
[ ] Zona libre
[ ] Blimp asegurado para pruebas iniciales
[ ] Potencias bajas al comenzar
[ ] Logs habilitados
```

---

# 51. Flujo de desarrollo recomendado a partir de este estado

Actualmente el paso lógico es:

1. terminar de validar P05 v3;
2. obtener un rango real de `altP` que mantenga sustentación;
3. validar cuánto `yaw-servo-authority` permite girar sin perder Z;
4. guardar ganancias finales de P04/P05;
5. migrar `BalloonMission` a la arquitectura por fases;
6. agregar adquisición de altura antes de SEARCH;
7. mantener Z durante SEARCH;
8. crear ALIGN separado;
9. después agregar APPROACH con tilt de avance;
10. caracterizar compensación vertical por tilt;
11. dejar Roll inicialmente como variable de seguridad;
12. solo después considerar control activo de Roll.

Arquitectura objetivo:

```text
CALIBRATION / ALTITUDE_ACQUIRE
             ↓
           SEARCH
             ↓
            ALIGN
             ↓
 APPROACH (Z + Yaw + X)
             ↓
       VISIT_CONFIRM
             ↓
           ESCAPE
             ↓
     WAIT_TARGET_LOST
             ↓
      ALTITUDE_RECOVER
             ↓
           SEARCH
             ↓
       visited == 4
             ↓
            DONE
```

---

# 52. Resumen de comandos más usados

## Sensores

```powershell
python run_test.py p01 --seconds 30
```

## Servo 1 a 35°

```powershell
python run_test.py p00 --servo 1 --servo-angle 35 --seconds 10
```

## Servo 2 a 85°

```powershell
python run_test.py p00 --servo 2 --servo-angle 85 --seconds 10
```

## Motores 10 % vertical

```powershell
python run_test.py p00m --motor both --power 10 --servo1-angle 35 --servo2-angle 85 --motor-seconds 3
```

## Manual

```powershell
python run_test.py p02
```

## Yaw

```powershell
python run_test.py p03 --yaw-deg 0 --yaw-kp 0.10 --yaw-kd 0.03 --yaw-min-power 7 --yaw-max-power 8.5 --yaw-deadband-deg 5 --seconds 60
```

## Altura

```powershell
python run_test.py p04 --height -1 --kp 0.30 --ki 0.025 --kd 0.01 --max-power 12 --slew 0.18 --seconds 60
```

## Altura + yaw P05 v3

```powershell
python run_test.py p05 --height 0 --yaw-deg 0 --kp 0.30 --ki 0.025 --kd 0.01 --alt-min-power 6 --max-power 15 --slew 0.18 --alt-success-cm 10 --yaw-kp 0.10 --yaw-kd 0.03 --yaw-min-power 7 --yaw-max-power 8.5 --yaw-success-deg 10 --yaw-servo-authority 30 --seconds 120
```

## Nicla RAW

```powershell
python run_test.py p06 --seconds 60
```

## Centrado visual

```powershell
python run_test.py p07 --vis-kp 0.10 --vis-kd 0.015 --vis-min-power 6 --vis-max-power 10 --vis-deadband-px 15 --seconds 60
```

## Un globo

```powershell
python run_test.py p08 --vis-kp 0.10 --vis-kd 0.015 --vis-min-power 6 --vis-max-power 10 --vis-deadband-px 15 --seconds 120
```

## Visita + escape

```powershell
python run_test.py p09 --vis-kp 0.10 --vis-kd 0.015 --vis-min-power 6 --vis-max-power 10 --vis-deadband-px 15 --seconds 180
```

## Dos globos — estado actual

```powershell
python run_test.py p10 --height 1.00 --kp 0.30 --ki 0.025 --kd 0.01 --max-power 15 --slew 0.18 --vis-kp 0.10 --vis-kd 0.015 --vis-max-power 10 --seconds 180
```

## Cuatro globos — estado actual

```powershell
python run_test.py p11 --height 1.00 --kp 0.30 --ki 0.025 --kd 0.01 --max-power 15 --slew 0.18 --vis-kp 0.10 --vis-kd 0.015 --vis-max-power 10 --seconds 600
```

---

## Fin

Este README representa el estado del código fuente revisado el **17-08-2026**.  
Antes de cambiar la arquitectura de P10/P11, conservar esta versión como punto de recuperación.
