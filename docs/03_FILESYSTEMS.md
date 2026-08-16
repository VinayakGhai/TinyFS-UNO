# Embedded Filesystems

Filesystems map logical files (filenames, content stream) to physical storage addresses.

## Challenges on Constraints
- **Fragmentation:** Deleting and writing files of varying sizes leaves gaps. Traditional filesystems use block allocation maps, which consume too much metadata space.
- **Wear Concentration:** Updating files in place repeatedly rewrites the same address (e.g. FAT allocation tables), quickly wearing out those cells.
- **Crash Recovery:** If power is cut mid-write, traditional metadata tables can become corrupt, losing all data.

## Log-Structured Filesystem (LFS)
TinyFS-UNO is inspired by LFS designs. We treat the active sector as a continuous sequential log. Updates are always appended to the end of the log rather than modifying files in place. This translates writes into sequential actions, providing wear-leveling and crash safety.\n