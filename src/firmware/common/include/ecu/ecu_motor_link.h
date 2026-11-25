/**
 * @file ecu_motor_link.h
 * Lado do ESP do motor no protocolo: responde aos pedidos da tela e envia a
 * telemetria. E aqui que mora a regra "o motor e a autoridade":
 *
 *   - toda escrita e conferida contra os limites antes de valer;
 *   - mapas e curvas podem mudar com o motor girando (acerto ao vivo);
 *   - ficha do motor, gravacao na flash e importacao de backup so com rpm = 0;
 *   - nada vai para a flash sem o pedido SAVE.
 *
 * Rode o feed() numa task comum (nunca dentro da interrupcao do giro): a
 * gravacao na flash do ESP32 desliga o cache por alguns milissegundos.
 */
#ifndef ECU_MOTOR_LINK_H
#define ECU_MOTOR_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ecu/ecu_calib.h"
#include "ecu/ecu_engine.h"
#include "ecu/ecu_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECU_NODE_MOTOR 1u
#define ECU_NODE_VIRTUAL 2u

typedef void (*ecu_send_fn)(void *ctx, const uint8_t *data, size_t len);
typedef void (*ecu_lock_fn)(void *ctx);

typedef struct {
    ecu_calib_t *calib;             /* calibracao viva, lida pelo calculo do motor */
    const ecu_storage_t *storage;   /* flash */
    ecu_send_fn send;
    void *send_ctx;
    ecu_lock_fn lock, unlock;       /* opcionais: protegem a troca da calibracao inteira */
    void *lock_ctx;
    uint8_t node;

    ecu_parser_t parser;
    uint16_t rpm_now;               /* rotacao no momento do pedido */
    bool calib_default;             /* a flash nao tinha calibracao valida */
    uint32_t saved_crc;             /* CRC do que esta na flash */
    uint8_t tlm_seq;

    /* Repete a ultima resposta se o mesmo pedido chegar de novo (a tela
     * reenviou porque a resposta se perdeu): nada e aplicado duas vezes. */
    bool have_last;
    uint8_t last_seq, last_type;
    uint8_t last_resp[ECU_MAX_FRAME];
    size_t last_resp_len;

    /* Backup: foto da calibracao (exportar) ou area de recebimento (importar) */
    uint8_t blob[ECU_CALIB_BLOB_MAX];
    bool restore_active;
    uint16_t restore_total, restore_received;
    uint32_t restore_crc;
    ecu_calib_t staging;

    uint32_t requests, nacks;
} ecu_motor_link_t;

/** Inicia e carrega a calibracao da flash (ou o padrao, se nao houver). */
void ecu_motor_link_init(ecu_motor_link_t *l, ecu_calib_t *calib, const ecu_storage_t *storage,
                         ecu_send_fn send, void *send_ctx, uint8_t node);

/** Bytes que chegaram da UART. @p rpm_now e a rotacao atual (regras de motor parado). */
void ecu_motor_link_feed(ecu_motor_link_t *l, const uint8_t *data, size_t len, uint16_t rpm_now);

/** A UART ficou parada (ECU_IDLE_FLUSH_MS sem bytes): descarta frame incompleto. */
void ecu_motor_link_line_idle(ecu_motor_link_t *l);

/** Monta e envia uma telemetria (chame a cada ECU_TELEMETRY_PERIOD_MS). */
void ecu_motor_link_send_telemetry(ecu_motor_link_t *l, const ecu_inputs_t *in, const ecu_outputs_t *out,
                                   uint32_t uptime_ms);

/** Ha alteracoes na RAM que nao estao na flash? */
bool ecu_motor_link_dirty(const ecu_motor_link_t *l);

/** Preenche a telemetria a partir das entradas e saidas do calculo. */
void ecu_telemetry_fill(ecu_telemetry_t *t, const ecu_inputs_t *in, const ecu_outputs_t *out, uint8_t flags,
                        uint32_t uptime_ms);

#ifdef __cplusplus
}
#endif

#endif /* ECU_MOTOR_LINK_H */
