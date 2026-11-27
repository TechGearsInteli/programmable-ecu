#include "ecu/ecu_engine.h"

#include <string.h>

/* Ponto fixo Q16: fracoes de 0 a 65536 sem usar float (rapido no ESP32 e
 * igual em qualquer maquina, o que deixa os testes exatos). */
#define Q16 65536LL

static int64_t round_div(int64_t num, int64_t den)
{
    /* divisao com arredondamento simetrico (metade para longe do zero) */
    if ((num >= 0) == (den >= 0)) {
        return (num + den / 2) / den;
    }
    return (num - den / 2) / den;
}

/* Acha o segmento [i, i+1] do eixo que contem q e a fracao t (Q16) dentro dele. */
static uint8_t segment(const int16_t *axis, uint8_t n, int32_t scale, int32_t q, int64_t *t)
{
    if (n < 2 || q <= (int32_t)axis[0] * scale) {
        *t = 0;
        return 0;
    }
    if (q >= (int32_t)axis[n - 1] * scale) {
        *t = Q16;
        return (uint8_t)(n - 2);
    }
    uint8_t i = 0;
    while (i < n - 2 && q >= (int32_t)axis[i + 1] * scale) {
        i++;
    }
    const int64_t a = (int64_t)axis[i] * scale;
    const int64_t b = (int64_t)axis[i + 1] * scale;
    *t = (q - a) * Q16 / (b - a);
    return i;
}

int32_t ecu_interp1d(const int16_t *axis, uint8_t n, int32_t scale, const int16_t *values, int32_t q)
{
    if (n == 0) {
        return 0;
    }
    if (n == 1) {
        return values[0];
    }
    int64_t t;
    const uint8_t i = segment(axis, n, scale, q, &t);
    const int64_t v = (int64_t)values[i] * (Q16 - t) + (int64_t)values[i + 1] * t;
    return (int32_t)round_div(v, Q16);
}

int32_t ecu_interp2d(const ecu_map_t *m, uint8_t rows, uint8_t cols, int32_t xq, int32_t x_scale,
                     int32_t yq, int32_t y_scale)
{
    int64_t tx, ty;
    const uint8_t c = segment(m->x, cols, x_scale, xq, &tx);
    const uint8_t r = segment(m->y, rows, y_scale, yq, &ty);
    if (cols < 2) {
        tx = 0;
    }
    if (rows < 2) {
        return (int32_t)round_div((int64_t)m->v[0][c] * (Q16 - tx) + (int64_t)m->v[0][c + (cols > 1)] * tx, Q16);
    }
    /* 1) interpola pela rotacao nas duas linhas vizinhas; 2) entre elas pela carga */
    const int64_t low = (int64_t)m->v[r][c] * (Q16 - tx) + (int64_t)m->v[r][c + 1] * tx;
    const int64_t high = (int64_t)m->v[r + 1][c] * (Q16 - tx) + (int64_t)m->v[r + 1][c + 1] * tx;
    const int64_t v = low * (Q16 - ty) + high * ty;
    return (int32_t)round_div(v, Q16 * Q16);
}

uint8_t ecu_axis_nearest(const int16_t *axis, uint8_t n, int32_t scale, int32_t q)
{
    uint8_t best = 0;
    int64_t best_d = -1;
    for (uint8_t i = 0; i < n; i++) {
        int64_t d = (int64_t)axis[i] * scale - q;
        if (d < 0) {
            d = -d;
        }
        if (best_d < 0 || d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

uint8_t ecu_firing_order(uint8_t cylinders, uint8_t order[8], uint16_t *spacing_deg)
{
    static const uint8_t o1[] = {1};
    static const uint8_t o2[] = {1, 2};
    static const uint8_t o3[] = {1, 2, 3};
    static const uint8_t o4[] = {1, 3, 4, 2};
    static const uint8_t o5[] = {1, 2, 4, 5, 3};
    static const uint8_t o6[] = {1, 5, 3, 6, 2, 4};
    static const uint8_t o8[] = {1, 8, 4, 3, 6, 5, 7, 2};
    const uint8_t *src = NULL;
    switch (cylinders) {
    case 1: src = o1; break;
    case 2: src = o2; break;
    case 3: src = o3; break;
    case 4: src = o4; break;
    case 5: src = o5; break;
    case 6: src = o6; break;
    case 8: src = o8; break;
    default: return 0;
    }
    memcpy(order, src, cylinders);
    if (spacing_deg) {
        *spacing_deg = (uint16_t)(720u / cylinders);
    }
    return cylinders;
}

void ecu_engine_init(ecu_engine_state_t *s)
{
    memset(s, 0, sizeof(*s));
}

static int32_t curve(const ecu_calib_t *c, uint8_t table, int32_t scale, int32_t q)
{
    const ecu_map_t *m = &c->table[table];
    return ecu_interp1d(m->x, ECU_CURVE_POINTS, scale, m->v[0], q);
}

/* Aplica uma correcao em % x10: (1000 + corr) / 1000. */
static int64_t apply_pct(int64_t value, int32_t corr_x10)
{
    return round_div(value * (1000 + corr_x10), 1000);
}

static void update_accel(const ecu_calib_t *c, const ecu_inputs_t *in, ecu_engine_state_t *s, uint32_t now)
{
    if (!s->has_last) {
        s->has_last = true;
        s->last_tps = in->tps_pct;
        s->last_ms = now;
    } else if (now - s->last_ms >= 10u) {
        const uint32_t dt = now - s->last_ms;
        const int32_t dtps = (int32_t)in->tps_pct - (int32_t)s->last_tps;
        s->last_tps = in->tps_pct;
        s->last_ms = now;
        if (dtps > 0) {
            const int32_t tpsdot = dtps * 1000 / (int32_t)dt; /* %/s */
            const int32_t target = curve(c, ECU_TBL_ACCEL, 1, tpsdot);
            if (target > 0 && (target > s->accel_x10 || now >= s->accel_until_ms)) {
                s->accel_x10 = (int16_t)target;
                s->accel_until_ms = now + ECU_ACCEL_HOLD_MS;
            }
        }
    }
    if (now >= s->accel_until_ms) {
        s->accel_x10 = 0;
    }
}

void ecu_engine_compute(const ecu_calib_t *c, const ecu_inputs_t *in, ecu_engine_state_t *s,
                        uint32_t now_ms, ecu_outputs_t *out)
{
    memset(out, 0, sizeof(*out));
    const ecu_map_t *fuel = &c->table[ECU_TBL_FUEL];
    const ecu_map_t *ign = &c->table[ECU_TBL_IGN];

    /* Celula atual (a mais proxima) e valor interpolado */
    out->fuel_col = ecu_axis_nearest(fuel->x, ECU_MAP_COLS, 1, in->rpm);
    out->fuel_row = ecu_axis_nearest(fuel->y, ECU_MAP_ROWS, 10, in->map_kpa_x10);
    out->fuel_cell = fuel->v[out->fuel_row][out->fuel_col];
    const int32_t base_x100 = ecu_interp2d(fuel, ECU_MAP_ROWS, ECU_MAP_COLS, in->rpm, 1, in->map_kpa_x10, 10);
    out->fuel_interp_x100 = (uint16_t)(base_x100 < 0 ? 0 : base_x100);

    out->ign_col = ecu_axis_nearest(ign->x, ECU_MAP_COLS, 1, in->rpm);
    out->ign_row = ecu_axis_nearest(ign->y, ECU_MAP_ROWS, 10, in->map_kpa_x10);
    out->ign_cell = ign->v[out->ign_row][out->ign_col];
    out->advance_x10 = (int16_t)ecu_interp2d(ign, ECU_MAP_ROWS, ECU_MAP_COLS, in->rpm, 1, in->map_kpa_x10, 10);
    out->dwell_us = (uint16_t)(c->param[ECU_PAR_DWELL_X10] * 100);

    /* Correcoes */
    out->cranking = in->rpm > 0 && in->rpm < (uint16_t)c->param[ECU_PAR_CRANK_RPM];
    out->corr_warm_x10 = (int16_t)curve(c, ECU_TBL_WARMUP, 10, in->clt_x10);
    out->corr_iat_x10 = (int16_t)curve(c, ECU_TBL_IAT, 10, in->iat_x10);
    out->corr_crank_x10 = out->cranking ? (int16_t)curve(c, ECU_TBL_CRANK, 10, in->clt_x10) : 0;
    update_accel(c, in, s, now_ms);
    out->corr_accel_x10 = s->accel_x10;
    /* eixo da bateria em V x10, consulta em mV: scale = 100 */
    const int32_t dead = curve(c, ECU_TBL_DEADTIME, 100, in->vbat_mv);
    out->dead_time_us = (uint16_t)(dead < 0 ? 0 : dead);

    /* Troca de bico: bico maior injeta mais por ms, entao o tempo cai na mesma proporcao */
    const int32_t flow = c->param[ECU_PAR_INJ_FLOW] > 0 ? c->param[ECU_PAR_INJ_FLOW] : 1;
    out->inj_scale_x1000 = (uint16_t)round_div((int64_t)c->param[ECU_PAR_INJ_FLOW_REF] * 1000, flow);

    int64_t pw = (int64_t)out->fuel_interp_x100 * 10; /* ms x100 -> us */
    pw = round_div(pw * c->param[ECU_PAR_INJ_FLOW_REF], flow);
    pw = apply_pct(pw, out->corr_warm_x10);
    pw = apply_pct(pw, out->corr_iat_x10);
    pw = apply_pct(pw, out->corr_crank_x10);
    pw = apply_pct(pw, out->corr_accel_x10);
    pw += out->dead_time_us;
    if (pw < 0) pw = 0;
    if (pw > 65535) pw = 65535;
    out->pw_us = (uint16_t)pw;
}
