/**
 * @file ui.h
 * Telas da HMI (ESP32-S3, 800x480) feitas em LVGL 9.2.2.
 *
 * O mesmo codigo roda no simulador do PC e na placa: a interface so conversa
 * com o link (ecu_hmi_link_t) e com a plataforma (arquivo de backup). Ela nao
 * sabe se do outro lado do link ha uma UART de verdade ou o motor virtual.
 *
 * Telas: Painel, Combustivel, Ignicao, Correcoes, Motor (ficha) e Gravar.
 */
#ifndef HMI_UI_H
#define HMI_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ecu/ecu_hmi_link.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HMI_SCREEN_W 800
#define HMI_SCREEN_H 480

typedef enum {
    HMI_SCR_PAINEL = 0,
    HMI_SCR_FUEL,
    HMI_SCR_IGN,
    HMI_SCR_CORR,
    HMI_SCR_MOTOR,
    HMI_SCR_SAVE,
    HMI_SCR_COUNT
} hmi_screen_t;

/** O que muda entre placa e simulador. */
typedef struct {
    bool (*backup_write)(void *ctx, const uint8_t *data, size_t len);
    size_t (*backup_read)(void *ctx, uint8_t *buf, size_t cap);
    void *ctx;
    const char *backup_name; /* mostrado na tela, ex.: "/backup.tgcal" */
} hmi_platform_t;

/** Cria as telas. Chame depois de lv_init() e de criar o display. */
void hmi_ui_init(ecu_hmi_link_t *link, const hmi_platform_t *platform);

/** Ligue no callback de eventos do link. Pode ser chamado fora do LVGL. */
void hmi_ui_on_link_event(void *ctx, const ecu_hmi_event_t *ev);

void hmi_ui_show(hmi_screen_t screen);
hmi_screen_t hmi_ui_current(void);

/** Forca a atualizacao imediata (o timer interno faz isso a cada 100 ms). */
void hmi_ui_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* HMI_UI_H */
