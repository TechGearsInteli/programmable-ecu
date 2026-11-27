#include "ecu/ecu_hmi_link.h"

#include <string.h>

#include "ecu/ecu_crc.h"

#define PING_PERIOD_MS 1000u
#define LOST_AFTER_MS 2000u

static void emit(ecu_hmi_link_t *l, const ecu_hmi_event_t *ev)
{
    if (l->on_event) {
        l->on_event(l->event_ctx, ev);
    }
}

static void send_current(ecu_hmi_link_t *l)
{
    uint8_t frame[ECU_MAX_FRAME];
    const size_t n = ecu_frame_encode(frame, sizeof(frame), l->current_seq, l->current.type, l->current.payload,
                                      l->current.len);
    l->sent_ms = l->now_ms;
    if (n > 0 && l->send) {
        l->send(l->send_ctx, frame, n);
    }
}

static void start_request(ecu_hmi_link_t *l, const ecu_hmi_req_t *r)
{
    l->current = *r;
    l->current_seq = l->next_seq++;
    l->retries = 0;
    l->waiting = true;
    send_current(l);
}

static bool enqueue(ecu_hmi_link_t *l, uint8_t type, const uint8_t *payload, uint8_t len)
{
    if (l->q_count >= ECU_HMI_QUEUE) {
        return false;
    }
    ecu_hmi_req_t *r = &l->queue[(l->q_head + l->q_count) % ECU_HMI_QUEUE];
    r->type = type;
    r->len = len;
    if (len) {
        memcpy(r->payload, payload, len);
    }
    l->q_count++;
    return true;
}

/* ------------------------------------------------------------------------ */
/* Trabalhos longos                                                          */
/* ------------------------------------------------------------------------ */

static uint16_t sync_total(const ecu_hmi_link_t *l)
{
    uint16_t n = ECU_TBL_COUNT + ECU_PAR_COUNT;
    for (uint8_t t = 0; t < ECU_TBL_COUNT; t++) {
        n = (uint16_t)(n + (l->tables[t].info_ok ? l->tables[t].info.rows : ecu_table_desc(t)->rows));
    }
    return n;
}

/* Gera o pedido numero @p step da sincronizacao. false = acabou. */
static bool sync_request(const ecu_hmi_link_t *l, uint16_t step, ecu_hmi_req_t *r)
{
    if (step < ECU_TBL_COUNT) {
        r->type = ECU_MSG_TABLE_INFO_REQ;
        r->payload[0] = (uint8_t)step;
        r->len = 1;
        return true;
    }
    step = (uint16_t)(step - ECU_TBL_COUNT);
    for (uint8_t t = 0; t < ECU_TBL_COUNT; t++) {
        const uint8_t rows = l->tables[t].info_ok ? l->tables[t].info.rows : 0;
        if (step < rows) {
            r->type = ECU_MSG_ROW_READ_REQ;
            r->len = ecu_pack_row_req(t, (uint8_t)step, r->payload);
            return true;
        }
        step = (uint16_t)(step - rows);
    }
    if (step < ECU_PAR_COUNT) {
        r->type = ECU_MSG_PARAM_READ_REQ;
        r->payload[0] = (uint8_t)step;
        r->len = 1;
        return true;
    }
    return false;
}

static uint16_t restore_chunks(const ecu_hmi_link_t *l)
{
    return (uint16_t)((l->blob_len + ECU_BACKUP_CHUNK - 1) / ECU_BACKUP_CHUNK);
}

/* Restauracao: passo 0 = BEGIN, 1..N = blocos, N+1 = COMMIT */
static bool restore_request(const ecu_hmi_link_t *l, uint16_t step, ecu_hmi_req_t *r)
{
    const uint16_t chunks = restore_chunks(l);
    if (step == 0) {
        const ecu_restore_begin_t b = {l->blob_len, ecu_crc32(l->blob, l->blob_len)};
        r->type = ECU_MSG_RESTORE_BEGIN_REQ;
        r->len = ecu_pack_restore_begin(&b, r->payload);
        return true;
    }
    if (step <= chunks) {
        ecu_chunk_t c;
        memset(&c, 0, sizeof(c));
        c.offset = (uint16_t)((step - 1) * ECU_BACKUP_CHUNK);
        c.total = l->blob_len;
        const uint16_t left = (uint16_t)(l->blob_len - c.offset);
        c.count = (uint8_t)(left < ECU_BACKUP_CHUNK ? left : ECU_BACKUP_CHUNK);
        memcpy(c.data, &l->blob[c.offset], c.count);
        r->type = ECU_MSG_RESTORE_DATA_REQ;
        r->len = ecu_pack_chunk(&c, r->payload);
        return true;
    }
    if (step == chunks + 1) {
        r->type = ECU_MSG_RESTORE_COMMIT_REQ;
        r->len = 0;
        return true;
    }
    return false;
}

static bool job_request(ecu_hmi_link_t *l, ecu_hmi_req_t *r)
{
    switch (l->job) {
    case ECU_JOB_SYNC: return sync_request(l, l->job_step, r);
    case ECU_JOB_BACKUP:
        r->type = ECU_MSG_BACKUP_READ_REQ;
        r->len = ecu_pack_u16(l->blob_len, r->payload);
        return true;
    case ECU_JOB_RESTORE: return restore_request(l, l->job_step, r);
    default: return false;
    }
}

static void job_finish(ecu_hmi_link_t *l, uint8_t ev_type, uint8_t status)
{
    l->job = ECU_JOB_NONE;
    l->job_step = 0;
    ecu_hmi_event_t ev = {0};
    ev.type = ev_type;
    ev.status = status;
    emit(l, &ev);
}

static void start_sync(ecu_hmi_link_t *l)
{
    for (uint8_t t = 0; t < ECU_TBL_COUNT; t++) {
        l->tables[t].info_ok = false;
        l->tables[t].rows_ok = 0;
    }
    for (uint8_t p = 0; p < ECU_PAR_COUNT; p++) {
        l->params[p].ok = false;
    }
    l->job = ECU_JOB_SYNC;
    l->job_step = 0;
}

/* Manda o proximo pedido, se o canal estiver livre. */
static void pump(ecu_hmi_link_t *l)
{
    if (l->waiting) {
        return;
    }
    if (l->q_count > 0) {
        ecu_hmi_req_t r = l->queue[l->q_head];
        l->q_head = (uint8_t)((l->q_head + 1) % ECU_HMI_QUEUE);
        l->q_count--;
        start_request(l, &r);
        return;
    }
    if (l->connected && l->job != ECU_JOB_NONE) {
        ecu_hmi_req_t r;
        if (job_request(l, &r)) {
            start_request(l, &r);
        } else if (l->job == ECU_JOB_SYNC) {
            job_finish(l, ECU_HMI_EV_SYNCED, ECU_OK);
        }
        return;
    }
    if (!l->connected && l->now_ms - l->last_ping_ms >= PING_PERIOD_MS) {
        ecu_hmi_req_t r = {ECU_MSG_PING, 0, {0}};
        l->last_ping_ms = l->now_ms;
        start_request(l, &r);
    }
}

/* ------------------------------------------------------------------------ */
/* Respostas                                                                 */
/* ------------------------------------------------------------------------ */

static void on_response(ecu_hmi_link_t *l, const ecu_frame_t *f)
{
    const uint8_t req = l->current.type;
    ecu_hmi_event_t ev = {0};
    ev.req_type = req;

    if (f->type == ECU_MSG_NACK) {
        if (l->job != ECU_JOB_NONE) {
            l->job = ECU_JOB_NONE;
        }
        ev.type = ECU_HMI_EV_TIMEOUT;
        ev.status = 0xFE;
        emit(l, &ev);
        return;
    }

    switch (f->type) {
    case ECU_MSG_PONG:
        if (ecu_unpack_pong(f->payload, f->len, &l->pong)) {
            l->connected = true;
            ev.type = ECU_HMI_EV_CONNECTED;
            emit(l, &ev);
            start_sync(l);
        }
        break;
    case ECU_MSG_TABLE_INFO: {
        ecu_table_info_t info;
        if (ecu_unpack_table_info(f->payload, f->len, &info) && info.table < ECU_TBL_COUNT) {
            l->tables[info.table].info = info;
            l->tables[info.table].info_ok = true;
        }
        if (l->job == ECU_JOB_SYNC) l->job_step++;
        break;
    }
    case ECU_MSG_ROW_DATA: {
        ecu_row_t r;
        if (ecu_unpack_row(f->payload, f->len, &r) && r.status == ECU_OK && r.table < ECU_TBL_COUNT &&
            r.row < ECU_MAP_ROWS) {
            memcpy(l->tables[r.table].v[r.row], r.values, (size_t)r.count * sizeof(int16_t));
            l->tables[r.table].rows_ok |= (uint16_t)(1u << r.row);
        }
        if (l->job == ECU_JOB_SYNC) l->job_step++;
        break;
    }
    case ECU_MSG_CELL_DATA:
    case ECU_MSG_CELL_WRITE_ACK: {
        ecu_cell_t c;
        if (ecu_unpack_cell_resp(f->payload, f->len, &c)) {
            if (c.table < ECU_TBL_COUNT && c.row < ECU_MAP_ROWS && c.col < ECU_MAP_COLS &&
                (c.status == ECU_OK || c.status == ECU_ERR_RANGE)) {
                l->tables[c.table].v[c.row][c.col] = c.value; /* o valor que o motor diz que vale */
            }
            if (f->type == ECU_MSG_CELL_WRITE_ACK) {
                ev.type = ECU_HMI_EV_CELL_ACK;
                ev.status = c.status;
                ev.table = c.table;
                ev.row = c.row;
                ev.col = c.col;
                ev.value = c.value;
                emit(l, &ev);
            }
        }
        break;
    }
    case ECU_MSG_PARAM_DATA:
    case ECU_MSG_PARAM_WRITE_ACK: {
        ecu_param_t p;
        if (ecu_unpack_param(f->payload, f->len, &p) && p.param < ECU_PAR_COUNT) {
            if (p.status != ECU_ERR_PARAM) {
                ecu_param_t keep = p;
                keep.status = ECU_OK;
                l->params[p.param].p = keep;
                l->params[p.param].ok = true;
            }
            if (f->type == ECU_MSG_PARAM_WRITE_ACK) {
                ev.type = ECU_HMI_EV_PARAM_ACK;
                ev.status = p.status;
                ev.param = p.param;
                ev.value = p.value;
                emit(l, &ev);
            }
        }
        if (l->job == ECU_JOB_SYNC && f->type == ECU_MSG_PARAM_DATA) l->job_step++;
        break;
    }
    case ECU_MSG_SAVE_ACK: {
        ecu_save_ack_t a;
        if (ecu_unpack_save_ack(f->payload, f->len, &a)) {
            ev.type = ECU_HMI_EV_SAVE_ACK;
            ev.status = a.status;
            emit(l, &ev);
        }
        break;
    }
    case ECU_MSG_BACKUP_DATA: {
        ecu_chunk_t c;
        if (!ecu_unpack_chunk(f->payload, f->len, &c) || c.status != ECU_OK || c.offset != l->blob_len ||
            c.total > sizeof(l->blob) || (uint32_t)c.offset + c.count > c.total) {
            job_finish(l, ECU_HMI_EV_BACKUP_DONE, ECU_ERR_BACKUP);
            break;
        }
        memcpy(&l->blob[c.offset], c.data, c.count);
        l->blob_len = (uint16_t)(l->blob_len + c.count);
        l->blob_total = c.total;
        if (l->blob_len >= c.total) {
            job_finish(l, ECU_HMI_EV_BACKUP_DONE, ECU_OK);
        }
        break;
    }
    case ECU_MSG_RESTORE_BEGIN_ACK:
    case ECU_MSG_RESTORE_DATA_ACK:
    case ECU_MSG_RESTORE_COMMIT_ACK: {
        ecu_restore_ack_t a;
        if (!ecu_unpack_restore_ack(f->payload, f->len, &a) || a.status != ECU_OK) {
            job_finish(l, ECU_HMI_EV_RESTORE_DONE, a.status != ECU_OK ? a.status : ECU_ERR_BACKUP);
            break;
        }
        if (f->type == ECU_MSG_RESTORE_COMMIT_ACK) {
            job_finish(l, ECU_HMI_EV_RESTORE_DONE, ECU_OK);
            start_sync(l); /* a calibracao inteira mudou: recarrega a copia */
        } else {
            l->job_step++;
        }
        break;
    }
    default:
        break;
    }
}

static void on_frame(void *ctx, const ecu_frame_t *f)
{
    ecu_hmi_link_t *l = (ecu_hmi_link_t *)ctx;
    if (f->type == ECU_MSG_TELEMETRY) {
        if (ecu_unpack_telemetry(f->payload, f->len, &l->tlm)) {
            l->tlm_ok = true;
            l->tlm_ms = l->now_ms;
            l->tlm_count++;
        }
        return;
    }
    if (!l->waiting || f->seq != l->current_seq) {
        return; /* resposta atrasada ou repetida: ja foi tratada */
    }
    if (f->type != ECU_RESPONSE_OF(l->current.type) && f->type != ECU_MSG_NACK) {
        return;
    }
    l->waiting = false;
    on_response(l, f);
    pump(l);
}

/* ------------------------------------------------------------------------ */
/* API                                                                       */
/* ------------------------------------------------------------------------ */

void ecu_hmi_link_init(ecu_hmi_link_t *l, ecu_send_fn send, void *send_ctx, ecu_hmi_event_cb on_event,
                       void *event_ctx, uint32_t now_ms)
{
    memset(l, 0, sizeof(*l));
    l->send = send;
    l->send_ctx = send_ctx;
    l->on_event = on_event;
    l->event_ctx = event_ctx;
    l->now_ms = now_ms;
    l->next_seq = (uint8_t)(now_ms | 1u);
    l->last_ping_ms = now_ms - PING_PERIOD_MS;
    ecu_parser_init(&l->parser);
}

void ecu_hmi_link_feed(ecu_hmi_link_t *l, const uint8_t *data, size_t len, uint32_t now_ms)
{
    l->now_ms = now_ms;
    ecu_parser_feed(&l->parser, data, len, on_frame, l);
}

void ecu_hmi_link_line_idle(ecu_hmi_link_t *l)
{
    ecu_parser_idle(&l->parser, on_frame, l);
}

void ecu_hmi_link_poll(ecu_hmi_link_t *l, uint32_t now_ms)
{
    l->now_ms = now_ms;
    if (l->connected && l->tlm_ok && now_ms - l->tlm_ms > LOST_AFTER_MS) {
        /* o motor sumiu (desligado, cabo solto, reiniciou): volta a procurar */
        l->connected = false;
        l->tlm_ok = false;
    }
    if (l->waiting && now_ms - l->sent_ms >= ECU_RESPONSE_TIMEOUT_MS) {
        if (l->retries < ECU_MAX_RETRIES) {
            l->retries++;
            send_current(l);
        } else {
            l->waiting = false;
            l->timeouts++;
            if (l->current.type != ECU_MSG_PING) {
                ecu_hmi_event_t ev = {0};
                ev.type = ECU_HMI_EV_TIMEOUT;
                ev.req_type = l->current.type;
                emit(l, &ev);
                if (l->job != ECU_JOB_NONE) {
                    l->job = ECU_JOB_NONE;
                    l->connected = false; /* volta ao PING e sincroniza de novo */
                }
            }
        }
    }
    pump(l);
}

bool ecu_hmi_link_alive(const ecu_hmi_link_t *l, uint32_t now_ms)
{
    return l->tlm_ok && now_ms - l->tlm_ms <= ECU_LINK_ALIVE_MS;
}

bool ecu_hmi_link_synced(const ecu_hmi_link_t *l)
{
    for (uint8_t t = 0; t < ECU_TBL_COUNT; t++) {
        const ecu_hmi_table_t *tb = &l->tables[t];
        if (!tb->info_ok || tb->rows_ok != (uint16_t)((1u << tb->info.rows) - 1u)) {
            return false;
        }
    }
    for (uint8_t p = 0; p < ECU_PAR_COUNT; p++) {
        if (!l->params[p].ok) {
            return false;
        }
    }
    return true;
}

bool ecu_hmi_link_busy(const ecu_hmi_link_t *l)
{
    return l->waiting || l->q_count > 0 || l->job != ECU_JOB_NONE;
}

uint8_t ecu_hmi_link_job_progress(const ecu_hmi_link_t *l)
{
    uint32_t done = 0, total = 1;
    switch (l->job) {
    case ECU_JOB_SYNC: done = l->job_step; total = sync_total(l); break;
    case ECU_JOB_BACKUP:
        done = l->blob_len;
        total = l->blob_total ? l->blob_total : ecu_calib_blob_size();
        break;
    case ECU_JOB_RESTORE: done = l->job_step; total = (uint32_t)restore_chunks(l) + 2u; break;
    default: return 100;
    }
    return (uint8_t)(done >= total ? 100u : done * 100u / total);
}

bool ecu_hmi_link_resync(ecu_hmi_link_t *l)
{
    if (l->job != ECU_JOB_NONE) {
        return false;
    }
    start_sync(l);
    pump(l);
    return true;
}

bool ecu_hmi_link_write_cell(ecu_hmi_link_t *l, uint8_t table, uint8_t row, uint8_t col, int16_t value)
{
    ecu_cell_t c = {table, row, col, 0, value};
    uint8_t p[8];
    const bool ok = enqueue(l, ECU_MSG_CELL_WRITE_REQ, p, ecu_pack_cell_write_req(&c, p));
    pump(l);
    return ok;
}

bool ecu_hmi_link_write_param(ecu_hmi_link_t *l, uint8_t param, int16_t value)
{
    uint8_t p[4];
    const bool ok = enqueue(l, ECU_MSG_PARAM_WRITE_REQ, p, ecu_pack_param_write_req(param, value, p));
    pump(l);
    return ok;
}

bool ecu_hmi_link_save(ecu_hmi_link_t *l)
{
    const bool ok = enqueue(l, ECU_MSG_SAVE_REQ, NULL, 0);
    pump(l);
    return ok;
}

bool ecu_hmi_link_backup(ecu_hmi_link_t *l)
{
    if (l->job != ECU_JOB_NONE || !l->connected) {
        return false;
    }
    l->job = ECU_JOB_BACKUP;
    l->job_step = 0;
    l->blob_len = 0;
    l->blob_total = 0;
    pump(l);
    return true;
}

bool ecu_hmi_link_restore(ecu_hmi_link_t *l, const uint8_t *blob, uint16_t len)
{
    if (l->job != ECU_JOB_NONE || !l->connected || len == 0 || len > sizeof(l->blob)) {
        return false;
    }
    memcpy(l->blob, blob, len);
    l->blob_len = len;
    l->job = ECU_JOB_RESTORE;
    l->job_step = 0;
    pump(l);
    return true;
}

uint8_t ecu_hmi_link_precheck_cell(const ecu_hmi_link_t *l, uint8_t table, int16_t value)
{
    if (table >= ECU_TBL_COUNT) {
        return ECU_ERR_TABLE;
    }
    const ecu_table_info_t *i = &l->tables[table].info;
    const int16_t min = l->tables[table].info_ok ? i->min : ecu_table_desc(table)->min;
    const int16_t max = l->tables[table].info_ok ? i->max : ecu_table_desc(table)->max;
    return (value < min || value > max) ? ECU_ERR_RANGE : ECU_OK;
}
