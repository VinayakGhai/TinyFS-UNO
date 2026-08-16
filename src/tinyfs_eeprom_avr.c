#ifdef ARDUINO

#include <avr/eeprom.h>
#include "tinyfs_eeprom.h"
#include "tinyfs_config.h"

void tfs_eeprom_init(void) {
    /* AVR hardware EEPROM requires no runtime initialization */
}

uint8_t tfs_eeprom_read(uint16_t addr) {
    if (addr >= TFS_EEPROM_SIZE) {
        return 0xFF;
    }
    return eeprom_read_byte((const uint8_t *)(uintptr_t)addr);
}

void tfs_eeprom_write(uint16_t addr, uint8_t val) {
    if (addr >= TFS_EEPROM_SIZE) {
        return;
    }
    /*
     * eeprom_update_byte reads the destination cell first and only performs 
     * a physical write operation if the value has changed, preserving cell endurance.
     */
    eeprom_update_byte((uint8_t *)(uintptr_t)addr, val);
}

#endif /* ARDUINO */
