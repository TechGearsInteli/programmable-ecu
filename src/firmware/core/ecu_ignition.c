#include "ecu_ignition.h"

#include <stddef.h>

bool ecu_angle_to_us(uint32_t angle_x10, uint16_t rpm, uint32_t *out_us)
{
    if (out_us == NULL || rpm == 0) {
        return false;
    }

    /*
     * Uma volta (360 graus = 3600 decimos) leva 60e6 / rpm us. Logo
     * us = angle_x10 * 60e6 / (3600 * rpm) = angle_x10 * 50000 / (3 * rpm).
     * Em 64 bits: angle_x10 * 50000 passa de 2^32 para angulos grandes.
     */
    const uint64_t num = (uint64_t)angle_x10 * 50000u;
    const uint64_t den = (uint64_t)rpm * 3u;
    const uint64_t us = (num + den / 2) / den;

    *out_us = us > UINT32_MAX ? UINT32_MAX : (uint32_t)us;
    return true;
}

ecu_spark_status_t ecu_spark_schedule(const ecu_spark_in_t *in,
                                      ecu_spark_out_t *out)
{
    if (in == NULL || out == NULL || in->rpm == 0) {
        return ECU_SPARK_BAD_INPUT;
    }

    const uint16_t advance =
        in->advance_x10 < in->max_advance_x10 ? in->advance_x10 : in->max_advance_x10;

    if (advance > in->ref_btdc_x10) {
        return ECU_SPARK_TOO_EARLY;
    }

    /* Angulo entre a referencia e a faisca: so uma conversao, um arredondamento. */
    uint32_t spark_us;
    if (!ecu_angle_to_us((uint32_t)(in->ref_btdc_x10 - advance), in->rpm, &spark_us)) {
        return ECU_SPARK_BAD_INPUT;
    }

    out->advance_x10 = advance;
    out->spark_us = spark_us;

    if (spark_us < in->dwell_us) {
        out->dwell_start_us = 0;
        return ECU_SPARK_SHORT_DWELL;
    }
    out->dwell_start_us = spark_us - in->dwell_us;
    return ECU_SPARK_OK;
}
