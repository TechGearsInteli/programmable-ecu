/* Ficha do motor (1.6.1.7): o que da unidade ao mapa. So muda com o motor parado. */
#include <stdio.h>

#include "ecu/ecu_engine.h"
#include "hmi/ui_internal.h"

static void motor_draw_cb(lv_event_t *e)
{
    lv_draw_task_t *task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t *base = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(task);
    if (base->part != LV_PART_ITEMS) return;
    lv_draw_fill_dsc_t *fill = lv_draw_task_get_fill_dsc(task);
    lv_draw_label_dsc_t *label = lv_draw_task_get_label_dsc(task);
    if (base->id1 == 0) {
        if (fill) fill->color = HMI_C_PANEL2;
        if (label) label->color = HMI_C_MUTED;
    } else if ((int)base->id1 - 1 == g_ui.m_sel) {
        if (fill) fill->color = lv_color_white();
        if (label) label->color = lv_color_black();
    }
}

static void motor_click_cb(lv_event_t *e)
{
    (void)e;
    uint32_t r, c;
    lv_table_get_selected_cell(g_ui.m_table, &r, &c);
    if (r == LV_TABLE_CELL_NONE || r == 0 || r > ECU_PAR_COUNT) return;
    hmi_motor_select((int)r - 1);
}

void hmi_motor_select(int param)
{
    if (param < 0 || param >= ECU_PAR_COUNT) return;
    g_ui.m_sel = param;
    hmi_editor_target_param(&g_ui.ed_param, (uint8_t)param);
    lv_obj_invalidate(g_ui.m_table);
}

void hmi_motor_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 6, 0);
    g_ui.m_table = lv_table_create(parent);
    lv_table_set_column_count(g_ui.m_table, 3);
    lv_table_set_row_count(g_ui.m_table, ECU_PAR_COUNT + 1);
    lv_table_set_column_width(g_ui.m_table, 0, 290);
    lv_table_set_column_width(g_ui.m_table, 1, 150);
    lv_table_set_column_width(g_ui.m_table, 2, HMI_CONTENT_W - 8 - 440);
    lv_obj_set_style_bg_color(g_ui.m_table, HMI_C_PANEL, LV_PART_ITEMS);
    lv_obj_set_style_text_color(g_ui.m_table, HMI_C_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(g_ui.m_table, &hmi_font_16, LV_PART_ITEMS);
    lv_obj_set_style_pad_ver(g_ui.m_table, 4, LV_PART_ITEMS);
    lv_obj_set_style_border_color(g_ui.m_table, HMI_C_BORDER, LV_PART_ITEMS);
    lv_obj_set_style_border_width(g_ui.m_table, 0, 0);
    lv_obj_set_style_pad_all(g_ui.m_table, 0, 0);
    lv_obj_remove_flag(g_ui.m_table, LV_OBJ_FLAG_SCROLLABLE);
    lv_table_set_cell_value(g_ui.m_table, 0, 0, "Ficha do motor");
    lv_table_set_cell_value(g_ui.m_table, 0, 1, "Valor no ESP");
    lv_table_set_cell_value(g_ui.m_table, 0, 2, "Limite");
    for (int p = 0; p < ECU_PAR_COUNT; p++) {
        lv_table_set_cell_value(g_ui.m_table, (uint32_t)p + 1u, 0, ecu_param_desc((uint8_t)p)->name);
    }
    lv_obj_add_event_cb(g_ui.m_table, motor_draw_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
    lv_obj_add_event_cb(g_ui.m_table, motor_click_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(g_ui.m_table, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);

    lv_obj_t *info = hmi_panel(parent);
    lv_obj_set_size(info, LV_PCT(100), 74);
    g_ui.m_order = hmi_label(info, &hmi_font_16, HMI_C_TEXT, "");
    lv_obj_set_pos(g_ui.m_order, 0, 0);
    g_ui.m_info = hmi_label(info, &hmi_font_14, HMI_C_MUTED, "");
    lv_obj_set_pos(g_ui.m_info, 0, 24);
    lv_obj_set_width(g_ui.m_info, LV_PCT(100));
    lv_label_set_long_mode(g_ui.m_info, LV_LABEL_LONG_WRAP);

    hmi_editor_create(&g_ui.ed_param, parent);
    g_ui.ed_param.kind = HMI_ED_PARAM;
    hmi_editor_refresh(&g_ui.ed_param);
}

void hmi_motor_refresh(void)
{
    const ecu_hmi_link_t *l = g_ui.link;
    char v[32], a[32], b[32], buf[160];
    if (g_ui.params_dirty || g_ui.current == HMI_SCR_MOTOR) {
        for (int p = 0; p < ECU_PAR_COUNT; p++) {
            const ecu_hmi_param_t *hp = &l->params[p];
            if (hp->ok) {
                hmi_fmt_with_unit(v, sizeof(v), hp->p.unit, hp->p.value, false);
                hmi_fmt_value(a, sizeof(a), hp->p.unit, hp->p.min, false);
                hmi_fmt_with_unit(b, sizeof(b), hp->p.unit, hp->p.max, false);
                snprintf(buf, sizeof(buf), "%s a %s", a, b);
            } else {
                snprintf(v, sizeof(v), "…");
                snprintf(buf, sizeof(buf), "…");
            }
            lv_table_set_cell_value(g_ui.m_table, (uint32_t)p + 1u, 1, v);
            lv_table_set_cell_value(g_ui.m_table, (uint32_t)p + 1u, 2, buf);
        }
    }
    if (g_ui.current != HMI_SCR_MOTOR) return;

    /* Ordem de ignicao calculada pela mesma funcao que o ESP do motor usa */
    uint8_t order[8];
    uint16_t spacing = 0;
    const uint8_t n = ecu_firing_order((uint8_t)l->params[ECU_PAR_CYLINDERS].p.value, order, &spacing);
    if (l->params[ECU_PAR_CYLINDERS].ok && n > 0) {
        int len = snprintf(buf, sizeof(buf), "Ordem de ignição: ");
        for (uint8_t i = 0; i < n && len < (int)sizeof(buf) - 8; i++) {
            len += snprintf(buf + len, sizeof(buf) - (size_t)len, i ? "-%u" : "%u", order[i]);
        }
        snprintf(buf + len, sizeof(buf) - (size_t)len, "  ·  um disparo a cada %u°", spacing);
    } else {
        snprintf(buf, sizeof(buf), "Ordem de ignição: --");
    }
    lv_label_set_text(g_ui.m_order, buf);

    const int teeth = l->params[ECU_PAR_TRIGGER_TEETH].p.value;
    const int miss = l->params[ECU_PAR_TRIGGER_MISSING].p.value;
    const int flow = l->params[ECU_PAR_INJ_FLOW].p.value;
    const int ref = l->params[ECU_PAR_INJ_FLOW_REF].p.value;
    const int rpm = ecu_hmi_link_alive(l, g_ui.now) ? l->tlm.rpm : -1;
    hmi_fmt_value(v, sizeof(v), ECU_UNIT_MS_X100, flow > 0 ? (ref * 100 + flow / 2) / flow : 0, false);
    snprintf(buf, sizeof(buf),
             "Roda fônica %d-%d · fator do bico %s (ref %d / bico %d cc/min) · %s",
             teeth, miss, v, ref, flow,
             rpm == 0 ? "motor parado: pode alterar" : "o ESP só aceita mudança na ficha com o motor parado (rpm = 0)");
    lv_label_set_text(g_ui.m_info, buf);
}
