/* Correcoes 2D sobre o mapa principal (1.6.1.6): a tela edita as curvas e
 * mostra a correcao que esta valendo agora. O calculo e feito no ESP do motor. */
#include <stdio.h>

#include "ecu/ecu_engine.h"
#include "hmi/ui_internal.h"

static const uint8_t CURVES[5] = {ECU_TBL_WARMUP, ECU_TBL_IAT, ECU_TBL_CRANK, ECU_TBL_DEADTIME, ECU_TBL_ACCEL};
static const char *const DESC[5] = {
    "Motor frio pede mais combustível. Soma % ao tempo de injeção conforme a temperatura do motor (CLT).",
    "Ar frio é mais denso, ar quente é menos. Corrige o tempo conforme a temperatura do ar (IAT).",
    "Só na partida (rpm abaixo da rotação de partida): enriquece conforme a temperatura do motor.",
    "O bico demora a abrir, mais ainda com bateria fraca. Soma este tempo morto (µs) ao pulso.",
    "Pedal pisado rápido: o ar chega antes do combustível. Enriquece pela velocidade do TPS (%/s).",
};

static void corr_btn_cb(lv_event_t *e)
{
    hmi_corr_select((uint8_t)(uintptr_t)lv_event_get_user_data(e));
}

void hmi_corr_select(uint8_t idx)
{
    if (idx >= 5) return;
    g_ui.corr_sel = idx;
    for (int i = 0; i < 5; i++) {
        lv_obj_set_style_border_color(g_ui.corr_btn[i], i == idx ? HMI_C_ACCENT : HMI_C_BORDER, 0);
        lv_obj_set_style_border_width(g_ui.corr_btn[i], i == idx ? 2 : 1, 0);
    }
    hmi_grid_set_table(&g_ui.curve, CURVES[idx]);
    g_ui.ed_curve.has_target = false;
    g_ui.ed_curve.kind = HMI_ED_CELL;
    hmi_editor_refresh(&g_ui.ed_curve);
    lv_label_set_text(g_ui.corr_title, ecu_table_desc(CURVES[idx])->name);
    lv_label_set_text(g_ui.corr_desc, DESC[idx]);
    g_ui.tables_dirty = true;
}

void hmi_corr_create(lv_obj_t *parent)
{
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_set_size(list, 206, 372);
    lv_obj_set_pos(list, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = lv_button_create(list);
        g_ui.corr_btn[i] = b;
        lv_obj_set_size(b, LV_PCT(100), 68);
        lv_obj_set_style_bg_color(b, HMI_C_PANEL, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, HMI_C_BORDER, 0);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_t *n = hmi_label(b, &hmi_font_16, HMI_C_TEXT, ecu_table_desc(CURVES[i])->name);
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 0, 0);
        g_ui.corr_btn_lbl[i] = hmi_label(b, &hmi_font_14, HMI_C_MUTED, "agora: --");
        lv_obj_align(g_ui.corr_btn_lbl[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_add_event_cb(b, corr_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }

    lv_obj_t *right = hmi_panel(parent);
    lv_obj_set_size(right, HMI_CONTENT_W - 8 - 212, 372);
    lv_obj_set_pos(right, 212, 0);
    g_ui.corr_title = hmi_label(right, &hmi_font_20, HMI_C_TEXT, "");
    lv_obj_set_pos(g_ui.corr_title, 0, 0);
    g_ui.corr_desc = hmi_label(right, &hmi_font_14, HMI_C_MUTED, "");
    lv_obj_set_width(g_ui.corr_desc, LV_PCT(100));
    lv_label_set_long_mode(g_ui.corr_desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(g_ui.corr_desc, 0, 28);

    g_ui.corr_chart = lv_chart_create(right);
    lv_obj_set_size(g_ui.corr_chart, LV_PCT(100), 150);
    lv_obj_set_pos(g_ui.corr_chart, 0, 70);
    lv_chart_set_type(g_ui.corr_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_ui.corr_chart, ECU_CURVE_POINTS);
    lv_obj_set_style_bg_color(g_ui.corr_chart, HMI_C_PANEL2, 0);
    lv_obj_set_style_border_width(g_ui.corr_chart, 0, 0);
    lv_obj_set_style_line_color(g_ui.corr_chart, HMI_C_BORDER, LV_PART_MAIN);
    g_ui.corr_ser = lv_chart_add_series(g_ui.corr_chart, HMI_C_CURRENT, LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_line_width(g_ui.corr_chart, 3, LV_PART_ITEMS);

    hmi_grid_create(&g_ui.curve, right, CURVES[0], &g_ui.ed_curve, HMI_CONTENT_W - 8 - 212 - 14);
    lv_obj_set_pos(g_ui.curve.table, 0, 228);

    g_ui.corr_now = hmi_label(right, &hmi_font_16, HMI_C_TEXT, "");
    lv_obj_set_pos(g_ui.corr_now, 0, 290);
    lv_obj_set_width(g_ui.corr_now, LV_PCT(100));
    lv_label_set_long_mode(g_ui.corr_now, LV_LABEL_LONG_WRAP);

    lv_obj_t *edwrap = lv_obj_create(parent);
    lv_obj_set_size(edwrap, LV_PCT(100), 60);
    lv_obj_set_pos(edwrap, 0, 378);
    lv_obj_set_style_bg_opa(edwrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(edwrap, 0, 0);
    lv_obj_set_style_pad_all(edwrap, 0, 0);
    lv_obj_remove_flag(edwrap, LV_OBJ_FLAG_SCROLLABLE);
    hmi_editor_create(&g_ui.ed_curve, edwrap);
    hmi_corr_select(0);
}

/* Valor que esta valendo agora (telemetria) e o sensor que a curva usa. */
static void now_of(uint8_t idx, char *val, size_t vn, char *src, size_t sn)
{
    const ecu_telemetry_t *t = &g_ui.link->tlm;
    switch (CURVES[idx]) {
    case ECU_TBL_WARMUP:
        hmi_fmt_with_unit(val, vn, ECU_UNIT_PCT_X10, t->corr_warm_x10, true);
        hmi_fmt_with_unit(src, sn, ECU_UNIT_DEGC_X10, t->clt_x10, false);
        break;
    case ECU_TBL_IAT:
        hmi_fmt_with_unit(val, vn, ECU_UNIT_PCT_X10, t->corr_iat_x10, true);
        hmi_fmt_with_unit(src, sn, ECU_UNIT_DEGC_X10, t->iat_x10, false);
        break;
    case ECU_TBL_CRANK:
        hmi_fmt_with_unit(val, vn, ECU_UNIT_PCT_X10, t->corr_crank_x10, true);
        snprintf(src, sn, "%s", (t->flags & ECU_TLM_CRANKING) ? "em partida" : "fora da partida");
        break;
    case ECU_TBL_DEADTIME:
        hmi_fmt_with_unit(val, vn, ECU_UNIT_US, t->dead_time_us, false);
        hmi_fmt_with_unit(src, sn, ECU_UNIT_VOLT_X10, (t->vbat_mv + 50) / 100, false);
        break;
    default:
        hmi_fmt_with_unit(val, vn, ECU_UNIT_PCT_X10, t->corr_accel_x10, true);
        snprintf(src, sn, "TPS %u %%", t->tps_pct);
        break;
    }
}

void hmi_corr_refresh(void)
{
    const ecu_hmi_link_t *l = g_ui.link;
    const bool alive = ecu_hmi_link_alive(l, g_ui.now);
    char val[32], src[32], buf[128];
    for (uint8_t i = 0; i < 5; i++) {
        if (alive) {
            now_of(i, val, sizeof(val), src, sizeof(src));
            snprintf(buf, sizeof(buf), "agora: %s", val);
        } else {
            snprintf(buf, sizeof(buf), "agora: --");
        }
        lv_label_set_text(g_ui.corr_btn_lbl[i], buf);
    }
    if (alive) {
        now_of(g_ui.corr_sel, val, sizeof(val), src, sizeof(src));
        snprintf(buf, sizeof(buf), "Valendo agora: %s (%s)", val, src);
    } else {
        snprintf(buf, sizeof(buf), "Valendo agora: sem telemetria");
    }
    lv_label_set_text(g_ui.corr_now, buf);

    /* ponto da curva em que o sensor esta agora (laranja), como na grade principal */
    const uint8_t tid = CURVES[g_ui.corr_sel];
    if (alive && l->tables[tid].info_ok) {
        int32_t q;
        int32_t scale = 10;
        switch (tid) {
        case ECU_TBL_IAT: q = l->tlm.iat_x10; break;
        case ECU_TBL_DEADTIME: q = l->tlm.vbat_mv; scale = 100; break;
        case ECU_TBL_ACCEL: q = 0; scale = 1; break;
        default: q = l->tlm.clt_x10; break;
        }
        hmi_grid_set_current(&g_ui.curve, 0, ecu_axis_nearest(l->tables[tid].info.x, ECU_CURVE_POINTS, scale, q));
    } else {
        hmi_grid_set_current(&g_ui.curve, -1, -1);
    }

    if (g_ui.tables_dirty) {
        hmi_grid_refresh(&g_ui.curve);
        const ecu_hmi_table_t *t = &l->tables[CURVES[g_ui.corr_sel]];
        const ecu_table_desc_t *d = ecu_table_desc(CURVES[g_ui.corr_sel]);
        int32_t mn = 0, mx = 1;
        for (uint8_t c = 0; c < ECU_CURVE_POINTS; c++) {
            if (t->v[0][c] < mn) mn = t->v[0][c];
            if (t->v[0][c] > mx) mx = t->v[0][c];
        }
        const int32_t pad = (mx - mn) / 10 + d->step;
        lv_chart_set_range(g_ui.corr_chart, LV_CHART_AXIS_PRIMARY_Y, mn - pad, mx + pad);
        for (uint8_t c = 0; c < ECU_CURVE_POINTS; c++) {
            lv_chart_set_value_by_id(g_ui.corr_chart, g_ui.corr_ser, c, t->v[0][c]);
        }
        lv_chart_refresh(g_ui.corr_chart);
    }
}
