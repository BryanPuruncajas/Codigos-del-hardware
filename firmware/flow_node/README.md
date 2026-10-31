# Prueba ESP-NOW para el segundo XIAO

Este proyecto verifica primero el enlace entre dos XIAO ESP32-S3. Todavía no lee
PMW3901 ni VL53L1X: transmite valores de ejemplo para comprobar que el formato y
la recepción funcionan antes de añadir sensores.

## Mensaje

El formato compartido está en `../lib/BlimpSwarm/src/comm/FlowProtocol.h`.
Contiene identificador y versión, contador de secuencia, `dx`, `dy`, distancia
en milímetros, calidad y banderas de validez. La estructura ocupa 16 bytes.

## Carga

1. Carga este proyecto en el XIAO #2 desde la carpeta `firmware/flow_node`.
2. Carga el firmware habitual de `firmware` en el XIAO #1.
3. Enciende ambos con alimentación segura y abre los monitores Serial a 115200.
4. El XIAO #2 debe imprimir `[TX]`; el XIAO #1 debe imprimir `[FLOW] FRESCO`.
5. Detén el XIAO #2: después de 500 ms, el XIAO #1 debe indicar `[FLOW] VENCIDO`.

El emisor usa broadcast solo para esta prueba y cada 100 ms envía `dx`/`dy`
variables, `range=820 mm` y `quality=87`. La estación base actual ignora estos
paquetes porque no tienen el tamaño de su telemetría normal. Más adelante se
puede restringir el receptor a la MAC del XIAO #2 y reemplazar los valores de
ejemplo por lecturas reales.
