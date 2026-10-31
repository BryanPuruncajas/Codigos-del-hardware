# Correccion conjunta de busqueda y adquisicion

ESPv25 y fuente Nicla N03-panel-v5-wide, proyecto Competencia.

Ultima prueba: logs/single_waypoint/20261027_233922_672570.jsonl. SEARCH durante
20s, Nicla estado0 en todos los paquetes muestreados; nunca entro en APPROACH.
F13 era un marcador fijo en SEARCH: los cambios de anticipacion v24 solo
actuaban despues de adquirir un objetivo. El usuario confirma techo/rebote.
Esto fue un fallo de integracion, no una prueba fisica del regulador v24.

Cambios ESP:

- VisualSearch usa el mismo InertialHeight que aproximacion. Su regulador de
  velocidad/prediccion y reduccion hasta6% funcionan aun sin objetivo visible.
- La subida ciega solicitada se limita a3cm/s y se neutraliza al alcanzar un
  incremento barometrico relativo provisional20cm. No es distancia al techo.
- Durante CONFIRM se pausa la subida y se mantiene el rumbo actual para no
  barrer sobre el candidato mientras Nicla termina su confirmacion temporal.
- Se conserva aprendizaje acotado en busqueda, techo configurado max20%,
  timeout20s, fallo UART1.5s y sin avance hasta confirmar un objetivo.
- F13 ahora transmite estimacion/frenado reales tambien en SEARCH.

Cambios en el archivo que se copia a main.py de Nicla:

- Adquisicion inicial ampliada de x90..150 a x20..220, en imagen240x160.
  No se toca LAB, redondez, relleno, resolucion ni confirmacion300ms/5frames.
- Un unico candidato plausible fuera del centro puede confirmarse; dos
  candidatos plausibles visibles siguen siendo AMBIG, sin elegir arbitrariamente.
- Cuando no hay objetivo confirmado se transmite una region candidata por
  los canales geometricos existentes, manteniendo flag0. Es diagnostico: no
  habilita avance ni llegada. El formato32bytes, checksum, estados, protocolo,
  muestra y ACK se conservan. Sin regiones la geometria permanece cero.
- Se prueban directamente las funciones del archivo standalone de produccion.
  Las pruebas antiguas del panel importan n02_core.py, un archivo distinto.

La franja90..150 se verifico en la fuente local; no se ha leido todavia el
main.py instalado en la Nicla de esta sesion. Por tanto no demuestra que fuese
la unica causa de SEARCH. La nueva geometria candidata permite distinguir
segmentation/rechazo/adquisicion sin interpretar flag0 como globo confirmado.

Antes de otro vuelo debe cargarse/verificarse el main.py nuevo en Nicla y el
firmware25 en ESP. No basta con cargar solamente ESP. Pines y cableado sin
cambios. Validacion fisica pendiente; no existe deteccion de techo/contacto.
