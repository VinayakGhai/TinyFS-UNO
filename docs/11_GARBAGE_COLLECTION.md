# Garbage Collection (Compaction)

When the active sector log becomes full, we must compact it to reclaim space.

## Compaction Steps
1. Identify all active files from the current active sector.
2. Write Sector Header of the inactive sector as `BUILDING` (`0x7F`).
3. Copy the latest valid records of all active files sequentially to the inactive sector.
4. Write the end-of-log marker (`0xFF`).
5. Update destination sector header to `VALID` (`0x00`).
6. Write the new superblock pointing to the new active sector and incrementing the generation.
7. Mark the old sector header as `EMPTY` (`0xFF`).
8. Re-point runtime context to the new sector.\n