# Comparison with Existing Filesystems

How TinyFS-UNO compares to other embedded storage engines:

| Metric | TinyFS-UNO | littlefs | OSFS | EEWL |
|---|---|---|---|---|
| **Target Media** | 1 KB EEPROM | SPI Flash | EEPROM | EEPROM |
| **RAM Needed** | 20 B | ~1.5 - 2 KB | ~100 B | ~50 B |
| **Flash footprint**| ~4 KB | ~12 - 20 KB | ~3 KB | ~2 KB |
| **Wear Leveling** | Yes (Log) | Yes (Block) | No | Yes (Erase) |
| **Atomicity** | Yes (Commit) | Yes (COW) | No | No |
| **CRC Checking** | Yes (CRC-8/16)| Yes (CRC-32) | No | No |
| **Metadata Size** | 16 B | 256 B | 16 B | 8 B |\n