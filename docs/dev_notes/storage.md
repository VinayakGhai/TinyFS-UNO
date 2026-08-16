# Storage Abstraction Notes
- **Problem:** Filesystem code gets bloated with direct hardware-specific I2C/SPI or EEPROM calls.
- **Why it exists:** To separate the filesystem algorithm from physical hardware interfaces.
- **Naive solution:** Scatter `EEPROM.write()` throughout the code.
- **Why naive fails:** Portability is ruined; cannot compile or test the logic on a PC.
- **Our design:** Use `tfs_storage_read_byte` and `tfs_storage_write_byte` wrapping low-level backends.
- **RAM consumed:** 0 bytes (functions are optimized/inlined).
- **EEPROM writes:** 0 writes.\n