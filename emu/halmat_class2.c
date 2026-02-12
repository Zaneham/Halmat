#include "halmat.h"
#include <stdio.h>

int halmat_exec_class2(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag)
{
    uint32_t pc = H->pc;

    switch (popcode) {

    case POP_CASN: {
        if (numop < 2) break;
        halmat_val_t src = halmat_resolve_operand(H, H->code[pc + 1]);
        uint32_t dest = HALMAT_DATA(H->code[pc + 2]);
        if (dest < HALMAT_MAX_SYT) {
            H->syt[dest].val.type = HTYPE_CHAR;
            memcpy(H->syt[dest].val.v.string.data, src.v.string.data,
                   src.v.string.len);
            H->syt[dest].val.v.string.len = src.v.string.len;
            H->syt[dest].allocated = 1;
        }
        break;
    }

    case POP_CCAT: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_CHAR;
        int total = a.v.string.len + b.v.string.len;
        if (total > 255) total = 255;
        memcpy(r.v.string.data, a.v.string.data, a.v.string.len);
        int blen = total - a.v.string.len;
        memcpy(r.v.string.data + a.v.string.len, b.v.string.data, blen);
        r.v.string.len = (uint16_t)total;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_CTOC: {
        if (numop < 1) break;
        halmat_val_t r = halmat_resolve_operand(H, H->code[pc + 1]);
        r.type = HTYPE_CHAR;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_ITOC: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_CHAR;
        int n = snprintf(r.v.string.data, 256, "%d",
                         (a.type == HTYPE_INTEGER) ? a.v.integer : (int)a.v.scalar);
        r.v.string.len = (uint16_t)(n > 255 ? 255 : n);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_STOC: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_CHAR;
        int n = snprintf(r.v.string.data, 256, "%g", a.v.scalar);
        r.v.string.len = (uint16_t)(n > 255 ? 255 : n);
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_BTOC: {
        if (numop < 1) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_CHAR;
        int n = snprintf(r.v.string.data, 256, "%u", a.v.bits);
        r.v.string.len = (uint16_t)(n > 255 ? 255 : n);
        halmat_store_vac(H, pc, r);
        break;
    }

    default:
        fprintf(stderr, "halmat_class2: unknown popcode 0x%03X at PC=%u\n",
                popcode, pc);
        break;
    }

    (void)tag;
    H->pc = pc + numop + 1;
    return HALMAT_OK;
}
