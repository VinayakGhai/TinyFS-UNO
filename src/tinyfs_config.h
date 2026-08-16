#ifndef TINYFS_CONFIG_H
#define TINYFS_CONFIG_H

#include <stdint.h>

/* Compatibility macro for compile-time assertions (Static Assert) */
#if defined(__cplusplus)
  #define TFS_STATIC_ASSERT(cond, msg) static_assert(cond, #msg)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
  #define TFS_STATIC_ASSERT(cond, msg) _Static_assert(cond, #msg)
#else
  #define TFS_CONCAT_INNER(a, b) a##b
  #define TFS_CONCAT(a, b) TFS_CONCAT_INNER(a, b)
  #define TFS_STATIC_ASSERT(cond, msg) typedef char TFS_CONCAT(tfs_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/* EEPROM Sizing and Layout Constraints */
#define TFS_EEPROM_SIZE            1024
#define TFS_SUPERBLOCK_SIZE        16
#define TFS_SECTOR_SIZE            496
#define TFS_SECTOR_HEADER_SIZE     8

/* Symmetrical Address Ranges */
#define TFS_SB1_ADDR               0
#define TFS_SB2_ADDR               16
#define TFS_SECTOR_A_ADDR          32
#define TFS_SECTOR_B_ADDR          528

/* Log Regions and Constraints */
#define TFS_SECTOR_LOG_SIZE        (TFS_SECTOR_SIZE - TFS_SECTOR_HEADER_SIZE) /* 488 bytes */
#define TFS_MAX_FILES              8
#define TFS_MAX_FILENAME           12  /* Includes null terminator, so max 11 char name */
#define TFS_MAX_PAYLOAD            256 /* Maximum file payload size in bytes */

/* Signatures and Magics */
#define TFS_MAGIC_SB_0             'T'
#define TFS_MAGIC_SB_1             'F'
#define TFS_MAGIC_SECTOR           0x53 /* 'S' */
#define TFS_COMMIT_MARKER          0x55

/* Sector States (Written to allow 1 -> 0 bit transitions without erase) */
#define TFS_SECTOR_STATE_EMPTY     0xFF
#define TFS_SECTOR_STATE_BUILDING  0x7F
#define TFS_SECTOR_STATE_VALID     0x00

/* Record Status Codes */
#define TFS_RECORD_STATUS_ACTIVE   0xAA
#define TFS_RECORD_STATUS_DELETED  0xDD
#define TFS_RECORD_STATUS_EMPTY    0xFF

/* Compile-time Layout Validation Checks */
TFS_STATIC_ASSERT(TFS_SUPERBLOCK_SIZE == 16, Superblock_must_be_16_bytes);
TFS_STATIC_ASSERT(TFS_SECTOR_HEADER_SIZE == 8, Sector_header_must_be_8_bytes);
TFS_STATIC_ASSERT(TFS_SECTOR_SIZE == 496, Sector_size_must_be_496_bytes);
TFS_STATIC_ASSERT(TFS_EEPROM_SIZE == 1024, EEPROM_size_must_be_1024_bytes);

TFS_STATIC_ASSERT(TFS_SB1_ADDR == 0, SB1_must_start_at_0);
TFS_STATIC_ASSERT(TFS_SB2_ADDR == 16, SB2_must_start_at_16);
TFS_STATIC_ASSERT(TFS_SECTOR_A_ADDR == 32, Sector_A_must_start_at_32);
TFS_STATIC_ASSERT(TFS_SECTOR_B_ADDR == 528, Sector_B_must_start_at_528);

TFS_STATIC_ASSERT(TFS_SB1_ADDR + TFS_SUPERBLOCK_SIZE == TFS_SB2_ADDR, SB1_SB2_boundary_mismatch);
TFS_STATIC_ASSERT(TFS_SB2_ADDR + TFS_SUPERBLOCK_SIZE == TFS_SECTOR_A_ADDR, SB2_SectorA_boundary_mismatch);
TFS_STATIC_ASSERT(TFS_SECTOR_A_ADDR + TFS_SECTOR_SIZE == TFS_SECTOR_B_ADDR, SectorA_SectorB_boundary_mismatch);
TFS_STATIC_ASSERT(TFS_SECTOR_B_ADDR + TFS_SECTOR_SIZE == TFS_EEPROM_SIZE, SectorB_EEPROM_boundary_mismatch);

#endif /* TINYFS_CONFIG_H */
