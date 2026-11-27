/* Modo sem janela: display em memoria, "dedo" virtual e capturas BMP.
 * O relogio e simulado (avanca de 10 em 10 ms), entao o resultado e o mesmo
 * em qualquer computador. */
#include <stdio.h>
#include <string.h>

#include "sim.h"
#include "src/widgets/table/lv_table_private.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

static uint16_t framebuf[HMI_SCREEN_W * HMI_SCREEN_H];
static lv_display_t *disp;
static uint32_t sim_ms = 1000;
static lv_point_t finger;
static bool finger_down;

static uint32_t sim_tick(void)
{
    return sim_ms;
}

static void flush_cb(lv_display_t *d, const lv_area_t *area, uint8_t *px)
{
    (void)area;
    (void)px;
    lv_display_flush_ready(d); /* modo DIRECT: o buffer ja e a tela inteira */
}

static void finger_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point = finger;
    data->state = finger_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void sim_run_ms(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 10) {
        sim_ms += 10;
        sim_pump(sim_ms);
        lv_timer_handler();
    }
}

static void tap_at(int32_t x, int32_t y)
{
    finger.x = x;
    finger.y = y;
    finger_down = true;
    sim_run_ms(80);
    finger_down = false;
    sim_run_ms(80);
}

void sim_tap_obj(lv_obj_t *obj)
{
    lv_obj_update_layout(lv_screen_active());
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    tap_at((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
}

void sim_tap_table_cell(lv_obj_t *table, uint32_t vrow, uint32_t vcol)
{
    lv_obj_update_layout(lv_screen_active());
    lv_table_t *t = (lv_table_t *)table;
    lv_area_t a;
    lv_obj_get_coords(table, &a);
    int32_t x = a.x1 + lv_obj_get_style_pad_left(table, LV_PART_MAIN);
    int32_t y = a.y1 + lv_obj_get_style_pad_top(table, LV_PART_MAIN);
    for (uint32_t c = 0; c < vcol; c++) x += t->col_w[c];
    for (uint32_t r = 0; r < vrow; r++) y += t->row_h[r];
    tap_at(x + t->col_w[vcol] / 2, y + t->row_h[vrow] / 2);
}

bool sim_snapshot(const char *out_dir, const char *name)
{
    lv_refr_now(disp);
    char path[600];
    snprintf(path, sizeof(path), "%s/%s.bmp", out_dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    const int w = HMI_SCREEN_W, h = HMI_SCREEN_H;
    const uint32_t row = (uint32_t)w * 3u, size = 54u + row * (uint32_t)h;
    uint8_t hdr[54] = {'B', 'M'};
    hdr[2] = (uint8_t)size;
    hdr[3] = (uint8_t)(size >> 8);
    hdr[4] = (uint8_t)(size >> 16);
    hdr[5] = (uint8_t)(size >> 24);
    hdr[10] = 54;
    hdr[14] = 40;
    hdr[18] = (uint8_t)w;
    hdr[19] = (uint8_t)(w >> 8);
    const int32_t neg_h = -h; /* linhas de cima para baixo */
    hdr[22] = (uint8_t)neg_h;
    hdr[23] = (uint8_t)(neg_h >> 8);
    hdr[24] = (uint8_t)(neg_h >> 16);
    hdr[25] = (uint8_t)(neg_h >> 24);
    hdr[26] = 1;
    hdr[28] = 24;
    fwrite(hdr, 1, sizeof(hdr), f);
    static uint8_t line[HMI_SCREEN_W * 3];
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const uint16_t p = framebuf[y * w + x];
            const uint8_t r = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
            const uint8_t g = (uint8_t)(((p >> 5) & 0x3F) * 255 / 63);
            const uint8_t b = (uint8_t)((p & 0x1F) * 255 / 31);
            line[x * 3 + 0] = b;
            line[x * 3 + 1] = g;
            line[x * 3 + 2] = r;
        }
        fwrite(line, 1, row, f);
    }
    fclose(f);
    return true;
}

int sim_run_headless(const char *scenario, const char *out_dir, const char *data_dir)
{
    MKDIR(out_dir);
    char dados[500];
    if (data_dir) {
        snprintf(dados, sizeof(dados), "%s", data_dir);
    } else {
        snprintf(dados, sizeof(dados), "%s/dados", out_dir);
    }
    lv_init();
    lv_tick_set_cb(sim_tick);
    disp = lv_display_create(HMI_SCREEN_W, HMI_SCREEN_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, framebuf, NULL, sizeof(framebuf), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, finger_read);

    sim_init(dados, sim_ms, true); /* comeca com a "flash" vazia: resultado repetivel */
    hmi_ui_init(&g_sim.link, &g_sim.platform);
    return sim_scenarios(scenario, out_dir);
}
