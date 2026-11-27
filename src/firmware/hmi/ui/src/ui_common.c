/* Pecas reutilizadas pelas telas: paineis, barra de edicao, grade e teclado. */
#include <stdio.h>
#include <string.h>

#include "hmi/ui_internal.h"

/* ------------------------------------------------------------------------ */
/* Blocos basicos                                                            */
/* ------------------------------------------------------------------------ */

lv_obj_t *hmi_panel(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_style_bg_color(o, HMI_C_PANEL, 0);
    lv_obj_set_style_border_color(o, HMI_C_BORDER, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_radius(o, 6, 0);
    lv_obj_set_style_pad_all(o, 6, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

lv_obj_t *hmi_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, text);
    return l;
}

lv_obj_t *hmi_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_style_bg_color(b, HMI_C_PANEL2, 0);
    lv_obj_set_style_bg_color(b, HMI_C_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(b, HMI_C_BORDER, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_pad_hor(b, 10, 0);
    lv_obj_set_style_pad_ver(b, 6, 0);
    lv_obj_t *l = hmi_label(b, &hmi_font_16, HMI_C_TEXT, text);
    lv_obj_center(l);
    if (cb) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    }
    return b;
}

/* ------------------------------------------------------------------------ */
/* Barra de edicao                                                           */
/* ------------------------------------------------------------------------ */

static void editor_limits(hmi_editor_t *ed)
{
    const ecu_hmi_link_t *l = g_ui.link;
    if (ed->kind == HMI_ED_CELL) {
        const ecu_table_info_t *i = &l->tables[ed->table].info;
        const ecu_table_desc_t *d = ecu_table_desc(ed->table);
        const bool ok = l->tables[ed->table].info_ok;
        ed->unit = ok ? i->value_unit : d->value_unit;
        ed->min = ok ? i->min : d->min;
        ed->max = ok ? i->max : d->max;
        ed->step = ok ? i->step : d->step;
    } else {
        const ecu_param_t *p = &l->params[ed->param].p;
        const ecu_param_desc_t *d = ecu_param_desc(ed->param);
        const bool ok = l->params[ed->param].ok;
        ed->unit = ok ? p->unit : d->unit;
        ed->min = ok ? p->min : d->min;
        ed->max = ok ? p->max : d->max;
        ed->step = ok ? p->step : d->step;
    }
    if (ed->step <= 0) ed->step = 1;
}

static int32_t editor_motor_value(const hmi_editor_t *ed)
{
    const ecu_hmi_link_t *l = g_ui.link;
    if (ed->kind == HMI_ED_CELL) {
        return l->tables[ed->table].v[ed->row][ed->col];
    }
    return l->params[ed->param].p.value;
}

static void axis_text(char *buf, size_t n, uint8_t unit, int32_t v)
{
    hmi_fmt_with_unit(buf, n, unit, v, false);
}

static void editor_title(const hmi_editor_t *ed, char *buf, size_t n)
{
    const ecu_hmi_link_t *l = g_ui.link;
    if (ed->kind == HMI_ED_PARAM) {
        snprintf(buf, n, "%s", ecu_param_desc(ed->param)->name);
        return;
    }
    const ecu_table_info_t *i = &l->tables[ed->table].info;
    char x[24], y[24];
    axis_text(x, sizeof(x), i->x_unit, i->x[ed->col]);
    if (i->rows > 1) {
        axis_text(y, sizeof(y), i->y_unit, i->y[ed->row]);
        snprintf(buf, n, "%s × %s", x, y);
    } else {
        snprintf(buf, n, "Ponto: %s", x);
    }
}

void hmi_editor_refresh(hmi_editor_t *ed)
{
    char buf[160], v[32], a[32], b[32];
    if (!ed->has_target) {
        lv_label_set_text(ed->title, ed->kind == HMI_ED_PARAM ? "Toque em um item" : "Toque em uma célula");
        lv_label_set_text(ed->value, "--");
        lv_label_set_text(ed->status, "Escolha o que editar.");
        lv_obj_set_style_text_color(ed->status, HMI_C_MUTED, 0);
        return;
    }
    char t[64], cur[32];
    editor_title(ed, t, sizeof(t));
    hmi_fmt_with_unit(cur, sizeof(cur), ed->unit, ed->current, ed->unit == ECU_UNIT_PCT_X10);
    snprintf(buf, sizeof(buf), "%s\nvalendo no ESP: %s", t, cur);
    lv_label_set_text(ed->title, buf);

    hmi_fmt_with_unit(v, sizeof(v), ed->unit, ed->pending, ed->unit == ECU_UNIT_PCT_X10);
    lv_label_set_text(ed->value, v);
    lv_obj_set_style_text_color(ed->value, ed->pending != ed->current ? HMI_C_ACCENT : HMI_C_TEXT, 0);

    hmi_fmt_with_unit(a, sizeof(a), ed->unit, ed->min, false);
    hmi_fmt_with_unit(b, sizeof(b), ed->unit, ed->max, false);
    const bool out = ed->pending < ed->min || ed->pending > ed->max;
    if (ed->waiting) {
        lv_label_set_text(ed->status, "Aguardando o ESP do motor…");
        lv_obj_set_style_text_color(ed->status, HMI_C_WARN, 0);
    } else if (ed->last_status == ECU_OK && ed->pending == ed->current) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_OK " Confirmado pelo ESP: %s", cur);
        lv_label_set_text(ed->status, buf);
        lv_obj_set_style_text_color(ed->status, HMI_C_OK, 0);
    } else if (ed->last_status != 0xFF && ed->last_status != ECU_OK && ed->pending == ed->current) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_CLOSE " Recusado pelo ESP: %s. Mantido %s", ecu_status_text(ed->last_status),
                 cur);
        lv_label_set_text(ed->status, buf);
        lv_obj_set_style_text_color(ed->status, HMI_C_DANGER, 0);
    } else if (out) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " Fora do limite (%s a %s): o ESP vai recusar", a, b);
        lv_label_set_text(ed->status, buf);
        lv_obj_set_style_text_color(ed->status, HMI_C_WARN, 0);
    } else {
        snprintf(buf, sizeof(buf), "Limite: %s a %s", a, b);
        lv_label_set_text(ed->status, buf);
        lv_obj_set_style_text_color(ed->status, HMI_C_MUTED, 0);
    }
}

void hmi_editor_set_pending(hmi_editor_t *ed, int32_t v)
{
    if (v < -32768) v = -32768;
    if (v > 32767) v = 32767;
    ed->pending = v;
    ed->last_status = 0xFF;
    hmi_editor_refresh(ed);
}

static void ed_minus(lv_event_t *e)
{
    hmi_editor_t *ed = (hmi_editor_t *)lv_event_get_user_data(e);
    if (!ed->has_target || ed->waiting) return;
    int32_t v = ed->pending - ed->step;
    if (v < ed->min) v = ed->min;
    hmi_editor_set_pending(ed, v);
}

static void ed_plus(lv_event_t *e)
{
    hmi_editor_t *ed = (hmi_editor_t *)lv_event_get_user_data(e);
    if (!ed->has_target || ed->waiting) return;
    int32_t v = ed->pending + ed->step;
    if (v > ed->max) v = ed->max;
    hmi_editor_set_pending(ed, v);
}

static void ed_keypad(lv_event_t *e)
{
    hmi_editor_t *ed = (hmi_editor_t *)lv_event_get_user_data(e);
    if (!ed->has_target || ed->waiting) return;
    hmi_keypad_open(ed);
}

static void ed_send(lv_event_t *e)
{
    hmi_editor_t *ed = (hmi_editor_t *)lv_event_get_user_data(e);
    if (!ed->has_target || ed->waiting) return;
    bool ok;
    if (ed->kind == HMI_ED_CELL) {
        ok = ecu_hmi_link_write_cell(g_ui.link, ed->table, ed->row, ed->col, (int16_t)ed->pending);
    } else {
        ok = ecu_hmi_link_write_param(g_ui.link, ed->param, (int16_t)ed->pending);
    }
    if (ok) {
        ed->waiting = true;
    }
    hmi_editor_refresh(ed);
}

void hmi_editor_create(hmi_editor_t *ed, lv_obj_t *parent)
{
    ed->last_status = 0xFF;
    ed->bar = hmi_panel(parent);
    lv_obj_set_size(ed->bar, LV_PCT(100), 58);
    lv_obj_set_style_pad_all(ed->bar, 4, 0);
    lv_obj_set_flex_flow(ed->bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ed->bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ed->bar, 6, 0);

    ed->title = hmi_label(ed->bar, &hmi_font_14, HMI_C_MUTED, "");
    lv_obj_set_width(ed->title, 176);
    lv_label_set_long_mode(ed->title, LV_LABEL_LONG_WRAP);

    ed->minus = hmi_button(ed->bar, LV_SYMBOL_MINUS, ed_minus, ed);
    lv_obj_set_size(ed->minus, 42, 44);
    ed->value = hmi_label(ed->bar, &hmi_font_20, HMI_C_TEXT, "--");
    lv_obj_set_width(ed->value, 96);
    lv_obj_set_style_text_align(ed->value, LV_TEXT_ALIGN_CENTER, 0);
    ed->plus = hmi_button(ed->bar, LV_SYMBOL_PLUS, ed_plus, ed);
    lv_obj_set_size(ed->plus, 42, 44);
    ed->keypad = hmi_button(ed->bar, LV_SYMBOL_KEYBOARD, ed_keypad, ed);
    lv_obj_set_size(ed->keypad, 48, 44);
    ed->send = hmi_button(ed->bar, "Enviar", ed_send, ed);
    lv_obj_set_size(ed->send, 78, 44);
    lv_obj_set_style_bg_color(ed->send, lv_color_hex(0x1D4E89), 0);

    ed->status = hmi_label(ed->bar, &hmi_font_14, HMI_C_MUTED, "");
    lv_obj_set_flex_grow(ed->status, 1);
    lv_label_set_long_mode(ed->status, LV_LABEL_LONG_WRAP);
    hmi_editor_refresh(ed);
}

void hmi_editor_target_cell(hmi_editor_t *ed, uint8_t table, uint8_t row, uint8_t col)
{
    ed->kind = HMI_ED_CELL;
    ed->has_target = true;
    ed->table = table;
    ed->row = row;
    ed->col = col;
    ed->waiting = false;
    ed->last_status = 0xFF;
    editor_limits(ed);
    ed->current = editor_motor_value(ed);
    ed->pending = ed->current;
    hmi_editor_refresh(ed);
}

void hmi_editor_target_param(hmi_editor_t *ed, uint8_t param)
{
    ed->kind = HMI_ED_PARAM;
    ed->has_target = true;
    ed->param = param;
    ed->waiting = false;
    ed->last_status = 0xFF;
    editor_limits(ed);
    ed->current = editor_motor_value(ed);
    ed->pending = ed->current;
    hmi_editor_refresh(ed);
}

void hmi_editor_on_ack(hmi_editor_t *ed, uint8_t status, int16_t value)
{
    ed->waiting = false;
    ed->last_status = status;
    ed->current = value;
    ed->pending = value; /* aceito: o novo; recusado: o que continua valendo */
    hmi_editor_refresh(ed);
}

void hmi_editor_on_timeout(hmi_editor_t *ed)
{
    ed->waiting = false;
    ed->last_status = 0xFF;
    hmi_editor_refresh(ed);
    lv_label_set_text(ed->status, LV_SYMBOL_WARNING " Sem resposta do ESP do motor (3 tentativas)");
    lv_obj_set_style_text_color(ed->status, HMI_C_DANGER, 0);
}

/* ------------------------------------------------------------------------ */
/* Grade                                                                     */
/* ------------------------------------------------------------------------ */

static uint8_t grid_rows(const hmi_grid_t *g)
{
    const ecu_hmi_table_t *t = &g_ui.link->tables[g->tid];
    return t->info_ok ? t->info.rows : ecu_table_desc(g->tid)->rows;
}

static uint8_t grid_cols(const hmi_grid_t *g)
{
    const ecu_hmi_table_t *t = &g_ui.link->tables[g->tid];
    return t->info_ok ? t->info.cols : ecu_table_desc(g->tid)->cols;
}

void hmi_grid_cell_to_view(const hmi_grid_t *g, int row, int col, uint32_t *vrow, uint32_t *vcol)
{
    const uint8_t rows = grid_rows(g);
    *vrow = (uint32_t)(g->flip_rows ? rows - row : row + 1);
    *vcol = (uint32_t)(col + 1);
}

static bool view_to_cell(const hmi_grid_t *g, uint32_t vrow, uint32_t vcol, int *row, int *col)
{
    const uint8_t rows = grid_rows(g);
    if (vrow == 0 || vcol == 0 || vrow > rows || vcol > grid_cols(g)) {
        return false;
    }
    *row = g->flip_rows ? (int)rows - (int)vrow : (int)vrow - 1;
    *col = (int)vcol - 1;
    return true;
}

static lv_color_t heat(const hmi_grid_t *g, int16_t v)
{
    int32_t span = g->vmax - g->vmin;
    int32_t t = span > 0 ? (int32_t)(v - g->vmin) * 1000 / span : 500;
    if (t < 0) t = 0;
    if (t > 1000) t = 1000;
    const uint16_t hue = (uint16_t)(225 - 225 * t / 1000); /* azul (baixo) -> vermelho (alto) */
    return lv_color_hsv_to_rgb(hue, 70, 48);
}

static void grid_draw_cb(lv_event_t *e)
{
    hmi_grid_t *g = (hmi_grid_t *)lv_event_get_user_data(e);
    lv_draw_task_t *task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t *base = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(task);
    if (base->part != LV_PART_ITEMS) {
        return;
    }
    lv_draw_fill_dsc_t *fill = lv_draw_task_get_fill_dsc(task);
    lv_draw_label_dsc_t *label = lv_draw_task_get_label_dsc(task);
    int row, col;
    if (!view_to_cell(g, base->id1, base->id2, &row, &col)) {
        if (fill) {
            fill->color = HMI_C_PANEL2;
            fill->opa = LV_OPA_COVER;
        }
        if (label) {
            label->color = HMI_C_MUTED;
        }
        return;
    }
    const int16_t v = g_ui.link->tables[g->tid].v[row][col];
    lv_color_t bg = heat(g, v);
    lv_color_t fg = lv_color_white();
    if (row == g->cur_row && col == g->cur_col) {
        bg = HMI_C_CURRENT; /* onde o motor esta agora */
        fg = lv_color_black();
    }
    if (row == g->sel_row && col == g->sel_col) {
        bg = lv_color_white(); /* a celula escolhida para editar */
        fg = lv_color_black();
    }
    if (fill) {
        fill->color = bg;
        fill->opa = LV_OPA_COVER;
    }
    if (label) {
        label->color = fg;
    }
}

static void grid_click_cb(lv_event_t *e)
{
    hmi_grid_t *g = (hmi_grid_t *)lv_event_get_user_data(e);
    uint32_t vr, vc;
    lv_table_get_selected_cell(g->table, &vr, &vc);
    int row, col;
    if (vr == LV_TABLE_CELL_NONE || !view_to_cell(g, vr, vc, &row, &col)) {
        return;
    }
    hmi_grid_select(g, row, col);
}

void hmi_grid_select(hmi_grid_t *g, int row, int col)
{
    g->sel_row = row;
    g->sel_col = col;
    if (g->ed) {
        hmi_editor_target_cell(g->ed, g->tid, (uint8_t)row, (uint8_t)col);
    }
    lv_obj_invalidate(g->table);
}

void hmi_grid_create(hmi_grid_t *g, lv_obj_t *parent, uint8_t tid, hmi_editor_t *ed, int32_t w)
{
    g->tid = tid;
    g->ed = ed;
    g->sel_row = g->sel_col = -1;
    g->cur_row = g->cur_col = -1;
    g->table = lv_table_create(parent);
    lv_obj_set_style_bg_color(g->table, HMI_C_PANEL, 0);
    lv_obj_set_style_border_width(g->table, 0, 0);
    lv_obj_set_style_pad_all(g->table, 0, 0);
    lv_obj_set_style_text_font(g->table, &hmi_font_14, LV_PART_ITEMS);
    lv_obj_set_style_text_color(g->table, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_text_align(g->table, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
    lv_obj_set_style_pad_ver(g->table, 2, LV_PART_ITEMS);
    lv_obj_set_style_pad_hor(g->table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_width(g->table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(g->table, HMI_C_BG, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g->table, HMI_C_PANEL2, LV_PART_ITEMS);
    lv_obj_remove_flag(g->table, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g->table, grid_draw_cb, LV_EVENT_DRAW_TASK_ADDED, g);
    lv_obj_add_event_cb(g->table, grid_click_cb, LV_EVENT_VALUE_CHANGED, g);
    lv_obj_add_flag(g->table, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);

    const ecu_table_desc_t *d = ecu_table_desc(tid);
    lv_table_set_row_count(g->table, (uint32_t)d->rows + 1u);
    lv_table_set_column_count(g->table, (uint32_t)d->cols + 1u);
    const int32_t first = d->rows > 1 ? 44 : 64;
    const int32_t cw = (w - first) / d->cols;
    lv_table_set_column_width(g->table, 0, first);
    for (uint32_t c = 1; c <= d->cols; c++) {
        lv_table_set_column_width(g->table, c, cw);
    }
    hmi_grid_refresh(g);
}

void hmi_grid_set_table(hmi_grid_t *g, uint8_t tid)
{
    g->tid = tid;
    g->sel_row = g->sel_col = -1;
    g->cur_row = g->cur_col = -1;
    hmi_grid_refresh(g);
}

void hmi_grid_refresh(hmi_grid_t *g)
{
    const ecu_hmi_table_t *t = &g_ui.link->tables[g->tid];
    const ecu_table_desc_t *d = ecu_table_desc(g->tid);
    const uint8_t rows = d->rows, cols = d->cols;
    char buf[16];
    const uint8_t vu = t->info_ok ? t->info.value_unit : d->value_unit;
    const uint8_t xu = t->info_ok ? t->info.x_unit : d->x_unit;

    if (rows > 1) {
        lv_table_set_cell_value(g->table, 0, 0, "kPa");
    } else {
        lv_table_set_cell_value(g->table, 0, 0, hmi_unit_suffix(xu));
        lv_table_set_cell_value(g->table, 1, 0, hmi_unit_suffix(vu));
    }
    g->vmin = 32767;
    g->vmax = -32768;
    for (uint8_t c = 0; c < cols; c++) {
        if (t->info_ok) {
            hmi_fmt_value(buf, sizeof(buf), xu, t->info.x[c], false);
        } else {
            snprintf(buf, sizeof(buf), "-");
        }
        lv_table_set_cell_value(g->table, 0, (uint32_t)c + 1u, buf);
    }
    for (uint8_t r = 0; r < rows; r++) {
        uint32_t vr, vc;
        hmi_grid_cell_to_view(g, r, 0, &vr, &vc);
        if (rows > 1) {
            if (t->info_ok) {
                hmi_fmt_value(buf, sizeof(buf), t->info.y_unit, t->info.y[r], false);
            } else {
                snprintf(buf, sizeof(buf), "-");
            }
            lv_table_set_cell_value(g->table, vr, 0, buf);
        }
        for (uint8_t c = 0; c < cols; c++) {
            const bool loaded = (t->rows_ok >> r) & 1u;
            const int16_t v = t->v[r][c];
            if (loaded) {
                hmi_fmt_value(buf, sizeof(buf), vu, v, false);
                if (v < g->vmin) g->vmin = v;
                if (v > g->vmax) g->vmax = v;
            } else {
                snprintf(buf, sizeof(buf), "…");
            }
            lv_table_set_cell_value(g->table, vr, (uint32_t)c + 1u, buf);
        }
    }
    lv_obj_invalidate(g->table);
}

void hmi_grid_set_current(hmi_grid_t *g, int row, int col)
{
    if (row != g->cur_row || col != g->cur_col) {
        g->cur_row = row;
        g->cur_col = col;
        lv_obj_invalidate(g->table);
    }
}

/* ------------------------------------------------------------------------ */
/* Teclado numerico                                                          */
/* ------------------------------------------------------------------------ */

static void keypad_close(void)
{
    lv_obj_add_flag(g_ui.kb_modal, LV_OBJ_FLAG_HIDDEN);
    g_ui.kb_target = NULL;
}

static void keypad_event(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY && g_ui.kb_target) {
        int32_t v;
        if (hmi_parse_value(lv_textarea_get_text(g_ui.kb_ta), g_ui.kb_target->unit, &v)) {
            hmi_editor_set_pending(g_ui.kb_target, v);
            keypad_close();
        } else {
            lv_label_set_text(g_ui.kb_hint, "Número inválido");
        }
    } else if (code == LV_EVENT_CANCEL) {
        keypad_close();
    }
}

void hmi_keypad_create(void)
{
    lv_obj_t *m = lv_obj_create(lv_layer_top());
    g_ui.kb_modal = m;
    lv_obj_set_size(m, HMI_SCREEN_W, HMI_SCREEN_H);
    lv_obj_set_pos(m, 0, 0);
    lv_obj_set_style_bg_color(m, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(m, LV_OPA_60, 0);
    lv_obj_set_style_border_width(m, 0, 0);
    lv_obj_set_style_radius(m, 0, 0);
    lv_obj_remove_flag(m, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *box = hmi_panel(m);
    lv_obj_set_size(box, 420, 330);
    lv_obj_center(box);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 6, 0);
    g_ui.kb_hint = hmi_label(box, &hmi_font_16, HMI_C_MUTED, "");
    g_ui.kb_ta = lv_textarea_create(box);
    lv_textarea_set_one_line(g_ui.kb_ta, true);
    lv_textarea_set_accepted_chars(g_ui.kb_ta, "0123456789.,-+");
    lv_obj_set_width(g_ui.kb_ta, LV_PCT(100));
    lv_obj_set_style_text_font(g_ui.kb_ta, &hmi_font_20, 0);
    g_ui.kb = lv_keyboard_create(box);
    lv_keyboard_set_mode(g_ui.kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(g_ui.kb, g_ui.kb_ta);
    lv_obj_set_size(g_ui.kb, LV_PCT(100), 220);
    lv_obj_add_event_cb(g_ui.kb, keypad_event, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_ui.kb, keypad_event, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(m, LV_OBJ_FLAG_HIDDEN);
}

void hmi_keypad_open(hmi_editor_t *ed)
{
    char v[24], a[24], b[24], buf[96];
    g_ui.kb_target = ed;
    hmi_fmt_value(v, sizeof(v), ed->unit, ed->pending, false);
    hmi_fmt_with_unit(a, sizeof(a), ed->unit, ed->min, false);
    hmi_fmt_with_unit(b, sizeof(b), ed->unit, ed->max, false);
    snprintf(buf, sizeof(buf), "Novo valor (%s). Limite: %s a %s", hmi_unit_suffix(ed->unit), a, b);
    lv_label_set_text(g_ui.kb_hint, buf);
    lv_textarea_set_text(g_ui.kb_ta, v);
    lv_obj_remove_flag(g_ui.kb_modal, LV_OBJ_FLAG_HIDDEN);
}
