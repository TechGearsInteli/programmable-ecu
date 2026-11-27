/**
 * @file ecu_protocol.h
 * Protocolo UART entre o ESP do motor e a tela ESP32-S3 (task 1.6.1.1, #145).
 *
 * Linha: 115200 bps, 8N1, 3,3 V. So TX, RX e GND passam entre as placas.
 *
 * Frame (todos os campos de 16/32 bits em little-endian):
 *
 *   +------+-----+-----+------+----------------+-------------+
 *   | 0xA5 | LEN | SEQ | TYPE | PAYLOAD (LEN)  | CRC16 lo hi |
 *   +------+-----+-----+------+----------------+-------------+
 *
 *   - 0xA5: inicio do frame (SOF). O receptor descarta bytes ate achar um.
 *   - LEN: tamanho do payload, 0 a 240.
 *   - SEQ: numero do pedido. A resposta repete o SEQ do pedido.
 *   - TYPE: tipo da mensagem (enum ecu_msg_type_t).
 *   - CRC16: CRC-16/CCITT-FALSE de LEN, SEQ, TYPE e PAYLOAD.
 *
 * Regra de tipos: resposta = pedido | 0x80. A telemetria (0x10) nao tem
 * resposta e o NACK (0x7F) avisa que um pedido nao foi entendido.
 *
 * O ESP do motor e a autoridade: ele confere todo valor recebido contra os
 * limites e so entao passa a usar o numero. A tela so mostra e pede.
 */
#ifndef ECU_PROTOCOL_H
#define ECU_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ECU_PROTO_VERSION 1u
#define ECU_UART_BAUD 115200u
#define ECU_SOF 0xA5u
#define ECU_MAX_PAYLOAD 240u
#define ECU_FRAME_OVERHEAD 6u /* SOF + LEN + SEQ + TYPE + CRC16 */
#define ECU_MAX_FRAME (ECU_MAX_PAYLOAD + ECU_FRAME_OVERHEAD)

#define ECU_TELEMETRY_PERIOD_MS 50u /* 20 Hz */
#define ECU_RESPONSE_TIMEOUT_MS 200u
#define ECU_MAX_RETRIES 3u
#define ECU_LINK_ALIVE_MS 500u     /* sem telemetria por mais que isso = "sem dados" */
#define ECU_BACKUP_CHUNK 200u      /* bytes por bloco de backup */
#define ECU_IDLE_FLUSH_MS 5u       /* linha parada por isso = descarta frame incompleto */
#define ECU_MAX_AXIS 16u           /* maior eixo de tabela */

/** Tipos de mensagem. Pedidos vao da tela para o motor; respostas voltam. */
typedef enum {
    ECU_MSG_PING = 0x01,              /* -> PONG */
    ECU_MSG_TELEMETRY = 0x10,         /* motor -> tela, 20 Hz, sem resposta */
    ECU_MSG_TABLE_INFO_REQ = 0x20,    /* dimensoes, eixos, unidade e limites de uma tabela */
    ECU_MSG_CELL_READ_REQ = 0x21,     /* ler uma celula */
    ECU_MSG_ROW_READ_REQ = 0x22,      /* ler uma linha inteira (carregar a grade) */
    ECU_MSG_CELL_WRITE_REQ = 0x23,    /* escrever uma celula (o motor confirma ou recusa) */
    ECU_MSG_PARAM_READ_REQ = 0x30,    /* ler um item da ficha do motor */
    ECU_MSG_PARAM_WRITE_REQ = 0x31,   /* escrever um item da ficha (so com motor parado) */
    ECU_MSG_SAVE_REQ = 0x40,          /* gravar a calibracao da RAM na flash */
    ECU_MSG_BACKUP_READ_REQ = 0x50,   /* ler um bloco do backup (exportar) */
    ECU_MSG_RESTORE_BEGIN_REQ = 0x51, /* comecar a importar um backup */
    ECU_MSG_RESTORE_DATA_REQ = 0x52,  /* um bloco do backup importado */
    ECU_MSG_RESTORE_COMMIT_REQ = 0x53,/* validar e aplicar o backup importado */
    ECU_MSG_NACK = 0x7F,              /* pedido nao entendido */

    ECU_MSG_PONG = 0x81,
    ECU_MSG_TABLE_INFO = 0xA0,
    ECU_MSG_CELL_DATA = 0xA1,
    ECU_MSG_ROW_DATA = 0xA2,
    ECU_MSG_CELL_WRITE_ACK = 0xA3,
    ECU_MSG_PARAM_DATA = 0xB0,
    ECU_MSG_PARAM_WRITE_ACK = 0xB1,
    ECU_MSG_SAVE_ACK = 0xC0,
    ECU_MSG_BACKUP_DATA = 0xD0,
    ECU_MSG_RESTORE_BEGIN_ACK = 0xD1,
    ECU_MSG_RESTORE_DATA_ACK = 0xD2,
    ECU_MSG_RESTORE_COMMIT_ACK = 0xD3,
} ecu_msg_type_t;

#define ECU_RESPONSE_OF(req) ((uint8_t)((req) | 0x80u))

/** Resultado de um pedido, devolvido nas respostas. */
typedef enum {
    ECU_OK = 0,
    ECU_ERR_RANGE = 1,          /* valor fora do limite: o motor recusou e manteve o anterior */
    ECU_ERR_INDEX = 2,          /* linha/coluna/offset invalido */
    ECU_ERR_TABLE = 3,          /* tabela inexistente */
    ECU_ERR_ENGINE_RUNNING = 4, /* exige motor parado (rpm = 0) */
    ECU_ERR_FLASH = 5,          /* falha ao gravar na flash */
    ECU_ERR_BACKUP = 6,         /* backup invalido (versao, tamanho, CRC ou limite) */
    ECU_ERR_PARAM = 7,          /* item da ficha inexistente ou combinacao invalida */
} ecu_status_t;

/** Texto curto do status, em portugues, para tela e log. */
const char *ecu_status_text(uint8_t status);

typedef enum {
    ECU_NACK_UNKNOWN_TYPE = 1,
    ECU_NACK_BAD_LENGTH = 2,
} ecu_nack_reason_t;

/** Unidades dos valores e dos eixos (a escala esta no nome). */
typedef enum {
    ECU_UNIT_NONE = 0,
    ECU_UNIT_RPM = 1,
    ECU_UNIT_KPA = 2,
    ECU_UNIT_MS_X100 = 3,   /* milissegundos x100 (850 = 8,50 ms) */
    ECU_UNIT_DEG_X10 = 4,   /* graus x10 (185 = 18,5 graus) */
    ECU_UNIT_PCT_X10 = 5,   /* porcento x10 (+125 = +12,5 %) */
    ECU_UNIT_US = 6,        /* microssegundos */
    ECU_UNIT_DEGC = 7,      /* graus Celsius */
    ECU_UNIT_DEGC_X10 = 8,  /* graus Celsius x10 */
    ECU_UNIT_VOLT_X10 = 9,  /* volts x10 (138 = 13,8 V) */
    ECU_UNIT_PCT_PER_S = 10,/* porcento de TPS por segundo */
    ECU_UNIT_CC_MIN = 11,   /* vazao do bico em cc/min */
    ECU_UNIT_MS_X10 = 12,   /* milissegundos x10 (30 = 3,0 ms) */
    ECU_UNIT_COUNT = 13,    /* quantidade (cilindros, dentes) */
} ecu_unit_t;

/* ------------------------------------------------------------------------ */
/* Frame e parser                                                            */
/* ------------------------------------------------------------------------ */

typedef struct {
    uint8_t seq;
    uint8_t type;
    uint8_t len;
    uint8_t payload[ECU_MAX_PAYLOAD];
} ecu_frame_t;

/**
 * Monta um frame completo em @p out.
 * @return quantos bytes escreveu, ou 0 se nao coube / payload grande demais.
 */
size_t ecu_frame_encode(uint8_t *out, size_t out_size, uint8_t seq, uint8_t type,
                        const uint8_t *payload, uint8_t len);

typedef void (*ecu_frame_cb)(void *ctx, const ecu_frame_t *frame);

/**
 * Parser de bytes da UART. Guarda bytes ate formar um frame valido.
 * Se o tamanho ou o CRC nao fecharem, descarta so o primeiro byte e procura o
 * proximo 0xA5 dentro do que ja recebeu (ressincroniza sem perder o frame bom
 * que vier logo depois de lixo).
 */
typedef struct {
    uint8_t buf[ECU_MAX_FRAME];
    uint16_t pos;
    uint32_t frames_ok;
    uint32_t crc_errors;
    uint32_t len_errors;
    ecu_frame_t frame;
} ecu_parser_t;

void ecu_parser_init(ecu_parser_t *p);
void ecu_parser_feed(ecu_parser_t *p, const uint8_t *data, size_t len, ecu_frame_cb cb, void *ctx);
/**
 * Chame quando a linha ficar parada (ex.: 5 ms sem bytes). Um 0xA5 de lixo
 * seguido de um LEN grande faria o parser esperar ate 246 bytes; com a linha
 * parada sabemos que o frame nao vai completar e ressincronizamos na hora.
 */
void ecu_parser_idle(ecu_parser_t *p, ecu_frame_cb cb, void *ctx);

/* ------------------------------------------------------------------------ */
/* Mensagens                                                                 */
/* ------------------------------------------------------------------------ */

/* Bits de ecu_telemetry_t.flags */
#define ECU_TLM_SYNC 0x01u          /* motor sincronizado com a roda fonica */
#define ECU_TLM_CRANKING 0x02u      /* em partida (rpm abaixo do rpm de partida) */
#define ECU_TLM_RUNNING 0x04u       /* rpm > 0 */
#define ECU_TLM_CALIB_DEFAULT 0x08u /* a flash estava vazia/invalida: usando o mapa padrao */
#define ECU_TLM_CALIB_DIRTY 0x10u   /* ha alteracoes na RAM que ainda nao foram gravadas */

/** Telemetria (motor -> tela, 20 Hz). 44 bytes de payload. */
typedef struct {
    uint16_t rpm;
    uint8_t flags;
    uint8_t tps_pct;
    uint16_t pw_us;            /* tempo de injecao comandado */
    int16_t advance_x10;       /* avanco de ignicao */
    uint16_t dwell_us;
    uint16_t map_kpa_x10;      /* carga */
    int16_t clt_x10;           /* temperatura do motor */
    int16_t iat_x10;           /* temperatura do ar */
    uint16_t vbat_mv;
    uint8_t fuel_row, fuel_col;/* celula de combustivel mais proxima */
    int16_t fuel_cell;         /* valor dessa celula (ms x100) */
    uint16_t fuel_interp_x100; /* valor interpolado usado no calculo (ms x100) */
    uint8_t ign_row, ign_col;  /* celula de ignicao mais proxima */
    int16_t ign_cell;          /* graus x10 */
    int16_t corr_warm_x10;     /* correcoes valendo agora, em % x10 */
    int16_t corr_iat_x10;
    int16_t corr_crank_x10;
    int16_t corr_accel_x10;
    uint16_t dead_time_us;     /* tempo morto do bico pela tensao da bateria */
    uint16_t inj_scale_x1000;  /* vazao_ref / vazao_bico x1000 */
    uint32_t uptime_ms;
} ecu_telemetry_t;

#define ECU_TELEMETRY_LEN 44u

typedef struct {
    uint8_t proto_version;
    uint8_t node;          /* 1 = ESP do motor, 2 = motor virtual do simulador */
    uint8_t calib_version;
    uint8_t flags;         /* mesmos bits de calibracao da telemetria */
} ecu_pong_t;

/** Descricao de uma tabela (resposta de TABLE_INFO_REQ). */
typedef struct {
    uint8_t table;
    uint8_t rows, cols;
    uint8_t value_unit, x_unit, y_unit;
    int16_t min, max, step; /* limites que o motor aceita e passo sugerido */
    int16_t x[ECU_MAX_AXIS];/* eixo das colunas (cols valores) */
    int16_t y[ECU_MAX_AXIS];/* eixo das linhas (rows valores; vazio se rows = 1) */
} ecu_table_info_t;

/** Uma celula: pedido de leitura/escrita e as respostas. */
typedef struct {
    uint8_t table, row, col;
    uint8_t status; /* so nas respostas */
    int16_t value;  /* no pedido de escrita: valor desejado; na resposta: valor que esta valendo */
} ecu_cell_t;

/** Uma linha de tabela (resposta de ROW_READ_REQ). */
typedef struct {
    uint8_t table, row, status, count;
    int16_t values[ECU_MAX_AXIS];
} ecu_row_t;

/** Item da ficha do motor. */
typedef struct {
    uint8_t param;
    uint8_t status;
    uint8_t unit;
    int16_t value, min, max, step;
} ecu_param_t;

typedef struct {
    uint8_t status;
    uint32_t crc32; /* CRC do que ficou gravado */
} ecu_save_ack_t;

/** Bloco de backup (exportar: BACKUP_DATA; importar: RESTORE_DATA_REQ). */
typedef struct {
    uint8_t status;
    uint16_t offset, total;
    uint8_t count;
    uint8_t data[ECU_BACKUP_CHUNK];
} ecu_chunk_t;

typedef struct {
    uint16_t total;
    uint32_t crc32;
} ecu_restore_begin_t;

typedef struct {
    uint8_t status;
    uint16_t received;
} ecu_restore_ack_t;

typedef struct {
    uint8_t reason;
    uint8_t rejected_type;
} ecu_nack_t;

/* Cada pack devolve o tamanho do payload escrito; cada unpack devolve false
 * se o tamanho nao bater com o esperado. */
uint8_t ecu_pack_telemetry(const ecu_telemetry_t *m, uint8_t *out);
bool ecu_unpack_telemetry(const uint8_t *in, uint8_t len, ecu_telemetry_t *m);
uint8_t ecu_pack_pong(const ecu_pong_t *m, uint8_t *out);
bool ecu_unpack_pong(const uint8_t *in, uint8_t len, ecu_pong_t *m);
uint8_t ecu_pack_table_info(const ecu_table_info_t *m, uint8_t *out);
bool ecu_unpack_table_info(const uint8_t *in, uint8_t len, ecu_table_info_t *m);

/* Pedidos e respostas de celula compartilham o formato:
 * pedido de leitura: table,row,col (3 bytes)
 * pedido de escrita: table,row,col,value (5 bytes)
 * respostas: table,row,col,status,value (6 bytes) */
uint8_t ecu_pack_cell_read_req(const ecu_cell_t *m, uint8_t *out);
uint8_t ecu_pack_cell_write_req(const ecu_cell_t *m, uint8_t *out);
uint8_t ecu_pack_cell_resp(const ecu_cell_t *m, uint8_t *out);
bool ecu_unpack_cell_read_req(const uint8_t *in, uint8_t len, ecu_cell_t *m);
bool ecu_unpack_cell_write_req(const uint8_t *in, uint8_t len, ecu_cell_t *m);
bool ecu_unpack_cell_resp(const uint8_t *in, uint8_t len, ecu_cell_t *m);

/* Linha: pedido table,row (2 bytes); resposta table,row,status,count,values */
uint8_t ecu_pack_row_req(uint8_t table, uint8_t row, uint8_t *out);
bool ecu_unpack_row_req(const uint8_t *in, uint8_t len, uint8_t *table, uint8_t *row);
uint8_t ecu_pack_row(const ecu_row_t *m, uint8_t *out);
bool ecu_unpack_row(const uint8_t *in, uint8_t len, ecu_row_t *m);

/* Ficha: leitura param (1 byte); escrita param,value (3 bytes);
 * respostas: param,status,unit,value,min,max,step (11 bytes) */
uint8_t ecu_pack_param_write_req(uint8_t param, int16_t value, uint8_t *out);
bool ecu_unpack_param_write_req(const uint8_t *in, uint8_t len, uint8_t *param, int16_t *value);
uint8_t ecu_pack_param(const ecu_param_t *m, uint8_t *out);
bool ecu_unpack_param(const uint8_t *in, uint8_t len, ecu_param_t *m);

uint8_t ecu_pack_save_ack(const ecu_save_ack_t *m, uint8_t *out);
bool ecu_unpack_save_ack(const uint8_t *in, uint8_t len, ecu_save_ack_t *m);

/* Backup: BACKUP_READ_REQ offset (2 bytes);
 * BACKUP_DATA e RESTORE_DATA_REQ: status,offset,total,count,data */
uint8_t ecu_pack_u16(uint16_t v, uint8_t *out);
bool ecu_unpack_u16(const uint8_t *in, uint8_t len, uint16_t *v);
uint8_t ecu_pack_chunk(const ecu_chunk_t *m, uint8_t *out);
bool ecu_unpack_chunk(const uint8_t *in, uint8_t len, ecu_chunk_t *m);
uint8_t ecu_pack_restore_begin(const ecu_restore_begin_t *m, uint8_t *out);
bool ecu_unpack_restore_begin(const uint8_t *in, uint8_t len, ecu_restore_begin_t *m);
uint8_t ecu_pack_restore_ack(const ecu_restore_ack_t *m, uint8_t *out);
bool ecu_unpack_restore_ack(const uint8_t *in, uint8_t len, ecu_restore_ack_t *m);

uint8_t ecu_pack_nack(const ecu_nack_t *m, uint8_t *out);
bool ecu_unpack_nack(const uint8_t *in, uint8_t len, ecu_nack_t *m);

#ifdef __cplusplus
}
#endif

#endif /* ECU_PROTOCOL_H */
