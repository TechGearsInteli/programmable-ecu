/**
 * Configuracao do LVGL 9.2.2 para a HMI da ECU.
 *
 * O MESMO arquivo vale para o ESP32-S3 e para o simulador no PC, para que a
 * tela desenhada no computador seja a mesma que vai para a placa. Tudo o que
 * nao aparece aqui usa o valor padrao do LVGL (lv_conf_internal.h).
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* RGB565: o mesmo formato do painel RGB de 16 bits da placa S3 */
#define LV_COLOR_DEPTH 16

#if defined(ARDUINO) || defined(ESP_PLATFORM)
/* ESP32-S3: malloc da libc, que usa a PSRAM para blocos grandes. O LVGL so e
 * chamado pela task da tela, entao nao precisa de sistema operacional nele. */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_USE_OS LV_OS_NONE
#else
/* Simulador no PC */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (2U * 1024U * 1024U)
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#if defined(_WIN32) && defined(HMI_SIM_WINDOWS)
/* O driver de janela do Windows do LVGL exige o modo de threads do Windows */
#define LV_USE_OS LV_OS_WINDOWS
#define LV_USE_WINDOWS 1
#else
#define LV_USE_OS LV_OS_NONE
#endif
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#endif

/* Atualiza a tela a cada 33 ms (~30 quadros/s); o painel muda de numero a 10 Hz */
#define LV_DEF_REFR_PERIOD 33
#define LV_DPI_DEF 130

/* Fontes: as Montserrat embutidas so tem ASCII. As fontes hmi_font_* (geradas por
 * tools/gen_font_latin1.py) trazem os acentos e caem nestas para o resto. */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Widgets que a HMI nao usa ficam fora para economizar flash */
#define LV_USE_CALENDAR 0
#define LV_USE_LOTTIE 0
#define LV_USE_MENU 0
#define LV_USE_ROLLER 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_ANIMIMG 0
#define LV_USE_SPAN 0

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#define LV_BUILD_EXAMPLES 0

#endif /* LV_CONF_H */
