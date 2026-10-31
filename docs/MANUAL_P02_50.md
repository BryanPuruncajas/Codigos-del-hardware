# P02 manual: potencia ajustable y estado real

Proyecto: `C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware`.

Preparado el 30/10/2026. **El firmware manual de este documento y la nueva app todavía no están instalados.** El blimp sigue con la carga v40 original, altura exclusivamente Flowdeck y P02 fijo al 20%.

## Cambios preparados

- P02 permite seleccionar 10–50% desde el panel manual de la app, en pasos de 5%, con valor inicial 30%. El ajuste se envía en AUX0/slot 5 como porcentaje; la siguiente pulsación lo aplica. Al soltar, los ejes van a cero y los motores quedan sin potencia. El deslizador se bloquea mientras se mantiene una dirección.
- Para la app anterior, que no envía AUX0, el firmware nuevo usa 30% por defecto. No se puede usar el selector nuevo con el firmware antiguo: este último ignora el porcentaje y sigue al 20%.
- El firmware limita P02 al 50%, conserva cualquier límite inferior del actuador, rechaza valores no finitos/negativos y publica cero de potencia cuando está desarmado. No cambia geometría, pines, reparto de motores ni controles automáticos.
- La sesión manual pasa de 20 a 120 segundos. Se conserva el plazo de parada, STOP y parada al desconectar la app. La app muestra botones de movimiento únicamente con F3 reciente, modo 11 y armado confirmado. Al desarmarse vuelve a mostrar «Armar P02» sin reconectar.
- La corrección previa de Ground que evita enviar STOP antes de cada pulsación de dirección ya se había activado. **El nuevo plazo de 120 s requiere reiniciar Ground con sus fuentes actualizadas.**

## Archivos

- `firmware\src\tests\P02ManualControl.cpp`: potencia de P02 y neutro.
- `groundstation\telemetry_bridge.py`: sesión manual y duración.
- `blimp_app\lib\widgets\manual_control_panel.dart`: selector y armado basado en F3.
- `blimp_app\lib\screens\dev_screen.dart`: integra el panel.
- `blimp_app\build\app\outputs\flutter-apk\app-debug.apk`: APK compilado con selector hasta 50%.

## Validación y estado de despliegue

Firmware compilado: RAM 50 936 bytes, flash 839 341 bytes; binario 839 712 bytes, SHA256 `6A905F2FCE2E2212BBBB1F29DAB1EBC6E93BD5B1764D55D4CBFB8AD6E24EA50C`.

Prueba del controlador P02 real con actuadores simulados: correcta; prueba porcentaje, tope 50%, límite inferior, neutro, descenso pasivo y desarmado. Once pruebas de Ground correctas. Quince pruebas de app correctas antes del ajuste final del máximo; cuatro pruebas específicas del panel, incluyendo envío de 50%, correctas después del ajuste. APK final compilado correctamente.

Pendiente: cargar la ESP fuera de la PCB, sin batería y solo USB; instalar la app; reiniciar Ground con el blimp desarmado; comprobar físicamente potencia, neutro y STOP. Nicla no necesita una actualización por estos cambios.
