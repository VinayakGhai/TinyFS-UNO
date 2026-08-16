/**
 * @file tinyfs_gc.h
 * @brief Garbage Collection (Compaction) Layer.
 *
 * Reclaims log space by migrating active files from the full sector
 * to the alternate inactive sector. Guarantees atomic transaction
 * switching and power-loss safety.
 */

#ifndef TINYFS_GC_H
#define TINYFS_GC_H

#include "tinyfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Perform sector compaction (Garbage Collection).
 *
 * Reclaims space by migrating active files to the inactive sector:
 * 1. Checks if active files + extra_needed will fit in the alternate sector.
 * 2. Writes the BUILDING header to the inactive sector.
 * 3. Copies all active files sequentially.
 * 4. Marks the new sector as VALID.
 * 5. Writes the new superblock to alternate SB slot.
 * 6. Marks the old sector header as EMPTY.
 *
 * @param[in] extra_needed Additional bytes required after compaction.
 * @return 0 on success, negative error code on failure.
 */
int tfs_gc_compact(uint16_t extra_needed);

#ifdef __cplusplus
}
#endif

#endif /* TINYFS_GC_H */
