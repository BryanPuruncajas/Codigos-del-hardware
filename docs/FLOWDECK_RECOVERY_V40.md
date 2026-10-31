# Recuperación de altura para preparar el vuelo — v40

Proyecto: `C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware`

Estado al 30/10/2026: **v40 cargada correctamente en COM4, MAC DC:B4:D9:39:B3:B4; hash verificado y reinicio por RTS, en 15.72 s.** ESP fuera de la PCB, sin batería y solo por USB, confirmado por el usuario. No se completó un vuelo en los dos intentos anteriores; falta repetir con esta carga.

## Evidencia de las pruebas

- `logs\single_waypoint\20261030_151508_245074.jsonl`: v39 confirmada; la preparación se canceló inmediatamente después del armado de los ESC. La pausa de armado de unos 3.7 s caduca la ventana Flowdeck. Se confirmó desarmado al terminar.
- `logs\single_waypoint\20261030_151916_729844.jsonl`: no se llegó a armar. Flowdeck recibía rango fresco de unos 1.61 m, pero el estimador conservaba 0.57 m como última altura aceptada y rechazaba la recuperación por superar 30 cm de diferencia. F3 confirmó ambos motores a cero, desarmado y modo seguro.

## Cambios concretos

1. `groundstation\check_single_waypoint.py`: tras armar los ESC a potencia cero, espera hasta 5 s una **nueva** ventana Flowdeck cualificada antes de entrar al control automático. No acepta la muestra anterior al armado. Si no se recupera, termina y desarma.
2. `firmware\src\mission\FlowHeight.h`: permite recuperar la altura absoluta actual con una nueva ventana cualificada cuando el blimp está **desarmado**. Moverlo manualmente a otra altura ya no exige reiniciarlo. Conserva límites de rango, secuencia, inclinación, frescura y filtrado. Durante el vuelo mantiene el rechazo de retornos contradictorios.
3. `firmware\src\main.cpp`: entrega al estimador el estado real de desarmado. La recuperación no manda órdenes de armado ni cambia el objetivo de altura de la misión.
4. `firmware\src\app\VisualBearing.h` y `firmware\src\app\Telemetry.cpp`: identifican v40 en F11/F14.

Se conserva altura exclusivamente Flowdeck; no se inicializa ni lee el barómetro. No cambian pines, reparto de motores, potencia máxima, ángulos, Nicla ni lógica de llegada. Esta corrección requiere actualizar solo la ESP y usar el lanzador actualizado; no requiere otro cambio de Nicla o app respecto a lo ya preparado.

## Comprobaciones

- Compilación ESP: correcta; RAM 50 936 bytes y flash 839 229 bytes.
- Ocho programas nativos correctos, con regresión específica: 0.57 m → pérdida de rango → 1.61 m; se rechaza el retorno contradictorio armado, se permite recuperar desarmado, y se siguen rechazando rango e inclinación inválidos.
- 27 pruebas Python correctas, incluidas espera tras armado, rechazo de muestras viejas/invalidas, cuenta por voz, guardas y lector sin actuadores.
- Binario: 839 600 bytes; SHA256 `508FFB0089CECCEEFE6FFA70B2D383E1FF5D0BF8765EC22487065B36CF7CB993`.

## Repetir después de cargar

Ground debe seguir ejecutándose en COM9. En CMD:

```bat
cd /d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\groundstation"
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -X utf8 check_single_waypoint.py --actuate --seconds 120 --require-flow
```

Sostener el frame, dejar libre la vista de Flowdeck al suelo y soltar únicamente tras la cuenta por voz. El lanzador conserva el máximo de 25% de potencia y confirma desarmado al finalizar. La validación física de v40 sigue pendiente.
