#ifndef TINYFS_STORAGE_H
#define TINYFS_STORAGE_H

#include <stdint.h>

/*
 * Storage Abstraction Layer.
 * Wraps low-level EEPROM reads/writes, provides block/buffer operations,
 * and maintains runtime session statistics (logical vs physical writes,
 * write amplification metrics) without persisting them to the media.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Runtime Session Metrics (reset on boot/re-init) */
struct TFS_Metrics {
    uint32_t logical_writes;            /* Count of logical file content updates */
    uint32_t logical_bytes_written;     /* Number of user payload bytes written */
    uint32_t physical_bytes_written;    /* Number of raw bytes written to EEPROM */
};

extern struct TFS_Metrics tfs_metrics;

/* Storage Abstraction API */
void tfs_storage_init(void);
uint8_t tfs_storage_read_byte(uint16_t addr);
void tfs_storage_read_buffer(uint16_t addr, uint8_t *buf, uint16_t len);

void tfs_storage_write_byte(uint16_t addr, uint8_t val);
void tfs_storage_write_buffer(uint16_t addr, const uint8_t *buf, uint16_t len);

/* Metric Helper Functions */
void tfs_metrics_reset(void);
void tfs_metrics_log_logical_write(uint16_t payload_len);
float tfs_metrics_get_write_amplification(void);

#ifdef __cplusplus
}
#endif

#endif /* TINYFS_STORAGE_H */
