/**
 * Camada da placa: tudo o que muda se a equipe comprar outra tela S3.
 *
 * Valores da Sunton ESP32-8048S043C (800x480, GT911), tirados da documentacao
 * publica da placa. NAO VALIDADO EM HARDWARE: confira cada pino no esquematico
 * da placa comprada antes de gravar.
 */
#pragma once

#include <stdint.h>

/* UART para o ESP do motor (conector de expansao). A confirmar na placa. */
#define PIN_MOTOR_TX 17
#define PIN_MOTOR_RX 18

/* Painel RGB 16 bits (RGB565) */
#define LCD_H_RES 800
#define LCD_V_RES 480
#define LCD_PCLK_HZ (14 * 1000 * 1000)
#define LCD_PIN_BACKLIGHT 2
#define LCD_PIN_DE 40
#define LCD_PIN_VSYNC 41
#define LCD_PIN_HSYNC 39
#define LCD_PIN_PCLK 42
/* ordem do esp_lcd: B0..B4, G0..G5, R0..R4 */
#define LCD_DATA_PINS {8, 3, 46, 9, 1, 5, 6, 7, 15, 16, 4, 45, 48, 47, 21, 14}

/* Touch capacitivo GT911 (I2C) */
#define TOUCH_PIN_SDA 19
#define TOUCH_PIN_SCL 20
#define TOUCH_PIN_RST 38
#define TOUCH_I2C_ADDR 0x5D

/** Liga painel, backlight e touch e registra o display e o "dedo" no LVGL. */
bool board_init(void);
