#include "ecu_curve.h"

#include <stddef.h>

bool ecu_curve_valid(const ecu_curve_t *c)
{
    if (c == NULL || c->x == NULL || c->y == NULL || c->n < 2) {
        return false;
    }
    for (uint8_t i = 1; i < c->n; i++) {
        if (c->x[i] <= c->x[i - 1]) {
            return false;
        }
    }
    return true;
}

bool ecu_curve_lookup(const ecu_curve_t *c, uint16_t x, uint16_t *out)
{
    if (out == NULL || !ecu_curve_valid(c)) {
        return false;
    }

    /* Fora da faixa nao extrapola: vale o ponto da borda. */
    if (x <= c->x[0]) {
        *out = c->y[0];
        return true;
    }
    if (x >= c->x[c->n - 1]) {
        *out = c->y[c->n - 1];
        return true;
    }

    /* Maior i com x[i] <= x. Aqui x[0] < x < x[n-1], entao i fica em [0, n-2]. */
    uint8_t i = 0;
    while ((uint8_t)(i + 2) < c->n && c->x[i + 1] <= x) {
        i++;
    }

    /*
     * y0*(dx - w) + y1*w, tudo sem sinal, para a curva poder subir ou descer
     * sem conta com negativo. Um unico arredondamento, no final.
     *
     * Cabe em 32 bits: como (dx - w) + w = dx <= 65535 e cada y <= 65535, o
     * numerador vale no maximo 65535 * 65535 = 4294836225, e somar dx / 2
     * (<= 32767) ainda fica abaixo de 2^32. Evita a divisao de 64 bits, que
     * no ESP32 e feita por software.
     */
    const uint32_t dx = (uint32_t)(c->x[i + 1] - c->x[i]);
    const uint32_t w = (uint32_t)(x - c->x[i]);
    const uint32_t num = (uint32_t)c->y[i] * (dx - w) + (uint32_t)c->y[i + 1] * w;

    *out = (uint16_t)((num + dx / 2) / dx);
    return true;
}
