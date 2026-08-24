/**
 * @file tinyfs.h
 * @brief TinyFS public API declarations.
 */

#ifndef TINYFS_H
#define TINYFS_H

#include <stdint.h>
#include <stdbool.h>
#include "tinyfs_config.h"

/*
 * TinyFS Core Header File.
 * Defines the user-facing filesystem structures, data models,
 * API functions, and compile-time layout constraints.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Superblock structure (exactly 16 bytes).
 * Located at addresses 0x000 (SB1) and 0x010 (SB2).
 */
struct __attribute__((packed)) Superblock {
    uint8_t magic[2];        /* "TF" */
    uint32_t generation;     /* Monotonically increasing sequence number */
    uint8_t active_sector;   /* 0 for Sector A, 1 for Sector B */
    uint8_t padding[8];      /* Padding/reserved (stats removed per instruction) */
    uint8_t crc8;            /* CRC-8 of previous 15 bytes */
};

/* 
 * Sector Header structure (exactly 8 bytes).
 * Located at start of each sector: address 32 (Sector A), address 528 (Sector B).
 */
struct __attribute__((packed)) SectorHeader {
    uint8_t magic;           /* 'S' (0x53) */
    uint8_t sector_id;       /* 0 for A, 1 for B */
    uint32_t generation;     /* Matches superblock generation when made active */
    uint8_t state;           /* 0xFF (EMPTY), 0x7F (BUILDING), 0x00 (VALID) */
    uint8_t crc8;            /* CRC-8 of previous 7 bytes */
};

/* 
 * Record Header structure (exactly 16 bytes).
 * Precedes every file entry or deletion marker in the sector log.
 */
struct __attribute__((packed)) RecordHeader {
    uint8_t status;          /* 0xAA (Active), 0xDD (Deleted), 0xFF (Empty) */
    char filename[TFS_MAX_FILENAME]; /* Null-terminated filename */
    uint16_t data_len;       /* Payload length (0 to TFS_MAX_PAYLOAD) */
    uint8_t header_crc8;     /* CRC-8 covering status, filename, and data_len */
};

#define TFS_ERR_FILE_NOT_FOUND   -40
#define TFS_ERR_SECTOR_FULL      -41
#define TFS_ERR_TOO_MANY_FILES   -42
#define TFS_ERR_NOT_MOUNTED      -43

/* File metadata representation for directory queries */
struct TFS_Stat {
    char filename[TFS_MAX_FILENAME];
    uint16_t size;
    uint16_t offset;         /* Physical address of the record header in EEPROM */
    uint8_t status;          /* 0xAA (Active) or 0xDD (Deleted) */
};

/* Filesystem Context / Runtime State (exactly 20 bytes) */
struct TFS_Context {
    uint8_t active_sector;                 /* 0 (A) or 1 (B) */
    uint16_t write_ptr;                    /* Current physical write pointer in EEPROM */
    uint16_t file_offsets[TFS_MAX_FILES];  /* EEPROM addresses of active files */
    uint8_t file_count;                    /* Current number of active files */
};

extern struct TFS_Context tfs_ctx;

struct TFS_StatFS {
    uint16_t total_eeprom;
    uint16_t metadata_overhead;   /* Superblocks + Sector Headers + Record Headers/Footers of active files */
    uint16_t usable_storage;      /* Size of log region */
    uint16_t live_data;           /* Active file payloads total size */
    uint16_t free_space;          /* Empty space left in active sector */
    uint16_t largest_free_record; /* Largest payload that can fit now */
    uint8_t file_count;
    uint16_t compaction_headroom; /* Bytes left until compaction is triggered */
};

struct TFS_CheckReport {
    bool sb1_valid;
    bool sb2_valid;
    bool active_sh_valid;
    bool inactive_sh_valid;
    uint8_t active_sector;
    uint32_t generation;
    uint8_t file_count;
    uint16_t valid_records;
    uint16_t corrupt_records;
    uint16_t free_space;
    bool healthy;
};

/* Filesystem Core APIs */
/** @brief tfs init. */
int tfs_init(void);
int tfs_mount(void);
int tfs_format(void);
int tfs_check(struct TFS_CheckReport *report);
/** @brief tfs repair. */
int tfs_repair(void);

/* File Operations APIs */
int tfs_write(const char *filename, const uint8_t *data, uint16_t len);
/** @brief tfs read. */
int tfs_read(const char *filename, uint8_t *buf, uint16_t len, uint16_t offset_bytes);
/** @brief tfs delete. */
int tfs_delete(const char *filename);
bool tfs_exists(const char *filename);
/** @brief tfs stat. */
int tfs_stat(const char *filename, struct TFS_Stat *stat_out);
int tfs_statfs(struct TFS_StatFS *statfs_out);

#ifdef __cplusplus
}
#endif

#endif /* TINYFS_H */
