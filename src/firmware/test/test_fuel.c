/*
 * Testes do calculo do tempo de bico (task 1.5.1.5). Rodam no computador:
 * `make -C src/firmware/test`.
 *
 * O caso de mesa e o do proprio doc do time (teoria-do-motor, Quadro 6): o
 * mapa 4x4 em ms, e o ponto 2500 RPM / 60 kPa que da 4,55 ms. Aqui as celulas
 * estao em us. Todos os valores esperados foram calculados a mao.
 *
 * Os numeros de dead time sao de teste, para exercitar a curva. Os do injetor
 * real vem da folha de dados dele, quando o hardware definir qual e.
 */
#include <stddef.h>

#include "check.h"
#include "ecu_fuel.h"
#include "ecu_table.h"

/* Dead time de teste: tensao (mV) -> us. */
static const uint16_t VX[5] = {8000, 10000, 12000, 14000, 16000};
static const uint16_t VY[5] = {1600, 1200, 900, 700, 600};
static const ecu_curve_t DEAD = {5, VX, VY};

/* Mapa do Quadro 6 do doc de teoria, em us. */
static const uint16_t RPM_AXIS[4] = {1000, 2000, 3000, 4000};
static const uint16_t LOAD_AXIS[4] = {30, 50, 70, 90};
static uint16_t FUEL_CELLS[16] = {
    2000, 3000, 4000, 5000, /* 1000 RPM */
    2400, 3600, 4800, 6000, /* 2000 RPM */
    2800, 4200, 5600, 7000, /* 3000 RPM */
    3200, 4800, 6400, 8000, /* 4000 RPM */
};

/* Entrada comum: base 4550 us, sem correcao, 14 V (dead time 700 us). */
static ecu_fuel_in_t base_input(void)
{
    ecu_fuel_in_t in;
    in.base_us = 4550;
    in.corr_permille = NULL;
    in.n_corr = 0;
    in.dead_time = &DEAD;
    in.vbat_mv = 14000;
    in.max_us = UINT32_MAX;
    return in;
}

static void expect_pw(const ecu_fuel_in_t *in, uint32_t want, const char *what)
{
    uint32_t got = 0xDEADBEEFu;
    const bool ok = ecu_fuel_pulse_width_us(in, &got);
    CHECK(ok, what);
    CHECK_EQ_U(got, want, what);
}

/* O caso de mesa do doc: 2500 RPM e 60 kPa dao 4,55 ms no mapa. */
static void test_known_bench_case(void)
{
    const ecu_table_t map = {4, 4, RPM_AXIS, LOAD_AXIS, FUEL_CELLS};
    uint16_t base = 0;
    CHECK(ecu_table_lookup(&map, 2500, 60, &base), "consulta ao mapa");
    CHECK_EQ_U(base, 4550, "mapa a 2500 RPM / 60 kPa = 4,55 ms (doc)");

    ecu_fuel_in_t in = base_input();
    in.base_us = base;
    expect_pw(&in, 5250, "4550 + dead time 700");

    /* Canto do mapa: 1000 RPM / 30 kPa = 2000 us, mais 700 de dead time. */
    CHECK(ecu_table_lookup(&map, 1000, 30, &base), "consulta ao canto");
    in.base_us = base;
    expect_pw(&in, 2700, "canto do mapa + dead time");
}

/* Criterio: caso sem correcao. */
static void test_without_correction(void)
{
    ecu_fuel_in_t in = base_input();
    expect_pw(&in, 5250, "n_corr = 0 e ponteiro NULL");

    const uint16_t neutral[1] = {ECU_CORR_NONE};
    in.corr_permille = neutral;
    in.n_corr = 1;
    expect_pw(&in, 5250, "fator 1000 nao altera");
}

/* Criterio: caso com correcao. */
static void test_with_correction(void)
{
    ecu_fuel_in_t in = base_input();

    const uint16_t clt[1] = {1200}; /* +20 % */
    in.corr_permille = clt;
    in.n_corr = 1;
    expect_pw(&in, 6160, "4550 * 1,20 = 5460, + 700");

    const uint16_t two[2] = {1200, 970}; /* +20 % e -3 % */
    in.corr_permille = two;
    in.n_corr = 2;
    /* 5460 * 0,970 = 5296,2 -> 5296; + 700 */
    expect_pw(&in, 5996, "duas correcoes em sequencia");
}

/* A ordem dos fatores muda o resultado em 1 us: comportamento documentado. */
static void test_order_changes_by_one_us(void)
{
    ecu_fuel_in_t in = base_input();

    const uint16_t a[2] = {1200, 970};
    in.corr_permille = a;
    in.n_corr = 2;
    uint32_t pa = 0;
    CHECK(ecu_fuel_pulse_width_us(&in, &pa), "ordem A");

    const uint16_t b[2] = {970, 1200};
    in.corr_permille = b;
    uint32_t pb = 0;
    CHECK(ecu_fuel_pulse_width_us(&in, &pb), "ordem B");

    /* B: 4550*0,970 = 4413,5 -> 4414; 4414*1,2 = 5296,8 -> 5297; + 700 */
    CHECK_EQ_U(pa, 5996, "ordem 1200 depois 970");
    CHECK_EQ_U(pb, 5997, "ordem 970 depois 1200");
}

static void test_rounding_of_a_factor(void)
{
    ecu_fuel_in_t in = base_input();
    const uint16_t half[1] = {500};
    in.base_us = 1001;
    in.corr_permille = half;
    in.n_corr = 1;
    /* 1001 * 0,5 = 500,5 -> 501 (meio para cima); + 700 */
    expect_pw(&in, 1201, "meio arredonda para cima");
}

/* O dead time acompanha a tensao da bateria. */
static void test_dead_time_follows_battery(void)
{
    ecu_fuel_in_t in = base_input();

    in.vbat_mv = 12000;
    expect_pw(&in, 5450, "12 V: dead time 900");
    in.vbat_mv = 13000;
    expect_pw(&in, 5350, "13 V: meio entre 900 e 700 = 800");
    in.vbat_mv = 8000;
    expect_pw(&in, 6150, "8 V: dead time 1600");
    in.vbat_mv = 5000;
    expect_pw(&in, 6150, "abaixo da curva vale a borda");
    in.vbat_mv = 20000;
    expect_pw(&in, 5150, "acima da curva vale a borda (600)");
}

static void test_no_dead_time_curve(void)
{
    static const uint16_t zy[2] = {0, 0};
    static const uint16_t zx[2] = {0, 20000};
    const ecu_curve_t zero = {2, zx, zy};
    ecu_fuel_in_t in = base_input();
    in.dead_time = &zero;
    expect_pw(&in, 4550, "curva com y = 0 nao compensa");
}

/* Corte de combustivel: o injetor nem e energizado, entao sem dead time. */
static void test_fuel_cut(void)
{
    ecu_fuel_in_t in = base_input();

    const uint16_t cut[1] = {0};
    in.corr_permille = cut;
    in.n_corr = 1;
    expect_pw(&in, 0, "fator 0 corta o combustivel");

    const uint16_t cut_late[2] = {1200, 0};
    in.corr_permille = cut_late;
    in.n_corr = 2;
    expect_pw(&in, 0, "corte depois de outra correcao");

    in.corr_permille = NULL;
    in.n_corr = 0;
    in.base_us = 0;
    expect_pw(&in, 0, "base 0 nao soma dead time");
}

/* Teto do tempo comandado (limite do ciclo do motor). */
static void test_cycle_limit(void)
{
    ecu_fuel_in_t in = base_input();

    in.max_us = 5000;
    expect_pw(&in, 5000, "5250 limitado a 5000");
    in.max_us = 5250;
    expect_pw(&in, 5250, "no limite exato nao muda");
    in.max_us = 0;
    expect_pw(&in, 0, "limite 0");
}

/* Fatores enormes nao podem estourar a conta. */
static void test_no_overflow(void)
{
    ecu_fuel_in_t in = base_input();
    in.base_us = 65535;

    const uint16_t big[1] = {65535};
    in.corr_permille = big;
    in.n_corr = 1;
    /* 65535 * 65535 / 1000 = 4294836,2 -> 4294836; + 700 */
    expect_pw(&in, 4295536, "um fator de 65535");

    const uint16_t many[8] = {65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535};
    in.corr_permille = many;
    in.n_corr = 8;
    expect_pw(&in, UINT32_MAX, "oito fatores enormes saturam sem estourar");

    /*
     * Aqui a saturacao muda o resultado, nao so o teto final: quatro fatores
     * enormes saturam em UINT32_MAX (4294967295), e o 0,5 seguinte da
     * (4294967295 * 500 + 500) / 1000 = 2147483648, mais 700 de dead time.
     * Sem saturar, o valor ficaria em 9,2e9 e seria cortado no teto.
     * Esperado conferido com um script Python independente do C.
     */
    const uint16_t sat[5] = {65535, 65535, 65535, 65535, 500};
    in.corr_permille = sat;
    in.n_corr = 5;
    expect_pw(&in, 2147484348u, "saturacao seguida de fator menor que 1");
}

static void test_invalid_inputs(void)
{
    uint32_t out = 777;
    ecu_fuel_in_t in = base_input();

    CHECK(!ecu_fuel_pulse_width_us(NULL, &out) && out == 777, "entrada NULL");
    CHECK(!ecu_fuel_pulse_width_us(&in, NULL), "saida NULL");

    ecu_fuel_in_t bad = base_input();
    bad.n_corr = 2;
    bad.corr_permille = NULL;
    CHECK(!ecu_fuel_pulse_width_us(&bad, &out) && out == 777, "n_corr > 0 sem vetor");

    bad = base_input();
    bad.dead_time = NULL;
    CHECK(!ecu_fuel_pulse_width_us(&bad, &out) && out == 777, "sem curva de dead time");

    static const uint16_t one[1] = {0};
    const ecu_curve_t short_curve = {1, one, one};
    bad = base_input();
    bad.dead_time = &short_curve;
    CHECK(!ecu_fuel_pulse_width_us(&bad, &out) && out == 777, "curva de dead time invalida");
}

int main(void)
{
    test_known_bench_case();
    test_without_correction();
    test_with_correction();
    test_order_changes_by_one_us();
    test_rounding_of_a_factor();
    test_dead_time_follows_battery();
    test_no_dead_time_curve();
    test_fuel_cut();
    test_cycle_limit();
    test_no_overflow();
    test_invalid_inputs();
    return check_finish();
}
