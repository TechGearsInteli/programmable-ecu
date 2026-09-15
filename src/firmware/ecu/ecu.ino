/*
  Programmable ECU — timer LEDC, pulso 1.00 ms (#133)

  A TFT mostra o valor programado, um contador de bordas no GPIO 4
  e um quadrado lento (olho humano). Sem LED externo.
  O pulso de 1 ms sai no GPIO 4 mesmo sem nada conectado.

  Arduino IDE 1.8: LovyanGFX, ESP32 Dev Module, COM do CP2102, 115200.
  Upload com TFT: se falhar, segura BOOT (D0 = GPIO12).
*/

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

static const int kPulsePin = 4;
static const uint32_t kPwmHz = 500;
static const uint8_t kPwmBits = 8;
static const uint32_t kPwmDuty = 128;
static const float kPulseMs = 1.00f;

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
static volatile uint32_t pulseCount = 0;
static unsigned long lastUiMs = 0;
static unsigned long lastSerialMs = 0;
static uint32_t lastShownCount = 0;
static bool lastBlinkOn = false;
static bool tftReady = false;

void IRAM_ATTR onPulseRise() {
  pulseCount++;
}

void displayBegin() {
  if (!tft.init()) {
    Serial.println("[display] init falhou");
    return;
  }

  tftReady = true;
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(12, 16);
  tft.print("ECU  timer LEDC  #133");
  tft.setCursor(12, 56);
  tft.printf("pulso  %.2f ms", kPulseMs);
  tft.setCursor(12, 88);
  tft.printf("GPIO %d  %u Hz  50%%", kPulsePin, kPwmHz);
  tft.setCursor(12, 128);
  tft.print("pulsos  0");
  tft.drawRect(tft.width() - 80, 20, 56, 56, TFT_WHITE);

  Serial.println("[display] init ok");
}

void timerBeginPulse() {
  if (!ledcAttach(kPulsePin, kPwmHz, kPwmBits)) {
    Serial.println("[timer] ledcAttach falhou");
    return;
  }
  ledcWrite(kPulsePin, kPwmDuty);
  attachInterrupt(kPulsePin, onPulseRise, RISING);
  Serial.println("[timer] LEDC 1.00 ms no GPIO 4 (sem LED, so o pino)");
}

void displayTick() {
  if (!tftReady) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastUiMs < 200) {
    return;
  }
  lastUiMs = now;

  uint32_t count;
  noInterrupts();
  count = pulseCount;
  interrupts();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.fillRect(12, 128, 280, 28, TFT_BLACK);
  tft.setCursor(12, 128);
  tft.printf("pulsos  %u", count);

  const bool blinkOn = ((count / 250) & 1) != 0;
  if (blinkOn != lastBlinkOn || count != lastShownCount) {
    lastBlinkOn = blinkOn;
    tft.fillRect(tft.width() - 76, 24, 48, 48, blinkOn ? TFT_YELLOW : TFT_BLACK);
    tft.drawRect(tft.width() - 80, 20, 56, 56, TFT_WHITE);
  }
  lastShownCount = count;

  if (now - lastSerialMs >= 2000) {
    lastSerialMs = now;
    Serial.printf("[timer] pulso=%.2f ms  GPIO=%d  pulsos=%u\n", kPulseMs, kPulsePin, count);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("programmable-ecu #133 timers");
  displayBegin();
  timerBeginPulse();
}

void loop() {
  displayTick();
}
