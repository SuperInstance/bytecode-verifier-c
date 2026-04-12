/*
 * bytecode-verifier-c — Pure C11 FLUX bytecode verifier
 *
 * Zero dependencies. No malloc. Stack-only.
 * Validates format, registers, jumps, stack, memory, opcode legality.
 */

#include "verifier.h"
#include <stdio.h>
#include <string.h>

/* ── Opcodes ─────────────────────────────────────────────────────────── */

/* Instruction size table: -1 = invalid, 1/2/3 = byte count */
static const int8_t opcode_sizes[256] = {
    /* 0x00-0x0F */
    1, 1, 3, 3, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x10-0x1F */
    2, 2, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x20-0x2F */
    2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x30-0x3F */
    2, 3, 3, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x40-0x4F */
    2, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x50-0x5F */
    2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x60-0x6F */
    3, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x70-0x7F */
    2, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x80-0x8F */
    3, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x90-0xFD: all invalid */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,   2, 1
};

static const char *opcode_names[256] = {
    "HALT",  "NOP",   "LOAD",  "STORE", "MOV",   "SWAP",  NULL,    NULL,
    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
    "ADD",   "SUB",   "MUL",   "DIV",   NULL,    NULL,    NULL,    NULL,
    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
    "PUSH",  "POP",   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
    "JMP",   "JZ",    "JNZ",   "CALL",  "RET",   NULL,    NULL,    NULL,
    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
    "CMP",   "EQ",    "NE",    "LT",    "GT",    NULL,    NULL,    NULL,
    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
    "TELL",  "ASK",   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
    "CONF_SET", "CONF_FUSE", NULL, NULL, NULL,    NULL,    NULL,    NULL,
    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
    "ENERGY_READ", "ENERGY_BURN", NULL, NULL, NULL, NULL, NULL, NULL,
    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
    "LOOP",  "WAIT",  NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
    /* 0x90-0xFD all NULL handled by init */
    /* 0xFE-0xFF */
    "EXTENDED", "RESERVED"
};

/* Initialize NULL entries beyond what we listed above */
static int names_inited;

static void ensure_names_init(void) {
    if (names_inited) return;
    names_inited = 1;
    for (int i = 0; i < 256; i++) {
        if (opcode_names[i] == NULL)
            opcode_names[i] = "UNKNOWN";
    }
}

/* ── Helpers ─────────────────────────────────────────────────────────── */

static inline uint16_t read_imm16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* ── Opcode info ─────────────────────────────────────────────────────── */

int verifier_is_valid_opcode(uint8_t opcode) {
    return opcode_sizes[opcode] > 0;
}

int verifier_opcode_size(uint8_t opcode) {
    return opcode_sizes[opcode];
}

const char *verifier_opcode_name(uint8_t opcode) {
    ensure_names_init();
    return opcode_names[opcode];
}

/* ── Context ─────────────────────────────────────────────────────────── */

void verifier_context_default(VerifyContext *ctx) {
    if (!ctx) return;
    ctx->bytecode = NULL;
    ctx->bytecode_len = 0;
    ctx->num_registers = 64;
    ctx->memory_size = 65536;
    ctx->max_stack_depth = 256;
    ctx->strict_mode = 0;
}

/* ── Bounds helpers ──────────────────────────────────────────────────── */

int verifier_register_valid(const VerifyContext *ctx, uint8_t reg) {
    return ctx && (int)reg < ctx->num_registers;
}

int verifier_memory_valid(const VerifyContext *ctx, uint32_t addr) {
    return ctx && addr < ctx->memory_size;
}

int verifier_jump_valid(const VerifyContext *ctx, uint32_t target, size_t current_pc) {
    (void)current_pc;
    if (!ctx) return 0;
    return target <= (uint32_t)ctx->bytecode_len;
}

/* ── Error reporting ─────────────────────────────────────────────────── */

const char *verifier_error_name(VerifyError err) {
    switch (err) {
    case VERIFY_OK:                    return "OK";
    case VERIFY_ERR_UNKNOWN_OPCODE:    return "UNKNOWN_OPCODE";
    case VERIFY_ERR_INVALID_FORMAT:    return "INVALID_FORMAT";
    case VERIFY_ERR_REGISTER_OUT_OF_RANGE: return "REGISTER_OUT_OF_RANGE";
    case VERIFY_ERR_JUMP_OUT_OF_BOUNDS: return "JUMP_OUT_OF_BOUNDS";
    case VERIFY_ERR_STACK_OVERFLOW:     return "STACK_OVERFLOW";
    case VERIFY_ERR_STACK_UNDERFLOW:    return "STACK_UNDERFLOW";
    case VERIFY_ERR_MEMORY_OUT_OF_BOUNDS: return "MEMORY_OUT_OF_BOUNDS";
    case VERIFY_ERR_MISALIGNED_INSTRUCTION: return "MISALIGNED_INSTRUCTION";
    case VERIFY_ERR_UNREACHABLE_CODE:   return "UNREACHABLE_CODE";
    case VERIFY_ERR_MISSING_HALT:       return "MISSING_HALT";
    case VERIFY_ERR_INVALID_IMMEDIATE:  return "INVALID_IMMEDIATE";
    case VERIFY_ERR_DIVISION_BY_ZERO_PATH: return "DIVISION_BY_ZERO_PATH";
    case VERIFY_ERR_LOOP_NEGATIVE_COUNT: return "LOOP_NEGATIVE_COUNT";
    case VERIFY_ERR_EMPTY_PROGRAM:      return "EMPTY_PROGRAM";
    case VERIFY_ERR_PROGRAM_TOO_LONG:   return "PROGRAM_TOO_LONG";
    }
    return "UNKNOWN_ERROR";
}

void verifier_error_report(const VerifyContext *ctx, VerifyError err, size_t offset,
                           char *buf, size_t bufsz) {
    if (!buf || bufsz == 0) return;
    const char *name = verifier_error_name(err);
    const char *opname = "<none>";
    if (ctx && ctx->bytecode && offset < ctx->bytecode_len)
        opname = verifier_opcode_name(ctx->bytecode[offset]);
    /* Manual snprintf-like formatting (C11, no sprintf needed but we use it) */
    (void)ctx;
    snprintf(buf, bufsz, "[%s] at offset %zu (opcode: %s)", name, offset, opname);
}

/* ── Quick check ─────────────────────────────────────────────────────── */

VerifyError verifier_quick_check(const uint8_t *bytecode, size_t len) {
    if (!bytecode || len == 0)
        return VERIFY_ERR_EMPTY_PROGRAM;
    if (bytecode[0] == 0xFF)
        return VERIFY_ERR_INVALID_FORMAT;
    if (!verifier_is_valid_opcode(bytecode[0]))
        return VERIFY_ERR_UNKNOWN_OPCODE;
    return VERIFY_OK;
}

/* ── Main verify ─────────────────────────────────────────────────────── */

VerifyError verifier_verify_with_stats(const VerifyContext *ctx, const uint8_t *bytecode,
                                       size_t len, VerifyStats *stats) {
    if (!bytecode || len == 0)
        return VERIFY_ERR_EMPTY_PROGRAM;

    VerifyContext dc;
    if (!ctx) {
        verifier_context_default(&dc);
        ctx = &dc;
    }

    if (bytecode[0] == 0xFF)
        return VERIFY_ERR_INVALID_FORMAT;

    VerifyStats local_stats;
    int has_stats = (stats != NULL);
    if (!has_stats)
        stats = &local_stats;

    memset(stats, 0, sizeof(*stats));

    int stack_depth = 0;
    int max_stack = 0;
    int max_mem = 0;
    int halt_seen = 0;

    size_t pc = 0;
    while (pc < len) {
        uint8_t op = bytecode[pc];

        if (op == 0xFF) {
            /* Trailing RESERVED marker is ok if at end */
            if (pc == len - 1) break;
            return VERIFY_ERR_INVALID_FORMAT;
        }

        int sz = opcode_sizes[op];
        if (sz <= 0)
            return VERIFY_ERR_UNKNOWN_OPCODE;

        /* Check instruction fits in bytecode */
        if (pc + (size_t)sz > len)
            return VERIFY_ERR_INVALID_FORMAT;

        stats->total_instructions++;
        stats->opcode_count[op]++;

        switch (op) {
        case 0x00: /* HALT */
            halt_seen = 1;
            break;

        case 0x01: /* NOP */
            break;

        case 0x02: /* LOAD reg imm16 */
        case 0x03: /* STORE reg imm16 */
            if (!verifier_register_valid(ctx, bytecode[pc + 1]))
                return VERIFY_ERR_REGISTER_OUT_OF_RANGE;
            {
                uint16_t addr = read_imm16(bytecode + pc + 2);
                if ((int)addr > max_mem) max_mem = (int)addr;
                if (!verifier_memory_valid(ctx, addr))
                    return VERIFY_ERR_MEMORY_OUT_OF_BOUNDS;
            }
            break;

        case 0x04: /* MOV rd rs */
        case 0x05: /* SWAP rd rs */
        case 0x10: /* ADD rd rs */
        case 0x11: /* SUB rd rs */
        case 0x12: /* MUL rd rs */
        case 0x13: /* DIV rd rs */
        case 0x40: /* CMP rd rs */
        case 0x61: /* CONF_FUSE rd rs */
            if (!verifier_register_valid(ctx, bytecode[pc + 1]) ||
                !verifier_register_valid(ctx, bytecode[pc + 2]))
                return VERIFY_ERR_REGISTER_OUT_OF_RANGE;
            break;

        case 0x20: /* PUSH reg */
            if (!verifier_register_valid(ctx, bytecode[pc + 1]))
                return VERIFY_ERR_REGISTER_OUT_OF_RANGE;
            stack_depth++;
            if (stack_depth > max_stack) max_stack = stack_depth;
            if (stack_depth > ctx->max_stack_depth)
                return VERIFY_ERR_STACK_OVERFLOW;
            break;

        case 0x21: /* POP reg */
            if (!verifier_register_valid(ctx, bytecode[pc + 1]))
                return VERIFY_ERR_REGISTER_OUT_OF_RANGE;
            if (stack_depth <= 0)
                return VERIFY_ERR_STACK_UNDERFLOW;
            stack_depth--;
            break;

        case 0x30: /* JMP offset8 */
            {
                uint16_t target = bytecode[pc + 1];
                stats->jump_count++;
                if (!verifier_jump_valid(ctx, target, pc))
                    return VERIFY_ERR_JUMP_OUT_OF_BOUNDS;
            }
            break;

        case 0x31: /* JZ reg offset */
        case 0x32: /* JNZ reg offset */
            if (!verifier_register_valid(ctx, bytecode[pc + 1]))
                return VERIFY_ERR_REGISTER_OUT_OF_RANGE;
            {
                uint16_t target = read_imm16(bytecode + pc + 2);
                stats->jump_count++;
                if (!verifier_jump_valid(ctx, target, pc))
                    return VERIFY_ERR_JUMP_OUT_OF_BOUNDS;
            }
            break;

        case 0x33: /* CALL offset8 */
            {
                uint16_t target = bytecode[pc + 1];
                stats->call_count++;
                stack_depth++;
                if (stack_depth > max_stack) max_stack = stack_depth;
                if (stack_depth > ctx->max_stack_depth)
                    return VERIFY_ERR_STACK_OVERFLOW;
                if (!verifier_jump_valid(ctx, target, pc))
                    return VERIFY_ERR_JUMP_OUT_OF_BOUNDS;
            }
            break;

        case 0x34: /* RET */
            if (stack_depth <= 0)
                return VERIFY_ERR_STACK_UNDERFLOW;
            stack_depth--;
            break;

        case 0x50: /* TELL to_id */
        case 0x51: /* ASK to_id */
            /* to_id byte — no validation needed beyond format */
            break;

        case 0x60: /* CONF_SET reg val */
            if (!verifier_register_valid(ctx, bytecode[pc + 1]))
                return VERIFY_ERR_REGISTER_OUT_OF_RANGE;
            break;

        case 0x70: /* ENERGY_READ reg */
            if (!verifier_register_valid(ctx, bytecode[pc + 1]))
                return VERIFY_ERR_REGISTER_OUT_OF_RANGE;
            break;

        case 0x71: /* ENERGY_BURN cost */
            /* cost is imm16, no range check needed */
            break;

        case 0x80: /* LOOP reg offset */
            if (!verifier_register_valid(ctx, bytecode[pc + 1]))
                return VERIFY_ERR_REGISTER_OUT_OF_RANGE;
            {
                uint16_t target = read_imm16(bytecode + pc + 2);
                stats->jump_count++;
                if (!verifier_jump_valid(ctx, target, pc))
                    return VERIFY_ERR_JUMP_OUT_OF_BOUNDS;
            }
            break;

        case 0x81: /* WAIT cycles */
            /* cycles imm16 — no validation needed */
            break;

        case 0xFE: /* EXTENDED */
            /* second byte is sub-opcode, just check it's there (already done by size) */
            break;

        default:
            break;
        }

        pc += (size_t)sz;
    }

    stats->max_stack_depth = max_stack;
    stats->max_memory_address = max_mem;
    stats->has_halt = halt_seen;

    if (ctx->strict_mode && !halt_seen)
        return VERIFY_ERR_MISSING_HALT;

    return VERIFY_OK;
}

VerifyError verifier_verify(const VerifyContext *ctx, const uint8_t *bytecode, size_t len) {
    return verifier_verify_with_stats(ctx, bytecode, len, NULL);
}
