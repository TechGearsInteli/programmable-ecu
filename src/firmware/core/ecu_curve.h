/*
 * ecu_curve.h - Curva 1D (x -> y) com interpolacao linear.
 *
 * Usada para o dead time do injetor em funcao da tensao da bateria (1.5.1.5)
 * e, mais adiante, para as correcoes por temperatura. C99 puro, so inteiros.
 */
#ifndef ECU_CURVE_H
#define ECU_CURVE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * n pontos (x[i], y[i]). O eixo x precisa ser estritamente crescente e ter
 * pelo menos 2 pontos. y pode subir ou descer. Nao e dona da memoria.
 */
typedef struct {
    uint8_t n;
    const uint16_t *x;
    const uint16_t *y;
} ecu_curve_t;

/* true se a curva pode ser consultada (ponteiros, tamanho e eixo x). */
bool ecu_curve_valid(const ecu_curve_t *c);

/*
 * Valor interpolado em x, arredondado para o inteiro mais proximo (o meio
 * arredonda para cima). Fora da faixa nao extrapola: vale o ponto da borda.
 * Devolve false, sem escrever em *out, se a curva for invalida.
 */
bool ecu_curve_lookup(const ecu_curve_t *c, uint16_t x, uint16_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ECU_CURVE_H */
