#include "ecu_fuel.h"

#include <stddef.h>

bool ecu_fuel_pulse_width_us(const ecu_fuel_in_t *in, uint32_t *out_us)
{
    if (in == NULL || out_us == NULL) {
        return false;
    }
    if (in->n_corr > 0 && in->corr_permille == NULL) {
        return false;
    }
    uint16_t dead_us;
    if (!ecu_curve_lookup(in->dead_time, in->vbat_mv, &dead_us)) {
        return false;
    }

    /*
     * Em 64 bits: o valor (ate UINT32_MAX) vezes um fator (ate 65535) cabe.
     * Se uma cadeia de fatores enormes passar de UINT32_MAX (mais de 70
     * minutos de bico), satura ali mesmo para a conta nao estourar.
     */
    uint64_t pw = in->base_us;
    for (uint8_t i = 0; i < in->n_corr; i++) {
        pw = (pw * in->corr_permille[i] + 500u) / 1000u;
        if (pw > UINT32_MAX) {
            pw = UINT32_MAX;
        }
    }

    if (pw == 0) {
        *out_us = 0;
        return true;
    }

    pw += dead_us;
    if (pw > in->max_us) {
        pw = in->max_us;
    }
    *out_us = (uint32_t)pw;
    return true;
}
