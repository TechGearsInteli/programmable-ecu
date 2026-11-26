/*
 * ecu_ignition.h - Avanco de ignicao e momento do disparo da faisca.
 *
 * Task 1.5.1.7. Parte do avanco lido no mapa de ignicao (ecu_table) e
 * calcula, em microssegundos a partir de um evento de referencia do
 * virabrequim, quando comecar a carregar a bobina e quando disparar a faisca.
 * Nao ha correcao por sonda lambda aqui: o avanco e so o do mapa.
 *
 * O padrao de dentes da roda fonica ainda nao esta decidido, entao o calculo
 * e relativo a um evento de referencia qualquer (por exemplo, um dente), cuja
 * posicao em relacao ao PMS de cada cilindro o chamador informa.
 *
 *   PMS = ponto morto superior; "avanco" = graus ANTES do PMS.
 *
 *        referencia                     faisca            PMS
 *            |<---- ref_btdc_x10 ----------------------------->|
 *            |<-- ref_btdc - avanco -->|<--- avanco ---------->|
 *
 * Unidades, no mesmo estilo do tempo de bico (ecu_fuel.h):
 *   - angulo em decimos de grau (x10): 25,5 graus e 255;
 *   - tempo em microssegundos (us);
 *   - rotacao em RPM.
 *
 * O avanco e sem sinal: nao ha atraso (faisca depois do PMS) nesta versao.
 *
 * C99 puro, so inteiros, sem Arduino.h.
 */
#ifndef ECU_IGNITION_H
#define ECU_IGNITION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Tempo que o virabrequim leva para girar angle_x10 (decimos de grau) a rpm
 * constante, em us, arredondado para o inteiro mais proximo (meio para cima).
 * Devolve false, sem escrever em *out_us, se rpm for 0 ou out_us for NULL.
 */
bool ecu_angle_to_us(uint32_t angle_x10, uint16_t rpm, uint32_t *out_us);

typedef struct {
    uint16_t rpm;

    /* Avanco lido do mapa de ignicao (decimos de grau antes do PMS). */
    uint16_t advance_x10;

    /*
     * Teto de seguranca do avanco. Uma celula errada no mapa (um zero a mais)
     * nao pode adiantar a faisca alem disso e causar detonacao.
     */
    uint16_t max_advance_x10;

    /* Quantos decimos de grau o evento de referencia vem antes do PMS. */
    uint16_t ref_btdc_x10;

    /* Tempo de carga da bobina (dwell), em us. */
    uint32_t dwell_us;
} ecu_spark_in_t;

typedef struct {
    /* Quando comecar a carregar a bobina, em us a partir da referencia. */
    uint32_t dwell_start_us;

    /* Quando disparar a faisca, em us a partir da referencia. */
    uint32_t spark_us;

    /* Avanco realmente usado, depois do teto (decimos de grau). */
    uint16_t advance_x10;
} ecu_spark_out_t;

typedef enum {
    /* Tudo valido. */
    ECU_SPARK_OK = 0,

    /*
     * Ha menos tempo que o dwell entre a referencia e a faisca. A faisca esta
     * certa (spark_us), mas a bobina comecaria a carregar antes da referencia:
     * dwell_start_us vale 0, ou seja, carregar ja a partir dela, com dwell
     * menor que o pedido. Quem chama decide se aceita ou usa uma referencia
     * anterior.
     */
    ECU_SPARK_SHORT_DWELL,

    /*
     * O avanco passa da referencia: a faisca teria que sair antes dela. Nada
     * e escrito em *out. Use uma referencia anterior (mais longe do PMS).
     */
    ECU_SPARK_TOO_EARLY,

    /* Ponteiro nulo ou rpm 0. Nada e escrito em *out. */
    ECU_SPARK_BAD_INPUT
} ecu_spark_status_t;

/*
 * Calcula o momento da faisca. Escreve em *out apenas para OK e SHORT_DWELL.
 * O avanco usado e min(advance_x10, max_advance_x10).
 */
ecu_spark_status_t ecu_spark_schedule(const ecu_spark_in_t *in,
                                      ecu_spark_out_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ECU_IGNITION_H */
