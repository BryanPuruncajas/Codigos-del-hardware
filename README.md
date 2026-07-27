# Mi Blimp — visita secuencial de 4 globos

Proyecto propio derivado de MochiSwarm/BlimpSwarm (Xu et al., 2025) y sus
repos de apoyo (`Blob-detection-and-Tracking`, `balltraining`), adaptado
a un solo blimp que visita 4 globos rojos en secuencia, sin enjambre y
sin entrega en aros.

Créditos: código base de https://github.com/LehighBlimpGroup — revisa su
LICENSE antes de republicar (incluida en `firmware/lib/BlimpSwarm/LICENSE`
original del repo).

## Estructura

```
firmware/         proyecto PlatformIO (Arduino C++), corre en el XIAO ESP32S3
  platformio.ini
  src/main.cpp    punto de entrada, copiado de examples/Bicopter4balloon
  lib/BlimpSwarm/ librería con act/, sense/, state/, comm/, robot/, util/

vision/           MicroPython/OpenMV, corre en la NiclaVision (con OpenMV IDE, no PlatformIO)
  perception_subsystem.py   detección de globo, mode=0 ya es el default
  get_gains.py              RGB gain de tu cámara

calibration/      Python de escritorio, se corre una sola vez en tu compu
  gaussian_manual_multi.py  ajusta la Gaussiana LAB (Ec. 1 del paper)
  colors/red.txt            AQUÍ van tus datos de color del globo rojo (vacío por ahora)

groundstation/    estación base, opcional para telemetría sin USB
  BaseTranseiver.ino        firmware de la ESP32 transmisora (ESP-NOW)
  user_parameters.py        direcciones MAC y config del enjambre (usa solo 1 robot)
  RobotControl.py           script Python de la estación base
```

## Orden para poner en marcha (checklist)

- [ ] **1. Sensores por USB (sin batería):** flashea `examples/TestSensors`
  de BlimpSwarm original o adapta `main.cpp` temporalmente para confirmar
  que el barómetro y el IMU responden por I2C.
- [ ] **2. Actuadores:** prueba servos + motores brushless por separado.
- [ ] **3. Altura + yaw:** valida el control PD sin cámara todavía.
- [ ] **4. Calibración de color:**
      1. Corre `get_gains.py` en la NiclaVision (OpenMV IDE) para tu cámara.
      2. Corre `vision/perception_subsystem.py`, apunta al globo rojo real
         por 30 s–2 min, copia las tuplas (A, B) impresas en consola a
         `calibration/colors/red.txt`.
      3. Corre `calibration/gaussian_manual_multi.py` para obtener el
         `COLOR_DATA` calibrado y pégalo de vuelta en `perception_subsystem.py`.
- [ ] **5. Integración cámara + blimp:** conecta la NiclaVision al ESP32
  por UART, flashea `firmware/` completo con PlatformIO
  (`pio run -t upload` desde la carpeta `firmware/`).
- [ ] **6. Lógica de 4 globos:** en `firmware/src/main.cpp` falta agregar
  el conteo de globos visitados y la transición de vuelta a búsqueda —
  no viene resuelto en el código original porque el paper usa una red que
  atrapa el globo (y así desaparece de la cámara). Define tu propio
  criterio de "visitado" (p. ej. `n_b` supera un umbral por X segundos).
- [ ] **7. (Opcional) Telemetría sin cable:** flashea `groundstation/BaseTranseiver.ino`
  en tu segunda ESP32, configura la MAC en `user_parameters.py`, y corre
  `RobotControl.py` para ver las lecturas del blimp por radio en vez de USB.

## Cómo subir esto a tu propio repo

```bash
cd mi-blimp-repo
git init
git add .
git commit -m "Estructura inicial: firmware, vision, calibration, groundstation"
git remote add origin <URL-de-tu-repo>
git push -u origin main
```
