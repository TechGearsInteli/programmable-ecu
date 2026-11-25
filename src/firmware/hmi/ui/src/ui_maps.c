/* Mapas de combustivel e ignicao: celula atual (1.6.1.2), edicao com
 * confirmacao (1.6.1.3), grade de combustivel (1.6.1.4) e de ignicao (1.6.1.5). */
#include <stdio.h>

#include "hmi/ui_internal.h"

static void build(lv_obj_t *parent, hmi_grid_t *g, hmi_editor_t *ed, uint8_t tid, lv_obj_t **info)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 4, 0);
    *info = hmi_label(parent, &hmi_font_14, HMI_C_MUTED, "");
    lv_obj_set_width(*info, LV_PCT(100));
    lv_obj_set_height(*info, 18);
    lv_label_set_long_mode(*info, LV_LABEL_LONG_CLIP);
    hmi_grid_create(g, parent, tid, ed, HMI_CONTENT_W - 8);
    g->flip_rows = true;
    hmi_editor_create(ed, parent);
}

void hmi_maps_create(lv_obj_t *fuel_parent, lv_obj_t *ign_parent)
{
    build(fuel_parent, &g_ui.fuel, &g_ui.ed_fuel, ECU_TBL_FUEL, &g_ui.fuel_info);
    build(ign_parent, &g_ui.ign, &g_ui.ed_ign, ECU_TBL_IGN, &g_ui.ign_info);
}

static void info_line(lv_obj_t *lbl, const char *title, uint8_t unit, int row, int col, int16_t cell, int32_t interp)
{
    const ecu_hmi_link_t *l = g_ui.link;
    char buf[200], c[24], i[24], m[24];
    if (!ecu_hmi_link_alive(l, g_ui.now)) {
        snprintf(buf, sizeof(buf), "%s · sem telemetria do ESP do motor", title);
    } else {
        hmi_fmt_with_unit(c, sizeof(c), unit, cell, false);
        hmi_fmt_with_unit(i, sizeof(i), unit, interp, false);
        hmi_fmt_value(m, sizeof(m), ECU_UNIT_PCT_X10, l->tlm.map_kpa_x10, false);
        snprintf(buf, sizeof(buf), "%s · Agora: %u rpm, %s kPa → célula [%d,%d] = %s · interpolado %s", title,
                 l->tlm.rpm, m, row, col, c, i);
    }
    lv_label_set_text(lbl, buf);
}

void hmi_maps_refresh(void)
{
    const ecu_hmi_link_t *l = g_ui.link;
    if (g_ui.tables_dirty) {
        hmi_grid_refresh(&g_ui.fuel);
        hmi_grid_refresh(&g_ui.ign);
    }
    const bool alive = ecu_hmi_link_alive(l, g_ui.now);
    hmi_grid_set_current(&g_ui.fuel, alive ? l->tlm.fuel_row : -1, alive ? l->tlm.fuel_col : -1);
    hmi_grid_set_current(&g_ui.ign, alive ? l->tlm.ign_row : -1, alive ? l->tlm.ign_col : -1);
    if (g_ui.current == HMI_SCR_FUEL) {
        info_line(g_ui.fuel_info, "Combustível (ms)", ECU_UNIT_MS_X100, l->tlm.fuel_row, l->tlm.fuel_col,
                  l->tlm.fuel_cell, l->tlm.fuel_interp_x100);
    } else if (g_ui.current == HMI_SCR_IGN) {
        info_line(g_ui.ign_info, "Ignição (graus)", ECU_UNIT_DEG_X10, l->tlm.ign_row, l->tlm.ign_col,
                  l->tlm.ign_cell, l->tlm.advance_x10);
    }
}
