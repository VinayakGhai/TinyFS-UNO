# Wear Leveling Strategy

TinyFS-UNO ensures that no single EEPROM cell is repeatedly rewritten.

## Mechanism
1. **Sequential Logging:** File updates append new records at the current `write_ptr` rather than overwriting old ones. Wear is distributed evenly.
2. **Metadata Alternation:** The active superblock alternates between SB1 and SB2 on each sector switch (compaction).
3. **Sector Ping-Pong:** Compaction alternates active sectors. Sector A is used, then compacted to Sector B, then B to A, distributing wear evenly across the two halves of the EEPROM.
4. **Targeted Header Updates:** Sector state updates (e.g. from BUILDING to VALID) only write the state and CRC bytes, saving other cells.\n