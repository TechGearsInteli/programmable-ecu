/*
  Programmable ECU — interpolacao bilinear do mapa de combustivel (#136)

  CKP e sensores continuam simulados. O mapa 2D (RPM x MAP) e consultado
  no loop, fora da ISR. A TFT redesenha a cada 400 ms de proposito:
  leitura/calculo primeiro, tela depois.

  Arduino IDE 2: LovyanGFX, ESP32 Dev Module, COM do CP2102, 115200.
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
static unsigned long engineStartMs = 0;
static bool tftReady = false;

struct EngineSensors {
  uint16_t mapKpa;
  uint8_t tpsPct;
  int16_t cltC;
  int16_t iatC;
  uint16_t lambdaX100;
};

static EngineSensors sensors = {35, 8, 18, 24, 100};
static uint32_t measuredRpm = 0;
static uint16_t fuelPwX100 = 0;
static uint8_t mapRpmCell = 0;
static uint8_t mapLoadCell = 0;

static const uint16_t kRpmAxis[] = {800, 1200, 1600, 2000, 2500, 3000, 3500, 4000};
static const uint16_t kMapAxis[] = {30, 40, 50, 60, 70, 80, 90, 100};
static const uint8_t kRpmBins = 8;
static const uint8_t kMapBins = 8;

// Tempo de injetor em centesimos de ms (850 = 8.50 ms).
static const uint16_t kFuelMap[8][8] = {
    {820, 910, 1000, 1140, 1280, 1410, 1520, 1600},
    {850, 950, 1080, 1230, 1390, 1520, 1640, 1710},
    {910, 1030, 1180, 1350, 1510, 1680, 1800, 1890},
    {980, 1120, 1290, 1470, 1660, 1840, 1980, 2070},
    {1060, 1230, 1420, 1630, 1840, 2040, 2200, 2300},
    {1150, 1340, 1550, 1790, 2020, 2250, 2430, 2540},
    {1230, 1450, 1680, 1940, 2200, 2450, 2650, 2770},
    {1300, 1540, 1790, 2080, 2360, 2630, 2850, 2990},
};

static uint8_t axisLowIndex(const uint16_t *axis, uint8_t n, uint16_t x) {
  if (x <= axis[0]) {
    return 0;
  }
  if (x >= axis[n - 1]) {
    return n - 2;
  }
  uint8_t i = 0;
  while (i + 1 < n && axis[i + 1] <= x) {
    i++;
  }
  if (i > n - 2) {
    i = n - 2;
  }
  return i;
}

static uint16_t lerpInt(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1, uint16_t x) {
  if (x1 == x0) {
    return y0;
  }
  if (x <= x0) {
    return y0;
  }
  if (x >= x1) {
    return y1;
  }
  return (uint16_t)(y0 + ((int32_t)(y1 - y0) * (int32_t)(x - x0)) / (int32_t)(x1 - x0));
}

static uint16_t lookupFuelPwX100(uint16_t rpm, uint16_t mapKpa) {
  const uint8_t i = axisLowIndex(kRpmAxis, kRpmBins, rpm);
  const uint8_t j = axisLowIndex(kMapAxis, kMapBins, mapKpa);
  mapRpmCell = i;
  mapLoadCell = j;

  const uint16_t rpm0 = kRpmAxis[i];
  const uint16_t rpm1 = kRpmAxis[i + 1];
  const uint16_t map0 = kMapAxis[j];
  const uint16_t map1 = kMapAxis[j + 1];

  const uint16_t v00 = kFuelMap[i][j];
  const uint16_t v10 = kFuelMap[i + 1][j];
  const uint16_t v01 = kFuelMap[i][j + 1];
  const uint16_t v11 = kFuelMap[i + 1][j + 1];

  const uint16_t v0 = lerpInt(rpm0, rpm1, v00, v10, rpm);
  const uint16_t v1 = lerpInt(rpm0, rpm1, v01, v11, rpm);
  return lerpInt(map0, map1, v0, v1, mapKpa);
}

static void updateMeasuredRpm() {
  uint32_t revolutionUs;
  noInterrupts();
  revolutionUs = revUs;
  interrupts();
  if (revolutionUs >= 10000 && revolutionUs <= 300000) {
    lastGoodRpm = 60000000UL / revolutionUs;
  }
  measuredRpm = lastGoodRpm;
}

static void sensorsSimulate(uint32_t rpm, SimPhase phase) {
  uint32_t tps = 8;
  if (phase == SIM_CRANK) {
    tps = 0;
  } else if (rpm > kIdleRpm) {
    tps = 8 + ((rpm - kIdleRpm) * 70UL) / (kRevRpm - kIdleRpm);
    if (tps > 100) {
      tps = 100;
    }
  }
  sensors.tpsPct = (uint8_t)tps;
  sensors.mapKpa = (uint16_t)(32 + (sensors.tpsPct * 66) / 100);

  const unsigned long warm = millis() - engineStartMs;
  if (warm >= 90000) {
    sensors.cltC = 88;
  } else {
    sensors.cltC = (int16_t)(18 + (70UL * warm) / 90000UL);
  }
  sensors.iatC = (int16_t)(24 + sensors.tpsPct / 20);

  if (phase == SIM_CRANK) {
    sensors.lambdaX100 = 120;
  } else if (phase == SIM_REV_UP) {
    sensors.lambdaX100 = 85;
  } else if (phase == SIM_REV_DOWN) {
    sensors.lambdaX100 = 108;
  } else {
    sensors.lambdaX100 = 100;
  }
}

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
  tft.print("ECU  mapa  #136");
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
  sensorsSimulate(commandedRpm, simPhase);
  updateMeasuredRpm();
  const uint16_t rpmForMap = measuredRpm > 0 ? (uint16_t)measuredRpm : (uint16_t)commandedRpm;
  fuelPwX100 = lookupFuelPwX100(rpmForMap, sensors.mapKpa);
}

void displayTick() {
  if (!tftReady) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastUiMs < 400) {
    return;
  }
  lastUiMs = now;

  uint8_t isSynced;
  uint8_t tooth;
  noInterrupts();
  isSynced = synced;
  tooth = toothIndex;
  interrupts();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.fillRect(12, 72, 460, 28, TFT_BLACK);
  tft.setCursor(12, 72);
  tft.printf("RPM %u  %s  %s", measuredRpm, phaseName(simPhase),
             isSynced ? "OK" : "--");

  const int barW = tft.width() - 24;
  const int filled = (int)((measuredRpm * (uint32_t)barW) / 4000UL);
  tft.fillRect(12, 104, barW, 14, TFT_DARKGREY);
  if (filled > 0) {
    tft.fillRect(12, 104, filled > barW ? barW : filled, 14, TFT_YELLOW);
  }

  tft.fillRect(12, 128, 460, 120, TFT_BLACK);
  tft.setCursor(12, 132);
  tft.printf("MAP %u  TPS %u  CLT %d", sensors.mapKpa, sensors.tpsPct, sensors.cltC);
  tft.setCursor(12, 168);
  tft.printf("PW  %u.%02u ms   cel %u,%u", fuelPwX100 / 100, fuelPwX100 % 100,
             mapRpmCell, mapLoadCell);
  tft.setCursor(12, 204);
  tft.printf("LAM %u.%02u  dente %u", sensors.lambdaX100 / 100,
             sensors.lambdaX100 % 100, tooth);

  if (now - lastSerialMs >= 2000) {
    lastSerialMs = now;
    Serial.printf("[map] RPM=%u MAP=%u PW=%u.%02u ms cel=%u,%u\n", measuredRpm,
                  sensors.mapKpa, fuelPwX100 / 100, fuelPwX100 % 100, mapRpmCell,
                  mapLoadCell);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("programmable-ecu #136 mapa bilinear");
  engineStartMs = millis();
  displayBegin();
  ckpBeginSimulated();
}

void loop() {
  simTick();
  displayTick();
}
