/**
 * @file tinyfs.h
 * @brief TinyFS Core Header File.
 *
 * Defines the user-facing filesystem structures, data models,
 * API functions, and compile-time layout constraints.
 */

#ifndef TINYFS_H
#define TINYFS_H

#include <stdint.h>
#include <stdbool.h>
#include "tinyfs_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Superblock structure (exactly 16 bytes).
 *
 * Located at addresses 0x000 (SB1) and 0x010 (SB2).
 * Contains filesystem metadata and integrity checks.
 */
struct __attribute__((packed)) Superblock {
    uint8_t magic[2];        /**< Magic bytes "TF" for identification */
    uint32_t generation;     /**< Monotonically increasing sequence number */
    uint8_t active_sector;   /**< 0 for Sector A, 1 for Sector B */
    uint8_t padding[8];      /**< Padding/reserved (stats removed per instruction) */
    uint8_t crc8;            /**< CRC-8 of previous 15 bytes */
};

/**
 * @brief Sector Header structure (exactly 8 bytes).
 *
 * Located at start of each sector: address 32 (Sector A), address 528 (Sector B).
 */
struct __attribute__((packed)) SectorHeader {
    uint8_t magic;           /**< Magic byte 'S' (0x53) */
    uint8_t sector_id;       /**< 0 for A, 1 for B */
    uint32_t generation;     /**< Matches superblock generation when made active */
    uint8_t state;           /**< 0xFF (EMPTY), 0x7F (BUILDING), 0x00 (VALID) */
    uint8_t crc8;            /**< CRC-8 of previous 7 bytes */
};

/**
 * @brief Record Header structure (exactly 16 bytes).
 *
 * Precedes every file entry or deletion marker in the sector log.
 */
struct __attribute__((packed)) RecordHeader {
    uint8_t status;          /**< 0xAA (Active), 0xDD (Deleted), 0xFF (Empty) */
    char filename[TFS_MAX_FILENAME]; /**< Null-terminated filename */
    uint16_t data_len;       /**< Payload length (0 to TFS_MAX_PAYLOAD) */
    uint8_t header_crc8;     /**< CRC-8 covering status, filename, and data_len */
};

/** @name Error Codes
 * @{
 */
#define TFS_ERR_FILE_NOT_FOUND   -40  /**< File not found in filesystem */
#define TFS_ERR_SECTOR_FULL      -41  /**< Active sector is full */
#define TFS_ERR_TOO_MANY_FILES   -42  /**< Maximum file count exceeded */
#define TFS_ERR_NOT_MOUNTED      -43  /**< Filesystem not mounted */
/** @} */

/**
 * @brief File metadata representation for directory queries.
 */
struct TFS_Stat {
    char filename[TFS_MAX_FILENAME]; /**< Null-terminated filename */
    uint16_t size;                   /**< File payload size in bytes */
    uint16_t offset;                 /**< Physical address of the record header in EEPROM */
    uint8_t status;                  /**< 0xAA (Active) or 0xDD (Deleted) */
};

/**
 * @brief Filesystem Context / Runtime State (exactly 20 bytes).
 *
 * Maintains the current state of the mounted filesystem.
 */
struct TFS_Context {
    uint8_t active_sector;                 /**< 0 (A) or 1 (B) */
    uint16_t write_ptr;                    /**< Current physical write pointer in EEPROM */
    uint16_t file_offsets[TFS_MAX_FILES];  /**< EEPROM addresses of active files */
    uint8_t file_count;                    /**< Current number of active files */
};

/** @brief Global filesystem context instance. */
extern struct TFS_Context tfs_ctx;

/**
 * @brief Filesystem statistics structure.
 *
 * Contains information about storage usage and capacity.
 */
struct TFS_StatFS {
    uint16_t total_eeprom;         /**< Total EEPROM size in bytes */
    uint16_t metadata_overhead;    /**< Superblocks + Sector Headers + Record Headers/Footers of active files */
    uint16_t usable_storage;       /**< Size of log region */
    uint16_t live_data;            /**< Active file payloads total size */
    uint16_t free_space;           /**< Empty space left in active sector */
    uint16_t largest_free_record;  /**< Largest payload that can fit now */
    uint8_t file_count;            /**< Number of active files */
    uint16_t compaction_headroom;  /**< Bytes left until compaction is triggered */
};

/**
 * @brief Filesystem check report structure.
 *
 * Contains the results of a filesystem integrity check.
 */
struct TFS_CheckReport {
    bool sb1_valid;            /**< Whether superblock 1 is valid */
    bool sb2_valid;            /**< Whether superblock 2 is valid */
    bool active_sh_valid;      /**< Whether active sector header is valid */
    bool inactive_sh_valid;    /**< Whether inactive sector header is valid */
    uint8_t active_sector;     /**< Currently active sector (0 or 1) */
    uint32_t generation;       /**< Current generation number */
    uint8_t file_count;        /**< Number of active files */
    uint16_t valid_records;    /**< Number of valid records */
    uint16_t corrupt_records;  /**< Number of corrupt records */
    uint16_t free_space;       /**< Free space in bytes */
    bool healthy;              /**< Overall filesystem health status */
};

/** @name Filesystem Core APIs
 * @{
 */

/**
 * @brief Initialize the TinyFS subsystem.
 *
 * Must be called before any other filesystem operations.
 *
 * @return 0 on success, negative error code on failure.
 */
int tfs_init(void);

/**
 * @brief Mount the filesystem from EEPROM.
 *
 * Reads superblocks and sector headers to determine the active sector.
 *
 * @return 0 on success, negative error code on failure.
 */
int tfs_mount(void);

/**
 * @brief Format the filesystem.
 *
 * Erases all data and initializes fresh superblocks and sector headers.
 *
 * @return 0 on success, negative error code on failure.
 */
int tfs_format(void);

/**
 * @brief Check filesystem integrity.
 *
 * Validates superblocks, sector headers, and record integrity.
 *
 * @param[out] report Pointer to a TFS_CheckReport structure to fill.
 * @return 0 on success, negative error code on failure.
 */
int tfs_check(struct TFS_CheckReport *report);

/**
 * @brief Repair the filesystem.
 *
 * Attempts to fix corrupt structures and recover data.
 *
 * @return 0 on success, negative error code on failure.
 */
int tfs_repair(void);

/** @} */

/** @name File Operations APIs
 * @{
 */

/**
 * @brief Write data to a file.
 *
 * Creates a new file or overwrites an existing one.
 *
 * @param[in] filename Null-terminated filename (max TFS_MAX_FILENAME-1 chars).
 * @param[in] data Pointer to the data to write.
 * @param[in] len Number of bytes to write.
 * @return 0 on success, negative error code on failure.
 */
int tfs_write(const char *filename, const uint8_t *data, uint16_t len);

/**
 * @brief Read data from a file.
 *
 * @param[in] filename Null-terminated filename to read.
 * @param[out] buf Buffer to store the read data.
 * @param[in] len Maximum number of bytes to read.
 * @param[in] offset_bytes Byte offset to start reading from.
 * @return Number of bytes read on success, negative error code on failure.
 */
int tfs_read(const char *filename, uint8_t *buf, uint16_t len, uint16_t offset_bytes);

/**
 * @brief Delete a file.
 *
 * Marks the file as deleted (does not immediately reclaim space).
 *
 * @param[in] filename Null-terminated filename to delete.
 * @return 0 on success, negative error code on failure.
 */
int tfs_delete(const char *filename);

/**
 * @brief Check if a file exists.
 *
 * @param[in] filename Null-terminated filename to check.
 * @return true if file exists, false otherwise.
 */
bool tfs_exists(const char *filename);

/**
 * @brief Get file metadata.
 *
 * @param[in] filename Null-terminated filename to query.
 * @param[out] stat_out Pointer to a TFS_Stat structure to fill.
 * @return 0 on success, negative error code on failure.
 */
int tfs_stat(const char *filename, struct TFS_Stat *stat_out);

/**
 * @brief Get filesystem statistics.
 *
 * @param[out] statfs_out Pointer to a TFS_StatFS structure to fill.
 * @return 0 on success, negative error code on failure.
 */
int tfs_statfs(struct TFS_StatFS *statfs_out);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* TINYFS_H */
