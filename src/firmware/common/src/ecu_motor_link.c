#include "ecu/ecu_motor_link.h"

#include <string.h>

#include "ecu/ecu_crc.h"

void ecu_motor_link_init(ecu_motor_link_t *l, ecu_calib_t *calib, const ecu_storage_t *storage,
                         ecu_send_fn send, void *send_ctx, uint8_t node)
{
    memset(l, 0, sizeof(*l));
    l->calib = calib;
    l->storage = storage;
    l->send = send;
    l->send_ctx = send_ctx;
    l->node = node;
    ecu_parser_init(&l->parser);
    l->calib_default = !ecu_calib_load(calib, storage);
    /* Referencia para "alterado": o que esta na flash ou, se ela estiver vazia, o padrao. */
    l->saved_crc = ecu_calib_crc(calib);
}

bool ecu_motor_link_dirty(const ecu_motor_link_t *l)
{
    return ecu_calib_crc(l->calib) != l->saved_crc;
}

static uint8_t calib_flags(const ecu_motor_link_t *l)
{
    uint8_t f = 0;
    if (l->calib_default) f |= ECU_TLM_CALIB_DEFAULT;
    if (ecu_motor_link_dirty(l)) f |= ECU_TLM_CALIB_DIRTY;
    return f;
}

static void reply(ecu_motor_link_t *l, uint8_t seq, uint8_t req_type, uint8_t type, const uint8_t *payload,
                  uint8_t len)
{
    const size_t n = ecu_frame_encode(l->last_resp, sizeof(l->last_resp), seq, type, payload, len);
    l->have_last = n > 0;
    l->last_seq = seq;
    l->last_type = req_type;
    l->last_resp_len = n;
    if (n > 0 && l->send) {
        l->send(l->send_ctx, l->last_resp, n);
    }
}

static void nack(ecu_motor_link_t *l, const ecu_frame_t *f, uint8_t reason)
{
    uint8_t p[2];
    const ecu_nack_t m = {reason, f->type};
    l->nacks++;
    reply(l, f->seq, f->type, ECU_MSG_NACK, p, ecu_pack_nack(&m, p));
}

static void handle_cell(ecu_motor_link_t *l, const ecu_frame_t *f, bool write)
{
    ecu_cell_t c;
    const bool ok = write ? ecu_unpack_cell_write_req(f->payload, f->len, &c)
                          : ecu_unpack_cell_read_req(f->payload, f->len, &c);
    if (!ok) {
        nack(l, f, ECU_NACK_BAD_LENGTH);
        return;
    }
    if (write) {
        c.status = ecu_calib_set_cell(l->calib, c.table, c.row, c.col, c.value);
    } else {
        const ecu_table_desc_t *d = ecu_table_desc(c.table);
        c.status = !d ? ECU_ERR_TABLE : (c.row >= d->rows || c.col >= d->cols) ? ECU_ERR_INDEX : ECU_OK;
    }
    /* A resposta traz o valor que esta valendo: o novo, se aceito; o antigo, se recusado. */
    if (c.status == ECU_OK || c.status == ECU_ERR_RANGE) {
        c.value = l->calib->table[c.table].v[c.row][c.col];
    }
    uint8_t p[8];
    reply(l, f->seq, f->type, ECU_RESPONSE_OF(f->type), p, ecu_pack_cell_resp(&c, p));
}

static void handle_row(ecu_motor_link_t *l, const ecu_frame_t *f)
{
    ecu_row_t r;
    memset(&r, 0, sizeof(r));
    if (!ecu_unpack_row_req(f->payload, f->len, &r.table, &r.row)) {
        nack(l, f, ECU_NACK_BAD_LENGTH);
        return;
    }
    const ecu_table_desc_t *d = ecu_table_desc(r.table);
    if (!d) {
        r.status = ECU_ERR_TABLE;
    } else if (r.row >= d->rows) {
        r.status = ECU_ERR_INDEX;
    } else {
        r.count = d->cols;
        memcpy(r.values, l->calib->table[r.table].v[r.row], (size_t)d->cols * sizeof(int16_t));
    }
    uint8_t p[ECU_MAX_PAYLOAD];
    reply(l, f->seq, f->type, ECU_MSG_ROW_DATA, p, ecu_pack_row(&r, p));
}

static void handle_table_info(ecu_motor_link_t *l, const ecu_frame_t *f)
{
    if (f->len != 1) {
        nack(l, f, ECU_NACK_BAD_LENGTH);
        return;
    }
    ecu_table_info_t info;
    ecu_calib_table_info(l->calib, f->payload[0], &info);
    uint8_t p[ECU_MAX_PAYLOAD];
    /* tabela inexistente: rows = 0 -> a tela trata como erro */
    reply(l, f->seq, f->type, ECU_MSG_TABLE_INFO, p, ecu_pack_table_info(&info, p));
}

static void fill_param(const ecu_motor_link_t *l, uint8_t param, uint8_t status, ecu_param_t *m)
{
    memset(m, 0, sizeof(*m));
    m->param = param;
    m->status = status;
    const ecu_param_desc_t *d = ecu_param_desc(param);
    if (d) {
        m->unit = d->unit;
        m->value = l->calib->param[param];
        m->min = d->min;
        m->max = d->max;
        m->step = d->step;
    } else if (status == ECU_OK) {
        m->status = ECU_ERR_PARAM;
    }
}

static void handle_param(ecu_motor_link_t *l, const ecu_frame_t *f, bool write)
{
    ecu_param_t m;
    if (!write) {
        if (f->len != 1) {
            nack(l, f, ECU_NACK_BAD_LENGTH);
            return;
        }
        fill_param(l, f->payload[0], ECU_OK, &m);
    } else {
        uint8_t param;
        int16_t value;
        if (!ecu_unpack_param_write_req(f->payload, f->len, &param, &value)) {
            nack(l, f, ECU_NACK_BAD_LENGTH);
            return;
        }
        uint8_t st;
        if (!ecu_param_desc(param)) {
            st = ECU_ERR_PARAM;
        } else if (l->rpm_now != 0) {
            st = ECU_ERR_ENGINE_RUNNING; /* ficha muda o calculo inteiro: so parado */
        } else {
            st = ecu_calib_set_param(l->calib, param, value);
        }
        fill_param(l, param, st, &m);
    }
    uint8_t p[16];
    reply(l, f->seq, f->type, ECU_RESPONSE_OF(f->type), p, ecu_pack_param(&m, p));
}

static void handle_save(ecu_motor_link_t *l, const ecu_frame_t *f)
{
    ecu_save_ack_t a = {ECU_OK, 0};
    if (l->rpm_now != 0) {
        a.status = ECU_ERR_ENGINE_RUNNING;
    } else {
        a.status = ecu_calib_save(l->calib, l->storage, &a.crc32);
        if (a.status == ECU_OK) {
            l->saved_crc = a.crc32;
            l->calib_default = false;
        }
    }
    uint8_t p[8];
    reply(l, f->seq, f->type, ECU_MSG_SAVE_ACK, p, ecu_pack_save_ack(&a, p));
}

static void handle_backup_read(ecu_motor_link_t *l, const ecu_frame_t *f)
{
    ecu_chunk_t c;
    memset(&c, 0, sizeof(c));
    if (!ecu_unpack_u16(f->payload, f->len, &c.offset)) {
        nack(l, f, ECU_NACK_BAD_LENGTH);
        return;
    }
    const uint16_t total = (uint16_t)ecu_calib_blob_size();
    c.total = total;
    if (c.offset == 0) {
        /* foto da calibracao no inicio: os blocos ficam coerentes entre si */
        ecu_calib_serialize(l->calib, l->blob, sizeof(l->blob));
    }
    if (c.offset >= total) {
        c.status = ECU_ERR_INDEX;
    } else {
        const uint16_t left = (uint16_t)(total - c.offset);
        c.count = (uint8_t)(left < ECU_BACKUP_CHUNK ? left : ECU_BACKUP_CHUNK);
        memcpy(c.data, &l->blob[c.offset], c.count);
    }
    uint8_t p[ECU_MAX_PAYLOAD];
    reply(l, f->seq, f->type, ECU_MSG_BACKUP_DATA, p, ecu_pack_chunk(&c, p));
}

static void restore_reply(ecu_motor_link_t *l, const ecu_frame_t *f, uint8_t status)
{
    const ecu_restore_ack_t a = {status, l->restore_received};
    uint8_t p[4];
    reply(l, f->seq, f->type, ECU_RESPONSE_OF(f->type), p, ecu_pack_restore_ack(&a, p));
}

static void handle_restore(ecu_motor_link_t *l, const ecu_frame_t *f)
{
    if (f->type == ECU_MSG_RESTORE_BEGIN_REQ) {
        ecu_restore_begin_t b;
        if (!ecu_unpack_restore_begin(f->payload, f->len, &b)) {
            nack(l, f, ECU_NACK_BAD_LENGTH);
            return;
        }
        l->restore_active = b.total == ecu_calib_blob_size();
        l->restore_total = b.total;
        l->restore_received = 0;
        l->restore_crc = b.crc32;
        restore_reply(l, f, l->restore_active ? ECU_OK : ECU_ERR_BACKUP);
        return;
    }
    if (f->type == ECU_MSG_RESTORE_DATA_REQ) {
        ecu_chunk_t c;
        if (!ecu_unpack_chunk(f->payload, f->len, &c)) {
            nack(l, f, ECU_NACK_BAD_LENGTH);
            return;
        }
        uint8_t st = ECU_OK;
        if (!l->restore_active) {
            st = ECU_ERR_BACKUP;
        } else if (c.offset != l->restore_received || (uint32_t)c.offset + c.count > l->restore_total) {
            st = ECU_ERR_INDEX;
        } else {
            memcpy(&l->blob[c.offset], c.data, c.count);
            l->restore_received = (uint16_t)(l->restore_received + c.count);
        }
        restore_reply(l, f, st);
        return;
    }
    /* COMMIT */
    uint8_t st = ECU_OK;
    if (l->rpm_now != 0) {
        st = ECU_ERR_ENGINE_RUNNING;
    } else if (!l->restore_active || l->restore_received != l->restore_total ||
               ecu_crc32(l->blob, l->restore_total) != l->restore_crc) {
        st = ECU_ERR_BACKUP;
    } else {
        st = ecu_calib_deserialize(&l->staging, l->blob, l->restore_total);
        if (st == ECU_OK) {
            if (l->lock) l->lock(l->lock_ctx);
            *l->calib = l->staging; /* aplica na RAM; a flash so muda com SAVE */
            if (l->unlock) l->unlock(l->lock_ctx);
        }
    }
    if (st != ECU_ERR_ENGINE_RUNNING) {
        l->restore_active = false;
    }
    restore_reply(l, f, st);
}

static void on_frame(void *ctx, const ecu_frame_t *f)
{
    ecu_motor_link_t *l = (ecu_motor_link_t *)ctx;
    if (f->type & 0x80u || f->type == ECU_MSG_TELEMETRY) {
        return; /* respostas e telemetria nao sao pedidos */
    }
    if (l->have_last && f->seq == l->last_seq && f->type == l->last_type) {
        if (l->send) l->send(l->send_ctx, l->last_resp, l->last_resp_len); /* reenvio */
        return;
    }
    l->requests++;
    switch (f->type) {
    case ECU_MSG_PING: {
        const ecu_pong_t m = {ECU_PROTO_VERSION, l->node, ECU_CALIB_VERSION, calib_flags(l)};
        uint8_t p[4];
        reply(l, f->seq, f->type, ECU_MSG_PONG, p, ecu_pack_pong(&m, p));
        break;
    }
    case ECU_MSG_TABLE_INFO_REQ: handle_table_info(l, f); break;
    case ECU_MSG_CELL_READ_REQ: handle_cell(l, f, false); break;
    case ECU_MSG_ROW_READ_REQ: handle_row(l, f); break;
    case ECU_MSG_CELL_WRITE_REQ: handle_cell(l, f, true); break;
    case ECU_MSG_PARAM_READ_REQ: handle_param(l, f, false); break;
    case ECU_MSG_PARAM_WRITE_REQ: handle_param(l, f, true); break;
    case ECU_MSG_SAVE_REQ: handle_save(l, f); break;
    case ECU_MSG_BACKUP_READ_REQ: handle_backup_read(l, f); break;
    case ECU_MSG_RESTORE_BEGIN_REQ:
    case ECU_MSG_RESTORE_DATA_REQ:
    case ECU_MSG_RESTORE_COMMIT_REQ: handle_restore(l, f); break;
    default: nack(l, f, ECU_NACK_UNKNOWN_TYPE); break;
    }
}

void ecu_motor_link_feed(ecu_motor_link_t *l, const uint8_t *data, size_t len, uint16_t rpm_now)
{
    l->rpm_now = rpm_now;
    ecu_parser_feed(&l->parser, data, len, on_frame, l);
}

void ecu_motor_link_line_idle(ecu_motor_link_t *l)
{
    ecu_parser_idle(&l->parser, on_frame, l);
}

void ecu_telemetry_fill(ecu_telemetry_t *t, const ecu_inputs_t *in, const ecu_outputs_t *out, uint8_t flags,
                        uint32_t uptime_ms)
{
    memset(t, 0, sizeof(*t));
    t->rpm = in->rpm;
    t->flags = flags;
    if (in->sync) t->flags |= ECU_TLM_SYNC;
    if (out->cranking) t->flags |= ECU_TLM_CRANKING;
    if (in->rpm > 0) t->flags |= ECU_TLM_RUNNING;
    t->tps_pct = in->tps_pct;
    t->pw_us = out->pw_us;
    t->advance_x10 = out->advance_x10;
    t->dwell_us = out->dwell_us;
    t->map_kpa_x10 = in->map_kpa_x10;
    t->clt_x10 = in->clt_x10;
    t->iat_x10 = in->iat_x10;
    t->vbat_mv = in->vbat_mv;
    t->fuel_row = out->fuel_row;
    t->fuel_col = out->fuel_col;
    t->fuel_cell = out->fuel_cell;
    t->fuel_interp_x100 = out->fuel_interp_x100;
    t->ign_row = out->ign_row;
    t->ign_col = out->ign_col;
    t->ign_cell = out->ign_cell;
    t->corr_warm_x10 = out->corr_warm_x10;
    t->corr_iat_x10 = out->corr_iat_x10;
    t->corr_crank_x10 = out->corr_crank_x10;
    t->corr_accel_x10 = out->corr_accel_x10;
    t->dead_time_us = out->dead_time_us;
    t->inj_scale_x1000 = out->inj_scale_x1000;
    t->uptime_ms = uptime_ms;
}

void ecu_motor_link_send_telemetry(ecu_motor_link_t *l, const ecu_inputs_t *in, const ecu_outputs_t *out,
                                   uint32_t uptime_ms)
{
    ecu_telemetry_t t;
    ecu_telemetry_fill(&t, in, out, calib_flags(l), uptime_ms);
    uint8_t p[ECU_TELEMETRY_LEN];
    uint8_t frame[ECU_TELEMETRY_LEN + ECU_FRAME_OVERHEAD];
    const size_t n = ecu_frame_encode(frame, sizeof(frame), l->tlm_seq++, ECU_MSG_TELEMETRY, p,
                                      ecu_pack_telemetry(&t, p));
    if (n > 0 && l->send) {
        l->send(l->send_ctx, frame, n);
    }
}
