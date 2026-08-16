# EEPROM Physical Layout

The 1024-byte EEPROM space is mapped symmetrically:

```
Address
0x000 ┌───────────────────┐
      │ Superblock 1 (16B)│
0x010 ├───────────────────┤
      │ Superblock 2 (16B)│
0x020 ├───────────────────┤
      │ Sector A (496B)   │  ← Header (8B) + Log Area (488B)
      │                   │
0x210 ├───────────────────┤
      │ Sector B (496B)   │  ← Header (8B) + Log Area (488B)
      │                   │
0x400 └───────────────────┘
```

## Structure Field Budgets
- **Superblock:** Magic (2B) + Generation (4B) + Active Sector ID (1B) + Padding (8B) + CRC-8 (1B) = 16 bytes.
- **Sector Header:** Magic (1B) + Sector ID (1B) + Generation (4B) + State (1B) + CRC-8 (1B) = 8 bytes.
- **Active Record Header:** Status (1B) + Filename (12B) + Length (2B) + CRC-8 (1B) = 16 bytes.
- **Log Area size:** 488 bytes.
- **Maximum file payload:** 256 bytes.\n