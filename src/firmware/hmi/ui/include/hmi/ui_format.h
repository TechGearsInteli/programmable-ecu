/**
 * @file ui_format.h
 * Numeros inteiros com escala (ms x100, graus x10...) <-> texto com virgula.
 * Sem float: o mesmo resultado na placa e no PC.
 */
#ifndef HMI_UI_FORMAT_H
#define HMI_UI_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Casas decimais da unidade (ms x100 -> 2, graus x10 -> 1, rpm -> 0). */
int hmi_unit_decimals(uint8_t unit);
/** Sufixo da unidade ("ms", "°", "%", "µs"...). */
const char *hmi_unit_suffix(uint8_t unit);
/** Valor sem unidade: "4,55", "-1,5", "+12,0" (com sinal se @p sign). */
void hmi_fmt_value(char *buf, size_t n, uint8_t unit, int32_t v, bool sign);
/** Valor com unidade: "4,55 ms", "18,5°", "+12,0 %". */
void hmi_fmt_with_unit(char *buf, size_t n, uint8_t unit, int32_t v, bool sign);
/** Le "4,6" ou "4.6" na unidade (-> 460 em ms x100). false se nao for numero. */
bool hmi_parse_value(const char *text, uint8_t unit, int32_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HMI_UI_FORMAT_H */
