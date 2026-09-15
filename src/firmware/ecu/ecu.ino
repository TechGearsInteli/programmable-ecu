/*
  Programmable ECU — CKP / RPM simulado, roda 36-1 (#134)

  Perfil mais perto de um motor: partida (~280), pega ate lenta (~900)
  com oscilacao, depois uma acelerada e volta. Continua tudo no GPIO 4.

  Arduino IDE 2: LovyanGFX, ESP32 Dev Module, COM do CP2102, 115200.
  Upload com TFT: se falhar, segura BOOT (D0 = GPIO12).
*/

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

static const int kPulsePin = 4;
static const uint32_t kToothSlots = 36;
static const uint8_t kMissingSlot = 0;
static const uint32_t kIdleRpm = 900;
static const uint32_t kCrankRpm = 280;
static const uint32_t kRevRpm = 3500;

enum SimPhase : uint8_t {
  SIM_CRANK = 0,
  SIM_CATCH,
  SIM_IDLE,
  SIM_REV_UP,
  SIM_REV_DOWN
};

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
static hw_timer_t *toothTimer = nullptr;
static volatile uint8_t genSlot = 0;
static volatile uint8_t genHalf = 0;
static volatile uint32_t lastRiseUs = 0;
static volatile uint32_t lastToothDtUs = 0;
static volatile uint32_t lastGapUs = 0;
static volatile uint32_t revUs = 0;
static volatile uint32_t pulseCount = 0;
static volatile uint8_t toothIndex = 0;
static volatile uint8_t synced = 0;
static uint32_t commandedRpm = kCrankRpm;
static uint32_t lastGoodRpm = 0;
static SimPhase simPhase = SIM_CRANK;
static unsigned long phaseStartMs = 0;
static unsigned long lastSimMs = 0;
static unsigned long lastUiMs = 0;
static unsigned long lastSerialMs = 0;
static bool tftReady = false;

static const char *phaseName(SimPhase phase) {
  switch (phase) {
    case SIM_CRANK:
      return "partida";
    case SIM_CATCH:
      return "pega";
    case SIM_IDLE:
      return "lenta";
    case SIM_REV_UP:
      return "acelera";
    case SIM_REV_DOWN:
      return "solta";
    default:
      return "?";
  }
}

void IRAM_ATTR onToothTimer() {
  if (genHalf == 0) {
    if (genSlot != kMissingSlot) {
      digitalWrite(kPulsePin, HIGH);
    }
    genHalf = 1;
    return;
  }

  digitalWrite(kPulsePin, LOW);
  genHalf = 0;
  genSlot++;
  if (genSlot >= kToothSlots) {
    genSlot = 0;
  }
}

void IRAM_ATTR onCkpRise() {
  const uint32_t now = micros();
  const uint32_t dt = now - lastRiseUs;
  lastRiseUs = now;
  pulseCount++;

  if (lastToothDtUs > 80 && dt > lastToothDtUs + (lastToothDtUs >> 1)) {
    synced = 1;
    toothIndex = 0;
    if (lastGapUs != 0) {
      revUs = now - lastGapUs;
    }
    lastGapUs = now;
    return;
  }

  if (dt > 80) {
    lastToothDtUs = dt;
    if (toothIndex < 254) {
      toothIndex++;
    }
  }
}

static uint32_t clampRpm(uint32_t rpm) {
  if (rpm < 200) {
    return 200;
  }
  if (rpm > 6000) {
    return 6000;
  }
  return rpm;
}

static void simApplyRpm(uint32_t rpm) {
  rpm = clampRpm(rpm);
  commandedRpm = rpm;
  if (toothTimer == nullptr) {
    return;
  }
  uint32_t halfUs = 60000000UL / (rpm * kToothSlots * 2UL);
  if (halfUs < 20) {
    halfUs = 20;
  }
  timerAlarm(toothTimer, halfUs, true, 0);
}

static uint32_t lerpU32(uint32_t a, uint32_t b, unsigned long elapsed, unsigned long span) {
  if (elapsed >= span) {
    return b;
  }
  if (a <= b) {
    return a + ((b - a) * elapsed) / span;
  }
  return a - ((a - b) * elapsed) / span;
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
  tft.setCursor(12, 12);
  tft.print("ECU  CKP sim  #134");
  tft.setCursor(12, 44);
  tft.printf("roda  36-1   GPIO %d", kPulsePin);
  Serial.println("[display] init ok");
}

void ckpBeginSimulated() {
  pinMode(kPulsePin, OUTPUT);
  digitalWrite(kPulsePin, LOW);

  toothTimer = timerBegin(1000000);
  if (toothTimer == nullptr) {
    Serial.println("[ckp] timerBegin falhou");
    return;
  }
  timerAttachInterrupt(toothTimer, &onToothTimer);
  simPhase = SIM_CRANK;
  phaseStartMs = millis();
  simApplyRpm(kCrankRpm);
  attachInterrupt(kPulsePin, onCkpRise, RISING);
  Serial.println("[ckp] perfil: partida -> lenta -> acelerada (ciclo)");
}

void simTick() {
  const unsigned long now = millis();
  if (now - lastSimMs < 20) {
    return;
  }
  lastSimMs = now;

  const unsigned long t = now - phaseStartMs;
  uint32_t rpm = commandedRpm;

  switch (simPhase) {
    case SIM_CRANK:
      rpm = kCrankRpm;
      if (t > 2500) {
        simPhase = SIM_CATCH;
        phaseStartMs = now;
      }
      break;
    case SIM_CATCH:
      rpm = lerpU32(kCrankRpm, kIdleRpm, t, 700);
      if (t >= 700) {
        simPhase = SIM_IDLE;
        phaseStartMs = now;
      }
      break;
    case SIM_IDLE: {
      const unsigned long cyc = (now / 30) % 80;
      const uint32_t wander = cyc < 40 ? cyc : (80 - cyc);
      rpm = 880 + wander;
      if (t > 10000) {
        simPhase = SIM_REV_UP;
        phaseStartMs = now;
      }
      break;
    }
    case SIM_REV_UP:
      rpm = lerpU32(kIdleRpm, kRevRpm, t, 1500);
      if (t >= 1500) {
        simPhase = SIM_REV_DOWN;
        phaseStartMs = now;
      }
      break;
    case SIM_REV_DOWN:
      rpm = lerpU32(kRevRpm, kIdleRpm, t, 1800);
      if (t >= 1800) {
        simPhase = SIM_IDLE;
        phaseStartMs = now;
      }
      break;
  }

  if (rpm != commandedRpm) {
    simApplyRpm(rpm);
  }
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

  uint32_t counted;
  uint32_t revolutionUs;
  uint8_t isSynced;
  uint8_t tooth;
  noInterrupts();
  counted = pulseCount;
  revolutionUs = revUs;
  isSynced = synced;
  tooth = toothIndex;
  interrupts();

  uint32_t measuredRpm = lastGoodRpm;
  if (revolutionUs >= 10000 && revolutionUs <= 300000) {
    lastGoodRpm = 60000000UL / revolutionUs;
    measuredRpm = lastGoodRpm;
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.fillRect(12, 76, 450, 28, TFT_BLACK);
  tft.setCursor(12, 76);
  tft.printf("modo  %s", phaseName(simPhase));

  tft.setTextSize(3);
  tft.fillRect(12, 112, 360, 40, TFT_BLACK);
  tft.setCursor(12, 112);
  tft.printf("RPM  %u", measuredRpm);

  const int barW = tft.width() - 24;
  const int filled = (int)((measuredRpm * (uint32_t)barW) / 4000UL);
  tft.fillRect(12, 156, barW, 18, TFT_DARKGREY);
  if (filled > 0) {
    tft.fillRect(12, 156, filled > barW ? barW : filled, 18, TFT_YELLOW);
  }

  tft.setTextSize(2);
  tft.fillRect(12, 184, 450, 72, TFT_BLACK);
  tft.setCursor(12, 184);
  tft.printf("alvo %u   sync %s", commandedRpm, isSynced ? "SIM" : "NAO");
  tft.setCursor(12, 216);
  tft.printf("dente %u   pulsos %u", tooth, counted);

  if (now - lastSerialMs >= 2000) {
    lastSerialMs = now;
    Serial.printf("[ckp] modo=%s  alvo=%u  medido=%u  sync=%u\n",
                  phaseName(simPhase), commandedRpm, measuredRpm, isSynced);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("programmable-ecu #134 CKP perfil de motor");
  displayBegin();
  ckpBeginSimulated();
}

void loop() {
  simTick();
  displayTick();
}
