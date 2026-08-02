#include "halmat.h"
#include <math.h>

static double scal_val(halmat_val_t v)
{
    return (v.type == HTYPE_INTEGER) ? (double)v.v.integer : v.v.scalar;
}

/* HAL/S Programmer's Guide p.75: a binary op runs at the precision of
 * its wider operand. Narrowing on store happens at SASN, not here. */
static uint8_t wider_prec(halmat_val_t a, halmat_val_t b)
{
    return (a.precision > b.precision) ? a.precision : b.precision;
}

/* IBM single is 24-bit hex mantissa, same bit count as IEEE single,
 * so a (float) round-trip is the right narrowing. Without this,
 * SCALAR and SCALAR DOUBLE held identical bit patterns and X - X2
 * always came out exactly zero in Ron's test (issue #10). */
static double narrow_to_dest(double v, uint8_t dest_prec)
{
    if (dest_prec == HPREC_SINGLE)
        return (double)(float)v;
    return v;
}

int halmat_exec_class5(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag)
{
    uint32_t pc = H->pc;

    switch (popcode) {

    case POP_SASN: {
        if (numop < 2) break;
        halmat_val_t src = halmat_resolve_operand(H, H->code[pc + 1]);
        uint32_t dest = HALMAT_DATA(H->code[pc + 2]);
        if (dest < HALMAT_MAX_SYT) {
            uint8_t prec = H->syt[dest].declared ? H->syt[dest].val.precision
                                                 : src.precision;
            H->syt[dest].val.type = HTYPE_SCALAR;
            H->syt[dest].val.precision = prec;
            H->syt[dest].val.v.scalar = narrow_to_dest(scal_val(src), prec);
            H->syt[dest].allocated = 1;
        }
        break;
    }

    case POP_SADD: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.precision = wider_prec(a, b);
        r.v.scalar = scal_val(a) + scal_val(b);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SSUB: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.precision = wider_prec(a, b);
        r.v.scalar = scal_val(a) - scal_val(b);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SSPR: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.precision = wider_prec(a, b);
        r.v.scalar = scal_val(a) * scal_val(b);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SSDV: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        double vb = scal_val(b);
        if (vb == 0.0) {
            H->pc = pc + numop + 1;
            return HALMAT_ERR_DIV_ZERO;
        }
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.precision = wider_prec(a, b);
        r.v.scalar = scal_val(a) / vb;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SEXP: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.precision = wider_prec(a, b);
        r.v.scalar = pow(scal_val(a), scal_val(b));
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SIEX: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        double base = scal_val(a);
        int exp = (b.type == HTYPE_INTEGER) ? b.v.integer : (int)b.v.scalar;
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.precision = a.precision;
        r.v.scalar = pow(base, (double)exp);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SPEX: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.precision = wider_prec(a, b);
        r.v.scalar = pow(scal_val(a), scal_val(b));
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SNEG: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.precision = a.precision;
        r.v.scalar = -scal_val(a);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_ITOS: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.precision = (a.precision == HPREC_DOUBLE) ? HPREC_DOUBLE : HPREC_SINGLE;
        r.v.scalar = scal_val(a);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_STOS: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.precision = a.precision;
        r.v.scalar = scal_val(a);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_BTOS: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.precision = HPREC_SINGLE;
        r.v.scalar = (double)a.v.bits;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_CTOS: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        int ok;
        r.type = HTYPE_SCALAR;
        r.precision = HPREC_SINGLE;
        r.v.scalar = halmat_cnum(a.v.string.data, a.v.string.len, &ok);
        if (!ok) {
            fprintf(stderr, "halmat_class5: CTOS operand '%.*s' at PC=%u is "
                    "not an arithmetic literal\n",
                    (int)a.v.string.len, a.v.string.data, pc);
            return HALMAT_ERR_BAD_QUAL;
        }
        halmat_store_vac(H, pc, r);
        break;
    }

    default:
        fprintf(stderr, "halmat_class5: unknown popcode 0x%03X at PC=%u\n",
                popcode, pc);
        break;
    }

    (void)tag;
    H->pc = pc + numop + 1;
    return HALMAT_OK;
}
