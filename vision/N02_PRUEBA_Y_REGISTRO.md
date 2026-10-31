# N02-v1 - seguimiento geometrico USB (2026-10-08)

Estado: implementado, 10 pruebas de logica en PC aprobadas; pendiente Nicla.
Script independiente nicla_n02_tracking.py; N01-v3.1 intacto. Sin UART, ESP,
actuadores, waypoint, llegada ni cambio automatico de globo.

Mantiene exactamente los filtros N01-v3.1 (incluido minimo20x20) y mascara.
Adquiere el F de mayor caja solo tras 5 fotogramas y300ms coherentes.
Si los dos mayores difieren menos del10% de area, no elige (AMBIG).
T1 es una identidad local de esta ejecucion, NO globo1 de competencia.
Seleccion inicial por tamano no garantiza proximidad fisica.
Asocia por distancia de centros (25-50px segun caja) y cociente de lados
0.5..2; para parciales permite0.25..2. Umbrales experimentales.
Dos candidatos compatibles: AMBIG, sin medida. Ninguno: MISSING.
Tras mas de1000ms sin asociacion: LOST enclavado hasta detener/reiniciar.
Una oclusion corta permite recuperacion geometrica. Cruces, fusiones,
objetos parecidos o falsos positivos persistentes pueden confundir identidad.
No es reconocimiento semantico ni seguimiento validado para vuelo.
P es region parcial: centro/tamano NO representan el globo completo.

COLOR muestra T0/T1 y estado al pie; cruz blanca en candidato asociado.
MASK sigue limpia. N01V3_FRAME/BLOB conservan esquema anterior.
N02_TRACK: frame,uptime_ms,track_id,state,observed,x,y,w,h,cx,cy,age_ms.
FULL/PARTIAL tienen medida actual observed=1. Otros estados observed=0,
coordenadas -1, nunca coordenadas antiguas como observacion vigente.
SEARCH/CONFIRM/AMBIG antes de adquirir usan T0; tras adquisicion T1.
N02_TRACK se imprime junto a muestreo de logs (200ms), no cada frame.

## Prueba de hardware pendiente
1. Motores desconectados, conectar Nicla USB. Abrir nicla_n02_tracking.py
   desde disco y Run. No requiere copiar modulos ni guardar main.py.
2. Presentar solo un globo entero a1.5m por5s: esperar T1 FULL.
3. Mover lentamente izquierda/derecha por10s: cruz blanca debe seguirlo.
4. Introducir segundo globo separado en imagen: no debe cambiar al mayor.
5. Recortar parcialmente el objetivo: PARTIAL si supera20x20.
6. Retirar objetivo >1s: LOST sin saltar al segundo. Detener/Run para repetir.
Guardar log desde N01_CONFIG,N02-v1 y video. Si hay cruce/solapamiento,
anotarlo como ambiguo; no atribuir identidad solo por el numero T1.
No evaluar tramos de colocacion/retirada como fallos sin contexto del video.

## Evidencia previa N01-v3.1, no validacion de N02
Globo4m: tramo inicial82/82 F, fondo final98 muestras sinF.
Dos globos: mejora77/125 a155/169 F2 segun orientacion/iluminacion.
Globo1.5m:112 muestras consecutivas F durante26.7s; usuario confirma distancia
y colocacion/retirada fuera de tramo. Movimiento lateral: continuidad F/P
aproximadamente31.8s; no mide precision ni error de control.
Word pendiente de sincronizacion. Este archivo conserva el registro N02.
