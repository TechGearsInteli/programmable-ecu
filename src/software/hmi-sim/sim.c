#include "sim.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

sim_t g_sim;

static void wire_send(void *ctx, const uint8_t *data, size_t len)
{
    sim_wire_t *w = (sim_wire_t *)ctx;
    if (w->len + len > sizeof(w->buf)) {
        return; /* fio "cheio": como uma UART sem espaco, perde bytes (o protocolo se recupera) */
    }
    memcpy(&w->buf[w->len], data, len);
    w->len += len;
}

static bool backup_write(void *ctx, const uint8_t *data, size_t len)
{
    (void)ctx;
    FILE *f = fopen(g_sim.backup_path, "wb");
    if (!f) return false;
    const bool ok = fwrite(data, 1, len, f) == len;
    fclose(f);
    return ok;
}

static size_t backup_read(void *ctx, uint8_t *buf, size_t cap)
{
    (void)ctx;
    FILE *f = fopen(g_sim.backup_path, "rb");
    if (!f) return 0;
    const size_t n = fread(buf, 1, cap, f);
    fclose(f);
    return n;
}

void sim_init(const char *data_dir, uint32_t now_ms, bool fresh)
{
    memset(&g_sim, 0, sizeof(g_sim));
    snprintf(g_sim.data_dir, sizeof(g_sim.data_dir), "%s", data_dir);
    MKDIR(data_dir);
    char flash[512];
    snprintf(flash, sizeof(flash), "%s/flash_motor.bin", data_dir);
    snprintf(g_sim.backup_path, sizeof(g_sim.backup_path), "%s/backup.tgcal", data_dir);
    if (fresh) {
        remove(flash);
        remove(g_sim.backup_path);
    }
    g_sim.now = now_ms;
    motor_virtual_init(&g_sim.motor, flash, wire_send, &g_sim.to_hmi, now_ms);
    ecu_hmi_link_init(&g_sim.link, wire_send, &g_sim.to_motor, hmi_ui_on_link_event, NULL, now_ms);
    g_sim.platform.backup_write = backup_write;
    g_sim.platform.backup_read = backup_read;
    g_sim.platform.ctx = NULL;
    g_sim.platform.backup_name = "backup.tgcal";
}

void sim_pump(uint32_t now_ms)
{
    g_sim.now = now_ms;
    motor_virtual_step(&g_sim.motor, now_ms);
    if (g_sim.to_motor.len) {
        static sim_wire_t w;
        w = g_sim.to_motor;
        g_sim.to_motor.len = 0;
        motor_virtual_feed(&g_sim.motor, w.buf, w.len);
    }
    if (g_sim.to_hmi.len) {
        static sim_wire_t w;
        w = g_sim.to_hmi;
        g_sim.to_hmi.len = 0;
        ecu_hmi_link_feed(&g_sim.link, w.buf, w.len, now_ms);
    }
    ecu_hmi_link_poll(&g_sim.link, now_ms);
}
