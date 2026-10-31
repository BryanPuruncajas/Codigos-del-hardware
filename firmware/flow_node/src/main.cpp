#include <Arduino.h>

unsigned long ultimoAviso = 0;
unsigned long bytesRecibidos = 0;

void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 44, 43);
}

void loop()
{
  while (Serial1.available()) {
    Serial.write(Serial1.read());
    bytesRecibidos++;
  }

  if (millis() - ultimoAviso >= 3000) {
    ultimoAviso = millis();
    Serial.printf("\n[XIAO] Bytes UART recibidos: %lu\n",
                  bytesRecibidos);
  }
}