/*
  Programmable ECU — smoke test da TFT 3.5" 8-bit paralelo (#131)

  Arduino IDE 1.8:
  1. Biblioteca: Sketch > Incluir Biblioteca > Gerenciar Bibliotecas
     instale "LovyanGFX" (lovyan03)
  2. Placa: ESP32 Dev Module
  3. Porta: COM do CP2102 (nao use COM Bluetooth)
  4. Serial Monitor 115200

  Se o upload falhar com a tela ligada, D0 esta no GPIO12 (pino de boot):
  segura BOOT, clica Upload, solta BOOT quando começar a gravar.

  Se a tela ficar branca/lixo, no Serial avise: tentamos ILI9486;
  o shield 3.5 às vezes e ILI9488 — a gente troca o painel.
*/

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class EcuDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9486 _panel;
  lgfx::Bus_Parallel8 _bus;

public:
  EcuDisplay(void) {
    {
      auto cfg = _bus.config();
      cfg.freq_write = 16000000;
      cfg.pin_wr = 21;
      cfg.pin_rd = 22;
      cfg.pin_rs = 23;
      cfg.pin_d0 = 12;
      cfg.pin_d1 = 13;
      cfg.pin_d2 = 26;
      cfg.pin_d3 = 25;
      cfg.pin_d4 = 17;
      cfg.pin_d5 = 16;
      cfg.pin_d6 = 19;
      cfg.pin_d7 = 18;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = 27;
      cfg.pin_rst = 32;
      cfg.pin_busy = -1;
      cfg.memory_width = 320;
      cfg.memory_height = 480;
      cfg.panel_width = 320;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};

static EcuDisplay tft;
static unsigned long lastHeartbeatMs = 0;

void displayBegin() {
  Serial.println("[display] pinout D0=12 D1=13 D2=26 D3=25 D4=17 D5=16 D6=19 D7=18");
  Serial.println("[display] RST=32 CS=27 RS=23 WR=21 RD=22");
  Serial.println("[display] aviso: shield em 5V; ESP32 e 3.3V — se esquentar/resetar, pare");

  if (!tft.init()) {
    Serial.println("[display] init falhou (cheque fios e LovyanGFX)");
    return;
  }

  tft.setRotation(1);
  tft.setBrightness(255);
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, tft.width() / 3, tft.height(), TFT_RED);
  tft.fillRect(tft.width() / 3, 0, tft.width() / 3, tft.height(), TFT_GREEN);
  tft.fillRect((tft.width() / 3) * 2, 0, tft.width() / 3, tft.height(), TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(12, tft.height() / 2 - 8);
  tft.print("programmable-ecu");

  Serial.print("[display] init ok  ");
  Serial.print(tft.width());
  Serial.print("x");
  Serial.println(tft.height());
}

void displayTick() {
  const unsigned long now = millis();
  if (now - lastHeartbeatMs < 2000) {
    return;
  }
  lastHeartbeatMs = now;
  Serial.println("[display] tft viva");
}

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
