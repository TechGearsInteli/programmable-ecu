/* Testes do protocolo UART (task 1.6.1.1): CRC, frames, parser e mensagens. */
#include <string.h>

#include "ecu/ecu_crc.h"
#include "ecu/ecu_protocol.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static ecu_frame_t got[8];
static int got_n;

static void collect(void *ctx, const ecu_frame_t *f)
{
    (void)ctx;
    if (got_n < 8) got[got_n++] = *f;
}

static void test_crc_vetores_conhecidos(void)
{
    const uint8_t s[] = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x29B1, ecu_crc16(s, 9));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, ecu_crc32(s, 9));
    /* encadear da o mesmo resultado */
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, ecu_crc32_update(ecu_crc32_update(0, s, 4), s + 4, 5));
}

static void test_frame_formato_documentado(void)
{
    /* Pedido de escrita: tabela 0, linha 8, coluna 6, valor 455 (4,55 ms), SEQ 7 */
    const ecu_cell_t c = {0, 8, 6, 0, 455};
    uint8_t p[8], frame[32];
    const uint8_t len = ecu_pack_cell_write_req(&c, p);
    const size_t n = ecu_frame_encode(frame, sizeof(frame), 7, ECU_MSG_CELL_WRITE_REQ, p, len);
    TEST_ASSERT_EQUAL(11, n);
    const uint8_t expect_head[] = {0xA5, 5, 7, 0x23, 0, 8, 6, 0xC7, 0x01};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expect_head, frame, sizeof(expect_head));
    const uint16_t crc = ecu_crc16(&frame[1], 3 + 5);
    TEST_ASSERT_EQUAL_HEX8(crc & 0xFF, frame[9]);
    TEST_ASSERT_EQUAL_HEX8(crc >> 8, frame[10]);
}

static void test_parser_ida_e_volta(void)
{
    uint8_t frame[ECU_MAX_FRAME];
    const uint8_t payload[3] = {1, 2, 3};
    const size_t n = ecu_frame_encode(frame, sizeof(frame), 42, ECU_MSG_CELL_READ_REQ, payload, 3);
    ecu_parser_t p;
    ecu_parser_init(&p);
    got_n = 0;
    ecu_parser_feed(&p, frame, n, collect, NULL);
    TEST_ASSERT_EQUAL(1, got_n);
    TEST_ASSERT_EQUAL(42, got[0].seq);
    TEST_ASSERT_EQUAL(ECU_MSG_CELL_READ_REQ, got[0].type);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, got[0].payload, 3);
}

static void test_parser_ressincroniza_depois_de_lixo(void)
{
    /* lixo com um 0xA5 falso no meio, depois um frame valido */
    uint8_t stream[64] = {0x00, 0x13, 0xA5, 0x02, 0x99, 0xFF, 0xA5};
    size_t n = 7;
    const uint8_t payload[2] = {0x10, 0x20};
    n += ecu_frame_encode(&stream[n], sizeof(stream) - n, 9, ECU_MSG_ROW_READ_REQ, payload, 2);
    ecu_parser_t p;
    ecu_parser_init(&p);
    got_n = 0;
    ecu_parser_feed(&p, stream, n, collect, NULL);
    /* o 0xA5 falso do fim do lixo tem LEN = 0xA5 (165): o parser espera... */
    TEST_ASSERT_EQUAL(0, got_n);
    /* ...ate a linha ficar parada; ai descarta o lixo e acha o frame bom */
    ecu_parser_idle(&p, collect, NULL);
    TEST_ASSERT_EQUAL(1, got_n);
    TEST_ASSERT_EQUAL(9, got[0].seq);
    TEST_ASSERT_TRUE(p.crc_errors + p.len_errors >= 1);
    TEST_ASSERT_EQUAL(0, p.pos);
}

static void test_parser_ressincroniza_sem_linha_parada(void)
{
    /* sem pausa na linha: o trafego seguinte (telemetria) tambem destrava */
    uint8_t stream[600] = {0xA5, 0xC8};
    size_t n = 2;
    const uint8_t payload[2] = {1, 2};
    for (int k = 0; k < 40; k++) {
        n += ecu_frame_encode(&stream[n], sizeof(stream) - n, (uint8_t)k, ECU_MSG_ROW_READ_REQ, payload, 2);
    }
    ecu_parser_t p;
    ecu_parser_init(&p);
    got_n = 0;
    ecu_parser_feed(&p, stream, n, collect, NULL);
    TEST_ASSERT_EQUAL(8, got_n); /* coletor guarda 8; o primeiro frame bom nao se perdeu */
    TEST_ASSERT_EQUAL(0, got[0].seq);
}

static void test_parser_recusa_crc_errado(void)
{
    uint8_t frame[16];
    const uint8_t payload[1] = {3};
    const size_t n = ecu_frame_encode(frame, sizeof(frame), 1, ECU_MSG_TABLE_INFO_REQ, payload, 1);
    frame[4] ^= 0x01; /* um bit trocado no caminho */
    ecu_parser_t p;
    ecu_parser_init(&p);
    got_n = 0;
    ecu_parser_feed(&p, frame, n, collect, NULL);
    TEST_ASSERT_EQUAL(0, got_n);
    TEST_ASSERT_EQUAL(1, p.crc_errors);
}

static void test_parser_frame_em_pedacos(void)
{
    uint8_t frame[ECU_MAX_FRAME];
    uint8_t payload[ECU_MAX_PAYLOAD];
    for (unsigned i = 0; i < sizeof(payload); i++) payload[i] = (uint8_t)i;
    const size_t n = ecu_frame_encode(frame, sizeof(frame), 3, ECU_MSG_RESTORE_DATA_REQ, payload, ECU_MAX_PAYLOAD);
    TEST_ASSERT_EQUAL(ECU_MAX_FRAME, n);
    ecu_parser_t p;
    ecu_parser_init(&p);
    got_n = 0;
    for (size_t i = 0; i < n; i++) {
        ecu_parser_feed(&p, &frame[i], 1, collect, NULL); /* byte a byte, como na UART */
    }
    TEST_ASSERT_EQUAL(1, got_n);
    TEST_ASSERT_EQUAL(ECU_MAX_PAYLOAD, got[0].len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, got[0].payload, ECU_MAX_PAYLOAD);
}

static void test_payload_grande_demais_nao_monta(void)
{
    uint8_t frame[ECU_MAX_FRAME + 8];
    uint8_t payload[255] = {0};
    TEST_ASSERT_EQUAL(0, ecu_frame_encode(frame, sizeof(frame), 1, 1, payload, 241));
}

static void test_telemetria_ida_e_volta(void)
{
    ecu_telemetry_t a, b;
    memset(&a, 0, sizeof(a));
    a.rpm = 2500;
    a.flags = ECU_TLM_SYNC | ECU_TLM_RUNNING;
    a.tps_pct = 35;
    a.pw_us = 5310;
    a.advance_x10 = -25;
    a.map_kpa_x10 = 600;
    a.clt_x10 = -55;
    a.vbat_mv = 13800;
    a.fuel_row = 8;
    a.fuel_col = 6;
    a.fuel_cell = 455;
    a.corr_iat_x10 = -20;
    a.uptime_ms = 123456789u;
    uint8_t p[ECU_MAX_PAYLOAD];
    const uint8_t len = ecu_pack_telemetry(&a, p);
    TEST_ASSERT_EQUAL(ECU_TELEMETRY_LEN, len);
    TEST_ASSERT_TRUE(ecu_unpack_telemetry(p, len, &b));
    TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof(a));
    TEST_ASSERT_FALSE(ecu_unpack_telemetry(p, (uint8_t)(len - 1), &b));
}

static void test_mensagens_ida_e_volta(void)
{
    uint8_t p[ECU_MAX_PAYLOAD];

    ecu_table_info_t ti, ti2;
    memset(&ti, 0, sizeof(ti));
    ti.table = 0;
    ti.rows = 16;
    ti.cols = 16;
    ti.value_unit = ECU_UNIT_MS_X100;
    ti.min = 50;
    ti.max = 2000;
    ti.step = 5;
    for (int i = 0; i < 16; i++) {
        ti.x[i] = (int16_t)(500 + 400 * i);
        ti.y[i] = (int16_t)(20 + 5 * i);
    }
    uint8_t len = ecu_pack_table_info(&ti, p);
    TEST_ASSERT_EQUAL(76, len);
    TEST_ASSERT_TRUE(ecu_unpack_table_info(p, len, &ti2));
    TEST_ASSERT_EQUAL_MEMORY(&ti, &ti2, sizeof(ti));

    /* curva: 1 linha, sem eixo y */
    ti.rows = 1;
    ti.cols = 8;
    memset(ti.y, 0, sizeof(ti.y));
    memset(&ti.x[8], 0, sizeof(int16_t) * 8);
    len = ecu_pack_table_info(&ti, p);
    TEST_ASSERT_EQUAL(28, len);
    TEST_ASSERT_TRUE(ecu_unpack_table_info(p, len, &ti2));
    TEST_ASSERT_EQUAL_MEMORY(&ti, &ti2, sizeof(ti));

    ecu_cell_t c, c2;
    memset(&c, 0, sizeof(c));
    c.table = 1;
    c.row = 2;
    c.col = 3;
    c.status = ECU_ERR_RANGE;
    c.value = -77;
    len = ecu_pack_cell_resp(&c, p);
    TEST_ASSERT_TRUE(ecu_unpack_cell_resp(p, len, &c2));
    TEST_ASSERT_EQUAL_MEMORY(&c, &c2, sizeof(c));

    ecu_row_t r, r2;
    memset(&r, 0, sizeof(r));
    r.table = 2;
    r.count = 8;
    for (int i = 0; i < 8; i++) r.values[i] = (int16_t)(i * 100 - 300);
    len = ecu_pack_row(&r, p);
    TEST_ASSERT_TRUE(ecu_unpack_row(p, len, &r2));
    TEST_ASSERT_EQUAL_MEMORY(&r, &r2, sizeof(r));

    ecu_param_t pa, pa2;
    memset(&pa, 0, sizeof(pa)); /* zera o preenchimento entre campos para comparar memoria */
    pa.param = 3;
    pa.status = ECU_OK;
    pa.unit = ECU_UNIT_CC_MIN;
    pa.value = 240;
    pa.min = 50;
    pa.max = 2000;
    pa.step = 10;
    len = ecu_pack_param(&pa, p);
    TEST_ASSERT_TRUE(ecu_unpack_param(p, len, &pa2));
    TEST_ASSERT_EQUAL_MEMORY(&pa, &pa2, sizeof(pa));

    ecu_chunk_t ch, ch2;
    memset(&ch, 0, sizeof(ch));
    ch.offset = 400;
    ch.total = 1342;
    ch.count = 200;
    for (int i = 0; i < 200; i++) ch.data[i] = (uint8_t)(i * 7);
    len = ecu_pack_chunk(&ch, p);
    TEST_ASSERT_EQUAL(206, len);
    TEST_ASSERT_TRUE(ecu_unpack_chunk(p, len, &ch2));
    TEST_ASSERT_EQUAL_MEMORY(&ch, &ch2, sizeof(ch));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_vetores_conhecidos);
    RUN_TEST(test_frame_formato_documentado);
    RUN_TEST(test_parser_ida_e_volta);
    RUN_TEST(test_parser_ressincroniza_depois_de_lixo);
    RUN_TEST(test_parser_ressincroniza_sem_linha_parada);
    RUN_TEST(test_parser_recusa_crc_errado);
    RUN_TEST(test_parser_frame_em_pedacos);
    RUN_TEST(test_payload_grande_demais_nao_monta);
    RUN_TEST(test_telemetria_ida_e_volta);
    RUN_TEST(test_mensagens_ida_e_volta);
    return UNITY_END();
}
