#include "halmat.h"

int halmat_exec_class1(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag)
{
    uint32_t pc = H->pc;

    switch (popcode) {

    case POP_BASN: {
        if (numop < 2) break;
        halmat_val_t src = halmat_resolve_operand(H, H->code[pc + 1]);
        uint32_t dest = HALMAT_DATA(H->code[pc + 2]);
        if (dest < HALMAT_MAX_SYT) {
            H->syt[dest].val.type = HTYPE_BIT;
            H->syt[dest].val.v.bits = src.v.bits;
            H->syt[dest].allocated = 1;
        }
        break;
    }

    case POP_BAND: {
        if (numop < 2) break;
        uint32_t a = halmat_resolve_operand(H, H->code[pc + 1]).v.bits;
        uint32_t b = halmat_resolve_operand(H, H->code[pc + 2]).v.bits;
        halmat_val_t r = {0};
        r.type = HTYPE_BIT;
        r.v.bits = a & b;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_BOR: {
        if (numop < 2) break;
        uint32_t a = halmat_resolve_operand(H, H->code[pc + 1]).v.bits;
        uint32_t b = halmat_resolve_operand(H, H->code[pc + 2]).v.bits;
        halmat_val_t r = {0};
        r.type = HTYPE_BIT;
        r.v.bits = a | b;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_BNOT: {
        if (numop < 1) break;
        uint32_t a = halmat_resolve_operand(H, H->code[pc + 1]).v.bits;
        halmat_val_t r = {0};
        r.type = HTYPE_BIT;
        r.v.bits = ~a;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_BCAT: {
        if (numop < 2) break;
        uint32_t a = halmat_resolve_operand(H, H->code[pc + 1]).v.bits;
        uint32_t b = halmat_resolve_operand(H, H->code[pc + 2]).v.bits;
        halmat_val_t r = {0};
        r.type = HTYPE_BIT;
        r.v.bits = (a << 16) | (b & 0xFFFF);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_BTOB: {
        if (numop < 1) break;
        halmat_val_t r = halmat_resolve_operand(H, H->code[pc + 1]);
        r.type = HTYPE_BIT;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_ITOB: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_BIT;
        r.v.bits = (uint32_t)a.v.integer;
        halmat_store_vac(H, pc, r);
        break;
    }

    default:
        fprintf(stderr, "halmat_class1: unknown popcode 0x%03X at PC=%u\n",
                popcode, pc);
        break;
    }

    (void)tag;
    H->pc = pc + numop + 1;
    return HALMAT_OK;
}
