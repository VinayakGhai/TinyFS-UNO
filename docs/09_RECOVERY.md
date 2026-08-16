# Crash Recovery Engine

The recovery engine runs automatically during `tfs_mount()`.

## Recovery Algorithm
1. **Superblock Selection:** Reads SB1 and SB2. Ignores corrupt ones. If both are valid, chooses the one with the higher `generation` number.
2. **Sector Header Validation:** Reads the sector header indicated by the superblock. If its state is not `VALID` (`0x00`) or CRC-8 fails, falls back to the previous sector.
3. **Sequential Scan:** Scans the active sector from the log start.
4. **Record Filtering:** For each record, if CRC and commit marker are valid, it updates the in-RAM offset list.
5. **Rollback:** If it encounters a corrupt record or empty space, it halts the scan. The `write_ptr` is set to this position, truncating the incomplete write.\n