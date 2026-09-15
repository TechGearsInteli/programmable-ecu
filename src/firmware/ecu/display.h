#ifndef ECU_DISPLAY_H
#define ECU_DISPLAY_H

// Tela touch do modulo ESP32.
// A biblioteca e os pinos dependem do modelo da placa (ainda nao definido).
// Enquanto isso, o "display" so fala pelo Serial Monitor.

void displayBegin();
void displayTick();

#endif
