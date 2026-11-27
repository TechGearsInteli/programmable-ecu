/**
 * @file ecu_engine.h
 * Calculo do motor a partir dos sensores e da calibracao.
 *
 *   PW = celula_interpolada x (vazao_ref / vazao_bico)
 *          x (1 + aquecimento) x (1 + ar) x (1 + partida) x (1 + aceleracao)
 *        + tempo_morto(tensao da bateria)
 *
 * Com todas as correcoes em zero, vazoes iguais e tempo morto zero, o PW e
 * exatamente o valor da celula (criterio da task 1.6.1.6).
 *
 * Este codigo nao agenda o pulso no tempo: ele so calcula quanto e quando. O
 * agendamento (timers, roda fonica, saidas) e do firmware do motor (1.5).
 * Pode rodar no laco do motor; nao aloca memoria e nao grava flash.
 */
#ifndef ECU_ENGINE_H
#define ECU_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "ecu/ecu_calib.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Tempo que o enriquecimento de aceleracao fica valendo depois do pico. */
#define ECU_ACCEL_HOLD_MS 300u

typedef struct {
    uint16_t rpm;
    uint16_t map_kpa_x10;
    uint8_t tps_pct;
    int16_t clt_x10;
    int16_t iat_x10;
    uint16_t vbat_mv;
    bool sync;
} ecu_inputs_t;

/** Memoria entre chamadas (so a aceleracao precisa). */
typedef struct {
    bool has_last;
    uint8_t last_tps;
    uint32_t last_ms;
    int16_t accel_x10;
    uint32_t accel_until_ms;
} ecu_engine_state_t;

typedef struct {
    uint16_t pw_us;
    int16_t advance_x10;
    uint16_t dwell_us;
    bool cranking;
    uint8_t fuel_row, fuel_col;
    int16_t fuel_cell;
    uint16_t fuel_interp_x100;
    uint8_t ign_row, ign_col;
    int16_t ign_cell;
    int16_t corr_warm_x10;
    int16_t corr_iat_x10;
    int16_t corr_crank_x10;
    int16_t corr_accel_x10;
    uint16_t dead_time_us;
    uint16_t inj_scale_x1000;
} ecu_outputs_t;

void ecu_engine_init(ecu_engine_state_t *s);
void ecu_engine_compute(const ecu_calib_t *c, const ecu_inputs_t *in, ecu_engine_state_t *s,
                        uint32_t now_ms, ecu_outputs_t *out);

/**
 * Interpolacao linear numa curva. O eixo esta em unidades inteiras e a
 * consulta @p q em unidades x @p scale (ex.: eixo em graus C, consulta em
 * graus C x10 -> scale = 10). Fora do eixo, usa o valor da ponta.
 */
int32_t ecu_interp1d(const int16_t *axis, uint8_t n, int32_t scale, const int16_t *values, int32_t q);

/** Interpolacao bilinear numa tabela (colunas = x, linhas = y). */
int32_t ecu_interp2d(const ecu_map_t *m, uint8_t rows, uint8_t cols, int32_t xq, int32_t x_scale,
                     int32_t yq, int32_t y_scale);

/** Indice do ponto do eixo mais perto da consulta (a celula "atual" da tela). */
uint8_t ecu_axis_nearest(const int16_t *axis, uint8_t n, int32_t scale, int32_t q);

/**
 * Ordem de ignicao para o numero de cilindros e o angulo entre disparos
 * (720 graus / cilindros, ciclo de 4 tempos). Devolve quantos cilindros
 * escreveu em @p order (0 se o numero nao e suportado).
 */
uint8_t ecu_firing_order(uint8_t cylinders, uint8_t order[8], uint16_t *spacing_deg);

#ifdef __cplusplus
}
#endif

#endif /* ECU_ENGINE_H */
