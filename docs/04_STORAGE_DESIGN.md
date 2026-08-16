# Storage Abstraction Design

TinyFS-UNO separates hardware access from filesystem logic.

```
TinyFS Core (API, Mount, GC)
     ↓
Storage Abstraction (tfs_storage.c - Session metrics)
     ↓
EEPROM Backend (tfs_eeprom_avr.c / tfs_eeprom_mock.c)
```

## Abstract APIs
- `tfs_storage_read_byte(addr)` / `tfs_storage_read_buffer(addr, buf, len)`
- `tfs_storage_write_byte(addr, val)` / `tfs_storage_write_buffer(addr, buf, len)`

## Why Abstraction?
1. **Portability:** Compiles with a RAM mock buffer on host PCs, allowing desktop unit-testing, and compiles with the actual AVR library on Arduino.
2. **Telemetry:** The storage abstraction layer intercepts write calls to track logical writes, physical writes, and compute Write Amplification.\n