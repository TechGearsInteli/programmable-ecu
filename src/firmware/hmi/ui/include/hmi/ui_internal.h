/**
 * @file ui_internal.h
 * Estado interno das telas. Usado pelos arquivos da interface e pelos
 * cenarios de teste do simulador (que "tocam" os mesmos botoes e leem os
 * mesmos textos que o usuario ve). O resto do firmware usa so ui.h.
 */
#ifndef HMI_UI_INTERNAL_H
#define HMI_UI_INTERNAL_H

#include "hmi/ui.h"
#include "hmi/ui_format.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_font_t hmi_font_14;
extern const lv_font_t hmi_font_16;
extern const lv_font_t hmi_font_20;
extern const lv_font_t hmi_font_28;

/* Paleta (tema escuro, como os painéis de injeção) */
#define HMI_C_BG lv_color_hex(0x0E1116)
#define HMI_C_PANEL lv_color_hex(0x171C23)
#define HMI_C_PANEL2 lv_color_hex(0x202833)
#define HMI_C_BORDER lv_color_hex(0x2C3644)
#define HMI_C_TEXT lv_color_hex(0xE6EAF0)
#define HMI_C_MUTED lv_color_hex(0x8C96A8)
#define HMI_C_ACCENT lv_color_hex(0x2F80ED)
#define HMI_C_CURRENT lv_color_hex(0xFF8A00)
#define HMI_C_OK lv_color_hex(0x27AE60)
#define HMI_C_WARN lv_color_hex(0xF2C94C)
#define HMI_C_DANGER lv_color_hex(0xEB5757)

#define HMI_TOP_H 34
#define HMI_NAV_W 104
#define HMI_CONTENT_W (HMI_SCREEN_W - HMI_NAV_W)
#define HMI_CONTENT_H (HMI_SCREEN_H - HMI_TOP_H)

typedef enum { HMI_ED_CELL = 1, HMI_ED_PARAM } hmi_editor_kind_t;

/** Barra de edicao: escolhe valor, envia ao ESP e mostra confirmacao ou recusa. */
typedef struct {
    lv_obj_t *bar, *title, *value, *minus, *plus, *keypad, *send, *status;
    uint8_t kind;
    bool has_target;
    uint8_t table, row, col, param;
    uint8_t unit;
    int16_t min, max, step;
    int32_t current; /* valor que o ESP diz que vale */
    int32_t pending; /* valor escolhido na tela, ainda nao enviado */
    bool waiting;
    uint8_t last_status; /* 0xFF = nenhum */
} hmi_editor_t;

/** Grade de uma tabela (lv_table) com a celula atual destacada. */
typedef struct {
    lv_obj_t *table;
    uint8_t tid;
    bool flip_rows;      /* carga alta no topo, como nos softwares de calibracao */
    int sel_row, sel_col;/* celula escolhida (indice da tabela), -1 = nenhuma */
    int cur_row, cur_col;/* celula em que o motor esta (telemetria), -1 = nenhuma */
    int16_t vmin, vmax;  /* para as cores do mapa de calor */
    hmi_editor_t *ed;
} hmi_grid_t;

#define HMI_EVQ 16

typedef struct {
    ecu_hmi_link_t *link;
    const hmi_platform_t *plat;
    hmi_screen_t current;
    uint32_t now;

    lv_obj_t *top, *nav, *content[HMI_SCR_COUNT], *nav_btn[HMI_SCR_COUNT];
    lv_obj_t *top_link, *top_calib, *top_alert;

    /* Painel */
    lv_obj_t *p_rpm, *p_rpm_bar, *p_sync, *p_alert, *p_alert_lbl, *p_corr;
    lv_obj_t *p_val[8];
    bool alert_rpm, alert_clt;

    /* Mapas */
    hmi_grid_t fuel, ign, curve;
    hmi_editor_t ed_fuel, ed_ign, ed_curve, ed_param;
    lv_obj_t *fuel_info, *ign_info;

    /* Correcoes */
    uint8_t corr_sel;
    lv_obj_t *corr_btn[5], *corr_btn_lbl[5], *corr_title, *corr_desc, *corr_now, *corr_chart;
    lv_chart_series_t *corr_ser;

    /* Ficha do motor */
    lv_obj_t *m_table, *m_order, *m_info;
    int m_sel;

    /* Gravar e backup */
    lv_obj_t *s_state, *s_save, *s_export, *s_import, *s_status, *s_bar;
    uint8_t s_last_job;

    /* Teclado numerico (janela por cima de tudo) */
    lv_obj_t *kb_modal, *kb_ta, *kb, *kb_hint;
    hmi_editor_t *kb_target;

    ecu_hmi_event_t evq[HMI_EVQ];
    volatile uint8_t evq_n;
    bool tables_dirty, params_dirty;
} hmi_ui_t;

extern hmi_ui_t g_ui;

/* ui_common.c */
lv_obj_t *hmi_panel(lv_obj_t *parent);
lv_obj_t *hmi_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);
lv_obj_t *hmi_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user);
void hmi_editor_create(hmi_editor_t *ed, lv_obj_t *parent);
void hmi_editor_target_cell(hmi_editor_t *ed, uint8_t table, uint8_t row, uint8_t col);
void hmi_editor_target_param(hmi_editor_t *ed, uint8_t param);
void hmi_editor_on_ack(hmi_editor_t *ed, uint8_t status, int16_t value);
void hmi_editor_on_timeout(hmi_editor_t *ed);
void hmi_editor_refresh(hmi_editor_t *ed);
void hmi_editor_set_pending(hmi_editor_t *ed, int32_t v);
void hmi_grid_create(hmi_grid_t *g, lv_obj_t *parent, uint8_t tid, hmi_editor_t *ed, int32_t w);
void hmi_grid_set_table(hmi_grid_t *g, uint8_t tid);
void hmi_grid_refresh(hmi_grid_t *g);
void hmi_grid_set_current(hmi_grid_t *g, int row, int col);
void hmi_grid_select(hmi_grid_t *g, int row, int col);
/** Linha/coluna da lv_table para uma celula da tabela de dados. */
void hmi_grid_cell_to_view(const hmi_grid_t *g, int row, int col, uint32_t *vrow, uint32_t *vcol);
void hmi_keypad_open(hmi_editor_t *ed);
void hmi_keypad_create(void);

/* telas */
void hmi_painel_create(lv_obj_t *parent);
void hmi_painel_refresh(void);
void hmi_maps_create(lv_obj_t *fuel_parent, lv_obj_t *ign_parent);
void hmi_maps_refresh(void);
void hmi_corr_create(lv_obj_t *parent);
void hmi_corr_refresh(void);
void hmi_corr_select(uint8_t idx);
void hmi_motor_create(lv_obj_t *parent);
void hmi_motor_refresh(void);
void hmi_motor_select(int param);
void hmi_save_create(lv_obj_t *parent);
void hmi_save_refresh(void);
void hmi_save_on_event(const ecu_hmi_event_t *ev);

#ifdef __cplusplus
}
#endif

#endif /* HMI_UI_INTERNAL_H */
