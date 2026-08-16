# TinyFS-UNO Limitations

TinyFS-UNO v0.1 has several intentional design limitations:

1. **No Directories:** Only supports a flat namespace (max 8 files).
2. **Max File Size:** Files must fit in a single sector, meaning they cannot exceed 256 bytes.
3. **No Multi-sector Fragmentation:** Fragmentation is solved by compaction, which moves entire files. Files are contiguous.
4. **Synchronous Writes:** Writes block execution for ~3.3ms per byte.\n