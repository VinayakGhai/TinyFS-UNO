# Testing Architecture

We split testing into Host-side testing and Arduino-side testing.

## Host Tests (tests/test_tinyfs.c)
- Compiles with standard `gcc`.
- Simulates the 1024-byte EEPROM in RAM.
- Emulates physical write counts to inspect wear.
- Implements a write-interruption limit to test recovery after power loss on every byte boundary.

## Arduino Tests (examples/TinyFS_CLI)
- Validates real hardware timing.
- Verifies RAM consumption.
- Tests interactive serial shell.\n