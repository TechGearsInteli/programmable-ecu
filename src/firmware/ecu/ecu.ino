/*
  Programmable ECU — esqueleto de firmware (ESP32, Arduino IDE)

  Feature 1.5 (nucleo de controle). Passo atual: smoke test da tela.
  Tasks 1.5.1.1+ (timers, CKP, sensores) ainda nao entram neste arquivo.

  Como abrir no Arduino IDE:
  1. Instale o suporte "esp32" (Boards Manager, Espressif)
  2. Arquivo > Abrir > esta pasta ecu/ecu.ino
  3. Selecione a placa ESP32 mais proxima da sua e a porta COM
  4. Upload e abra o Serial Monitor em 115200 baud
*/

#include "display.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("programmable-ecu firmware skeleton");
  displayBegin();
}

void loop() {
  displayTick();
}
