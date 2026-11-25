/**
 * @file ecu_crc.h
 * Verificacoes de integridade usadas pela ECU.
 *
 * - CRC-16/CCITT-FALSE: fecha cada frame da UART entre o ESP do motor e a S3.
 *   Polinomio 0x1021, valor inicial 0xFFFF, sem reflexao. Vetor de teste:
 *   "123456789" -> 0x29B1.
 * - CRC-32 (IEEE 802.3, o mesmo do ZIP e do PNG): protege a calibracao gravada
 *   na flash e o arquivo de backup. Vetor de teste: "123456789" -> 0xCBF43926.
 */
#ifndef ECU_CRC_H
#define ECU_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** CRC-16 de um bloco inteiro. */
uint16_t ecu_crc16(const uint8_t *data, size_t len);
/** Continua um CRC-16 ja iniciado (comece com 0xFFFF). */
uint16_t ecu_crc16_update(uint16_t crc, const uint8_t *data, size_t len);

/** CRC-32 de um bloco inteiro. */
uint32_t ecu_crc32(const uint8_t *data, size_t len);
/** Continua um CRC-32 (comece com 0; o resultado pode ser encadeado). */
uint32_t ecu_crc32_update(uint32_t crc, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ECU_CRC_H */
