/* Estrutura das telas: barra de cima, menu da esquerda e a troca de telas. */
#include <stdio.h>
#include <string.h>

#include "hmi/ui_internal.h"

hmi_ui_t g_ui;

static const char *const NAV_TEXT[HMI_SCR_COUNT] = {
    LV_SYMBOL_HOME "\nPainel",   LV_SYMBOL_TINT "\nCombustível", LV_SYMBOL_CHARGE "\nIgnição",
    LV_SYMBOL_SHUFFLE "\nCorreções", LV_SYMBOL_SETTINGS "\nMotor", LV_SYMBOL_SAVE "\nGravar",
};

static void nav_cb(lv_event_t *e)
{
    hmi_ui_show((hmi_screen_t)(uintptr_t)lv_event_get_user_data(e));
}

void hmi_ui_show(hmi_screen_t screen)
{
    if (screen >= HMI_SCR_COUNT) return;
    g_ui.current = screen;
    for (int i = 0; i < HMI_SCR_COUNT; i++) {
        if (i == (int)screen) {
            lv_obj_remove_flag(g_ui.content[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(g_ui.nav_btn[i], HMI_C_ACCENT, 0);
        } else {
            lv_obj_add_flag(g_ui.content[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(g_ui.nav_btn[i], HMI_C_PANEL, 0);
        }
    }
    hmi_ui_refresh();
}

hmi_screen_t hmi_ui_current(void)
{
    return g_ui.current;
}

void hmi_ui_on_link_event(void *ctx, const ecu_hmi_event_t *ev)
{
    (void)ctx;
    /* So guarda: quem mexe nas telas e o timer do LVGL (seguro com threads). */
    if (g_ui.evq_n < HMI_EVQ) {
        g_ui.evq[g_ui.evq_n] = *ev;
        g_ui.evq_n++;
    }
}

static hmi_editor_t *editor_of_table(uint8_t table)
{
    if (table == ECU_TBL_FUEL) return &g_ui.ed_fuel;
    if (table == ECU_TBL_IGN) return &g_ui.ed_ign;
    return &g_ui.ed_curve;
}

static void handle_event(const ecu_hmi_event_t *ev)
{
    switch (ev->type) {
    case ECU_HMI_EV_SYNCED:
    case ECU_HMI_EV_CONNECTED:
        g_ui.tables_dirty = true;
        g_ui.params_dirty = true;
        break;
    case ECU_HMI_EV_CELL_ACK: {
        hmi_editor_t *ed = editor_of_table(ev->table);
        if (ed->kind == HMI_ED_CELL && ed->table == ev->table && ed->row == ev->row && ed->col == ev->col) {
            hmi_editor_on_ack(ed, ev->status, ev->value);
        }
        g_ui.tables_dirty = true;
        break;
    }
    case ECU_HMI_EV_PARAM_ACK:
        if (g_ui.ed_param.param == ev->param) {
            hmi_editor_on_ack(&g_ui.ed_param, ev->status, ev->value);
        }
        g_ui.params_dirty = true;
        break;
    case ECU_HMI_EV_TIMEOUT:
        if (ev->req_type == ECU_MSG_CELL_WRITE_REQ) {
            if (g_ui.ed_fuel.waiting) hmi_editor_on_timeout(&g_ui.ed_fuel);
            if (g_ui.ed_ign.waiting) hmi_editor_on_timeout(&g_ui.ed_ign);
            if (g_ui.ed_curve.waiting) hmi_editor_on_timeout(&g_ui.ed_curve);
        } else if (ev->req_type == ECU_MSG_PARAM_WRITE_REQ && g_ui.ed_param.waiting) {
            hmi_editor_on_timeout(&g_ui.ed_param);
        }
        hmi_save_on_event(ev);
        break;
    default:
        hmi_save_on_event(ev);
        break;
    }
}

static void top_refresh(void)
{
    const ecu_hmi_link_t *l = g_ui.link;
    char buf[96];
    if (ecu_hmi_link_alive(l, g_ui.now)) {
        if (l->job == ECU_JOB_SYNC) {
            snprintf(buf, sizeof(buf), LV_SYMBOL_REFRESH " Carregando mapas do ESP… %u%%", ecu_hmi_link_job_progress(l));
            lv_obj_set_style_text_color(g_ui.top_link, HMI_C_WARN, 0);
        } else {
            snprintf(buf, sizeof(buf), LV_SYMBOL_OK " ESP do motor conectado%s",
                     l->pong.node == ECU_NODE_VIRTUAL ? " (motor virtual)" : "");
            lv_obj_set_style_text_color(g_ui.top_link, HMI_C_OK, 0);
        }
    } else if (l->connected) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " Conectado, sem telemetria");
        lv_obj_set_style_text_color(g_ui.top_link, HMI_C_WARN, 0);
    } else {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " Procurando o ESP do motor…");
        lv_obj_set_style_text_color(g_ui.top_link, HMI_C_DANGER, 0);
    }
    lv_label_set_text(g_ui.top_link, buf);

    if (l->tlm_ok) {
        if (l->tlm.flags & ECU_TLM_CALIB_DIRTY) {
            lv_label_set_text(g_ui.top_calib, LV_SYMBOL_EDIT " Alterado, não gravado");
            lv_obj_set_style_text_color(g_ui.top_calib, HMI_C_WARN, 0);
        } else if (l->tlm.flags & ECU_TLM_CALIB_DEFAULT) {
            lv_label_set_text(g_ui.top_calib, "Calibração padrão");
            lv_obj_set_style_text_color(g_ui.top_calib, HMI_C_MUTED, 0);
        } else {
            lv_label_set_text(g_ui.top_calib, LV_SYMBOL_SAVE " Gravado na flash");
            lv_obj_set_style_text_color(g_ui.top_calib, HMI_C_OK, 0);
        }
    } else {
        lv_label_set_text(g_ui.top_calib, "");
    }

    if (g_ui.alert_rpm || g_ui.alert_clt) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " %s%s%s", g_ui.alert_rpm ? "RPM" : "",
                 g_ui.alert_rpm && g_ui.alert_clt ? " + " : "", g_ui.alert_clt ? "TEMPERATURA" : "");
        lv_label_set_text(g_ui.top_alert, buf);
        lv_obj_remove_flag(g_ui.top_alert, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_ui.top_alert, LV_OBJ_FLAG_HIDDEN);
    }
}

void hmi_ui_refresh(void)
{
    g_ui.now = lv_tick_get();
    while (g_ui.evq_n > 0) {
        ecu_hmi_event_t ev = g_ui.evq[0];
        g_ui.evq_n--;
        memmove(&g_ui.evq[0], &g_ui.evq[1], sizeof(ev) * g_ui.evq_n);
        handle_event(&ev);
    }
    hmi_painel_refresh(); /* tambem avalia os alertas, que aparecem em todas as telas */
    top_refresh();
    hmi_maps_refresh();
    hmi_corr_refresh();
    hmi_motor_refresh();
    hmi_save_refresh();
    g_ui.tables_dirty = false;
    g_ui.params_dirty = false;
}

static void refresh_timer(lv_timer_t *t)
{
    (void)t;
    hmi_ui_refresh();
}

void hmi_ui_init(ecu_hmi_link_t *link, const hmi_platform_t *platform)
{
    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.link = link;
    g_ui.plat = platform;
    g_ui.m_sel = -1;

    lv_display_t *disp = lv_display_get_default();
    lv_theme_t *th = lv_theme_default_init(disp, HMI_C_ACCENT, HMI_C_CURRENT, true, &hmi_font_16);
    lv_display_set_theme(disp, th);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, HMI_C_BG, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* barra de cima */
    g_ui.top = lv_obj_create(scr);
    lv_obj_set_size(g_ui.top, HMI_SCREEN_W, HMI_TOP_H);
    lv_obj_set_pos(g_ui.top, 0, 0);
    lv_obj_set_style_bg_color(g_ui.top, HMI_C_PANEL, 0);
    lv_obj_set_style_border_width(g_ui.top, 0, 0);
    lv_obj_set_style_radius(g_ui.top, 0, 0);
    lv_obj_set_style_pad_hor(g_ui.top, 10, 0);
    lv_obj_set_style_pad_ver(g_ui.top, 0, 0);
    lv_obj_remove_flag(g_ui.top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *brand = hmi_label(g_ui.top, &hmi_font_16, HMI_C_TEXT, "TechGears ECU");
    lv_obj_align(brand, LV_ALIGN_LEFT_MID, 0, 0);
    g_ui.top_link = hmi_label(g_ui.top, &hmi_font_14, HMI_C_MUTED, "");
    lv_obj_align(g_ui.top_link, LV_ALIGN_LEFT_MID, 140, 0);
    g_ui.top_calib = hmi_label(g_ui.top, &hmi_font_14, HMI_C_MUTED, "");
    lv_obj_align(g_ui.top_calib, LV_ALIGN_LEFT_MID, 478, 0);
    g_ui.top_alert = hmi_label(g_ui.top, &hmi_font_14, lv_color_white(), "");
    lv_obj_set_style_bg_color(g_ui.top_alert, HMI_C_DANGER, 0);
    lv_obj_set_style_bg_opa(g_ui.top_alert, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(g_ui.top_alert, 6, 0);
    lv_obj_set_style_radius(g_ui.top_alert, 4, 0);
    lv_obj_align(g_ui.top_alert, LV_ALIGN_RIGHT_MID, 0, 0);

    /* menu da esquerda */
    g_ui.nav = lv_obj_create(scr);
    lv_obj_set_size(g_ui.nav, HMI_NAV_W, HMI_CONTENT_H);
    lv_obj_set_pos(g_ui.nav, 0, HMI_TOP_H);
    lv_obj_set_style_bg_color(g_ui.nav, HMI_C_BG, 0);
    lv_obj_set_style_border_width(g_ui.nav, 0, 0);
    lv_obj_set_style_radius(g_ui.nav, 0, 0);
    lv_obj_set_style_pad_all(g_ui.nav, 4, 0);
    lv_obj_set_style_pad_row(g_ui.nav, 4, 0);
    lv_obj_set_flex_flow(g_ui.nav, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(g_ui.nav, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < HMI_SCR_COUNT; i++) {
        lv_obj_t *b = lv_button_create(g_ui.nav);
        g_ui.nav_btn[i] = b;
        lv_obj_set_size(b, LV_PCT(100), (HMI_CONTENT_H - 8 - 5 * 4) / HMI_SCR_COUNT);
        lv_obj_set_style_bg_color(b, HMI_C_PANEL, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_set_style_pad_all(b, 2, 0);
        lv_obj_t *lbl = hmi_label(b, &hmi_font_14, HMI_C_TEXT, NAV_TEXT[i]);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(b, nav_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }

    /* areas de conteudo (uma por tela, so uma visivel) */
    for (int i = 0; i < HMI_SCR_COUNT; i++) {
        lv_obj_t *c = lv_obj_create(scr);
        g_ui.content[i] = c;
        lv_obj_set_size(c, HMI_CONTENT_W, HMI_CONTENT_H);
        lv_obj_set_pos(c, HMI_NAV_W, HMI_TOP_H);
        lv_obj_set_style_bg_color(c, HMI_C_BG, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_set_style_radius(c, 0, 0);
        lv_obj_set_style_pad_all(c, 4, 0);
        lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    }

    hmi_painel_create(g_ui.content[HMI_SCR_PAINEL]);
    hmi_maps_create(g_ui.content[HMI_SCR_FUEL], g_ui.content[HMI_SCR_IGN]);
    hmi_corr_create(g_ui.content[HMI_SCR_CORR]);
    hmi_motor_create(g_ui.content[HMI_SCR_MOTOR]);
    hmi_save_create(g_ui.content[HMI_SCR_SAVE]);
    hmi_keypad_create();

    hmi_ui_show(HMI_SCR_PAINEL);
    lv_timer_create(refresh_timer, 100, NULL); /* a tela atualiza a 10 Hz */
}
