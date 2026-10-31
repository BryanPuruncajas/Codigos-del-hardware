# Correccion de oscilacion vertical, version24

Proyecto Competencia/Blimp/Blimp/Codigos-del-hardware, perfil P08/P09 calibrado.

Video revisado: C:/Users/stefa/Downloads/WhatsApp Video 2026-10-27 at 11.15.21 PM.mp4,
duracion14.93s. Muestra descenso y nuevo ascenso repetidos. El usuario confirma
choques/rebotes con el techo: no son descensos logrados con empuje hacia abajo.
Se compara con el patron del log single_waypoint/20261027_231227_949320.jsonl,
sin asumir sincronizacion exacta entre fotogramas y telemetria.

Problemas encontrados en v23:

- El centro recortado anulaba la derivada visual; el control hacia una correccion
  de posicion tardia y el frenado barometrico tenia condiciones distintas por
  direccion/estado FULL/PARTIAL.
- El minimo fijo10% limitaba la reduccion de empuje mientras ya habia velocidad
  ascendente. No existe empuje vertical negativo para detener esa inercia.
- Los integrales de imagen y de PARTIAL podian aprender durante la oscilacion.
- Al inclinar para avanzar, la compensacion vertical subia RPM y restauraba
  parte de la sustentacion que se necesitaba reducir para frenar el ascenso.

Cambios:

- Un regulador vertical para RUN/HOLD, con velocidad barometrica filtrada,
  aceleracion limitada y prediccion0.5s. La imagen proyecta el error1.2s hacia
  delante cuando el objetivo completo permite una derivada fiable.
- Velocidad vertical solicitada limitada a+/-4cm/s; el frenado reduce potencia
  ANTES del centro si ya sube deprisa, o recupera sustentacion ANTES del centro
  si ya cae deprisa. Sigue siendo exclusivamente empuje positivo y gravedad.
- Durante frenado de ascenso o estando por encima del objetivo, se permite
  bajar hasta6% de comando, conservando10% en marcha normal. Es un valor
  experimental: no garantiza giro continuo del motor ni fuerza fisica conocida.
- Si una derivada visual fiable contradice al barometro, gana la imagen. No se
  calcula velocidad desde un centro recortado; sin datos fiables se congela el
  aprendizaje y en perdida se conserva la ultima reduccion, sin subir a ciegas.
- El trim aprende como maximo0.2 puntos porcentuales/s durante movimiento
  tranquilo tras1s; se congela durante impulsos, cambios bruscos y rebotes
  aparentes. No se incorpora deteccion fisica de contacto.
- Si coincide impulso con frenado ascendente, se permite inclinacion hacia
  delante hasta18grados, con rampa20grados/s y yaw solicitado cero. No se eleva
  RPM para compensar la componente vertical redirigida. El retorno conserva
  la rampa fisica de servos y el resto usa el limite anterior12grados.
- Techo20%, pines, base52.5/64.5, protocolo Nicla, llegada y pistas P10/P11 se
  conservan. Se mantiene la busqueda inicial v23 con su limite de tiempo.

F13,5Hz: inertia_vz, inertia_predicted_vz, inertia_target_vz,
inertia_motor_floor, inertia_flags, reservado. Bits:1velocidad aceptada,
2compensacion de sustentacion desactivada en fase neutral,4frenar subida,
8recuperar caida. Ground debe reiniciarse para cargar estos nombres.

Verificacion: pruebas de frenado anticipado en ambos sentidos, potencia siempre
positiva, bloqueo del aprendizaje durante una secuencia de rebotes, rechazo
de velocidad contradictoria, redireccion sin incremento de RPM y limites
mecanicos. Una planta vertical simplificada muestra menos sobrepaso; eso no
demuestra la respuesta fisica del blimp. Validacion de vuelo pendiente.
