/*
 * Cenarios de tela (teste ponta a ponta + capturas para a documentacao).
 *
 * Cada cenario mexe no motor virtual (como os potenciometros da bancada),
 * toca nos mesmos botoes que o usuario tocaria e confere o que aparece na tela
 * e o que mudou dentro do "ESP do motor". Saida: 0 se tudo passou.
 */
#include <stdio.h>
#include <string.h>

#include "ecu/ecu_calib.h"
#include "hmi/ui_internal.h"
#include "sim.h"

static int failures, checks;
static const char *out;

#define CHECK(cond, ...)                                                                                       \
    do {                                                                                                       \
        checks++;                                                                                              \
        if (cond) {                                                                                            \
            printf("  [ok]     ");                                                                             \
        } else {                                                                                               \
            failures++;                                                                                        \
            printf("  [FALHOU] ");                                                                             \
        }                                                                                                      \
        printf(__VA_ARGS__);                                                                                   \
        printf("\n");                                                                                          \
    } while (0)

static bool has(lv_obj_t *label, const char *text)
{
    return strstr(lv_label_get_text(label), text) != NULL;
}

static bool visible(lv_obj_t *o)
{
    return !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void shot(const char *name)
{
    if (sim_snapshot(out, name)) {
        printf("  captura: %s.bmp\n", name);
    } else {
        failures++;
        printf("  [FALHOU] nao gravou a captura %s\n", name);
    }
}

static void tap_grid(hmi_grid_t *g, int row, int col)
{
    uint32_t vr, vc;
    hmi_grid_cell_to_view(g, row, col, &vr, &vc);
    sim_tap_table_cell(g->table, vr, vc);
}

static void taps(lv_obj_t *btn, int n)
{
    for (int i = 0; i < n; i++) sim_tap_obj(btn);
}

static void nav(hmi_screen_t s)
{
    sim_tap_obj(g_ui.nav_btn[s]);
    sim_run_ms(200);
}

static motor_virtual_t *motor(void)
{
    return &g_sim.motor;
}

/* ------------------------------------------------------------------------ */

static void sc_painel(void)
{
    printf("\n== Painel e conexao (1.7.1.1 / 1.7.1.2)\n");
    motor_virtual_set(motor(), ECU_FIT_IDLE_RPM, 340, 2, 250);
    sim_run_ms(2000);
    CHECK(g_sim.link.connected, "a tela achou o ESP do motor (PING/PONG)");
    CHECK(ecu_hmi_link_synced(&g_sim.link), "a tela copiou as 7 tabelas e a ficha do motor");
    CHECK(strcmp(lv_label_get_text(g_ui.p_rpm), "750") == 0, "painel mostra 750 rpm (marcha lenta do Fit)");
    CHECK(has(g_ui.p_val[4], "25,0"), "painel mostra a temperatura do motor (25,0 °C)");
    CHECK(!g_ui.alert_rpm && !g_ui.alert_clt && has(g_ui.p_alert_lbl, "Sem alertas"), "abaixo dos limites, o alerta fica apagado");
    shot("01_painel_marcha_lenta");

    motor_virtual_set(motor(), 3000, 600, 30, 850);
    sim_run_ms(300);
    CHECK(has(g_ui.p_rpm, "3000"), "um dado novo do motor muda o número na tela (3000 rpm)");

    motor_virtual_set(motor(), 6800, 980, 90, 880);
    motor()->in.sync = false;
    sim_run_ms(300);
    CHECK(strcmp(lv_label_get_text(g_ui.p_rpm), "0") == 0, "sem sincronismo o giro mostra 0, sem ficar no ultimo valor");
    CHECK(!g_ui.alert_rpm, "giro sem sincronismo nao acende alerta de rotacao");
}

static void sc_alertas(void)
{
    printf("\n== Alertas de RPM e temperatura (1.7.1.3)\n");
    const ecu_calib_t before = motor()->calib;
    motor_virtual_set(motor(), 6800, 980, 90, 880);
    sim_run_ms(300);
    CHECK(g_ui.alert_rpm && has(g_ui.p_alert_lbl, "Rotação acima"), "6800 rpm > 6500: alerta de rotação aceso");
    CHECK(visible(g_ui.top_alert), "o aviso também aparece na barra de cima (todas as telas)");
    shot("02_painel_alerta_rpm");
    motor_virtual_set(motor(), 6400, 980, 90, 880);
    sim_run_ms(300);
    CHECK(!g_ui.alert_rpm && has(g_ui.p_alert_lbl, "Sem alertas"), "6400 rpm: abaixo do limite, o alerta apaga");
    motor_virtual_set(motor(), 2500, 600, 25, 1100);
    sim_run_ms(300);
    CHECK(g_ui.alert_clt && has(g_ui.p_alert_lbl, "Motor quente"), "110 °C > 105 °C: alerta de temperatura");
    shot("03_painel_alerta_temperatura");
    CHECK(memcmp(&before, &motor()->calib, sizeof(before)) == 0, "o alerta não altera o mapa sozinho");
    motor_virtual_set(motor(), 2500, 600, 25, 900);
    sim_run_ms(300);
}

static void sc_celula_atual(void)
{
    printf("\n== Célula atual e grade de combustível (1.6.1.2 / 1.6.1.4)\n");
    nav(HMI_SCR_FUEL);
    motor_virtual_set(motor(), 2500, 600, 25, 900);
    sim_run_ms(300);
    const ecu_telemetry_t *t = &g_sim.link.tlm;
    CHECK(t->fuel_row == 8 && t->fuel_col == 6, "ESP do motor diz: célula [8,6] (60 kPa × 2500 rpm)");
    CHECK(g_ui.fuel.cur_row == 8 && g_ui.fuel.cur_col == 6, "a grade destaca a mesma célula [8,6]");
    CHECK(has(g_ui.fuel_info, "[8,6]"), "a linha de cima mostra índices e valor da célula");
    CHECK(g_sim.link.tables[ECU_TBL_FUEL].rows_ok == 0xFFFF, "a grade inteira (16×16) foi carregada");
    shot("04_combustivel_celula_atual");
    motor_virtual_set(motor(), 2500, 800, 40, 900); /* "pot de carga" para 80 kPa */
    sim_run_ms(300);
    CHECK(g_ui.fuel.cur_row == 12, "mudar a carga (80 kPa) muda a célula destacada para a linha 12");
    motor_virtual_set(motor(), 2500, 600, 25, 900);
    sim_run_ms(300);
}

static void sc_editar(void)
{
    printf("\n== Editar uma célula com confirmação do ESP (1.6.1.3)\n");
    const int16_t old = motor()->calib.table[ECU_TBL_FUEL].v[8][6];
    const uint16_t pw_before = g_sim.link.tlm.pw_us;
    tap_grid(&g_ui.fuel, 8, 6);
    CHECK(g_ui.ed_fuel.has_target && g_ui.ed_fuel.row == 8 && g_ui.ed_fuel.col == 6,
          "tocar na grade escolhe a célula [8,6] para editar");
    taps(g_ui.ed_fuel.plus, 4); /* +4 × 0,05 ms */
    CHECK(g_ui.ed_fuel.pending == old + 20, "o + soma 0,20 ms (ainda não enviado)");
    CHECK(motor()->calib.table[ECU_TBL_FUEL].v[8][6] == old, "antes de enviar, o ESP do motor não mudou");
    shot("05_combustivel_editando");
    sim_tap_obj(g_ui.ed_fuel.send);
    sim_run_ms(300);
    CHECK(has(g_ui.ed_fuel.status, "Confirmado"), "o ESP respondeu: Confirmado");
    CHECK(motor()->calib.table[ECU_TBL_FUEL].v[8][6] == old + 20, "o valor passou a valer no ESP do motor");
    CHECK(g_sim.link.tlm.pw_us > pw_before, "o Pulse Width calculado subiu (%u -> %u us)", pw_before,
          g_sim.link.tlm.pw_us);
    shot("06_combustivel_confirmado");
}

static void sc_recusar(void)
{
    printf("\n== Valor absurdo é recusado e o mapa fica (1.6.1.3)\n");
    const int16_t old = motor()->calib.table[ECU_TBL_FUEL].v[8][6];
    sim_tap_obj(g_ui.ed_fuel.keypad);
    CHECK(visible(g_ui.kb_modal), "o teclado numérico abriu");
    lv_textarea_set_text(g_ui.kb_ta, "99,99");
    sim_run_ms(100);
    shot("07_teclado_numerico");
    lv_obj_send_event(g_ui.kb, LV_EVENT_READY, NULL);
    sim_run_ms(100);
    CHECK(has(g_ui.ed_fuel.status, "Fora do limite"), "a tela avisa: fora do limite, o ESP vai recusar");
    sim_tap_obj(g_ui.ed_fuel.send); /* manda mesmo assim: quem decide é o motor */
    sim_run_ms(300);
    CHECK(has(g_ui.ed_fuel.status, "Recusado pelo ESP"), "o ESP recusou e a tela mostra a recusa");
    CHECK(motor()->calib.table[ECU_TBL_FUEL].v[8][6] == old, "o mapa anterior permanece no ESP");
    CHECK(g_sim.link.tables[ECU_TBL_FUEL].v[8][6] == old, "a grade continua mostrando o valor que vale");
    shot("08_combustivel_recusado");
}

static void sc_ignicao(void)
{
    printf("\n== Grade de ignição (1.6.1.5)\n");
    nav(HMI_SCR_IGN);
    const int16_t old = motor()->calib.table[ECU_TBL_IGN].v[8][6];
    tap_grid(&g_ui.ign, 8, 6);
    taps(g_ui.ed_ign.plus, 3); /* +1,5 grau */
    sim_tap_obj(g_ui.ed_ign.send);
    sim_run_ms(300);
    CHECK(has(g_ui.ed_ign.status, "Confirmado"), "o ESP confirmou o novo avanço");
    CHECK(g_sim.link.tlm.advance_x10 == old + 15, "o avanço calculado mudou para a nova célula (%d -> %d)", old,
          g_sim.link.tlm.advance_x10);
    shot("09_ignicao_grade");
    sim_tap_obj(g_ui.ed_ign.keypad);
    lv_textarea_set_text(g_ui.kb_ta, "60");
    lv_obj_send_event(g_ui.kb, LV_EVENT_READY, NULL);
    sim_tap_obj(g_ui.ed_ign.send);
    sim_run_ms(300);
    CHECK(has(g_ui.ed_ign.status, "Recusado"), "60° (acima de 45°) é recusado");
    CHECK(motor()->calib.table[ECU_TBL_IGN].v[8][6] == old + 15, "o avanço anterior continua valendo");
}

static void sc_correcoes(void)
{
    printf("\n== Correções 2D (1.6.1.6)\n");
    nav(HMI_SCR_CORR);
    sim_tap_obj(g_ui.corr_btn[0]);
    sim_run_ms(200);
    motor_virtual_set(motor(), 2500, 600, 25, 900); /* 90 °C: ponto 7 da curva */
    sim_run_ms(300);
    const uint16_t pw0 = g_sim.link.tlm.pw_us;
    const int16_t warm0 = g_sim.link.tlm.corr_warm_x10;
    CHECK(has(g_ui.corr_now, "Valendo agora"), "a tela mostra a correção que está valendo");
    tap_grid(&g_ui.curve, 0, 7);
    taps(g_ui.ed_curve.plus, 5); /* +5 % */
    sim_tap_obj(g_ui.ed_curve.send);
    sim_run_ms(300);
    CHECK(has(g_ui.ed_curve.status, "Confirmado"), "o ESP aceitou o ponto da curva de aquecimento");
    CHECK(g_sim.link.tlm.corr_warm_x10 == warm0 + 50, "a correção de aquecimento passou a +5,0 %%");
    const uint32_t expect = (uint32_t)(pw0 - g_sim.link.tlm.dead_time_us);
    CHECK(g_sim.link.tlm.pw_us > pw0, "o Pulse Width subiu com a correção (%u -> %u us, base %u)", pw0,
          g_sim.link.tlm.pw_us, expect);
    shot("10_correcoes_aquecimento");
    sim_tap_obj(g_ui.corr_btn[3]);
    sim_run_ms(300);
    CHECK(has(g_ui.corr_title, "Bateria"), "curva do tempo morto do bico pela bateria");
    shot("11_correcoes_bateria");
}

static void sc_ficha(void)
{
    printf("\n== Ficha do motor (1.6.1.7)\n");
    nav(HMI_SCR_MOTOR);
    motor_virtual_set(motor(), 2500, 600, 25, 900);
    sim_run_ms(200);
    const uint16_t pw_ref = g_sim.link.tlm.pw_us;
    sim_tap_table_cell(g_ui.m_table, 1 + ECU_PAR_INJ_FLOW, 1);
    CHECK(g_ui.ed_param.has_target && g_ui.ed_param.param == ECU_PAR_INJ_FLOW, "escolheu a vazão do bico");
    taps(g_ui.ed_param.plus, 6); /* 240 -> 300 cc/min */
    sim_tap_obj(g_ui.ed_param.send);
    sim_run_ms(300);
    CHECK(has(g_ui.ed_param.status, "motor parado"), "com o motor girando, o ESP recusa mexer na ficha");
    CHECK(motor()->calib.param[ECU_PAR_INJ_FLOW] == 240, "a vazão continua 240 cc/min no ESP");
    shot("12_ficha_recusada_motor_girando");

    motor_virtual_set(motor(), 0, 1000, 25, 900); /* motor parado (pedal igual: sem enriquecimento de aceleracao) */
    sim_run_ms(200);
    taps(g_ui.ed_param.plus, 6);
    sim_tap_obj(g_ui.ed_param.send);
    sim_run_ms(300);
    CHECK(has(g_ui.ed_param.status, "Confirmado"), "parado, o ESP aceita a vazão de 300 cc/min");
    CHECK(motor()->calib.param[ECU_PAR_INJ_FLOW] == 300, "a ficha mudou no ESP do motor");
    motor_virtual_set(motor(), 2500, 600, 25, 900);
    sim_run_ms(300);
    CHECK(g_sim.link.tlm.inj_scale_x1000 == 800 && g_sim.link.tlm.pw_us < pw_ref,
          "bico maior: mesma célula, Pulse Width menor (%u -> %u us)", pw_ref, g_sim.link.tlm.pw_us);

    motor_virtual_set(motor(), 0, 1000, 25, 900);
    sim_run_ms(200);
    sim_tap_table_cell(g_ui.m_table, 1 + ECU_PAR_CYLINDERS, 1);
    sim_tap_obj(g_ui.ed_param.minus); /* 4 -> 3 cilindros */
    sim_tap_obj(g_ui.ed_param.send);
    sim_run_ms(300);
    CHECK(motor()->calib.param[ECU_PAR_CYLINDERS] == 3, "o ESP aceitou 3 cilindros");
    CHECK(has(g_ui.m_order, "1-2-3"), "a ordem de ignição mudou para 1-2-3 (a cada 240°)");
    shot("13_ficha_do_motor");
    sim_tap_obj(g_ui.ed_param.plus); /* volta para 4 */
    sim_tap_obj(g_ui.ed_param.send);
    sim_run_ms(300);
    CHECK(has(g_ui.m_order, "1-3-4-2"), "4 cilindros: ordem 1-3-4-2");
    sim_tap_table_cell(g_ui.m_table, 1 + ECU_PAR_INJ_FLOW, 1);
    taps(g_ui.ed_param.minus, 6);
    sim_tap_obj(g_ui.ed_param.send);
    sim_run_ms(300);
}

static void sc_gravar(void)
{
    printf("\n== Gravar na flash e ler depois de reiniciar (1.6.1.8)\n");
    nav(HMI_SCR_SAVE);
    const int16_t edited = motor()->calib.table[ECU_TBL_FUEL].v[8][6];
    motor_virtual_set(motor(), 2500, 600, 25, 900);
    sim_run_ms(200);
    sim_tap_obj(g_ui.s_save);
    sim_run_ms(300);
    CHECK(has(g_ui.s_status, "Recusado") && has(g_ui.s_status, "motor parado"),
          "com o motor girando, o ESP recusa gravar");
    motor_virtual_set(motor(), 0, 1000, 25, 900);
    sim_run_ms(200);
    sim_tap_obj(g_ui.s_save);
    sim_run_ms(300);
    CHECK(has(g_ui.s_status, "Gravado"), "parado, o ESP gravou na flash");
    CHECK(!(g_sim.link.tlm.flags & ECU_TLM_CALIB_DIRTY), "a RAM ficou igual à flash");
    shot("14_gravar_na_flash");

    motor_virtual_reboot(motor(), g_sim.now); /* desliga e liga o ESP do motor */
    sim_run_ms(500);
    CHECK(!motor()->link.calib_default, "depois de reiniciar, o ESP leu a calibração da flash");
    CHECK(motor()->calib.table[ECU_TBL_FUEL].v[8][6] == edited, "a célula editada continua com o valor editado");
}

static void sc_backup(void)
{
    printf("\n== Exportar e importar backup (1.6.1.8)\n");
    const ecu_calib_t saved = motor()->calib;
    sim_tap_obj(g_ui.s_export);
    sim_run_ms(500);
    CHECK(has(g_ui.s_status, "Backup salvo"), "backup exportado para o arquivo");
    /* estraga a calibracao depois do backup */
    ecu_hmi_link_write_cell(&g_sim.link, ECU_TBL_FUEL, 0, 0, 1999);
    ecu_hmi_link_write_cell(&g_sim.link, ECU_TBL_IGN, 15, 15, -50);
    sim_run_ms(300);
    CHECK(motor()->calib.table[ECU_TBL_FUEL].v[0][0] == 1999, "mudamos a tabela depois do backup");
    sim_tap_obj(g_ui.s_import);
    sim_run_ms(800);
    CHECK(has(g_ui.s_status, "Backup aceito"), "o ESP aceitou o backup importado");
    CHECK(memcmp(&saved, &motor()->calib, sizeof(saved)) == 0, "a tabela voltou exatamente ao que foi exportado");
    shot("15_backup_importado");
}

static void sc_final(void)
{
    printf("\n== Painel em cruzeiro (capturas finais)\n");
    nav(HMI_SCR_PAINEL);
    motor_virtual_set(motor(), 2500, 600, 25, 880);
    sim_run_ms(400);
    shot("16_painel_cruzeiro");
    nav(HMI_SCR_FUEL);
    sim_run_ms(200);
    shot("17_combustivel_cruzeiro");
}

int sim_scenarios(const char *which, const char *out_dir)
{
    out = out_dir;
    const bool all = !strcmp(which, "todos");
    printf("Cenarios da HMI (saida: %s)\n", out_dir);
    sc_painel(); /* sempre: conecta e sincroniza */
    if (all || !strcmp(which, "alertas")) sc_alertas();
    if (all || !strcmp(which, "mapas")) {
        sc_celula_atual();
        sc_editar();
        sc_recusar();
        sc_ignicao();
    }
    if (all || !strcmp(which, "correcoes")) sc_correcoes();
    if (all || !strcmp(which, "ficha")) sc_ficha();
    if (all || !strcmp(which, "gravar")) {
        sc_gravar();
        sc_backup();
    }
    if (all) sc_final();
    printf("\n%d verificações, %d falhas\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
