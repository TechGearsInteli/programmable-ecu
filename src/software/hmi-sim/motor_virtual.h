/**
 * Motor virtual do simulador: faz o papel do ESP do motor no PC.
 *
 * Usa o MESMO nucleo do firmware (ecu_engine + ecu_motor_link) e fala com a
 * tela pelo MESMO protocolo em bytes. A "flash" e um arquivo no disco.
 * Os sensores vem de um ciclo automatico (marcha lenta, aceleracao, corte,
 * cruzeiro, com o motor esquentando) ou sao fixados pelos cenarios de teste.
 */
#ifndef MOTOR_VIRTUAL_H
#define MOTOR_VIRTUAL_H

#include <stdbool.h>
#include <stdint.h>

#include "ecu/ecu_engine.h"
#include "ecu/ecu_motor_link.h"

typedef struct {
    ecu_calib_t calib;
    ecu_engine_state_t est;
    ecu_inputs_t in;
    ecu_outputs_t out;
    ecu_motor_link_t link;
    ecu_storage_t storage;
    char flash_path[512];
    bool manual;
    uint32_t start_ms;
    uint32_t last_tlm_ms;
} motor_virtual_t;

void motor_virtual_init(motor_virtual_t *m, const char *flash_path, ecu_send_fn send, void *ctx, uint32_t now_ms);
/** Reinicia como se o ESP tivesse sido desligado e ligado (le a "flash" de novo). */
void motor_virtual_reboot(motor_virtual_t *m, uint32_t now_ms);
void motor_virtual_feed(motor_virtual_t *m, const uint8_t *data, size_t len);
void motor_virtual_step(motor_virtual_t *m, uint32_t now_ms);
/** Fixa os sensores (os cenarios usam). */
void motor_virtual_set(motor_virtual_t *m, uint16_t rpm, uint16_t map_kpa_x10, uint8_t tps, int16_t clt_x10);
void motor_virtual_auto(motor_virtual_t *m, bool on);

#endif
