# Sosten + rumbo aislados — 2026-10-26

Estado: experimental. Version con piso14% ya cargada y ensayada, sin sostener
el objetivo en el encuadre por deriva de rumbo. Correccion XY/version2 compilada
SUCCESS (824277 bytes de programa,48136 bytes RAM), pruebas de software aprobadas;
pendiente de carga/validacion fisica. No cambia potencia ni pines.
Sin autoarmado ni avance.

## Evidencia y alcance

Ensayo P00M vertical 6s, 14%, S1=35/S2=95: el usuario confirmo caida mas suave
pero persistente, y giro fisico a derecha. IMU ~50deg/6s. Bateria reportada
3.83V al inicio, 3.77V al final. El barometro indico descenso pero no constituye
verdad fisica de altura. Por ello no sintonizar altura y yaw simultaneamente.

P03 incorpora una opcion explicita, AUX4=1 / --yaw-support-hold. AUX4=0 conserva
el P03 anterior. No se crea otro modo, no cambian pines, geometria 35/95,
limites mecanicos 0..120, app, Nicla, P04/P05 o Balloon Mission. El controlador
nuevo NO se activa aun en la mision completa: primero validar este ensayo aislado.

## Control experimental

- Empuje base constante configurado; no lee altura ni deriva la medida barometrica.
- Yaw con IMU, signo de gyro existente -1. Objetivo angular capturado de la posicion
  actual por el operador/script. Zona muerta angular suave; amortiguamiento por
  velocidad e integral de compensacion permanecen activos dentro de esa zona.
- Servo proporcional al mando, autoridad maxima 25% del recorrido desde vertical,
  limitada a 30deg/s. No inclinacion fija de 75% por una correccion pequena.
- Motor limitado por el techo solicitado, rampa .18/s. Compensacion vertical
  calculada con los angulos limitados de servo, no los objetivos futuros.
- Ante yaw/gyro invalidos: motor cero y desarmado, sin rearmado automatico.
- El integral no aprende mientras esta desarmado; un reinicio elimina el trim.

Parametros de ensayo propuestos, NO calibrados: base=16%, techo=16%, Kp=.4,
Ki=.25, Kd=2, deadband=1deg, autoridad=25%. Deseo de velocidad limitado a
.20rad/s. Estos no reemplazan las ganancias del P03 historico ni de la mision.

IMPORTANTE: con base igual al techo no hay margen para compensar toda la perdida
vertical al inclinarse. A autoridad 25%, etaZ del modelo permanece >=.95: al
16% de motor, la componente vertical puede bajar aproximadamente a 15.2%.
No se promete conservar 16% vertical ni sustentacion neutra. Tampoco hay retencion
de posicion horizontal: gondolas asimetricas pueden producir deriva lateral.
La relacion mando/empuje y los limites dinamicos necesitan validacion fisica.

## Ejecucion

Usar Ground y la secuencia de voz habitual: preseleccion vertical con potencia
cero/desarmado, armado a cero confirmado, captura reciente de rumbo y luego ensayo
temporizado de 6..10s, STOP triple y confirmar armed=0/mode=0. No iniciar desde la
app todavia: no hay boton propio y el P03 normal no selecciona esta opcion.

El script supervisor debe empaquetar:

```
make_args('p03', yaw_deg=rumbo_actual_en_grados,
          yaw_support_hold=True, yaw_base_power=16,
          yaw_min_power=0, yaw_max_power=16, yaw_servo_authority=25,
          yaw_kp=.4, yaw_ki=.25, yaw_kd=2, yaw_deadband_deg=1)
```

No instalar por USB mientras la ESP esta en PCB alimentada. Para probar montado,
solo Ground por USB. No subir automaticamente de 16% si cae: revisar observacion
fisica, bateria y giro antes de decidir otro escalon.

Despues de validar sostén y rumbo, integrar este mezclador en Balloon Mission
y corregir la conmutacion avance/centrado (yaw continuo, bandas distintas y espera
de estabilidad). Sigue pendiente; no es una mision de vuelo ya validada.

## Ampliacion: altura visual + rumbo, sin avance

El ensayo fijo16% de 8s levanto el blimp hasta el techo (usuario confirmo poco
margen inicial). Desvio IMU maximo24.1deg, final19.4deg: no logro mantener rumbo.
Los contactos invalidan una sintonizacion de vuelo libre. Se conserva como prueba
de capacidad, NO como potencia neutra ni control final de altura.

AUX4=2 / --yaw-visual-hold selecciona una nueva variante P03: regulacion Y con
VisualHeight existente y rumbo con SupportYaw. Sin avance ni barometro. No cambia
Balloon Mission, Nicla, app, pines o geometria. Base propuesta15%, techo duro16%.
15% es provisional, NO se asume neutral por ser promedio de14 y16.

FX=1 prepara mientras se sujeta: rampa al empuje base, servos verticales, sin
aprendizaje del integral visual/yaw. Solo iniciar si hay objetivo confirmado
N03 FULL/PARTIAL fresco y geometria valida. Preparacion maxima20s en la ESP.
FX=0 regula tras la cuenta: misma configuracion/mismo modo, RESET=0 para NO
borrar la rampa. Capturar rumbo actual al soltar; integral de yaw se limpia en
la transicion. Potencia ya preparada, referencia Y=80, solo muestras nuevas.

Descenso normal: minimo motor configurable --yaw-support-min-power (14% inicial
provisional: en el ensayo fisico14% descendia suavemente). Viaja en el minimo del
YAW_PACK existente; no cambia estructura de comandos, UART, pines ni app. Base
propuesta15%, techo16%. Se aplica al objetivo del mezclador, no al arranque desde
cero: conserva rampa .18/s. F10 demanda vertical puede ser menor; F3 muestra la
potencia motor realmente ordenada. No garantiza descenso suave con otro peso,
bateria o helio, ni menor potencia al remontar: hay que frenar la caida.
STOP, perdida de observacion, IMU invalida y plazos agotados siguen dando cero
y desarmando inmediatamente, sin respetar el minimo de descenso.
El anti-windup visual considera el piso motor convertido a componente vertical
con etaZ del ultimo mando: no sigue reduciendo el integral si el actuador ya
no puede bajar mas. Es compensacion del modelo, no medicion real de empuje.

No aumenta potencia por demanda de yaw cuando la vision pide descender: el
mezclador usa el empuje vertical disponible. Por ello el giro puede debilitarse
durante un descenso; no existe control independiente de todos los ejes ni
retencion horizontal garantizada. El controlador tampoco mide distancia al techo.

Perdida de observacion valida/nueva durante700ms: demanda cero y desarmado,
sin busqueda ciega ni rearmado al reaparecer. Primera observacion invalida tambien
impide arrancar. Run maximo15s a bordo, ademas del limite host de8s propuesto.
Datos IMU invalidos tambien desarman. Preparacion no puede reiniciarse durante
Run para extender ese plazo. Una nueva tentativa requiere reset explicito.

Telemetria F10 a5Hz: support_visual_y, support_visual_error, support_trim,
support_vertical_demand, support_yaw_cmd, support_phase. Fases:0=preparar,
1=regular,2=espera breve observacion,3=fallo/desarmado,4=desarmado.
Reiniciar Ground tras actualizar common/telemetry.py para decodificar F10;
no requiere recompilar app ni Ground ESP. F1..F9 conservan sus campos.

Supervision de ensayo: motor nunca >16%; no soltar antes de recibir F10 fase0
y F3 potencia base estable/desarmado ya verificado antes de armar. La voz debe
avisar que los motores empiezan MIENTRAS SE SUJETA, y solo despues dar la cuenta.
Soltar cambia FX a0 y captura rumbo sin reset. STOP triple y verificar F3 al final.

El waypoint debe estar a una altura compatible con el blimp COMPLETO, con margen
libre al techo y objetos. No pedir altura visual hacia un objetivo que obligue
a chocar para centrar la camara. No es evitacion de obstaculos ni validacion de
vuelo libre. No iniciar motores automaticamente al cargar el nuevo firmware.

Pruebas de la ampliacion: core VisualSupport, P03 visual real con hardware
simulado, P03 soporte fijo real y regresion BalloonMission aprobadas;13 tests
Python de empaquetado/Ground/P12 aprobados. Compilacion completa SUCCESS.
Se ensayo esta ampliacion antes de la correccion XY; los datos siguientes
no constituyen validacion de la nueva referencia horizontal.

## Correccion de referencia horizontal y lanzador — 2026-10-26

Ultimo ensayo: al soltar ya x=49, y=67. En unos2s, x paso49->27->16->7 y
desaparecio, yaw se desvio unos14deg. Potencia15.0..15.14%, no fue un corte de
descenso ni por bateria. El ensayo estaba reteniendo rumbo inicial y no el
centro horizontal del objetivo. STOP/desarmado confirmados por F3.

P03 visual ahora actualiza referencia de yaw desde cada muestra X confirmada
y fresca: yaw - (x-120)/240 * 70deg, signo heredado de BalloonMission. Primera
muestra apunta al objetivo; despues filtra tau.35s y limita referencia a
.35rad/s. No resetea integral al centrarse ni alterna con avance. IMU sigue
amortiguando velocidad. Conserva ultimo rumbo visual durante HOLD breve y los
mismos fallos/tiempos a bordo. Modo constante P03 y BalloonMission no cambian.

F11 a5Hz: support_visual_x, support_yaw_reference, support_yaw_error (radianes),
support_visual_bearing (activo), support_version=2, support_forward_request=0.
Reiniciar Ground para decodificarlo. P03 desarmado emite version antes del ensayo,
sin mover motores. El nuevo lanzador exige version2 y F10 antes de cualquier ARM.

`groundstation/check_visual_support.py` usa el WebSocket de Ground, no abre COM9.
Sin opciones solo lee telemetria; --dry-run imprime paquetes sin red/hardware.
--actuate autoriza ensayo con voz (8s por defecto,2..12 configurables). Tiene
registro JSONL en logs/visual_support; no guarda token de autenticacion.
Compatibilidad comprobada desarmado, armado a cero, empuje preparado15%, cuenta,
regulacion sin reset de potencia, STOP y confirmacion fresca. Fallo durante voz
interrumpe el proceso de voz propio para no continuar una cuenta de suelta.
No toma control de un ensayo que ya esta armado. Ground debe estar activo.

Comando PowerShell (solo DESPUES de cargar la nueva ESP):

```
& 'C:\Users\stefa\.platformio\penv\Scripts\python.exe' 'C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation\check_visual_support.py' --actuate
```

No es evitacion de obstaculos ni garantia de estabilidad. FOV, ganancias y tiempos
siguen siendo provisionales; una deriva angular inicial ya no es por si sola
error, pues ahora el rumbo objetivo apunta a la deteccion, no a la orientacion
de la mano al soltar. Evaluar X/Y, gyro, referencia visual y accion real.

Verificacion:19 pruebas Python del lanzador/empaquetado/Ground aprobadas, incluyendo
rechazo de firmware viejo antes de ARM, STOP con tramas nuevas, no tomar otra
sesion armada, transicion sin reset y cancelacion de voz ante fallo. Verificacion
por contador de paquetes, no igualdad de marcas de tiempo Windows. Pruebas C++
de signo/FOV/wrap/slew y P03 visual real con hardware simulado aprobadas, asi
como regresion del P03 constante y BalloonMission. No se acciono hardware.

## Verificacion de software

Prueba matematica: signo de frenado ante deriva derecha a error angular cero,
integral conservado, proporcionalidad de servo, limites de servo y potencia,
recalculado de etaZ con slew, techo 16%, y perdida vertical por falta de margen.
Prueba del P03.cpp real con hardware simulado: ruta experimental con altura NaN,
compensacion de giro, limite 16%, fallo gyro que desarma y no rearma solo.
Regresion de BalloonMission y Ground/P12 aprobada; P03 sin la opcion conserva
su paquete previo. La opcion se rechaza en otros modos para evitar creer que
desactiva el avance en P08/P11. No se ha validado aun la respuesta fisica.

## Confirmacion de posicion antes de ARM (2026-10-26)

El ensayo 20261026_132749_674952 perdio el objetivo durante las instrucciones,
antes de ARM a potencia cero. No llego a aplicar empuje. El lanzador ahora
exige medio segundo de deteccion reciente y estable despues de colocar el
blimp. Si falta objetivo, solicita readquisicion desarmada y vuelve a comprobar
antes de ARM. No requiere cambios de firmware, pines, Nicla ni app.

## Correccion de yaw durante preparacion (version 3)

La prueba 20261026_153418_653915 llego a RUN, pero perdio el objetivo por
el borde izquierdo (x22 a x7) y desarmo tras el limite de perdida. El firmware
anterior inhibia yaw durante PREPARE, aunque ya aplicaba sustentacion.

Version 3 mantiene referencia visual y frenado por giroscopio desde PREPARE.
No aprende integral mientras se sostiene el frame para evitar acumular error
por la restriccion manual. Al pasar a RUN conserva referencia, filtro de
velocidad, integral y posiciones de servo; empieza a aprender integral sin
reiniciar la correccion. Potencia 14-16%, limites mecanicos y STOP sin cambios.
El lanzador exige F11 version 3 antes de armar y acepta la compensacion de
potencia por inclinacion durante PREPARE. Requiere cargar ESP; no Nicla/app.
Esto corrige una inhibicion de software; el sentido y autoridad fisicos de
la compensacion siguen pendientes de verificar en el blimp.

## Compensacion medida de yaw (version 4, 2026-10-26)

Referencias en P00M, dos motores a15%, 5s nominales, sin PID/vision:
- 35/95: log yaw_neutral/20261026_164305_295825.jsonl, delta yaw -63.81deg
  en4.84s, media -13.17deg/s (derecha).
- 31.5/85.5: log yaw_neutral/20261026_164514_745414_left.jsonl, -6.57deg
  en4.92s, media -1.33deg/s. Reduccion aproximada90%.
El usuario confirmo ambos ensayos libres, sin contacto ni intervencion manual.
En el segundo observo algo de retroceso; no se considera vertical pura.

Solo el P03 visual experimental incorpora blend positivo .10 como feedforward.
El PID puede sumar, cancelar o invertir ese sesgo. El antiwindup usa la salida
total y la autoridad final sigue limitada a25%; no se suman limites de servo.
Se mantiene compensacion etaZ calculada desde35/95: no se redefine la geometria.
La compensacion de inclinacion puede ordenar algo mas de15% para la misma
demanda vertical, sin superar16%. Dos motores reciben igual potencia.
P03 normal, opcion de soporte sin vision y BalloonMission conservan su conducta.
El sesgo se aplica durante PREPARE/RUN y no se acumula en la integral ni se
duplica al soltar. STOP/perdida siguen llevando potencia a0 y desarmado.
El lanzador exige F11version4 antes de armar. Pendiente validacion fisica con
vision y otras potencias/estado de bateria; el sesgo es provisional para este
montaje. No requiere cambios de pines, Nicla o app, pero si cargar la ESP.

## Base revisada para reducir retroceso (version 5)

Ensayo yaw_neutral/20261026_165740_051028_equal_tilt.jsonl: 28.5/88.5,
ambos motores15%, delta yaw +1.28deg en4.92s, excursion2.10deg. El usuario
observo traslacion menor y mas manejable; contacto con techo coincidente con
apagado. Se acepta el tramo previo al apagado como referencia provisional.
El ensayo visual version4 anterior tuvo contacto con lampara: no valida su
recuperacion de rumbo ni demuestra estabilidad del lazo cerrado.

Version5 conserva el sesgo de yaw e incorpora ajuste de base -3deg en S1 y
+3deg en S2; a error/velocidad/integral cero produce28.5/88.5. PID suma su
correccion sobre esa base, con el mismo limite de autoridad y velocidad.
Los objetivos finales se recortan al envolvente angular previo; no se amplia
recorrido. Se calcula etaZ con los angulos finales, referencia35/95 intacta.
No es un mezclador de giro puro para cualquier mando ni retencion de posicion.
No elimina toda traslacion ni se ha validado aun con vision/PID activos.
Solo afecta P03 experimental visual; pines, mision, modos previos, Nicla y app
sin cambios. Potencia normal14-16%, fallos/STOP cero. Lanzador exigeF11version5.

## Avance opcional para compensar retroceso (version6)

El usuario confirmo retroceso persistente con version5. La referencia26.5/90.5
tambien retrocedio y tuvo contacto con lampara: no valida el giro posterior.
La convencion existente FORWARD es S1 hacia120 y S2 hacia0. Los ultimos ajustes
de traslacion (-S1,+S2) iban en el sentido opuesto; no se continua ese barrido.

Solo el ensayo visual permite TX0..0.01 con --forward; por defectoTX0 mantiene
el ensayo sin avance solicitado. Se transforma en inclinacion comun +S1/-S2,
max4grados, rampa6grados/s. Se mantiene base28.5/88.5, PID residual, limites
anteriores de servo y motor14-16%. La demanda es una fraccion nominal de empuje,
no velocidad ni porcentaje adicional de motor. No se aumenta el techo16%.

Habilita solo tras soltar, con FULL valido y muestra<=200ms, error yaw<=8deg,
x dentro120+/-24, y dentro80+/-30, w<=120 y h<=112. Preparacion, parcial, falta
de observacion, descentrado, giro o region grande ponen solicitud en0. No son
medidas de distancia/contacto. Servos conservan slew; anular avance no frena
instantaneamente la inercia. Perdida700ms/fallo/STOP mantienen corte y desarmado.
F11 ultimo campo informa la solicitud de avance; lanzador exigeversion6 y
supervisa que no supere lo autorizado. TX0 conserva la conducta sin esta opcion.
No es control de posicion horizontal: falta medir velocidad/desplazamiento y
validar el sentido/efecto fisico. No se ha ejecutado ensayo actuado con avance.
No cambian pines, Nicla, app o BalloonMission; requiere cargar la ESP.

## Referencia aceptada48.5/68.5 (version7)

Prueba yaw_neutral/20261026_174107_464509_forward_angle16.jsonl, 44.5/72.5,
15%, 5s: usuario observo casi nada de retroceso. Siguiente referencia
20261026_174243_862867_forward_angle20.jsonl, 48.5/68.5, igual potencia/tiempo:
usuario observo ni avance ni retroceso y eligio esa posicion. Son observaciones
visuales, no mediciones de velocidad; referencia provisional para montaje,
flotabilidad y bateria actuales. No se afirma retencion de posicion a largo plazo.

P03 visual usa esa base desde PREPARE, sin sumar antiguos sesgos ni exigir
--forward para mantenerla. PID residual mueve ambos servos por igual en grados
al girar, conservando la diferencia angular calibrada. Autoridad25% se centra
ahora en esa base; con ese ajuste los rangos software son S1[36.375,61.375] y
S2[56.375,81.375], ambos dentro del limite mecanico0..120. Velocidad30deg/s.
El modelo de eficiencia se normaliza a la nueva base: demanda15% reproduce
potencia15% a correccion cero. Solo compensa cambios relativos de inclinacion;
no se redefine la geometria global35/95 ni se afirma que el modelo mida empuje.

Potencia normal14-16%, fallos/STOP cero, perdida700ms y plazos sin cambios.
Siguiente ensayo visual sin --forward: la opcion agrega avance adicional y no
es necesaria para la compensacion aceptada. P03 normal/soporte sin vision y
BalloonMission no cambian. Lanzador exigeF11version7 antes de armar. Requiere
cargar ESP; no pines, Nicla o app. Pendiente validacion de la nueva base con PID.

## Anticipacion de altura visual (version8)

Ensayo visual_support/20261026_175003_714165.jsonl completo8s con deteccion:
usuario confirma contacto con techo. No valida estabilidad ni el tramo posterior
al contacto. Datos muestran empuje15-16% durante acercamiento vertical; al
pasar el centro y caer el globo en imagen recien baja al piso14%.

Solo VisualSupport/P03 experimental usa proyeccion del error vertical0.8s
con la derivada filtrada de imagen existente. Cuando el centro del globo se
desplaza hacia el centro por subida, reduce sustentacion antes de cruzarlo;
en descenso hace la correccion inversa. No predice distancia ni contacto.
Integral de hover solo aprende con movimiento lento(<0.12 error normalizado/s).
La proyeccion se deshabilita en centros parciales/recortados y se reinicia
la derivada tras huecos; no se estima velocidad de un centro truncado.

La funcion compartida tiene parametro opcional por defecto0: BalloonMission
conserva su ley previa. Base48.5/68.5, referencia15%, piso14%, techo16%,
perdida700ms, STOP/desarmado y pines sin cambios. El piso limita el frenado
posible: no hay empuje vertical negativo ni sensor de techo. Ganancia0.8s
provisional, requiere nueva prueba fisica sin contactos. No garantiza ausencia
de sobrepaso. Lanzador exigeF11version8; falta cargar ESP, no Nicla/app.

## Frenado con minimo positivo10% (version9)

Ensayo visual_support/20261026_210913_548759.jsonl: redujo al14% antes
del centro, pero el usuario confirma que llego al techo y el retorno fue por
rebote. NO valida estabilizacion ni reduccion suficiente de velocidad.
La demanda visual bajaba aproximadamente a12%, recortada por piso motor14%.

El lanzador ahora transmite minimo10% en el YAW_PACK existente. El firmware
impone un minimo positivo10% para control visual normal; referencia15% y
techo16% se conservan. La misma ley anticipada0.8s puede bajar gradualmente
de14 a10 cuando lo pide posicion/velocidad de imagen, sin desarmar; no se
manda10% constante. Recupera empuje al detectar descenso hacia el centro.
Antiwindup usa el piso efectivo10% convertido por etaZ. No cambian yaw, base
48.5/68.5, pines, modo sin vision, BalloonMission, Nicla ni app. STOP/fallo/
perdida siguen mandando cero y desarmando; no los bloquea el minimo positivo.

Prueba de software compara la misma demanda antes del centro: piso14 impide
frenado adicional, piso10 permite potencia entre10 y14. Tambien comprueba
recuperacion de potencia antes del centro en descenso, limites y perdida.
No es un modelo identificado de masa/helio ni deteccion de techo. Minimo10
provisional para este ensayo, pendiente validacion fisica sin contactos.
Lanzador exigeF11version9 para evitar probar el firmware previo por error.

## Velocidad vertical y sustentacion aprendida (version10)

Referencia visual_support/20261026_212943_027293.jsonl: el usuario confirma
contacto con techo pese a percibir mayor estabilidad. No valida el retorno.
Primeros3s: globo parcial/recortado por arriba,16% sin derivada fiable. Techo
no detectable con esta telemetria: no hay distancia ni separacion garantizada.

Solo P03 visual usa cascada: posicion proyectada solicita velocidad de imagen
acotada a0.10 error/s (~8px/s); PI de velocidad compara con derivada filtrada
y adapta la estimacion de sustentacion. En el centro pide velocidad0, no15%
fijo. Si sigue subiendo con objetivo debajo, salida menor a la sustentacion
aprendida; recupera potencia si esta cayendo para frenar esa caida. Ganancias
.18/.04 provisionales y anticipacion0.8s. No estima velocidad en metros/s.
Un contorno parcial no aumenta empuje sobre sustentacion estimada, sin I/D
de centro truncado. Ensayo exige FULL estable sin recorte vertical antes de
armar, conservando seguimiento parcial durante ejecucion.

BalloonMission usa los parametros por defecto y conserva su ley anterior.
Base48.5/68.5, piso10%, techo16%, STOP y pines sin cambios. Si el minimo ya
mantiene ascenso no puede frenarlo sin revisar flotabilidad/minimo; no es
sensor de techo. Lanzador exigeF11version10. Pendiente validacion fisica.

## Avance ampliado (version11)

Ensayo visual_support/20261026_214901_840873.jsonl: usuario confirma centrado,
sin contacto con techo y sin avance. Sustentacion estimada15 a12.7%. Se pidio
avance0.01, limitado a4grados y anulado por pequenas desviaciones: limita
fuerza/tiempo disponible; no demuestra fallo mecanico.

--forward solicita0.03 (unidad nominal de empuje, no incremento de potencia).
Inclinacion adicional maxima12grados y rampa12grados/s. Base48.5/68.5 y ley
vertical version10 intactas. Avance usa solo margen angular restante en bloque;
yaw conserva prioridad. No aumenta rangos software de servo ni techo16%.
Entrada centrada x+/-24,y+/-30,yaw8deg; mantiene hasta x+/-36,y+/-40,yaw12deg.
Al salir exige centrar otra vez. Falta de muestra, parcial, region grande o
preparacion anulan avance. Sigue optativo/temporizado; no es recorrido de pista.
Lanzador exigeF11version11. ESP cargada; pendiente verificar avance fisico.

## Arranque desde abajo con PARTIAL (lanzador, 26 octubre)

Se elimina el requisito FULL y sin recorte antes de armar. Se admite FULL o
PARTIAL confirmado, con telemetria reciente durante0.5s. SEARCH, CONFIRM,
MISSING, AMBIGUOUS, LOST o datos atrasados no permiten arrancar. El LED verde
puede indicar objetivo aceptado PARTIAL; no equivale a FULL ni a ausencia de
deteccion. La clasificacion no prueba que el globo fisico este recortado.

Firmware version11 sin cambios: regula altura por imagen, yaw con IMU y
direccion visual. PARTIAL mantiene seguimiento sin aprender I/D del centro
truncado ni aumentar empuje por encima de la sustentacion estimada. Avance
solo al disponer de FULL centrado y alineado; no se autoriza por PARTIAL solo.
Esto permite iniciar abajo con un objetivo parcial ya confirmado, pero no
implementa busqueda ascendente si no existe ninguna deteccion.

El lanzador registra height_reference una vez, durante RUN, tras0.5s de FULL
con x120+/-24 e y80+/-10 y lectura barometrica reciente. Es referencia
diagnostica de la lectura del sensor al centrar la imagen; no altura absoluta
del globo ni nueva realimentacion barometrica de motores. No supone que la
camara este nivelada. No se sube firmware ni app para este cambio.

La regulacion vertical sigue visual, no mantiene esta referencia con barometro
al perder el objetivo. Sin Flowdeck ni otro sensor de desplazamiento no hay
medicion de avance real: F11 informa demanda, F3 comandos, y el cambio de
tamano de imagen solo es un indicio de aproximacion. Pendiente integrar y
validar mantenimiento de referencia relativa con sensores en BalloonMission.

## Recuperacion temporal a bordo (version12)

La prueba version11 cancelaba la cuenta al primer MISSING y el firmware
desarmaba tras700ms. No cumplia la tolerancia a balanceo solicitada.
Version12 conserva PARTIAL como objetivo valido, suspende avance al perder
vision y mantiene sustentacion aprendida, sin retener la ultima orden de
subida. Durante los primeros500ms conserva rumbo; luego busca +/-8grados
alrededor del ultimo rumbo visual, periodo4s, sin acumular giros. La IMU
amortigua yaw; se congela su integral en HOLD. Al recuperar vuelve al control
visual y exige de nuevo centrado para avance. Sin recuperar durante4s se
desarma; no se elimina STOP ni el limite total15s del ensayo.

Durante RUN la ESP captura una referencia barometrica relativa una vez,
tras500ms de FULL centrado x120+/-24,y80+/-10, sin recorte vertical inferido,
pitch dentro10grados y barometro saludable/reciente (edad<=250ms). No aprende
referencia mientras se sujeta en PREPARE ni de una region PARTIAL. Es la
lectura del sensor al centrar la camara; no altura absoluta medida del globo.
F4 height_ref publica la referencia; antes de captura vale0.

En HOLD con referencia y barometro valido, correccion de sustentacion:
0.04*(referencia-altura)-0.025*vz, limitada a+/-0.02 de demanda nominal,
sin integral. Mantiene piso motor10%/techo16%. Sin referencia o barometro
fiable conserva sustentacion aprendida y busca por yaw; no inventa altura
ni asciende buscando a ciegas. No hay sensor de techo, posicion horizontal
ni avance real medido. La correccion barometrica es provisional y acotada,
no soluciona la deriva del sensor. Version12 requiere cargar nueva ESP;
Nicla, pines, PCB y app no cambian. BalloonMission aun conserva su controlador
anterior; estos cambios se validan en P03 antes de integrarlos en la mision.

El lanzador exige version12 y tolera una perdida breve durante la cuenta
sin cancelar por un MISSING instantaneo. Para iniciar la primera entrega de
potencia espera un objetivo hasta3s. Desconexion, bateria baja, fallo a bordo
o tiempo agotado siguen deteniendo. No se declara validacion fisica por las
pruebas simuladas: queda pendiente cargar y probar recuperacion sin contactos.
