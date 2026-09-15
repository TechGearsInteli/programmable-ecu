#include "display.h"

#include <Arduino.h>

static unsigned long lastHeartbeatMs = 0;

void displayBegin() {
  Serial.println("[display] stub ativo: placa/touch ainda nao configurados");
  Serial.println("[display] quando tiver o modulo, anote o modelo na traseira");
  Serial.println("[display] (ex.: ESP32-2432S028, ILI9341, ST7789, etc.)");
}

void displayTick() {
  const unsigned long now = millis();
  if (now - lastHeartbeatMs < 2000) {
    return;
  }
  lastHeartbeatMs = now;
  Serial.println("[display] aguardando driver da tela");
}
