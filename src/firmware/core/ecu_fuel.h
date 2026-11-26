/*
 * ecu_fuel.h - Tempo de abertura do injetor (pulse width).
 *
 * Task 1.5.1.5. Parte do valor base lido no mapa de combustivel (ecu_table),
 * aplica as correcoes multiplicativas e soma o dead time do injetor:
 *
 *     pw = base * corr[0] * corr[1] * ...       (fatores em permil)
 *     pw = 0                         se o resultado das correcoes for 0
 *     pw = pw + dead_time(Vbat)      senao
 *     pw = min(pw, max_us)
 *
 * Unidades, para o mapa, as correcoes e o firmware falarem a mesma lingua:
 *   - tempo em microssegundos (us): 4,55 ms e 4550;
 *   - tensao em milivolts (mV): 13,8 V e 13800;
 *   - correcao em permil: 1000 = sem correcao, 1200 = +20 %, 0 = corte.
 *
 * C99 puro, so inteiros, sem Arduino.h.
 */
#ifndef ECU_FUEL_H
#define ECU_FUEL_H

#include <stdbool.h>
#include <stdint.h>

#include "ecu_curve.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fator de correcao que nao altera nada. */
#define ECU_CORR_NONE 1000u

typedef struct {
    /* Valor base do mapa (us), ja interpolado com ecu_table_lookup. */
    uint16_t base_us;

    /* n_corr fatores em permil. Pode ser NULL se n_corr for 0. */
    const uint16_t *corr_permille;
    uint8_t n_corr;

    /*
     * Dead time do injetor (us) em funcao da tensao da bateria (mV).
     * Obrigatoria. Para nao compensar, passe uma curva com y = 0.
     */
    const ecu_curve_t *dead_time;
    uint16_t vbat_mv;

    /*
     * Teto do tempo comandado (us), tipicamente o tempo do ciclo do motor.
     * Para nao limitar, passe UINT32_MAX.
     */
    uint32_t max_us;
} ecu_fuel_in_t;

/*
 * Calcula o tempo comandado ao injetor, em microssegundos.
 *
 * Se as correcoes zerarem o tempo (corte de combustivel), o resultado e 0 e o
 * dead time NAO e somado: o injetor nem chega a ser energizado.
 *
 * Cada fator e aplicado em sequencia sobre o valor em us, arredondando para o
 * inteiro mais proximo (o meio arredonda para cima). A ordem dos fatores muda
 * o resultado em no maximo 1 us por fator.
 *
 * Devolve false, sem escrever em *out_us, se algum ponteiro obrigatorio for
 * NULL, se n_corr > 0 com corr_permille NULL, ou se a curva de dead time for
 * invalida.
 */
bool ecu_fuel_pulse_width_us(const ecu_fuel_in_t *in, uint32_t *out_us);

#ifdef __cplusplus
}
#endif

#endif /* ECU_FUEL_H */
