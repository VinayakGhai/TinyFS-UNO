#include "tinyfs_record.h"
#include "tinyfs_crc.h"
#include "tinyfs_storage.h"
#include <string.h>

int tfs_record_read_header(uint16_t addr, struct RecordHeader *header) {
    if (addr + TFS_SECTOR_HEADER_SIZE > TFS_EEPROM_SIZE) {
        return TFS_ERR_RECORD_INVALID;
    }
    
    /* Read the raw record header from storage */
    tfs_storage_read_buffer(addr, (uint8_t *)header, sizeof(struct RecordHeader));
    
    /* Calculate CRC-8 over the first 15 bytes (excluding the crc8 field itself) */
    uint8_t calculated_crc = tfs_crc8(0, (const uint8_t *)header, sizeof(struct RecordHeader) - 1);
    
    if (calculated_crc != header->header_crc8) {
        return TFS_ERR_CRC_HEADER;
    }
    
    return 0;
}

int tfs_record_validate(uint16_t addr, struct RecordHeader *header) {
    /* 1. Read and validate the record header first */
    int ret = tfs_record_read_header(addr, header);
    if (ret < 0) {
        return ret;
    }
    
    /* 2. Check if the record is empty or invalid */
    if (header->status == TFS_RECORD_STATUS_EMPTY) {
        return TFS_ERR_RECORD_INVALID;
    }
    if (header->data_len > TFS_MAX_PAYLOAD) {
        return TFS_ERR_RECORD_INVALID;
    }
    
    /* 3. Calculate payload CRC-16 byte-by-byte to minimize SRAM usage */
    uint16_t payload_crc = 0xFFFF;
    for (uint16_t i = 0; i < header->data_len; i++) {
        uint8_t val = tfs_storage_read_byte(addr + sizeof(struct RecordHeader) + i);
        payload_crc = tfs_crc16(payload_crc, &val, 1);
    }
    
    /* 4. Read expected CRC-16 from EEPROM and verify */
    uint8_t crc_bytes[2];
    uint16_t crc_addr = addr + sizeof(struct RecordHeader) + header->data_len;
    tfs_storage_read_buffer(crc_addr, crc_bytes, 2);
    
    uint16_t expected_crc = ((uint16_t)crc_bytes[0] << 8) | crc_bytes[1];
    if (payload_crc != expected_crc) {
        return TFS_ERR_CRC_DATA;
    }
    
    /* 5. Read and verify commit marker */
    uint16_t commit_addr = crc_addr + 2;
    uint8_t commit_marker = tfs_storage_read_byte(commit_addr);
    if (commit_marker != TFS_COMMIT_MARKER) {
        return TFS_ERR_COMMIT_MARKER;
    }
    
    return 0;
}

int tfs_record_write(uint16_t addr, const char *filename, uint8_t status, const uint8_t *data, uint16_t len) {
    /* Validate inputs */
    if (filename == NULL) {
        return TFS_ERR_FILENAME_EMPTY;
    }
    
    size_t name_len = strlen(filename);
    if (name_len == 0) {
        return TFS_ERR_FILENAME_EMPTY;
    }
    if (name_len >= TFS_MAX_FILENAME) {
        return TFS_ERR_FILENAME_TOOLONG;
    }
    if (len > TFS_MAX_PAYLOAD) {
        return TFS_ERR_RECORD_INVALID;
    }
    
    /* Calculate physical record size: Header (16B) + Payload (len) + CRC-16 (2B) + Commit (1B) */
    uint16_t record_size = sizeof(struct RecordHeader) + len + 2 + 1;
    if (addr + record_size > TFS_EEPROM_SIZE) {
        return TFS_ERR_RECORD_INVALID; /* Out of EEPROM bounds */
    }
    
    /* Construct Record Header */
    struct RecordHeader header;
    header.status = status;
    memset(header.filename, 0, TFS_MAX_FILENAME);
    strncpy(header.filename, filename, name_len);
    header.data_len = len;
    
    /* Calculate CRC-8 over header fields */
    header.header_crc8 = tfs_crc8(0, (const uint8_t *)&header, sizeof(struct RecordHeader) - 1);
    
    /* Begin writing to storage */
    tfs_storage_write_buffer(addr, (const uint8_t *)&header, sizeof(struct RecordHeader));
    
    /* Write payload data if length is non-zero */
    if (len > 0 && data != NULL) {
        tfs_storage_write_buffer(addr + sizeof(struct RecordHeader), data, len);
    }
    
    /* Calculate payload CRC-16 and serialize in big-endian */
    uint16_t payload_crc = tfs_crc16(0xFFFF, data, len);
    uint8_t crc_bytes[2];
    crc_bytes[0] = (payload_crc >> 8) & 0xFF;
    crc_bytes[1] = payload_crc & 0xFF;
    
    uint16_t crc_addr = addr + sizeof(struct RecordHeader) + len;
    tfs_storage_write_buffer(crc_addr, crc_bytes, 2);
    
    /* Finally, write the commit marker to complete the transaction */
    uint16_t commit_addr = crc_addr + 2;
    tfs_storage_write_byte(commit_addr, TFS_COMMIT_MARKER);
    
    return record_size;
}
