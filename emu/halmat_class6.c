#include "halmat.h"

static int32_t to_int(halmat_val_t v)
{
    switch (v.type) {
    case HTYPE_INTEGER: return v.v.integer;
    case HTYPE_SCALAR:  return (int32_t)v.v.scalar;
    case HTYPE_BIT:     return (int32_t)v.v.bits;
    default:            return 0;
    }
}

int halmat_exec_class6(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag)
{
    uint32_t pc = H->pc;

    switch (popcode) {

    case POP_IASN: {
        if (numop < 2) break;
        halmat_val_t src = halmat_resolve_operand(H, H->code[pc + 1]);
        uint32_t dest = HALMAT_DATA(H->code[pc + 2]);
        if (dest < HALMAT_MAX_SYT) {
            H->syt[dest].val.type = HTYPE_INTEGER;
            H->syt[dest].val.v.integer = to_int(src);
            H->syt[dest].allocated = 1;
        }
        break;
    }

    case POP_IADD: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.v.integer = to_int(a) + to_int(b);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_ISUB: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.v.integer = to_int(a) - to_int(b);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_IIPR: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.v.integer = to_int(a) * to_int(b);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_INEG: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.v.integer = -to_int(a);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_IPEX: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        int32_t base = to_int(a);
        int32_t exp = to_int(b);
        int32_t result = 1;
        for (int32_t i = 0; i < exp && i < 31; i++)
            result *= base;
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.v.integer = result;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_STOI: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.v.integer = to_int(a);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_BTOI: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.v.integer = (int32_t)a.v.bits;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_CTOI: {
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.v.integer = 0;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_ITOI: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.v.integer = to_int(a);
        halmat_store_vac(H, pc, r);
        break;
    }

    default:
        fprintf(stderr, "halmat_class6: unknown popcode 0x%03X at PC=%u\n",
                popcode, pc);
        break;
    }

    (void)tag;
    H->pc = pc + numop + 1;
    return HALMAT_OK;
}
