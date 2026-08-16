# TinyFS-UNO Core Invariants

This document outlines the design invariants of TinyFS-UNO. These invariants guarantee the correctness, crash consistency, integrity, and safety of the filesystem under all operating conditions, including sudden power loss.

---

## 1. Record Validation Invariant
> **A record is committed and valid if and only if its header CRC-8 is valid, its data payload CRC-16 is valid, and its trailing commit marker is exactly `0x55`.**

* **Rationale:** Writing to EEPROM occurs byte-by-byte. If power is lost mid-write, the record will be incomplete. The mount scanner must verify all three elements (header CRC, data CRC, commit marker) before considering a record part of the active filesystem.
* **Corollaries:**
  * Any record with a mismatching CRC or a missing commit marker is treated as garbage/free space.
  * The scan stops at the first invalid record in the sector log.

---

## 2. Active Sector Selection Invariant
> **Only the valid superblock with the highest generation number determines the active sector of the filesystem.**

* **Rationale:** We maintain a metadata pair (Superblock 1 at address 0, Superblock 2 at address 8). During compaction, the new superblock is written to the alternate slot. If power fails while writing the new superblock, its CRC-8 will fail, and the system will fall back to the older valid superblock.
* **Corollaries:**
  * If both superblocks are valid, the one with `generation_A > generation_B` is active.
  * If one superblock is corrupt, the other valid one is active.
  * If both are corrupt, the filesystem is unmounted/unformatted.

---

## 3. Sector State Invariant
> **A sector in the `BUILDING` state (`0x7F`) can never be treated as the active sector.**

* **Rationale:** During compaction, active files are copied to the inactive sector. This destination sector's header is set to `BUILDING` before copying begins. Only after all copies complete successfully is the state updated to `VALID` (`0x00`), followed by writing the new superblock.
* **Corollaries:**
  * If the active superblock points to a sector whose header is not `VALID` (e.g. because the superblock write somehow succeeded but the sector state was not updated, or due to corruption), the mount fails or falls back.

---

## 4. Compaction Crash-Safety Invariant
> **An interrupted compaction must leave the source sector fully intact and recoverable.**

* **Rationale:** Compaction reads from the active sector and writes to the inactive sector. The active sector is never modified during this process. The superblock pointer is only updated *after* the inactive sector has been successfully written and marked `VALID`.
* **Corollaries:**
  * If power is lost at any point during compaction, the reboot mount will still point to the original active sector, restoring the filesystem to its exact state prior to compaction.

---

## 5. Memory Boundary Invariant
> **No filesystem operation may read or write EEPROM addresses outside the defined bounds of the superblock and sector regions.**

* **Rationale:** Preventing buffer overflows and memory corruption is critical on a 1 KB EEPROM.
* **Boundaries:**
  * Superblock 1: `0` to `15` (16 bytes)
  * Superblock 2: `16` to `31` (16 bytes)
  * Sector A: `32` to `527` (496 bytes)
    * Sector A Header: `32` to `39` (8 bytes)
    * Sector A Log: `40` to `527` (488 bytes)
  * Sector B: `528` to `1023` (496 bytes)
    * Sector B Header: `528` to `535` (8 bytes)
    * Sector B Log: `536` to `1023` (488 bytes)

---

## 6. Logical State Reconstruction Invariant
> **Mounting must reconstruct the same logical file state (names, content, existence) regardless of physical location changes during compaction.**

* **Rationale:** Compaction moves records to different physical addresses. The filesystem must ensure that the logical file lookup maps to the newly copied records exactly as it did before.
* **Corollaries:**
  * The in-RAM offset list is reconstructed on every mount by scanning the active sector log from start to end.
  * If multiple records for the same filename exist in the log, the one appearing latest (highest address) overrides previous records.
