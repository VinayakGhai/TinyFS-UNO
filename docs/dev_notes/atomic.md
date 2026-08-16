# Atomic Write Notes
- **Problem:** Power loss mid-write causes corruption.
- **Why it exists:** To ensure filesystem transitions are all-or-nothing.
- **Naive solution:** Write data directly.
- **Why naive fails:** A power cut leaves the file half-written and corrupts the format.
- **Our design:** Write the record with payload CRC and write the commit marker last.
- **RAM consumed:** 0 bytes.
- **EEPROM writes:** 0 extra writes.\n