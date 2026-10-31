# Busqueda inicial autonoma, version 22

Proyecto Competencia/Blimp/Blimp/Codigos-del-hardware. Perfil calibrado P08/P09
de un waypoint. No cambia las pistas P10/P11, pines, Nicla ni app.

La prueba 20261027_224206_178521 no arranco porque el lanzador exigia un
objetivo confirmado ANTES de armar. Se elimina ese requisito: ahora exige
comunicacion N03 reciente y ACK completado, aunque la Nicla este en SEARCH.
Los ESC se preparan armados a potencia cero antes de activar la mision.

La ESP ejecuta SEARCH con la base52.5/64.5 y sin avance solicitado:

- Barrido de yaw +/-20 grados alrededor del rumbo inicial, periodo20s.
- Demanda inicial12%, techo de motor14%, suelo10%, misma rampa existente.
- Con barometro sano/recent, solicita velocidad vertical+0.04m/s mediante
  ajuste PI limitado; no garantiza esa velocidad fisica.
- Al detectar incremento barometrico relativo de0.40m, suspende la solicitud
  de ascenso y baja al suelo de potencia. Se mantiene el barrido hasta adquirir
  objetivo o agotar el tiempo. Es un limite barometrico provisional; no mide
  suelo, techo, contacto ni flotabilidad.
- Timeout embarcado20s sin objetivo. Sin feedback vertical util durante8s,
  tambien termina. Sin UART fresco durante1.5s o IMU no finita, termina antes.
- LOST terminal de Nicla solicita reinicioR/ACK con intervalo minimo3s.

FULL/PARTIAL ya confirmado por Nicla cancela el barrido y transfiere a la
aproximacion visual sin reiniciar la potencia a15% ni a cero. Se conserva el
trim aprendido, estado de yaw y angulos/potencia con rampa. La aproximacion
mantiene sus reglas de impulso, recuperacion y llegada estimada de v21.

El lanzador anuncia "Objetivo fijado. Aproximando." cuando F5 cambia a
APPROACH/VISIT_CONFIRM. F12fase5 identifica BUSCAR; F11version22 permite
rechazar firmware antiguo antes de armar. La comunicacion sigue con Ground.

Pruebas: arranque sin globo y sin avance, barrido/potencia acotados,
transferencia a aproximacion/impulso, limite relativo, timeout embarcado,
perdida de UART y feedback vertical, reloj wrap, preparacion sin objetivo
y rechazo de camara sin enlace antes de armar. Validacion fisica pendiente.
