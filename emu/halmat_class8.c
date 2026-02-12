#include "halmat.h"

int halmat_exec_class8(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag)
{
    uint32_t pc = H->pc;

    switch (popcode) {

    case POP_IINT: {
        if (numop < 2) break;
        uint32_t dest = HALMAT_DATA(H->code[pc + 1]);
        halmat_val_t src = halmat_resolve_operand(H, H->code[pc + 2]);
        if (dest < HALMAT_MAX_SYT) {
            H->syt[dest].val.type = HTYPE_INTEGER;
            /* literals are IBM float */
            if (src.type == HTYPE_SCALAR)
                H->syt[dest].val.v.integer = (int32_t)src.v.scalar;
            else
                H->syt[dest].val.v.integer = src.v.integer;
            H->syt[dest].allocated = 1;
        }
        break;
    }

    case POP_SINT: {
        if (numop < 2) break;
        uint32_t dest = HALMAT_DATA(H->code[pc + 1]);
        halmat_val_t src = halmat_resolve_operand(H, H->code[pc + 2]);
        if (dest < HALMAT_MAX_SYT) {
            H->syt[dest].val.type = HTYPE_SCALAR;
            if (src.type == HTYPE_INTEGER)
                H->syt[dest].val.v.scalar = (double)src.v.integer;
            else
                H->syt[dest].val.v.scalar = src.v.scalar;
            H->syt[dest].allocated = 1;
        }
        break;
    }

    case POP_CINT: {
        if (numop < 2) break;
        uint32_t dest = HALMAT_DATA(H->code[pc + 1]);
        halmat_val_t src = halmat_resolve_operand(H, H->code[pc + 2]);
        if (dest < HALMAT_MAX_SYT) {
            H->syt[dest].val.type = HTYPE_CHAR;
            memcpy(H->syt[dest].val.v.string.data, src.v.string.data,
                   src.v.string.len);
            H->syt[dest].val.v.string.len = src.v.string.len;
            H->syt[dest].allocated = 1;
        }
        break;
    }

    case POP_BINT: {
        if (numop < 2) break;
        uint32_t dest = HALMAT_DATA(H->code[pc + 1]);
        halmat_val_t src = halmat_resolve_operand(H, H->code[pc + 2]);
        if (dest < HALMAT_MAX_SYT) {
            H->syt[dest].val.type = HTYPE_BIT;
            H->syt[dest].val.v.bits = src.v.bits;
            H->syt[dest].allocated = 1;
        }
        break;
    }

    case POP_MINT: {
        /* TODO */
        break;
    }

    case POP_VINT: {
        /* TODO */
        break;
    }

    case POP_NINT:
    case POP_TINT:
    case POP_EINT:
        /* TODO */
        break;

    case POP_STRI:
    case POP_SLRI:
    case POP_ELRI:
    case POP_ETRI:
        break;

    default:
        fprintf(stderr, "halmat_class8: unknown popcode 0x%03X at PC=%u\n",
                popcode, pc);
        break;
    }

    (void)tag;
    H->pc = pc + numop + 1;
    return HALMAT_OK;
}
