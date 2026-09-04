# Arquitectura de mi-blimp-mio

**Para:** capítulo de análisis/diseño de la tesis.
**Alcance:** explica CÓMO funciona cada pieza del sistema y POR QUÉ está diseñada así — no es un manual de uso (eso es el `README.md` raíz), es la arquitectura interna.

---

## 0. Panorama general

El sistema tiene **cinco subsistemas** que se comunican entre sí:

```text
┌─────────────────┐   UART iBus 32B    ┌──────────────────────┐
│  Nicla Vision    │ ─────────────────► │  XIAO ESP32-S3       │
│  (percepción)    │   115200 baud      │  (firmware, "cerebro")│
└─────────────────┘                    └───────────┬──────────┘
                                                     │ ESP-NOW (radio 2.4GHz)
                                                     │ paquetes ControlInput (13 floats)
                                        ┌────────────▼──────────┐
                                        │  ESP32 Base Station    │
                                        │  (puente Serial↔radio) │
                                        └────────────┬───────────┘
                                                      │ USB Serial 115200
                                       ┌──────────────┴───────────────┐
                                       │                               │
                              ┌────────▼────────┐          ┌──────────▼─────────┐
                              │  run_test.py     │          │ telemetry_bridge.py │
                              │  (CLI, un test    │          │ (WebSocket, sesión  │
                              │   a la vez)       │          │  larga, multi-cliente)│
                              └───────────────────┘          └──────────┬──────────┘
                                                                          │ WebSocket
                                                               ┌──────────▼──────────┐
                                                               │  App AeroStock       │
                                                               │  (Flutter, celular)  │
                                                               └──────────────────────┘
```

Dos caminos de control **paralelos e independientes** llegan al mismo firmware:
la consola (`run_test.py`) y la app (a través de `telemetry_bridge.py`). Nunca
corren los dos a la vez porque ambos abren el mismo puerto Serial de la base
station. El firmware no distingue de dónde vino el comando — para él, todo
comando es un mismo tipo de paquete (`ControlInput`).

---

## 1. Hardware y su rol

| Componente | Rol | Por qué esa elección |
|---|---|---|
| **XIAO ESP32-S3** | Corre el firmware principal: lee sensores, ejecuta el lazo de control a ~250 Hz, decide qué actuar. | Dual-core 240 MHz, tamaño/peso mínimos (crítico en un blimp: cada gramo es sustentación que hay que generar). |
| **BNO085/086** | IMU: da roll/pitch/yaw absolutos y velocidades angulares vía fusión de sensores integrada en el chip (no hay que fusionar acelerómetro+giroscopio+magnetómetro en el firmware). | Delegar la fusión al chip ahorra ciclos de CPU y evita reimplementar un filtro de Kalman/Madgwick. |
| **BMP390** | Barómetro: altura relativa al punto de arranque (se recalibra el "cero" en cada boot). | Un blimp no puede usar un rango-láser/ultrasonido de forma confiable con una envolvente de tela moviéndose cerca; el barómetro es inmune a eso. |
| **Nicla Vision** | Corre OpenMV/MicroPython, detecta el color del globo, manda `[flag, x, y, w, h, ...]` por UART. | Desacopla el procesamiento de imagen (pesado) del lazo de control (que necesita determinismo a 250 Hz) — si la visión se cuelga o tarda, el control sigue corriendo. |
| **2× motor brushless + ESC** | Generan el empuje. | Mucho mejor relación peso/empuje que motores DC con escobillas. |
| **2× servo K-Power P0025** | Inclinan cada motor (nacela) para vectorizar el empuje entre "arriba" y "adelante/atrás/giro". | Con solo 2 actuadores de dirección (no 4 como un dron), el vector de empuje de CADA motor cubre tanto sustentación como traslación/rotación — más simple y liviano que agregar actuadores dedicados por eje. |
| **ESP32 "Base Station"** | Puente Serial(USB)↔ESP-NOW. No ejecuta ninguna lógica de vuelo. | El PC no tiene radio ESP-NOW; la base es solo un adaptador de protocolo. |

**Pinout activo** (fuente de verdad: `firmware/src/app/AppConfig.h`): D0/D1 servos,
D2 reset del BNO (reservado, NUNCA reusar), D4/D5 I2C, D6/D7 UART Nicla, D8 ADC
batería, D9/D10 ESC.

---

## 2. Arquitectura del firmware

### 2.1 Capas (de abajo hacia arriba)

```text
lib/BlimpSwarm/          <- librería heredada del proyecto original (Lehigh
  robot/                    University, "BlimpSwarm"/"MochiSwarm"). Clases de
    Robot.h (interfaz)       bajo nivel: Robot es la interfaz abstracta,
    RawBicopter               RawBicopter implementa actuadores+ESC+servos
    FullBicopter              crudos, FullBicopter le agrega sensores (BNO,
                               BMP, Nicla) y un control() heredado (poco usado
                               por este proyecto, que prefiere mandar
                               comandos ya resueltos vía commandMotorPowerTest/
                               commandServoCalibrationUs).
  act/                      AServo, BLMotor: wrappers de PWM (ESP32Servo).
  sense/                    NiclaSuite, BNO/BMP wrappers.
  comm/                     BaseCommunicator + LLC_ESPNow: abstrae el
                             transporte (podría cambiarse por otro radio
                             sin tocar el resto).

src/app/                 <- capa de aplicación de ESTE proyecto (no heredada)
  AppConfig.h                Constantes: pines, modos, calibración P0025,
                              índices de ControlInput.
  AppContext.h                El "estado global": robot, comm, sensores,
                               comando actual, modo actual.
  ControlCommon.h              NÚCLEO DE CONTROL compartido: PID de altura,
                               cascada de yaw, geometría de servos, mixer,
                               y (nuevo) la asignación analítica de empuje.
  Telemetry.cpp                Arma y manda las 5 tramas de telemetría.
  HardwareSafety.h              Secuencia de boot seguro (D2 HIGH, PWM en
                               alta impedancia, sin auto-arm).

src/tests/                <- una prueba = un archivo, P00 a P07
  TestModes.cpp               Dispatcher: mode -> función de test.
  P0X....cpp                   Cada P0X-P07 es relativamente independiente
                               (usa ControlCommon pero no BalloonMission).

src/mission/              <- P08-P11 comparten UNA sola máquina de estados
  BalloonMission.cpp/h        SEARCH -> APPROACH -> VISIT_CONFIRM -> ESCAPE
                               -> WAIT_TARGET_LOST -> (siguiente globo o DONE)

src/main.cpp              <- setup()/loop(): pega todo. Ver 2.2.
```

**Por qué esta separación:** el proyecto empezó (ver `legacy/`) como un
firmware monolítico donde cada prueba tenía su propia copia de las
ganancias/geometría — sintonizar en una prueba no servía para la siguiente.
La refactorización a `app/ControlCommon.h` compartido es la decisión de
diseño más importante del firmware: **una sola fuente de verdad para
constantes de control**, así lo que se sintoniza en P03/P04/P05 se hereda
automáticamente en P07 y en la misión.

### 2.2 El bucle principal (`main.cpp`)

```cpp
void loop() {
    processCommands();                 // ¿llegó un ControlInput nuevo? aplicarlo
    app.sensorCount = app.robot->sense(app.sensors);   // leer BNO+BMP+batería+Nicla
    TestModes::run(app);               // ejecutar la lógica del modo actual
    // esperar hasta completar CONTROL_LOOP_US (~4000us => ~250 Hz)
}
```

Tres cosas importantes de este diseño:

1. **Un solo `ControlInput` en vuelo.** No hay cola de comandos: el paquete
   más reciente reemplaza al anterior. Esto es intencional — en control en
   tiempo real no querés estar ejecutando comandos viejos.
2. **`processCommands()` es el único lugar que arma/desarma o cambia de
   modo.** Cualquier cambio de modo pasa por `TestModes::onModeEnter()`,
   que resetea integrales/estado — evita que P05 arranque con el integral
   de yaw contaminado de una prueba anterior.
3. **El período del lazo es fijo, no reactivo.** Aunque `TestModes::run()`
   tarde menos, se espera hasta completar los 4 ms. Esto le da al
   controlador un `dt` predecible, necesario para que las ganancias PID
   sintonizadas en un test sigan siendo válidas en otro.

### 2.3 El protocolo `ControlInput` (13 floats)

```text
[0]  MODE     -- qué prueba/modo ejecutar
[1]  FX       -- fuerza horizontal deseada (según el modo)
[2]  FZ       -- fuerza vertical deseada / altura objetivo (según el modo)
[3]  TX       -- (varía según modo)
[4]  TZ       -- (varía según modo, frecuentemente yaw objetivo)
[5..9]  AUX0..AUX4  -- 5 huecos genéricos, su significado depende del modo
[10] ARM      -- >0.5 arma, <-0.5 desarma
[11] RELOAD   -- releer preferencias NVS
[12] RESET    -- reinicia el estado del modo actual sin cambiar de modo
```

**Por qué "genérico" y no un struct por modo:** cambiar el firmware para
agregar un campo específico a cada prueba habría significado tocar el
protocolo de radio cada vez. En cambio, cada prueba interpreta AUX0-AUX4 a
su manera (documentado en cada archivo `PXX....cpp`) — el costo es que hay
que leer el código fuente de cada prueba para saber qué significa cada
AUX, pero la ventaja es que el protocolo de transporte nunca cambió desde
P00 hasta P11.

**El problema de los 9 huecos y P05:** P05 necesita 2 referencias + 6
ganancias + 8 límites — mucho más que 9 floats. La solución (ver
`groundstation/common/control_pack.py`) fue empaquetar varios límites
enteros pequeños dentro de UN SOLO float usando manipulación de bits
(`pack_alt`/`pack_yaw`), con un bit marcador de versión para detectar si
el firmware recibe un paquete del formato viejo. Esto es un patrón de
diseño clásico de sistemas embebidos con ancho de banda de mensaje fijo.

### 2.4 Modelo de control: geometría del bicóptero

Cada servo va de `Z_DEG` (vector vertical) a un extremo (`FORWARD_DEG`,
`BACK_DEG`, `LEFT_DEG` o `RIGHT_DEG`, según el eje que se esté
comandando). La función central es:

```cpp
computeEfficiencies(servo1Deg, servo2Deg, etaZ, etaYaw)
  phi1 = servo1 - SERVO1_Z_DEG
  phi2 = SERVO2_Z_DEG - servo2        // signo invertido: montaje espejado
  etaZ   = 0.5*(cos(phi1) + cos(phi2))   // fracción de la potencia que es empuje vertical
  etaYaw = 0.5*(|sin(phi1)| + |sin(phi2)|) // fracción que es empuje lateral
```

Esta es la pieza matemática que conecta "ángulo de servo" con "cuánta
sustentación real estoy generando" — sin ella, cualquier controlador que
incline los servos para girar o avanzar perdería sustentación sin que el
lazo de altura lo supiera compensar.

**`SERVO2_Z_DEG` no es una constante arbitraria: es una calibración física.**
La góndola izquierda tiene ~10° de desalineación mecánica de fábrica/montaje.
En vuelo (23/08) se midió que con `SERVO2_Z_DEG=85°` el blimp giraba solo
(~28°/s) incluso con ambos motores a la misma potencia — la asimetría
generaba un par de yaw permanente. Subiendo a 95° el ascenso sale limpio.
Esto ilustra un punto de análisis importante: **una constante de "geometría
ideal" en el código en realidad está compensando una imperfección de
fabricación real**, y por eso el propio código deja anotado "pendiente:
confirmar si el óptimo es exactamente 95, o enderezar la góndola
mecánicamente" — la compensación por software es un parche, no la
solución de fondo.

### 2.5 Lazo de altura (PID)

```text
error = Zref - Z
P = Kp * error
I = integral condicional (se congela fuera de una zona de ±1.5 m, anti-windup)
D = -Kd * Vz_filtrada   (Vz viene de un filtro exponencial, alpha=0.82,
                          porque derivar la altura cruda amplifica ruido)
salida = clamp(P + I + D, 0, maxPower)
```

Sin empuje activo hacia -Z (el diseño asume que si hay exceso de altura,
alcanza con bajar potencia y dejar que el descenso sea pasivo por la
física del sistema — no está validado un empuje activo hacia abajo).

### 2.6 Cascada de yaw

Es una cascada de DOS lazos, no un PD simple:

```text
lazo EXTERNO: error de ángulo (rad) -> velocidad angular deseada (rad/s)
              (con el error truncado a 36° para no pedir velocidades
               absurdas ante errores grandes)
lazo INTERNO: error de velocidad -> mando de potencia (PID sobre el
              giroscopio, no sobre el ángulo)
```

**Por qué cascada y no un PD directo sobre el ángulo:** un blimp tiene MUCHA
inercia rotacional y CASI NADA de amortiguamiento aerodinámico en yaw
(constante de tiempo medida ~125 s). Un PD directo sobre el ángulo con
ganancias que no saturan tarda demasiado en asentarse; controlando
velocidad angular en el lazo interno se logra un amortiguamiento mucho más
fuerte y predecible (las ganancias documentadas, `DEF_YAW_KD=9.0`, se
llegaron a esa magnitud sintonizando explícitamente contra ese
amortiguamiento aerodinámico casi nulo).

### 2.7 El mixer: de "dos lazos" a "dos servos + dos motores"

`mixOutputs()` combina la demanda de altura y el mando de yaw en un solo
comando físico:

```text
blend = f(yawCmd)                          // hacia qué lado y cuánto inclinar
servo1,servo2 = interpolar(Z, LEFT/RIGHT, blend)
etaZ, etaYaw = computeEfficiencies(servo1, servo2)
pAlt = altitudeDemand / etaZ               // potencia para sostener altura A PESAR del tilt
pYaw = interpolación de potencia según |yawCmd|
potencia_final = max(pAlt, pYaw)           // ninguno de los dos lazos se queda sin actuador
```

Este mixer usa la MISMA potencia para los dos motores (solo varía el
ángulo del servo). Fue suficiente para P03-P07, pero tenía una limitación
real que se descubrió en la fase de avance de la misión:

### 2.8 La asignación analítica de empuje (`computeThrustAllocation`, nueva)

**El problema que resolvió:** en la fase de avance de `APPROACH`
(BalloonMission), el diseño original inclinaba los servos a una fracción
FIJA (0.50) del recorrido hacia "avance completo", y después intentaba
compensar la sustentación perdida subiendo la potencia. El problema: a
blend=0.50 la eficiencia vertical real es ~0.71 — el 71% de la potencia
del motor sigue siendo empuje vertical aunque la intención sea "avanzar".
Combinado con un piso de potencia fijo (`MOVE_POWER=0.20`), el blimp
generaba más sustentación de la necesaria mientras avanzaba y **subía en
vez de mantenerse nivelado**.

**La solución** está tomada directamente del modelo de control del paper
en el que se basa este proyecto — *MochiSwarm: A testbed for robotic
blimps in realistic environments* (Xu et al., 2025, arXiv:2503.03077),
ecuaciones (4), (5), (9)-(11) — que resuelve el problema de asignación de
empuje de forma cerrada en vez de por prueba y error:

```text
dado: fx (empuje horizontal deseado), fz (empuje vertical deseado)
potencia = sqrt(fx² + fz²)
ángulo_desde_vertical = atan2(|fx|, fz)     // 0 = puro vertical, 90° = puro horizontal
```

Con esto, el ángulo de servo y la potencia YA NO se fijan de antemano —
se calculan exactamente para la combinación de avance+altura que se pide
en cada instante. Con `fx=0` el resultado es matemáticamente idéntico al
vector vertical de siempre (cero regresión); con `fx>0` el sistema
inclina "lo justo y necesario", nunca más. Además prioriza la altura sobre
el avance si la potencia disponible no alcanza para ambos (recortando el
avance, nunca la sustentación) — el mismo criterio de seguridad que ya
usaba `mixOutputs`.

**Punto de análisis para la tesis:** esto es un buen ejemplo de la
diferencia entre una heurística ajustada empíricamente (fracción fija +
compensación de potencia) y una solución de forma cerrada derivada del
modelo dinámico del vehículo — la heurística "funciona" dentro de un rango
limitado de operación, pero desperdicia autoridad de control fuera de ese
rango; la solución analítica generaliza sin necesidad de volver a ajustar
constantes a mano.

---

## 3. Percepción visual (Nicla Vision)

`vision/perception_subsystem.py` corre en la Nicla bajo OpenMV/MicroPython.
**No hay red neuronal ni "entrenamiento" en el sentido de machine learning
supervisado** — el pipeline es:

```text
imagen RGB565
  -> espacio de color LAB
  -> grilla 14x21 celdas
  -> por celda: distancia de Mahalanobis de (A,B) a una gaussiana 2D
     ajustada al color del globo (media + matriz de covarianza inversa)
  -> filtro probabilístico temporal por celda (creencia bayesiana simple,
     no un solo frame)
  -> blob más grande que pasa el filtro de forma (circular/rectangular)
  -> [flag, x_roi, y_roi, w_roi, h_roi, x, y, w, h, distancia=9999] por UART
```

"Entrenar" significa: capturar muchos pares (A,B) del color real del globo
bajo distintas condiciones de luz/ángulo/distancia, calcular su media y
matriz de covarianza (`calibration/gaussian_manual_multi.py`, en la PC), y
copiar esos dos números a `COLOR_DATA` en el script de la Nicla. Es
clasificación estadística clásica (distancia de Mahalanobis a una
distribución gaussiana), no aprendizaje profundo — una elección deliberada
dado el presupuesto de cómputo de la Nicla y la necesidad de correr a
~10 Hz de forma determinística.

**`nicla_distance` siempre vale 9999** — es un campo del protocolo que
existía cuando el proyecto tenía un sensor ultrasónico (retirado, ver
`legacy/unused_sensors/`); se dejó el campo relleno en vez de romper el
formato de mensaje de 10 valores. El proxy real de "qué tan cerca estoy"
que usa la misión es el ancho del blob (`nicla_w`), no la distancia.

---

## 4. Comunicación

### 4.1 Nicla -> XIAO

UART tipo iBus, 32 bytes, 115200 baud, ~10 Hz. Un solo sentido
(Nicla->XIAO); el XIAO puede mandar un byte de vuelta para cambiar de modo
(0x40 = modo globo).

### 4.2 XIAO <-> Base Station

ESP-NOW (radio 2.4 GHz de bajo nivel de Espressif, sin necesidad de
router/AP — por eso "sin infraestructura externa", un requisito del propio
paper de MochiSwarm para operar en campo). El paquete es el
`ControlInput` de 13 floats descrito en 2.3, mandado desde la base hacia
el robot; la telemetría va en sentido inverso.

### 4.3 Base Station <-> PC

Serial USB, 115200 baud. La base es un traductor puro Serial<->ESP-NOW: no
interpreta el contenido de los paquetes, solo los pasa. Formato de
telemetría desde la base hacia el PC (texto plano, fácil de parsear y de
inspeccionar a ojo en un monitor serie):

```text
TEL,flag,v0,v1,v2,v3,v4,v5
```

5 "flags" (grupos de 6 valores) rotan a 10 Hz cada uno: F1 altura/actitud/
batería, F2 Nicla, F3 actuadores, F4 control (ganancias/errores), F5
misión.

### 4.4 PC <-> App (WebSocket, `telemetry_bridge.py`)

Este es el puente que se agregó para que la app móvil pudiera controlar el
robot sin reimplementar el protocolo de la base station en Dart. Rol:

```text
Serial (mismo protocolo que run_test.py)
   <-> telemetry_bridge.py (asyncio, un solo proceso)
   <-> WebSocket ws://<ip-laptop>:8765
   <-> N clientes (viewers de solo lectura + como máximo un "dev" con permiso de comando)
```

Decisiones de diseño clave:

- **Un solo lector de Serial** (`read_loop`), compartido por todos los
  clientes vía `broadcast()`. Si cada cliente abriera su propia conexión
  Serial, se pisarían.
- **Los comandos se escriben directo al Serial sin esperar el ACK de
  vuelta** — si el bridge esperara una línea de respuesta después de cada
  comando, competiría con `read_loop` por las mismas líneas entrantes y
  ambos leerían basura del otro. El efecto del comando se confirma por la
  telemetría normal (ej. el campo `armed` cambiando), no por un ACK
  síncrono.
- **Roles viewer/dev por token**, no por usuario/contraseña — suficiente
  para un dispositivo de laboratorio en una red de confianza, evitando la
  complejidad de un sistema de autenticación real.
- **`common/control_pack.py` es compartido bit a bit con `run_test.py`**:
  ambos importan las mismas funciones de empaquetado, así que un comando
  mandado desde la app y el mismo comando mandado por consola producen el
  IDÉNTICO paquete de bytes. Esto evitó un bug real de este proyecto: la
  primera versión de la app tenía su propia lógica de armado de paquetes,
  desincronizada de la de `run_test.py`, y mandaba valores a índices AUX
  equivocados.

---

## 5. Ground station (Python)

| Archivo | Rol |
|---|---|
| `common/link.py` | `BlimpLink`: abre el Serial, arma/lee paquetes `ControlInput` crudos. Es la única pieza que sabe el formato binario exacto. |
| `common/telemetry.py` | Parsea líneas `TEL,...` a un objeto `Telemetry(flag, values)` + `LABELS` (nombre de cada campo por flag). |
| `common/csv_logger.py` | Escribe cada `Telemetry` recibido a un CSV con nombres de columna legibles. |
| `common/control_pack.py` | El empaquetador de bits (`pack_alt`/`pack_yaw`) y el armador de payload por modo (`build_control_payload`) — la lógica de protocolo más delicada del proyecto, ahora compartida entre CLI y bridge. |
| `run_test.py` | CLI interactiva: un test por invocación, valida parámetros, arma con confirmación tecleada, corre el test, desarma al salir. |
| `telemetry_bridge.py` | Servidor WebSocket de sesión larga para la app (ver 4.4). |
| `analyze.py` | Post-procesamiento offline de un CSV: gráficas de altura/yaw/servos/motores para ajustar ganancias. |

**Por qué la CLI valida tanto (rangos, cuantización) y el bridge menos:**
`run_test.py` es la herramienta de un humano tecleando comandos sueltos —
tiene sentido rechazar valores fuera de rango con un mensaje claro. El
bridge recibe comandos ya generados por la UI de la app (que ya limita los
sliders a rangos válidos), así que su validación es más liviana; los
límites "duros" de seguridad (potencia, ángulos) siguen estando en las
funciones de empaquetado compartidas, no en cada punto de entrada por
separado.

---

## 6. App móvil (AeroStock, Flutter)

```text
lib/
  main.dart                 MaterialApp + tema
  core/theme/app_theme.dart Paleta y ThemeData centralizados
  models/telemetry_message.dart   Parseo del JSON de telemetría
  services/telemetry_service.dart  EL ESTADO DE LA APP (ChangeNotifier):
                                     conexión WebSocket, telemetría acumulada,
                                     historial para gráficas, modo demo,
                                     y todos los métodos de comando (arm,
                                     startMission, setServo, etc.)
  screens/
    home_screen.dart         Landing: viewer / dev / demo
    viewer_screen.dart        Solo lectura
    dev_screen.dart            Todos los paneles de control (misión,
                                 P04/P05, P02 manual, banco de pruebas P00/P00M)
  widgets/
    telemetry_widgets.dart     Gráficas, chips de estado, track de globos
    aerostock_logo.dart          Marca
    institution_footer.dart      Logos ESPOL/FIMCP
```

**Patrón de estado: un único `TelemetryService` (ChangeNotifier) inyectado
a cada pantalla**, no un gestor de estado más pesado (Provider/Bloc/Riverpod).
Justificado por el tamaño de la app: un solo flujo de datos (telemetría) y
un puñado de comandos, sin necesidad de composición de estado compleja.

**Por qué el modo demo genera datos localmente en vez de grabar una sesión
real:** permite iterar el diseño de la interfaz (gráficas, layout,
animaciones) sin tener el hardware encendido, y sirve como demostración
para terceros (comité de tesis, por ejemplo) sin depender de que el blimp
esté armado y en el aire en ese momento.

**Un detalle de Flutter con impacto real en este proyecto:** los paneles
con controles de texto (`_MissionPanel`, `_ControlPanel`, `_BenchTestPanel`,
`_ManualControlPanel`) usan `AutomaticKeepAliveClientMixin`. Sin eso, al
vivir dentro de una lista scrolleable, Flutter destruye y recrea el widget
cuando sale de pantalla — perdiendo cualquier valor tecleado. Es un detalle
de implementación, pero ilustra un problema real de UI con listas
virtualizadas que vale la pena documentar si la tesis discute la app.

---

## 7. Máquina de estados de misión (`BalloonMission`, P08-P11)

```text
        ┌─────────┐  ve un globo  ┌───────────┐  centrado y cerca  ┌───────────────┐
        │ SEARCH  ├──────────────►│ APPROACH  ├────────────────────►│ VISIT_CONFIRM │
        └────▲────┘               └─────┬─────┘                    └───────┬───────┘
             │                          │ pierde el globo                  │ 3 frames confirmando
             │                          ▼                                  ▼
             │                      SEARCH                            ┌─────────┐
             │                                                        │ ESCAPE  │ (1.8 s, aleja del globo)
             │  8 frames sin ver nada, y ya giró >45°                 └────┬────┘
             │                                                             ▼
             │                                                   ┌──────────────────┐
             └───────────────────────────────────────────────────┤ WAIT_TARGET_LOST │
                                                                   └──────────────────┘
```

`visited==target_count` desde `WAIT_TARGET_LOST` -> `DONE` (desarma solo).

**Por qué el ciclo completo por cada globo, en vez de "detectar todos y
visitarlos en orden":** la Nicla no entrega un ID por globo (todos son
idénticos a propósito, parte del enunciado del problema). No hay forma de
saber si el globo que se ve ahora es "el mismo de antes" — por eso, tras
visitar uno, hay que alejarse (ESCAPE), esperar a perderlo de vista
(WAIT_TARGET_LOST) y girar al menos 45° antes de aceptar una nueva
detección como "el siguiente globo". Es una solución de bajo costo
sensorial a un problema de identificación que normalmente se resolvería
con visión más sofisticada (tracking con re-identificación) o con un
sensor adicional.

**P10/P11 todavía usan un supervisor de altura de prioridad distinto al
resto:** cuando el error de altura es grande, la misión se pausa por
completo y un lazo separado corrige antes de retomar — a diferencia de
P08/P09, que corrigen altura y persiguen el globo de forma simultánea todo
el tiempo. Es una decisión de robustez tomada para 2-4 globos (más tiempo
de vuelo total = más oportunidades de que la altura se desvíe mucho), no
una limitación técnica del mixer.

---

## 8. Flujo de datos end-to-end (ejemplo: lanzar P08 desde la app)

```text
1. App: usuario configura Kp/Ki/Kd/altura/potencia en el panel de Misión,
   toca "Iniciar misión".
2. App -> bridge (WebSocket): {"cmd":"mission_start","test":"p08", height:..., kp:..., ...}
3. bridge: llama common.control_pack.build_control_payload() -- LA MISMA
   función que usa run_test.py -- arma fx/fz/tx/tz/aux (con ALT_PACK/
   YAW_PACK empaquetados en bits donde corresponde).
4. bridge -> Serial: escribe 'C' + MAC + 13 floats (arm=1).
5. Base station: recibe por USB, reenvía por ESP-NOW al XIAO.
6. XIAO (processCommands): ve PARAM_ARM>0.5, arma actuadores; ve que el
   modo pedido (P08) es distinto al actual, llama TestModes::onModeEnter
   -> BalloonMission::configure(1,true) + reset().
7. bridge espera 4.2 s (tiempo real que tarda la secuencia de ARM del ESC).
8. bridge -> Serial: mismo paquete pero con reset=1 (arranca limpio,
   conservando las ganancias ya mandadas).
9. XIAO (loop, ~250 Hz): sense() lee BNO+BMP+Nicla; TestModes::run()
   despacha a BalloonMission::update(); la maquina de estados corre
   SEARCH -> ... segun lo que vea la Nicla; Telemetry:: manda F1-F5 por
   ESP-NOW a 10 Hz cada una.
10. Base station: reenvia la telemetria por USB como texto "TEL,flag,...".
11. bridge: parsea cada linea, la agrega al CSV de logs/, y la reenvia por
    WebSocket a todos los clientes conectados (broadcast).
12. App: TelemetryService recibe el JSON, actualiza el estado, notifica a
    los widgets -> las graficas y el track de globos se redibujan.
```

Este mismo flujo, sin los pasos 2-4 y 7-8 (o sea, sin el bridge en el
medio), es exactamente lo que hace `run_test.py p08 ...` por consola —
por diseño, los dos caminos convergen al mismo protocolo desde el paso 4
en adelante.

---

## 9. Resumen de decisiones de diseño (para la sección de análisis)

| Decisión | Alternativa descartada | Por qué |
|---|---|---|
| Protocolo `ControlInput` genérico de 13 floats | Un mensaje por tipo de comando | Simplicidad del transporte a costa de tener que documentar el significado de AUX por modo |
| Empaquetado de bits para límites (ALT_PACK/YAW_PACK) | Ampliar `ControlInput` | No romper el protocolo de radio ya validado en vuelo |
| Clasificación gaussiana + Mahalanobis para color | Red neuronal / clasificador entrenado | Presupuesto de cómputo de la Nicla y necesidad de determinismo a ~10 Hz |
| Cascada de yaw (velocidad interna, ángulo externo) | PD directo sobre el ángulo | Inercia rotacional alta + amortiguamiento aerodinámico casi nulo |
| Asignación analítica de empuje (MochiSwarm eqs.) | Blend fijo + compensación de potencia | El blend fijo desperdiciaba componente vertical fuera de su punto de ajuste; la solución cerrada generaliza |
| Reidentificación de globos por "girar 45° + timeout" | Tracking visual con ID persistente | La Nicla no distingue globos idénticos entre sí; esto evita necesitar hardware/visión adicional |
| Un solo `TelemetryService` compartido en la app | Gestor de estado más pesado (Bloc/Riverpod) | Un solo flujo de datos, la complejidad no se justifica |
| CLI y app comparten `control_pack.py` | Cada cliente arma su propio paquete | Evitó (y evita a futuro) que los dos caminos de control queden desincronizados |

---

## Referencias

- Xu, J., Vu, T., D'Antonio, D. S., & Saldaña, D. (2025). *MochiSwarm: A
  testbed for robotic blimps in realistic environments*. arXiv:2503.03077.
  — Modelo de dinámica/control del bicóptero (ecs. 4,5,9-11) y arquitectura
  de hardware en la que se basa este proyecto.
- BlimpSwarm (Lehigh University, AIRLab) — librería de robot/actuadores
  original de la que parte `firmware/lib/BlimpSwarm/`.
