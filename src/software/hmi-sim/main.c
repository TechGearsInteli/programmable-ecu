/*
 * hmi_sim: a tela da ECU no computador.
 *
 *   hmi_sim                       abre a janela (Windows) com o motor virtual em ciclo automatico
 *   hmi_sim --headless --cenario todos --saida PASTA
 *                                 roda os cenarios sem janela, confere a tela e salva capturas BMP
 *   --dados PASTA                 onde ficam a "flash" do motor virtual e o backup (padrao: hmi-sim-dados)
 */
#include <stdio.h>
#include <string.h>

#include "sim.h"

#if defined(_WIN32) && LV_USE_WINDOWS
#include <windows.h>

static uint32_t win_tick(void)
{
    return (uint32_t)GetTickCount();
}

static int run_window(const char *data_dir)
{
    lv_init();
    lv_tick_set_cb(win_tick);
    lv_display_t *disp = lv_windows_create_display(L"TechGears ECU - simulador da tela S3 (800x480)", HMI_SCREEN_W,
                                                   HMI_SCREEN_H, 100, false, true);
    if (!disp) {
        fprintf(stderr, "nao foi possivel abrir a janela\n");
        return 1;
    }
    lv_windows_acquire_pointer_indev(disp);
    sim_init(data_dir, win_tick(), false);
    lv_lock();
    hmi_ui_init(&g_sim.link, &g_sim.platform);
    lv_unlock();
    HWND hwnd = lv_windows_get_display_window_handle(disp);
    printf("Simulador aberto. O motor virtual repete um ciclo de 40 s (partida, lenta, aceleracao,\n"
           "corte e cruzeiro). Feche a janela para sair.\n");
    while (IsWindow(hwnd)) {
        sim_pump(win_tick());
        uint32_t next = lv_timer_handler();
        Sleep(next > 5 ? 5 : next);
    }
    return 0;
}
#endif

int main(int argc, char **argv)
{
    bool headless = false;
    const char *scenario = "todos";
    const char *out_dir = "capturas";
    const char *data_dir = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--headless")) {
            headless = true;
        } else if (!strcmp(argv[i], "--cenario") && i + 1 < argc) {
            scenario = argv[++i];
        } else if (!strcmp(argv[i], "--saida") && i + 1 < argc) {
            out_dir = argv[++i];
        } else if (!strcmp(argv[i], "--dados") && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("uso: hmi_sim [--headless --cenario todos --saida PASTA] [--dados PASTA]\n");
            return 0;
        }
    }
    if (headless) {
        return sim_run_headless(scenario, out_dir, data_dir);
    }
#if defined(_WIN32) && LV_USE_WINDOWS
    return run_window(data_dir ? data_dir : "hmi-sim-dados");
#else
    fprintf(stderr, "Janela so no Windows por enquanto. Use --headless para gerar as capturas.\n");
    return 1;
#endif
}
