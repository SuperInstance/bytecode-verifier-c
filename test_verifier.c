/*
 * test_verifier.c — Tests for bytecode-verifier-c
 *
 * Compile: gcc -std=c11 -Wall -Wextra -pedantic -o test_verifier test_verifier.c verifier.c
 * Run:     ./test_verifier
 */

#include "verifier.h"
#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) do { \
    tests_run++; \
    if ((a) != (b)) { \
        printf("  FAIL: %s — got %d, expected %d (line %d)\n", msg, (int)(a), (int)(b), __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_NEQ(a, b, msg) do { \
    tests_run++; \
    if ((a) == (b)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

/* Test 1: valid empty program (just HALT) passes */
static void test_valid_halt(void) {
    printf("Test 1: valid HALT program\n");
    uint8_t bc[] = { 0x00 };
    VerifyContext ctx;
    verifier_context_default(&ctx);
    ASSERT_EQ(verifier_verify(&ctx, bc, sizeof(bc)), VERIFY_OK, "HALT should pass");
}

/* Test 2: invalid opcode detected */
static void test_invalid_opcode(void) {
    printf("Test 2: invalid opcode\n");
    uint8_t bc[] = { 0x90 };
    VerifyContext ctx;
    verifier_context_default(&ctx);
    ASSERT_EQ(verifier_verify(&ctx, bc, sizeof(bc)), VERIFY_ERR_UNKNOWN_OPCODE, "invalid opcode");
}

/* Test 3: register out of range */
static void test_register_out_of_range(void) {
    printf("Test 3: register out of range\n");
    /* MOV rd=200, rs=0 — 200 >= 64 (default num_registers) */
    uint8_t bc[] = { 0x04, 200, 0x00, 0x00 };
    VerifyContext ctx;
    verifier_context_default(&ctx);
    ASSERT_EQ(verifier_verify(&ctx, bc, sizeof(bc)), VERIFY_ERR_REGISTER_OUT_OF_RANGE, "reg OOB");
}

/* Test 4: jump out of bounds */
static void test_jump_out_of_bounds(void) {
    printf("Test 4: jump out of bounds\n");
    /* JMP to offset 0xFFFF, program is only 2 bytes */
    uint8_t bc[] = { 0x30, 0xFF, 0xFF };
    VerifyContext ctx;
    verifier_context_default(&ctx);
    ASSERT_EQ(verifier_verify(&ctx, bc, sizeof(bc)), VERIFY_ERR_JUMP_OUT_OF_BOUNDS, "jump OOB");
}

/* Test 5: stack overflow on deep CALL */
static void test_stack_overflow(void) {
    printf("Test 5: stack overflow\n");
    /* 257 CALLs with max_stack_depth=256 */
    uint8_t bc[2 * 257 + 1];
    size_t i;
    for (i = 0; i < 257; i++) {
        bc[i * 2] = 0x33;     /* CALL */
        bc[i * 2 + 1] = 0x00; /* offset 0 */
    }
    bc[i * 2] = 0x00; /* HALT */
    VerifyContext ctx;
    verifier_context_default(&ctx);
    ctx.max_stack_depth = 256;
    ASSERT_EQ(verifier_verify(&ctx, bc, sizeof(bc)), VERIFY_ERR_STACK_OVERFLOW, "stack overflow");
}

/* Test 6: stack underflow on RET */
static void test_stack_underflow(void) {
    printf("Test 6: stack underflow\n");
    uint8_t bc[] = { 0x34, 0x00 };
    VerifyContext ctx;
    verifier_context_default(&ctx);
    ASSERT_EQ(verifier_verify(&ctx, bc, sizeof(bc)), VERIFY_ERR_STACK_UNDERFLOW, "stack underflow");
}

/* Test 7: memory out of bounds */
static void test_memory_out_of_bounds(void) {
    printf("Test 7: memory out of bounds\n");
    /* LOAD reg=0, addr=0xFFFF (65535) — memory_size=65536, so 0xFFFF is valid */
    /* Use addr=0x10000 to go OOB — wait, imm16 max is 0xFFFF. Need smaller memory_size. */
    uint8_t bc[] = { 0x02, 0x00, 0xFF, 0xFF };
    VerifyContext ctx;
    verifier_context_default(&ctx);
    ctx.memory_size = 1024;
    ASSERT_EQ(verifier_verify(&ctx, bc, sizeof(bc)), VERIFY_ERR_MEMORY_OUT_OF_BOUNDS, "memory OOB");
}

/* Test 8: misaligned instruction (truncated) */
static void test_misaligned_instruction(void) {
    printf("Test 8: misaligned/truncated instruction\n");
    /* LOAD needs 3 bytes, give only 2 */
    uint8_t bc[] = { 0x02, 0x00 };
    VerifyContext ctx;
    verifier_context_default(&ctx);
    ASSERT_EQ(verifier_verify(&ctx, bc, sizeof(bc)), VERIFY_ERR_INVALID_FORMAT, "truncated");
}

/* Test 9: missing HALT in strict mode */
static void test_missing_halt_strict(void) {
    printf("Test 9: missing HALT strict\n");
    uint8_t bc[] = { 0x01, 0x01 }; /* two NOPs */
    VerifyContext ctx;
    verifier_context_default(&ctx);
    ctx.strict_mode = 1;
    ASSERT_EQ(verifier_verify(&ctx, bc, sizeof(bc)), VERIFY_ERR_MISSING_HALT, "missing HALT strict");
}

/* Test 10: missing HALT allowed in permissive mode */
static void test_missing_halt_permissive(void) {
    printf("Test 10: missing HALT permissive\n");
    uint8_t bc[] = { 0x01, 0x01 };
    VerifyContext ctx;
    verifier_context_default(&ctx);
    ASSERT_EQ(verifier_verify(&ctx, bc, sizeof(bc)), VERIFY_OK, "missing HALT ok in permissive");
}

/* Test 11: quick_check passes valid bytecode */
static void test_quick_check_pass(void) {
    printf("Test 11: quick_check pass\n");
    uint8_t bc[] = { 0x00 };
    ASSERT_EQ(verifier_quick_check(bc, sizeof(bc)), VERIFY_OK, "quick_check pass");
}

/* Test 12: quick_check rejects bad opcode */
static void test_quick_check_fail(void) {
    printf("Test 12: quick_check fail\n");
    uint8_t bc[] = { 0x90 };
    ASSERT_EQ(verifier_quick_check(bc, sizeof(bc)), VERIFY_ERR_UNKNOWN_OPCODE, "quick_check fail");
}

/* Test 13: stats track instruction count */
static void test_stats_instruction_count(void) {
    printf("Test 13: stats instruction count\n");
    uint8_t bc[] = { 0x01, 0x01, 0x00 }; /* NOP, NOP, HALT */
    VerifyContext ctx;
    verifier_context_default(&ctx);
    VerifyStats stats;
    memset(&stats, 0, sizeof(stats));
    ASSERT_EQ(verifier_verify_with_stats(&ctx, bc, sizeof(bc), &stats), VERIFY_OK, "verify");
    ASSERT_EQ(stats.total_instructions, 3, "count should be 3");
}

/* Test 14: stats detect has_halt */
static void test_stats_has_halt(void) {
    printf("Test 14: stats has_halt\n");
    uint8_t bc[] = { 0x01, 0x00 };
    VerifyContext ctx;
    verifier_context_default(&ctx);
    VerifyStats stats;
    memset(&stats, 0, sizeof(stats));
    verifier_verify_with_stats(&ctx, bc, sizeof(bc), &stats);
    ASSERT_EQ(stats.has_halt, 1, "has_halt should be 1");
}

/* Test 15: opcode_name returns non-null for valid opcode */
static void test_opcode_name_valid(void) {
    printf("Test 15: opcode_name non-null\n");
    const char *n = verifier_opcode_name(0x00);
    ASSERT(n != NULL && n[0] != '\0', "HALT name should be non-empty");
    n = verifier_opcode_name(0x10);
    ASSERT(strcmp(n, "ADD") == 0, "0x10 should be ADD");
}

/* Test 16: verifier_opcode_size returns correct sizes */
static void test_opcode_size(void) {
    printf("Test 16: opcode_size correct\n");
    ASSERT_EQ(verifier_opcode_size(0x00), 1, "HALT size 1");
    ASSERT_EQ(verifier_opcode_size(0x01), 1, "NOP size 1");
    ASSERT_EQ(verifier_opcode_size(0x04), 2, "MOV size 2");
    ASSERT_EQ(verifier_opcode_size(0x02), 3, "LOAD size 3");
    ASSERT_EQ(verifier_opcode_size(0x30), 2, "JMP size 2");
    ASSERT_EQ(verifier_opcode_size(0x33), 2, "CALL size 2");
    ASSERT_EQ(verifier_opcode_size(0x90), -1, "invalid opcode -1");
}

/* Test 17: null context uses defaults */
static void test_null_context(void) {
    printf("Test 17: NULL context uses defaults\n");
    uint8_t bc[] = { 0x00 };
    ASSERT_EQ(verifier_verify(NULL, bc, sizeof(bc)), VERIFY_OK, "NULL ctx ok");
}

/* Test 18: empty bytecode */
static void test_empty_bytecode(void) {
    printf("Test 18: empty bytecode\n");
    ASSERT_EQ(verifier_quick_check(NULL, 0), VERIFY_ERR_EMPTY_PROGRAM, "empty");
    ASSERT_EQ(verifier_verify(NULL, (uint8_t[]){0}, 0), VERIFY_ERR_EMPTY_PROGRAM, "empty verify");
}

int main(void) {
    printf("=== bytecode-verifier-c tests ===\n\n");

    test_valid_halt();
    test_invalid_opcode();
    test_register_out_of_range();
    test_jump_out_of_bounds();
    test_stack_overflow();
    test_stack_underflow();
    test_memory_out_of_bounds();
    test_misaligned_instruction();
    test_missing_halt_strict();
    test_missing_halt_permissive();
    test_quick_check_pass();
    test_quick_check_fail();
    test_stats_instruction_count();
    test_stats_has_halt();
    test_opcode_name_valid();
    test_opcode_size();
    test_null_context();
    test_empty_bytecode();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
