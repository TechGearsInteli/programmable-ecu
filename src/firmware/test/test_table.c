/*
 * Testes da interpolacao bilinear (task 1.5.1.4). Rodam no computador, sem
 * placa: `make -C src/firmware/test`. Sai com codigo != 0 se algum falhar.
 *
 * Os valores esperados foram calculados a mao, nao com o proprio codigo.
 */
#include <stdio.h>

#include "ecu_table.h"

static int g_checks;
static int g_failures;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        g_checks++;                                                            \
        if (!(cond)) {                                                         \
            g_failures++;                                                      \
            printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, (msg));           \
        }                                                                      \
    } while (0)

/* Confere o valor interpolado em (rpm, load). */
static void expect_lookup(const ecu_table_t *t, uint16_t rpm, uint16_t load,
                          uint16_t want, const char *what)
{
    uint16_t got = 0;
    const bool ok = ecu_table_lookup(t, rpm, load, &got);
    g_checks++;
    if (!ok || got != want) {
        g_failures++;
        printf("FALHOU %s: (%u, %u) deu %u (ok=%d), esperado %u\n", what,
               (unsigned)rpm, (unsigned)load, (unsigned)got, (int)ok,
               (unsigned)want);
    }
}

static void expect_cell(const ecu_table_t *t, uint16_t rpm, uint16_t load,
                        uint8_t want_i, uint8_t want_j, const char *what)
{
    ecu_cell_t c = {0xFF, 0xFF};
    const bool ok = ecu_table_locate(t, rpm, load, &c);
    g_checks++;
    if (!ok || c.rpm_idx != want_i || c.load_idx != want_j) {
        g_failures++;
        printf("FALHOU %s: (%u, %u) deu celula (%u, %u) ok=%d, esperado (%u, %u)\n",
               what, (unsigned)rpm, (unsigned)load, (unsigned)c.rpm_idx,
               (unsigned)c.load_idx, (int)ok, (unsigned)want_i, (unsigned)want_j);
    }
}

/*
 * Grade 3 x 3 com eixos nao uniformes, para pegar erro de indice:
 *
 *            carga=30  carga=60  carga=100
 *  rpm=1000     100       200        300
 *  rpm=2000     150       250        350
 *  rpm=4000     300       400        500
 */
static const uint16_t RPM3[3] = {1000, 2000, 4000};
static const uint16_t LOAD3[3] = {30, 60, 100};

static void make_grid(ecu_table_t *t, uint16_t cells[9])
{
    static const uint16_t base[9] = {100, 200, 300, 150, 250, 350, 300, 400, 500};
    for (int i = 0; i < 9; i++) {
        cells[i] = base[i];
    }
    t->n_rpm = 3;
    t->n_load = 3;
    t->rpm_axis = RPM3;
    t->load_axis = LOAD3;
    t->cells = cells;
}

/* Criterio 1: os cantos devolvem o valor exato da celula. */
static void test_corners_exact(void)
{
    uint16_t cells[9];
    ecu_table_t t;
    make_grid(&t, cells);

    expect_lookup(&t, 1000, 30, 100, "canto rpm min / carga min");
    expect_lookup(&t, 1000, 100, 300, "canto rpm min / carga max");
    expect_lookup(&t, 4000, 30, 300, "canto rpm max / carga min");
    expect_lookup(&t, 4000, 100, 500, "canto rpm max / carga max");
}

/* Todos os nos da grade, inclusive os do meio, devolvem a propria celula. */
static void test_every_node_exact(void)
{
    uint16_t cells[9];
    ecu_table_t t;
    make_grid(&t, cells);

    for (uint8_t i = 0; i < 3; i++) {
        for (uint8_t j = 0; j < 3; j++) {
            expect_lookup(&t, RPM3[i], LOAD3[j], cells[i * 3 + j], "no da grade");
        }
    }
}

/* Criterio 2: um ponto no meio de quatro celulas devolve o valor esperado. */
static void test_center_of_four_cells(void)
{
    uint16_t cells[9];
    ecu_table_t t;
    make_grid(&t, cells);

    /* Celula (rpm 2000..4000) x (carga 60..100), centro: (250+400+350+500)/4. */
    expect_lookup(&t, 3000, 80, 375, "centro da celula (1,1)");
    /* Celula (rpm 1000..2000) x (carga 30..60), centro: (100+200+150+250)/4. */
    expect_lookup(&t, 1500, 45, 175, "centro da celula (0,0)");
}

/*
 * Ponto fora do centro, para o peso de cada canto importar de verdade:
 * (2500, 70) = (250*1500*30 + 400*500*30 + 350*1500*10 + 500*500*10) / 80000
 *            = 25000000 / 80000 = 312,5, que arredonda para 313.
 */
static void test_off_center_and_rounding(void)
{
    uint16_t cells[9];
    ecu_table_t t;
    make_grid(&t, cells);

    expect_lookup(&t, 2500, 70, 313, "fora do centro, meio arredonda para cima");

    /* Arredondamento para os dois lados: 10/3 = 3,33 -> 3 e 20/3 = 6,67 -> 7. */
    static const uint16_t rpm2[2] = {0, 4};
    static const uint16_t load2[2] = {0, 3};
    uint16_t c2[4] = {0, 10, 0, 10};
    const ecu_table_t t2 = {2, 2, rpm2, load2, c2};
    expect_lookup(&t2, 2, 1, 3, "arredonda para baixo");
    expect_lookup(&t2, 2, 2, 7, "arredonda para cima");
}

/* Sobre uma aresta a conta e uma interpolacao linear simples. */
static void test_along_edges(void)
{
    uint16_t cells[9];
    ecu_table_t t;
    make_grid(&t, cells);

    expect_lookup(&t, 1000, 45, 150, "aresta rpm=1000, entre 100 e 200");
    expect_lookup(&t, 1500, 30, 125, "aresta carga=30, entre 100 e 150");
    expect_lookup(&t, 4000, 80, 450, "aresta rpm=4000, entre 400 e 500");
}

/* Fora da faixa nao extrapola: vale a borda. */
static void test_clamps_outside_range(void)
{
    uint16_t cells[9];
    ecu_table_t t;
    make_grid(&t, cells);

    expect_lookup(&t, 0, 0, 100, "abaixo dos dois eixos");
    expect_lookup(&t, 65535, 65535, 500, "acima dos dois eixos");
    expect_lookup(&t, 500, 80, 250, "rpm abaixo, carga dentro");
    expect_lookup(&t, 9999, 45, 350, "rpm acima, carga dentro");
    expect_lookup(&t, 1500, 5, 125, "carga abaixo, rpm dentro");
    expect_lookup(&t, 1500, 500, 325, "carga acima, rpm dentro");
}

/* A celula devolvida e a que contem o ponto; a borda de cima fica na ultima. */
static void test_locate(void)
{
    uint16_t cells[9];
    ecu_table_t t;
    make_grid(&t, cells);

    expect_cell(&t, 3000, 80, 1, 1, "meio da ultima celula");
    expect_cell(&t, 1500, 45, 0, 0, "meio da primeira celula");
    expect_cell(&t, 1000, 30, 0, 0, "canto inferior");
    expect_cell(&t, 2000, 60, 1, 1, "no interno cai na celula de cima");
    expect_cell(&t, 4000, 100, 1, 1, "canto superior fica na ultima celula");
    expect_cell(&t, 0, 0, 0, 0, "abaixo da faixa");
    expect_cell(&t, 65535, 65535, 1, 1, "acima da faixa");
}

/* Valores de 16 bits no limite nao podem estourar a conta. */
static void test_no_overflow(void)
{
    static const uint16_t axis[2] = {0, 65535};
    uint16_t diag[4] = {0, 65535, 65535, 0};
    const ecu_table_t t = {2, 2, axis, axis, diag};
    /* 2 * 65535 * 32768 * 32767 / 65535^2 = 32767,49999 -> 32767 */
    expect_lookup(&t, 32767, 32767, 32767, "perto do centro com valores de 16 bits");
    expect_lookup(&t, 0, 65535, 65535, "canto com valor maximo");
    expect_lookup(&t, 65535, 65535, 0, "canto com valor zero");

    uint16_t all_max[4] = {65535, 65535, 65535, 65535};
    const ecu_table_t tm = {2, 2, axis, axis, all_max};
    expect_lookup(&tm, 12345, 54321, 65535, "tabela toda no maximo");
}

/* Tabela invalida nao pode ser consultada e nao escreve na saida. */
static void test_invalid_tables(void)
{
    static const uint16_t ok_axis[2] = {0, 10};
    static const uint16_t one_axis[1] = {0};
    static const uint16_t flat_axis[3] = {0, 5, 5};
    static const uint16_t down_axis[3] = {0, 9, 5};
    uint16_t cells[9] = {0};
    uint16_t out = 777;

    const ecu_table_t good = {2, 2, ok_axis, ok_axis, cells};
    CHECK(ecu_table_valid(&good), "tabela valida deve passar");

    const ecu_table_t one = {1, 2, one_axis, ok_axis, cells};
    const ecu_table_t flat = {3, 2, flat_axis, ok_axis, cells};
    const ecu_table_t down = {3, 2, down_axis, ok_axis, cells};
    const ecu_table_t no_cells = {2, 2, ok_axis, ok_axis, NULL};
    const ecu_table_t no_axis = {2, 2, NULL, ok_axis, cells};
    const ecu_table_t zero = {0, 2, ok_axis, ok_axis, cells};

    CHECK(!ecu_table_valid(NULL), "tabela NULL");
    CHECK(!ecu_table_valid(&one), "eixo com 1 ponto");
    CHECK(!ecu_table_valid(&flat), "eixo com valor repetido");
    CHECK(!ecu_table_valid(&down), "eixo nao crescente");
    CHECK(!ecu_table_valid(&no_cells), "celulas NULL");
    CHECK(!ecu_table_valid(&no_axis), "eixo NULL");
    CHECK(!ecu_table_valid(&zero), "eixo com 0 pontos");

    CHECK(!ecu_table_lookup(NULL, 1, 1, &out) && out == 777, "lookup em NULL");
    CHECK(!ecu_table_lookup(&one, 1, 1, &out) && out == 777, "lookup em eixo curto");
    CHECK(!ecu_table_lookup(&flat, 1, 1, &out) && out == 777, "lookup em eixo repetido");
    CHECK(!ecu_table_lookup(&good, 1, 1, NULL), "lookup sem saida");

    ecu_cell_t c = {9, 9};
    CHECK(!ecu_table_locate(&one, 1, 1, &c) && c.rpm_idx == 9 && c.load_idx == 9,
          "locate em tabela invalida nao escreve");
    CHECK(!ecu_table_locate(&good, 1, 1, NULL), "locate sem saida");
}

/* Tabela 2 x 2 e o menor caso valido. */
static void test_minimum_size(void)
{
    static const uint16_t rpm[2] = {1000, 3000};
    static const uint16_t load[2] = {20, 80};
    uint16_t cells[4] = {10, 20, 30, 40};
    const ecu_table_t t = {2, 2, rpm, load, cells};

    expect_lookup(&t, 1000, 20, 10, "2x2 canto (0,0)");
    expect_lookup(&t, 1000, 80, 20, "2x2 canto (0,1)");
    expect_lookup(&t, 3000, 20, 30, "2x2 canto (1,0)");
    expect_lookup(&t, 3000, 80, 40, "2x2 canto (1,1)");
    expect_lookup(&t, 2000, 50, 25, "2x2 centro");
}

/* Tabela retangular (mais colunas que linhas) para pegar troca de n_rpm e n_load. */
static void test_non_square(void)
{
    static const uint16_t rpm[2] = {1000, 2000};
    static const uint16_t load[4] = {10, 20, 30, 40};
    /* linha rpm=1000: 1 2 3 4; linha rpm=2000: 11 12 13 14 */
    uint16_t cells[8] = {1, 2, 3, 4, 11, 12, 13, 14};
    const ecu_table_t t = {2, 4, rpm, load, cells};

    expect_lookup(&t, 1000, 40, 4, "retangular canto (0,3)");
    expect_lookup(&t, 2000, 10, 11, "retangular canto (1,0)");
    expect_lookup(&t, 2000, 40, 14, "retangular canto (1,3)");
    /* rpm no meio (+5), carga entre 20 e 30 no meio (+0,5): 2,5 + 5 = 7,5 -> 8 */
    expect_lookup(&t, 1500, 25, 8, "retangular centro da celula (0,1)");
    expect_cell(&t, 1500, 35, 0, 2, "retangular ultima coluna");
}

/*
 * Mais linhas que colunas (4 x 2). Com n_rpm > n_load o indice de linha passa
 * de 1 e o passo entre linhas (n_load) faz diferenca; sem isso um n_rpm no
 * lugar de n_load passaria despercebido.
 */
static void test_more_rows_than_columns(void)
{
    static const uint16_t rpm[4] = {1000, 2000, 3000, 4000};
    static const uint16_t load[2] = {10, 20};
    /* linhas: (1 2) (3 4) (5 6) (7 8) */
    uint16_t cells[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const ecu_table_t t = {4, 2, rpm, load, cells};

    for (uint8_t i = 0; i < 4; i++) {
        for (uint8_t j = 0; j < 2; j++) {
            expect_lookup(&t, rpm[i], load[j], cells[i * 2 + j], "4x2 no da grade");
        }
    }
    /* centro da celula (2,0): (5+6+7+8)/4 = 6,5 -> 7 */
    expect_lookup(&t, 3500, 15, 7, "4x2 centro da celula de baixo");
    /* centro da celula (0,0): (1+2+3+4)/4 = 2,5 -> 3 */
    expect_lookup(&t, 1500, 15, 3, "4x2 centro da celula de cima");
    expect_cell(&t, 3500, 15, 2, 0, "4x2 celula do meio para baixo");
}

int main(void)
{
    test_corners_exact();
    test_every_node_exact();
    test_center_of_four_cells();
    test_off_center_and_rounding();
    test_along_edges();
    test_clamps_outside_range();
    test_locate();
    test_no_overflow();
    test_invalid_tables();
    test_minimum_size();
    test_non_square();
    test_more_rows_than_columns();

    if (g_failures != 0) {
        printf("%d de %d verificacoes falharam\n", g_failures, g_checks);
        return 1;
    }
    printf("ok: %d verificacoes passaram\n", g_checks);
    return 0;
}
