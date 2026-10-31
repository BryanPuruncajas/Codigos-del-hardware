# v35: motores intercambiados y ataque invertido de mayor ángulo

2026-10-29. El usuario confirmó motores derecho/izquierdo intercambiados y
retroceso durante los impulsos v34. También solicitó un ataque más pronunciado.

## Cambios

1. `firmware/lib/BlimpSwarm/src/robot/MotorRouting.h`: `SWAP_MOTORS=true`.
   El mando lógico M1 sale por D10 y M2 por D9. Los objetos/pines físicos
   siguen definidos como THRUST1=D9 y THRUST2=D10. Se aplica en las dos rutas
   reales de `RawBicopter.cpp`: `actuate` y `commandMotorPowerTest`.
2. `firmware/src/mission/ApproachProfile.h`: `FORWARD_SIGN=-1`, demanda
   horizontal 0.12 y límite de ataque 28°. La orden de impulso ahora disminuye
   S1 y aumenta S2, invirtiendo el sentido que el usuario observó como retroceso.
3. `SupportYaw::Mixer` admite un signo explícito y calcula el margen mecánico
   del lado correspondiente. La pose neutral permanece 52.5°/64.5°; al alcanzar
   el máximo negativo queda aproximadamente 24.5°/92.5°. El presupuesto de
   retorno aumenta a 1000 ms para 28° a 30°/s. No exige alcanzar el máximo si
   la demanda vertical o las rampas producen un ángulo menor.
4. F11 anuncia versión35. El lanzador exige >=35 antes de armar y mantiene
   máximo25%,120s, voz, corte estimado de batería suspendido y demás paradas.

Intercambiar dos potencias iguales no cambia la traslación. La corrección del
retroceso corresponde al signo del ataque, apoyada en la observación física
del usuario; el nuevo sentido aún requiere comprobarse físicamente. No se
cambia el sentido de giro eléctrico de los ESC ni se invierten señales PWM.

No se intercambian cables/pines de servo ni sus calibraciones neutrales. El
mapa de motores se aplica globalmente a las salidas de RawBicopter, incluyendo
pruebas directas y control manual. F3 mantiene los mandos lógicos M1/M2; para
relacionarlos con conectores físicos debe usarse el mapa D10/D9 anterior.

El signo/ataque nuevos se solicitan en la aproximación N03 de un waypoint
P08/P09. Otros clientes del mezclador mantienen signo+1 por defecto. Las rutas
completas P10/P11 conservan su lógica de avance previa y reciben el mapa global
de motores; no se afirma que todo el perfil de aproximación esté integrado allí.

Se mantienen los impulsos1.8s, corrección en inercia0.9s, memoria breve0.8s,
misma asociación, límites de pérdida y llegada estimada120px. La memoria no
confirma contacto. Las ganancias de yaw, pose de sustentación, Nicla y app no
cambian.

## Verificación

`groundstation/test_motor_routing.py` extrae y compila los cuerpos de las dos
funciones reales de salida contra actuadores falsos: verifica cruce lógico,
potencias iguales, saturación, servos conservados, desarmado y calibración.
No accede a puertos ni hardware.

`groundstation/mission_host/test_routing35.cpp` verifica el ataque negativo
28°, pose neutral, máximo25%, interbloqueo yaw y que la misión real solicite
el signo corregido. Ejecutor general: `groundstation/routing_v35_tests.sh`.

Estado: compilación correcta (56.35 s),14 pruebas Python y suite nativa completa
correctas. v35 cargado en COM4 el2026-10-29, ESP MAC DC:B4:D9:39:B3:B4,
843312 bytes escritos, hash verificado, reinicio RTS y PlatformIO SUCCESS
(16.05 s). No se ejecutaron motores durante la carga. El sentido y rendimiento
físico del nuevo ataque están pendientes de comprobar.
