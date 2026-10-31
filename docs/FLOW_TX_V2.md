# Retransmisor Flowdeck: FLOW-TX-v2

Proyecto: `C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\flowdeck_sender`

Archivo: `FlowdeckEspNow\FlowdeckEspNow.ino`. Destino: ESP emisora dedicada, MAC `7C:4F:AD:1D:CA:D4`. No cargar este proyecto en Ground ni en la ESP principal del blimp.

Este programa recibe por UART; no es el programa que adquiere las lecturas del sensor y genera `FLOW,...`.

## Cambios respecto al código compartido

- D7/GPIO44 RX, D6/GPIO43 TX sin conectar y 115200 baudios permanecen iguales.
- Conserva broadcast y el paquete de 16 bytes versión 1, compatible con el blimp actual.
- Arranca sin esperar una consola USB. Buffer RX de 1024 bytes, trabajo UART acotado por vuelta y registro resumido cada dos segundos, solo con consola disponible.
- Sustituye String por un buffer fijo; descarta por completo líneas demasiado largas y valida límites numéricos.
- Transmite `squal` en `packet.quality`; no atribuye unidades de velocidad a dx/dy.
- Canal fijo 1 y ahorro Wi-Fi desactivado. Los demás nodos deben usar el mismo canal.
- No reenvía medidas antiguas ni inventa muestras para ocultar interrupciones.

## Diagnóstico por consola

La línea `FLOW-TX-v2` muestra contadores acumulados:

| Campo | Interpretación |
|---|---|
| `bytes`, `edad_byte` | Actividad UART, incluso si las líneas son inválidas. -1 significa que no se recibió ningún byte. |
| `UART`, `edad_UART` | Líneas completas válidas y edad de la última. |
| `invalidas` | Líneas con formato inválido o exceso de longitud. |
| `encolados`, `errores_envio`, `ultimo_error` | Resultado local de esp_now_send. Encolado no confirma recepción. |
| `seq`, `z`, `calidad` | Última muestra válida recibida por UART. |

Si `bytes` deja de crecer, investigar el productor UART, su alimentación y la conexión. Si bytes crece pero UART no, investigar el formato y la delimitación. Si UART y encolados crecen mientras F6 envejece, investigar el enlace ESP-NOW/recepción. Estos indicadores localizan el corte; no prueban por sí solos una avería concreta.

## Validación y estado

2026-10-30: compilación PlatformIO correcta (RAM 43520 bytes, flash 694841 bytes). Prueba nativa del parser y comparación del protocolo compartido correctas.

Carga realizada el 2026-10-30 por COM3 en MAC 7C:4F:AD:1D:CA:D4: SUCCESS, hash verificado. Consola FLOW-TX-v2 operativa. Primera observación tras la carga: bytes=0, UART=0, encolados=0; no llega entrada UART a D7 en esta condición. Esto sitúa la comprobación siguiente en el productor UART, su alimentación o cableado; no demuestra cuál falla. No se modificó firmware del blimp, Ground o app en esta intervención.

Para cargar la emisora, su batería debe estar desconectada y la placa alimentada solo por USB, fuera de cualquier PCB alimentada.

