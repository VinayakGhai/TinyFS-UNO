# Contributing to TinyFS-UNO

Contributions are welcome! Since this is an educational project, we prioritize clarity, documentation, testing, and resource efficiency.

## Guidelines
1. **Low-Level C-Style:** The filesystem core (`src/`) must be written in a low-level C-style. Avoid heavy modern abstractions, templates, or dynamic allocations.
2. **Deterministic Memory:** Keep the runtime RAM overhead static (bounded).
3. **No Magic Numbers:** Define layout addresses and configuration constants in `src/tinyfs_config.h`. Include static compile-time assertions where appropriate.
4. **Host Testability:** Ensure all changes to the filesystem logic compile and pass host unit tests (`tests/test_tinyfs.c`).
5. **Detailed Documentation:** Update `docs/` and code comments to explain *why* code functions as it does, detailing invariants and failure recovery.
