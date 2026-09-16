/*
  Programmable ECU — Demo visual interativo (#1.x)

  Versao de apresentacao focada em PROCESSAMENTO de dados do motor.
  Le sensores (simulados), calcula tempo de injecao e mostra tudo no TFT
  de forma visual e facil de entender.

  Controles:
  - Botoes fisicos (ativo em LOW, pull-up interno):
      GPIO 4  = POWER    (liga/desliga motor)
      GPIO 33 = ACCEL    (acelera / solta acelerador)
      GPIO 14 = COLD     (partida no frio)
      GPIO 15 = HOT      (superaquecimento)
      GPIO 2  = LIMIT    (liga/desliga limitador de RPM)
  - Touch: areas na tela espelham os botoes (se o shield tiver touch
    calibrado; senao, use os botoes fisicos).

  Arduino IDE 2: LovyanGFX, ESP32 Dev Module, COM do CP2102, 115200.
*/

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <stdarg.h>

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
// Reutilizam pinos que no sketch principal sao CKP/injetores.
// Todos suportam pull-up interno (diferente dos GPIOs 34-39).
static const int kBtnPowerPin = 4;
static const int kBtnAccelPin = 33;
static const int kBtnColdPin = 14;
static const int kBtnHotPin = 15;
static const int kBtnLimitPin = 2;

// ---------- Constantes de simulacao ----------
static const uint32_t kIdleRpm = 900;
static const uint32_t kMaxRpm = 6000;
static const uint32_t kRpmLimit = 3000;
static const int16_t kCltLimitC = 105;
static const uint16_t kNormalCltC = 88;
static const uint16_t kColdCltC = 18;
static const uint16_t kHotCltC = 110;

// ---------- Estado do motor ----------
static EcuDisplay tft;
static bool engineOn = false;
static bool btnAccelHeld = false;
static bool coldMode = false;
static bool hotMode = false;
static bool limiterOn = false;

static uint32_t rpm = 0;
static uint8_t tps = 0;
static uint16_t mapKpa = 35;
static int16_t cltC = kColdCltC;
static int16_t iatC = 24;
static uint16_t lambdaX100 = 100;
static uint16_t fuelPwX100 = 0;

static unsigned long lastEngineMs = 0;
static unsigned long lastDrawMs = 0;
static unsigned long lastSerialMs = 0;

// ---------- Utilitarios de desenho ----------
static int clamp(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static uint16_t colorForRpm(uint32_t value) {
  if (value < 2000) return TFT_GREEN;
  if (value < 4000) return TFT_YELLOW;
  if (value < kRpmLimit) return TFT_ORANGE;
  return TFT_RED;
}

static uint16_t colorForClt(int16_t value) {
  if (value < 40) return TFT_CYAN;
  if (value < 85) return TFT_GREEN;
  if (value < kCltLimitC) return TFT_YELLOW;
  return TFT_RED;
}

// Desenha um arco de gauge entre startAngle e endAngle (em graus, 0 = topo).
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

static void drawGaugeRPM(int cx, int cy, int r) {
  const int thick = 12;
  // Fundo cinza do arco (sobrescreve frame anterior)
  drawArc(cx, cy, r, thick, 135.0f, 405.0f, TFT_DARKGREY);
  // Parte preenchida
  const float frac = clamp((int)rpm, 0, (int)kMaxRpm) / (float)kMaxRpm;
  const float endAngle = 135.0f + frac * 270.0f;
  const uint16_t color = colorForRpm(rpm);
  drawArc(cx, cy, r, thick, 135.0f, endAngle, color);

  // Texto central — background preto apaga o numero antigo
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(middle_center);
  tft.drawString("RPM", cx, cy - 18);
  tft.setTextSize(3);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawNumber(rpm, cx, cy + 8);
  tft.setTextDatum(top_left);
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
                         int valX, const char *fmt, ...) {
  // Limpa preenchimento anterior
  tft.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_DARKGREY);
  const int filled = clamp((value - minV) * (w - 2) / (maxV - minV), 0, w - 2);
  tft.fillRect(x + 1, y + 1, filled, h - 2, color);

  // Valor numerico ao lado com background preto
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(valX, y + 4);
  va_list args;
  va_start(args, fmt);
  tft.vprintf(fmt, args);
  va_end(args);
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

// Desenha tudo que nao muda (fundos, labels, contornos). Chamado no setup.
static void drawStatic() {
  tft.fillScreen(TFT_BLACK);

  // Cabecalho
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(12, 10);
  tft.print("Programmable ECU  DEMO");
  tft.drawLine(12, 34, tft.width() - 12, 34, TFT_WHITE);

  // Labels e contornos das barras
  drawBarStatic("TPS %", 12, 200, 140, 18);
  drawBarStatic("MAP kPa", 168, 200, 140, 18);
  drawBarStatic("CLT C", 12, 250, 140, 18);
  drawBarStatic("IAT C", 168, 250, 140, 18);
  drawBarStatic("LAMBDA", 12, 300, 140, 18);
  drawBarStatic("FUEL ms", 168, 300, 140, 18);

  // Label do indicador de injecao
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(12, 388);
  tft.print("Injecao:");

  // Contorno do indicador de injecao
  const int fuelW = tft.width() - 100;
  tft.drawRect(96, 388, fuelW, 20, TFT_WHITE);

  // Legendas de botoes
  drawButtonHint(12, 446, "P", "Ligar");
  drawButtonHint(90, 446, "A", "Acelerar");
  drawButtonHint(180, 446, "C", "Frio");
  drawButtonHint(252, 446, "H", "Hot");
  drawButtonHint(334, 446, "L", "Lim");
}

// Desenha apenas elementos que mudam a cada frame. Chamado no loop.
static void drawDynamic() {
  // Gauge RPM no centro superior (raio menor para nao cobrir as barras)
  drawGaugeRPM(tft.width() / 2, 100, 56);

  // Barras de sensores (valores)
  drawBarValue(12, 200, 140, 18, tps, 0, 100, TFT_GREEN, 12 + 142, "%u%%");
  drawBarValue(168, 200, 140, 18, mapKpa, 30, 100, TFT_YELLOW, 168 + 142, "%u");
  drawBarValue(12, 250, 140, 18, cltC, 0, 120, colorForClt(cltC), 12 + 142, "%d");
  drawBarValue(168, 250, 140, 18, iatC, 0, 60, TFT_ORANGE, 168 + 142, "%d");
  drawBarValue(12, 300, 140, 18, lambdaX100, 80, 120, TFT_CYAN, 12 + 142, "%u.%02u", lambdaX100 / 100, lambdaX100 % 100);
  drawBarValue(168, 300, 140, 18, fuelPwX100, 0, 2500, TFT_BLUE, 168 + 142, "%u.%02u", fuelPwX100 / 100, fuelPwX100 % 100);

  // Caixas de status
  drawStatusBox(12, 340, "POWER", engineOn, engineOn ? TFT_GREEN : TFT_RED);
  drawStatusBox(114, 340, "COLD", coldMode, TFT_CYAN);
  drawStatusBox(216, 340, "HOT", hotMode, TFT_RED);
  drawStatusBox(318 - 90, 340, "LIMIT", limiterOn && engineOn, TFT_ORANGE);

  // Preenchimento do indicador de injecao
  const int fuelW = tft.width() - 100;
  const int fuelFill = clamp((int)fuelPwX100 * (fuelW - 2) / 2500, 0, fuelW - 2);
  tft.fillRect(96 + 1, 388 + 1, fuelW - 2, 20 - 2, TFT_DARKGREY);
  tft.fillRect(96 + 1, 388 + 1, fuelFill, 20 - 2, fuelPwX100 > 0 ? TFT_BLUE : TFT_DARKGREY);

  // Aviso de protecao — limpa a linha antes
  tft.fillRect(12, 420, 300, 22, TFT_BLACK);
  if (engineOn && ((limiterOn && rpm >= kRpmLimit) || cltC >= kCltLimitC)) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(12, 420);
    if (cltC >= kCltLimitC) {
      tft.print("PROT: SUPERAQUECIMENTO");
    } else {
      tft.print("PROT: LIMITADOR RPM");
    }
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(12, 420);
    tft.print("Sistema normal");
  }
}

// ---------- Processamento do motor (foco principal) ----------
static void engineProcess(unsigned long dtMs) {
  if (!engineOn) {
    rpm = 0;
    tps = 0;
    mapKpa = 35;
    fuelPwX100 = 0;
    lambdaX100 = 100;
    return;
  }

  // TPS: 0 no comeco, acelera se botao pressionado
  const uint8_t targetTps = btnAccelHeld ? (uint8_t)80 : (uint8_t)8;
  if (tps < targetTps) tps = (uint8_t)(tps + 2);
  if (tps > targetTps) tps = (uint8_t)(tps - 2);

  // CLT: varia lentamente conforme modo
  const int16_t targetClt = hotMode ? kHotCltC : (coldMode ? kColdCltC : kNormalCltC);
  if (cltC < targetClt) cltC++;
  if (cltC > targetClt) cltC--;

  // MAP: segue TPS de forma simples
  mapKpa = (uint16_t)(32 + (tps * 66) / 100);

  // IAT: levemente influenciado por TPS
  iatC = (int16_t)(24 + tps / 20);

  // Lambda: rico na aceleracao, pobre na solta, estavel no marcha-lenta
  if (tps > 30) {
    lambdaX100 = 85;
  } else if (tps < 8) {
    lambdaX100 = 104;
  } else {
    lambdaX100 = 100;
  }

  // RPM: sobe com TPS, limitado
  uint32_t targetRpm = kIdleRpm + (tps * (kMaxRpm - kIdleRpm)) / 100;
  if (limiterOn && targetRpm > kRpmLimit) {
    targetRpm = kRpmLimit;
  }
  const uint32_t rpmStep = 50;
  if (rpm < targetRpm) rpm = (rpm + rpmStep > targetRpm) ? targetRpm : rpm + rpmStep;
  if (rpm > targetRpm) rpm = (rpm < rpmStep + targetRpm) ? targetRpm : rpm - rpmStep;

  // Protecao: se superaquecer ou limitar, zera combustivel
  bool cutFuel = (cltC >= kCltLimitC) || (limiterOn && rpm >= kRpmLimit);

  // Calculo de PW (simplificado, nao usa mapa real neste demo)
  // Base ~ 8ms no lenta, sobe com carga
  uint16_t basePw = (uint16_t)(800 + (mapKpa - 30) * 10 + (rpm * 25) / 1000);
  // Correcao CLT: +40% frio
  if (cltC < 20) basePw = (uint16_t)(basePw * 140 / 100);
  else if (cltC < 80) basePw = (uint16_t)(basePw * (100 + (80 - cltC) / 2) / 100);
  // Correcao aceleracao: +18%
  if (btnAccelHeld && tps > 20) basePw = (uint16_t)(basePw * 118 / 100);

  fuelPwX100 = cutFuel ? 0 : basePw;
}

// ---------- Leitura de botoes fisicos ----------
static bool wasPressed[5] = {false, false, false, false, false};

static void readButtons() {
  const bool pressed[5] = {
    digitalRead(kBtnPowerPin) == LOW,
    digitalRead(kBtnAccelPin) == LOW,
    digitalRead(kBtnColdPin) == LOW,
    digitalRead(kBtnHotPin) == LOW,
    digitalRead(kBtnLimitPin) == LOW,
  };

  // POWER: toggle no flanco de descida
  if (pressed[0] && !wasPressed[0]) {
    engineOn = !engineOn;
    if (!engineOn) rpm = 0;
  }
  // ACCEL: mantem pressionado
  btnAccelHeld = pressed[1];
  // COLD: toggle
  if (pressed[2] && !wasPressed[2]) coldMode = !coldMode;
  // HOT: toggle
  if (pressed[3] && !wasPressed[3]) hotMode = !hotMode;
  // LIMIT: toggle
  if (pressed[4] && !wasPressed[4]) limiterOn = !limiterOn;

  for (int i = 0; i < 5; i++) wasPressed[i] = pressed[i];
}

// ---------- Leitura de touch (opcional) ----------
static bool wasTouched = false;

static void readTouch() {
  if (!tft.touch()) {
    return; // Touch nao configurado para este shield
  }
  int tx, ty;
  if (tft.getTouch(&tx, &ty)) {
    if (!wasTouched) {
      // Areas correspondem as legendas de botoes na parte inferior
      if (ty > 440) {
        if (tx < 70) engineOn = !engineOn;
        else if (tx < 160) btnAccelHeld = true;
        else if (tx < 230) coldMode = !coldMode;
        else if (tx < 310) hotMode = !hotMode;
        else limiterOn = !limiterOn;
      }
    }
    wasTouched = true;
  } else {
    wasTouched = false;
    btnAccelHeld = false; // solta acelerador se so touch estiver solto
  }
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

  delay(1200);
  drawStatic(); // desenha elementos estaticos uma unica vez

  Serial.println("[demo] botoes: P=power A=accel C=cold H=hot L=limit");
  if (!tft.touch()) {
    Serial.println("[demo] touch nao configurado; usando botoes fisicos");
  }
}

void loop() {
  readButtons();
  readTouch();

  const unsigned long now = millis();
  engineProcess(now - lastEngineMs);
  lastEngineMs = now;

  if (now - lastDrawMs >= 100) {
    lastDrawMs = now;
    drawDynamic();
  }

  if (now - lastSerialMs >= 1000) {
    lastSerialMs = now;
    Serial.printf("[demo] on=%u rpm=%u tps=%u map=%u clt=%d pw=%u.%02u lim=%u hot=%u\n",
                  engineOn, rpm, tps, mapKpa, cltC,
                  fuelPwX100 / 100, fuelPwX100 % 100,
                  limiterOn, hotMode);
  }
}
