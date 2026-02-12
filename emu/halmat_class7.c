#include "halmat.h"

static double to_scalar(halmat_val_t v)
{
    switch (v.type) {
    case HTYPE_SCALAR:  return v.v.scalar;
    case HTYPE_INTEGER: return (double)v.v.integer;
    default:            return 0.0;
    }
}

static int32_t to_int(halmat_val_t v)
{
    switch (v.type) {
    case HTYPE_INTEGER: return v.v.integer;
    case HTYPE_SCALAR:  return (int32_t)v.v.scalar;
    case HTYPE_BIT:     return (int32_t)v.v.bits;
    default:            return 0;
    }
}

int halmat_exec_class7(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag)
{
    uint32_t pc = H->pc;
    halmat_val_t result = {0};
    result.type = HTYPE_INTEGER;

    switch (popcode) {


    case POP_IEQU: {
        if (numop < 2) break;
        int32_t a = to_int(halmat_resolve_operand(H, H->code[pc + 1]));
        int32_t b = to_int(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a == b) ? 1 : 0;
        break;
    }

    case POP_INEQ: {
        if (numop < 2) break;
        int32_t a = to_int(halmat_resolve_operand(H, H->code[pc + 1]));
        int32_t b = to_int(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a != b) ? 1 : 0;
        break;
    }

    case POP_IGT: {
        if (numop < 2) break;
        int32_t a = to_int(halmat_resolve_operand(H, H->code[pc + 1]));
        int32_t b = to_int(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a > b) ? 1 : 0;
        break;
    }

    case POP_ILT: {
        if (numop < 2) break;
        int32_t a = to_int(halmat_resolve_operand(H, H->code[pc + 1]));
        int32_t b = to_int(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a < b) ? 1 : 0;
        break;
    }

    case POP_INGT: {
        if (numop < 2) break;
        int32_t a = to_int(halmat_resolve_operand(H, H->code[pc + 1]));
        int32_t b = to_int(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a <= b) ? 1 : 0;
        break;
    }

    case POP_INLT: {
        if (numop < 2) break;
        int32_t a = to_int(halmat_resolve_operand(H, H->code[pc + 1]));
        int32_t b = to_int(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a >= b) ? 1 : 0;
        break;
    }


    case POP_SEQU: {
        if (numop < 2) break;
        double a = to_scalar(halmat_resolve_operand(H, H->code[pc + 1]));
        double b = to_scalar(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a == b) ? 1 : 0;
        break;
    }

    case POP_SNEQ: {
        if (numop < 2) break;
        double a = to_scalar(halmat_resolve_operand(H, H->code[pc + 1]));
        double b = to_scalar(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a != b) ? 1 : 0;
        break;
    }

    case POP_SGT: {
        if (numop < 2) break;
        double a = to_scalar(halmat_resolve_operand(H, H->code[pc + 1]));
        double b = to_scalar(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a > b) ? 1 : 0;
        break;
    }

    case POP_SLT: {
        if (numop < 2) break;
        double a = to_scalar(halmat_resolve_operand(H, H->code[pc + 1]));
        double b = to_scalar(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a < b) ? 1 : 0;
        break;
    }

    case POP_SNGT: {
        if (numop < 2) break;
        double a = to_scalar(halmat_resolve_operand(H, H->code[pc + 1]));
        double b = to_scalar(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a <= b) ? 1 : 0;
        break;
    }

    case POP_SNLT: {
        if (numop < 2) break;
        double a = to_scalar(halmat_resolve_operand(H, H->code[pc + 1]));
        double b = to_scalar(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a >= b) ? 1 : 0;
        break;
    }


    case POP_BTRU: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        result.v.integer = (a.v.bits != 0) ? 1 : 0;
        break;
    }

    case POP_BEQU: {
        if (numop < 2) break;
        uint32_t a = halmat_resolve_operand(H, H->code[pc + 1]).v.bits;
        uint32_t b = halmat_resolve_operand(H, H->code[pc + 2]).v.bits;
        result.v.integer = (a == b) ? 1 : 0;
        break;
    }

    case POP_BNEQ: {
        if (numop < 2) break;
        uint32_t a = halmat_resolve_operand(H, H->code[pc + 1]).v.bits;
        uint32_t b = halmat_resolve_operand(H, H->code[pc + 2]).v.bits;
        result.v.integer = (a != b) ? 1 : 0;
        break;
    }


    case POP_CEQU: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        int cmp = strncmp(a.v.string.data, b.v.string.data,
                          a.v.string.len < b.v.string.len ?
                          a.v.string.len : b.v.string.len);
        result.v.integer = (cmp == 0 && a.v.string.len == b.v.string.len) ? 1 : 0;
        break;
    }

    case POP_CNEQ: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        int cmp = strncmp(a.v.string.data, b.v.string.data,
                          a.v.string.len < b.v.string.len ?
                          a.v.string.len : b.v.string.len);
        result.v.integer = (cmp != 0 || a.v.string.len != b.v.string.len) ? 1 : 0;
        break;
    }

    case POP_CGT: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        int cmp = strncmp(a.v.string.data, b.v.string.data, 256);
        result.v.integer = (cmp > 0) ? 1 : 0;
        break;
    }

    case POP_CLT: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        int cmp = strncmp(a.v.string.data, b.v.string.data, 256);
        result.v.integer = (cmp < 0) ? 1 : 0;
        break;
    }

    case POP_CNGT: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        int cmp = strncmp(a.v.string.data, b.v.string.data, 256);
        result.v.integer = (cmp <= 0) ? 1 : 0;
        break;
    }

    case POP_CNLT: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        int cmp = strncmp(a.v.string.data, b.v.string.data, 256);
        result.v.integer = (cmp >= 0) ? 1 : 0;
        break;
    }


    case POP_MEQU:
    case POP_VEQU:
        /* TODO: element-wise comparison */
        result.v.integer = 0;
        break;

    case POP_MNEQ:
    case POP_VNEQ:
        result.v.integer = 1;
        break;


    case POP_CAND: {
        if (numop < 2) break;
        int a = to_int(halmat_resolve_operand(H, H->code[pc + 1]));
        int b = to_int(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a && b) ? 1 : 0;
        break;
    }

    case POP_COR: {
        if (numop < 2) break;
        int a = to_int(halmat_resolve_operand(H, H->code[pc + 1]));
        int b = to_int(halmat_resolve_operand(H, H->code[pc + 2]));
        result.v.integer = (a || b) ? 1 : 0;
        break;
    }

    case POP_CNOT: {
        if (numop < 1) break;
        int a = to_int(halmat_resolve_operand(H, H->code[pc + 1]));
        result.v.integer = (!a) ? 1 : 0;
        break;
    }

    default:
        fprintf(stderr, "halmat_class7: unknown popcode 0x%03X at PC=%u\n",
                popcode, pc);
        break;
    }

    halmat_store_vac(H, pc, result);
    H->cond_true = result.v.integer;

    (void)tag;
    H->pc = pc + numop + 1;
    return HALMAT_OK;
}
