#ifndef ARDUINO

#include "tinyfs_eeprom.h"
#include "tinyfs_config.h"
#include <string.h>
#include <stdbool.h>

/* Mock 1024-byte EEPROM storage */
static uint8_t eeprom_buf[TFS_EEPROM_SIZE];

/* Byte-level physical write counters for wear-level analysis */
static uint32_t write_counts[TFS_EEPROM_SIZE];

static int write_limit = -1; /* -1 means unlimited */
static bool write_aborted = false;

void tfs_eeprom_init(void) {
    /* Simulate brand-new EEPROM state (all 0xFF) */
    memset(eeprom_buf, 0xFF, TFS_EEPROM_SIZE);
    memset(write_counts, 0, sizeof(write_counts));
    write_limit = -1;
    write_aborted = false;
}

uint8_t tfs_eeprom_read(uint16_t addr) {
    if (addr >= TFS_EEPROM_SIZE) {
        return 0xFF; /* Read bounds protection */
    }
    return eeprom_buf[addr];
}

void tfs_eeprom_write(uint16_t addr, uint8_t val) {
    if (addr >= TFS_EEPROM_SIZE) {
        return; /* Write bounds protection */
    }
    
    /* Simulate write abort/power loss */
    if (write_limit == 0) {
        write_aborted = true;
        return; /* Drop write */
    }
    if (write_limit > 0) {
        write_limit--;
    }
    
    /* 
     * Record write event. In actual hardware (ATmega328P), writing to 
     * EEPROM takes ~3.3ms and causes wear regardless of whether the cell
     * value changes, unless the driver explicitly uses EEPROM.update().
     * Here, we count all physical write requests to reflect driver performance.
     */
    write_counts[addr]++;
    eeprom_buf[addr] = val;
}

uint32_t tfs_eeprom_mock_get_write_count(uint16_t addr) {
    if (addr >= TFS_EEPROM_SIZE) {
        return 0;
    }
    return write_counts[addr];
}

void tfs_eeprom_mock_reset_write_counts(void) {
    memset(write_counts, 0, sizeof(write_counts));
}

uint8_t* tfs_eeprom_mock_get_buffer(void) {
    return eeprom_buf;
}

void tfs_eeprom_mock_set_write_limit(int limit) {
    write_limit = limit;
    write_aborted = false;
}

bool tfs_eeprom_mock_was_aborted(void) {
    return write_aborted;
}

#endif /* ARDUINO */
