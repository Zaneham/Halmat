#include "halmat.h"
#include <math.h>

/* App. D: a SCALAR reaches BIT via INTEGER, so it rounds first. Matters
   because an INTEGER-looking literal like BIT(12) arrives as an ARITH
   literal — reading .v.integer off it used to yield an empty BIT. */
static int32_t to_int(halmat_val_t v)
{
    switch (v.type) {
    case HTYPE_INTEGER: return v.v.integer;
    case HTYPE_SCALAR:  return (int32_t)round(v.v.scalar);
    case HTYPE_BIT:     return (int32_t)v.v.bits;
    default:            return 0;
    }
}

static void cat_warn(uint32_t pc)
{
    static int warned = 0;
    if (warned) return;
    warned = 1;
    fprintf(stderr, "halmat_class1: BCAT at PC=%u has no declared operand "
            "width; assuming 16. Supply the symbol table with --symtab.\n", pc);
}

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

    case POP_BAND:
    case POP_BOR: {
        if (numop < 2) break;
        halmat_val_t av = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t bv = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_BIT;
        r.blen = (av.blen > bv.blen) ? av.blen : bv.blen;
        r.v.bits = (popcode == POP_BAND) ? (av.v.bits & bv.v.bits)
                                         : (av.v.bits | bv.v.bits);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_BNOT: {
        if (numop < 1) break;
        halmat_val_t av = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_BIT;
        r.blen = av.blen;
        r.v.bits = ~av.v.bits;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_BCAT: {
        if (numop < 2) break;
        halmat_val_t av = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t bv = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        /* The left operand shifts by the right one's *declared* width, not a
           fixed 16 — BIT(4)'10' || BIT(4)'5' is 0b10100101, not 10<<16|5. */
        int w = bv.blen;
        if (w < 1 || w > 32) {
            w = 16;
            cat_warn(pc);
        }
        r.type = HTYPE_BIT;
        r.blen = (uint8_t)((av.blen + w > 32) ? 32 : av.blen + w);
        r.v.bits = (w >= 32) ? bv.v.bits
                             : ((av.v.bits << w) | (bv.v.bits & ((1u << w) - 1)));
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
        r.v.bits = (uint32_t)to_int(a);
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
