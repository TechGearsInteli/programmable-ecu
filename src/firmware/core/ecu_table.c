#include "ecu_table.h"

#include <stddef.h>

static bool axis_valid(const uint16_t *axis, uint8_t n)
{
    if (axis == NULL || n < 2) {
        return false;
    }
    for (uint8_t i = 1; i < n; i++) {
        if (axis[i] <= axis[i - 1]) {
            return false;
        }
    }
    return true;
}

/* Maior i em [0, n - 2] com axis[i] <= x. Abaixo do eixo vale 0. */
static uint8_t axis_low_index(const uint16_t *axis, uint8_t n, uint16_t x)
{
    uint8_t i = 0;
    while ((uint8_t)(i + 2) < n && axis[i + 1] <= x) {
        i++;
    }
    return i;
}

static uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi)
{
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}

bool ecu_table_valid(const ecu_table_t *t)
{
    return t != NULL && t->cells != NULL && axis_valid(t->rpm_axis, t->n_rpm) &&
           axis_valid(t->load_axis, t->n_load);
}

bool ecu_table_locate(const ecu_table_t *t, uint16_t rpm, uint16_t load,
                      ecu_cell_t *out)
{
    if (out == NULL || !ecu_table_valid(t)) {
        return false;
    }
    out->rpm_idx = axis_low_index(t->rpm_axis, t->n_rpm, rpm);
    out->load_idx = axis_low_index(t->load_axis, t->n_load, load);
    return true;
}

bool ecu_table_lookup(const ecu_table_t *t, uint16_t rpm, uint16_t load,
                      uint16_t *out)
{
    ecu_cell_t c;
    if (out == NULL || !ecu_table_locate(t, rpm, load, &c)) {
        return false;
    }

    const uint16_t x0 = t->rpm_axis[c.rpm_idx];
    const uint16_t x1 = t->rpm_axis[c.rpm_idx + 1];
    const uint16_t y0 = t->load_axis[c.load_idx];
    const uint16_t y1 = t->load_axis[c.load_idx + 1];

    /* Sem extrapolar: fora da faixa o ponto cai na borda da grade. */
    const uint16_t x = clamp_u16(rpm, t->rpm_axis[0], t->rpm_axis[t->n_rpm - 1]);
    const uint16_t y = clamp_u16(load, t->load_axis[0], t->load_axis[t->n_load - 1]);

    const uint16_t *row0 = &t->cells[(size_t)c.rpm_idx * t->n_load];
    const uint16_t *row1 = row0 + t->n_load;
    const uint64_t v00 = row0[c.load_idx];
    const uint64_t v01 = row0[c.load_idx + 1];
    const uint64_t v10 = row1[c.load_idx];
    const uint64_t v11 = row1[c.load_idx + 1];

    /*
     * Peso de cada canto = area do retangulo oposto. A soma dos pesos e o
     * denominador, entao so ha um arredondamento no final (duas interpolacoes
     * lineares em sequencia arredondariam duas vezes). Em uint64_t o maior
     * numerador (65535 * 65535 * 65535) nao estoura.
     */
    const uint64_t dx = (uint64_t)(x1 - x0);
    const uint64_t dy = (uint64_t)(y1 - y0);
    const uint64_t wx1 = (uint64_t)(x - x0);
    const uint64_t wx0 = dx - wx1;
    const uint64_t wy1 = (uint64_t)(y - y0);
    const uint64_t wy0 = dy - wy1;

    const uint64_t num = v00 * wx0 * wy0 + v10 * wx1 * wy0 + v01 * wx0 * wy1 +
                         v11 * wx1 * wy1;
    const uint64_t den = dx * dy;

    *out = (uint16_t)((num + den / 2) / den);
    return true;
}
