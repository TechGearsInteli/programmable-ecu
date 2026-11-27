/* Tela <-> motor de ponta a ponta pelo protocolo, sem placa: os dois lados
 * conversam por "fios" em memoria. Cobre os criterios das tasks 1.6.1.2 a
 * 1.6.1.8 que nao dependem de hardware. */
#include <string.h>

#include "ecu/ecu_hmi_link.h"
#include "ecu/ecu_motor_link.h"
#include "unity.h"

typedef struct {
    uint8_t data[8192];
    size_t len;
} wire_t;

static wire_t to_motor, to_hmi;
static ecu_motor_link_t motor;
static ecu_hmi_link_t hmi;
static ecu_calib_t calib;
static uint8_t flash[ECU_CALIB_BLOB_MAX];
static size_t flash_len;
static uint32_t now;
static uint16_t rpm;
static int drop_next_motor_reply;
static ecu_hmi_event_t last_ev;
static int events;

static bool fwrite_(void *ctx, const uint8_t *d, size_t n)
{
    (void)ctx;
    memcpy(flash, d, n);
    flash_len = n;
    return true;
}
static size_t fread_(void *ctx, uint8_t *b, size_t cap)
{
    (void)ctx;
    if (flash_len > cap) return 0;
    memcpy(b, flash, flash_len);
    return flash_len;
}
static const ecu_storage_t storage = {fwrite_, fread_, NULL};

static void wire_send(void *ctx, const uint8_t *d, size_t n)
{
    wire_t *w = (wire_t *)ctx;
    if (w == &to_hmi && drop_next_motor_reply > 0) {
        drop_next_motor_reply--; /* simula resposta perdida no cabo */
        return;
    }
    TEST_ASSERT_TRUE(w->len + n <= sizeof(w->data));
    memcpy(&w->data[w->len], d, n);
    w->len += n;
}

static void on_event(void *ctx, const ecu_hmi_event_t *ev)
{
    (void)ctx;
    last_ev = *ev;
    events++;
}

/* Um "passo" de 10 ms: entrega o que cada lado mandou. */
static void step(void)
{
    now += 10;
    ecu_hmi_link_poll(&hmi, now);
    if (to_motor.len) {
        wire_t w = to_motor;
        to_motor.len = 0;
        ecu_motor_link_feed(&motor, w.data, w.len, rpm);
    }
    if (to_hmi.len) {
        wire_t w = to_hmi;
        to_hmi.len = 0;
        ecu_hmi_link_feed(&hmi, w.data, w.len, now);
    }
}

static void run_until_idle(void)
{
    for (int i = 0; i < 2000 && (ecu_hmi_link_busy(&hmi) || to_motor.len || to_hmi.len || !hmi.connected); i++) {
        step();
    }
}

static void send_telemetry(void)
{
    ecu_inputs_t in;
    memset(&in, 0, sizeof(in));
    in.rpm = rpm;
    in.map_kpa_x10 = 600;
    in.clt_x10 = 900;
    in.iat_x10 = 200;
    in.vbat_mv = 13800;
    in.sync = rpm > 0;
    ecu_engine_state_t s;
    ecu_outputs_t out;
    ecu_engine_init(&s);
    ecu_engine_compute(&calib, &in, &s, now, &out);
    ecu_motor_link_send_telemetry(&motor, &in, &out, now);
    step();
}

void setUp(void)
{
    memset(&to_motor, 0, sizeof(to_motor));
    memset(&to_hmi, 0, sizeof(to_hmi));
    flash_len = 0;
    now = 1000;
    rpm = 0;
    drop_next_motor_reply = 0;
    events = 0;
    ecu_motor_link_init(&motor, &calib, &storage, wire_send, &to_hmi, ECU_NODE_VIRTUAL);
    ecu_hmi_link_init(&hmi, wire_send, &to_motor, on_event, NULL, now);
    run_until_idle();
}

void tearDown(void) {}

static void test_conecta_e_copia_tudo(void)
{
    TEST_ASSERT_TRUE(hmi.connected);
    TEST_ASSERT_TRUE(ecu_hmi_link_synced(&hmi));
    TEST_ASSERT_EQUAL(ECU_TLM_CALIB_DEFAULT, hmi.pong.flags & ECU_TLM_CALIB_DEFAULT);
    for (unsigned t = 0; t < ECU_TBL_COUNT; t++) {
        TEST_ASSERT_EQUAL_MEMORY(calib.table[t].v, hmi.tables[t].v, sizeof(calib.table[t].v[0]) * hmi.tables[t].info.rows);
        TEST_ASSERT_EQUAL_MEMORY(calib.table[t].x, hmi.tables[t].info.x, sizeof(int16_t) * hmi.tables[t].info.cols);
    }
    TEST_ASSERT_EQUAL(240, hmi.params[ECU_PAR_INJ_FLOW].p.value);
    TEST_ASSERT_EQUAL(2000, hmi.tables[ECU_TBL_FUEL].info.max);
}

static void test_telemetria_mostra_a_celula_do_motor(void)
{
    rpm = 3000;
    send_telemetry();
    TEST_ASSERT_TRUE(ecu_hmi_link_alive(&hmi, now));
    TEST_ASSERT_EQUAL(3000, hmi.tlm.rpm);
    TEST_ASSERT_EQUAL(8, hmi.tlm.fuel_row);
    TEST_ASSERT_EQUAL(7, hmi.tlm.fuel_col);
    TEST_ASSERT_EQUAL(calib.table[ECU_TBL_FUEL].v[8][7], hmi.tlm.fuel_cell);
    now += 600;
    TEST_ASSERT_FALSE(ecu_hmi_link_alive(&hmi, now)); /* sem telemetria = "sem dados" */
}

static void test_editar_celula_dentro_do_limite_muda_o_pw(void)
{
    rpm = 3000;
    send_telemetry();
    const uint16_t pw_before = hmi.tlm.pw_us;
    const int16_t cell = calib.table[ECU_TBL_FUEL].v[8][7];
    TEST_ASSERT_TRUE(ecu_hmi_link_write_cell(&hmi, ECU_TBL_FUEL, 8, 7, (int16_t)(cell + 50)));
    run_until_idle();
    TEST_ASSERT_EQUAL(ECU_HMI_EV_CELL_ACK, last_ev.type);
    TEST_ASSERT_EQUAL(ECU_OK, last_ev.status);
    TEST_ASSERT_EQUAL(cell + 50, calib.table[ECU_TBL_FUEL].v[8][7]); /* passou a valer no motor */
    TEST_ASSERT_EQUAL(cell + 50, hmi.tables[ECU_TBL_FUEL].v[8][7]);
    send_telemetry();
    TEST_ASSERT_TRUE(hmi.tlm.pw_us > pw_before);
    TEST_ASSERT_TRUE(hmi.tlm.flags & ECU_TLM_CALIB_DIRTY);
}

static void test_valor_absurdo_e_recusado_e_o_mapa_fica(void)
{
    const int16_t cell = calib.table[ECU_TBL_FUEL].v[2][3];
    TEST_ASSERT_EQUAL(ECU_ERR_RANGE, ecu_hmi_link_precheck_cell(&hmi, ECU_TBL_FUEL, 9999));
    /* a tela avisa, mas quem decide e o motor: mandamos mesmo assim */
    TEST_ASSERT_TRUE(ecu_hmi_link_write_cell(&hmi, ECU_TBL_FUEL, 2, 3, 9999));
    run_until_idle();
    TEST_ASSERT_EQUAL(ECU_HMI_EV_CELL_ACK, last_ev.type);
    TEST_ASSERT_EQUAL(ECU_ERR_RANGE, last_ev.status);
    TEST_ASSERT_EQUAL(cell, last_ev.value);
    TEST_ASSERT_EQUAL(cell, calib.table[ECU_TBL_FUEL].v[2][3]);
    TEST_ASSERT_EQUAL(cell, hmi.tables[ECU_TBL_FUEL].v[2][3]);
}

static void test_ignicao_muda_o_avanco(void)
{
    rpm = 3000;
    TEST_ASSERT_TRUE(ecu_hmi_link_write_cell(&hmi, ECU_TBL_IGN, 8, 7, 185));
    run_until_idle();
    TEST_ASSERT_EQUAL(ECU_OK, last_ev.status);
    send_telemetry();
    TEST_ASSERT_EQUAL(185, hmi.tlm.advance_x10);
    TEST_ASSERT_TRUE(ecu_hmi_link_write_cell(&hmi, ECU_TBL_IGN, 8, 7, 700)); /* 70 graus */
    run_until_idle();
    TEST_ASSERT_EQUAL(ECU_ERR_RANGE, last_ev.status);
    TEST_ASSERT_EQUAL(185, calib.table[ECU_TBL_IGN].v[8][7]);
}

static void test_ficha_so_com_motor_parado(void)
{
    rpm = 900;
    TEST_ASSERT_TRUE(ecu_hmi_link_write_param(&hmi, ECU_PAR_INJ_FLOW, 300));
    run_until_idle();
    TEST_ASSERT_EQUAL(ECU_HMI_EV_PARAM_ACK, last_ev.type);
    TEST_ASSERT_EQUAL(ECU_ERR_ENGINE_RUNNING, last_ev.status);
    TEST_ASSERT_EQUAL(240, calib.param[ECU_PAR_INJ_FLOW]);
    rpm = 0;
    TEST_ASSERT_TRUE(ecu_hmi_link_write_param(&hmi, ECU_PAR_INJ_FLOW, 300));
    run_until_idle();
    TEST_ASSERT_EQUAL(ECU_OK, last_ev.status);
    TEST_ASSERT_EQUAL(300, calib.param[ECU_PAR_INJ_FLOW]);
    TEST_ASSERT_EQUAL(300, hmi.params[ECU_PAR_INJ_FLOW].p.value);
}

static void test_gravar_reiniciar_e_a_celula_continua(void)
{
    TEST_ASSERT_TRUE(ecu_hmi_link_write_cell(&hmi, ECU_TBL_FUEL, 5, 5, 444));
    run_until_idle();
    rpm = 1200;
    TEST_ASSERT_TRUE(ecu_hmi_link_save(&hmi));
    run_until_idle();
    TEST_ASSERT_EQUAL(ECU_HMI_EV_SAVE_ACK, last_ev.type);
    TEST_ASSERT_EQUAL(ECU_ERR_ENGINE_RUNNING, last_ev.status);
    TEST_ASSERT_EQUAL(0, flash_len);
    rpm = 0;
    TEST_ASSERT_TRUE(ecu_hmi_link_save(&hmi));
    run_until_idle();
    TEST_ASSERT_EQUAL(ECU_OK, last_ev.status);
    TEST_ASSERT_FALSE(ecu_motor_link_dirty(&motor));
    /* reinicia o motor: nova RAM carregada da flash */
    memset(&calib, 0, sizeof(calib));
    ecu_motor_link_init(&motor, &calib, &storage, wire_send, &to_hmi, ECU_NODE_VIRTUAL);
    TEST_ASSERT_FALSE(motor.calib_default);
    TEST_ASSERT_EQUAL(444, calib.table[ECU_TBL_FUEL].v[5][5]);
}

static void test_backup_exportado_e_importado_reproduz_a_tabela(void)
{
    ecu_calib_t original = calib;
    TEST_ASSERT_TRUE(ecu_hmi_link_backup(&hmi));
    run_until_idle();
    TEST_ASSERT_EQUAL(ECU_HMI_EV_BACKUP_DONE, last_ev.type);
    TEST_ASSERT_EQUAL(ECU_OK, last_ev.status);
    TEST_ASSERT_EQUAL(ecu_calib_blob_size(), hmi.blob_len);
    uint8_t file[ECU_CALIB_BLOB_MAX];
    const uint16_t file_len = hmi.blob_len;
    memcpy(file, hmi.blob, file_len);

    /* muda varias coisas depois do backup */
    TEST_ASSERT_TRUE(ecu_hmi_link_write_cell(&hmi, ECU_TBL_FUEL, 0, 0, 1999));
    TEST_ASSERT_TRUE(ecu_hmi_link_write_cell(&hmi, ECU_TBL_IGN, 15, 15, -50));
    run_until_idle();
    TEST_ASSERT_EQUAL(1999, calib.table[ECU_TBL_FUEL].v[0][0]);

    TEST_ASSERT_TRUE(ecu_hmi_link_restore(&hmi, file, file_len));
    run_until_idle();
    TEST_ASSERT_EQUAL(ECU_OK, ecu_hmi_link_synced(&hmi) ? ECU_OK : 1);
    TEST_ASSERT_EQUAL_MEMORY(&original, &calib, sizeof(calib));
    TEST_ASSERT_EQUAL(original.table[ECU_TBL_FUEL].v[0][0], hmi.tables[ECU_TBL_FUEL].v[0][0]);
    /* exportar de novo da os mesmos bytes */
    TEST_ASSERT_TRUE(ecu_hmi_link_backup(&hmi));
    run_until_idle();
    TEST_ASSERT_EQUAL_MEMORY(file, hmi.blob, file_len);
}

static void test_backup_corrompido_nao_entra(void)
{
    TEST_ASSERT_TRUE(ecu_hmi_link_backup(&hmi));
    run_until_idle();
    uint8_t file[ECU_CALIB_BLOB_MAX];
    const uint16_t len = hmi.blob_len;
    memcpy(file, hmi.blob, len);
    file[len / 2] ^= 0xFF;
    ecu_calib_t before = calib;
    TEST_ASSERT_TRUE(ecu_hmi_link_restore(&hmi, file, len));
    run_until_idle();
    TEST_ASSERT_EQUAL(ECU_HMI_EV_RESTORE_DONE, last_ev.type);
    TEST_ASSERT_EQUAL(ECU_ERR_BACKUP, last_ev.status);
    TEST_ASSERT_EQUAL_MEMORY(&before, &calib, sizeof(calib));
}

static void test_resposta_perdida_e_reenviada_sem_aplicar_duas_vezes(void)
{
    const uint32_t requests_before = motor.requests;
    drop_next_motor_reply = 1;
    TEST_ASSERT_TRUE(ecu_hmi_link_write_cell(&hmi, ECU_TBL_FUEL, 1, 1, 600));
    run_until_idle();
    TEST_ASSERT_EQUAL(ECU_OK, last_ev.status);
    TEST_ASSERT_EQUAL(600, hmi.tables[ECU_TBL_FUEL].v[1][1]);
    TEST_ASSERT_EQUAL(requests_before + 1, motor.requests); /* o reenvio repetiu a resposta guardada */
}

static ecu_frame_t nack_frame;
static void keep_frame(void *ctx, const ecu_frame_t *f)
{
    (void)ctx;
    nack_frame = *f;
}

static void test_tipo_desconhecido_recebe_nack(void)
{
    uint8_t frame[16];
    const size_t n = ecu_frame_encode(frame, sizeof(frame), 200, 0x6E, NULL, 0);
    to_hmi.len = 0;
    ecu_motor_link_feed(&motor, frame, n, 0);
    TEST_ASSERT_EQUAL(1, motor.nacks);
    ecu_parser_t p;
    ecu_parser_init(&p);
    ecu_parser_feed(&p, to_hmi.data, to_hmi.len, keep_frame, NULL);
    TEST_ASSERT_EQUAL(ECU_MSG_NACK, nack_frame.type);
    TEST_ASSERT_EQUAL(200, nack_frame.seq);
    ecu_nack_t m;
    TEST_ASSERT_TRUE(ecu_unpack_nack(nack_frame.payload, nack_frame.len, &m));
    TEST_ASSERT_EQUAL(ECU_NACK_UNKNOWN_TYPE, m.reason);
    TEST_ASSERT_EQUAL(0x6E, m.rejected_type);
    to_hmi.len = 0;
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_conecta_e_copia_tudo);
    RUN_TEST(test_telemetria_mostra_a_celula_do_motor);
    RUN_TEST(test_editar_celula_dentro_do_limite_muda_o_pw);
    RUN_TEST(test_valor_absurdo_e_recusado_e_o_mapa_fica);
    RUN_TEST(test_ignicao_muda_o_avanco);
    RUN_TEST(test_ficha_so_com_motor_parado);
    RUN_TEST(test_gravar_reiniciar_e_a_celula_continua);
    RUN_TEST(test_backup_exportado_e_importado_reproduz_a_tabela);
    RUN_TEST(test_backup_corrompido_nao_entra);
    RUN_TEST(test_resposta_perdida_e_reenviada_sem_aplicar_duas_vezes);
    RUN_TEST(test_tipo_desconhecido_recebe_nack);
    return UNITY_END();
}
