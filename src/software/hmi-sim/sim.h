/**
 * Simulador da HMI no PC (task 1.7.1.1).
 *
 *   tela (LVGL + ecu_hmi_link)  <-- bytes do protocolo -->  motor virtual (ecu_motor_link)
 *
 * Os dois lados trocam bytes por dois "fios" em memoria, como se fosse a UART.
 */
#ifndef SIM_H
#define SIM_H

#include <stdbool.h>
#include <stdint.h>

#include "ecu/ecu_hmi_link.h"
#include "hmi/ui.h"
#include "motor_virtual.h"

typedef struct {
    uint8_t buf[16384];
    size_t len;
} sim_wire_t;

typedef struct {
    sim_wire_t to_motor, to_hmi;
    ecu_hmi_link_t link;
    motor_virtual_t motor;
    hmi_platform_t platform;
    char data_dir[400];
    char backup_path[512];
    uint32_t now;
} sim_t;

extern sim_t g_sim;

void sim_init(const char *data_dir, uint32_t now_ms, bool fresh);
/** Avanca a simulacao ate @p now_ms: motor calcula, bytes passam pelos fios, tela le. */
void sim_pump(uint32_t now_ms);

/* headless.c */
int sim_run_headless(const char *scenario, const char *out_dir, const char *data_dir);

/* scenarios.c */
int sim_scenarios(const char *which, const char *out_dir);

/* helpers usados pelos cenarios (headless.c) */
void sim_run_ms(uint32_t ms);
void sim_tap_obj(lv_obj_t *obj);
void sim_tap_table_cell(lv_obj_t *table, uint32_t vrow, uint32_t vcol);
bool sim_snapshot(const char *out_dir, const char *name);

#endif
