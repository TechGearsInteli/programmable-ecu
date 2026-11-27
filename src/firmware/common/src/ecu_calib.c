#include "ecu/ecu_calib.h"

#include <string.h>

#include "ecu/ecu_crc.h"

/* ------------------------------------------------------------------------ */
/* Descricao das tabelas e da ficha (limites que o ESP do motor aceita)     */
/* ------------------------------------------------------------------------ */

static const ecu_table_desc_t TABLES[ECU_TBL_COUNT] = {
    [ECU_TBL_FUEL] = {"Combustível", ECU_MAP_ROWS, ECU_MAP_COLS, ECU_UNIT_MS_X100, ECU_UNIT_RPM, ECU_UNIT_KPA,
                      50, 2000, 5},     /* 0,50 a 20,00 ms */
    [ECU_TBL_IGN] = {"Ignição", ECU_MAP_ROWS, ECU_MAP_COLS, ECU_UNIT_DEG_X10, ECU_UNIT_RPM, ECU_UNIT_KPA,
                     -100, 450, 5},     /* -10,0 a 45,0 graus */
    [ECU_TBL_WARMUP] = {"Aquecimento", 1, ECU_CURVE_POINTS, ECU_UNIT_PCT_X10, ECU_UNIT_DEGC, ECU_UNIT_NONE,
                        0, 2000, 10},   /* 0 a +200 % */
    [ECU_TBL_IAT] = {"Temperatura do ar", 1, ECU_CURVE_POINTS, ECU_UNIT_PCT_X10, ECU_UNIT_DEGC, ECU_UNIT_NONE,
                     -300, 300, 10},    /* -30 a +30 % */
    [ECU_TBL_CRANK] = {"Partida", 1, ECU_CURVE_POINTS, ECU_UNIT_PCT_X10, ECU_UNIT_DEGC, ECU_UNIT_NONE,
                       0, 4000, 50},    /* 0 a +400 % */
    [ECU_TBL_DEADTIME] = {"Bateria (tempo morto)", 1, ECU_CURVE_POINTS, ECU_UNIT_US, ECU_UNIT_VOLT_X10,
                          ECU_UNIT_NONE, 0, 3000, 10}, /* 0 a 3000 us */
    [ECU_TBL_ACCEL] = {"Aceleração rápida", 1, ECU_CURVE_POINTS, ECU_UNIT_PCT_X10, ECU_UNIT_PCT_PER_S,
                       ECU_UNIT_NONE, 0, 2000, 10}, /* 0 a +200 % */
};

static const ecu_param_desc_t PARAMS[ECU_PAR_COUNT] = {
    [ECU_PAR_CYLINDERS] = {"Cilindros", ECU_UNIT_COUNT, 1, 8, 1},
    [ECU_PAR_TRIGGER_TEETH] = {"Dentes da roda fônica", ECU_UNIT_COUNT, 4, 60, 1},
    [ECU_PAR_TRIGGER_MISSING] = {"Dentes faltantes", ECU_UNIT_COUNT, 0, 2, 1},
    [ECU_PAR_INJ_FLOW] = {"Vazão do bico", ECU_UNIT_CC_MIN, 50, 2000, 10},
    [ECU_PAR_INJ_FLOW_REF] = {"Vazão de referência do mapa", ECU_UNIT_CC_MIN, 50, 2000, 10},
    [ECU_PAR_DWELL_X10] = {"Dwell da bobina", ECU_UNIT_MS_X10, 10, 60, 1},
    [ECU_PAR_CRANK_RPM] = {"Rotação de partida", ECU_UNIT_RPM, 100, 800, 10},
    [ECU_PAR_ALERT_RPM] = {"Alerta de rotação", ECU_UNIT_RPM, 1000, 9000, 100},
    [ECU_PAR_ALERT_CLT_X10] = {"Alerta de temperatura", ECU_UNIT_DEGC_X10, 600, 1300, 10},
};

const ecu_table_desc_t *ecu_table_desc(uint8_t table)
{
    return table < ECU_TBL_COUNT ? &TABLES[table] : NULL;
}

const ecu_param_desc_t *ecu_param_desc(uint8_t param)
{
    return param < ECU_PAR_COUNT ? &PARAMS[param] : NULL;
}

/* ------------------------------------------------------------------------ */
/* Calibracao de fabrica                                                     */
/* ------------------------------------------------------------------------ */

static const int16_t RPM_AXIS[ECU_MAP_COLS] = {500, 800, 1100, 1400, 1700, 2000, 2500, 3000,
                                               3500, 4000, 4500, 5000, 5500, 6000, 6500, 7000};
static const int16_t MAP_AXIS[ECU_MAP_ROWS] = {20, 25, 30, 35, 40, 45, 50, 55,
                                               60, 65, 70, 75, 80, 85, 90, 100};

/* Interpolacao linear simples usada so para montar o padrao. */
static int32_t lerp_points(const int16_t *xs, const int16_t *ys, int n, int32_t x)
{
    if (x <= xs[0]) {
        return ys[0];
    }
    for (int i = 0; i < n - 1; i++) {
        if (x <= xs[i + 1]) {
            return ys[i] + (int32_t)(ys[i + 1] - ys[i]) * (x - xs[i]) / (xs[i + 1] - xs[i]);
        }
    }
    return ys[n - 1];
}

static void set_curve(ecu_map_t *m, const int16_t *x, const int16_t *v)
{
    memcpy(m->x, x, ECU_CURVE_POINTS * sizeof(int16_t));
    memcpy(m->v[0], v, ECU_CURVE_POINTS * sizeof(int16_t));
}

void ecu_calib_defaults(ecu_calib_t *c)
{
    memset(c, 0, sizeof(*c));

    /* Combustivel: PW = tempo de referencia (11,50 ms para ~375 cc por cilindro
     * com bico de 240 cc/min) x eficiencia volumetrica x MAP/100, com um pouco
     * de enriquecimento em plena carga. E um ponto de partida para a bancada,
     * nao um mapa acertado. */
    static const int16_t ve_rpm[] = {500, 1000, 2000, 3000, 4000, 5000, 6000, 7000};
    static const int16_t ve_pct[] = {60, 68, 80, 88, 94, 96, 92, 85};
    ecu_map_t *fuel = &c->table[ECU_TBL_FUEL];
    ecu_map_t *ign = &c->table[ECU_TBL_IGN];
    memcpy(fuel->x, RPM_AXIS, sizeof(RPM_AXIS));
    memcpy(fuel->y, MAP_AXIS, sizeof(MAP_AXIS));
    memcpy(ign->x, RPM_AXIS, sizeof(RPM_AXIS));
    memcpy(ign->y, MAP_AXIS, sizeof(MAP_AXIS));

    /* Ignicao: avanco cresce com a rotacao e cai com a carga (conservador). */
    static const int16_t adv_rpm[] = {500, 1000, 2000, 3000, 4000, 5000, 7000};
    static const int16_t adv_deg[] = {100, 140, 240, 300, 330, 340, 340};

    for (unsigned r = 0; r < ECU_MAP_ROWS; r++) {
        const int32_t map = MAP_AXIS[r];
        const int32_t enrich = map <= 70 ? 1000 : 1000 + (map - 70) * 120 / 30; /* ate +12 % */
        for (unsigned col = 0; col < ECU_MAP_COLS; col++) {
            const int32_t rpm = RPM_AXIS[col];
            const int32_t ve = lerp_points(ve_rpm, ve_pct, 8, rpm);
            int32_t pw = (int32_t)((int64_t)1150 * ve * map * enrich / (100 * 100 * 1000));
            if (pw < TABLES[ECU_TBL_FUEL].min) pw = TABLES[ECU_TBL_FUEL].min;
            if (pw > TABLES[ECU_TBL_FUEL].max) pw = TABLES[ECU_TBL_FUEL].max;
            fuel->v[r][col] = (int16_t)pw;

            int32_t adv = lerp_points(adv_rpm, adv_deg, 7, rpm);
            if (map > 30) {
                adv -= (map - 30) * 12 / 10; /* -1,2 grau a cada 10 kPa */
            }
            if (adv < 50) adv = 50;
            if (adv > 400) adv = 400;
            ign->v[r][col] = (int16_t)adv;
        }
    }

    static const int16_t clt_axis[] = {-10, 0, 10, 20, 40, 60, 80, 90};
    static const int16_t warm[] = {450, 380, 300, 220, 120, 50, 10, 0};
    static const int16_t crank[] = {2500, 2200, 1900, 1600, 1200, 900, 700, 600};
    static const int16_t iat_axis[] = {-10, 0, 10, 20, 30, 40, 50, 60};
    static const int16_t iat[] = {60, 40, 20, 0, -20, -40, -60, -80};
    static const int16_t vbat_axis[] = {80, 90, 100, 110, 120, 130, 140, 150};
    static const int16_t dead[] = {1600, 1350, 1150, 1000, 880, 790, 720, 660};
    static const int16_t tpsdot_axis[] = {0, 50, 100, 200, 300, 500, 800, 1200};
    static const int16_t accel[] = {0, 0, 50, 120, 200, 300, 400, 450};
    set_curve(&c->table[ECU_TBL_WARMUP], clt_axis, warm);
    set_curve(&c->table[ECU_TBL_IAT], iat_axis, iat);
    set_curve(&c->table[ECU_TBL_CRANK], clt_axis, crank);
    set_curve(&c->table[ECU_TBL_DEADTIME], vbat_axis, dead);
    set_curve(&c->table[ECU_TBL_ACCEL], tpsdot_axis, accel);

    /* Ficha: 4 cilindros, roda 36-1 (a do Fit ainda sera medida), bico de 240 cc/min */
    c->param[ECU_PAR_CYLINDERS] = 4;
    c->param[ECU_PAR_TRIGGER_TEETH] = 36;
    c->param[ECU_PAR_TRIGGER_MISSING] = 1;
    c->param[ECU_PAR_INJ_FLOW] = 240;
    c->param[ECU_PAR_INJ_FLOW_REF] = 240;
    c->param[ECU_PAR_DWELL_X10] = 30;
    c->param[ECU_PAR_CRANK_RPM] = 400;
    c->param[ECU_PAR_ALERT_RPM] = 6500;
    c->param[ECU_PAR_ALERT_CLT_X10] = 1050;
}

/* ------------------------------------------------------------------------ */
/* Conferencias                                                              */
/* ------------------------------------------------------------------------ */

uint8_t ecu_calib_check_cell(uint8_t table, uint8_t row, uint8_t col, int16_t value)
{
    const ecu_table_desc_t *d = ecu_table_desc(table);
    if (!d) {
        return ECU_ERR_TABLE;
    }
    if (row >= d->rows || col >= d->cols) {
        return ECU_ERR_INDEX;
    }
    if (value < d->min || value > d->max) {
        return ECU_ERR_RANGE;
    }
    return ECU_OK;
}

uint8_t ecu_calib_set_cell(ecu_calib_t *c, uint8_t table, uint8_t row, uint8_t col, int16_t value)
{
    const uint8_t st = ecu_calib_check_cell(table, row, col, value);
    if (st == ECU_OK) {
        /* Escrita de 16 bits alinhada: atomica no ESP32, o calculo nunca le meio valor. */
        c->table[table].v[row][col] = value;
    }
    return st;
}

static bool cylinders_supported(int16_t n)
{
    return n >= 1 && n <= 8 && n != 7;
}

uint8_t ecu_calib_check_param(const ecu_calib_t *c, uint8_t param, int16_t value)
{
    const ecu_param_desc_t *d = ecu_param_desc(param);
    if (!d) {
        return ECU_ERR_PARAM;
    }
    if (value < d->min || value > d->max) {
        return ECU_ERR_RANGE;
    }
    switch (param) {
    case ECU_PAR_CYLINDERS:
        return cylinders_supported(value) ? ECU_OK : ECU_ERR_PARAM;
    case ECU_PAR_TRIGGER_TEETH:
        return value > 2 * c->param[ECU_PAR_TRIGGER_MISSING] ? ECU_OK : ECU_ERR_PARAM;
    case ECU_PAR_TRIGGER_MISSING:
        return 2 * value < c->param[ECU_PAR_TRIGGER_TEETH] ? ECU_OK : ECU_ERR_PARAM;
    default:
        return ECU_OK;
    }
}

uint8_t ecu_calib_set_param(ecu_calib_t *c, uint8_t param, int16_t value)
{
    const uint8_t st = ecu_calib_check_param(c, param, value);
    if (st == ECU_OK) {
        c->param[param] = value;
    }
    return st;
}

bool ecu_calib_validate(const ecu_calib_t *c)
{
    for (uint8_t t = 0; t < ECU_TBL_COUNT; t++) {
        const ecu_table_desc_t *d = &TABLES[t];
        const ecu_map_t *m = &c->table[t];
        for (uint8_t i = 1; i < d->cols; i++) {
            if (m->x[i] <= m->x[i - 1]) {
                return false;
            }
        }
        if (d->rows > 1) {
            for (uint8_t i = 1; i < d->rows; i++) {
                if (m->y[i] <= m->y[i - 1]) {
                    return false;
                }
            }
        }
        for (uint8_t r = 0; r < d->rows; r++) {
            for (uint8_t col = 0; col < d->cols; col++) {
                if (m->v[r][col] < d->min || m->v[r][col] > d->max) {
                    return false;
                }
            }
        }
    }
    for (uint8_t p = 0; p < ECU_PAR_COUNT; p++) {
        if (ecu_calib_check_param(c, p, c->param[p]) != ECU_OK) {
            return false;
        }
    }
    return true;
}

void ecu_calib_table_info(const ecu_calib_t *c, uint8_t table, ecu_table_info_t *info)
{
    memset(info, 0, sizeof(*info));
    const ecu_table_desc_t *d = ecu_table_desc(table);
    if (!d) {
        return;
    }
    info->table = table;
    info->rows = d->rows;
    info->cols = d->cols;
    info->value_unit = d->value_unit;
    info->x_unit = d->x_unit;
    info->y_unit = d->y_unit;
    info->min = d->min;
    info->max = d->max;
    info->step = d->step;
    memcpy(info->x, c->table[table].x, sizeof(info->x));
    if (d->rows > 1) {
        memcpy(info->y, c->table[table].y, sizeof(info->y));
    }
}

/* ------------------------------------------------------------------------ */
/* Blob                                                                      */
/* ------------------------------------------------------------------------ */

/* Percorre todos os valores na ordem do blob. visit() recebe o ponteiro. */
typedef void (*visit_fn)(void *ctx, int16_t *v);

static void walk(ecu_calib_t *c, visit_fn visit, void *ctx)
{
    for (uint8_t t = 0; t < ECU_TBL_COUNT; t++) {
        const ecu_table_desc_t *d = &TABLES[t];
        ecu_map_t *m = &c->table[t];
        for (uint8_t i = 0; i < d->cols; i++) visit(ctx, &m->x[i]);
        if (d->rows > 1) {
            for (uint8_t i = 0; i < d->rows; i++) visit(ctx, &m->y[i]);
        }
        for (uint8_t r = 0; r < d->rows; r++) {
            for (uint8_t col = 0; col < d->cols; col++) visit(ctx, &m->v[r][col]);
        }
    }
    for (uint8_t p = 0; p < ECU_PAR_COUNT; p++) visit(ctx, &c->param[p]);
}

static void count_visit(void *ctx, int16_t *v)
{
    (void)v;
    (*(size_t *)ctx)++;
}

size_t ecu_calib_blob_size(void)
{
    static size_t values = 0;
    if (values == 0) {
        ecu_calib_t tmp;
        walk(&tmp, count_visit, &values);
    }
    return ECU_CALIB_HEADER + 2u * values;
}

typedef struct {
    uint8_t *p;
    size_t n;
} out_t;

static void write_visit(void *ctx, int16_t *v)
{
    out_t *o = (out_t *)ctx;
    o->p[o->n++] = (uint8_t)((uint16_t)*v & 0xFFu);
    o->p[o->n++] = (uint8_t)((uint16_t)*v >> 8);
}

typedef struct {
    const uint8_t *p;
    size_t n;
} in_t;

static void read_visit(void *ctx, int16_t *v)
{
    in_t *i = (in_t *)ctx;
    *v = (int16_t)(uint16_t)(i->p[i->n] | (i->p[i->n + 1] << 8));
    i->n += 2;
}

static void crc_visit(void *ctx, int16_t *v)
{
    uint8_t b[2] = {(uint8_t)((uint16_t)*v & 0xFFu), (uint8_t)((uint16_t)*v >> 8)};
    *(uint32_t *)ctx = ecu_crc32_update(*(uint32_t *)ctx, b, 2);
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    put_u16(p, (uint16_t)(v & 0xFFFFu));
    put_u16(p + 2, (uint16_t)(v >> 16));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

size_t ecu_calib_serialize(const ecu_calib_t *c, uint8_t *out, size_t cap)
{
    const size_t total = ecu_calib_blob_size();
    if (cap < total) {
        return 0;
    }
    out_t o = {out + ECU_CALIB_HEADER, 0};
    walk((ecu_calib_t *)c, write_visit, &o); /* write_visit so le */
    put_u32(out, ECU_CALIB_MAGIC);
    put_u16(out + 4, ECU_CALIB_VERSION);
    put_u16(out + 6, (uint16_t)o.n);
    put_u32(out + 8, ecu_crc32(out + ECU_CALIB_HEADER, o.n));
    return total;
}

uint8_t ecu_calib_deserialize(ecu_calib_t *out, const uint8_t *in, size_t len)
{
    const size_t total = ecu_calib_blob_size();
    if (len != total || get_u32(in) != ECU_CALIB_MAGIC) {
        return ECU_ERR_BACKUP;
    }
    const uint16_t version = (uint16_t)(in[4] | (in[5] << 8));
    const uint16_t payload = (uint16_t)(in[6] | (in[7] << 8));
    if (version != ECU_CALIB_VERSION || payload != total - ECU_CALIB_HEADER) {
        return ECU_ERR_BACKUP;
    }
    if (ecu_crc32(in + ECU_CALIB_HEADER, payload) != get_u32(in + 8)) {
        return ECU_ERR_BACKUP;
    }
    ecu_calib_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    in_t i = {in + ECU_CALIB_HEADER, 0};
    walk(&tmp, read_visit, &i);
    if (!ecu_calib_validate(&tmp)) {
        return ECU_ERR_BACKUP;
    }
    *out = tmp;
    return ECU_OK;
}

uint32_t ecu_calib_crc(const ecu_calib_t *c)
{
    uint32_t crc = 0;
    walk((ecu_calib_t *)c, crc_visit, &crc);
    return crc;
}

bool ecu_calib_load(ecu_calib_t *c, const ecu_storage_t *storage)
{
    uint8_t blob[ECU_CALIB_BLOB_MAX];
    if (storage && storage->read) {
        const size_t n = storage->read(storage->ctx, blob, sizeof(blob));
        if (n > 0 && ecu_calib_deserialize(c, blob, n) == ECU_OK) {
            return true;
        }
    }
    ecu_calib_defaults(c);
    return false;
}

uint8_t ecu_calib_save(const ecu_calib_t *c, const ecu_storage_t *storage, uint32_t *crc_out)
{
    uint8_t blob[ECU_CALIB_BLOB_MAX];
    const size_t n = ecu_calib_serialize(c, blob, sizeof(blob));
    if (n == 0 || !storage || !storage->write || !storage->write(storage->ctx, blob, n)) {
        return ECU_ERR_FLASH;
    }
    if (crc_out) {
        *crc_out = ecu_calib_crc(c);
    }
    return ECU_OK;
}
