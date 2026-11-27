#include "motor_virtual.h"

#include "ecu/ecu_calib.h"

#include <stdio.h>
#include <string.h>

static bool file_write(void *ctx, const uint8_t *data, size_t len)
{
    motor_virtual_t *m = (motor_virtual_t *)ctx;
    /* grava num temporario e troca: como a NVS, ou fica o antigo ou o novo */
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.tmp", m->flash_path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    const bool ok = fwrite(data, 1, len, f) == len;
    fclose(f);
    if (!ok) return false;
    remove(m->flash_path);
    return rename(tmp, m->flash_path) == 0;
}

static size_t file_read(void *ctx, uint8_t *buf, size_t cap)
{
    motor_virtual_t *m = (motor_virtual_t *)ctx;
    FILE *f = fopen(m->flash_path, "rb");
    if (!f) return 0;
    const size_t n = fread(buf, 1, cap, f);
    fclose(f);
    return n;
}

void motor_virtual_init(motor_virtual_t *m, const char *flash_path, ecu_send_fn send, void *ctx, uint32_t now_ms)
{
    memset(m, 0, sizeof(*m));
    snprintf(m->flash_path, sizeof(m->flash_path), "%s", flash_path);
    m->storage.write = file_write;
    m->storage.read = file_read;
    m->storage.ctx = m;
    m->start_ms = now_ms;
    ecu_motor_link_init(&m->link, &m->calib, &m->storage, send, ctx, ECU_NODE_VIRTUAL);
    ecu_engine_init(&m->est);
    m->in.vbat_mv = 13800;
    m->in.iat_x10 = 250;
    m->in.clt_x10 = 250;
}

void motor_virtual_reboot(motor_virtual_t *m, uint32_t now_ms)
{
    const ecu_send_fn send = m->link.send;
    void *ctx = m->link.send_ctx;
    const ecu_inputs_t in = m->in;
    const bool manual = m->manual;
    char path[512];
    snprintf(path, sizeof(path), "%s", m->flash_path);
    motor_virtual_init(m, path, send, ctx, now_ms);
    m->in = in;
    m->manual = manual;
}

void motor_virtual_feed(motor_virtual_t *m, const uint8_t *data, size_t len)
{
    ecu_motor_link_feed(&m->link, data, len, m->in.rpm);
}

void motor_virtual_set(motor_virtual_t *m, uint16_t rpm, uint16_t map_kpa_x10, uint8_t tps, int16_t clt_x10)
{
    m->manual = true;
    m->in.rpm = rpm;
    m->in.map_kpa_x10 = map_kpa_x10;
    m->in.tps_pct = tps;
    m->in.clt_x10 = clt_x10;
    m->in.sync = rpm > 0;
}

void motor_virtual_auto(motor_virtual_t *m, bool on)
{
    m->manual = !on;
}

static int32_t ramp(int32_t a, int32_t b, uint32_t t, uint32_t t0, uint32_t t1)
{
    if (t <= t0) return a;
    if (t >= t1) return b;
    return a + (int32_t)((int64_t)(b - a) * (int64_t)(t - t0) / (int64_t)(t1 - t0));
}

/* Ciclo de 40 s que passa por todas as situacoes que a tela precisa mostrar. */
static void auto_cycle(motor_virtual_t *m, uint32_t now)
{
    const uint32_t t = (now - m->start_ms) % 40000u;
    const uint32_t total = now - m->start_ms;
    int32_t rpm, map, tps;
    if (t < 1500) { /* partida */
        rpm = ramp(0, 280, t, 0, 600);
        map = 950;
        tps = 0;
    } else if (t < 8000) { /* marcha lenta, firme: manual Honda do L15A */
        rpm = ECU_FIT_IDLE_RPM;
        map = 340;
        tps = 2;
    } else if (t < 14000) { /* aceleracao forte */
        rpm = ramp(ECU_FIT_IDLE_RPM, 6000, t, 8000, 14000);
        map = ramp(450, 960, t, 8000, 9000);
        tps = t < 8300 ? ramp(2, 85, t, 8000, 8300) : 85;
    } else if (t < 17000) { /* chega no corte do L15A; nao passa dele */
        rpm = ramp(6000, ECU_FIT_REV_CUT_RPM, t, 14000, 16000);
        map = 980;
        tps = 90;
    } else if (t < 22000) { /* tira o pe: desaceleracao */
        rpm = ramp(ECU_FIT_REV_CUT_RPM, 2200, t, 17000, 22000);
        map = 240;
        tps = 0;
    } else { /* cruzeiro: 2500 rpm e 60 kPa */
        rpm = 2500 + (int32_t)((t / 131) % 40) - 20;
        map = 600;
        tps = 25;
    }
    m->in.rpm = (uint16_t)rpm;
    m->in.map_kpa_x10 = (uint16_t)map;
    m->in.tps_pct = (uint8_t)tps;
    /* motor esquenta de 20 para 92 graus em ~2 minutos */
    m->in.clt_x10 = (int16_t)(200 + (int32_t)(720 * (total > 120000u ? 120000u : total) / 120000u));
    m->in.sync = rpm > 0;
}

void motor_virtual_step(motor_virtual_t *m, uint32_t now_ms)
{
    if (!m->manual) {
        auto_cycle(m, now_ms);
    }
    ecu_engine_compute(&m->calib, &m->in, &m->est, now_ms, &m->out);
    if (now_ms - m->last_tlm_ms >= ECU_TELEMETRY_PERIOD_MS) {
        m->last_tlm_ms = now_ms;
        ecu_motor_link_send_telemetry(&m->link, &m->in, &m->out, now_ms - m->start_ms);
    }
}
