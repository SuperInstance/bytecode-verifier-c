# bytecode-verifier-c

Pure C11 bytecode verifier for the FLUX VM ISA. Zero external dependencies. No heap allocation.

**Purpose:** Addresses [SuperInstance/flux-runtime#15](https://github.com/SuperInstance/flux-runtime/issues/15) — CRITICAL: Zero bytecode verification before execution.

## What It Does

Validates FLUX bytecode before the VM executes it, checking:

- **Format** — valid opcodes, correct instruction sizes, no truncation
- **Register references** — all register operands within bounds
- **Jump targets** — all jump/call targets within bytecode
- **Stack depth** — CALL/RET balanced, no overflow/underflow
- **Memory bounds** — LOAD/STORE addresses within memory size
- **HALT presence** — mandatory in strict mode

## Build

```sh
make          # builds libverifier.a (static library)
make test     # builds + runs tests
make clean    # remove artifacts
```

Requires: GCC (or any C11 compiler), `ar`.

## Usage

```c
#include "verifier.h"

uint8_t *bytecode = ...;
size_t len = ...;

VerifyContext ctx;
verifier_context_default(&ctx);
ctx.strict_mode = 1;  // require HALT

VerifyError err = verifier_verify(&ctx, bytecode, len);
if (err != VERIFY_OK) {
    char buf[128];
    verifier_error_report(&ctx, err, 0, buf, sizeof(buf));
    fprintf(stderr, "Verification failed: %s\n", buf);
}

// With stats:
VerifyStats stats;
err = verifier_verify_with_stats(&ctx, bytecode, len, &stats);
printf("Instructions: %d, max stack: %d, has halt: %d\n",
       stats.total_instructions, stats.max_stack_depth, stats.has_halt);
```

## Hot Path

For fast pre-filtering:

```c
VerifyError err = verifier_quick_check(bytecode, len);
if (err != VERIFY_OK) return;  // reject immediately
```

## Supported Opcodes

| Range | Opcodes |
|-------|---------|
| `0x00-0x05` | HALT, NOP, LOAD, STORE, MOV, SWAP |
| `0x10-0x13` | ADD, SUB, MUL, DIV |
| `0x20-0x21` | PUSH, POP |
| `0x30-0x34` | JMP, JZ, JNZ, CALL, RET |
| `0x40-0x44` | CMP, EQ, NE, LT, GT |
| `0x50-0x51` | TELL, ASK |
| `0x60-0x61` | CONF_SET, CONF_FUSE |
| `0x70-0x71` | ENERGY_READ, ENERGY_BURN |
| `0x80-0x81` | LOOP, WAIT |
| `0xFE` | EXTENDED (2-byte escape) |
| `0xFF` | RESERVED (stop marker) |

## License

MIT
