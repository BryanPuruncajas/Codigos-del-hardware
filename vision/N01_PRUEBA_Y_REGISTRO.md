# N01 v1 Prueba de regiones rojas independientes
Fecha: 2026-09-29. Estado: implementado, pendiente de validacion en Nicla.
Archivo: nicla_n01_blobs.py. El programa perception_subsystem.py se conserva intacto.
Firmware ESP, app y protocolo IBUS: sin cambios.

## Alcance y limites
- Detecta componentes rojas separadas con find_blobs(merge=False), rectangulo,
  centro y dimensiones en pixeles. No mide distancia ni identifica semanticamente globos.
- C1/C2 son indices locales de cada imagen, NO globo 1/2 ni seguimiento.
- No selecciona objetivo, no confirma visitas, no conserva detecciones antiguas.
- Umbral LAB inicial (2,80,20,80,-5,70) NO calibrado. Cambia el detector
  gaussiano por segmentacion LAB solo en este ensayo aislado.
- Reflejos pueden fragmentar un globo; objetos rojos pueden producir falsos
  positivos; globos tocandose pueden formar una sola region. Son fallos a registrar.
- Filtros iniciales: 25 pixeles, caja 36 px2, lado 4 px, ocupacion 0.15,
  relacion ancho/alto 0.4 a 2.5. Hasta 8 candidatos mostrados, total en cabecera.
- Asterisco: region recortada por el borde. Medidas de la region de color,
  no necesariamente del globo entero. No reutilizar umbrales de acercamiento.
- No abre UART/IBUS. La app NO recibira estas detecciones; podria mostrar
  valores anteriores. Evaluar solamente imagen y terminal de OpenMV.

## Cargar y probar sin sustituir el programa de arranque
1. Desconectar bateria de propulsion y alimentar solo la Nicla por USB de datos.
   No armar la ESP ni iniciar misiones. No cambiar cableado con alimentacion.
2. Abrir OpenMV IDE y conectar. Si pide borrar/actualizar firmware, cancelar
   y revisar antes: esta prueba no necesita actualizar el runtime si ya funciona.
3. Detener el script en ejecucion. Copiar main.py y dependencias actuales
   desde la unidad de la Nicla a una carpeta de respaldo en el PC.
   La copia local perception_subsystem.py no acredita lo que esta cargado.
4. File > Open: abrir nicla_n01_blobs.py de esta carpeta. Pulsar Run (triangulo).
   Esta ejecucion temporal no sustituye el main.py almacenado.
5. Ver imagen y terminal: N01_CONFIG, N01_FRAME y N01_BLOB.
   timestamp=uptime_ms de Nicla, NO hora del PC. Grabar reloj del PC con segundos
   y terminal al iniciar; guardar salida de terminal junto al video.
6. Si no detecta rojo o detecta demasiado fondo: ajustar RED_LAB con
   Tools > Machine Vision > Threshold Editor, anotar valores y volver a ejecutar.
   Anotar cambios de luz/posicion al arrancar: el balance se fija tras adaptacion.
7. Solo si se desea arranque autonomo y ya existe respaldo:
   Tools > Save open script to OpenMV Cam (as main.py), luego Reset OpenMV Cam.
   Esto REEMPLAZA main.py. No hacerlo aun para esta primera validacion USB.
8. Para volver atras tras prueba temporal: Stop y reiniciar la Nicla.
   Si se guardo main.py: restaurar el main.py respaldado y reiniciar.
   Para retomar pruebas anteriores, comprobar primero que vuelve su telemetria.

## Ensayo N01 T01
Registrar 10 segundos por condicion con buena luz y motores sin alimentacion:
A. Sin objetos rojos: esperar 0 candidatos; anotar falsos positivos.
B. Un globo: una caja razonable con centro sobre el globo.
C. Dos globos separados: dos cajas, sin union artificial.
D. Acercar/alejar uno: w/h cambian con su extension visible.
E. Ocultar uno: su caja desaparece; no pedir bloqueo ni recuperacion aun.
Guardar video de la imagen de NICLA (no solo camara externa), terminal y umbral LAB.
Aceptar provisionalmente solo si A-E se cumplen; si no, registrar fallo,
condicion y fotograma. No pasar al control de vuelo por aprobar solo sintaxis.

## Registro
| Cambio | Estado | Evidencia |
| --- | --- | --- |
| N01 regiones separadas y medidas | Implementado, hardware pendiente | Script y pruebas locales con dobles de imagen |
| N02 seleccionar objetivo inicial | Propuesto | Sin implementar |
| N03 asociar objetivo entre imagenes | Propuesto | Sin implementar |
| N04 perdida y recuperacion | Propuesto | Sin implementar |
| N01 T01 A-E | Pendiente | Falta captura de Nicla, terminal y video |

## Referencias
OpenMV find_blobs: https://docs.openmv.io/library/omv.image.html
Ejecucion y main.py: https://docs.openmv.io/v5.0.0/es/openmvcam/tutorial/tools/ide/running-scripts.html

## Actualizacion N01 v2 - 2026-09-29
Esta seccion sustituye los parametros de forma de v1 descritos arriba.
Evidencia v1: captura del usuario muestra cajas 34x35 y 45x50 sobre globos
y candidato extra 19x11 a la derecha. Deteccion inicial favorable; existe
falso candidato. Una captura no valida seguimiento, estabilidad ni identidad.

Cambios v2:
- Aspecto ancho/alto: 0.65 a 1.45 (antes 0.4 a 2.5).
- Redondez minima: 0.45; circularidad minima: 0.35.
- Circularidad = 4*pi*pixels/perimeter^2, no usa w*h como area segmentada.
- Umbrales PROVISIONALES, no calibrados. No se aumento el tamano minimo.
- Se mantienen cajas rechazadas en amarillo; aceptadas en verde. Son
  candidatos por color/forma, NO reconocimiento garantizado de globos.
- Leyenda: AR proporcion, R redondez, C circularidad, FILL ocupacion,
  SIZE lado pequeno, EDGE recortado, METRIC metrica no disponible/invalida.
  OK pasa filtros; motivos multiples se unen con +. R/C=-1 significa no
  disponible, no una medida real. METRIC rechaza: no desactivar a ciegas.
- Cabecera N01v2 A=aceptados, X=rechazados, S=mostrados (maximo 8).
  Los umbrales iniciales find_blobs de pixeles/area siguen suprimiendo ruido
  pequeno antes de esta clasificacion; no se dibuja todo pixel rojo.
- Terminal usa N01V2_FRAME y N01V2_BLOB; anade aspecto, ocupacion, redondez,
  circularidad, accepted y reason. No es el formato anterior N01_BLOB.
- Los reflejos/oclusiones pueden hacer rechazar un globo real. Un objeto rojo
  redondo aun puede pasar. Un borde recortado se rechaza conservadoramente.
- Respaldo del archivo realmente existente: backups/nicla_n01_blobs_v1.py.
  Para volver a v1, abrir ese respaldo en OpenMV y Run sin sobreescribir v2.
- Firmware ESP y perception_subsystem.py intactos. Sigue sin UART/IBUS.

Prueba N01 T02 pendiente: Stop, reabrir nicla_n01_blobs.py DESDE DISCO y Run.
Verificar cabecera N01v2. No guardar main.py todavia. Propulsion desconectada.
Repetir escena: ambos globos deberian pasar; el objeto 19x11 deberia fallar AR
si conserva esas dimensiones. Enviar captura y terminal con R/C/motivos.
Mover globos hacia bordes y cambiar distancia/reflejos para medir falsos
rechazos. No asumir que la circularidad de la captura se conoce: no se ha
calculado desde la imagen original de la Nicla.
Estado v2: implementado, pendiente de validacion fisica. N02-N04 pendientes.

## Correccion N01 v2.1 - compatibilidad OpenMV 5.0.0
Evidencia del usuario: MicroPython 1.28.0-49, Arduino Nicla Vision.
N01 v2 FALLA en hardware: TypeError al dibujar texto (linea 98) y al
llamar blob.rect() (linea 47). No se validaron filtros de forma.
Correccion: coordenadas de draw_string/draw_cross como tuplas; rect,
pixels, cx, cy, width, height mediante acceso compatible con propiedades
y metodos. Eliminada llamada set_auto_gain no soportada por GC2145.
La advertencia de ganancia no era el error fatal; no se afirma ganancia fija.
Umbrales de color y forma, geometria y protocolo de terminal sin cambios.
Version y cabecera N01-v2.1 / N01v2.1 permiten confirmar recarga del archivo.
Respaldo anterior: backups/nicla_n01_blobs_v2.py.
Los dobles de pruebas anteriores eran permisivos en las llamadas de dibujo
y no probaban todos los atributos Blob. Se refuerzan para reproducir esos
dos fallos. Las pruebas locales no sustituyen repetir Run en la Nicla.
Estado v2.1: correccion implementada, pendiente de reensayo fisico.
Firmware ESP y perception_subsystem.py no se editan. Sigue solo USB.
Entrada pendiente de incorporar al Word; registro tecnico actualizado aqui.

## N01 v3 - 2026-10-03
Evidencia: diez capturas del usuario 16:49:55 a 16:52:34 y configuracion
v2.1 confirmada: LAB (2,80,20,80,-5,70), Rmin=.45, Cmin=.35.
v2.1 funciona en placa en estas capturas. Se ven falsos rechazos por C
(.32/.34 e incluso .13), recortes EDGE y cajas que abarcan ambos globos
al solaparse. Reflejos son hipotesis, no causa unica demostrada.
v3 implementado: comparacion diagnostica pendiente de hardware.
Respaldo: backups/nicla_n01_blobs_v21.py.

### Que cambia
Alterna 2 s COLOR y 2 s MASK; son imagenes consecutivas, no el mismo fotograma.
Mantener camara y globos quietos para comparar. MASK no lleva anotaciones:
blanco=pixel dentro de LAB, negro=fuera. Se mide siempre RGB ANTES de
dibujar/binarizar; no se modifica la segmentacion ni se agregan filtros.
Umbrales conservados. Circularidad deja de vetar SOLO en este ensayo USB.
Terminal registra tambien decision estricta v2.1 para comparar el MISMO frame.
No hay UART, seleccion de objetivo, identificacion persistente ni control ESP.

Estados: F verde=completo plausible; P cian=region recortada con soporte
minimo; D magenta=geometria dudosa o metrica no disponible; X amarillo=SIZE/FILL.
P no significa globo reconocido ni proximidad confirmada. Una region
con globos unidos puede seguir pasando como F; v3 NO separa solapamientos.
Etiqueta 1F! = candidato 1 completo, pero circularidad habria fallado en v2.1.
Indices por imagen. S indica cuantos se muestran (max8). Los filtros minimos
find_blobs siguen excluyendo ruido antes de estos estados.
Medidas/motivos en N01V3_BLOB; N01V3_FRAME incluye F/P/D/X y v21_accepted.
No mezclar formatos v2/v3. uptime_ms no es hora PC.

### Ensayo T03 cuantificable
Propulsion desconectada. Stop, reabrir archivo desde disco y Run; VERSION N01-v3.
No guardar main.py aun. Grabar pantalla y terminal. No mover ni cambiar luz
durante cada pareja COLOR/MASK. Cada escena dura 40 s: 10 parejas de 4 s.
Tomar una captura COLOR y una MASK por pareja, evitando cambios de vista.
A: sin globos (y luego objeto rojo de fondo). Esperado F=0.
B: un globo completo lejos, por ejemplo 30-50 px de ancho. Esperado F=1.
C: un globo completo mas cerca, por ejemplo 70-100 px. Esperado F=1.
D: dos globos completos a distinta distancia, con separacion visible. F=2.
E: uno parcialmente fuera del borde. Esperado P>=1 sobre ese globo, no F.
F: dos globos solapados. Solo diagnosticar si se unen: NO exigir exito aun.
Rangos en pixeles son orientativos, NO distancias fisicas calibradas.

Contar por escena sobre 10 muestras elegidas:
- Aciertos: cajas F sobre cada globo completo (1/escena B-C, 2/escena D).
- Falsos positivos: cajas F que no son globos.
- Omisiones/rechazos: globo completo sin F.
- Fusiones: una caja que abarca dos globos.
Ejemplo B: 8 de10 con F correcto =80% de muestras, no precision general.
Ejemplo D: hasta20 oportunidades (2 globos x10); anotar ademas cuantas
de las10 muestras tienen AMBOS detectados. Objetivo provisional: >=9/10
muestras correctas y 0 falsos positivos observados en escena separada.
No acredita vuelo seguro ni generalizacion. Guardar tambien los fallos.
Mascara: buscar huecos negros en reflejos, fragmentos blancos y puentes
blancos entre globos. No bajar umbrales antes de ver esta evidencia.
Word: entradas v2.1 y v3 pendientes de sincronizacion; registro tecnico aqui.

## 2026-10-08 - N01-v3.1: minimo de candidato 20 x 20

Cambio experimental autorizado: w>=20 Y h>=20 para estados F/P/D;
si algun lado es menor, se muestra X. No elimina la region de la mascara.
LAB, extraccion de blobs, forma, exposicion y comunicacion no cambian.
MIN_SIDE=4 y reason_v21/v21_accepted conservan la comparacion historica;
por eso X puede mostrar reason_v21=OK. N01_SIZE anuncia el filtro nuevo.
Se conserva el esquema CSV N01V3. Sin seguimiento temporal, UART ni ESP.

Evidencia previa: globo a 4 m, primeras 84 muestras aceptadas, w32-35/h38-46.
Fondo independiente: 32.214 s,135 muestras,67 con F,69 candidatos F.
Filtro propuesto excluye esos69 y conserva esos84 retrospectivamente.
No implica precision general ni garantia para otras distancias/luces.
Pendiente hardware: repetir 30 s con globo a4m y30 s fondo sin personas;
guardar logs independientes. Comprobar F sobre globo y ausencia de F fondo.
Backup: backups/nicla_n01_blobs_v3_before_size20.py.
Documento Word pendiente de sincronizacion; esta entrada es registro tecnico.
