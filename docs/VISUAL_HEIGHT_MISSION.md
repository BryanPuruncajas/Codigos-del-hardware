# Altura visual en Balloon Mission — 2026-10-24

Estado: experimental. Revision con correccion por un solo borde cargada el
2026-10-26 en ESP COM4 (MAC DC:B4:D9:39:B3:B4, hash verificado).
La correccion posterior de conservacion del objetivo al arrancar requiere nueva
carga. El centrado vertical actuado NO esta validado.

## Cambio

P08–P12 emplean el centro vertical HQVGA (y=80) para la demanda vertical
cuando siguen el globo (o detectan el primero). Y menor significa objetivo
arriba: aumentar empuje; y mayor significa abajo: reducirlo. No hay empuje
activo hacia abajo. No se cambia ningun pin, protocolo UART, Nicla ni servo.

El lazo visual tiene unidades de imagen, no metros: P=.06, I=.008/s,
D=.015 s; trim inicial provisional .10, recortado al maximo configurado
por el operador, con el slew ya existente. NO cambia P04/P05 ni sus ganancias.
No aumenta automaticamente el limite configurado. El 12% usado en las pruebas
P04 NO se convierte en un limite global de mision: revisar el maximo en la app.

Solo observaciones nuevas FULL y sin corte vertical ajustan el trim y el PI/D.
PARTIAL conserva el trim: su centro puede estar sesgado por el borde. Si solo
se corta por arriba o abajo, se permite correccion P limitada hacia ese borde,
sin integrar ni derivar el centro truncado. Si se corta por ambos bordes o no
hay evidencia de borde, se conserva solo el trim. No hay golpe D al volver a FULL.
Avance y llegada estimada requieren y=80 +/-18px ademas del centrado horizontal
y de los filtros existentes. La llegada por anchura sigue sin probar contacto.

Se elimina la prioridad de la referencia barometrica fija cuando hay seguimiento
visual y, despues de haber observado un objetivo, no vuelve a apropiarse de la
mision. Tampoco se reduce a la mitad el sostenimiento durante centrado de yaw.
El mixer conserva los limites configurados, pero altura/yaw comparten motores:
la inclinacion limita la autoridad efectiva disponible.

## Perdida / siguiente objetivo

Se retira avance inmediatamente. Durante los primeros 700ms se conserva el
ultimo rumbo observado y una referencia barometrica capturada al verlo.
Luego se explora +/-12 grados y +/-20cm barometricos, periodo 6s, alrededor
de esa referencia (sin acumular desplazamientos), hasta 10s desde ultima vision.
No se cambia a un giro completo ni se vuelve a marcar el waypoint como visitado.
LOST de Nicla pide reset del tracker, manteniendo la busqueda acotada en ESP.

Al adquirir el siguiente waypoint se conserva el rumbo de pista y se explora
verticalmente hasta 10s; FULL, alineacion de ruta y rechazo por tamano siguen
vigentes. Durante TURN se usa altura barometrica capturada como puente temporal.
Agotar recuperacion, o perder sensores indispensables durante ella, termina
en TURN_FAILED y desarma. El nombre de ese estado se reutiliza por compatibilidad
con el dashboard; no significa siempre un fallo del giro. STOP sigue disponible.

SEARCH inicial, antes de observar el primer objetivo, conserva el comportamiento
barometrico previo y la altura configurada. Esto no implementa despegue visual
ni un buscador global. P12 continua estrictamente desarmado.

## Limites y validacion requerida

La busqueda en cm es una referencia de barometro, NO una envolvente fisica segura:
la deriva sigue existiendo. No hay posicion horizontal, frenado activo ni garantia
de recuperar un objetivo fuera del campo visual. El pitch tambien desplaza y:
estas ganancias no incorporan compensacion geometrica de pitch y requieren prueba.
Si el empuje maximo no sostiene el peso, la vision no puede compensarlo.

Primero P12 sin motores: mover objetivo arriba/abajo y comprobar que no permite
avance cuando y queda fuera de banda. P12 mantiene demanda vertical fisica cero;
el signo y la magnitud del lazo vertical se comprueban con la prueba unitaria.
Despues comprobar perdida y recuperacion
en menos de 700ms y en 1–9s; despues de 10s debe fallar sin continuar la mision.
Verificar que dos candidatos no saltan los filtros de ruta/tamano. Validar signo
de y con montaje real antes de vuelo. Luego ensayo actuado breve con limite
explicito, bateria revisada y STOP disponible. No cargar por USB en PCB alimentada.

## Repetir pruebas de software (g++ en Linux/WSL, desde el proyecto)

```
g++ -std=c++11 -Wall -Wextra -Werror -Ifirmware/src/mission groundstation/test_visual_height.cpp -o /tmp/test_visual_height
/tmp/test_visual_height
g++ -std=c++11 -Wall -Wextra -Wno-unused-variable -Igroundstation/mission_host -Ifirmware/src groundstation/mission_host/test_mission.cpp -o /tmp/test_mission_visual
/tmp/test_mission_visual
```

La segunda compila el BalloonMission.cpp real con reloj, radio, Nicla y actuadores
simulados. Comprueba prioridad visual ante altura solicitada erronea, veto de avance
por Y, seguimiento con barometro invalido, perdida temporal, readquisicion, tiempo
limite/desarmado, ausencia de accion fisica en P12 y filtros de siguiente objetivo.
Estos tests NO modelan la dinamica del blimp ni validan las ganancias en vuelo.

## P12 con hardware, despues de la primera carga

Enlace Nicla fresco; 0 muestras de armado durante toda la prueba y STOP/desarmado
confirmados al terminar. Cuatro ventanas de 5s, guiadas por voz:

- Centro: y=56..80, x=116..185; 4 permisos de avance entre 24 decisiones.
- Arriba: y=32..50, x=103..123; 0 permisos entre 25 decisiones.
- Abajo: y=70..102, x=108..132; 18 permisos entre 25 decisiones. Parte de esta
  ventana aun estaba dentro de la banda central: no prueba avance fuera de banda.
- Centro final: y=52..63, x=111..123; 2 permisos entre 25 decisiones.

Estas ventanas NO son poses exactas ni muestras sincronizadas F2/F8. Confirman
deteccion, conexion y veto de avance al mantener arriba; no validan actuacion
vertical, descenso, ni sustentacion. Se observaron detecciones PARTIAL al borde
superior (y=36,h=84) e inferior (y=101,h=127); esto motivo la correccion P por
un solo borde. No se accionaron motores en esta prueba.

## Ensayo actuado y correccion de arranque — 2026-10-26

Antes de iniciar: enlace fresco, bateria 3.90V; P12 confirmo PARTIAL x=101,y=32,
w=143,h=81. Ensayo P08 con maximo explicito 12%, giro 6..10%, plazo 20s.
Al entrar P08, reset() envio R incondicional a Nicla: ack cambio de 1 a 2 y
se borro el target confirmado. SEARCH giro ciego durante casi todo el ensayo.
Al final reaparecio PARTIAL durante unos 2s (y=37..48), con salida 12%.
Solo 21/194 muestras compuestas por F3 tenian deteccion; F2/F3/F7 no sincronizados.
STOP y desarmado se confirmaron al terminar. NO valida sustentacion/centrado.

Cambio: al iniciar o reiniciar una mision, una observacion N03 FULL/PARTIAL
confirmada y fresca, sin reset pendiente y con flag/geometria validos, se
conserva sin enviar R y se entra directamente en APPROACH. La primera muestra
se consume una vez aunque aun no llegue otro frame UART; no se cuenta visita.
Si no hay observacion valida, se mantiene el reset/SEARCH anterior. No se modifica
el reset que exige el giro al siguiente waypoint ni los filtros de adquisicion.
No cambia ningun pin, potencia maxima, servo, Nicla, app, ni parametro P04/P05.

Pruebas de regresion: FULL/PARTIAL retenidos, generacion en cero/wrap, muestra
inicial consumida una vez, ARM/RESET sucesivo, rechazo de datos obsoletos/pending/
LOST/NaN/anchura cero/flag invalido, y P12 siempre sin accion fisica. Pendiente
nueva carga y repetir ensayo fisico breve con seguimiento ya adquirido.
