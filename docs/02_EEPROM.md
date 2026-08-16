# EEPROM Hardware Characteristics

EEPROM (Electrically Erasable Programmable Read-Only Memory) is the primary non-volatile storage on the ATmega328P.

## Key Hardware Invariants
1. **Byte-level Addressability:** Unlike Flash memory, which must be erased in pages (typically 64 or 128 bytes), EEPROM allows reading and writing individual bytes.
2. **Write Performance:** Writing a byte in EEPROM takes approximately 3.3 milliseconds. Reading takes only 4 CPU clock cycles.
3. **Endurance Limits:** Every write cycle causes physical wear to the oxide layer of the EEPROM cell. After ~100,000 writes, the cell may fail to retain data.
4. **AVR update() vs write():** Writing a byte of the same value causes physical wear on standard drivers. The AVR EEPROM library provides `EEPROM.update()`, which reads the cell first and only writes if the value has changed.\n