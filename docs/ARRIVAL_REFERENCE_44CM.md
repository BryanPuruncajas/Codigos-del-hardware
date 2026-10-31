# Referencia visual de llegada: 44 cm

**Documento histórico.** El 2026-10-28 el usuario eligió 80 cm / 120 px.
Consultar `ARRIVAL_REFERENCE_80CM.md` para el ajuste y comando actuales. Las
capturas de este documento usaron 220 px; sus resultados no cambian. Para
reproducir su secuencia, ejecutar el script con
`--near-cm 44 --far-cm 80 --threshold-px 220`.

Esta prueba contrasta el umbral provisional **220 px** ya instalado en la ESP.
Los **44 cm se miden entre cámara y superficie del globo**, no hasta su centro.
No cambia firmware, calibración, pines ni aplicación. No activa motores ni
envía órdenes de control: su único mensaje de salida es autenticación `viewer`.

## Ejecutar en CMD

Ground debe estar abierto con su puerto actual (COM9). No abrir otro lector del
puerto ni conectar USB a las placas montadas y alimentadas desde la PCB.

```bat
cd /d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation"
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u check_arrival_reference.py
```

La voz guía: preparar 80 cm, fijar FULL, mantener 8 s, acercar a 44 cm en
20 s, mantener 8 s, volver a 80 cm en 20 s y mantener 8 s. Se puede ampliar
el traslado con `--move-seconds 30`. Las pausas empiezan después de cada voz.
Las distancias físicas las mide el operador; la cámara no las mide.

## Qué valida

- Observación confirmada FULL previa en la misma asociación.
- Ancho confirmado >=220 px durante >=500 ms y al menos tres muestras UART
  distintas observadas por Ground.
- A 44 cm debe cumplirse esa referencia; a 80 cm no debe cumplirse.
- Cambio de asociación, datos repetidos o pérdida no heredan confirmación.
- Requiere modo 0/desarmado durante toda la captura. Si cambia, termina.

El resultado `visual_reference_separated` describe únicamente esas mesetas y
esa toma. Un `false` no cambia automáticamente el umbral. Los registros quedan
en `Codigos-del-hardware\logs\arrival_reference\*.jsonl`.

## Alcance pendiente

**No ejecuta ni valida `waypoint_cleared` de la misión a bordo.** La ESP también
comprueba alineación, cobertura del centro por la ROI recortada, referencia
barométrica de altura, yaw y velocidad de giro. Esa validación integrada es otra
prueba. La telemetría de esta captura no expone todos esos estados internos.
El anuncio final dice «referencia visual», nunca contacto o waypoint completado.

La evidencia anterior incluía 215 px a 80 cm y 220–240 px a unos 44 cm. El margen
es pequeño y depende de orientación/segmentación; 220 px no garantiza distancia
métrica ni contacto. PARTIAL/recorte por sí solo no significa llegada.

Implementación: `groundstation\check_arrival_reference.py`.
Comprobaciones: `python -B -m unittest test_arrival_reference -v` (7 casos).

## Captura 2026-10-28, 20:23

Registro: `logs\arrival_reference\20261028_202329_630632.jsonl`.

| Posición solicitada por voz | Ancho mínimo / mediano / máximo | Confirmadas | Referencia 220 px |
|---|---|---|---|
| 80 cm iniciales | 112 / 114.5 / 117 px | 80 de 80 | No |
| 44 cm | 177 / 202 / 216 px | 80 de 80 | No |
| 80 cm de retorno | 113 / 116 / 120 px | 79 de 79 | No |

La asociación permaneció en época 4 y hubo observación confirmada en todas las
muestras de las tres mesetas. El ancho de 44 cm solicitado no alcanzó el umbral
actual. No hubo cambios al firmware ni al umbral. Falta que el operador confirme
que mantuvo realmente los 44 cm cámara-superficie durante la meseta; la secuencia
por voz no mide esa distancia. La prueba no valida llegada a bordo.
