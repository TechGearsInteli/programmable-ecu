/* Painel de instrumentos (1.7.1.1 / 1.7.1.2) e alertas (1.7.1.3). */
#include <stdio.h>

#include "hmi/ui_internal.h"

#define RPM_MAX 8000

static lv_obj_t *rpm_unit;

static const char *const TILE_NAME[8] = {
    "Tempo de injeção", "Avanço de ignição", "Carga (MAP)", "Borboleta (TPS)",
    "Motor (CLT)", "Ar (IAT)", "Bateria", "Célula de comb.",
};

void hmi_painel_create(lv_obj_t *parent)
{
    g_ui.p_alert = lv_obj_create(parent);
    lv_obj_set_size(g_ui.p_alert, LV_PCT(100), 40);
    lv_obj_set_pos(g_ui.p_alert, 0, 0);
    lv_obj_set_style_bg_color(g_ui.p_alert, HMI_C_PANEL, 0);
    lv_obj_set_style_border_width(g_ui.p_alert, 0, 0);
    lv_obj_set_style_radius(g_ui.p_alert, 6, 0);
    lv_obj_remove_flag(g_ui.p_alert, LV_OBJ_FLAG_SCROLLABLE);
    g_ui.p_alert_lbl = hmi_label(g_ui.p_alert, &hmi_font_16, HMI_C_MUTED, "");
    lv_obj_center(g_ui.p_alert_lbl);

    lv_obj_t *rpm = hmi_panel(parent);
    lv_obj_set_size(rpm, LV_PCT(100), 132);
    lv_obj_set_pos(rpm, 0, 46);
    lv_obj_t *t = hmi_label(rpm, &hmi_font_14, HMI_C_MUTED, "Rotação");
    lv_obj_set_pos(t, 4, 0);
    g_ui.p_rpm = hmi_label(rpm, &lv_font_montserrat_48, HMI_C_TEXT, "0");
    lv_obj_set_pos(g_ui.p_rpm, 4, 18);
    rpm_unit = hmi_label(rpm, &hmi_font_20, HMI_C_MUTED, "rpm");
    lv_obj_align_to(rpm_unit, g_ui.p_rpm, LV_ALIGN_OUT_RIGHT_BOTTOM, 10, -8);
    g_ui.p_sync = hmi_label(rpm, &hmi_font_16, HMI_C_MUTED, "");
    lv_obj_align(g_ui.p_sync, LV_ALIGN_TOP_RIGHT, -4, 0);

    g_ui.p_rpm_bar = lv_bar_create(rpm);
    lv_obj_set_size(g_ui.p_rpm_bar, LV_PCT(100), 22);
    lv_obj_align(g_ui.p_rpm_bar, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_bar_set_range(g_ui.p_rpm_bar, 0, RPM_MAX);
    lv_obj_set_style_bg_color(g_ui.p_rpm_bar, HMI_C_PANEL2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_ui.p_rpm_bar, HMI_C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(g_ui.p_rpm_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(g_ui.p_rpm_bar, 4, LV_PART_INDICATOR);

    const int32_t tw = (HMI_CONTENT_W - 8 - 3 * 6) / 4;
    for (int i = 0; i < 8; i++) {
        lv_obj_t *tile = hmi_panel(parent);
        lv_obj_set_size(tile, tw, 96);
        lv_obj_set_pos(tile, (i % 4) * (tw + 6), 184 + (i / 4) * 102);
        lv_obj_t *n = hmi_label(tile, &hmi_font_14, HMI_C_MUTED, TILE_NAME[i]);
        lv_obj_set_pos(n, 2, 0);
        g_ui.p_val[i] = hmi_label(tile, &hmi_font_28, HMI_C_TEXT, "--");
        lv_obj_align(g_ui.p_val[i], LV_ALIGN_BOTTOM_LEFT, 2, -4);
    }

    lv_obj_t *corr = hmi_panel(parent);
    lv_obj_set_size(corr, LV_PCT(100), 46);
    lv_obj_set_pos(corr, 0, 390);
    g_ui.p_corr = hmi_label(corr, &hmi_font_14, HMI_C_MUTED, "");
    lv_obj_set_width(g_ui.p_corr, LV_PCT(100));
    lv_label_set_long_mode(g_ui.p_corr, LV_LABEL_LONG_WRAP);
}

static void fmt_x10(char *buf, size_t n, int32_t v, const char *unit)
{
    hmi_fmt_value(buf, n, ECU_UNIT_PCT_X10, v, false); /* qualquer unidade com 1 casa */
    size_t len = 0;
    while (buf[len]) len++;
    snprintf(buf + len, n - len, " %s", unit);
}

void hmi_painel_refresh(void)
{
    const ecu_hmi_link_t *l = g_ui.link;
    const bool alive = ecu_hmi_link_alive(l, g_ui.now);
    const ecu_telemetry_t *t = &l->tlm;
    char buf[320], a[24], b[24], c[24], d[24], e[24];

    /* Alertas: regra escrita em docs/hmi-s3.md. Os limites moram na ficha do
     * motor (ESP do motor); a tela so compara e acende o aviso. Nao mexe no mapa. */
    const bool have_limits = l->params[ECU_PAR_ALERT_RPM].ok && l->params[ECU_PAR_ALERT_CLT_X10].ok;
    const int32_t lim_rpm = l->params[ECU_PAR_ALERT_RPM].p.value;
    const int32_t lim_clt = l->params[ECU_PAR_ALERT_CLT_X10].p.value;
    /* Sem sincronismo o pacote pode trazer o ultimo giro. Nao usar esse numero. */
    const bool rpm_trusted = (t->flags & (ECU_TLM_SYNC | ECU_TLM_CRANKING)) != 0;
    const uint16_t rpm_show = rpm_trusted ? t->rpm : 0;
    g_ui.alert_rpm = alive && rpm_trusted && have_limits && t->rpm > lim_rpm;
    g_ui.alert_clt = alive && have_limits && t->clt_x10 > lim_clt;

    if (g_ui.alert_rpm || g_ui.alert_clt) {
        char r[80] = "", k[80] = "";
        if (g_ui.alert_rpm) {
            snprintf(r, sizeof(r), "Rotação acima do limite (%u > %ld rpm)", rpm_show, (long)lim_rpm);
        }
        if (g_ui.alert_clt) {
            hmi_fmt_with_unit(a, sizeof(a), ECU_UNIT_DEGC_X10, t->clt_x10, false);
            hmi_fmt_with_unit(b, sizeof(b), ECU_UNIT_DEGC_X10, lim_clt, false);
            snprintf(k, sizeof(k), "Motor quente (%s > %s)", a, b);
        }
        snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " %s%s%s", r, (r[0] && k[0]) ? "  ·  " : "", k);
        lv_label_set_text(g_ui.p_alert_lbl, buf);
        lv_obj_set_style_bg_color(g_ui.p_alert, HMI_C_DANGER, 0);
        lv_obj_set_style_text_color(g_ui.p_alert_lbl, lv_color_white(), 0);
    } else {
        if (have_limits) {
            hmi_fmt_with_unit(a, sizeof(a), ECU_UNIT_DEGC_X10, lim_clt, false);
            snprintf(buf, sizeof(buf), "Sem alertas · avisa acima de %ld rpm ou %s", (long)lim_rpm, a);
        } else {
            snprintf(buf, sizeof(buf), "Alertas: aguardando os limites da ficha do motor");
        }
        lv_label_set_text(g_ui.p_alert_lbl, buf);
        lv_obj_set_style_bg_color(g_ui.p_alert, HMI_C_PANEL, 0);
        lv_obj_set_style_text_color(g_ui.p_alert_lbl, HMI_C_MUTED, 0);
    }

    if (!alive) {
        lv_label_set_text(g_ui.p_rpm, "--");
        lv_bar_set_value(g_ui.p_rpm_bar, 0, LV_ANIM_OFF);
        lv_label_set_text(g_ui.p_sync, "Sem dados do ESP do motor");
        lv_obj_set_style_text_color(g_ui.p_sync, HMI_C_DANGER, 0);
        for (int i = 0; i < 8; i++) lv_label_set_text(g_ui.p_val[i], "--");
        lv_label_set_text(g_ui.p_corr, "Correções: aguardando telemetria");
        return;
    }

    lv_label_set_text_fmt(g_ui.p_rpm, "%u", rpm_show);
    lv_obj_align_to(rpm_unit, g_ui.p_rpm, LV_ALIGN_OUT_RIGHT_BOTTOM, 10, -8);
    lv_obj_set_style_text_color(g_ui.p_rpm, g_ui.alert_rpm ? HMI_C_DANGER : HMI_C_TEXT, 0);
    lv_bar_set_value(g_ui.p_rpm_bar, rpm_show > RPM_MAX ? RPM_MAX : rpm_show, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_ui.p_rpm_bar, g_ui.alert_rpm ? HMI_C_DANGER : HMI_C_ACCENT, LV_PART_INDICATOR);
    if (t->flags & ECU_TLM_CRANKING) {
        lv_label_set_text(g_ui.p_sync, LV_SYMBOL_REFRESH " Partida");
        lv_obj_set_style_text_color(g_ui.p_sync, HMI_C_WARN, 0);
    } else if (t->flags & ECU_TLM_SYNC) {
        lv_label_set_text(g_ui.p_sync, LV_SYMBOL_OK " Sincronizado");
        lv_obj_set_style_text_color(g_ui.p_sync, HMI_C_OK, 0);
    } else {
        lv_label_set_text(g_ui.p_sync, "Motor parado / sem sincronismo");
        lv_obj_set_style_text_color(g_ui.p_sync, HMI_C_MUTED, 0);
    }

    hmi_fmt_with_unit(buf, sizeof(buf), ECU_UNIT_MS_X100, (t->pw_us + 5) / 10, false);
    lv_label_set_text(g_ui.p_val[0], buf);
    hmi_fmt_with_unit(buf, sizeof(buf), ECU_UNIT_DEG_X10, t->advance_x10, false);
    lv_label_set_text(g_ui.p_val[1], buf);
    fmt_x10(buf, sizeof(buf), t->map_kpa_x10, "kPa");
    lv_label_set_text(g_ui.p_val[2], buf);
    lv_label_set_text_fmt(g_ui.p_val[3], "%u %%", t->tps_pct);
    hmi_fmt_with_unit(buf, sizeof(buf), ECU_UNIT_DEGC_X10, t->clt_x10, false);
    lv_label_set_text(g_ui.p_val[4], buf);
    lv_obj_set_style_text_color(g_ui.p_val[4], g_ui.alert_clt ? HMI_C_DANGER : HMI_C_TEXT, 0);
    hmi_fmt_with_unit(buf, sizeof(buf), ECU_UNIT_DEGC_X10, t->iat_x10, false);
    lv_label_set_text(g_ui.p_val[5], buf);
    hmi_fmt_with_unit(buf, sizeof(buf), ECU_UNIT_VOLT_X10, (t->vbat_mv + 50) / 100, false);
    lv_label_set_text(g_ui.p_val[6], buf);
    hmi_fmt_value(a, sizeof(a), ECU_UNIT_MS_X100, t->fuel_cell, false);
    snprintf(buf, sizeof(buf), "[%u,%u] %s", t->fuel_row, t->fuel_col, a);
    lv_label_set_text(g_ui.p_val[7], buf);

    hmi_fmt_with_unit(a, sizeof(a), ECU_UNIT_PCT_X10, t->corr_warm_x10, true);
    hmi_fmt_with_unit(b, sizeof(b), ECU_UNIT_PCT_X10, t->corr_iat_x10, true);
    hmi_fmt_with_unit(c, sizeof(c), ECU_UNIT_PCT_X10, t->corr_crank_x10, true);
    hmi_fmt_with_unit(d, sizeof(d), ECU_UNIT_PCT_X10, t->corr_accel_x10, true);
    hmi_fmt_value(e, sizeof(e), ECU_UNIT_MS_X100, t->fuel_interp_x100, false);
    snprintf(buf, sizeof(buf),
             "Correções valendo: aquecimento %s · ar %s · partida %s · aceleração %s · tempo morto %u µs · "
             "mapa interpolado %s ms",
             a, b, c, d, t->dead_time_us, e);
    lv_label_set_text(g_ui.p_corr, buf);
}
