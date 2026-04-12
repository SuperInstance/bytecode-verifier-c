# MAINTENANCE.md — bytecode-verifier-c

## Architecture

Single-pass linear scan. No control-flow graph needed for basic verification. Stack-only — no malloc, no heap.

## Adding Opcodes

1. Update `opcode_sizes[256]` in `verifier.c` with the new opcode's byte count.
2. Add a name in `opcode_names[256]`.
3. Add validation logic in the `switch` in `verifier_verify_with_stats()`.
4. Add tests in `test_verifier.c`.

## Key Constraints

- **Zero dependencies** — only `<stdint.h>`, `<stddef.h>`, `<string.h>`.
- **No malloc** — all state on stack or caller-provided buffers.
- **C11 only** — `-std=c11 -pedantic`.
- **Thread-safe** — all functions are pure (no global mutable state except `names_inited` which is idempotent).

## Testing

Run `make test` after any change. All 18 tests must pass.

## Performance

Linear scan: O(n) where n = bytecode length. `verifier_quick_check()` is O(1) — validates only the first opcode.
