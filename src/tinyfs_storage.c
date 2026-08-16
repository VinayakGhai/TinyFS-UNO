#include "tinyfs_storage.h"
#include "tinyfs_eeprom.h"

/* Define global session metrics */
struct TFS_Metrics tfs_metrics;

void tfs_storage_init(void) {
    tfs_eeprom_init();
    tfs_metrics_reset();
}

uint8_t tfs_storage_read_byte(uint16_t addr) {
    return tfs_eeprom_read(addr);
}

void tfs_storage_read_buffer(uint16_t addr, uint8_t *buf, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = tfs_eeprom_read(addr + i);
    }
}

void tfs_storage_write_byte(uint16_t addr, uint8_t val) {
    /* 
     * Even if the hardware backend (like EEPROM.update) optimizes away 
     * redundant writes, we count the requested physical byte write here.
     * This ensures that our physical metric tracks the raw write requests 
     * sent to the storage media, which correlates directly with code efficiency.
     */
    tfs_metrics.physical_bytes_written++;
    tfs_eeprom_write(addr, val);
}

void tfs_storage_write_buffer(uint16_t addr, const uint8_t *buf, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        tfs_storage_write_byte(addr + i, buf[i]);
    }
}

void tfs_metrics_reset(void) {
    tfs_metrics.logical_writes = 0;
    tfs_metrics.logical_bytes_written = 0;
    tfs_metrics.physical_bytes_written = 0;
}

void tfs_metrics_log_logical_write(uint16_t payload_len) {
    tfs_metrics.logical_writes++;
    tfs_metrics.logical_bytes_written += payload_len;
}

float tfs_metrics_get_write_amplification(void) {
    if (tfs_metrics.logical_bytes_written == 0) {
        return 0.0f;
    }
    return (float)tfs_metrics.physical_bytes_written / (float)tfs_metrics.logical_bytes_written;
}
