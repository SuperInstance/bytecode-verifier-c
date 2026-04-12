#ifndef VERIFIER_H
#define VERIFIER_H

#include <stdint.h>
#include <stddef.h>

// Verification result
typedef enum {
    VERIFY_OK = 0,
    VERIFY_ERR_UNKNOWN_OPCODE,
    VERIFY_ERR_INVALID_FORMAT,
    VERIFY_ERR_REGISTER_OUT_OF_RANGE,
    VERIFY_ERR_JUMP_OUT_OF_BOUNDS,
    VERIFY_ERR_STACK_OVERFLOW,
    VERIFY_ERR_STACK_UNDERFLOW,
    VERIFY_ERR_MEMORY_OUT_OF_BOUNDS,
    VERIFY_ERR_MISALIGNED_INSTRUCTION,
    VERIFY_ERR_UNREACHABLE_CODE,
    VERIFY_ERR_MISSING_HALT,
    VERIFY_ERR_INVALID_IMMEDIATE,
    VERIFY_ERR_DIVISION_BY_ZERO_PATH,
    VERIFY_ERR_LOOP_NEGATIVE_COUNT,
    VERIFY_ERR_EMPTY_PROGRAM,
    VERIFY_ERR_PROGRAM_TOO_LONG
} VerifyError;

// Verification stats
typedef struct {
    int total_instructions;
    int opcode_count[256];  // histogram
    int max_stack_depth;
    int max_memory_address;
    int jump_count;
    int call_count;
    int has_halt;
} VerifyStats;

// Verification context
typedef struct {
    const uint8_t *bytecode;
    size_t bytecode_len;
    int num_registers;      // default 64
    size_t memory_size;     // default 65536 (64KB)
    int max_stack_depth;    // default 256
    int strict_mode;        // 0 = permissive, 1 = strict (reject unreachable code)
} VerifyContext;

// Core API
void verifier_context_default(VerifyContext *ctx);
VerifyError verifier_verify(const VerifyContext *ctx, const uint8_t *bytecode, size_t len);
VerifyError verifier_verify_with_stats(const VerifyContext *ctx, const uint8_t *bytecode, size_t len, VerifyStats *stats);

// Error reporting
const char *verifier_error_name(VerifyError err);
void verifier_error_report(const VerifyContext *ctx, VerifyError err, size_t offset, char *buf, size_t bufsz);

// Quick check (for hot path — skip full analysis, just format validation)
VerifyError verifier_quick_check(const uint8_t *bytecode, size_t len);

// Opcode info
int verifier_is_valid_opcode(uint8_t opcode);
int verifier_opcode_size(uint8_t opcode);  // returns instruction size in bytes, -1 if invalid
const char *verifier_opcode_name(uint8_t opcode);

// Bounds checking helpers
int verifier_register_valid(const VerifyContext *ctx, uint8_t reg);
int verifier_memory_valid(const VerifyContext *ctx, uint32_t addr);
int verifier_jump_valid(const VerifyContext *ctx, uint32_t target, size_t current_pc);

#endif
