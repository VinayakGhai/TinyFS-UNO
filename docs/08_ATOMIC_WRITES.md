# Atomic File Writes

An update to a file must never leave the filesystem in a half-written, corrupt state.

## Sequence of a Write Transaction
1. The filesystem locates the current `write_ptr`.
2. The `RecordHeader` (with status = `0xAA` or `0xDD`) is written.
3. The file payload data bytes are written.
4. The big-endian CRC-16 of the payload is written.
5. The trailing `commit_marker = 0x55` is written to finalize the transaction.

## Safety Guarantee
- If power fails at step 2, 3, or 4, the commit marker remains `0xFF` or the CRC fails.
- During mount scanning, any record that fails CRC validation or lacks the `0x55` marker is ignored. The filesystem rolls back to the previous valid record.\n