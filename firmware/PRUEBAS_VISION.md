# Registro inicial de pruebas de visión

Fecha de preparación: 29 de septiembre de 2026.

Proyecto activo de PlatformIO:
`C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\firmware`.

## Cambio de esta revisión

P07 y las misiones P08–P11 leen el centro horizontal y tamaño de la detección
(`NICLA_X_VALUE` y `NICLA_W_VALUE`). La misión también exige un ancho finito
y mayor que cero para aceptar la detección. Los campos `NICLA_X/W` son el
ROI del tracker y no se usan para estas decisiones.

P06 ya transmite estos campos mediante F2; por eso no requiere un cambio.
En los CSV pueden seguir llamándose `nicla_x`, `nicla_w`, etc.: son los
nombres históricos de la telemetría. El significado se verifica en
`src/app/Telemetry.cpp`.

Se conserva la configuración existente de pines, servos, ganancias de yaw
y altura, parámetros visuales y recepción/telemetría Flow. La secuencia con
identificación de globos 1 y 2 sigue pendiente. Esta revisión no demuestra
todavía que el centrado o la visita funcionen en vuelo.

## T0 detección con P06

1. Compilar y cargar el proyecto principal, usando el entorno
   `seeed_xiao_esp32s3` de este `platformio.ini`. `flow_node` es otro proyecto.
2. Arrancar desarmado y seleccionar P06 (modo numérico 15) sin enviar ARM.
   Si se viene de una prueba con motores activos, enviar DISARM o SAFE_STOP
   antes: entrar a P06 por sí solo no garantiza cortar la salida anterior.
3. Registrar 20–30 segundos del globo 1 en la posición de inicio, y repetir
   con el globo 2 desde la posición prevista de observación. Anotar distancia
   aproximada, iluminación y si hay otros globos en imagen.
4. Mover el objetivo a izquierda, centro y derecha. Acercarlo y alejarlo.
   Retirarlo del encuadre. Verificar cambios de coordenada, tamaño y flag.
5. Guardar el CSV y video con el mismo identificador de ensayo. Confirmar
   `armed=0` en F3 durante toda la prueba.

F2 contiene flag, x, y, ancho, alto y distancia de la Nicla. Primero confirmar
que x corresponde al objetivo visible y que el ancho permite evaluar
proximidad; todavía no fijar umbrales de identidad o visita. Un único tamaño
no identifica de manera permanente un globo cuando cambia la distancia.

## T1 centrado con P07

Después de validar T0, usar P07 (modo numérico 16) con un globo fijo y una
prueba supervisada. Registrar F2, F3 y F4, además del video. F4 incluye
`yaw_ref`, tasas angulares y `fx_cmd`, que en P07 es el error horizontal
en píxeles; F1 contiene yaw y altura.

Anotar los parámetros realmente enviados y la línea de configuración
`[P07] cfg` del monitor serial a 115200 baudios. P07 controla yaw y no
mantiene altura. Finalizar con DISARM o SAFE_STOP: todavía no hay watchdog
general de comandos.

Evaluar detección estable, sentido de corrección, sobrepaso, oscilaciones
y tiempo dentro de la banda. Conservar un ensayo base antes de cambiar
ganancias. Los parámetros visuales de P07 y misión siguen siendo distintos;
su unificación y el arreglo del respaldo de yaw requieren otra revisión.

## Bitácora de ensayos

| Ensayo | Fecha | Firmware y parámetros | Evidencia | Resultado observado | Decisión |
| --- | --- | --- | --- | --- | --- |
| T0 globo 1 | Pendiente | Registrar | CSV y video | Pendiente | Calibrar detección |
| T0 globo 2 | Pendiente | Registrar | CSV y video | Pendiente | Comparar tamaño inicial |
| T1 P07 | Pendiente | Registrar | CSV y video | Pendiente | Evaluar centrado |

La carga al ESP32 y la validación física deben anotarse por separado de
la compilación. Los cambios permanecen locales hasta un commit y push
explícitos.
