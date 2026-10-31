# N03 panel de prueba manual

Actualizacion central v2: para cada adquisicion (T1, T2, etc.) se exige un
unico candidato F con centro horizontal entre 90 y 150 inclusive.
Dos candidatos F dentro producen AMBIG; ninguno produce SEARCH.
Las lineas blancas delimitan la franja mientras busca. Tras adquirir,
la franja no limita el seguimiento. No implica calibracion angular ni yaw
medido. Reiniciar el panel y la conexion para cargar esta version en RAM.

Solo banco, motores desconectados. No controla ESP, UART, GPIO ni actuadores.
N02 original permanece intacto. Este panel carga un programa temporal en RAM
mediante raw REPL; no guarda main.py ni modifica el firmware de la placa.

## Inicio

1. Detener el programa en OpenMV IDE y desconectar el IDE de la placa.
2. Ejecutar `iniciar_panel.cmd`. Usa el Python preparado en esta computadora.
3. Confirmar puerto de la Nicla (COM10 es solo el valor inicial) y pulsar
   **Conectar e iniciar**. Esta acción interrumpe cualquier script de esa placa.
4. Esperar imagen y T1. Mantener el foco en el panel durante el recorrido.

## Una tecla

- Seguir el globo hasta LLEGADA CANDIDATA: caja >=180x150 durante 2000 ms
  continuos. Es una hipótesis visual, no distancia ni contacto confirmado.
- Pulsar y soltar **1**: acepta manualmente la llegada y entra en GIRO.
  No se sigue ni adquiere ningún candidato durante esta etapa.
- Girar la cámara; el operador debe encuadrar el siguiente globo correcto.
- Pulsar y soltar **1** otra vez: habilita nueva adquisición como T2.
- La cruz blanca se dibuja solo sobre la observación actual del objetivo.
  Las cruces de color pertenecen a candidatos individuales, no a waypoints.
- **Esc** o Detener termina el ensayo y libera el puerto.

La primera orden se rechaza si no hay llegada candidata en ese instante.
No se acumulan pulsaciones mientras falta confirmación. Cada comando tiene
un número de secuencia; una retransmisión no avanza otra etapa.
LOST sigue bloqueado tras >2000 ms sin asociación. Detener y conectar inicia
un ensayo nuevo. No se reacquiere otro globo automáticamente después de LOST.

## Datos y limitaciones

Los logs completos se guardan automáticamente junto al panel en `logs/*.jsonl`,
incluyen estado, cajas, candidatos y órdenes. No se guarda video. La vista
es siempre COLOR para facilitar el recorrido. Se envía imagen como máximo
cada 200 ms; la detección corre entre envíos, pero el transporte puede bajar
los FPS y debe medirse en hardware. Sin imágenes actuales, se deshabilitan
las órdenes y se oculta la imagen vieja. Sin latidos del panel durante 8 s,
el programa de placa termina.

T2 significa segunda adquisición, NO identidad demostrada del segundo globo.
El panel no mide rumbo ni sabe si terminó el giro: esa confirmación es manual.
La misma mancha o un objeto rojo distinto puede ser adquirido si se encuadra.
Ni esta prueba ni la regla de tamaño autorizan vuelo o contacto autónomo.

El inicio USB, el JPEG y la tasa real requieren ensayo físico. Si falla,
anotar el error, detener el panel y volver a conectar con OpenMV IDE.
No usar simultáneamente ambos programas. No guardar el código como main.py.

## Pruebas locales

`test_panel.py` cubre confirmación, tiempo de llegada, pérdida, cambios de
etapa, duplicados, contador circular y límites del protocolo de paquetes.
No sustituye la validación de la Nicla real.

Fuentes del transporte:
https://docs.micropython.org/en/latest/reference/repl.html#raw-mode-and-raw-paste-mode
https://docs.openmv.io/library/pyb.USB_VCP.html
