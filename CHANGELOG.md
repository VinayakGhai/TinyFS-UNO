# Changelog

All notable changes to this project will be documented in this file.

## [0.1.0] - 2026-08-16

### Added
- Symmetrical 1024-byte EEPROM layout (SB1, SB2, Sector A, Sector B).
- double superblock generation-based active sector selection.
- Symmetrical sector headers with BUILDING, VALID, and EMPTY states.
- Append-only sequential log records with CRC-8 header and CRC-16 data payload checks.
- 1-byte trailing commit marker (`0x55`) write transaction model.
- 20-byte SRAM static file context tracking.
- Garbage collection / compaction algorithm (Ping-Pong migration).
- Filesystem consistency checks and redundant repair routines.
- Telemetry metrics mapping and write amplification computations.
- Host-side mock storage with byte-level write counting and write-limit fault injection.
- Modular, portable testing suite.
- Interactive Arduino serial console CLI sketch.
