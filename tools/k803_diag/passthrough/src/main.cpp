// Diagnostic only: USB console <-> K803 COM1, same UART and pins as the rover env.
#include <Arduino.h>

static HardwareSerial Gnss(1);

void setup() {
  Serial.begin(115200);
  Gnss.setRxBufferSize(4096);
  Gnss.begin(115200, SERIAL_8N1, 17, 16);
}

void loop() {
  static uint8_t buf[256];
  size_t n = Gnss.available();
  if (n) {
    if (n > sizeof(buf)) n = sizeof(buf);
    Gnss.readBytes(buf, n);
    Serial.write(buf, n);
  }
  n = Serial.available();
  if (n) {
    if (n > sizeof(buf)) n = sizeof(buf);
    Serial.readBytes(buf, n);
    Gnss.write(buf, n);
  }
  delay(1);
}
