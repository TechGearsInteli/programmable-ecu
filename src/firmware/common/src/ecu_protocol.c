#include "ecu/ecu_protocol.h"

#include <string.h>

#include "ecu/ecu_crc.h"

const char *ecu_status_text(uint8_t status)
{
    switch (status) {
    case ECU_OK: return "OK";
    case ECU_ERR_RANGE: return "fora do limite";
    case ECU_ERR_INDEX: return "célula inválida";
    case ECU_ERR_TABLE: return "tabela inválida";
    case ECU_ERR_ENGINE_RUNNING: return "só com o motor parado";
    case ECU_ERR_FLASH: return "falha ao gravar na flash";
    case ECU_ERR_BACKUP: return "backup inválido";
    case ECU_ERR_PARAM: return "parâmetro inválido";
    default: return "erro desconhecido";
    }
}

/* ------------------------------------------------------------------------ */
/* Frame                                                                     */
/* ------------------------------------------------------------------------ */

size_t ecu_frame_encode(uint8_t *out, size_t out_size, uint8_t seq, uint8_t type,
                        const uint8_t *payload, uint8_t len)
{
    const size_t total = (size_t)len + ECU_FRAME_OVERHEAD;
    if (len > ECU_MAX_PAYLOAD || out_size < total) {
        return 0;
    }
    out[0] = ECU_SOF;
    out[1] = len;
    out[2] = seq;
    out[3] = type;
    if (len > 0) {
        memcpy(&out[4], payload, len);
    }
    const uint16_t crc = ecu_crc16(&out[1], (size_t)len + 3u);
    out[4 + len] = (uint8_t)(crc & 0xFFu);
    out[5 + len] = (uint8_t)(crc >> 8);
    return total;
}

void ecu_parser_init(ecu_parser_t *p)
{
    memset(p, 0, sizeof(*p));
}

/* Descarta os n primeiros bytes guardados. */
static void parser_drop(ecu_parser_t *p, uint16_t n)
{
    if (n >= p->pos) {
        p->pos = 0;
        return;
    }
    memmove(p->buf, &p->buf[n], (size_t)(p->pos - n));
    p->pos = (uint16_t)(p->pos - n);
}

/* Tenta tirar frames do inicio do buffer. Sem recursao: cabe na pilha de
 * qualquer task do ESP32. */
static void parser_scan(ecu_parser_t *p, ecu_frame_cb cb, void *ctx)
{
    for (;;) {
        /* 1. o buffer tem que comecar em um SOF */
        uint16_t sof = 0;
        while (sof < p->pos && p->buf[sof] != ECU_SOF) {
            sof++;
        }
        parser_drop(p, sof);
        if (p->pos < 2) {
            return;
        }
        /* 2. tamanho plausivel? */
        const uint8_t len = p->buf[1];
        if (len > ECU_MAX_PAYLOAD) {
            p->len_errors++;
            parser_drop(p, 1); /* era um 0xA5 de lixo: procura o proximo */
            continue;
        }
        const uint16_t total = (uint16_t)(len + ECU_FRAME_OVERHEAD);
        if (p->pos < total) {
            return; /* espera o resto */
        }
        /* 3. CRC */
        const uint16_t crc = ecu_crc16(&p->buf[1], (size_t)len + 3u);
        const uint16_t got = (uint16_t)(p->buf[4 + len] | (p->buf[5 + len] << 8));
        if (crc != got) {
            p->crc_errors++;
            parser_drop(p, 1);
            continue;
        }
        p->frames_ok++;
        p->frame.len = len;
        p->frame.seq = p->buf[2];
        p->frame.type = p->buf[3];
        memcpy(p->frame.payload, &p->buf[4], len);
        parser_drop(p, total);
        if (cb) {
            cb(ctx, &p->frame);
        }
    }
}

void ecu_parser_idle(ecu_parser_t *p, ecu_frame_cb cb, void *ctx)
{
    /* A linha ficou parada: quem manda envia o frame inteiro de uma vez, entao o
     * que sobrou incompleto e lixo. Descarta byte a byte, aproveitando qualquer
     * frame completo que estiver escondido no meio. */
    while (p->pos > 0) {
        parser_drop(p, 1);
        parser_scan(p, cb, ctx);
    }
}

void ecu_parser_feed(ecu_parser_t *p, const uint8_t *data, size_t len, ecu_frame_cb cb, void *ctx)
{
    for (size_t i = 0; i < len; i++) {
        if (p->pos == 0 && data[i] != ECU_SOF) {
            continue; /* atalho: lixo antes do primeiro SOF */
        }
        if (p->pos >= sizeof(p->buf)) {
            parser_drop(p, 1); /* nunca deveria acontecer: protege o buffer */
        }
        p->buf[p->pos++] = data[i];
        parser_scan(p, cb, ctx);
    }
}

/* ------------------------------------------------------------------------ */
/* Leitura e escrita little-endian                                           */
/* ------------------------------------------------------------------------ */

typedef struct {
    uint8_t *p;
    uint8_t n;
} wr_t;

static void w_u8(wr_t *w, uint8_t v) { w->p[w->n++] = v; }
static void w_u16(wr_t *w, uint16_t v)
{
    w->p[w->n++] = (uint8_t)(v & 0xFFu);
    w->p[w->n++] = (uint8_t)(v >> 8);
}
static void w_i16(wr_t *w, int16_t v) { w_u16(w, (uint16_t)v); }
static void w_u32(wr_t *w, uint32_t v)
{
    w_u16(w, (uint16_t)(v & 0xFFFFu));
    w_u16(w, (uint16_t)(v >> 16));
}

typedef struct {
    const uint8_t *p;
    uint8_t n;
} rd_t;

static uint8_t r_u8(rd_t *r) { return r->p[r->n++]; }
static uint16_t r_u16(rd_t *r)
{
    uint16_t v = (uint16_t)(r->p[r->n] | (r->p[r->n + 1] << 8));
    r->n = (uint8_t)(r->n + 2);
    return v;
}
static int16_t r_i16(rd_t *r) { return (int16_t)r_u16(r); }
static uint32_t r_u32(rd_t *r)
{
    uint32_t lo = r_u16(r);
    uint32_t hi = r_u16(r);
    return lo | (hi << 16);
}

/* ------------------------------------------------------------------------ */
/* Mensagens                                                                 */
/* ------------------------------------------------------------------------ */

uint8_t ecu_pack_telemetry(const ecu_telemetry_t *m, uint8_t *out)
{
    wr_t w = {out, 0};
    w_u16(&w, m->rpm);
    w_u8(&w, m->flags);
    w_u8(&w, m->tps_pct);
    w_u16(&w, m->pw_us);
    w_i16(&w, m->advance_x10);
    w_u16(&w, m->dwell_us);
    w_u16(&w, m->map_kpa_x10);
    w_i16(&w, m->clt_x10);
    w_i16(&w, m->iat_x10);
    w_u16(&w, m->vbat_mv);
    w_u8(&w, m->fuel_row);
    w_u8(&w, m->fuel_col);
    w_i16(&w, m->fuel_cell);
    w_u16(&w, m->fuel_interp_x100);
    w_u8(&w, m->ign_row);
    w_u8(&w, m->ign_col);
    w_i16(&w, m->ign_cell);
    w_i16(&w, m->corr_warm_x10);
    w_i16(&w, m->corr_iat_x10);
    w_i16(&w, m->corr_crank_x10);
    w_i16(&w, m->corr_accel_x10);
    w_u16(&w, m->dead_time_us);
    w_u16(&w, m->inj_scale_x1000);
    w_u32(&w, m->uptime_ms);
    return w.n;
}

bool ecu_unpack_telemetry(const uint8_t *in, uint8_t len, ecu_telemetry_t *m)
{
    if (len != ECU_TELEMETRY_LEN) {
        return false;
    }
    rd_t r = {in, 0};
    m->rpm = r_u16(&r);
    m->flags = r_u8(&r);
    m->tps_pct = r_u8(&r);
    m->pw_us = r_u16(&r);
    m->advance_x10 = r_i16(&r);
    m->dwell_us = r_u16(&r);
    m->map_kpa_x10 = r_u16(&r);
    m->clt_x10 = r_i16(&r);
    m->iat_x10 = r_i16(&r);
    m->vbat_mv = r_u16(&r);
    m->fuel_row = r_u8(&r);
    m->fuel_col = r_u8(&r);
    m->fuel_cell = r_i16(&r);
    m->fuel_interp_x100 = r_u16(&r);
    m->ign_row = r_u8(&r);
    m->ign_col = r_u8(&r);
    m->ign_cell = r_i16(&r);
    m->corr_warm_x10 = r_i16(&r);
    m->corr_iat_x10 = r_i16(&r);
    m->corr_crank_x10 = r_i16(&r);
    m->corr_accel_x10 = r_i16(&r);
    m->dead_time_us = r_u16(&r);
    m->inj_scale_x1000 = r_u16(&r);
    m->uptime_ms = r_u32(&r);
    return true;
}

uint8_t ecu_pack_pong(const ecu_pong_t *m, uint8_t *out)
{
    wr_t w = {out, 0};
    w_u8(&w, m->proto_version);
    w_u8(&w, m->node);
    w_u8(&w, m->calib_version);
    w_u8(&w, m->flags);
    return w.n;
}

bool ecu_unpack_pong(const uint8_t *in, uint8_t len, ecu_pong_t *m)
{
    if (len != 4) {
        return false;
    }
    m->proto_version = in[0];
    m->node = in[1];
    m->calib_version = in[2];
    m->flags = in[3];
    return true;
}

uint8_t ecu_pack_table_info(const ecu_table_info_t *m, uint8_t *out)
{
    wr_t w = {out, 0};
    w_u8(&w, m->table);
    w_u8(&w, m->rows);
    w_u8(&w, m->cols);
    w_u8(&w, m->value_unit);
    w_u8(&w, m->x_unit);
    w_u8(&w, m->y_unit);
    w_i16(&w, m->min);
    w_i16(&w, m->max);
    w_i16(&w, m->step);
    for (uint8_t i = 0; i < m->cols && i < ECU_MAX_AXIS; i++) {
        w_i16(&w, m->x[i]);
    }
    if (m->rows > 1) {
        for (uint8_t i = 0; i < m->rows && i < ECU_MAX_AXIS; i++) {
            w_i16(&w, m->y[i]);
        }
    }
    return w.n;
}

bool ecu_unpack_table_info(const uint8_t *in, uint8_t len, ecu_table_info_t *m)
{
    if (len < 12) {
        return false;
    }
    rd_t r = {in, 0};
    memset(m, 0, sizeof(*m));
    m->table = r_u8(&r);
    m->rows = r_u8(&r);
    m->cols = r_u8(&r);
    m->value_unit = r_u8(&r);
    m->x_unit = r_u8(&r);
    m->y_unit = r_u8(&r);
    m->min = r_i16(&r);
    m->max = r_i16(&r);
    m->step = r_i16(&r);
    if (m->rows == 0 || m->cols == 0 || m->rows > ECU_MAX_AXIS || m->cols > ECU_MAX_AXIS) {
        return false;
    }
    const unsigned expected = 12u + 2u * m->cols + (m->rows > 1 ? 2u * m->rows : 0u);
    if (len != expected) {
        return false;
    }
    for (uint8_t i = 0; i < m->cols; i++) {
        m->x[i] = r_i16(&r);
    }
    if (m->rows > 1) {
        for (uint8_t i = 0; i < m->rows; i++) {
            m->y[i] = r_i16(&r);
        }
    }
    return true;
}

uint8_t ecu_pack_cell_read_req(const ecu_cell_t *m, uint8_t *out)
{
    out[0] = m->table;
    out[1] = m->row;
    out[2] = m->col;
    return 3;
}

bool ecu_unpack_cell_read_req(const uint8_t *in, uint8_t len, ecu_cell_t *m)
{
    if (len != 3) {
        return false;
    }
    memset(m, 0, sizeof(*m));
    m->table = in[0];
    m->row = in[1];
    m->col = in[2];
    return true;
}

uint8_t ecu_pack_cell_write_req(const ecu_cell_t *m, uint8_t *out)
{
    wr_t w = {out, 0};
    w_u8(&w, m->table);
    w_u8(&w, m->row);
    w_u8(&w, m->col);
    w_i16(&w, m->value);
    return w.n;
}

bool ecu_unpack_cell_write_req(const uint8_t *in, uint8_t len, ecu_cell_t *m)
{
    if (len != 5) {
        return false;
    }
    rd_t r = {in, 0};
    memset(m, 0, sizeof(*m));
    m->table = r_u8(&r);
    m->row = r_u8(&r);
    m->col = r_u8(&r);
    m->value = r_i16(&r);
    return true;
}

uint8_t ecu_pack_cell_resp(const ecu_cell_t *m, uint8_t *out)
{
    wr_t w = {out, 0};
    w_u8(&w, m->table);
    w_u8(&w, m->row);
    w_u8(&w, m->col);
    w_u8(&w, m->status);
    w_i16(&w, m->value);
    return w.n;
}

bool ecu_unpack_cell_resp(const uint8_t *in, uint8_t len, ecu_cell_t *m)
{
    if (len != 6) {
        return false;
    }
    rd_t r = {in, 0};
    m->table = r_u8(&r);
    m->row = r_u8(&r);
    m->col = r_u8(&r);
    m->status = r_u8(&r);
    m->value = r_i16(&r);
    return true;
}

uint8_t ecu_pack_row_req(uint8_t table, uint8_t row, uint8_t *out)
{
    out[0] = table;
    out[1] = row;
    return 2;
}

bool ecu_unpack_row_req(const uint8_t *in, uint8_t len, uint8_t *table, uint8_t *row)
{
    if (len != 2) {
        return false;
    }
    *table = in[0];
    *row = in[1];
    return true;
}

uint8_t ecu_pack_row(const ecu_row_t *m, uint8_t *out)
{
    wr_t w = {out, 0};
    w_u8(&w, m->table);
    w_u8(&w, m->row);
    w_u8(&w, m->status);
    w_u8(&w, m->count);
    for (uint8_t i = 0; i < m->count && i < ECU_MAX_AXIS; i++) {
        w_i16(&w, m->values[i]);
    }
    return w.n;
}

bool ecu_unpack_row(const uint8_t *in, uint8_t len, ecu_row_t *m)
{
    if (len < 4) {
        return false;
    }
    rd_t r = {in, 0};
    memset(m, 0, sizeof(*m));
    m->table = r_u8(&r);
    m->row = r_u8(&r);
    m->status = r_u8(&r);
    m->count = r_u8(&r);
    if (m->count > ECU_MAX_AXIS || len != 4u + 2u * m->count) {
        return false;
    }
    for (uint8_t i = 0; i < m->count; i++) {
        m->values[i] = r_i16(&r);
    }
    return true;
}

uint8_t ecu_pack_param_write_req(uint8_t param, int16_t value, uint8_t *out)
{
    wr_t w = {out, 0};
    w_u8(&w, param);
    w_i16(&w, value);
    return w.n;
}

bool ecu_unpack_param_write_req(const uint8_t *in, uint8_t len, uint8_t *param, int16_t *value)
{
    if (len != 3) {
        return false;
    }
    rd_t r = {in, 0};
    *param = r_u8(&r);
    *value = r_i16(&r);
    return true;
}

uint8_t ecu_pack_param(const ecu_param_t *m, uint8_t *out)
{
    wr_t w = {out, 0};
    w_u8(&w, m->param);
    w_u8(&w, m->status);
    w_u8(&w, m->unit);
    w_i16(&w, m->value);
    w_i16(&w, m->min);
    w_i16(&w, m->max);
    w_i16(&w, m->step);
    return w.n;
}

bool ecu_unpack_param(const uint8_t *in, uint8_t len, ecu_param_t *m)
{
    if (len != 11) {
        return false;
    }
    rd_t r = {in, 0};
    memset(m, 0, sizeof(*m));
    m->param = r_u8(&r);
    m->status = r_u8(&r);
    m->unit = r_u8(&r);
    m->value = r_i16(&r);
    m->min = r_i16(&r);
    m->max = r_i16(&r);
    m->step = r_i16(&r);
    return true;
}

uint8_t ecu_pack_save_ack(const ecu_save_ack_t *m, uint8_t *out)
{
    wr_t w = {out, 0};
    w_u8(&w, m->status);
    w_u32(&w, m->crc32);
    return w.n;
}

bool ecu_unpack_save_ack(const uint8_t *in, uint8_t len, ecu_save_ack_t *m)
{
    if (len != 5) {
        return false;
    }
    rd_t r = {in, 0};
    m->status = r_u8(&r);
    m->crc32 = r_u32(&r);
    return true;
}

uint8_t ecu_pack_u16(uint16_t v, uint8_t *out)
{
    wr_t w = {out, 0};
    w_u16(&w, v);
    return w.n;
}

bool ecu_unpack_u16(const uint8_t *in, uint8_t len, uint16_t *v)
{
    if (len != 2) {
        return false;
    }
    rd_t r = {in, 0};
    *v = r_u16(&r);
    return true;
}

uint8_t ecu_pack_chunk(const ecu_chunk_t *m, uint8_t *out)
{
    wr_t w = {out, 0};
    const uint8_t count = m->count > ECU_BACKUP_CHUNK ? (uint8_t)ECU_BACKUP_CHUNK : m->count;
    w_u8(&w, m->status);
    w_u16(&w, m->offset);
    w_u16(&w, m->total);
    w_u8(&w, count);
    memcpy(&out[w.n], m->data, count);
    return (uint8_t)(w.n + count);
}

bool ecu_unpack_chunk(const uint8_t *in, uint8_t len, ecu_chunk_t *m)
{
    if (len < 6) {
        return false;
    }
    rd_t r = {in, 0};
    memset(m, 0, sizeof(*m));
    m->status = r_u8(&r);
    m->offset = r_u16(&r);
    m->total = r_u16(&r);
    m->count = r_u8(&r);
    if (m->count > ECU_BACKUP_CHUNK || len != 6u + m->count) {
        return false;
    }
    memcpy(m->data, &in[6], m->count);
    return true;
}

uint8_t ecu_pack_restore_begin(const ecu_restore_begin_t *m, uint8_t *out)
{
    wr_t w = {out, 0};
    w_u16(&w, m->total);
    w_u32(&w, m->crc32);
    return w.n;
}

bool ecu_unpack_restore_begin(const uint8_t *in, uint8_t len, ecu_restore_begin_t *m)
{
    if (len != 6) {
        return false;
    }
    rd_t r = {in, 0};
    m->total = r_u16(&r);
    m->crc32 = r_u32(&r);
    return true;
}

uint8_t ecu_pack_restore_ack(const ecu_restore_ack_t *m, uint8_t *out)
{
    wr_t w = {out, 0};
    w_u8(&w, m->status);
    w_u16(&w, m->received);
    return w.n;
}

bool ecu_unpack_restore_ack(const uint8_t *in, uint8_t len, ecu_restore_ack_t *m)
{
    if (len != 3) {
        return false;
    }
    rd_t r = {in, 0};
    m->status = r_u8(&r);
    m->received = r_u16(&r);
    return true;
}

uint8_t ecu_pack_nack(const ecu_nack_t *m, uint8_t *out)
{
    out[0] = m->reason;
    out[1] = m->rejected_type;
    return 2;
}

bool ecu_unpack_nack(const uint8_t *in, uint8_t len, ecu_nack_t *m)
{
    if (len != 2) {
        return false;
    }
    m->reason = in[0];
    m->rejected_type = in[1];
    return true;
}
