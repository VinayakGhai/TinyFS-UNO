# Performance Benchmarking

TinyFS-UNO implements a self-benchmarking system.

## Metrics Evaluated
1. **Logical Writes:** User write requests.
2. **Logical Payload Bytes:** Size of raw user data written.
3. **Physical Bytes Written:** Actual bytes written to EEPROM.
4. **Write Amplification (WA):** Ratio of physical bytes written to logical payload bytes.
5. **Compactions:** Number of sector compactions triggered.

## Diagnostic Insights
Under compaction load, write amplification rises because active files must be copied to the new sector. Benchmarking shows the actual overhead of logging, transaction metadata, and compaction.\n