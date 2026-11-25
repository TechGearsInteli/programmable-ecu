/* Testes da calibracao: limites, ficha e gravacao (tasks 1.6.1.3, 1.6.1.7 e 1.6.1.8). */
#include <string.h>

#include "ecu/ecu_calib.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t flash[ECU_CALIB_BLOB_MAX];
static size_t flash_len;
static bool flash_fail;

static bool fake_write(void *ctx, const uint8_t *d, size_t n)
{
    (void)ctx;
    if (flash_fail) return false;
    memcpy(flash, d, n);
    flash_len = n;
    return true;
}

static size_t fake_read(void *ctx, uint8_t *buf, size_t cap)
{
    (void)ctx;
    if (flash_len > cap) return 0;
    memcpy(buf, flash, flash_len);
    return flash_len;
}

static const ecu_storage_t storage = {fake_write, fake_read, NULL};

static void test_padrao_e_valido(void)
{
    ecu_calib_t c;
    ecu_calib_defaults(&c);
    TEST_ASSERT_TRUE(ecu_calib_validate(&c));
    TEST_ASSERT_EQUAL(4, c.param[ECU_PAR_CYLINDERS]);
}

static void test_limites_da_celula(void)
{
    ecu_calib_t c;
    ecu_calib_defaults(&c);
    const int16_t before = c.table[ECU_TBL_FUEL].v[8][6];
    TEST_ASSERT_EQUAL(ECU_OK, ecu_calib_set_cell(&c, ECU_TBL_FUEL, 8, 6, 500));
    TEST_ASSERT_EQUAL(500, c.table[ECU_TBL_FUEL].v[8][6]);
    /* valor absurdo: 99,99 ms -> recusa e o valor fica */
    TEST_ASSERT_EQUAL(ECU_ERR_RANGE, ecu_calib_set_cell(&c, ECU_TBL_FUEL, 8, 6, 9999));
    TEST_ASSERT_EQUAL(500, c.table[ECU_TBL_FUEL].v[8][6]);
    TEST_ASSERT_EQUAL(ECU_ERR_INDEX, ecu_calib_set_cell(&c, ECU_TBL_FUEL, 16, 0, 500));
    TEST_ASSERT_EQUAL(ECU_ERR_INDEX, ecu_calib_set_cell(&c, ECU_TBL_WARMUP, 1, 0, 0)); /* curva tem 1 linha */
    TEST_ASSERT_EQUAL(ECU_ERR_TABLE, ecu_calib_set_cell(&c, 99, 0, 0, 0));
    TEST_ASSERT_EQUAL(ECU_ERR_RANGE, ecu_calib_set_cell(&c, ECU_TBL_IGN, 0, 0, 600)); /* 60 graus */
    (void)before;
}

static void test_ficha_do_motor(void)
{
    ecu_calib_t c;
    ecu_calib_defaults(&c);
    TEST_ASSERT_EQUAL(ECU_OK, ecu_calib_set_param(&c, ECU_PAR_CYLINDERS, 3));
    TEST_ASSERT_EQUAL(ECU_ERR_PARAM, ecu_calib_set_param(&c, ECU_PAR_CYLINDERS, 7));
    TEST_ASSERT_EQUAL(ECU_ERR_RANGE, ecu_calib_set_param(&c, ECU_PAR_CYLINDERS, 12));
    TEST_ASSERT_EQUAL(ECU_OK, ecu_calib_set_param(&c, ECU_PAR_TRIGGER_MISSING, 2)); /* 36-2 */
    TEST_ASSERT_EQUAL(ECU_ERR_PARAM, ecu_calib_set_param(&c, ECU_PAR_TRIGGER_TEETH, 4)); /* 4-2 nao fecha */
    TEST_ASSERT_EQUAL(ECU_ERR_PARAM, ecu_calib_set_param(&c, 99, 1));
    TEST_ASSERT_EQUAL(3, c.param[ECU_PAR_CYLINDERS]);
}

static void test_blob_ida_e_volta(void)
{
    ecu_calib_t a, b;
    ecu_calib_defaults(&a);
    a.table[ECU_TBL_FUEL].v[3][4] = 777;
    a.param[ECU_PAR_INJ_FLOW] = 310;
    uint8_t blob[ECU_CALIB_BLOB_MAX];
    const size_t n = ecu_calib_serialize(&a, blob, sizeof(blob));
    TEST_ASSERT_EQUAL(ecu_calib_blob_size(), n);
    TEST_ASSERT_EQUAL(1342, n);
    TEST_ASSERT_EQUAL_HEX8('T', blob[0]);
    TEST_ASSERT_EQUAL_HEX8('G', blob[1]);
    TEST_ASSERT_EQUAL_HEX8('C', blob[2]);
    TEST_ASSERT_EQUAL_HEX8('L', blob[3]);
    memset(&b, 0, sizeof(b));
    TEST_ASSERT_EQUAL(ECU_OK, ecu_calib_deserialize(&b, blob, n));
    TEST_ASSERT_EQUAL(777, b.table[ECU_TBL_FUEL].v[3][4]);
    TEST_ASSERT_EQUAL(310, b.param[ECU_PAR_INJ_FLOW]);
    TEST_ASSERT_EQUAL_HEX32(ecu_calib_crc(&a), ecu_calib_crc(&b));
}

static void test_blob_corrompido_e_recusado(void)
{
    ecu_calib_t a, b;
    ecu_calib_defaults(&a);
    uint8_t blob[ECU_CALIB_BLOB_MAX];
    const size_t n = ecu_calib_serialize(&a, blob, sizeof(blob));
    memset(&b, 0x55, sizeof(b));
    ecu_calib_t untouched = b;

    blob[100] ^= 0x40; /* CRC nao fecha */
    TEST_ASSERT_EQUAL(ECU_ERR_BACKUP, ecu_calib_deserialize(&b, blob, n));
    TEST_ASSERT_EQUAL_MEMORY(&untouched, &b, sizeof(b)); /* nao mexeu */
    blob[100] ^= 0x40;

    blob[4] = 9; /* versao desconhecida */
    TEST_ASSERT_EQUAL(ECU_ERR_BACKUP, ecu_calib_deserialize(&b, blob, n));
    blob[4] = ECU_CALIB_VERSION;

    TEST_ASSERT_EQUAL(ECU_ERR_BACKUP, ecu_calib_deserialize(&b, blob, n - 1)); /* cortado */
    TEST_ASSERT_EQUAL(ECU_OK, ecu_calib_deserialize(&b, blob, n));
}

static void test_blob_com_valor_fora_do_limite_e_recusado(void)
{
    /* CRC certo, mas uma celula com 99 ms: o motor nao aceita nem vindo de backup */
    ecu_calib_t a, b;
    ecu_calib_defaults(&a);
    a.table[ECU_TBL_FUEL].v[0][0] = 9900;
    uint8_t blob[ECU_CALIB_BLOB_MAX];
    const size_t n = ecu_calib_serialize(&a, blob, sizeof(blob));
    TEST_ASSERT_EQUAL(ECU_ERR_BACKUP, ecu_calib_deserialize(&b, blob, n));
}

static void test_flash_vazia_carrega_padrao(void)
{
    ecu_calib_t c, def;
    flash_len = 0;
    flash_fail = false;
    TEST_ASSERT_FALSE(ecu_calib_load(&c, &storage));
    ecu_calib_defaults(&def);
    TEST_ASSERT_EQUAL_MEMORY(&def, &c, sizeof(c));
}

static void test_gravar_e_reiniciar_mantem_a_celula(void)
{
    ecu_calib_t c, after;
    ecu_calib_defaults(&c);
    flash_len = 0;
    flash_fail = false;
    TEST_ASSERT_EQUAL(ECU_OK, ecu_calib_set_cell(&c, ECU_TBL_FUEL, 8, 6, 512));
    uint32_t crc = 0;
    TEST_ASSERT_EQUAL(ECU_OK, ecu_calib_save(&c, &storage, &crc));
    TEST_ASSERT_EQUAL_HEX32(ecu_calib_crc(&c), crc);
    /* "reinicio": calibracao nova lida da flash */
    TEST_ASSERT_TRUE(ecu_calib_load(&after, &storage));
    TEST_ASSERT_EQUAL(512, after.table[ECU_TBL_FUEL].v[8][6]);

    flash_fail = true;
    TEST_ASSERT_EQUAL(ECU_ERR_FLASH, ecu_calib_save(&c, &storage, NULL));
    flash_fail = false;
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_padrao_e_valido);
    RUN_TEST(test_limites_da_celula);
    RUN_TEST(test_ficha_do_motor);
    RUN_TEST(test_blob_ida_e_volta);
    RUN_TEST(test_blob_corrompido_e_recusado);
    RUN_TEST(test_blob_com_valor_fora_do_limite_e_recusado);
    RUN_TEST(test_flash_vazia_carrega_padrao);
    RUN_TEST(test_gravar_e_reiniciar_mantem_a_celula);
    return UNITY_END();
}
