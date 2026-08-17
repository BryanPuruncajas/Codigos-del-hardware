# Firmware XIAO ESP32-S3

Un único firmware contiene P01–P11. `main.cpp` arranca siempre desarmado y despacha el modo recibido por ESP-NOW.

Puntos fijos de hardware: D2=RST BNO (HIGH), D0/D1=servos, D9/D10=brushless. El límite duro de motores es 50%.
## Hardware de sensores activo (tesis)

Esta version NO usa GY-US42V2/ultrasonico. La ruta activa contiene:
- BNO085/BNO086 por I2C (D2 reservado como RST, D4 SDA, D5 SCL).
- BMP390 por I2C.
- Monitor de bateria por D8 (divisor de la PCB).
- Nicla Vision por UART como subsistema de percepcion.

Por ello la Nicla comienza en `sensors[11]` y el arreglo completo tiene 21 valores.

