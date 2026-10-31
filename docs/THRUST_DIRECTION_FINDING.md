# Retroceso persistente: comprobar la base, no retocar otra vez la mision

Log single_waypoint/20261027_130542_835381.jsonl, firmware19. Usuario confirma
retroceso durante sustentacion y alejamiento del waypoint. F12 fase0 produjo
396 muestras (~79s a200ms), ordenes medias48.787/68.787 grados y potencia15.2%.
Fase2 impulso solo4 muestras; el vehiculo no tiene avance predominante.
La base sigue48.5/68.5; versiones17-19 cambiaron frenado de altura, secuencia
y recuperacion, no esa base. No procede afirmar que resolvieron el retroceso.

48.5/68.5 fue aceptada por observacion del usuario en una prueba abierta
de5s el26octubre, a15%. Esa observacion limitada no certifica ausencia de
retroceso con montaje/carga/condiciones posteriores. No se tiene medida de
velocidad horizontal. El relato fisico actual indica que hay que revisar
direccion del empuje durante sustentacion antes de otra mision completa.

check_thrust_direction.py prepara prueba fija de5s a15%, sin vision ni PID.
Por defecto +4grados de avance sobre esa base:52.5/64.5. Es un CANDIDATO,
no una nueva calibracion aceptada ni cambio al firmware. Lee F3 y exige
angulos/potencia ordenados, ARM a0, voz antes de soltar, limite host total18s,
vigilancia de bateria/telemetria/potencia y STOP con confirmacion nueva.
P00M es un ensayo directo; requiere el proceso host operativo para la parada.

No ejecutar otra mision antes de observar si esa pose avanza, queda neutra,
retrocede o sale de lado. Si sale de lado incluso con pose fija y sin giro,
hay que comprobar direccion/montaje de las gondolas; un cambio de PID o
temporizadores no lo corrige. No se ha modificado la base de vuelo a ciegas.
Registrar tambien contactos, condicion de bateria y configuracion fisica.
