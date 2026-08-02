#include "halmat.h"
#include <math.h>

/* HAL/S spec: INTEGER(3.5)=4, INTEGER(-1.4)=-1.
 * That's round-half-away-from-zero, not C's truncation. */
static int32_t to_int(halmat_val_t v)
{
    switch (v.type) {
    case HTYPE_INTEGER: return v.v.integer;
    case HTYPE_SCALAR:  return (int32_t)round(v.v.scalar);
    case HTYPE_BIT:     return (int32_t)v.v.bits;
    default:            return 0;
    }
}

/* Ron's AP-101S trace (LH/MR/SLL/STH on Don's emulator) confirmed
 * INTEGER arithmetic wraps mod 2^16, INTEGER DOUBLE mod 2^32.
 * No saturation. See issue #11. */
static int32_t narrow_int(int32_t v, uint8_t prec)
{
    if (prec == HPREC_SINGLE)
        return (int32_t)(int16_t)v;
    return v;
}

static uint8_t wider_prec(halmat_val_t a, halmat_val_t b)
{
    return (a.precision > b.precision) ? a.precision : b.precision;
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
            uint8_t prec = H->syt[dest].declared ? H->syt[dest].val.precision
                                                 : src.precision;
            H->syt[dest].val.type = HTYPE_INTEGER;
            H->syt[dest].val.precision = prec;
            H->syt[dest].val.v.integer = narrow_int(to_int(src), prec);
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
        r.precision = wider_prec(a, b);
        r.v.integer = narrow_int(to_int(a) + to_int(b), r.precision);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_ISUB: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.precision = wider_prec(a, b);
        r.v.integer = narrow_int(to_int(a) - to_int(b), r.precision);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_IIPR: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.precision = wider_prec(a, b);
        r.v.integer = narrow_int(to_int(a) * to_int(b), r.precision);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_INEG: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.precision = a.precision;
        r.v.integer = narrow_int(-to_int(a), r.precision);
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
        r.precision = wider_prec(a, b);
        r.v.integer = narrow_int(result, r.precision);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_STOI: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.precision = (a.precision == HPREC_DOUBLE) ? HPREC_DOUBLE : HPREC_SINGLE;
        r.v.integer = narrow_int(to_int(a), r.precision);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_BTOI: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.precision = HPREC_SINGLE;
        r.v.integer = (int32_t)(int16_t)a.v.bits;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_CTOI: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        int ok;
        r.type = HTYPE_INTEGER;
        r.precision = HPREC_SINGLE;
        r.v.integer = halmat_cint(a.v.string.data, a.v.string.len, &ok);
        if (!ok) {
            fprintf(stderr, "halmat_class6: CTOI operand '%.*s' at PC=%u is "
                    "not a signed whole number\n",
                    (int)a.v.string.len, a.v.string.data, pc);
            return HALMAT_ERR_BAD_QUAL;
        }
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_ITOI: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_INTEGER;
        r.precision = a.precision;
        r.v.integer = narrow_int(to_int(a), r.precision);
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
