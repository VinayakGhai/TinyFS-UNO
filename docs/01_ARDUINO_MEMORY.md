# Arduino Memory Architecture

The ATmega328P uses a Harvard architecture, meaning program instructions and data are stored in separate memory spaces.

## Memory Spaces
1. **Flash Memory (32 KB):**
   - Non-volatile storage for program code.
   - Can only be written in blocks (pages). Slow to rewrite during execution.
   - Limited to ~10,000 write cycles.
2. **SRAM (2 KB):**
   - Volatile memory for active program state, stack, and heap.
   - Unlimited reads and writes.
   - Extremely small: a few large arrays can easily exhaust RAM, causing stack overflows.
3. **EEPROM (1 KB):**
   - Non-volatile byte-addressable memory.
   - Can be read and written byte-by-byte at runtime.
   - Limited to ~100,000 write cycles.
   - Takes ~3.3ms to write a single byte.

## Memory Allocation Strategy in TinyFS-UNO
Due to the 2 KB SRAM limit, TinyFS-UNO avoids any dynamic allocation (`malloc` or `new`). We use a static context structure that tracks only the 2-byte offsets of active files. Filenames are read and compared directly from EEPROM on the fly.\n