# Record Serialization Notes
- **Problem:** Files must be stored with filenames, lengths, and integrity checks.
- **Why it exists:** To reconstruct files and check for corruption on mount.
- **Naive solution:** Write raw data to hardcoded offsets.
- **Why naive fails:** Updating files overwrites them, causing fragmentation and wear.
- **Our design:** Append-only record format: Header (16B) + Payload + CRC-16 + Commit (1B).
- **RAM consumed:** 16 bytes (temporary stack header).
- **EEPROM writes:** 19 + N bytes per file.\n