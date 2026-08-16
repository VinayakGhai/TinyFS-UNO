# Recovery Notes
- **Problem:** Mounting a crashed filesystem.
- **Why it exists:** To rebuild the file directory and truncate partial writes.
- **Naive solution:** Assume the write pointer is at a fixed address.
- **Why naive fails:** We don't know where the write was interrupted.
- **Our design:** Sequential scan until a CRC or commit marker check fails, then truncate log.
- **RAM consumed:** 16 bytes (for header buffer).
- **EEPROM writes:** 0 writes.\n