#ifdef ARDUINO

#include "tinyfs_eeprom.h"
#include <Arduino.h>
#include <EEPROM.h>

void tfs_eeprom_init(void) {
    /* No hardware initialization needed for ATmega328P internal EEPROM */
}

uint8_t tfs_eeprom_read(uint16_t addr) {
    if (addr >= 1024) {
        return 0xFF; /* Bounds safety check */
    }
    return EEPROM.read(addr);
}

void tfs_eeprom_write(uint16_t addr, uint8_t val) {
    if (addr >= 1024) {
        return; /* Bounds safety check */
    }
    
    /*
     * Use EEPROM.update() on AVR. It checks the existing byte value 
     * and only performs a physical write if the value is different.
     * This is a critical wear-leveling optimization for actual hardware.
     */
    EEPROM.update(addr, val);
}

#endif /* ARDUINO */
