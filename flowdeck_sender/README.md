# Emisor Flowdeck UART → ESP-NOW, compatible con v38

Proyecto:

`C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\flowdeck_sender`

Se carga en la **ESP dedicada a Flowdeck**, no en la ESP del blimp ni en Ground.

- RX D7/GPIO44 a 115200, 8N1; TX D6/GPIO43 no se utiliza para esta entrada.
- Formato actual: `FLOW,secuencia,dx,dy,distanciaMm,squal,motion` seguido de salto de línea.
- También acepta el antiguo `dx,dy,z_mm`, sin calidad disponible.
- Broadcast, paquete binario de 16 bytes con el protocolo existente `0xB17F`, versión 1.
- Canal 1 configurable en `FLOW_WIFI_CHANNEL`; debe coincidir con blimp/Ground.
- El campo `squal` ahora se transmite en `packet.quality`. `motion` se valida como número sin atribuirle un significado no documentado.
- El emisor no convierte dx/dy a m/s. No se conoce una escala métrica ni el intervalo original de adquisición a partir del formato UART solamente.
- No retransmite muestras antiguas por temporizador. Los envíos fallidos y líneas inválidas quedan contados en el monitor.

Tu emisor broadcast compartido en el chat ya es compatible con **altura**. Para añadir calidad, basta copiar `squal` al campo `packet.quality` antes de enviar. Este proyecto ofrece además validación de rangos y descarte completo de líneas truncadas.

En Arduino IDE, abrir `FlowdeckEspNow\FlowdeckEspNow.ino`, conservando `FlowProtocol.h` y `FlowUartParser.h` junto al sketch. Seleccionar XIAO ESP32S3 y el puerto del emisor.

Desde CMD, compilar:

```bat
"%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" run -d "C:\Users\stefa\Desktop\BLIMP\Competencia\Blimp\Blimp\Codigos-del-hardware\flowdeck_sender" -e seeed_xiao_esp32s3
```

Para cargar, añadir `-t upload --upload-port COMx` con el puerto identificado del emisor. No se ha cargado ninguna placa como parte de esta actualización.

El contador `encolados` confirma aceptación de envío por la API; la recepción real se comprueba en F6 de la app. Las líneas `FLOW,1,4,-2,800,75,128` deben dar dx=4, dy=−2, rango=800 mm y calidad=75.

La altura del blimp y su velocidad vertical se calculan/controlan en la ESP del blimp, no en este emisor. Ver la [guía de integración v38](../docs/FLOWDECK_CONTROL_V38.md).

Validación sin hardware: `test_flow_sender.sh` compara el formato compartido y ejecuta el parser con valores normales, límites, secuencias y entradas inválidas. Compilado con Espressif32 7.0.1 / Arduino-ESP32 2.0.17.
