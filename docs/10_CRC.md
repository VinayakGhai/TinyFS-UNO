# Cyclic Redundancy Checks (CRC)

CRC algorithms calculate a checksum over a data buffer to detect corruption.

## CRC Implementations
- **CRC-8:** SMBus polynomial `0x07` (\(x^8 + x^2 + x + 1\)), initial value `0x00`. Used to protect Superblocks, Sector Headers, and Record Headers.
- **CRC-16:** CRC-16-CCITT polynomial `0x1021` (\(x^{16} + x^{12} + x^5 + 1\)), initial value `0xFFFF`. Used to protect the data payload.

## Flash-optimized Bit-by-Bit Loop
To avoid using 768 bytes of program flash for lookup tables on the ATmega328P, we calculate CRCs bit-by-bit. While slightly slower, it has zero RAM overhead and fits in a few instructions.\n