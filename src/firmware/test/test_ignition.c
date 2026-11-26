/*
 * Testes do avanco de ignicao e do momento da faisca (task 1.5.1.7). Rodam no
 * computador: `make -C src/firmware/test`.
 *
 * O mapa abaixo e um mapa de TESTE, nao e o de nenhum motor. Os valores
 * esperados foram calculados a mao; os que nao sao redondos foram conferidos
 * tambem com um script Python independente do C.
 */
#include <stddef.h>

#include "check.h"
#include "ecu_ignition.h"
#include "ecu_table.h"

/* Mapa de ignicao de teste, em decimos de grau antes do PMS. */
static const uint16_t RPM_AXIS[4] = {1000, 2000, 3000, 4000};
static const uint16_t LOAD_AXIS[3] = {30, 60, 90};
static uint16_t IGN_CELLS[12] = {
    150, 120, 100, /* 1000 RPM */
    220, 180, 150, /* 2000 RPM */
    280, 240, 200, /* 3000 RPM */
    320, 280, 240, /* 4000 RPM */
};

static void expect_angle(uint32_t angle_x10, uint16_t rpm, uint32_t want,
                         const char *what)
{
    uint32_t got = 0xDEADBEEFu;
    CHECK(ecu_angle_to_us(angle_x10, rpm, &got), what);
    CHECK_EQ_U(got, want, what);
}

static ecu_spark_in_t input(uint16_t rpm, uint16_t adv, uint16_t ref, uint32_t dwell)
{
    ecu_spark_in_t in;
    in.rpm = rpm;
    in.advance_x10 = adv;
    in.max_advance_x10 = 450;
    in.ref_btdc_x10 = ref;
    in.dwell_us = dwell;
    return in;
}

/* Conversao de angulo em tempo. */
static void test_angle_to_us(void)
{
    expect_angle(3600, 1000, 60000, "uma volta a 1000 RPM leva 60 ms");
    expect_angle(1800, 1000, 30000, "meia volta a 1000 RPM");
    expect_angle(3600, 6000, 10000, "uma volta a 6000 RPM leva 10 ms");
    expect_angle(0, 3000, 0, "angulo zero");

    /* 100 * 50000 / 3000 = 1666,67 -> 1667 */
    expect_angle(100, 1000, 1667, "10 graus a 1000 RPM");
    /* 2 * 50000 / 3000 = 33,33 -> 33 */
    expect_angle(2, 1000, 33, "arredonda para baixo");
    /* 3 * 50000 / 96 = 1562,5 -> 1563: o meio exato arredonda para cima */
    expect_angle(3, 32, 1563, "meio exato arredonda para cima");
}

static void test_angle_limits(void)
{
    /* 65535 * 50000 / 3 = 1092250000 */
    expect_angle(65535, 1, 1092250000u, "angulo grande a 1 RPM");
    expect_angle(UINT32_MAX, 1, UINT32_MAX, "satura em vez de estourar");

    uint32_t out = 777;
    CHECK(!ecu_angle_to_us(100, 0, &out) && out == 777, "rpm 0 e recusado");
    CHECK(!ecu_angle_to_us(100, 1000, NULL), "saida NULL");
}

/* Criterio: uma celula de ignicao conhecida produz o avanco esperado. */
static void test_known_cell_advance(void)
{
    const ecu_table_t map = {4, 3, RPM_AXIS, LOAD_AXIS, IGN_CELLS};
    uint16_t adv = 0;

    CHECK(ecu_table_lookup(&map, 3000, 60, &adv), "consulta ao mapa");
    CHECK_EQ_U(adv, 240, "3000 RPM / 60 kPa = 24,0 graus");

    CHECK(ecu_table_lookup(&map, 1000, 30, &adv), "canto");
    CHECK_EQ_U(adv, 150, "canto 1000 RPM / 30 kPa = 15,0 graus");

    /* Centro da celula (2000..3000) x (60..90): (180+150+240+200)/4 = 192,5 */
    CHECK(ecu_table_lookup(&map, 2500, 75, &adv), "ponto entre celulas");
    CHECK_EQ_U(adv, 193, "interpolado 19,25 graus arredonda para 19,3");
}

/* Do mapa ate o disparo: o caminho que o firmware vai fazer. */
static void test_map_to_spark(void)
{
    const ecu_table_t map = {4, 3, RPM_AXIS, LOAD_AXIS, IGN_CELLS};
    uint16_t adv = 0;
    CHECK(ecu_table_lookup(&map, 3000, 60, &adv), "consulta ao mapa");

    /* 60 graus antes do PMS na referencia, 24 de avanco: 36 graus a 3000 RPM. */
    const ecu_spark_in_t in = input(3000, adv, 600, 1500);
    ecu_spark_out_t out = {0, 0, 0};
    CHECK(ecu_spark_schedule(&in, &out) == ECU_SPARK_OK, "status OK");
    CHECK_EQ_U(out.spark_us, 2000, "36 graus a 3000 RPM = 2000 us");
    CHECK_EQ_U(out.dwell_start_us, 500, "comeca a carregar 1500 us antes");
    CHECK_EQ_U(out.advance_x10, 240, "avanco usado");
}

static void test_schedule_other_points(void)
{
    ecu_spark_out_t out = {0, 0, 0};

    /* 1000 RPM, 90 - 10 = 80 graus: 80 * 50000 / 30 ... = 13333,3 -> 13333 */
    ecu_spark_in_t in = input(1000, 100, 900, 3000);
    CHECK(ecu_spark_schedule(&in, &out) == ECU_SPARK_OK, "1000 RPM");
    CHECK_EQ_U(out.spark_us, 13333, "faisca a 1000 RPM");
    CHECK_EQ_U(out.dwell_start_us, 10333, "inicio do dwell a 1000 RPM");

    /* Mais rotacao, mesmo avanco: o atraso encolhe. 6000 RPM, 30 graus = 833 us. */
    in = input(6000, 300, 600, 500);
    CHECK(ecu_spark_schedule(&in, &out) == ECU_SPARK_OK, "6000 RPM");
    CHECK_EQ_U(out.spark_us, 833, "faisca a 6000 RPM");
    CHECK_EQ_U(out.dwell_start_us, 333, "inicio do dwell a 6000 RPM");
}

/* O avanco do mapa nunca passa do teto de seguranca. */
static void test_advance_cap(void)
{
    ecu_spark_out_t out = {0, 0, 0};

    /* Celula errada: 250,0 graus em vez de 25,0. Teto de 45,0 graus. */
    ecu_spark_in_t in = input(3000, 2500, 600, 500);
    CHECK(ecu_spark_schedule(&in, &out) == ECU_SPARK_OK, "avanco acima do teto");
    CHECK_EQ_U(out.advance_x10, 450, "usa o teto");
    /* 600 - 450 = 150 decimos de grau: 833,3 -> 833 */
    CHECK_EQ_U(out.spark_us, 833, "disparo calculado com o teto");
    CHECK_EQ_U(out.dwell_start_us, 333, "inicio do dwell com o teto");

    /* Abaixo do teto, nada muda. */
    in = input(3000, 240, 600, 500);
    CHECK(ecu_spark_schedule(&in, &out) == ECU_SPARK_OK, "abaixo do teto");
    CHECK_EQ_U(out.advance_x10, 240, "mantem o avanco do mapa");
}

/* Pouco tempo entre a referencia e a faisca: dwell menor que o pedido. */
static void test_short_dwell(void)
{
    ecu_spark_out_t out = {111, 222, 333};

    /* 6000 RPM, 30 graus = 833 us, mas o dwell pedido e 2500 us. */
    ecu_spark_in_t in = input(6000, 300, 600, 2500);
    CHECK(ecu_spark_schedule(&in, &out) == ECU_SPARK_SHORT_DWELL, "dwell curto");
    CHECK_EQ_U(out.spark_us, 833, "a faisca continua certa");
    CHECK_EQ_U(out.dwell_start_us, 0, "carrega ja a partir da referencia");

    /* Exatamente o dwell: ainda e valido, comeca em 0. */
    in = input(3000, 240, 600, 2000);
    CHECK(ecu_spark_schedule(&in, &out) == ECU_SPARK_OK, "dwell igual ao tempo");
    CHECK_EQ_U(out.spark_us, 2000, "faisca");
    CHECK_EQ_U(out.dwell_start_us, 0, "comeca na referencia");
}

/* Avanco maior que a distancia da referencia: precisa de uma referencia anterior. */
static void test_too_early(void)
{
    ecu_spark_out_t out = {111, 222, 333};

    ecu_spark_in_t in = input(3000, 440, 400, 1500);
    CHECK(ecu_spark_schedule(&in, &out) == ECU_SPARK_TOO_EARLY, "avanco passa da referencia");
    CHECK(out.dwell_start_us == 111 && out.spark_us == 222 && out.advance_x10 == 333,
          "TOO_EARLY nao escreve na saida");

    /* Avanco igual a distancia: faisca na propria referencia, sem dwell. */
    in = input(3000, 400, 400, 0);
    CHECK(ecu_spark_schedule(&in, &out) == ECU_SPARK_OK, "faisca na referencia");
    CHECK_EQ_U(out.spark_us, 0, "atraso zero");
    CHECK_EQ_U(out.dwell_start_us, 0, "dwell zero");
}

static void test_bad_input(void)
{
    ecu_spark_out_t out = {111, 222, 333};
    ecu_spark_in_t in = input(3000, 240, 600, 1500);

    CHECK(ecu_spark_schedule(NULL, &out) == ECU_SPARK_BAD_INPUT, "entrada NULL");
    CHECK(ecu_spark_schedule(&in, NULL) == ECU_SPARK_BAD_INPUT, "saida NULL");

    in.rpm = 0;
    CHECK(ecu_spark_schedule(&in, &out) == ECU_SPARK_BAD_INPUT, "rpm 0");
    CHECK(out.dwell_start_us == 111 && out.spark_us == 222 && out.advance_x10 == 333,
          "BAD_INPUT nao escreve na saida");
}

int main(void)
{
    test_angle_to_us();
    test_angle_limits();
    test_known_cell_advance();
    test_map_to_spark();
    test_schedule_other_points();
    test_advance_cap();
    test_short_dwell();
    test_too_early();
    test_bad_input();
    return check_finish();
}
