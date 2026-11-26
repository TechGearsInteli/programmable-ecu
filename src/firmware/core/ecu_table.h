/*
 * ecu_table.h - Tabela 2D (RPM x carga) com interpolacao bilinear.
 *
 * Task 1.5.1.4. C99 puro, sem Arduino.h nem ESP-IDF: o mesmo codigo roda no
 * ESP32 do motor e nos testes do computador (src/firmware/test).
 *
 * Usada pelos mapas de combustivel (1.5.1.5) e de ignicao (1.5.1.7).
 */
#ifndef ECU_TABLE_H
#define ECU_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Tabela com n_rpm linhas e n_load colunas, guardada em ordem de linha:
 * a celula (i, j) fica em cells[i * n_load + j], com i no eixo RPM e j no
 * eixo de carga.
 *
 * Os eixos precisam ser estritamente crescentes e ter pelo menos 2 pontos.
 * A tabela nao e dona da memoria: os vetores ficam com quem a monta. As
 * celulas nao sao const porque a calibracao (1.6) vai editar o mapa.
 */
typedef struct {
    uint8_t n_rpm;
    uint8_t n_load;
    const uint16_t *rpm_axis;
    const uint16_t *load_axis;
    uint16_t *cells;
} ecu_table_t;

/* Celula que contem o ponto consultado: o canto inferior e o seu vizinho. */
typedef struct {
    uint8_t rpm_idx;  /* 0 .. n_rpm - 2 */
    uint8_t load_idx; /* 0 .. n_load - 2 */
} ecu_cell_t;

/* true se a tabela pode ser consultada (ponteiros, tamanhos e eixos). */
bool ecu_table_valid(const ecu_table_t *t);

/*
 * Acha a celula que contem (rpm, load). Fora da faixa, vale a celula da
 * borda. Devolve false, sem escrever em *out, se a tabela for invalida.
 */
bool ecu_table_locate(const ecu_table_t *t, uint16_t rpm, uint16_t load,
                      ecu_cell_t *out);

/*
 * Valor interpolado em (rpm, load), arredondado para o inteiro mais proximo
 * (meio arredonda para cima).
 *
 *  - Sobre um no da grade devolve a celula exata, inclusive nos cantos.
 *  - Fora da faixa dos eixos nao extrapola: usa o valor da borda.
 *  - Devolve false, sem escrever em *out, se a tabela for invalida.
 */
bool ecu_table_lookup(const ecu_table_t *t, uint16_t rpm, uint16_t load,
                      uint16_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ECU_TABLE_H */
