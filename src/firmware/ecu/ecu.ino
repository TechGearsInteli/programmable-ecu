/*
  Programmable ECU — testes automáticos no boot por tecla (!) (#142)

  Boot: carrega mapa com checksum; se falhar, usa o padrao.
  Serial: T = altera cel 0,0 | S = grava | D = restaura padrao.
          ! = roda self-tests e trava no relatorio.
  Reset depois de S deve manter o valor. Nao grava na ISR.

  Arduino IDE 2: LovyanGFX, ESP32 Dev Module, COM do CP2102, 115200.
*/

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <Preferences.h>
#include <esp_task_wdt.h>

static const int kPulsePin = 4;
static const int kInjPins[4] = {2, 14, 15, 33};
static const uint32_t kToothSlots = 36;
static const uint8_t kMissingSlot = 0;
static const uint32_t kIdleRpm = 900;
static const uint32_t kCrankRpm = 280;
static const uint32_t kRevRpm = 3500;
static const uint32_t kRpmLimit = 3000;
static const int16_t kCltLimitC = 105;

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
static hw_timer_t *injOffTimer = nullptr;
static volatile uint8_t genSlot = 0;
static volatile uint8_t genHalf = 0;
static volatile uint32_t lastRiseUs = 0;
static volatile uint32_t lastToothDtUs = 0;
static volatile uint32_t lastGapUs = 0;
static volatile uint32_t revUs = 0;
static volatile uint32_t pulseCount = 0;
static volatile uint8_t toothIndex = 0;
static volatile uint8_t synced = 0;
static volatile uint32_t lastSyncUs = 0;
static volatile uint8_t triggerErrorCount = 0;
static volatile uint32_t lastValidDtUs = 0;
static volatile uint8_t camRev = 0;
static volatile uint32_t injPwUs = 8000;

// Parametros de decodificacao robusta do trigger wheel.
static const uint16_t kGapRatioMinX10 = 15; // 1.5x
static const uint16_t kGapRatioMaxX10 = 30; // 3.0x
static const uint32_t kTriggerFilterMinDtUs = 80;
static const uint32_t kSyncLossTimeoutUs = 500000; // 500 ms
static const uint8_t kMaxTriggerErrors = 3;
static volatile uint8_t injBusy[4];
static volatile uint32_t injOffUs[4];
static volatile uint32_t injCount[4];
static volatile uint8_t injUiMask = 0;
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
static uint16_t fuelPwFinalX100 = 0;
static uint16_t corrCltX1000 = 1000;
static uint16_t corrIatX1000 = 1000;
static uint16_t corrAccelX1000 = 1000;
static uint16_t corrCutX1000 = 1000;
static uint16_t corrLamX1000 = 1000;

// Chaves para habilitar/desabilitar cada correcao individualmente.
// Permitem testar cada fator de forma isolada e desligar no limp mode.
static bool enableCorrClt = true;
static bool enableCorrIat = true;
static bool enableCorrAccel = true;
static bool enableCorrCut = true;
static bool enableCorrLam = true;

// ---------- Niveis de erro e watchdog ----------
// Separar erros em camadas evita reiniciar o MCU por falha leve.
enum EcuErrorLevel : uint8_t {
  ERR_NONE = 0,
  ERR_WARNING,      // recuperavel: sensor fora da faixa, valor default usado
  ERR_CONFIG,       // erro de calibracao/tune
  ERR_FIRMWARE,     // bug detectado: funcao desativada, loga erro
  ERR_CRITICAL      // estado inseguro: corte total ou reset
};

static EcuErrorLevel ecuErrorLevel = ERR_NONE;
static uint32_t ecuErrorCode = 0;      // codigo da ultima falha
static unsigned long ecuErrorSince = 0; // millis da primeira ocorrencia
static bool ecuCriticalShutdown = false; // true = corte geral de atuadores

static const uint32_t kWatchdogTimeoutS = 5;
static bool watchdogEnabled = false;

static uint8_t protRpm = 0;
static uint8_t protClt = 0;
static unsigned long lastLamMs = 0;
static uint8_t lastTpsPct = 0;
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

static uint16_t fuelMap[8][8];
static uint8_t mapFromNvs = 0;
static Preferences ecuPrefs;

static uint16_t mapChecksum(const uint16_t map[8][8]) {
  uint32_t s = 0xEC01;
  for (uint8_t i = 0; i < 8; i++) {
    for (uint8_t j = 0; j < 8; j++) {
      s += map[i][j];
      s ^= (uint32_t)map[i][j] << ((i + j) & 7);
    }
  }
  return (uint16_t)(s ^ (s >> 16));
}

static void mapLoadDefault() {
  memcpy(fuelMap, kFuelMap, sizeof(fuelMap));
  mapFromNvs = 0;
}

static bool mapSaveNvs() {
  const uint16_t crc = mapChecksum(fuelMap);
  if (!ecuPrefs.begin("ecu", false)) {
    Serial.println("[nvs] begin falhou");
    return false;
  }
  bool ok = true;
  if (ecuPrefs.putUShort("magic", 0xEC01) != 0xEC01) ok = false;
  if (ecuPrefs.putUShort("crc", crc) != crc) ok = false;
  if (ecuPrefs.putBytes("map", fuelMap, sizeof(fuelMap)) != sizeof(fuelMap)) ok = false;
  ecuPrefs.end();
  if (!ok) {
    Serial.println("[nvs] gravacao incompleta");
    return false;
  }
  mapFromNvs = 1;
  Serial.printf("[nvs] gravado crc=%u cel00=%u\n", crc, fuelMap[0][0]);
  return true;
}

static void mapLoadNvs() {
  mapLoadDefault();
  ecuPrefs.begin("ecu", true);
  const uint16_t magic = ecuPrefs.getUShort("magic", 0);
  const uint16_t crc = ecuPrefs.getUShort("crc", 0);
  uint16_t loaded[8][8];
  const size_t n = ecuPrefs.getBytes("map", loaded, sizeof(loaded));
  ecuPrefs.end();
  if (magic != 0xEC01 || n != sizeof(loaded) || mapChecksum(loaded) != crc) {
    Serial.println("[nvs] padrao (vazio ou checksum ruim)");
    return;
  }
  memcpy(fuelMap, loaded, sizeof(fuelMap));
  mapFromNvs = 1;
  Serial.printf("[nvs] ok crc=%u cel00=%u\n", crc, fuelMap[0][0]);
}

static void mapPollSerial() {
  if (!Serial.available()) {
    return;
  }
  const char c = (char)Serial.read();
  if (c == '!') {
    runSelfTests();
    while (true) { delay(100); }
  } else if (c == 'T' || c == 't') {
    fuelMap[0][0] = (uint16_t)(fuelMap[0][0] + 50);
    Serial.printf("[nvs] teste cel00=%u  (envie S e reset)\n", fuelMap[0][0]);
  } else if (c == 'S' || c == 's') {
    mapSaveNvs();
  } else if (c == 'D' || c == 'd') {
    mapLoadDefault();
    mapSaveNvs();
    Serial.println("[nvs] restaurado padrao");
  }
}

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

  const uint16_t v00 = fuelMap[i][j];
  const uint16_t v10 = fuelMap[i + 1][j];
  const uint16_t v01 = fuelMap[i][j + 1];
  const uint16_t v11 = fuelMap[i + 1][j + 1];

  const uint16_t v0 = lerpInt(rpm0, rpm1, v00, v10, rpm);
  const uint16_t v1 = lerpInt(rpm0, rpm1, v01, v11, rpm);
  return lerpInt(map0, map1, v0, v1, mapKpa);
}

static uint16_t applyFuelCorrections(uint16_t pwMapX100, SimPhase phase) {
  if (enableCorrClt) {
    if (sensors.cltC >= 80) {
      corrCltX1000 = 1000;
    } else if (sensors.cltC <= 20) {
      corrCltX1000 = 1400;
    } else {
      corrCltX1000 = (uint16_t)(1400 - ((int32_t)(sensors.cltC - 20) * 400) / 60);
    }
  } else {
    corrCltX1000 = 1000;
  }

  if (enableCorrIat) {
    if (sensors.iatC <= 20) {
      corrIatX1000 = 1020;
    } else if (sensors.iatC >= 50) {
      corrIatX1000 = 970;
    } else {
      corrIatX1000 = (uint16_t)(1020 - ((int32_t)(sensors.iatC - 20) * 50) / 30);
    }
  } else {
    corrIatX1000 = 1000;
  }

  if (enableCorrAccel) {
    if (sensors.tpsPct > lastTpsPct + 4) {
      corrAccelX1000 = 1180;
    } else if (corrAccelX1000 > 1000) {
      corrAccelX1000 = (uint16_t)(corrAccelX1000 - 15);
      if (corrAccelX1000 < 1000) {
        corrAccelX1000 = 1000;
      }
    }
  } else {
    corrAccelX1000 = 1000;
  }
  lastTpsPct = sensors.tpsPct;

  if (enableCorrCut) {
    if (phase == SIM_REV_DOWN) {
      corrCutX1000 = 800;
    } else {
      corrCutX1000 = 1000;
    }
  } else {
    corrCutX1000 = 1000;
  }

  uint32_t v = pwMapX100;
  if (enableCorrClt)   v = (v * corrCltX1000) / 1000;
  if (enableCorrIat)   v = (v * corrIatX1000) / 1000;
  if (enableCorrAccel) v = (v * corrAccelX1000) / 1000;
  if (enableCorrCut)   v = (v * corrCutX1000) / 1000;
  if (enableCorrLam)   v = (v * corrLamX1000) / 1000;
  if (v > 65535) {
    v = 65535;
  }
  return (uint16_t)v;
}

static uint16_t applyProtections(uint16_t pw) {
  protRpm = (measuredRpm >= kRpmLimit) ? 1 : 0;
  protClt = (sensors.cltC >= kCltLimitC) ? 1 : 0;
  if (protRpm || protClt) {
    return 0;
  }
  return pw;
}

// ---------- Niveis de erro ----------
// Registra uma falha. Niveis mais graves sobrescrevem os mais leves.
// Acoes imediatas:
//   WARNING    -> usa valor default, continua funcionando
//   CONFIG     -> usa mapa seguro, sinaliza interface
//   FIRMWARE   -> desliga funcionalidade afetada, loga
//   CRITICAL   -> corte total de atuadores (ou watchdog reset)
static void setEcuError(EcuErrorLevel level, uint32_t code) {
  if (level < ecuErrorLevel) {
    return;
  }
  ecuErrorLevel = level;
  ecuErrorCode = code;
  ecuErrorSince = millis();
  if (level == ERR_CRITICAL) {
    ecuCriticalShutdown = true;
  }
}

static void clearEcuError() {
  ecuErrorLevel = ERR_NONE;
  ecuErrorCode = 0;
  ecuErrorSince = 0;
  ecuCriticalShutdown = false;
}

static const char *errorLevelName() {
  switch (ecuErrorLevel) {
    case ERR_WARNING:   return "WARN";
    case ERR_CONFIG:    return "CONFIG";
    case ERR_FIRMWARE:  return "FIRM";
    case ERR_CRITICAL:  return "CRIT";
    default:            return "OK";
  }
}

// ---------- Watchdog ----------
static void watchdogInit() {
  // WDT de 5 segundos. Se o loop principal travar, o ESP32 reinicia.
  // Em hardware real tambem recomenda-se um watchdog externo independente.
  if (esp_task_wdt_init(kWatchdogTimeoutS, true) == ESP_OK) {
    if (esp_task_wdt_add(NULL) == ESP_OK) {
      watchdogEnabled = true;
      Serial.println("[wdt] watchdog ativo (5s)");
      return;
    }
  }
  Serial.println("[wdt] watchdog init falhou");
}

static void watchdogReset() {
  if (watchdogEnabled) {
    esp_task_wdt_reset();
  }
}

static void updateLambdaClosedLoop(SimPhase phase) {
  const unsigned long now = millis();
  if (now - lastLamMs < 80) {
    return;
  }
  lastLamMs = now;

  // Closed-loop lambda so atua no marcha-lenta quente com borboleta fechada.
  const bool active = (phase == SIM_IDLE && sensors.cltC >= 40 && sensors.tpsPct <= 8);
  if (!active) {
    // Volta suavemente a correcao para neutro quando sai da malha fechada.
    if (corrLamX1000 > 1000) {
      corrLamX1000--;
    } else if (corrLamX1000 < 1000) {
      corrLamX1000++;
    }
    return;
  }

  if (sensors.lambdaX100 > 102 && corrLamX1000 < 1150) {
    corrLamX1000++;
  } else if (sensors.lambdaX100 < 98 && corrLamX1000 > 850) {
    corrLamX1000--;
  }
}

static void updateMeasuredRpm() {
  const uint32_t now = micros();
  uint32_t revolutionUs;
  uint32_t lastRise;
  uint8_t sync;
  uint32_t lastSync;
  noInterrupts();
  revolutionUs = revUs;
  lastRise = lastRiseUs;
  sync = synced;
  lastSync = lastSyncUs;
  interrupts();

  // Se nao houver pulso CKP por 500 ms, considera motor parado.
  if ((now - lastRise) > 500000UL) {
    lastGoodRpm = 0;
    measuredRpm = 0;
    if (sync) {
      setEcuError(ERR_WARNING, 101); // CKP timeout
      sync = 0;
    }
    return;
  }

  // Se perdeu sincronismo por muito tempo, reporta warning e zera sync.
  if (sync && (now - lastSync) > kSyncLossTimeoutUs) {
    setEcuError(ERR_WARNING, 102); // sync loss timeout
    noInterrupts();
    synced = 0;
    sync = 0;
    interrupts();
  }

  if (sync && revolutionUs >= 10000 && revolutionUs <= 300000) {
    lastGoodRpm = 60000000UL / revolutionUs;
  } else if (!sync) {
    lastGoodRpm = 0;
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

  if (phase == SIM_IDLE) {
    const unsigned long idleT = millis() - phaseStartMs;
    if (idleT > 7000 && idleT < 9000) {
      sensors.cltC = 110;
    }
  }

  if (phase == SIM_CRANK) {
    sensors.lambdaX100 = 120;
  } else if (phase == SIM_REV_UP) {
    sensors.lambdaX100 = 85;
  } else if (phase == SIM_REV_DOWN) {
    sensors.lambdaX100 = 108;
  } else {
    sensors.lambdaX100 = 104;
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

void IRAM_ATTR scheduleInj(uint8_t ch) {
  uint32_t pw = injPwUs;
  if (pw < 400) {
    return;
  }
  digitalWrite(kInjPins[ch], HIGH);
  injBusy[ch] = 1;
  injOffUs[ch] = micros() + pw;
  injCount[ch]++;
  injUiMask |= (uint8_t)(1 << ch);
}

void IRAM_ATTR onInjOffTimer() {
  const uint32_t now = micros();
  for (uint8_t i = 0; i < 4; i++) {
    if (injBusy[i] && (int32_t)(now - injOffUs[i]) >= 0) {
      digitalWrite(kInjPins[i], LOW);
      injBusy[i] = 0;
    }
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

  // Trigger filter: rejeita pulsos muito proximos (ruido/ringing).
  if (dt < kTriggerFilterMinDtUs) {
    return;
  }
  lastRiseUs = now;
  pulseCount++;

  // Deteccao de gap por razao entre intervalos. Para 36-1 o gap vale
  // aproximadamente 2 dentes, entao espera-se ratio entre 1.5x e 3.0x.
  bool isGap = false;
  if (lastValidDtUs > 0) {
    const uint32_t ratioX10 = (dt * 10UL) / lastValidDtUs;
    isGap = (ratioX10 >= kGapRatioMinX10 && ratioX10 <= kGapRatioMaxX10);
  }

  if (isGap) {
    synced = 1;
    lastSyncUs = now;
    triggerErrorCount = 0;
    toothIndex = 0;
    if (lastGapUs != 0) {
      revUs = now - lastGapUs;
    }
    lastGapUs = now;
    if (camRev == 0) {
      scheduleInj(0);
    } else {
      scheduleInj(2);
    }
    // Estima duracao de um dente normal apos o gap (dt / ratio minimo).
    lastValidDtUs = (dt * 10UL) / kGapRatioMinX10;
    if (lastValidDtUs == 0) {
      lastValidDtUs = 1;
    }
    return;
  }

  lastValidDtUs = dt;
  if (toothIndex < 254) {
    toothIndex++;
  }
  if (toothIndex == 18) {
    if (camRev == 0) {
      scheduleInj(1);
    } else {
      scheduleInj(3);
    }
    camRev ^= 1;
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
  tft.print("ECU  NVS mapa  #141");
  tft.setCursor(12, 44);
  tft.printf("roda  36-1   GPIO %d", kPulsePin);
  Serial.println("[display] init ok");
}

void ckpBeginSimulated() {
  pinMode(kPulsePin, OUTPUT);
  digitalWrite(kPulsePin, LOW);
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(kInjPins[i], OUTPUT);
    digitalWrite(kInjPins[i], LOW);
    injBusy[i] = 0;
    injCount[i] = 0;
  }

  toothTimer = timerBegin(1000000);
  if (toothTimer == nullptr) {
    Serial.println("[ckp] timerBegin falhou");
    return;
  }
  timerAttachInterrupt(toothTimer, &onToothTimer);
  injOffTimer = timerBegin(1000000);
  if (injOffTimer != nullptr) {
    timerAttachInterrupt(injOffTimer, &onInjOffTimer);
    timerAlarm(injOffTimer, 50, true, 0);
  }
  simPhase = SIM_CRANK;
  phaseStartMs = millis();
  simApplyRpm(kCrankRpm);
  attachInterrupt(kPulsePin, onCkpRise, RISING);
  Serial.println("[inj] GPIO 2,14,15,33 ordem 1-3-4-2  (sem bico real)");
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
  updateLambdaClosedLoop(simPhase);
  updateMeasuredRpm();
  const uint16_t rpmForMap = measuredRpm > 0 ? (uint16_t)measuredRpm : (uint16_t)commandedRpm;
  fuelPwX100 = lookupFuelPwX100(rpmForMap, sensors.mapKpa);
  fuelPwFinalX100 = applyProtections(applyFuelCorrections(fuelPwX100, simPhase));
  injPwUs = (uint32_t)fuelPwFinalX100 * 10UL;

  // Protecao: PW nao pode ocupar quase uma revolucao inteira.
  const uint32_t maxPwUs = (measuredRpm > 100) ? (48000000UL / measuredRpm) : 30000UL;
  if (injPwUs > maxPwUs) {
    injPwUs = maxPwUs;
  }
  // PW menor que ~0.4 ms e desprezavel; evita pulso invalido na ISR.
  if (injPwUs < 400UL) {
    injPwUs = 0;
  }
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
  uint8_t uiMask;
  uint32_t c0, c1, c2, c3;
  noInterrupts();
  isSynced = synced;
  uiMask = injUiMask;
  injUiMask = 0;
  c0 = injCount[0];
  c1 = injCount[1];
  c2 = injCount[2];
  c3 = injCount[3];
  interrupts();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.fillRect(12, 72, 460, 28, TFT_BLACK);
  tft.setCursor(12, 72);
  const char *status = protRpm ? "LIM" : (protClt ? "HOT" : (isSynced ? "OK" : "--"));
  if (ecuErrorLevel != ERR_NONE) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
  }
  tft.printf("RPM %u  %s  %s  %s", measuredRpm, phaseName(simPhase), status, errorLevelName());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  const int barW = tft.width() - 24;
  const int filled = (int)((measuredRpm * (uint32_t)barW) / 4000UL);
  tft.fillRect(12, 104, barW, 14, TFT_DARKGREY);
  if (filled > 0) {
    tft.fillRect(12, 104, filled > barW ? barW : filled, 14, TFT_YELLOW);
  }

  tft.fillRect(12, 128, 460, 100, TFT_BLACK);
  tft.setCursor(12, 128);
  tft.printf("PWf %u.%02u  LAM %u.%02u", fuelPwFinalX100 / 100,
             fuelPwFinalX100 % 100, sensors.lambdaX100 / 100,
             sensors.lambdaX100 % 100);
  tft.setCursor(12, 156);
  tft.printf("xLAM %u.%02u  n %u %u %u %u", corrLamX1000 / 1000,
             (corrLamX1000 % 1000) / 10, c0, c1, c2, c3);

  const int box = 52;
  const int yb = 192;
  const char *lab[4] = {"1", "3", "4", "2"};
  for (int i = 0; i < 4; i++) {
    const int x = 12 + i * 70;
    const bool on = (uiMask & (1 << i)) != 0;
    tft.fillRect(x, yb, box, box, on ? TFT_YELLOW : TFT_DARKGREY);
    tft.setTextColor(TFT_BLACK, on ? TFT_YELLOW : TFT_DARKGREY);
    tft.setCursor(x + 18, yb + 16);
    tft.print(lab[i]);
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  if (now - lastSerialMs >= 2000) {
    lastSerialMs = now;
    Serial.printf("[nvs] src=%s cel00=%u rpm=%u PWf=%u.%02u err=%s/%u sync=%u\n",
                  mapFromNvs ? "flash" : "padrao", fuelMap[0][0], measuredRpm,
                  fuelPwFinalX100 / 100, fuelPwFinalX100 % 100,
                  errorLevelName(), ecuErrorCode, (unsigned)synced);
  }
}

// ---------- self-tests (#142) ----------
// Roda no boot se '!' for pressionado nos primeiros 1,5 s, ou a qualquer
// momento no loop. Apos os testes o firmware trava no relatorio.
#define TEST_ASSERT(cond, name) do { \
  total++; \
  if (cond) { pass++; Serial.printf("[test] PASS %s\n", name); } \
  else { fail++; Serial.printf("[test] FAIL %s  (%s:%d)\n", name, __FILE__, __LINE__); } \
} while (0)

static void runSelfTests() {
  uint16_t pass = 0, fail = 0, total = 0;
  Serial.println("\n[test] === SELF-TEST START ===");

  // Backup do estado para nao contaminar a simulacao depois.
  EngineSensors sensorsBackup = sensors;
  uint32_t measuredRpmBackup = measuredRpm;
  uint16_t corrLamBackup = corrLamX1000;
  uint16_t fuelMapBackup[8][8];
  memcpy(fuelMapBackup, fuelMap, sizeof(fuelMapBackup));

  // 1. Interpolacao bilinear no mapa de combustivel.
  mapLoadDefault();
  TEST_ASSERT(lookupFuelPwX100(800, 30) == fuelMap[0][0],
              "mapa canto inferior rpm/map");
  TEST_ASSERT(lookupFuelPwX100(4000, 100) == fuelMap[7][7],
              "mapa canto superior rpm/map");
  TEST_ASSERT(lookupFuelPwX100(1200, 40) == fuelMap[1][1],
              "mapa ponto de grade exato");
  const uint16_t pInterp = lookupFuelPwX100(1000, 35);
  TEST_ASSERT(pInterp >= fuelMap[0][0] && pInterp <= fuelMap[1][1],
              "mapa interpolado dentro da caixa");
  TEST_ASSERT(pInterp > fuelMap[0][0] && pInterp < fuelMap[1][1],
              "mapa interpolado nao eh extremo");
  TEST_ASSERT(lookupFuelPwX100(500, 25) == fuelMap[0][0],
              "mapa clamp abaixo");
  TEST_ASSERT(lookupFuelPwX100(5000, 110) == fuelMap[7][7],
              "mapa clamp acima");

  // 2. Correcoes de combustivel.
  sensors.cltC = 10;
  sensors.iatC = 90;
  sensors.tpsPct = 10;
  lastTpsPct = 10;
  corrAccelX1000 = 1000;
  corrCutX1000 = 1000;
  corrLamX1000 = 1000;
  const uint16_t pwBase = 1000;

  const uint16_t pwCold = applyFuelCorrections(pwBase, SIM_IDLE);
  const uint16_t expectedCold =
      (uint16_t)(((uint32_t)pwBase * 1400UL * 970UL) / 1000000UL);
  TEST_ASSERT(pwCold == expectedCold, "correcao CLT frio + IAT quente");

  sensors.cltC = 90;
  sensors.iatC = 10;
  const uint16_t pwWarm = applyFuelCorrections(pwBase, SIM_IDLE);
  const uint16_t expectedWarm =
      (uint16_t)(((uint32_t)pwBase * 1000UL * 1020UL) / 1000000UL);
  TEST_ASSERT(pwWarm == expectedWarm, "correcao CLT quente + IAT frio");

  sensors.tpsPct = 20;
  const uint16_t pwAccel = applyFuelCorrections(pwBase, SIM_IDLE);
  TEST_ASSERT(corrAccelX1000 == 1180, "deteccao de aceleracao");
  TEST_ASSERT(pwAccel > expectedWarm, "enriquecimento de aceleracao");

  const uint16_t pwCut = applyFuelCorrections(pwBase, SIM_REV_DOWN);
  TEST_ASSERT(pwCut < expectedWarm, "corte na desaceleracao");

  // Teste de correcoes isoladas com chaves de habilitacao.
  sensors.cltC = 10;
  sensors.iatC = 90;
  sensors.tpsPct = 10;
  lastTpsPct = 10;
  corrAccelX1000 = 1000;
  corrCutX1000 = 1000;
  enableCorrIat = false;
  enableCorrAccel = false;
  enableCorrCut = false;
  const uint16_t pwCltOnly = applyFuelCorrections(pwBase, SIM_IDLE);
  TEST_ASSERT(pwCltOnly == (uint16_t)((uint32_t)pwBase * 1400 / 1000),
              "correcao CLT isolada");
  enableCorrIat = true;
  enableCorrAccel = true;
  enableCorrCut = true;

  // 3. Protecoes do motor. Limite CLT = 105C (ativa em >= 105).
  measuredRpm = 3000;
  sensors.cltC = 80;
  TEST_ASSERT(applyProtections(1000) == 0, "protecao RPM limite");
  measuredRpm = 1000;
  sensors.cltC = 104;
  TEST_ASSERT(applyProtections(1000) == 1000, "protecao CLT abaixo do limite");
  sensors.cltC = 106;
  TEST_ASSERT(applyProtections(1000) == 0, "protecao CLT acima do limite");
  sensors.cltC = 80;
  TEST_ASSERT(applyProtections(1234) == 1234, "protecao normal");

  // 4. Checksum do mapa.
  mapLoadDefault();
  const uint16_t crc1 = mapChecksum(fuelMap);
  TEST_ASSERT(crc1 == mapChecksum(fuelMap), "checksum estavel");
  fuelMap[0][0] += 1;
  const uint16_t crc2 = mapChecksum(fuelMap);
  TEST_ASSERT(crc1 != crc2, "checksum muda com dado");
  fuelMap[0][0] -= 1;

  // 5. NVS round-trip. Usa o mapa atual (que pode ser padrao ou customizado
  // carregado na inicializacao), salva um valor alterado, recarrega e
  // restaura o mapa original completo no final para nao perder a calibracao.
  const uint16_t originalCel00 = fuelMap[0][0];
  fuelMap[0][0] = (uint16_t)(originalCel00 + 50);
  const uint16_t testCel00 = fuelMap[0][0];
  TEST_ASSERT(mapSaveNvs(), "NVS save ok");
  mapLoadDefault();
  TEST_ASSERT(fuelMap[0][0] != testCel00, "RAM apaga mapa antes do load");
  mapLoadNvs();
  TEST_ASSERT(fuelMap[0][0] == testCel00, "NVS load volta cel00 alterado");
  memcpy(fuelMap, fuelMapBackup, sizeof(fuelMap));
  TEST_ASSERT(mapSaveNvs(), "NVS restore original map");

  // Restaura estado.
  sensors = sensorsBackup;
  measuredRpm = measuredRpmBackup;
  corrLamX1000 = corrLamBackup;
  memcpy(fuelMap, fuelMapBackup, sizeof(fuelMap));

  Serial.printf("[test] === RESULTADO: %u/%u PASS (falhas %u) ===\n",
                pass, total, fail);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("programmable-ecu #142 self-test (!) | NVS T/S/D");
  engineStartMs = millis();
  mapLoadNvs();
  watchdogInit();

  // Opcao de self-test no boot: mantem a porta aberta e aperta '!' ao resetar.
  const unsigned long testWaitStart = millis();
  while (millis() - testWaitStart < 1500) {
    if (Serial.available() && Serial.read() == '!') {
      runSelfTests();
      while (true) { delay(100); }
    }
  }

  displayBegin();
  ckpBeginSimulated();
}

void loop() {
  mapPollSerial();
  simTick();
  displayTick();
  watchdogReset();
}
