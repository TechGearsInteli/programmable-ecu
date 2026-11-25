/*
 * Driver da placa Sunton ESP32-8048S043C para o LVGL 9.2.2.
 * Painel: esp_lcd RGB (ESP-IDF 4.4 do Arduino-ESP32 2.0.17), framebuffer na PSRAM.
 * Touch: GT911 lido por I2C (so o primeiro dedo).
 * NAO VALIDADO EM HARDWARE.
 */
#include <Arduino.h>
#include <Wire.h>
#include <esp_heap_caps.h>

#include "board.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "lvgl.h"

static esp_lcd_panel_handle_t panel;
static uint16_t touch_max_x = LCD_H_RES, touch_max_y = LCD_V_RES;

/* LVGL desenha faixas de 40 linhas na RAM interna e o esp_lcd copia para o
 * framebuffer da PSRAM, que o controlador RGB varre sozinho. */
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px);
    lv_display_flush_ready(disp);
}

static bool gt911_read(uint16_t reg, uint8_t *buf, size_t n)
{
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)TOUCH_I2C_ADDR, (int)n) != (int)n) return false;
    for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)Wire.read();
    return true;
}

static void gt911_write8(uint16_t reg, uint8_t v)
{
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.write(v);
    Wire.endTransmission();
}

static void touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    static int32_t last_x, last_y;
    uint8_t status = 0;
    data->state = LV_INDEV_STATE_RELEASED;
    if (gt911_read(0x814E, &status, 1) && (status & 0x80)) {
        if ((status & 0x0F) > 0) {
            uint8_t p[4];
            if (gt911_read(0x8150, p, 4)) {
                const uint16_t x = (uint16_t)(p[0] | (p[1] << 8));
                const uint16_t y = (uint16_t)(p[2] | (p[3] << 8));
                last_x = (int32_t)x * LCD_H_RES / touch_max_x;
                last_y = (int32_t)y * LCD_V_RES / touch_max_y;
                data->state = LV_INDEV_STATE_PRESSED;
            }
        }
        gt911_write8(0x814E, 0); /* libera o buffer do GT911 */
    }
    data->point.x = last_x;
    data->point.y = last_y;
}

static void touch_init(void)
{
    pinMode(TOUCH_PIN_RST, OUTPUT);
    digitalWrite(TOUCH_PIN_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_PIN_RST, HIGH);
    delay(60);
    Wire.begin(TOUCH_PIN_SDA, TOUCH_PIN_SCL, 400000);
    uint8_t cfg[4];
    /* resolucao configurada no GT911: escala para os 800x480 da tela */
    if (gt911_read(0x8048, cfg, 4)) {
        const uint16_t mx = (uint16_t)(cfg[0] | (cfg[1] << 8));
        const uint16_t my = (uint16_t)(cfg[2] | (cfg[3] << 8));
        if (mx > 0 && my > 0) {
            touch_max_x = mx;
            touch_max_y = my;
        }
    }
}

bool board_init(void)
{
    pinMode(LCD_PIN_BACKLIGHT, OUTPUT);
    digitalWrite(LCD_PIN_BACKLIGHT, LOW);

    esp_lcd_rgb_panel_config_t cfg = {};
    cfg.clk_src = LCD_CLK_SRC_PLL160M;
    cfg.timings.pclk_hz = LCD_PCLK_HZ;
    cfg.timings.h_res = LCD_H_RES;
    cfg.timings.v_res = LCD_V_RES;
    cfg.timings.hsync_pulse_width = 4;
    cfg.timings.hsync_back_porch = 8;
    cfg.timings.hsync_front_porch = 8;
    cfg.timings.vsync_pulse_width = 4;
    cfg.timings.vsync_back_porch = 8;
    cfg.timings.vsync_front_porch = 8;
    cfg.timings.flags.pclk_active_neg = 1;
    cfg.data_width = 16;
    cfg.psram_trans_align = 64;
    cfg.hsync_gpio_num = LCD_PIN_HSYNC;
    cfg.vsync_gpio_num = LCD_PIN_VSYNC;
    cfg.de_gpio_num = LCD_PIN_DE;
    cfg.pclk_gpio_num = LCD_PIN_PCLK;
    const int pins[16] = LCD_DATA_PINS;
    for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = pins[i];
    cfg.disp_gpio_num = -1;
    cfg.flags.fb_in_psram = 1;
    if (esp_lcd_new_rgb_panel(&cfg, &panel) != ESP_OK) return false;
    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    digitalWrite(LCD_PIN_BACKLIGHT, HIGH);

    lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    const size_t lines = 40;
    const size_t bytes = LCD_H_RES * lines * sizeof(uint16_t);
    void *buf1 = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    void *buf2 = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf1 || !buf2) return false;
    lv_display_set_buffers(disp, buf1, buf2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);

    touch_init();
    lv_indev_t *touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, touch_read);
    return true;
}
