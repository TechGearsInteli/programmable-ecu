/* Gravar na flash e backup em arquivo (1.6.1.8). */
#include <stdio.h>

#include "ecu/ecu_crc.h"
#include "hmi/ui_internal.h"

static uint8_t file_buf[ECU_CALIB_BLOB_MAX];

static void set_status(lv_color_t color, const char *text)
{
    lv_label_set_text(g_ui.s_status, text);
    lv_obj_set_style_text_color(g_ui.s_status, color, 0);
}

static void save_cb(lv_event_t *e)
{
    (void)e;
    if (ecu_hmi_link_save(g_ui.link)) {
        set_status(HMI_C_WARN, "Gravando na flash do ESP do motor…");
    }
}

static void export_cb(lv_event_t *e)
{
    (void)e;
    if (ecu_hmi_link_backup(g_ui.link)) {
        g_ui.s_last_job = ECU_JOB_BACKUP;
        set_status(HMI_C_WARN, "Lendo a calibração do ESP do motor…");
    } else {
        set_status(HMI_C_DANGER, "Não deu para começar: ESP desconectado ou ocupado.");
    }
}

static void import_cb(lv_event_t *e)
{
    (void)e;
    const hmi_platform_t *p = g_ui.plat;
    size_t n = (p && p->backup_read) ? p->backup_read(p->ctx, file_buf, sizeof(file_buf)) : 0;
    if (n == 0) {
        set_status(HMI_C_DANGER, "Nenhum arquivo de backup encontrado.");
        return;
    }
    if (ecu_hmi_link_restore(g_ui.link, file_buf, (uint16_t)n)) {
        g_ui.s_last_job = ECU_JOB_RESTORE;
        set_status(HMI_C_WARN, "Enviando o backup para o ESP do motor…");
    } else {
        set_status(HMI_C_DANGER, "Não deu para começar: ESP desconectado ou ocupado.");
    }
}

static lv_obj_t *big_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *b = hmi_button(parent, text, cb, NULL);
    lv_obj_set_size(b, 220, 56);
    return b;
}

void hmi_save_create(lv_obj_t *parent)
{
    lv_obj_t *card = hmi_panel(parent);
    lv_obj_set_size(card, LV_PCT(100), 96);
    lv_obj_set_pos(card, 0, 0);
    lv_obj_t *t = hmi_label(card, &hmi_font_14, HMI_C_MUTED, "Calibração no ESP do motor");
    lv_obj_set_pos(t, 0, 0);
    g_ui.s_state = hmi_label(card, &hmi_font_20, HMI_C_TEXT, "");
    lv_obj_set_pos(g_ui.s_state, 0, 22);
    lv_obj_t *rule = hmi_label(card, &hmi_font_14, HMI_C_MUTED,
                               "Edições valem na hora, mas ficam na RAM. Só o botão Gravar leva para a flash.");
    lv_obj_set_pos(rule, 0, 56);

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 70);
    lv_obj_set_pos(row, 0, 104);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    g_ui.s_save = big_button(row, LV_SYMBOL_SAVE "  Gravar na flash", save_cb);
    lv_obj_set_style_bg_color(g_ui.s_save, lv_color_hex(0x1D4E89), 0);
    g_ui.s_export = big_button(row, LV_SYMBOL_UPLOAD "  Exportar backup", export_cb);
    g_ui.s_import = big_button(row, LV_SYMBOL_DOWNLOAD "  Importar backup", import_cb);

    g_ui.s_bar = lv_bar_create(parent);
    lv_obj_set_size(g_ui.s_bar, LV_PCT(100), 14);
    lv_obj_set_pos(g_ui.s_bar, 0, 184);
    lv_bar_set_range(g_ui.s_bar, 0, 100);
    lv_obj_set_style_bg_color(g_ui.s_bar, HMI_C_PANEL2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_ui.s_bar, HMI_C_ACCENT, LV_PART_INDICATOR);

    g_ui.s_status = hmi_label(parent, &hmi_font_16, HMI_C_MUTED, "");
    lv_obj_set_pos(g_ui.s_status, 0, 206);
    lv_obj_set_width(g_ui.s_status, LV_PCT(100));
    lv_label_set_long_mode(g_ui.s_status, LV_LABEL_LONG_WRAP);

    lv_obj_t *how = hmi_panel(parent);
    lv_obj_set_size(how, LV_PCT(100), 160);
    lv_obj_set_pos(how, 0, 276);
    char buf[512];
    snprintf(buf, sizeof(buf),
             "Como funciona\n"
             "• Gravar: o ESP do motor grava a calibração inteira na flash (NVS) numa tarefa comum, fora da "
             "interrupção do giro, e só com o motor parado (rpm = 0).\n"
             "• Exportar: a calibração vira um arquivo (%s) com versão e CRC-32.\n"
             "• Importar: o ESP confere versão, CRC e limites antes de aceitar; depois grave para manter.",
             (g_ui.plat && g_ui.plat->backup_name) ? g_ui.plat->backup_name : "backup.tgcal");
    lv_obj_t *txt = hmi_label(how, &hmi_font_14, HMI_C_MUTED, buf);
    lv_obj_set_width(txt, LV_PCT(100));
    lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
}

void hmi_save_on_event(const ecu_hmi_event_t *ev)
{
    char buf[200];
    const hmi_platform_t *p = g_ui.plat;
    switch (ev->type) {
    case ECU_HMI_EV_SAVE_ACK:
        if (ev->status == ECU_OK) {
            set_status(HMI_C_OK, LV_SYMBOL_OK " Gravado na flash do ESP do motor. Vale depois de reiniciar.");
        } else {
            snprintf(buf, sizeof(buf), LV_SYMBOL_CLOSE " Recusado pelo ESP: %s.", ecu_status_text(ev->status));
            set_status(HMI_C_DANGER, buf);
        }
        break;
    case ECU_HMI_EV_BACKUP_DONE:
        if (ev->status != ECU_OK) {
            set_status(HMI_C_DANGER, LV_SYMBOL_CLOSE " Falha ao ler o backup do ESP.");
        } else if (p && p->backup_write && p->backup_write(p->ctx, g_ui.link->blob, g_ui.link->blob_len)) {
            snprintf(buf, sizeof(buf), LV_SYMBOL_OK " Backup salvo em %s (%u bytes, CRC %08lX).",
                     p->backup_name ? p->backup_name : "arquivo", g_ui.link->blob_len,
                     (unsigned long)ecu_crc32(g_ui.link->blob, g_ui.link->blob_len));
            set_status(HMI_C_OK, buf);
        } else {
            set_status(HMI_C_DANGER, LV_SYMBOL_CLOSE " Não foi possível gravar o arquivo de backup.");
        }
        break;
    case ECU_HMI_EV_RESTORE_DONE:
        if (ev->status == ECU_OK) {
            set_status(HMI_C_OK, LV_SYMBOL_OK " Backup aceito pelo ESP: mapas restaurados na RAM. Grave para manter.");
        } else {
            snprintf(buf, sizeof(buf), LV_SYMBOL_CLOSE " Recusado pelo ESP: %s. Nada mudou.", ecu_status_text(ev->status));
            set_status(HMI_C_DANGER, buf);
        }
        g_ui.tables_dirty = true;
        g_ui.params_dirty = true;
        break;
    case ECU_HMI_EV_TIMEOUT:
        if (ev->req_type == ECU_MSG_SAVE_REQ || ev->req_type == ECU_MSG_BACKUP_READ_REQ ||
            (ev->req_type >= ECU_MSG_RESTORE_BEGIN_REQ && ev->req_type <= ECU_MSG_RESTORE_COMMIT_REQ)) {
            set_status(HMI_C_DANGER, LV_SYMBOL_WARNING " Sem resposta do ESP do motor.");
        }
        break;
    default:
        break;
    }
}

void hmi_save_refresh(void)
{
    const ecu_hmi_link_t *l = g_ui.link;
    if (!l->tlm_ok) {
        lv_label_set_text(g_ui.s_state, "Sem dados do ESP do motor");
    } else if (l->tlm.flags & ECU_TLM_CALIB_DIRTY) {
        lv_label_set_text(g_ui.s_state, (l->tlm.flags & ECU_TLM_CALIB_DEFAULT)
                                            ? "Calibração padrão, com alterações não gravadas"
                                            : "Alterada na RAM, ainda não gravada");
        lv_obj_set_style_text_color(g_ui.s_state, HMI_C_WARN, 0);
    } else {
        lv_label_set_text(g_ui.s_state, "Igual à que está gravada na flash");
        lv_obj_set_style_text_color(g_ui.s_state, HMI_C_OK, 0);
    }
    const bool job = l->job == ECU_JOB_BACKUP || l->job == ECU_JOB_RESTORE;
    lv_bar_set_value(g_ui.s_bar, job ? ecu_hmi_link_job_progress(l) : (g_ui.s_last_job ? 100 : 0), LV_ANIM_OFF);
}
