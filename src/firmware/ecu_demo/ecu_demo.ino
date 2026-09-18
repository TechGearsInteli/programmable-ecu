/*
  Programmable ECU — Demo visual interativo (#1.x)

  Versao de apresentacao focada em PROCESSAMENTO de dados do motor.
  Le sensores (simulados), calcula tempo de injecao e mostra tudo no TFT
  de forma visual e facil de entender.

  Controles:
  - Botoes fisicos (ativo em LOW, pull-up interno):
      GPIO 4  = POWER    (liga/desliga motor)
      GPIO 33 = ACCEL    (acelera / solta acelerador)
      GPIO 14 = COLD     (ar e arrefecimento frios)
      GPIO 15 = HOT      (superaquecimento)
      GPIO 2  = LIMIT    (liga/desliga limitador de RPM)
  - Touch: areas na tela espelham os botoes (se o shield tiver touch
    calibrado; senao, use os botoes fisicos).

  Portas livres para mais botoes/sensores no ESP32 com esta tela:
    GPIO 5, 34, 35, 36, 39  (34-39 sao input-only).

  Arduino IDE 2: LovyanGFX, ESP32 Dev Module, COM do CP2102, 115200.
*/

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
static const uint32_t kMaxRpm = 6500;     // fundo de escala do gauge
static const uint32_t kRpmLimit = 6000;   // corte de seguranca
static const int16_t kCltLimitC = 105;
static const uint16_t kNormalCltC = 88;
static const uint16_t kAmbientCltC = 18;
static const uint16_t kAmbientIatC = 22;
static const uint16_t kColdCltC = 10;
static const uint16_t kColdIatC = 8;
static const uint16_t kHotCltC = 110;
static const uint16_t kHotIatC = 55;

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
static int16_t cltC = kAmbientCltC;
static int16_t iatC = kAmbientIatC;
static uint16_t lambdaX100 = 100;
static uint16_t fuelPwX100 = 0;
static float engineTempTimer = 0.0f; // segundos desde ligou

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
  if (value < 5000) return TFT_ORANGE;
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

  // Angulos da faixa colorida do fundo do gauge
  const float greenEndFrac  = clamp(2000, 0, (int)kMaxRpm) / (float)kMaxRpm;
  const float yellowEndFrac = clamp(4000, 0, (int)kMaxRpm) / (float)kMaxRpm;
  const float orangeEndFrac = clamp(5000, 0, (int)kMaxRpm) / (float)kMaxRpm;
  const float greenEndAngle  = 135.0f + greenEndFrac * 270.0f;
  const float yellowEndAngle = 135.0f + yellowEndFrac * 270.0f;
  const float orangeEndAngle = 135.0f + orangeEndFrac * 270.0f;

  // Fundo do arco com cores de zona
  drawArc(cx, cy, r, thick, 135.0f, greenEndAngle, TFT_DARKGREEN);
  drawArc(cx, cy, r, thick, greenEndAngle, yellowEndAngle, TFT_YELLOW);
  drawArc(cx, cy, r, thick, yellowEndAngle, orangeEndAngle, TFT_ORANGE);
  drawArc(cx, cy, r, thick, orangeEndAngle, 405.0f, TFT_RED);

  // Parte preenchida por cima
  const float frac = clamp((int)rpm, 0, (int)kMaxRpm) / (float)kMaxRpm;
  const float endAngle = 135.0f + frac * 270.0f;
  const uint16_t color = colorForRpm(rpm);
  drawArc(cx, cy, r, thick, 135.0f, endAngle, color);

  // Apaga o centro sem tocar o arco (raio interno = r - thick/2)
  tft.fillCircle(cx, cy, r - thick / 2 - 2, TFT_BLACK);

  // Texto central
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
                         const char *fmt, ...) {
  // Limpa preenchimento anterior
  tft.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_DARKGREY);
  const int filled = clamp((value - minV) * (w - 2) / (maxV - minV), 0, w - 2);
  if (filled > 0) {
    tft.fillRect(x + 1, y + 1, filled, h - 2, color);
  }

  // Valor numerico dentro da barra, alinhado a direita
  char buf[16];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  const int pad = 4;
  const int textW = strlen(buf) * 6; // fonte 1 = 6 px por caractere
  const int textX = x + w - pad - textW;
  // Apaga uma pequena area atras do texto para legibilidade
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
  drawButtonHint(180, 446, "C", "Ar/Agua frios");
  drawButtonHint(252, 446, "H", "Hot");
  drawButtonHint(334, 446, "L", "Lim 6k");
}

// Desenha apenas elementos que mudam a cada frame. Chamado no loop.
static void drawDynamic() {
  // Gauge RPM no centro superior (raio menor para nao cobrir as barras)
  drawGaugeRPM(tft.width() / 2, 100, 56);

  // Barras de sensores (valores dentro da barra)
  drawBarValue(12, 200, 140, 18, tps, 0, 100, TFT_GREEN, "%u%%", tps);
  drawBarValue(168, 200, 140, 18, mapKpa, 30, 100, TFT_YELLOW, "%u", mapKpa);
  drawBarValue(12, 250, 140, 18, cltC, 0, 120, colorForClt(cltC), "%d", cltC);
  drawBarValue(168, 250, 140, 18, iatC, 0, 60, TFT_ORANGE, "%d", iatC);
  drawBarValue(12, 300, 140, 18, lambdaX100, 80, 120, TFT_CYAN, "%u.%02u", lambdaX100 / 100, lambdaX100 % 100);
  drawBarValue(168, 300, 140, 18, fuelPwX100, 0, 2500, TFT_BLUE, "%u.%02u", fuelPwX100 / 100, fuelPwX100 % 100);

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

  // Aviso de protecao / estado — limpa a linha antes
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
    tft.print("PROT: CORTE RPM 6000");
  } else if (coldMode) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.print("Modo frio ativo");
  } else if (hotMode) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.print("Modo quente ativo");
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.print("Sistema normal");
  }
}

// Aproxima value em direction com taxa rate por segundo (dtMs em ms).
static int32_t moveTowards(int32_t value, int32_t target, float ratePerSecond, unsigned long dtMs) {
  if (ratePerSecond <= 0) return target;
  const int32_t delta = (int32_t)(ratePerSecond * dtMs / 1000.0f);
  if (delta == 0) {
    return value < target ? value + 1 : (value > target ? value - 1 : value);
  }
  if (value < target) return (value + delta > target) ? target : value + delta;
  if (value > target) return (value - delta < target) ? target : value - delta;
  return value;
}

// ---------- Processamento do motor (foco principal) ----------
static void engineProcess(unsigned long dtMs) {
  if (!engineOn) {
    // Desligamento gradual
    rpm = (uint32_t)moveTowards((int32_t)rpm, 0, 120.0f, dtMs);
    tps = (uint8_t)moveTowards((int32_t)tps, 0, 60.0f, dtMs);
    mapKpa = (uint16_t)moveTowards((int32_t)mapKpa, 35, 18.0f, dtMs);
    fuelPwX100 = 0;
    lambdaX100 = 100;
    engineTempTimer = 0.0f;
    return;
  }

  engineTempTimer += dtMs / 1000.0f;

  // TPS: acelera e solta devagar (pedal eletronico)
  const uint8_t targetTps = btnAccelHeld ? (uint8_t)85 : (uint8_t)8;
  tps = (uint8_t)moveTowards((int32_t)tps, targetTps, 45.0f, dtMs);

  // IAT: segue ambiente + influencia leve de carga
  int16_t targetIat;
  if (coldMode) {
    targetIat = kColdIatC;
  } else if (hotMode) {
    targetIat = kHotIatC;
  } else {
    targetIat = (int16_t)(kAmbientIatC + tps / 12);
  }
  iatC = (int16_t)moveTowards((int32_t)iatC, targetIat, 5.0f, dtMs);

  // CLT: aquece com o tempo desde ligado, a menos que modo frio/quente force
  int16_t targetClt;
  if (coldMode) {
    targetClt = kColdCltC;
  } else if (hotMode) {
    targetClt = kHotCltC;
  } else {
    // Aquece de ambiente ate normal em ~120 s
    const float tempFactor = engineTempTimer / 120.0f;
    if (tempFactor >= 1.0f) {
      targetClt = kNormalCltC;
    } else {
      targetClt = (int16_t)(kAmbientCltC + (kNormalCltC - kAmbientCltC) * tempFactor);
    }
  }
  cltC = (int16_t)moveTowards((int32_t)cltC, targetClt, 1.2f, dtMs);

  // MAP: segue TPS com um pequeno atraso (corpo de borboleta + admissao)
  const uint16_t targetMap = (uint16_t)(32 + (tps * 68) / 100);
  mapKpa = (uint16_t)moveTowards((int32_t)mapKpa, targetMap, 22.0f, dtMs);

  // Lambda: rico na aceleracao, pobre na solta, estavel no marcha-lenta
  if (tps > 35) {
    lambdaX100 = 85;
  } else if (tps < 10) {
    lambdaX100 = 104;
  } else {
    lambdaX100 = 100;
  }

  // RPM: tem inercia e depende do TPS
  uint32_t targetRpm = kIdleRpm + (tps * (kMaxRpm - kIdleRpm)) / 100;
  // Partida: quando acabou de ligar, nao salta direto para marcha-lenta
  if (engineTempTimer < 1.6f) {
    const uint32_t crankRpm = 200;
    const uint32_t catchRpm = (uint32_t)(crankRpm + (kIdleRpm - crankRpm) * (engineTempTimer / 1.6f));
    targetRpm = (targetRpm < catchRpm) ? targetRpm : catchRpm;
  }
  // Limitador de seguranca em 6000 rpm
  if (limiterOn && targetRpm > kRpmLimit) {
    targetRpm = kRpmLimit;
  }

  // Protecao: se superaquecer, o motor perde forca e desacelera
  if (cltC >= kCltLimitC) {
    targetRpm = kIdleRpm / 2; // cai para ~450 rpm (motor "morrendo")
  }

  rpm = (uint32_t)moveTowards((int32_t)rpm, (int32_t)targetRpm, 600.0f, dtMs);

  // Corte de combustivel no superaquecimento ou limitador ativo
  bool cutFuel = (cltC >= kCltLimitC) || (limiterOn && rpm >= kRpmLimit);

  // Calculo de PW (simplificado, nao usa mapa real neste demo)
  // Base ~ 8ms no lenta, sobe com carga
  uint16_t basePw = (uint16_t)(800 + (mapKpa - 30) * 10 + (rpm * 25) / 1000);
  // Correcao CLT: +40% muito frio; intermediario ate 80C
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
    Serial.printf("[demo] on=%u rpm=%u tps=%u map=%u clt=%d iat=%d pw=%u.%02u lim=%u cold=%u hot=%u\n",
                  engineOn, rpm, tps, mapKpa, cltC, iatC,
                  fuelPwX100 / 100, fuelPwX100 % 100,
                  limiterOn, coldMode, hotMode);
  }
}
