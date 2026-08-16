#ifndef TINYFS_RECORD_H
#define TINYFS_RECORD_H

#include "tinyfs.h"

/*
 * Record Layer Interface.
 * Contains helpers to read, write, serialize, and validate individual records
 * (headers, payload CRCs, and commit markers) on the EEPROM.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes specific to record parsing */
#define TFS_ERR_CRC_HEADER      -20
#define TFS_ERR_CRC_DATA        -21
#define TFS_ERR_COMMIT_MARKER   -22
#define TFS_ERR_RECORD_INVALID  -23
#define TFS_ERR_FILENAME_EMPTY  -24
#define TFS_ERR_FILENAME_TOOLONG -25

/*
 * Read and validate a record header from EEPROM address `addr`.
 * Returns 0 on success (with header fields populated), or TFS_ERR_CRC_HEADER.
 */
int tfs_record_read_header(uint16_t addr, struct RecordHeader *header);

/*
 * Validates an entire record at address `addr`.
 * Performs header CRC verification, payload CRC verification, and commit marker check.
 * Returns 0 on success, or a negative error code (TFS_ERR_*).
 */
int tfs_record_validate(uint16_t addr, struct RecordHeader *header);

/*
 * Writes a full file/delete record to EEPROM starting at `addr`.
 * Calculates the header CRC-8, writes the header, writes the payload,
 * calculates and writes the payload CRC-16, and finally writes the commit marker.
 * Returns the total physical bytes written on success, or a negative error code.
 */
int tfs_record_write(uint16_t addr, const char *filename, uint8_t status, const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* TINYFS_RECORD_H */
