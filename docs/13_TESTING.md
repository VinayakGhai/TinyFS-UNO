# Testing Architecture

We split testing into Host-side testing and Arduino-side testing.

## Host Tests (tests/test_tinyfs.c)
- Compiles with standard `gcc`.
- Contains **12 test blocks** verifying layout constants, raw storage, metrics, CRC algorithms, record serialization, metadata headers, mount scanning, file APIs, compaction sector migration, integrity check/repair, fault injection recovery, and CLI commands.
- Simulates the 1024-byte EEPROM in RAM with address-level write counters.
- Implements a write-interruption limit to test recovery after power loss on every byte boundary.

## Resource Audit & SRAM Breakdown
- **Core Library Static RAM:** 34 bytes (Context + Metrics).
- **CLI Shell Buffer:** 80 bytes (PROGMEM flash-optimized format string buffer).
- **Arduino Hardware Serial Buffers:** 128 bytes (64B RX + 64B TX).
- **Compaction Stack Frame:** 416 bytes (`headers[8]` + `copy_buf[256]` + local frame variables).
- **Peak Worst-Case SRAM Consumption:** ~951 bytes (leaving over 1 KB SRAM headroom on ATmega328P).

## Physical EEPROM vs Mock Measurement
- **Host Mock:** Measures exact physical byte write counts per cell via the `write_counts` tracking array.
- **AVR Hardware:** Uses `eeprom_update_byte()`, which reads the cell first and only performs a physical write if the value has changed. Wear statistics on hardware are estimated mathematically from the superblock generation count.\n