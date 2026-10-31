# Predicción de yaw durante aproximación — versión27

Carga confirmada el28 de octubre de2026: ESP COM4, MAC dc:b4:d9:39:b3:b4; 836080bytes escritos, hash verificado y PlatformIO SUCCESS16,04s. No se iniciaron motores. La validación física de este ajuste sigue pendiente. Nicla no se modificó.

Proyecto: C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware.

## Última prueba física: versión26

Registro: logs/single_waypoint/20261028_005540_922467.jsonl.

ESP26 confirmada. Ocho impulsos; ancho detectado máximo154px frente al umbral provisional220px; visitados0; final por plazo de60s después de la cuenta y STOP confirmado. Trim de sustentación15% inicial y aproximadamente12,5% al final. El usuario confirmó acercamiento físico, correcciones después de choques, contacto con techo/pared/objetos y que el blimp no tocó el waypoint. Por tanto, hay avance observado pero no llegada ni estabilidad libre validadas.

## Cambio de versión27

P08 dejaba de corregir yaw durante preparación, impulso y retorno del vector. Sus límites instantáneos podían permitir un impulso con velocidad angular suficiente para salir de la alineación mientras la corrección permanecía suspendida.

Se comprueba el error futuro `error - yawRate * intervalo` además del error presente. El intervalo incluye preparación500ms, impulso máximo1200ms y retorno de mando600ms (18grados / 30grados por segundo). Es una proyección de velocidad constante, no una medida de desplazamiento ni un modelo completo de dinámica. Antes de impulsar usa8grados; durante el pulso usa12grados para anticipar su cancelación antes de llegar al límite instantáneo. Se mantienen los límites de píxeles, velocidad, confirmación y frescura.

No se modifican ganancias, duración máxima de pulso, calibración de servos, control vertical26, criterio de llegada ni pines. F13 bit32 indica limitación por proyección de yaw. No se aplican giro y avance simultáneos. El lanzador muestra la versión confirmada antes de armar.

La regresión del controlador real falla con versión26 porque inicia un impulso mientras sigue girando5,7grados/s. Con la modificación, espera el frenado, reanuda sin reiniciar y cancela temprano ante deriva creciente. Pasan también las regresiones de misión, PARTIAL y las ocho comprobaciones del lanzador. La validación física de27 está pendiente. No se carga firmware automáticamente si sólo está conectado Ground.

## Ejecución manual de la última prueba

Carpeta: C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation.

1. Ejecutar01_GROUND.cmd si Ground no está abierto. Usa COM9 y conserva esa ventana abierta. Si8765 ya tiene un puente, no abre otra instancia.
2. Ejecutar02_PRUEBA_WAYPOINT.cmd. Es una prueba CON MOTORES. Verifica telemetría, cámara y versión compatible, prepara ESC y anuncia la cuenta por voz. Duración60s después de la cuenta; termina antes ante llegada o fallo.
3. Para interrumpir: Ctrl+C en la ventana de prueba; comprobar STOP_CONFIRMADO: True. También está disponible STOP de la app mediante Ground.

Los registros quedan en logs/single_waypoint. No se requiere ejecutar una consola como administrador. En el montaje alimentado por PCB, sólo Ground se conecta por USB al PC, respetando la restricción eléctrica de este equipo.

El lanzador acepta26 o posterior para poder repetir la última prueba ya instalada. Ver FIRMWARE_CONFIRMADO en consola:27 sólo se informa después de cargar el firmware nuevo.
