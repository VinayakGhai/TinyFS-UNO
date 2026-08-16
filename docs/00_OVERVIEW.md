# TinyFS-UNO Overview

TinyFS-UNO is a crash-consistent, wear-aware, integrity-checked miniature filesystem designed specifically for the Arduino Uno's internal 1 KB EEPROM.

The project demonstrates how reliable storage engineering can be achieved on extremely constrained hardware:
- **Persistent Storage:** 1024 bytes (internal EEPROM)
- **RAM Capacity:** 2048 bytes (internal SRAM)
- **CPU:** ATmega328P (8-bit RISC, 16 MHz)

## Core Features
1. **Symmetrical Log-Structured Layout:** Eliminates centralized metadata tables to distribute wear evenly.
2. **Ping-Pong Sectoring:** Splits EEPROM into Sector A and Sector B. Compaction migrates files atomically from one to the other.
3. **Double Superblock Pair:** Uses alternate metadata slots with monotonically increasing generation numbers to select the active sector.
4. **Crash Consistency:** Implements a single-pass write transaction with trailing commit markers and payload CRC-16 checks.
5. **Ultra-low RAM Footprint:** Requires only 20 bytes of static RAM at runtime.\n