#ifndef TINYFS_EEPROM_H
#define TINYFS_EEPROM_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Low-level hardware-level EEPROM interface.
 * Separates the actual hardware access from the filesystem core,
 * allowing identical code to compile and run on the Host PC (with a mock buffer)
 * and the Arduino Uno (via the standard AVR EEPROM library).
 */

#ifdef __cplusplus
extern "C" {
#endif

void tfs_eeprom_init(void);
uint8_t tfs_eeprom_read(uint16_t addr);
void tfs_eeprom_write(uint16_t addr, uint8_t val);

#ifndef ARDUINO
/* 
 * Host-side testing helper APIs. 
 * Allows the test suite to inspect and assert against direct physical writes 
 * and simulate power failure.
 */
uint32_t tfs_eeprom_mock_get_write_count(uint16_t addr);
void tfs_eeprom_mock_reset_write_counts(void);
uint8_t* tfs_eeprom_mock_get_buffer(void);
void tfs_eeprom_mock_set_write_limit(int limit);
bool tfs_eeprom_mock_was_aborted(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* TINYFS_EEPROM_H */
