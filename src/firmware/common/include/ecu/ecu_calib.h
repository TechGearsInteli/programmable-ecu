/**
 * @file ecu_calib.h
 * Calibracao da ECU: mapas 3D, curvas de correcao 2D e ficha do motor.
 *
 * Modelo FuelTech: as tabelas oficiais moram na RAM do ESP do motor. A tela
 * so le e pede alteracoes. Cada tabela tem limites; um valor fora do limite e
 * recusado e o valor anterior continua valendo.
 *
 * Tabelas (rows = linhas, cols = colunas):
 *   - Combustivel 16x16: tempo de injecao base em ms x100, RPM (colunas) por
 *     MAP em kPa (linhas). Vale para o bico de referencia da ficha.
 *   - Ignicao 16x16: avanco em graus x10, mesmos eixos.
 *   - Cinco curvas 2D de 8 pontos (1 linha): aquecimento (CLT), ar (IAT),
 *     partida (CLT), tempo morto do bico (tensao da bateria) e aceleracao
 *     (velocidade do TPS). As correcoes em % usam 0 como "sem correcao".
 */
#ifndef ECU_CALIB_H
#define ECU_CALIB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ecu/ecu_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECU_MAP_ROWS 16u
#define ECU_MAP_COLS 16u
#define ECU_CURVE_POINTS 8u

/* Fit 1.5 L15A (EX 2006). A lenta vem do manual Honda. O corte vem de notas
 * Haltech para esse motor: forte, mas nao e o manual. A faixa vermelha do
 * painel e o fundo da escala esperam uma foto do conta-giros; nao usar o
 * corte como vermelho nem como fim da barra. */
#define ECU_FIT_IDLE_RPM 750
#define ECU_FIT_REV_CUT_RPM 6200

typedef enum {
    ECU_TBL_FUEL = 0,
    ECU_TBL_IGN = 1,
    ECU_TBL_WARMUP = 2,   /* aquecimento: % x10 por CLT */
    ECU_TBL_IAT = 3,      /* temperatura do ar: % x10 por IAT */
    ECU_TBL_CRANK = 4,    /* partida: % x10 por CLT, so com rpm < rpm de partida */
    ECU_TBL_DEADTIME = 5, /* bateria: tempo morto do bico em us por tensao */
    ECU_TBL_ACCEL = 6,    /* aceleracao: % x10 pela velocidade do TPS (%/s) */
    ECU_TBL_COUNT
} ecu_table_id_t;

typedef enum {
    ECU_PAR_CYLINDERS = 0,     /* define a ordem de ignicao e o espacamento */
    ECU_PAR_TRIGGER_TEETH,     /* dentes da roda fonica (contando os faltantes) */
    ECU_PAR_TRIGGER_MISSING,   /* dentes faltantes (36-1 -> 1) */
    ECU_PAR_INJ_FLOW,          /* vazao do bico instalado, cc/min */
    ECU_PAR_INJ_FLOW_REF,      /* vazao do bico com que o mapa foi feito, cc/min */
    ECU_PAR_DWELL_X10,         /* tempo de carga da bobina, ms x10 */
    ECU_PAR_CRANK_RPM,         /* abaixo disso o motor esta "em partida" */
    ECU_PAR_ALERT_RPM,         /* alerta de rotacao na tela */
    ECU_PAR_ALERT_CLT_X10,     /* alerta de temperatura do motor na tela */
    ECU_PAR_COUNT
} ecu_param_id_t;

/** Uma tabela. Curvas usam so a linha 0 e o eixo x. */
typedef struct {
    int16_t x[ECU_MAX_AXIS];
    int16_t y[ECU_MAX_AXIS];
    int16_t v[ECU_MAP_ROWS][ECU_MAP_COLS];
} ecu_map_t;

typedef struct {
    ecu_map_t table[ECU_TBL_COUNT];
    int16_t param[ECU_PAR_COUNT];
} ecu_calib_t;

typedef struct {
    const char *name;
    uint8_t rows, cols;
    uint8_t value_unit, x_unit, y_unit;
    int16_t min, max, step;
} ecu_table_desc_t;

typedef struct {
    const char *name;
    uint8_t unit;
    int16_t min, max, step;
} ecu_param_desc_t;

const ecu_table_desc_t *ecu_table_desc(uint8_t table); /* NULL se nao existe */
const ecu_param_desc_t *ecu_param_desc(uint8_t param); /* NULL se nao existe */

/** Calibracao de fabrica: ponto de partida seguro para a bancada. */
void ecu_calib_defaults(ecu_calib_t *c);

/** Confere uma celula sem alterar nada. */
uint8_t ecu_calib_check_cell(uint8_t table, uint8_t row, uint8_t col, int16_t value);
/** Altera uma celula se passar na conferencia. Devolve ECU_OK ou o erro. */
uint8_t ecu_calib_set_cell(ecu_calib_t *c, uint8_t table, uint8_t row, uint8_t col, int16_t value);
/** Confere um item da ficha, inclusive combinacoes (faltantes < dentes...). */
uint8_t ecu_calib_check_param(const ecu_calib_t *c, uint8_t param, int16_t value);
uint8_t ecu_calib_set_param(ecu_calib_t *c, uint8_t param, int16_t value);
/** Confere a calibracao inteira: limites, eixos crescentes e ficha. */
bool ecu_calib_validate(const ecu_calib_t *c);

/** Preenche a descricao que vai na mensagem TABLE_INFO. */
void ecu_calib_table_info(const ecu_calib_t *c, uint8_t table, ecu_table_info_t *info);

/* ---------------- Gravacao (flash) e backup ----------------
 * Blob = cabecalho de 12 bytes + valores int16 little-endian.
 *   magic "TGCL" (u32) | versao (u16) | tamanho do conteudo (u16) | CRC-32 do conteudo (u32)
 * Conteudo, na ordem: para cada tabela, eixo x, eixo y (se tiver mais de
 * uma linha) e valores linha por linha; depois os itens da ficha. */
#define ECU_CALIB_MAGIC 0x4C434754u /* bytes 'T' 'G' 'C' 'L' */
#define ECU_CALIB_VERSION 1u
#define ECU_CALIB_HEADER 12u
#define ECU_CALIB_BLOB_MAX 1400u

size_t ecu_calib_blob_size(void);
size_t ecu_calib_serialize(const ecu_calib_t *c, uint8_t *out, size_t cap);
/** Le e valida um blob (magic, versao, tamanho, CRC e limites). Se falhar, @p out nao muda. */
uint8_t ecu_calib_deserialize(ecu_calib_t *out, const uint8_t *in, size_t len);
/** CRC-32 do conteudo, sem precisar de buffer: compara RAM com flash. */
uint32_t ecu_calib_crc(const ecu_calib_t *c);

/** Onde a calibracao e gravada: NVS no ESP32, arquivo no simulador. */
typedef struct {
    bool (*write)(void *ctx, const uint8_t *data, size_t len);
    size_t (*read)(void *ctx, uint8_t *buf, size_t cap);
    void *ctx;
} ecu_storage_t;

/** Carrega da flash. Se nao houver ou estiver invalida, usa o padrao e devolve false. */
bool ecu_calib_load(ecu_calib_t *c, const ecu_storage_t *storage);
uint8_t ecu_calib_save(const ecu_calib_t *c, const ecu_storage_t *storage, uint32_t *crc_out);

#ifdef __cplusplus
}
#endif

#endif /* ECU_CALIB_H */
