# Design Decisions & Trade-offs

During development, we resolved several design challenges:

1. **No Filenames in RAM:** Instead of storing strings in SRAM, we only keep 2-byte offsets. Filename comparisons read directly from EEPROM. RAM usage drops from ~120 bytes to 20 bytes.
2. **No Table-based CRC:** Saved ~768 bytes of Flash by using bit-by-bit loops instead of lookup tables.
3. **Superblock Stats Removal:** Removed logical/physical counters from the persistent superblock to prevent wear-out from statistics writes. Stats are runtime-only.
4. **Double Superblocks:** Used two slots to prevent corrupting filesystem metadata if power cuts during superblock updates.\n