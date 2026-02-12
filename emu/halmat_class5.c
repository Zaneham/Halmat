#include "halmat.h"
#include <math.h>

int halmat_exec_class5(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag)
{
    uint32_t pc = H->pc;

    switch (popcode) {

    case POP_SASN: {
        if (numop < 2) break;
        halmat_val_t src = halmat_resolve_operand(H, H->code[pc + 1]);
        uint32_t dest = HALMAT_DATA(H->code[pc + 2]);
        double val = (src.type == HTYPE_INTEGER) ? (double)src.v.integer : src.v.scalar;
        if (dest < HALMAT_MAX_SYT) {
            H->syt[dest].val.type = HTYPE_SCALAR;
            H->syt[dest].val.v.scalar = val;
            H->syt[dest].allocated = 1;
        }
        break;
    }

    case POP_SADD: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        double va = (a.type == HTYPE_INTEGER) ? (double)a.v.integer : a.v.scalar;
        double vb = (b.type == HTYPE_INTEGER) ? (double)b.v.integer : b.v.scalar;
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.v.scalar = va + vb;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SSUB: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        double va = (a.type == HTYPE_INTEGER) ? (double)a.v.integer : a.v.scalar;
        double vb = (b.type == HTYPE_INTEGER) ? (double)b.v.integer : b.v.scalar;
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.v.scalar = va - vb;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SSPR: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        double va = (a.type == HTYPE_INTEGER) ? (double)a.v.integer : a.v.scalar;
        double vb = (b.type == HTYPE_INTEGER) ? (double)b.v.integer : b.v.scalar;
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.v.scalar = va * vb;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SSDV: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        double va = (a.type == HTYPE_INTEGER) ? (double)a.v.integer : a.v.scalar;
        double vb = (b.type == HTYPE_INTEGER) ? (double)b.v.integer : b.v.scalar;
        if (vb == 0.0) {
            H->pc = pc + numop + 1;
            return HALMAT_ERR_DIV_ZERO;
        }
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.v.scalar = va / vb;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SEXP: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        double va = (a.type == HTYPE_INTEGER) ? (double)a.v.integer : a.v.scalar;
        double vb = (b.type == HTYPE_INTEGER) ? (double)b.v.integer : b.v.scalar;
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.v.scalar = pow(va, vb);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SIEX: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        double base = (a.type == HTYPE_INTEGER) ? (double)a.v.integer : a.v.scalar;
        int exp = (b.type == HTYPE_INTEGER) ? b.v.integer : (int)b.v.scalar;
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.v.scalar = pow(base, (double)exp);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SPEX: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        double va = (a.type == HTYPE_INTEGER) ? (double)a.v.integer : a.v.scalar;
        double vb = (b.type == HTYPE_INTEGER) ? (double)b.v.integer : b.v.scalar;
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.v.scalar = pow(va, vb);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_SNEG: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        double va = (a.type == HTYPE_INTEGER) ? (double)a.v.integer : a.v.scalar;
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.v.scalar = -va;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_ITOS: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.v.scalar = (a.type == HTYPE_INTEGER) ? (double)a.v.integer : a.v.scalar;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_STOS: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.v.scalar = (a.type == HTYPE_INTEGER) ? (double)a.v.integer : a.v.scalar;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_BTOS: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.v.scalar = (double)a.v.bits;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_CTOS: {
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        r.v.scalar = 0.0;
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
