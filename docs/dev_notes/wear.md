# Wear-Leveling Notes
- **Problem:** EEPROM cells fail after 100,000 writes.
- **Why it exists:** To prevent early device death due to hot spots.
- **Naive solution:** Maintain a physical counter for each byte.
- **Why naive fails:** Storing the counter wears out the EEPROM cells storing it!
- **Our design:** Sequential logging naturally wear levels; host mock tracks exact writes; Arduino maps wear estimates from generation number.
- **RAM consumed:** 0 bytes.
- **EEPROM writes:** 0 writes.\n