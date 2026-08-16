# Garbage Collection Notes
- **Problem:** Log space runs out.
- **Why it exists:** To reclaim space from obsolete files.
- **Naive solution:** Move files in place.
- **Why naive fails:** Interrupted compaction destroys the files.
- **Our design:** Symmetrical sector ping-pong with a BUILDING destination sector.
- **RAM consumed:** 256 bytes (payload buffer).
- **EEPROM writes:** Size of migrated files + superblock.\n