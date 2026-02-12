#include "halmat.h"
#include <math.h>

halmat_val_t halmat_resolve_operand(halmat_t *H, uint32_t operand_word)
{
    uint32_t data = HALMAT_DATA(operand_word);
    uint32_t qual = HALMAT_QUAL(operand_word);
    uint32_t tag1 = HALMAT_TAG1(operand_word);
    halmat_val_t v;
    memset(&v, 0, sizeof(v));

    switch (qual) {
    case QUAL_SYT:
        if (data < HALMAT_MAX_SYT)
            return H->syt[data].val;
        break;

    case QUAL_LIT:
        if (data < H->lit_count) {
            int typ = H->lit[data].lit1;
            switch (typ) {
            case 0: /* CHAR */
                v.type = HTYPE_CHAR;
                halmat_decode_char_lit(H, data, v.v.string.data, (int *)&v.v.string.len);
                break;
            case 1: /* ARITH (single float) */
                v.type = HTYPE_SCALAR;
                v.v.scalar = ibm_float_to_double((uint32_t)H->lit[data].lit2);
                break;
            case 2: /* BIT */
                v.type = HTYPE_BIT;
                v.v.bits = (uint32_t)H->lit[data].lit2;
                break;
            case 5: /* DOUBLE */
                v.type = HTYPE_SCALAR;
                v.v.scalar = ibm_double_to_double(
                    (uint32_t)H->lit[data].lit2,
                    (uint32_t)H->lit[data].lit3);
                break;
            }
        }
        break;

    case QUAL_VAC:
        return H->vac[VAC_SLOT(data)];

    case QUAL_IMD:
        v.type = HTYPE_INTEGER;
        v.v.integer = (int32_t)data;
        break;

    case QUAL_INL:
        v.type = HTYPE_INTEGER;
        v.v.integer = (int32_t)data;
        break;

    default:
        break;
    }

    (void)tag1;
    return v;
}

void halmat_store_vac(halmat_t *H, uint32_t addr, halmat_val_t val)
{
    H->vac[VAC_SLOT(addr)] = val;
}

int halmat_step(halmat_t *H)
{
    if (H->halted)
        return H->halted;

    if (H->pc >= H->code_len) {
        H->halted = 1;
        return HALMAT_HALT;
    }

    uint32_t w = H->code[H->pc];

    /* skip stray operand words */
    if (!HALMAT_IS_OP(w)) {
        H->pc++;
        return HALMAT_OK;
    }

    uint32_t popcode = HALMAT_POPCODE(w);
    uint32_t cls     = HALMAT_CLASS(w);
    uint32_t numop   = HALMAT_NUMOP(w);
    uint32_t tag     = HALMAT_TAG(w);

    int rc;
    switch (cls) {
    case 0: rc = halmat_exec_class0(H, popcode, numop, tag); break;
    case 1: rc = halmat_exec_class1(H, popcode, numop, tag); break;
    case 2: rc = halmat_exec_class2(H, popcode, numop, tag); break;
    case 3: rc = halmat_exec_class3(H, popcode, numop, tag); break;
    case 4: rc = halmat_exec_class4(H, popcode, numop, tag); break;
    case 5: rc = halmat_exec_class5(H, popcode, numop, tag); break;
    case 6: rc = halmat_exec_class6(H, popcode, numop, tag); break;
    case 7: rc = halmat_exec_class7(H, popcode, numop, tag); break;
    case 8: rc = halmat_exec_class8(H, popcode, numop, tag); break;
    default:
        fprintf(stderr, "halmat_step: unknown class %u at PC=%u\n", cls, H->pc);
        rc = HALMAT_ERR_UNKNOWN;
        break;
    }

    H->cycle_count++;

    if (rc < 0) {
        H->halted = -1;
        fprintf(stderr, "halmat_step: error %d at PC=%u (popcode=0x%03X)\n",
                rc, H->pc, popcode);
    }

    return rc;
}

int halmat_run(halmat_t *H)
{
    int rc;
    while ((rc = halmat_step(H)) == HALMAT_OK)
        ;
    return rc;
}
