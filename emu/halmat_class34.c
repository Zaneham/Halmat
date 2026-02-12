#include "halmat.h"
#include <math.h>

static double val_scalar(halmat_val_t v)
{
    return (v.type == HTYPE_INTEGER) ? (double)v.v.integer : v.v.scalar;
}

int halmat_exec_class3(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag)
{
    uint32_t pc = H->pc;

    switch (popcode) {

    case POP_MASN: {
        if (numop < 2) break;
        halmat_val_t src = halmat_resolve_operand(H, H->code[pc + 1]);
        uint32_t dest = HALMAT_DATA(H->code[pc + 2]);
        if (dest < HALMAT_MAX_SYT) {
            H->syt[dest].val = src;
            H->syt[dest].val.type = HTYPE_MATRIX;
            H->syt[dest].allocated = 1;
        }
        break;
    }

    case POP_MADD:
    case POP_MSUB: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_MATRIX;
        r.rows = a.rows > b.rows ? a.rows : b.rows;
        r.cols = a.cols > b.cols ? a.cols : b.cols;
        int n = r.rows * r.cols;
        if (n > 64) n = 64;
        for (int i = 0; i < n; i++) {
            if (popcode == POP_MADD)
                r.v.matrix[i] = a.v.matrix[i] + b.v.matrix[i];
            else
                r.v.matrix[i] = a.v.matrix[i] - b.v.matrix[i];
        }
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_MSPR: {
        if (numop < 2) break;
        halmat_val_t m = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t s = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = m;
        r.type = HTYPE_MATRIX;
        double sv = val_scalar(s);
        int n = r.rows * r.cols;
        if (n > 64) n = 64;
        for (int i = 0; i < n; i++)
            r.v.matrix[i] *= sv;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_MNEG: {
        if (numop < 1) break;
        halmat_val_t m = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = m;
        r.type = HTYPE_MATRIX;
        int n = r.rows * r.cols;
        if (n > 64) n = 64;
        for (int i = 0; i < n; i++)
            r.v.matrix[i] = -r.v.matrix[i];
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_MTRA: {
        if (numop < 1) break;
        halmat_val_t m = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = {0};
        r.type = HTYPE_MATRIX;
        r.rows = m.cols;
        r.cols = m.rows;
        for (int i = 0; i < m.rows && i < 8; i++)
            for (int j = 0; j < m.cols && j < 8; j++)
                r.v.matrix[j * r.cols + i] = m.v.matrix[i * m.cols + j];
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_MMPR: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_MATRIX;
        r.rows = a.rows;
        r.cols = b.cols;
        for (int i = 0; i < a.rows && i < 8; i++)
            for (int j = 0; j < b.cols && j < 8; j++) {
                double sum = 0;
                for (int k = 0; k < a.cols && k < 8; k++)
                    sum += a.v.matrix[i * a.cols + k] * b.v.matrix[k * b.cols + j];
                r.v.matrix[i * r.cols + j] = sum;
            }
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_MSDV:
    case POP_MDET:
    case POP_MIDN:
    case POP_MINV:
    case POP_MTOM:
    case POP_VVPR:
        /* TODO */
        break;

    default:
        fprintf(stderr, "halmat_class3: unknown popcode 0x%03X at PC=%u\n",
                popcode, pc);
        break;
    }

    (void)tag;
    H->pc = pc + numop + 1;
    return HALMAT_OK;
}

int halmat_exec_class4(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag)
{
    uint32_t pc = H->pc;

    switch (popcode) {

    case POP_VASN: {
        if (numop < 2) break;
        halmat_val_t src = halmat_resolve_operand(H, H->code[pc + 1]);
        uint32_t dest = HALMAT_DATA(H->code[pc + 2]);
        if (dest < HALMAT_MAX_SYT) {
            H->syt[dest].val = src;
            H->syt[dest].val.type = HTYPE_VECTOR;
            H->syt[dest].allocated = 1;
        }
        break;
    }

    case POP_VADD:
    case POP_VSUB: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_VECTOR;
        r.rows = a.rows > b.rows ? a.rows : b.rows;
        int n = r.rows;
        if (n > 64) n = 64;
        for (int i = 0; i < n; i++) {
            if (popcode == POP_VADD)
                r.v.vector[i] = a.v.vector[i] + b.v.vector[i];
            else
                r.v.vector[i] = a.v.vector[i] - b.v.vector[i];
        }
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_VSPR: {
        if (numop < 2) break;
        halmat_val_t v = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t s = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = v;
        r.type = HTYPE_VECTOR;
        double sv = val_scalar(s);
        for (int i = 0; i < v.rows && i < 64; i++)
            r.v.vector[i] *= sv;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_VNEG: {
        if (numop < 1) break;
        halmat_val_t v = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t r = v;
        r.type = HTYPE_VECTOR;
        for (int i = 0; i < v.rows && i < 64; i++)
            r.v.vector[i] = -r.v.vector[i];
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_VCRS: {
        /* 3D only */
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_VECTOR;
        r.rows = 3;
        r.v.vector[0] = a.v.vector[1]*b.v.vector[2] - a.v.vector[2]*b.v.vector[1];
        r.v.vector[1] = a.v.vector[2]*b.v.vector[0] - a.v.vector[0]*b.v.vector[2];
        r.v.vector[2] = a.v.vector[0]*b.v.vector[1] - a.v.vector[1]*b.v.vector[0];
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_VDOT: {
        if (numop < 2) break;
        halmat_val_t a = halmat_resolve_operand(H, H->code[pc + 1]);
        halmat_val_t b = halmat_resolve_operand(H, H->code[pc + 2]);
        halmat_val_t r = {0};
        r.type = HTYPE_SCALAR;
        double sum = 0;
        int n = a.rows < b.rows ? a.rows : b.rows;
        if (n > 64) n = 64;
        for (int i = 0; i < n; i++)
            sum += a.v.vector[i] * b.v.vector[i];
        r.v.scalar = sum;
        halmat_store_vac(H, pc, r);
        break;
    }

    case POP_VMPR:
    case POP_MVPR:
    case POP_VTOV:
        /* TODO */
        break;

    default:
        fprintf(stderr, "halmat_class4: unknown popcode 0x%03X at PC=%u\n",
                popcode, pc);
        break;
    }

    (void)tag;
    H->pc = pc + numop + 1;
    return HALMAT_OK;
}
