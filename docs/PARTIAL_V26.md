# P08: aproximación desde PARTIAL confirmado

Proyecto: C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware

## Prueba física que identifica el bloqueo

Registro: logs/single_waypoint/20261028_003101_898773.jsonl.

La prueba terminó por plazo de 60 segundos después de la cuenta, con STOP confirmado. Durante la etapa P08 hubo 325 mensajes de estado Nicla PARTIAL, 645 mensajes de geometría con flag66, cero FULL y cero impulsos. El usuario confirmó contacto con techo u otro objeto. El contacto impide usar este vuelo como validación de respuesta vertical libre.

P08 aceptaba PARTIAL para seguimiento, pero exigía `approachHadFull_` tanto para iniciar como para conservar un impulso. Con cero FULL durante todo el recorrido, la orden de avance era imposible incluso en ventanas bien alineadas. El banco P12 usaba una ruta distinta y no cubría esa condición; su solicitud de avance no validaba la política P08.

## Cambio acotado

- PARTIAL confirmado por el tracker N03 puede habilitar aproximación sin un FULL anterior.
- Se conservan observación válida/reciente, confirmación temporal, centrado horizontal/vertical, límite de error de yaw y límite de velocidad de giro.
- Candidato sin confirmar, AMBIG y pérdida visual no habilitan avance.
- No cambia el filtro de Nicla, pines, geometría de servos, techo de potencia20% ni temporización de impulsos.
- La condición de llegada sigue separada: esta corrección no declara contacto ni relaja su requisito de FULL previo. Una aproximación exclusivamente parcial todavía no acredita llegada.
- El lanzador requiere versión26 para no volver a ejecutar involuntariamente el firmware con este bloqueo.

## Validación de software

`groundstation/mission_host/test_partial_start.cpp` compila el mismo BalloonMission.cpp de producción. Antes de la corrección falla al esperar un impulso desde PARTIAL confirmado. Después pasa y verifica también rechazo de observaciones cacheadas, mala alineación, giro rápido, candidato sin confirmar, ambigüedad, pérdida y separación de llegada.

Pasan además la regresión de misión (impulsos, recuperación, llegada, desarmado) y las ocho comprobaciones del lanzador. Esto prueba la corrección lógica, no la estabilidad física del blimp.

## Alcance pendiente

Validar en espacio libre que el mando corregido produce avance real y regula altura sin contacto. La observación FULL necesaria para la llegada y el equilibrio vertical siguen pendientes de validación. No usar P12 como prueba de la política P08; su alcance es enlace, detección y decisiones de su propia ruta de banco.

Nicla v5-wide ya está instalada; esta corrección requiere actualizar sólo ESP. No se solicita ni ejecuta otro vuelo automáticamente.

## Actualización de despliegue — 28 de octubre

Versión26 completa cargada en ESP por COM4, MAC dc:b4:d9:39:b3:b4. PlatformIO SUCCESS16,46s; 835808bytes escritos y hash de datos verificado. Incluye tanto avance desde PARTIAL como prioridad visual de altura. No se iniciaron motores durante la carga. Nicla no se modificó en esta carga. Validación física pendiente después de volver a montar y alimentar desde PCB.

## Corrección vertical incorporada antes de cargar versión26

El mismo registro muestra casos con el objetivo por debajo del centro y una petición de potencia superior al trim: el regulador de velocidad barométrica podía recuperar sustentación mientras persistía sobrealtura visual. En PARTIAL no había una derivada visual fiable que lo contradijera. Además, el aprendizaje anterior dependía de la velocidad de presión y el mando tenía mínimos de6/10% que no representaban flotabilidad medida.

Se incorpora error visual directamente a la corrección. Con PARTIAL por debajo del centro, una caída indicada únicamente por presión no puede invertir esa corrección y aumentar la sustentación. El error visual sostenido durante600ms adapta el trim (descenso máximo0,8 puntos porcentuales/s; incremento máximo0,4 puntos/s), con límites y antiwindup. Durante impulsos, movimiento visual fiable rápido o cambios de signo no se aprende ese trim. La predicción de movimiento visual fiable sigue permitiendo anticipar una caída antes de cruzar el centro.

La demanda vertical puede reducirse hasta el mínimo de mando del ESC; mantener un porcentaje fijo para conservar giro de hélices no puede garantizar altura si ese empuje ya supera lo necesario. No se añade empuje activo hacia abajo. En el centro se conserva el trim aprendido por debajo de los mínimos anteriores. El lanzador ya no exige10% para anunciar la liberación: exige armado confirmado, modo correcto y órdenes dentro del techo. F13 bit16 identifica reducción por sobrealtura visual.

La nueva regresión `groundstation/test_visual_height_priority.cpp` reproduce la petición contradictoria anterior (falla antes de la corrección y pasa después), comprueba adaptación por debajo de6/10%, conservación del trim, anticipación visual y congelación de aprendizaje. Continúan pasando las pruebas del P08 real y sus bloqueos. Estas son comprobaciones lógicas; la estabilidad física y la llegada permanecen pendientes.
