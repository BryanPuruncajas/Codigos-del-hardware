# Recuperacion local N03 — 2026-10-22

Cambios aplicados unicamente en Competencia. No se modificaron ESP, pines,
umbrales LAB, resolucion, baudrate ni formato iBUS (14 canales / 32 bytes).

- Seguimiento normal conserva su asociacion geometrica anterior.
- Ante perdida se permite recuperar un unico candidato F, sin otros F/P
  presentes, a <=60 px del ultimo centro y con cada dimension entre
  0.67 y 1.5 veces la anterior. Debe mantenerse compatible durante >=500 ms
  y >=5 observaciones. No entrega coordenadas durante esa confirmacion.
- Solo se permite dentro de 5 segundos desde la ultima observacion valida.
  Despues requiere comando R de ESP o reiniciar. R sigue borrando el tracker
  y aumentando reset_ack. No hay cambio de waypoint automatico por perdida.
- Es una heuristica geometrica, NO una garantia de identidad: otro globo
  parecido que sustituya al anterior cerca podria satisfacerla. No autoriza
  dar por validada una mision de vuelo ni el contacto fisico.
- Limpieza explicita de memoria una vez por segundo en UART y diagnostico.
  Su efecto sobre las pausas debe medirse en la placa, no se da por resuelto.
- LED verde intermitente: medida FULL/PARTIAL actual. Rojo intermitente:
  sin medida actual. Ninguno confirma recepcion de datos por la ESP.
- Diagnostico: gc_ms mide limpieza; max_interval_ms mide el mayor intervalo
  entre inicios de bucle, incluyendo esperas entre llamadas no visibles
  en capture_ms/detect_ms/tx_ms. FPS es promedio por ventana.

## Validacion realizada en PC

`python test_recovery.py`: 7 pruebas correctas. Sintaxis de los cuatro scripts
verificada y core identico en las dos versiones autonomas. Sin acceso a placa.

## Pendiente en Nicla (banco, no vuelo)

1. Nicla fuera de PCB, solo USB. Reabrir nicla_diagnostico_main.py actualizado
   y ejecutarlo desde OpenMV sin guardarlo como main.py.
2. Con imagen habilitada, mover lentamente 60 s, luego repetir sin imagen.
   Registrar DIAG y si el IDE deja de responder. Vigilar max_interval_ms;
   >=500 ms es incompatible con el limite de frescura actual de la ESP.
3. Forzar salto local de unos 40 px y mantenerlo: debe recuperar tras
   confirmacion, sin entregar datos viejos durante la perdida.
4. Dos globos o desaparicion prolongada: no debe seleccionar arbitrariamente.
5. Solo tras validacion, guardar nicla_balloon_mission_main.py como main.py.
   Este es autonomo, sin requerir n02_core.py en el almacenamiento de Nicla.

La transmision visual USB y su efecto sobre OpenMV siguen bajo investigacion.
Un write UART exitoso no acredita recepcion por la ESP; falta prueba extremo
a extremo antes de actuadores. No conectar USB con alimentacion de PCB.
