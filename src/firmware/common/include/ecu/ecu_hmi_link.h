/**
 * @file ecu_hmi_link.h
 * Lado da tela (ESP32-S3) no protocolo.
 *
 * Mantem uma COPIA das tabelas e da ficha que estao no ESP do motor, so para
 * desenhar. A copia so muda quando o motor responde: a tela nunca "decide"
 * um valor. Os pedidos saem um de cada vez; se a resposta nao chega em 200 ms
 * o pedido e reenviado (ate 3 vezes) com o mesmo SEQ.
 *
 * Nao conhece LVGL nem UART: recebe bytes por feed(), manda bytes pela funcao
 * send e avisa a interface por um callback de eventos.
 */
#ifndef ECU_HMI_LINK_H
#define ECU_HMI_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ecu/ecu_calib.h"
#include "ecu/ecu_motor_link.h"
#include "ecu/ecu_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECU_HMI_QUEUE 8u

typedef struct {
    bool info_ok;
    ecu_table_info_t info;
    uint16_t rows_ok;  /* um bit por linha ja recebida */
    int16_t v[ECU_MAP_ROWS][ECU_MAP_COLS];
} ecu_hmi_table_t;

typedef struct {
    bool ok;
    ecu_param_t p;
} ecu_hmi_param_t;

typedef enum {
    ECU_HMI_EV_CONNECTED = 1,   /* o motor respondeu ao PING */
    ECU_HMI_EV_SYNCED,          /* todas as tabelas e a ficha foram carregadas */
    ECU_HMI_EV_CELL_ACK,        /* resposta de escrita de celula (status diz se aceitou) */
    ECU_HMI_EV_PARAM_ACK,
    ECU_HMI_EV_SAVE_ACK,
    ECU_HMI_EV_BACKUP_DONE,     /* backup exportado inteiro em link->blob */
    ECU_HMI_EV_RESTORE_DONE,    /* backup importado (status diz se o motor aceitou) */
    ECU_HMI_EV_TIMEOUT,         /* o motor nao respondeu depois das tentativas */
} ecu_hmi_event_type_t;

typedef struct {
    uint8_t type;
    uint8_t status;
    uint8_t req_type;
    uint8_t table, row, col, param;
    int16_t value; /* valor que esta valendo no motor */
} ecu_hmi_event_t;

typedef void (*ecu_hmi_event_cb)(void *ctx, const ecu_hmi_event_t *ev);

typedef struct {
    uint8_t type;
    uint8_t len;
    uint8_t payload[ECU_BACKUP_CHUNK + 8];
} ecu_hmi_req_t;

typedef enum { ECU_JOB_NONE = 0, ECU_JOB_SYNC, ECU_JOB_BACKUP, ECU_JOB_RESTORE } ecu_hmi_job_t;

typedef struct {
    ecu_send_fn send;
    void *send_ctx;
    ecu_hmi_event_cb on_event;
    void *event_ctx;
    ecu_parser_t parser;
    uint32_t now_ms;

    /* estado do motor */
    bool connected;
    ecu_pong_t pong;
    bool tlm_ok;
    ecu_telemetry_t tlm;
    uint32_t tlm_ms;
    uint32_t tlm_count;
    ecu_hmi_table_t tables[ECU_TBL_COUNT];
    ecu_hmi_param_t params[ECU_PAR_COUNT];

    /* pedidos */
    ecu_hmi_req_t queue[ECU_HMI_QUEUE];
    uint8_t q_head, q_count;
    bool waiting;
    ecu_hmi_req_t current;
    uint8_t current_seq;
    uint8_t retries;
    uint32_t sent_ms;
    uint8_t next_seq;
    uint32_t last_ping_ms;

    /* trabalhos longos (um por vez): sincronizar, exportar, importar */
    uint8_t job;
    uint16_t job_step;
    uint8_t blob[ECU_CALIB_BLOB_MAX];
    uint16_t blob_len;
    uint16_t blob_total;

    uint32_t timeouts;
} ecu_hmi_link_t;

void ecu_hmi_link_init(ecu_hmi_link_t *l, ecu_send_fn send, void *send_ctx, ecu_hmi_event_cb on_event,
                       void *event_ctx, uint32_t now_ms);
void ecu_hmi_link_feed(ecu_hmi_link_t *l, const uint8_t *data, size_t len, uint32_t now_ms);
/** A UART ficou parada (ECU_IDLE_FLUSH_MS sem bytes): descarta frame incompleto. */
void ecu_hmi_link_line_idle(ecu_hmi_link_t *l);
/** Chame sempre (ex.: a cada 10 ms): reenvios, PING e proximo pedido. */
void ecu_hmi_link_poll(ecu_hmi_link_t *l, uint32_t now_ms);

bool ecu_hmi_link_alive(const ecu_hmi_link_t *l, uint32_t now_ms);
bool ecu_hmi_link_synced(const ecu_hmi_link_t *l);
bool ecu_hmi_link_busy(const ecu_hmi_link_t *l);
/** Progresso do trabalho longo atual, 0 a 100. */
uint8_t ecu_hmi_link_job_progress(const ecu_hmi_link_t *l);

/* Pedidos. Devolvem false se a fila esta cheia (ou o pedido e impossivel). */
bool ecu_hmi_link_resync(ecu_hmi_link_t *l);
bool ecu_hmi_link_write_cell(ecu_hmi_link_t *l, uint8_t table, uint8_t row, uint8_t col, int16_t value);
bool ecu_hmi_link_write_param(ecu_hmi_link_t *l, uint8_t param, int16_t value);
bool ecu_hmi_link_save(ecu_hmi_link_t *l);
bool ecu_hmi_link_backup(ecu_hmi_link_t *l);
bool ecu_hmi_link_restore(ecu_hmi_link_t *l, const uint8_t *blob, uint16_t len);

/**
 * Checagem previa so para a tela avisar ("fora do limite") antes de enviar.
 * Quem decide continua sendo o motor.
 */
uint8_t ecu_hmi_link_precheck_cell(const ecu_hmi_link_t *l, uint8_t table, int16_t value);

#ifdef __cplusplus
}
#endif

#endif /* ECU_HMI_LINK_H */
