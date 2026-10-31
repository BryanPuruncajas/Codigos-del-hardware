# Correccion de aproximacion, version 21

Proyecto: Competencia/Blimp/Blimp/Codigos-del-hardware.

Evidencia: logs/single_waypoint/20261027_222940_118537.jsonl.
La version 20 permanecio en ALIGN durante toda la prueba, sin impulsos.
Durante detecciones inferiores/parciales el trim quedo cerca de 0.149.
Al perder momentaneamente la deteccion, HOLD volvia a ese trim y anulaba
la reduccion de empuje solicitada para bajar. El usuario confirmo contacto
con el techo; la telemetria no mide contactos.

Cambios exclusivos del perfil calibrado de un waypoint P08/P09:

- Un objetivo persistentemente debajo del centro reduce gradualmente el
  trim incluso en PARTIAL, sin calcular derivadas de un contorno recortado.
  No se aprende esa reduccion si hay evidencia de descenso ya en curso.
- HOLD conserva el menor empuje de frenado/descenso. El barometro sano puede
  frenar ascenso tambien antes de haber capturado una referencia de altura.
- El impulso requiere objetivo confirmado y alineacion horizontal estable,
  pero permite error vertical de hasta 50 pixeles al iniciarlo. La altura
  sigue regulandose durante el impulso. Perdida visual cancela el avance.
- Entrada: x +/-24 px, y +/-50 px, yaw +/-8 grados, velocidad yaw <=0.12 rad/s;
  mantenimiento: x +/-40 px, y +/-65 px, yaw +/-12 grados, <=0.20 rad/s.
  Se mantiene la confirmacion temporal, separacion impulso/giro y retorno.
- Yaw P08/P09: kp 0.35, ki 0.08, kd 3.0 para frenar antes y reducir acumulacion.

Sin cambios de pines, protocolo UART, Nicla, app, potencia maxima 16%,
suelo de marcha 10%, base de servos 52.5/64.5 ni control de las pistas P10/P11.
No se incorpora deteccion de techo, contacto o velocidad horizontal.

Validacion de software: reproduce objetivo inferior parcial seguido de
oclusiones sin restaurar potencia de subida; verifica impulso con error
vertical, cancelacion por desalineacion, recuperacion, llegada y parada.
La respuesta fisica sigue pendiente; los umbrales son experimentales.
