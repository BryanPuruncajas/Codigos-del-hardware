# Sustentación v33: envolvente con menos helio

2026-10-29. A petición del usuario se aumenta el margen de sustentación de la
prueba de un waypoint P08/P09.

- Semilla de sustentación: 18% (antes 15% al entrar directamente en aproximación
  y 12% durante búsqueda). No es una potencia fija de flotabilidad neutra.
- Máximo solicitado por el lanzador y límite superior del gobernador: 25%
  (antes 20%). El porcentaje expresa orden al ESC, no empuje medido.
- Se conserva la rampa y la adaptación con visión/barómetro; el controlador
  puede reducir potencia por sobrealtura o ascenso. Un límite menor solicitado
  por otro cliente sigue respetándose.
- La búsqueda entrega su trim aprendido a la aproximación sin volver a 15%.
- Se conservan ángulos de servo, ganancias de yaw, impulso de avance 0.03,
  llegada de 120 px, recuperación visual de 15 s y corte por batería suspendido
  en el lanzador de waypoint. Una mayor potencia cambia las fuerzas físicas,
  por lo que el resultado de vuelo todavía requiere comprobarse.

## Archivos

`firmware/src/mission/AdaptiveLift.h`: `START_TRIM=.18`, `HARD_MAX=.25`.
`firmware/src/mission/VisualSearch.h` y `BalloonMission.{h,cpp}`: semilla común.
`firmware/src/app/VisualBearing.h`: versión visible en F11 = 33.
`groundstation/check_single_waypoint.py`: pide máximo 25%, supervisa hasta
25% y exige firmware >=33 antes de armar. Su máximo de giro solicitado sigue
siendo 20%; no se aumentan las ganancias de yaw.

No se modifica Nicla, app ni pinout. La app u otros clientes conservan sus
parámetros: solo disponer del firmware nuevo no les impone 25% automáticamente.

## Estado

Compilación PlatformIO correcta (17.70 s); 14 comprobaciones del supervisor
Python correctas. `groundstation/lift_v33_tests.sh` pasó las pruebas nativas
de sustentación >20%, máximo 25%, descarga por sobrealtura, respeto de límites
menores, transición búsqueda-aproximación, pérdida UART, búsqueda, recuperación
de asociación, llegada, avance y gobierno vertical.

Pendiente de cargar en la ESP y validar físicamente la sustentación. El lanzador
rechazará la versión 32 hasta que se cargue este firmware, evitando una prueba
con límites distintos. La última carga confirmada sigue siendo v32 / 120 px.
