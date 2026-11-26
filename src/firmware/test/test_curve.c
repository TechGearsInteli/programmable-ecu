/*
 * Testes da curva 1D com interpolacao linear (usada no dead time, 1.5.1.5).
 * Valores esperados calculados a mao.
 */
#include <stddef.h>

#include "check.h"
#include "ecu_curve.h"

/* Dead time de teste: tensao (mV) -> us. Desce conforme a tensao sobe. */
static const uint16_t VX[5] = {8000, 10000, 12000, 14000, 16000};
static const uint16_t VY[5] = {1600, 1200, 900, 700, 600};

static void expect_curve(const ecu_curve_t *c, uint16_t x, uint16_t want,
                         const char *what)
{
    uint16_t got = 0xFFFF;
    const bool ok = ecu_curve_lookup(c, x, &got);
    CHECK(ok, what);
    CHECK_EQ_U(got, want, what);
}

static void test_nodes_exact(void)
{
    const ecu_curve_t c = {5, VX, VY};
    for (uint8_t i = 0; i < 5; i++) {
        expect_curve(&c, VX[i], VY[i], "no da curva");
    }
}

static void test_between_nodes(void)
{
    const ecu_curve_t c = {5, VX, VY};
    expect_curve(&c, 11000, 1050, "meio entre 1200 e 900");
    expect_curve(&c, 13000, 800, "meio entre 900 e 700");
    expect_curve(&c, 12500, 850, "tres quartos entre 900 e 700");
    expect_curve(&c, 8500, 1500, "um quarto entre 1600 e 1200");
}

/* Arredondamento para o inteiro mais proximo, nos dois sentidos. */
static void test_rounding(void)
{
    const ecu_curve_t c = {5, VX, VY};
    /* 8001: (1600*1999 + 1200*1)/2000 = 1599,8 -> 1600 */
    expect_curve(&c, 8001, 1600, "1599,8 arredonda para cima");
    /* 9999: (1600*1 + 1200*1999)/2000 = 2400400/2000 = 1200,2 -> 1200 */
    expect_curve(&c, 9999, 1200, "1200,2 arredonda para baixo");
    /* 8003: (1600*1997 + 1200*3)/2000 = 3198800/2000 = 1599,4 -> 1599 */
    expect_curve(&c, 8003, 1599, "1599,4 arredonda para baixo");

    /* Meio exato arredonda para cima, tanto subindo quanto descendo. */
    static const uint16_t hx[2] = {0, 2};
    static const uint16_t up[2] = {0, 1};
    static const uint16_t down[2] = {1, 0};
    const ecu_curve_t cu = {2, hx, up};
    const ecu_curve_t cd = {2, hx, down};
    expect_curve(&cu, 1, 1, "0,5 subindo arredonda para cima");
    expect_curve(&cd, 1, 1, "0,5 descendo arredonda para cima");
}

/* Fora da faixa nao extrapola. */
static void test_clamps(void)
{
    const ecu_curve_t c = {5, VX, VY};
    expect_curve(&c, 0, 1600, "x = 0");
    expect_curve(&c, 7999, 1600, "logo abaixo do primeiro ponto");
    expect_curve(&c, 16001, 600, "logo acima do ultimo ponto");
    expect_curve(&c, 65535, 600, "x maximo");
}

/* Valores de 16 bits no limite nao podem estourar. */
static void test_no_overflow(void)
{
    static const uint16_t ax[2] = {0, 65535};
    static const uint16_t ay[2] = {0, 65535};
    static const uint16_t fy[2] = {65535, 65535};
    const ecu_curve_t ramp = {2, ax, ay};
    const ecu_curve_t flat = {2, ax, fy};
    expect_curve(&ramp, 32768, 32768, "rampa 0..65535 no meio");
    expect_curve(&ramp, 65535, 65535, "rampa no fim");
    expect_curve(&flat, 12345, 65535, "curva toda no maximo");
}

static void test_invalid(void)
{
    static const uint16_t ok[2] = {0, 10};
    static const uint16_t one[1] = {0};
    static const uint16_t flat[3] = {0, 5, 5};
    static const uint16_t down[3] = {0, 9, 5};
    uint16_t out = 777;

    const ecu_curve_t good = {2, ok, ok};
    const ecu_curve_t short1 = {1, one, one};
    const ecu_curve_t repeated = {3, flat, flat};
    const ecu_curve_t descending = {3, down, down};
    const ecu_curve_t no_x = {2, NULL, ok};
    const ecu_curve_t no_y = {2, ok, NULL};
    const ecu_curve_t empty = {0, ok, ok};

    CHECK(ecu_curve_valid(&good), "curva valida");
    CHECK(!ecu_curve_valid(NULL), "curva NULL");
    CHECK(!ecu_curve_valid(&short1), "1 ponto");
    CHECK(!ecu_curve_valid(&repeated), "x repetido");
    CHECK(!ecu_curve_valid(&descending), "x decrescente");
    CHECK(!ecu_curve_valid(&no_x), "x NULL");
    CHECK(!ecu_curve_valid(&no_y), "y NULL");
    CHECK(!ecu_curve_valid(&empty), "0 pontos");

    CHECK(!ecu_curve_lookup(NULL, 1, &out) && out == 777, "lookup em NULL");
    CHECK(!ecu_curve_lookup(&short1, 1, &out) && out == 777, "lookup em curva curta");
    CHECK(!ecu_curve_lookup(&repeated, 1, &out) && out == 777, "lookup com x repetido");
    CHECK(!ecu_curve_lookup(&good, 1, NULL), "lookup sem saida");
}

int main(void)
{
    test_nodes_exact();
    test_between_nodes();
    test_rounding();
    test_clamps();
    test_no_overflow();
    test_invalid();
    return check_finish();
}
