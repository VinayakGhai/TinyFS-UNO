#ifndef TINYFS_CRC_H
#define TINYFS_CRC_H

#include <stdint.h>
#include <stddef.h>

/*
 * Integrity check utility header.
 * Provides CRC-8 and CRC-16 implementations optimized for microcontrollers.
 * To save flash and RAM on the ATmega328P, this uses a bit-by-bit calculation
 * rather than loading large pre-computed tables into memory.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Calculate CRC-8 (SMBus polynomial 0x07: x^8 + x^2 + x + 1).
 * Initial CRC value is typically 0x00.
 */
uint8_t tfs_crc8(uint8_t crc, const uint8_t *data, size_t len);

/*
 * Calculate CRC-16-CCITT (polynomial 0x1021: x^16 + x^12 + x^5 + 1).
 * Initial CRC value is typically 0xFFFF.
 */
uint16_t tfs_crc16(uint16_t crc, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* TINYFS_CRC_H */
