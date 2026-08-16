#ifndef TINYFS_METADATA_H
#define TINYFS_METADATA_H

#include "tinyfs.h"

/*
 * Metadata Layer Interface.
 * Manages low-level access, validation, and writing of the superblock pair
 * and the individual sector state headers. Implements generation-based selection
 * and transaction-safety invariants.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes for metadata parsing */
#define TFS_ERR_NO_VALID_SB     -30
#define TFS_ERR_INVALID_SECTOR   -31
#define TFS_ERR_SB_WRITE_FAIL    -32
#define TFS_ERR_SECTOR_WRITE_FAIL -33

/* --- Superblock Operations --- */

/*
 * Reads a superblock from EEPROM slot 0 (SB1) or slot 1 (SB2).
 * Returns 0 on success, or a negative error code.
 */
int tfs_sb_read(uint8_t sb_idx, struct Superblock *sb);

/*
 * Validates the signature and CRC-8 checksum of a superblock.
 * Returns true if valid, false otherwise.
 */
bool tfs_sb_validate(const struct Superblock *sb);

/*
 * Writes a superblock structure to the specified slot, calculating CRC-8 automatically.
 * Returns 0 on success, or a negative error code.
 */
int tfs_sb_write(uint8_t sb_idx, const struct Superblock *sb);

/*
 * Scans both superblock slots and selects the valid superblock with the highest generation.
 * Populates active_sb and returns 0, or returns TFS_ERR_NO_VALID_SB if both are corrupt.
 */
int tfs_sb_select(struct Superblock *active_sb, uint16_t *active_sb_addr);


/* --- Sector Header Operations --- */

/*
 * Reads a sector header from Sector A (id=0) or Sector B (id=1).
 * Returns 0 on success, or a negative error code.
 */
int tfs_sector_read_header(uint8_t sector_id, struct SectorHeader *sh);

/*
 * Validates a sector header's magic number, sector ID, and CRC-8.
 * Returns true if valid, false otherwise.
 */
bool tfs_sector_validate_header(const struct SectorHeader *sh, uint8_t expected_sector_id);

/*
 * Formats a sector header by writing its signature, ID, generation, and state,
 * computing its CRC-8 automatically.
 * Returns 0 on success, or a negative error code.
 */
int tfs_sector_write_header(uint8_t sector_id, uint32_t generation, uint8_t state);

/*
 * Updates a sector's state (EMPTY, BUILDING, VALID) and its CRC-8 checksum
 * by writing only the state and CRC bytes to minimize write wear.
 * Returns 0 on success, or a negative error code.
 */
int tfs_sector_update_state(uint8_t sector_id, uint8_t new_state);

#ifdef __cplusplus
}
#endif

#endif /* TINYFS_METADATA_H */
