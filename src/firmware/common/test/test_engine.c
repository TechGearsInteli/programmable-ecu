/* Testes do calculo: interpolacao, PW, correcoes 2D, ficha (tasks 1.6.1.6 e 1.6.1.7). */
#include <string.h>

#include "ecu/ecu_engine.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void zero_curve(ecu_calib_t *c, unsigned t)
{
    for (unsigned i = 0; i < ECU_CURVE_POINTS; i++) c->table[t].v[0][i] = 0;
}

/* Calibracao "neutra": correcoes em zero, tempo morto zero, bicos iguais. */
static void neutral(ecu_calib_t *c)
{
    ecu_calib_defaults(c);
    for (unsigned t = ECU_TBL_WARMUP; t < ECU_TBL_COUNT; t++) {
        zero_curve(c, t);
    }
}

/* Ponto de mesa conhecido: 3000 rpm e 60 kPa caem exatamente numa celula
 * (coluna 7, linha 8), entao o valor interpolado e o valor da celula. */
static ecu_inputs_t bench(void)
{
    ecu_inputs_t in;
    memset(&in, 0, sizeof(in));
    in.rpm = 3000;
    in.map_kpa_x10 = 600;
    in.tps_pct = 30;
    in.clt_x10 = 900;
    in.iat_x10 = 200;
    in.vbat_mv = 13800;
    in.sync = true;
    return in;
}

static ecu_outputs_t run(const ecu_calib_t *c, const ecu_inputs_t *in)
{
    ecu_engine_state_t s;
    ecu_outputs_t out;
    ecu_engine_init(&s);
    ecu_engine_compute(c, in, &s, 1000, &out);
    return out;
}

static void test_interpolacao_exemplo_da_teoria_do_motor(void)
{
    /* docs/teoria-do-motor.md: 2500 rpm e 60 kPa entre 3,6 / 4,2 / 4,8 / 5,6 ms -> 4,55 ms */
    ecu_map_t m;
    memset(&m, 0, sizeof(m));
    m.x[0] = 2000;
    m.x[1] = 3000;
    m.y[0] = 50;
    m.y[1] = 70;
    m.v[0][0] = 360;
    m.v[0][1] = 420;
    m.v[1][0] = 480;
    m.v[1][1] = 560;
    TEST_ASSERT_EQUAL(455, ecu_interp2d(&m, 2, 2, 2500, 1, 600, 10));
    /* fora da grade usa a borda */
    TEST_ASSERT_EQUAL(360, ecu_interp2d(&m, 2, 2, 100, 1, 0, 10));
    TEST_ASSERT_EQUAL(560, ecu_interp2d(&m, 2, 2, 9000, 1, 2000, 10));
    /* sobre um ponto da grade da o proprio valor */
    TEST_ASSERT_EQUAL(420, ecu_interp2d(&m, 2, 2, 3000, 1, 500, 10));
}

static void test_interpolacao_1d_e_mais_proximo(void)
{
    const int16_t axis[4] = {80, 100, 120, 140}; /* V x10 */
    const int16_t val[4] = {1600, 1150, 880, 720};
    TEST_ASSERT_EQUAL(1015, ecu_interp1d(axis, 4, 100, val, 11000)); /* 11,0 V, entre 1150 e 880 */
    TEST_ASSERT_EQUAL(1600, ecu_interp1d(axis, 4, 100, val, 5000));
    TEST_ASSERT_EQUAL(720, ecu_interp1d(axis, 4, 100, val, 16000));
    TEST_ASSERT_EQUAL(1, ecu_axis_nearest(axis, 4, 100, 10900));
    TEST_ASSERT_EQUAL(2, ecu_axis_nearest(axis, 4, 100, 11100));
}

static void test_correcoes_em_zero_pw_igual_a_celula(void)
{
    ecu_calib_t c;
    neutral(&c);
    const ecu_inputs_t in = bench();
    const ecu_outputs_t out = run(&c, &in);
    TEST_ASSERT_EQUAL(8, out.fuel_row);
    TEST_ASSERT_EQUAL(7, out.fuel_col);
    TEST_ASSERT_EQUAL(out.fuel_cell, out.fuel_interp_x100);
    TEST_ASSERT_EQUAL(out.fuel_cell * 10, out.pw_us); /* ms x100 -> us */
}

static void test_cada_correcao_muda_o_pw(void)
{
    ecu_calib_t c;
    neutral(&c);
    const ecu_inputs_t in = bench();
    const uint32_t base = run(&c, &in).pw_us;

    /* aquecimento +20 % em toda a curva -> PW x 1,20 */
    for (int i = 0; i < 8; i++) c.table[ECU_TBL_WARMUP].v[0][i] = 200;
    ecu_outputs_t o = run(&c, &in);
    TEST_ASSERT_EQUAL(200, o.corr_warm_x10);
    TEST_ASSERT_EQUAL(base * 12 / 10, o.pw_us);
    zero_curve(&c, ECU_TBL_WARMUP);

    /* ar -10 % -> PW x 0,90 */
    for (int i = 0; i < 8; i++) c.table[ECU_TBL_IAT].v[0][i] = -100;
    o = run(&c, &in);
    TEST_ASSERT_EQUAL(base * 9 / 10, o.pw_us);
    zero_curve(&c, ECU_TBL_IAT);

    /* bateria: tempo morto de 800 us e somado */
    for (int i = 0; i < 8; i++) c.table[ECU_TBL_DEADTIME].v[0][i] = 800;
    o = run(&c, &in);
    TEST_ASSERT_EQUAL(base + 800, o.pw_us);
    zero_curve(&c, ECU_TBL_DEADTIME);

    /* partida: so vale abaixo do rpm de partida */
    for (int i = 0; i < 8; i++) c.table[ECU_TBL_CRANK].v[0][i] = 1000; /* +100 % */
    o = run(&c, &in);
    TEST_ASSERT_FALSE(o.cranking);
    TEST_ASSERT_EQUAL(base, o.pw_us);
    ecu_inputs_t crank = in;
    crank.rpm = 250;
    o = run(&c, &crank);
    TEST_ASSERT_TRUE(o.cranking);
    TEST_ASSERT_EQUAL(1000, o.corr_crank_x10);
    TEST_ASSERT_EQUAL(o.fuel_interp_x100 * 10 * 2, o.pw_us);
}

static void test_aceleracao_rapida(void)
{
    ecu_calib_t c;
    neutral(&c);
    for (int i = 0; i < 8; i++) c.table[ECU_TBL_ACCEL].v[0][i] = (int16_t)(i >= 2 ? 300 : 0); /* +30 % acima de 100 %/s */
    ecu_inputs_t in = bench();
    ecu_engine_state_t s;
    ecu_outputs_t o;
    ecu_engine_init(&s);
    ecu_engine_compute(&c, &in, &s, 1000, &o);
    TEST_ASSERT_EQUAL(0, o.corr_accel_x10);
    in.tps_pct = 60; /* +30 % em 100 ms = 300 %/s */
    ecu_engine_compute(&c, &in, &s, 1100, &o);
    TEST_ASSERT_EQUAL(300, o.corr_accel_x10);
    TEST_ASSERT_EQUAL(o.fuel_interp_x100 * 10 * 13 / 10, o.pw_us);
    ecu_engine_compute(&c, &in, &s, 1300, &o); /* ainda segurando */
    TEST_ASSERT_EQUAL(300, o.corr_accel_x10);
    ecu_engine_compute(&c, &in, &s, 1500, &o); /* passou o tempo: volta a zero */
    TEST_ASSERT_EQUAL(0, o.corr_accel_x10);
}

static void test_trocar_bico_muda_pw_da_mesma_celula(void)
{
    ecu_calib_t c;
    neutral(&c);
    const ecu_inputs_t in = bench();
    const ecu_outputs_t a = run(&c, &in);
    c.param[ECU_PAR_INJ_FLOW] = 300; /* bico 25 % maior */
    const ecu_outputs_t b = run(&c, &in);
    TEST_ASSERT_EQUAL(a.fuel_cell, b.fuel_cell);
    TEST_ASSERT_EQUAL(800, b.inj_scale_x1000);
    TEST_ASSERT_EQUAL((a.pw_us * 240 + 150) / 300, b.pw_us);
}

static void test_cilindros_mudam_a_sequencia(void)
{
    uint8_t order[8];
    uint16_t spacing = 0;
    TEST_ASSERT_EQUAL(4, ecu_firing_order(4, order, &spacing));
    const uint8_t four[] = {1, 3, 4, 2};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(four, order, 4);
    TEST_ASSERT_EQUAL(180, spacing);
    TEST_ASSERT_EQUAL(3, ecu_firing_order(3, order, &spacing));
    const uint8_t three[] = {1, 2, 3};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(three, order, 3);
    TEST_ASSERT_EQUAL(240, spacing);
    TEST_ASSERT_EQUAL(0, ecu_firing_order(7, order, &spacing));
}

static void test_avanco_e_dwell(void)
{
    ecu_calib_t c;
    neutral(&c);
    const ecu_inputs_t in = bench();
    ecu_outputs_t o = run(&c, &in);
    TEST_ASSERT_EQUAL(c.table[ECU_TBL_IGN].v[8][7], o.advance_x10);
    TEST_ASSERT_EQUAL(3000, o.dwell_us);
    c.table[ECU_TBL_IGN].v[8][7] = 155;
    o = run(&c, &in);
    TEST_ASSERT_EQUAL(155, o.advance_x10);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_interpolacao_exemplo_da_teoria_do_motor);
    RUN_TEST(test_interpolacao_1d_e_mais_proximo);
    RUN_TEST(test_correcoes_em_zero_pw_igual_a_celula);
    RUN_TEST(test_cada_correcao_muda_o_pw);
    RUN_TEST(test_aceleracao_rapida);
    RUN_TEST(test_trocar_bico_muda_pw_da_mesma_celula);
    RUN_TEST(test_cilindros_mudam_a_sequencia);
    RUN_TEST(test_avanco_e_dwell);
    return UNITY_END();
}
