/*
  Programmable ECU — Demo visual interativo (#1.x)

  Versao de apresentacao focada em PROCESSAMENTO de dados do motor.
  Le sensores (simulados), calcula tempo de injecao e mostra tudo no TFT
  de forma visual e facil de entender.

  Controles:
  - Botoes fisicos (ativo em LOW, pull-up interno):
      GPIO 4  = POWER    (liga/desliga motor)
      GPIO 33 = ACCEL    (segurar = sobe TPS; soltar = desce TPS)
      GPIO 14 = COLD     (ar e arrefecimento frios)
      GPIO 15 = HOT      (superaquecimento)
      GPIO 2  = LIMIT    (liga/desliga limitador de RPM)

  Arduino IDE 2: LovyanGFX, ESP32 Dev Module, COM do CP2102, 115200.
*/

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265f
#endif

// ---------- TFT 3.5" 8-bit paralelo (mesmo pinout do projeto) ----------
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

// ---------- Pinos dos botoes fisicos ----------
static const int kBtnPowerPin = 4;
static const int kBtnAccelPin = 33;
static const int kBtnColdPin = 14;
static const int kBtnHotPin = 15;
static const int kBtnLimitPin = 2;

// ---------- Constantes de simulacao ----------
static const uint32_t kIdleRpm = 900;
static const uint32_t kMaxRpm = 6000;
static const uint32_t kRpmLimit = 3000;
static const uint32_t kRpmRedFrom = 5000;
static const float kIdleTps = 8.0f;
static const float kFullTps = 100.0f;
static const int16_t kCltLimitC = 105;
static const uint16_t kNormalCltC = 88;
static const uint16_t kAmbientCltC = 18;
static const uint16_t kAmbientIatC = 22;
static const uint16_t kColdCltC = 10;
static const uint16_t kColdIatC = 8;
static const uint16_t kHotCltC = 110;
static const uint16_t kHotIatC = 55;

// Passo fixo da fisica. Independente de quanto o TFT demora para desenhar.
static const unsigned long kSimDtMs = 20;
static const unsigned long kMaxElapsedMs = 40; // nunca simula mais que 40 ms por volta do loop
static const uint8_t kMaxSimSteps = 2;

// Taxas (unidades por segundo) — como um carro, nao um interruptor.
static const float kTpsRate = 92.0f;     // 8% -> 100% em ~1.0 s
static const float kRpmRate = 680.0f;    // lenta -> 6000 em ~7.5 s
static const float kMapRate = 18.0f;     // MAP atrasado em relacao ao TPS
static const float kIatRate = 4.0f;
static const float kCltRate = 6.0f;
static const float kLambdaRate = 12.0f;
static const float kRpmOffRate = 400.0f;

// ---------- Estado do motor (fisica em float, display em inteiro) ----------
static EcuDisplay tft;
static bool engineOn = false;
static bool accelHeld = false;  // true enquanto o botao fisico ACCEL estiver pressionado
static bool coldMode = false;
static bool hotMode = false;
static bool limiterOn = false;

static float fRpm = 0.0f;
static float fTps = 0.0f;
static float fMap = 35.0f;
static float fClt = (float)kAmbientCltC;
static float fIat = (float)kAmbientIatC;
static float fLambda = 100.0f;
static float fFuelPw = 0.0f;
static float engineTempTimer = 0.0f;
static float idlePhase = 0.0f;

static uint32_t rpm = 0;
static uint8_t tps = 0;
static uint16_t mapKpa = 35;
static int16_t cltC = kAmbientCltC;
static int16_t iatC = kAmbientIatC;
static uint16_t lambdaX100 = 100;
static uint16_t fuelPwX100 = 0;

static unsigned long lastLoopMs = 0;
static unsigned long simAccMs = 0;
static unsigned long lastDrawMs = 0;
static unsigned long lastSerialMs = 0;

// Gauge estatico
static int gCx = 240;
static int gCy = 100;
static int gR = 56;
static int lastNeedleX = -1;
static int lastNeedleY = -1;
static uint32_t lastDrawnRpm = 0xFFFFFFFF;

// ---------- Utilitarios ----------
static int clamp(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static uint16_t colorForRpm(uint32_t value) {
  return (value >= kRpmRedFrom) ? TFT_RED : TFT_GREEN;
}

static uint16_t colorForClt(int16_t value) {
  if (value < 40) return TFT_CYAN;
  if (value < 85) return TFT_GREEN;
  if (value < kCltLimitC) return TFT_YELLOW;
  return TFT_RED;
}

static float moveTowardsF(float value, float target, float ratePerSecond, float dtS) {
  const float step = ratePerSecond * dtS;
  if (value < target) {
    return (value + step > target) ? target : value + step;
  }
  if (value > target) {
    return (value - step < target) ? target : value - step;
  }
  return value;
}

static void publishDisplayVars() {
  rpm = (uint32_t)(fRpm + 0.5f);
  tps = (uint8_t)clamp((int)(fTps + 0.5f), 0, 100);
  mapKpa = (uint16_t)clamp((int)(fMap + 0.5f), 0, 200);
  cltC = (int16_t)(fClt + (fClt >= 0 ? 0.5f : -0.5f));
  iatC = (int16_t)(fIat + (fIat >= 0 ? 0.5f : -0.5f));
  lambdaX100 = (uint16_t)(fLambda + 0.5f);
  fuelPwX100 = (uint16_t)clamp((int)(fFuelPw + 0.5f), 0, 4000);
}

// Desenha um arco de gauge entre startAngle e endAngle (0 = topo). So no setup.
static void drawArc(int cx, int cy, int r, int thick, float startDeg, float endDeg, uint16_t color) {
  const float step = 3.0f;
  for (float a = startDeg; a <= endDeg; a += step) {
    const float rad0 = (a - 90.0f) * PI / 180.0f;
    const float rad1 = (a + step - 90.0f) * PI / 180.0f;
    const int x0 = cx + (int)((r - thick / 2) * cosf(rad0));
    const int y0 = cy + (int)((r - thick / 2) * sinf(rad0));
    const int x1 = cx + (int)((r + thick / 2) * cosf(rad0));
    const int y1 = cy + (int)((r + thick / 2) * sinf(rad0));
    const int x2 = cx + (int)((r + thick / 2) * cosf(rad1));
    const int y2 = cy + (int)((r + thick / 2) * sinf(rad1));
    const int x3 = cx + (int)((r - thick / 2) * cosf(rad1));
    const int y3 = cy + (int)((r - thick / 2) * sinf(rad1));
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
    tft.fillTriangle(x0, y0, x2, y2, x3, y3, color);
  }
}

static void rpmToNeedleTip(uint32_t rpmVal, int *x, int *y) {
  const float frac = clamp((int)rpmVal, 0, (int)kMaxRpm) / (float)kMaxRpm;
  const float ang = (135.0f + frac * 270.0f - 90.0f) * PI / 180.0f;
  const int tipR = gR - 16;
  *x = gCx + (int)(tipR * cosf(ang));
  *y = gCy + (int)(tipR * sinf(ang));
}

static void drawGaugeFace() {
  const int thick = 4; // cerca de 1/3 da espessura anterior
  const float redFrac = (float)kRpmRedFrom / (float)kMaxRpm;
  const float redStart = 135.0f + redFrac * 270.0f;
  drawArc(gCx, gCy, gR, thick, 135.0f, redStart, TFT_DARKGREEN);
  drawArc(gCx, gCy, gR, thick, redStart, 405.0f, TFT_RED);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(gCx + gR + 10, gCy - 18);
  tft.print("RPM");
}

static void drawNeedle() {
  int x, y;
  rpmToNeedleTip(rpm, &x, &y);

  if (lastNeedleX >= 0) {
    tft.drawLine(gCx, gCy, lastNeedleX, lastNeedleY, TFT_BLACK);
    tft.drawLine(gCx + 1, gCy, lastNeedleX + 1, lastNeedleY, TFT_BLACK);
  }

  const uint16_t col = colorForRpm(rpm);
  tft.drawLine(gCx, gCy, x, y, col);
  tft.drawLine(gCx + 1, gCy, x + 1, y, col);
  tft.fillCircle(gCx, gCy, 3, TFT_WHITE);

  lastNeedleX = x;
  lastNeedleY = y;

  // Numero fora da roda, a direita, para nao cobrir a agulha.
  if (rpm != lastDrawnRpm) {
    tft.fillRect(gCx + gR + 8, gCy - 4, 72, 22, TFT_BLACK);
    tft.setTextColor(col, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(gCx + gR + 10, gCy);
    tft.print(rpm);
    lastDrawnRpm = rpm;
  }
}

static void drawBarStatic(const char *label, int x, int y, int w, int h) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(x, y - 18);
  tft.print(label);
  tft.drawRect(x, y, w, h, TFT_WHITE);
}

static void drawBarValue(int x, int y, int w, int h,
                         int value, int minV, int maxV, uint16_t color,
                         const char *fmt, ...) {
  tft.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_DARKGREY);
  const int filled = clamp((value - minV) * (w - 2) / (maxV - minV), 0, w - 2);
  if (filled > 0) {
    tft.fillRect(x + 1, y + 1, filled, h - 2, color);
  }

  char buf[16];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  const int pad = 4;
  const int textW = (int)strlen(buf) * 6;
  const int textX = x + w - pad - textW;
  tft.fillRect(textX - 2, y + 2, textW + 4, h - 4, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(textX, y + 4);
  tft.print(buf);
}

static void drawStatusBox(int x, int y, const char *label, bool active, uint16_t activeColor) {
  const int w = 90;
  const int h = 32;
  tft.fillRoundRect(x, y, w, h, 4, active ? activeColor : TFT_DARKGREY);
  tft.setTextColor(TFT_BLACK, active ? activeColor : TFT_DARKGREY);
  tft.setTextSize(2);
  tft.setTextDatum(middle_center);
  tft.drawString(label, x + w / 2, y + h / 2);
  tft.setTextDatum(top_left);
}

static void drawButtonHint(int x, int y, const char *key, const char *txt) {
  tft.fillCircle(x + 10, y + 10, 10, TFT_LIGHTGREY);
  tft.setTextColor(TFT_BLACK, TFT_LIGHTGREY);
  tft.setTextSize(1);
  tft.setTextDatum(middle_center);
  tft.drawString(key, x + 10, y + 10);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(top_left);
  tft.setCursor(x + 26, y + 4);
  tft.print(txt);
}

static void drawSnowflake(int cx, int cy, int r) {
  for (int i = 0; i < 6; i++) {
    const float a = (float)i * 60.0f * PI / 180.0f;
    const int x1 = cx + (int)(r * cosf(a));
    const int y1 = cy + (int)(r * sinf(a));
    tft.drawLine(cx, cy, x1, y1, TFT_CYAN);
    tft.drawLine(cx + 1, cy, x1 + 1, y1, TFT_CYAN);
    const int mx = cx + (int)(0.55f * r * cosf(a));
    const int my = cy + (int)(0.55f * r * sinf(a));
    const float a1 = a + 0.55f;
    const float a2 = a - 0.55f;
    const int br = r / 3;
    tft.drawLine(mx, my, mx + (int)(br * cosf(a1)), my + (int)(br * sinf(a1)), TFT_CYAN);
    tft.drawLine(mx, my, mx + (int)(br * cosf(a2)), my + (int)(br * sinf(a2)), TFT_CYAN);
  }
  tft.fillCircle(cx, cy, 3, TFT_WHITE);
}

static void drawFlame(int cx, int cy, int r) {
  tft.fillTriangle(cx, cy - r, cx - r + 2, cy + r / 3, cx + r - 2, cy + r / 3, TFT_RED);
  tft.fillTriangle(cx, cy - r + 6, cx - r / 2, cy + r / 4, cx + r / 2, cy + r / 4, TFT_ORANGE);
  tft.fillTriangle(cx, cy - r / 3, cx - r / 4, cy + r / 3, cx + r / 4, cy + r / 3, TFT_YELLOW);
  tft.fillCircle(cx, cy + r / 3, r / 3, TFT_RED);
}

static void drawHazard(int cx, int cy, int r) {
  const int topY = cy - r;
  const int botY = cy + r;
  const int half = r;
  tft.fillTriangle(cx, topY, cx - half, botY, cx + half, botY, TFT_YELLOW);
  tft.drawTriangle(cx, topY, cx - half, botY, cx + half, botY, TFT_BLACK);
  tft.fillRect(cx - 2, cy - r / 3, 5, r / 2 + 4, TFT_BLACK);
  tft.fillCircle(cx, cy + r / 2, 3, TFT_BLACK);
}

static void drawLimiterIcon(int cx, int cy, int r) {
  tft.fillCircle(cx, cy, r, TFT_ORANGE);
  tft.drawCircle(cx, cy, r, TFT_BLACK);
  tft.setTextDatum(middle_center);
  tft.setTextColor(TFT_BLACK, TFT_ORANGE);
  tft.setTextSize(1);
  tft.drawString("LIM", cx, cy - 6);
  tft.drawString("3k", cx, cy + 6);
  tft.setTextDatum(top_left);
}

static void drawIconPanel() {
  const int size = 50;
  const int gap = 4;
  const int x1 = tft.width() - size - 8;
  const int x0 = x1 - size - gap;
  const int y1 = tft.height() - size - 8;
  const int y0 = y1 - size - gap;

  tft.fillRect(x0, y0, size, size, TFT_BLACK);
  if (coldMode) {
    drawSnowflake(x0 + size / 2, y0 + size / 2, size / 2 - 6);
  }

  tft.fillRect(x1, y0, size, size, TFT_BLACK);
  if (hotMode || cltC >= kCltLimitC) {
    drawFlame(x1 + size / 2, y0 + size / 2, size / 2 - 6);
  }

  tft.fillRect(x0, y1, size, size, TFT_BLACK);
  if (limiterOn && engineOn) {
    drawLimiterIcon(x0 + size / 2, y1 + size / 2, size / 2 - 4);
  }

  tft.fillRect(x1, y1, size, size, TFT_BLACK);
  if (engineOn && rpm >= kRpmRedFrom) {
    drawHazard(x1 + size / 2, y1 + size / 2 - 2, size / 2 - 4);
  }
}

static void drawStatic() {
  tft.fillScreen(TFT_BLACK);
  gCx = tft.width() / 2;
  gCy = 100;
  gR = 56;

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(12, 10);
  tft.print("Programmable ECU  DEMO");
  tft.drawLine(12, 34, tft.width() - 12, 34, TFT_WHITE);

  drawGaugeFace();

  drawBarStatic("TPS %", 12, 200, 140, 18);
  drawBarStatic("MAP kPa", 168, 200, 140, 18);
  drawBarStatic("CLT C", 12, 250, 140, 18);
  drawBarStatic("IAT C", 168, 250, 140, 18);
  drawBarStatic("LAMBDA", 12, 300, 140, 18);
  drawBarStatic("FUEL ms", 168, 300, 140, 18);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(12, 388);
  tft.print("Injecao:");
  const int fuelW = tft.width() - 100;
  tft.drawRect(96, 388, fuelW, 20, TFT_WHITE);

  drawButtonHint(12, 446, "P", "Ligar");
  drawButtonHint(90, 446, "A", "Acelerar");
  drawButtonHint(180, 446, "C", "Ar/Agua frios");
  drawButtonHint(252, 446, "H", "Hot");
  drawButtonHint(334, 446, "L", "Lim 3k");
}

static void drawDynamic() {
  drawNeedle();

  drawBarValue(12, 200, 140, 18, tps, 0, 100, TFT_GREEN, "%u%%", tps);
  drawBarValue(168, 200, 140, 18, mapKpa, 30, 100, TFT_YELLOW, "%u", mapKpa);
  drawBarValue(12, 250, 140, 18, cltC, 0, 120, colorForClt(cltC), "%d", cltC);
  drawBarValue(168, 250, 140, 18, iatC, 0, 60, TFT_ORANGE, "%d", iatC);
  drawBarValue(12, 300, 140, 18, lambdaX100, 80, 120, TFT_CYAN, "%u.%02u",
               lambdaX100 / 100, lambdaX100 % 100);
  drawBarValue(168, 300, 140, 18, fuelPwX100, 0, 2500, TFT_BLUE, "%u.%02u",
               fuelPwX100 / 100, fuelPwX100 % 100);

  drawStatusBox(12, 340, "POWER", engineOn, engineOn ? TFT_GREEN : TFT_RED);
  drawStatusBox(114, 340, "ACCEL", accelHeld && engineOn, TFT_YELLOW);
  drawStatusBox(216, 340, "HOT", hotMode, TFT_RED);
  drawStatusBox(318 - 90, 340, "LIMIT", limiterOn && engineOn, TFT_ORANGE);

  const int fuelW = tft.width() - 100;
  const int fuelFill = clamp((int)fuelPwX100 * (fuelW - 2) / 2500, 0, fuelW - 2);
  tft.fillRect(96 + 1, 388 + 1, fuelW - 2, 20 - 2, TFT_DARKGREY);
  tft.fillRect(96 + 1, 388 + 1, fuelFill, 20 - 2, fuelPwX100 > 0 ? TFT_BLUE : TFT_DARKGREY);

  tft.fillRect(12, 420, 300, 22, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(12, 420);
  if (!engineOn) {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.print("Motor desligado");
  } else if (cltC >= kCltLimitC) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("PROT: SUPERAQUECIMENTO");
  } else if (limiterOn && rpm >= kRpmLimit) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("PROT: CORTE RPM 3000");
  } else if (coldMode) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.print("Modo frio ativo");
  } else if (accelHeld) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.print("Acelerando");
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.print("Sistema normal");
  }

  drawIconPanel();
}

static void engineProcess(unsigned long dtMs) {
  const float dtS = dtMs / 1000.0f;

  if (!engineOn) {
    fRpm = moveTowardsF(fRpm, 0.0f, kRpmOffRate, dtS);
    fTps = moveTowardsF(fTps, 0.0f, kTpsRate, dtS);
    fMap = moveTowardsF(fMap, 35.0f, kMapRate, dtS);
    fFuelPw = 0.0f;
    fLambda = 100.0f;
    engineTempTimer = 0.0f;
    publishDisplayVars();
    return;
  }

  engineTempTimer += dtS;
  idlePhase += dtS * 2.2f;

  // Pedal: segurar sobe TPS, soltar volta para a lenta.
  const float targetTps = accelHeld ? kFullTps : kIdleTps;
  fTps = moveTowardsF(fTps, targetTps, kTpsRate, dtS);

  float targetIat;
  if (coldMode) {
    targetIat = (float)kColdIatC;
  } else if (hotMode) {
    targetIat = (float)kHotIatC;
  } else {
    targetIat = (float)kAmbientIatC + fTps / 12.0f;
  }
  fIat = moveTowardsF(fIat, targetIat, kIatRate, dtS);

  float targetClt;
  if (coldMode) {
    targetClt = (float)kColdCltC;
  } else if (hotMode) {
    targetClt = (float)kHotCltC;
  } else {
    const float tempFactor = engineTempTimer / 120.0f;
    if (tempFactor >= 1.0f) {
      targetClt = (float)kNormalCltC;
    } else {
      targetClt = (float)kAmbientCltC + ((float)kNormalCltC - (float)kAmbientCltC) * tempFactor;
    }
  }
  fClt = moveTowardsF(fClt, targetClt, kCltRate, dtS);

  const float targetMap = 32.0f + (fTps * 68.0f) / 100.0f;
  fMap = moveTowardsF(fMap, targetMap, kMapRate, dtS);

  float targetLam;
  if (fTps > 35.0f) {
    targetLam = 85.0f;
  } else if (fTps < 10.0f) {
    targetLam = 104.0f;
  } else {
    targetLam = 100.0f;
  }
  fLambda = moveTowardsF(fLambda, targetLam, kLambdaRate, dtS);

  // TPS manda no RPM: 8% = lenta, 100% = 6000.
  float tpsSpan = kFullTps - kIdleTps;
  float tpsNorm = (fTps - kIdleTps) / tpsSpan;
  if (tpsNorm < 0.0f) tpsNorm = 0.0f;
  if (tpsNorm > 1.0f) tpsNorm = 1.0f;
  float targetRpm = (float)kIdleRpm + tpsNorm * (float)(kMaxRpm - kIdleRpm);

  // Variacao da marcha lenta (motor "vivo").
  if (!accelHeld && fTps <= (kIdleTps + 2.0f) && fClt < (float)kCltLimitC) {
    targetRpm += 40.0f * sinf(idlePhase);
  }

  if (engineTempTimer < 1.6f) {
    const float crankRpm = 200.0f;
    const float catchRpm = crankRpm + ((float)kIdleRpm - crankRpm) * (engineTempTimer / 1.6f);
    if (targetRpm > catchRpm) {
      targetRpm = catchRpm;
    }
  }

  // Teto mecanico: nunca passa de 6000.
  if (targetRpm > (float)kMaxRpm) {
    targetRpm = (float)kMaxRpm;
  }

  if (fClt >= (float)kCltLimitC) {
    targetRpm = (float)kIdleRpm * 0.5f;
  }

  // Limitador armado: ao bater 6000 corta combustivel e o RPM cai um pouco,
  // depois sobe de novo (oscilacao visivel).
  bool limiterCut = false;
  if (limiterOn && fRpm >= (float)kRpmLimit) {
    limiterCut = true;
    targetRpm = (float)kRpmLimit - 150.0f;
  }

  fRpm = moveTowardsF(fRpm, targetRpm, kRpmRate, dtS);

  const bool cutFuel = (fClt >= (float)kCltLimitC) || limiterCut;
  float basePw = 800.0f + (fMap - 30.0f) * 10.0f + (fRpm * 25.0f) / 1000.0f;
  if (fClt < 20.0f) {
    basePw *= 1.40f;
  } else if (fClt < 80.0f) {
    basePw *= (100.0f + (80.0f - fClt) / 2.0f) / 100.0f;
  }
  if (accelHeld && fTps > 20.0f) {
    basePw *= 1.18f;
  }
  fFuelPw = cutFuel ? 0.0f : basePw;

  publishDisplayVars();
}

static bool wasPressed[5] = {false, false, false, false, false};

static void readButtons() {
  const bool pressed[5] = {
    digitalRead(kBtnPowerPin) == LOW,
    digitalRead(kBtnAccelPin) == LOW,
    digitalRead(kBtnColdPin) == LOW,
    digitalRead(kBtnHotPin) == LOW,
    digitalRead(kBtnLimitPin) == LOW,
  };

  if (pressed[0] && !wasPressed[0]) {
    engineOn = !engineOn;
  }
  // ACCEL: enquanto segura, acelera; ao soltar, o TPS volta sozinho.
  accelHeld = pressed[1];
  if (pressed[2] && !wasPressed[2]) coldMode = !coldMode;
  if (pressed[3] && !wasPressed[3]) hotMode = !hotMode;
  if (pressed[4] && !wasPressed[4]) limiterOn = !limiterOn;

  for (int i = 0; i < 5; i++) wasPressed[i] = pressed[i];
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("programmable-ecu DEMO visual");

  pinMode(kBtnPowerPin, INPUT_PULLUP);
  pinMode(kBtnAccelPin, INPUT_PULLUP);
  pinMode(kBtnColdPin, INPUT_PULLUP);
  pinMode(kBtnHotPin, INPUT_PULLUP);
  pinMode(kBtnLimitPin, INPUT_PULLUP);

  if (!tft.init()) {
    Serial.println("[display] init falhou");
    return;
  }
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(12, tft.height() / 2 - 20);
  tft.print("Programmable ECU");
  tft.setCursor(12, tft.height() / 2 + 8);
  tft.print("Demo visual");

  delay(800);
  drawStatic();
  drawNeedle();

  lastLoopMs = millis();
  lastDrawMs = lastLoopMs;
  lastSerialMs = lastLoopMs;

  Serial.println("[demo] botoes: P=power  A=segurar acelera  C=cold  H=hot  L=limit 3k");
  Serial.println("[demo] fisica 20 ms, gauge estatico + agulha");
}

void loop() {
  readButtons();

  const unsigned long now = millis();
  unsigned long elapsed = now - lastLoopMs;
  lastLoopMs = now;
  if (elapsed > kMaxElapsedMs) {
    elapsed = kMaxElapsedMs;
  }
  simAccMs += elapsed;

  uint8_t steps = 0;
  while (simAccMs >= kSimDtMs && steps < kMaxSimSteps) {
    engineProcess(kSimDtMs);
    simAccMs -= kSimDtMs;
    steps++;
  }

  if (now - lastDrawMs >= 50) {
    lastDrawMs = now;
    drawDynamic();
  }

  if (now - lastSerialMs >= 250) {
    lastSerialMs = now;
    Serial.printf("[demo] on=%u rpm=%u tps=%u map=%u clt=%d iat=%d pw=%u.%02u acc=%u lim=%u cold=%u hot=%u\n",
                  engineOn, rpm, tps, mapKpa, cltC, iatC,
                  fuelPwX100 / 100, fuelPwX100 % 100,
                  accelHeld, limiterOn, coldMode, hotMode);
  }
}
