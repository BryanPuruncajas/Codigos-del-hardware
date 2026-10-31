# Adaptacion continua de sustentacion, version 23

Proyecto Competencia/Blimp/Blimp/Codigos-del-hardware, perfil P08/P09 calibrado.

Evidencia: logs/single_waypoint/20261027_225452_868561.jsonl. La busqueda alcanzo
repetidamente14% mientras la altura barometrica paso de0.183 a-0.452m. El usuario
confirmo perdida de flotabilidad por desinflado. La altura es relativa y deriva;
ese cambio no constituye una medida precisa de desplazamiento fisico.

Antes: techo fijo14% en SEARCH,16% en APPROACH y aprendizaje ascendente congelado
en PARTIAL. Una potencia suficiente con mas helio dejaba de ser suficiente.

Ahora:

- SEARCH aprende el trim a partir del error de velocidad vertical; aumento o
  reduccion acotado a0.6 puntos porcentuales por segundo, banda muerta2cm/s.
  Se mantiene la solicitud de subida lenta4cm/s y el barrido de yaw.
- Cuando actua el limite barometrico relativo de subida, regula velocidad
  vertical cero conservando el trim aprendido. No vuelve bruscamente a10%.
- La transferencia SEARCH -> APPROACH conserva ese trim y la rampa de motores.
- FULL fiable sigue aprendiendo con la velocidad de la imagen. PARTIAL superior
  persistente tambien puede incrementar el trim, sin derivar un contorno cortado.
  La evidencia de ascenso ya en curso inhibe ese incremento direccional.
- HOLD adapta solo con velocidad barometrica sana, reciente y persistente750ms;
  el cambio aprendido durante una oclusion se limita a+/-2 puntos respecto a su
  entrada. Si comienza a caer, libera gradualmente el frenado de descenso previo.
  Sin feedback valido se congela el aprendizaje. Recuperar vision devuelve la
  autoridad al centrado visual.
- Se respeta el maximo configurado en la mision, con techo absoluto20% en estos
  perfiles. El lanzador de la prueba ahora solicita20%; las llamadas desde la
  app conservan el maximo que configure/envie la app. P03 sigue con16%.
- Los limites verticales efectivos incluyen la eficiencia de los angulos de
  servo para evitar integrar una demanda que los motores ya no pueden aplicar.
- F10trim/demand y F3potencia muestran la evolucion. El lanzador registra
  potencia_al_limite y rechaza comandos por encima de20%.

Se mantienen los timeouts de busqueda20s, perdida visual15s, fallo UART1.5s,
potencia cero al parar, pines, Nicla, geometria de servos y pistas P10/P11.
No se identifica helio ni fuerza fisica; se adapta al efecto vertical observado.
No se mide contacto, techo, suelo ni velocidad horizontal. La respuesta fisica
y la suficiencia del techo20% quedan pendientes de vuelo.

Pruebas: planta vertical simplificada con necesidad de soporte variable en
ambos sentidos; SEARCH aprendiendo por encima16% sin pasar20%; continuidad
del trim al entrar a APPROACH; aprendizaje de PARTIAL y HOLD; congelacion con
datos invalidos; limites de potencia y regresiones de busqueda/recuperacion/llegada.
