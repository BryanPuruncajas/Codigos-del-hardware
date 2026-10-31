# Primera llegada con avance calibrado, versiones13/14

Objetivo inmediato: P08/P09 de un waypoint en BalloonMission debe aproximarse
con propulsion y emitir F5 visited=1,state=DONE al cumplir llegada estimada.
P03 --forward es un ensayo de inclinacion temporizado; no cuenta llegadas y
su corte de region120px impedia usarlo como aproximacion completa a220px.

P08/P09 N03 ahora usan la base de servos48.5/68.5, mixer de SupportYaw con
avance nominal0.03 e inclinacion adicional hasta12grados, autoridad yaw25%,
limite motor16% y piso normal10%. Altura visual por PI de velocidad, yaw por
IMU y direccion visual suavizada. No se cambia ningun pin, cableado o app.
El efecto fisico de este avance aun requiere vuelo libre de contactos.

PARTIAL inicial permite regular/reencuadrar, pero se exige haber adquirido
FULL antes de habilitar avance. Centrado inicial x120+/-24,y80+/-24 y yaw
dentro8grados; durante avance tolera36px y12grados. Requiere5 muestras/300ms
y giro lento. Avance no se corta por superar120px: sigue hasta cercania,
incluyendo PARTIAL cercano del mismo tracker tras un FULL previo.

Llegada: ancho>=220px, centro dentro24px horizontal/vertical, al menos3
muestras nuevas y500ms continuos. Un pico aislado no cuenta. Este umbral
es la calibracion provisional previa, NO detector de contacto fisico. Retira
avance en VISIT_CONFIRM; en DONE publica visited=1, potencia0 y desarma.
Retirar avance no equivale a frenar la velocidad horizontal real: no hay
medicion de desplazamiento ni freno horizontal validado, ni se anade retroceso.

Perdida: avance0, sustentacion aprendida, busqueda alrededor del ultimo rumbo
y puente barometrico relativo cuando exista referencia centrada saludable,
segun version12. Tras4s sin recuperar termina en TURN_FAILED. Tiempo maximo
de aproximacion60s a bordo, ademas del plazo de Ground. Sin altura coherente
pero con vision valida puede seguir centrando. La referencia del operador
no suplanta al waypoint visible. STOP y sensores de yaw invalidos desarman.

Version13 se identifica por F11 en el firmware. F8 informa forward_request
(demanda, no velocidad medida); F3 informa los angulos y potencias ordenados.
F5 informa estado y contador. La app actual ya consume estos campos.

Alcance deliberado: integrar primero una llegada completa en P08/P09. P10/P11
y P12 conservan su flujo anterior; no se afirma que todos los tramos y giros
de ambas pistas tengan ya esta nueva asignacion. Ese paso sigue pendiente
despues de validar fisicamente el primer avance/llegada. Cargar nueva ESP;
Nicla y app no necesitan cambios para esta prueba.

## Caida durante el avance, correccion version14

Ensayo single_waypoint/20261026_233118_679491.jsonl: usuario confirma avance
continuo sin contactos, seguido de caida sin recuperacion. No hubo llegada.
Al pasar a PARTIAL la ley visual bloqueaba cualquier aumento sobre trim,
ya reducido a0.128867. Y bajo de~87 a21px mientras la demanda quedaba en
0.128867; el puente barometrico solo actuaba despues de perder la deteccion.
Por tanto el avance fisico queda observado; la regulacion conjunta fallo.

En P08/P09 version14 PARTIAL permite P direccional acotado por techo16%,
sin integrar ni derivar el centro truncado. La regla antigua de no aumentar
sobre trim se conserva por defecto para los otros consumidores VisualHeight.
Si imagen indica objetivo arriba (y<70) y un barometro saludable confirma
descenso vz<-.04m/s, demanda al menos trim+min(.03,-.15*vz), siempre<=.16.
Una lectura barometrica sola no activa ese refuerzo.

Avance nominal0.03 se reduce gradualmente con error de altura positivo: sin
reduccion hasta4px arriba del centro, llega a0 a20px arriba. Tras recuperar
centrado reanuda aproximacion. Prioriza recuperar altura frente a consumir
empuje en avance; no se elimina el control de yaw. En perdida completa el
puente barometrico relativo pasa a P=.08,D=.05,correccion maxima+/-4 puntos,
manteniendo piso10%/techo16% y plazo4s. No garantiza poder subir si al16%
el empuje fisico disponible es insuficiente; eso requiere validar en vuelo.

Prueba del fallo reproducido en test_single_waypoint.cpp: PARTIAL y60, vz-.12
pide16%, suspende avance, sigue armado; al volver a y80 reanuda avance y
conserva validacion de llegada. Falta cargar version14 y verificar fisicamente.

Pruebas: test_single_waypoint.cpp ejecuta el BalloonMission real y comprueba
inclinacion de avance, continuacion a150px, PARTIAL cercano, perdida1s sin
desarme, rechazo de cercania fugaz y llegada estable a225px con DONE/desarme
una sola vez. test_mission.cpp conserva las regresiones de las rutas/P12.

## Yaw mas reactivo, version15

Ensayo single_waypoint/20261027_000546_651984.jsonl: usuario confirma mejor
recuperacion de altura; aun no hay llegada. Se observaron errores de rumbo
20-25grados durante varios segundos; tambien oscilacion, por lo que no se
eliminan filtros ni se amplian topes mecanicos o potencia.

Solo P08/P09 N03: ganancia externa de posicion yaw0.40 a0.50 (+25%).
Amortiguacion de velocidad Kd2, integral Ki.25, velocidad deseada maxima.20
rad/s, autoridad25%, rampa30grados/s, filtro de referencia y piso/techo motor
siguen iguales. La altura y el avance de version14 no cambian. El aumento
de25% describe la respuesta de mando no saturada al error, no una rapidez
fisica garantizada. Pendiente validacion de sobrepaso/oscilacion en vuelo.

test_single_waypoint comprueba respuesta de posicion mayor y mismo frenado
en el centro, ademas de los casos de altura PARTIAL, avance y llegada.
El lanzador exige F11version15. No cambian Nicla, app, pines ni P10/P11.

## Duracion ampliada, version16

El usuario indica que45s no permiten completar la aproximacion. El lanzador
ahora permite120s despues de la cuenta de voz; --seconds acepta5..120, defecto
120. Se registra duration_s. El limite independiente de aproximacion en ESP
pasa de60s a180s para incluir margen de preparacion sin cortar antes al host.
No se reinicia ese reloj por perdidas ni por pasar a VISIT_CONFIRM. Sigue
parando al completar, STOP, bateria baja, fallo o perdida4s sin recuperar.
Ganancias, altura, avance, limites de potencia, pines y app no cambian.
Lanzador exige version16 para no quedar limitado por firmware previo.
Prueba: observaciones validas durante62s siguen APPROACH; despues de180s
el controlador desarma por su propio limite. Falta carga de ESP y vuelo.

## Frenado de subida y avance durante giros, version17

Log single_waypoint/20261027_090832_577025.jsonl: final por fallo a bordo,
objetivo ausente durante4s, STOP confirmado; no llego al limite120s ni hubo
waypoint completado. Usuario informa sobrepaso de altura y trayectoria
lateral; confirma tambien contacto con un objeto. Movimiento posterior al
contacto no permite identificar por si solo un fallo del controlador o una
orbita. No hay medicion de desplazamiento horizontal ni contacto/techo.

Antes de cruzar el centro, PARTIAL podia seguir pidiendo mas sustentacion
sin derivada de imagen fiable para frenarla. En P08/P09, con barometro sano,
referencia de velocidad vertical deseada limitada a+/-0.06m/s desde error
visual. Si ya sube mas rapido que esa referencia+0.02, reduce demanda bajo
trim hasta3 puntos: FULL requiere derivada visual de ascenso; PARTIAL exige
vz>0.08m/s por no tener derivada fiable. No modifica trim por ese frenado.
Con datos invalidos omite la nueva limitacion. Conserva recuperacion ante
descenso y PARTIAL, piso10%, techo16% y ganancias de yaw version15.

Avance: entrada x+/-18px y error yaw5grados; mantiene hasta24px/8grados,
velocidad de giro<=0.12rad/s. Demanda se reduce gradualmente por error de
imagen, error de yaw y giro medido, ademas de la prioridad vertical previa.
No pide todo el avance hasta el instante de salir del margen, como antes.
No es frenado horizontal activo ni garantia de trayectoria recta. Inercia
lateral residual puede persistir sin sensor de desplazamiento.

Pruebas reales de codigo: menor avance al desalinearse/girar, avance pleno
al centrar; PARTIAL con descenso sigue pidiendo recuperacion, PARTIAL con
ascenso excesivo pide menos que trim antes del centro; conserva llegada,
recuperacion, limite180s y parada. Lanzador exige version17. No cambian PCB,
pines, app ni Nicla. Pendiente cargar y probar sin contactos.

## Avance por impulso e inercia, version18

El usuario confirma tres repeticiones con el firmware anterior y deriva
diagonal/lateral. Solicita dejar de mezclar avance continuo con giro: orientar,
impulsar, retirar avance y corregir yaw/altura durante la inercia. La deriva
no se considera explicada solo por el contacto reportado previamente.

Nueva secuencia SOLO P08/P09 N03:
0 ALINEAR: yaw/altura activos, avance0. Exige x/y dentro14px, error yaw4grados,
giro<=.06rad/s,5 muestras nuevas y300ms; requiere haber visto FULL.
1 ACOMODAR: avance0, mando yaw0. Espera>=500ms y comandos de servos a menos
de0.5grados de la base48.5/68.5. No son encoders; se comprueba la orden.
2 IMPULSO: avance nominal.03 con mismo limite12grados adicional y rampa,
hasta1200ms, mando yaw0 e integral de yaw congelada. No mezcla correccion
de rumbo con este impulso. Altura sigue regulando potencia.
3 RETIRAR_AVANCE: avance0, yaw0, retorna a base; espera>=500ms y pose de
comando neutra antes de permitir giro.
4 CORREGIR_EN_INERCIA: avance0 durante>=2000ms, yaw/altura activos; luego
vuelve a ALINEAR y exige confirmacion nueva antes de otro impulso.

Durante impulso/acondicionamiento, perder vision, superar24px horizontal o
vertical,8grados de yaw o.12rad/s cancela el impulso y retira avance. Llegada
tambien retira avance; DONE/STOP/fallo mandan0 y desarman. La recuperacion
de altura y su frenado de version17 siguen activos en todas las fases.
Conserva potencia10..16% normal, prueba120s, limite ESP180s, perdida4s y
confirmacion de llegada provisional220px/500ms. No hay retroceso automatico.

F12 adicional: impulse_phase,elapsed_ms,forward,yaw,count,max_ms. Permite
verificar que no hay mando de yaw simultaneo con impulso. F10 ahora reporta
yaw aplicado real, no0 fijo. Reiniciar Ground para cargar etiquetas F12;
la consola muestra nombres de fase. La app conserva F1..F11, no necesita
actualizacion para ejecutar la prueba. Se exige firmware18 en el lanzador.

La fase llamada inercia solo significa que no se ordena nuevo empuje de
avance; no mide velocidad ni posicion horizontal. La sustentacion/yaw aun
pueden generar traslacion por montaje o aerodinamica. Si el impulso puro
sigue siendo diagonal, debe comprobarse fisicamente la direccion del empuje
de cada gondola; el codigo no certifica que el eje de empuje coincida con
el frente. Pendiente carga y prueba fisica de esta secuencia.

Prueba del controlador real: acomoda antes de impulsar, pulso acotado,
yaw aplicado0 mientras forward>0, integral congelada, aborto de impulso
por desalineacion, retorno a base antes de corregir, al menos2s sin avance,
recuperacion de altura PARTIAL, perdida1s, llegada una sola vez y fallo al
agotar4s de perdida/180s de aproximacion. No valida movimiento fisico.

## Compensacion persistente del retroceso, version20

Usuario confirma retroceso durante sustentacion aun con version19. Log
single_waypoint/20261027_130542_835381.jsonl:396 muestras F12 faseALINEAR,
servo medio48.787/68.787, potencia media15.2%, y solo4 muestras de impulso.
La base48.5/68.5 seguia aplicada durante sustentacion/coast; secuenciar
impulsos no compensa una deriva que ocurre casi todo el tiempo restante.

Version20 cambia la orden base a52.5/64.5: +4grados en el sentido de avance
para S1 y-4 para S2. Esta compensacion permanece durante alineacion, altura,
retorno del impulso, correccion en inercia y recuperacion visual. Preparacion
del lanzador usa la misma pose. Las excursiones de yaw se conservan respecto
de su referencia anterior para no incrementar autoridad al desplazar la base.
La normalizacion de sustentacion conserva la referencia48.5/68.5, de modo
que el modelo compensa perdida vertical de la inclinacion adicional; para
demanda15% el motor pide algo mas de15%, siempre dentro del techo16%.

Esto es una correccion de direccion de mando basada en el retroceso observado
y el signo de avance previamente comprobado. El valor4grados es provisional:
pruebas de software no demuestran que neutralice el retroceso fisico. No se
debe etiquetar como nueva calibracion validada sin observacion de vuelo.
No se afirmara llegada o eliminacion de deriva antes de comprobarlo.
No cambia pines, Nicla, app, PID de altura, yaw, impulso1.2s, recuperacion15s
ni prueba120s. Version20 debe cargarse en ESP; lanzador exige ese numero.

## Balanceo y readquisicion extendida, version19

P08/P09 N03 pasan de4s a15s sin objetivo confirmado. Mientras falta vision,
sin impulso, sustentacion aprendida, referencia relativa de altura si existe
y barometro valido, busqueda de yaw alrededor del ultimo rumbo. Reencuadrar
reanuda regulacion y exige alineacion antes del siguiente impulso. No reinicia
mision, contador, ancla de altura ni tiempo total de prueba.

La implementacion local de Nicla permite recuperacion geometrica5s, despues
LOST puede persistir aunque haya imagen. La ESP, tras5.2s sin deteccion,
solicita R por UART si sigue LOST y hay comunicacion valida; espera ACK y
SEARCH. Peticiones separadas>=3s y ninguna mientras resetPending. No reinicia
el reloj15s por enviar R. Tras reinicio conserva rumbo previo y rechaza un
candidato confirmado a mas20grados de ese rumbo; no es identificacion unica
del waypoint. Se admite FULL/PARTIAL confirmado dentro de ese sector.

Perdida visual NO equivale a fallo del enlace: si feedback.fresh no vuelve
durante1.5s, falla y desarma. Comunicacion valida con estado MISSING/LOST
puede usar los15s de recuperacion. Se mantienen120s de prueba,180s a bordo,
limites de potencia, llegada, STOP y secuencia por impulso. Otros ensayos
conservan sus limites previos; este cambio no alarga indiscriminadamente P03.

Pruebas: sin deteccion4.5s sigue armado sin avance; LOST tras5.3s genera un
solo R y espera ACK; candidato fuera del sector se rechaza; readquisicion
correcta conserva ancla/contador; ausencia14.9s sigue recuperando y15s falla;
enlace de camara sin datos1.6s falla. Sigue pasando secuencia de impulso,
regulacion de altura, llegada y limite global. Falta carga y prueba fisica.
