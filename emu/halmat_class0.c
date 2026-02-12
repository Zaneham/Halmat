#include "halmat.h"
#include "halmat_io.h"

/* Advance PC past current operator + operands */
#define ADVANCE() do { H->pc += numop + 1; } while (0)

/* Scan forward for a matching operator, respecting nesting.
 * inc_pop/dec_pop define the nesting brackets. Returns address of dec_pop. */
static uint32_t scan_forward(halmat_t *H, uint32_t start,
                             uint32_t inc_pop, uint32_t dec_pop)
{
    int depth = 1;
    uint32_t scan = start;
    while (scan < H->code_len && depth > 0) {
        uint32_t w = H->code[scan];
        if (HALMAT_IS_OP(w)) {
            uint32_t pop = HALMAT_POPCODE(w);
            uint32_t n   = HALMAT_NUMOP(w);
            if (pop == inc_pop) depth++;
            if (pop == dec_pop) depth--;
            if (depth == 0)
                return scan;
            scan += n + 1;
        } else {
            scan++;
        }
    }
    return H->code_len; /* not found */
}

int halmat_exec_class0(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag)
{
    switch (popcode) {

    case POP_NOP:
    case POP_PXRC:
    case POP_EXTN:
    case POP_IMRK:
        ADVANCE();
        return HALMAT_OK;

    case POP_XREC:
        if (tag == 1) {
            /* Final block — halt */
            H->halted = 1;
            ADVANCE();
            return HALMAT_HALT;
        }
        /* Non-final block: advance to next block */
        {
            uint32_t cur_block = H->pc / HALMAT_BLOCK_WORDS;
            uint32_t next_base = (cur_block + 1) * HALMAT_BLOCK_WORDS;
            if (next_base + 2 < H->code_len)
                H->pc = next_base + 2;
            else {
                H->halted = 1;
                return HALMAT_HALT;
            }
        }
        return HALMAT_OK;

    case POP_SMRK: {
        if (numop >= 1) {
            uint32_t op = H->code[H->pc + 1];
            H->current_stmt = HALMAT_DATA(op);
        }
        H->stmt_count++;
        ADVANCE();
        return HALMAT_OK;
    }

    case POP_MDEF:
    case POP_TDEF:
    case POP_UDEF:
    case POP_CDEF:
        ADVANCE();
        return HALMAT_OK;

    case POP_EDCL:
        ADVANCE();
        return HALMAT_OK;

    case POP_CLOS:
        if (H->frame_depth > 0) {
            call_frame_t *f = &H->frames[--H->frame_depth];
            H->pc = f->return_pc;
            return HALMAT_OK;
        }
        H->halted = 1;
        ADVANCE();
        return HALMAT_HALT;

    case POP_IFHD:
        ADVANCE();
        return HALMAT_OK;

    case POP_FBRA: {
        if (numop < 2) { ADVANCE(); return HALMAT_OK; }
        uint32_t target_flow = HALMAT_DATA(H->code[H->pc + 1]);
        halmat_val_t cond = halmat_resolve_operand(H, H->code[H->pc + 2]);
        int cond_val = cond.v.integer;
        ADVANCE();
        if (!cond_val) {
            if (target_flow < HALMAT_MAX_FLOW && H->flow[target_flow] != 0) {
                H->pc = H->flow[target_flow];
            }
        }
        return HALMAT_OK;
    }

    case POP_BRA: {
        if (numop < 1) { ADVANCE(); return HALMAT_OK; }
        uint32_t target_flow = HALMAT_DATA(H->code[H->pc + 1]);
        if (target_flow < HALMAT_MAX_FLOW && H->flow[target_flow] != 0) {
            H->pc = H->flow[target_flow];
        } else {
            ADVANCE();
        }
        return HALMAT_OK;
    }

    case POP_LBL:
        ADVANCE();
        return HALMAT_OK;

    case POP_DSMP: {
        if (numop >= 1) {
            uint32_t flow_num = HALMAT_DATA(H->code[H->pc + 1]);
            if (flow_num < HALMAT_MAX_FLOW)
                H->flow[flow_num] = H->pc;
        }
        ADVANCE();
        return HALMAT_OK;
    }

    case POP_ESMP:
        ADVANCE();
        return HALMAT_OK;

    case POP_DTST: {
        /* TAG=0: WHILE (test at top), TAG=1: UNTIL (skip first test) */
        if (numop < 1) { ADVANCE(); return HALMAT_OK; }
        uint32_t flow_num = HALMAT_DATA(H->code[H->pc + 1]);
        uint32_t cmp_addr = H->pc + numop + 1; /* comparison starts here */

        /* Push loop info */
        if (H->loop_depth >= HALMAT_MAX_LOOPS) {
            fprintf(stderr, "halmat: loop stack overflow at PC=%u\n", H->pc);
            return HALMAT_ERR_STACK;
        }
        loop_info_t *loop = &H->loops[H->loop_depth++];
        loop->flow_num = flow_num;
        loop->cmp_addr = cmp_addr;
        loop->tag = tag;

        if (flow_num < HALMAT_MAX_FLOW)
            H->flow[flow_num] = cmp_addr;

        if (tag == 1) {
            /* UNTIL: skip first test, jump to body */
            uint32_t scan = cmp_addr;
            while (scan < H->code_len) {
                uint32_t w = H->code[scan];
                if (HALMAT_IS_OP(w)) {
                    uint32_t pop = HALMAT_POPCODE(w);
                    uint32_t n   = HALMAT_NUMOP(w);
                    if (pop == POP_CTST) {
                        H->pc = scan + n + 1; /* body starts after CTST */
                        return HALMAT_OK;
                    }
                    scan += n + 1;
                } else {
                    scan++;
                }
            }
        }

        H->pc = cmp_addr;
        return HALMAT_OK;
    }

    case POP_CTST: {
        if (numop < 1) { ADVANCE(); return HALMAT_OK; }

        halmat_val_t cond = halmat_resolve_operand(H, H->code[H->pc + 1]);
        int cond_val = cond.v.integer;

        int should_exit;
        if (H->loop_depth > 0 && H->loops[H->loop_depth - 1].tag == 1)
            should_exit = cond_val;     /* UNTIL: exit if TRUE */
        else
            should_exit = !cond_val;    /* WHILE: exit if FALSE */

        if (should_exit) {
            uint32_t exit_addr = scan_forward(H, H->pc + numop + 1,
                                              POP_DTST, POP_ETST);
            if (exit_addr < H->code_len) {
                uint32_t n = HALMAT_NUMOP(H->code[exit_addr]);
                H->pc = exit_addr + n + 1;
            } else {
                ADVANCE();
            }
            if (H->loop_depth > 0)
                H->loop_depth--;
            return HALMAT_OK;
        }

        ADVANCE();
        return HALMAT_OK;
    }

    case POP_ETST: {
        if (H->loop_depth > 0) {
            loop_info_t *loop = &H->loops[H->loop_depth - 1];
            H->pc = loop->cmp_addr;
        } else {
            ADVANCE();
        }
        return HALMAT_OK;
    }

    case POP_DFOR: {
        uint32_t flow_num = HALMAT_DATA(H->code[H->pc + 1]);
        uint32_t loop_var = (numop >= 2) ? HALMAT_DATA(H->code[H->pc + 2]) : 0;

        if (H->loop_depth >= HALMAT_MAX_LOOPS) {
            fprintf(stderr, "halmat: loop stack overflow at PC=%u\n", H->pc);
            return HALMAT_ERR_STACK;
        }

        if (flow_num < HALMAT_MAX_FLOW)
            H->flow[flow_num] = H->pc;

        if (numop == 2) {
            /* Discrete FOR: values from subsequent AFOR operators */
            uint32_t scan = H->pc + numop + 1;
            halmat_val_t first_val;
            memset(&first_val, 0, sizeof(first_val));
            int found_afor = 0;
            while (scan < H->code_len) {
                uint32_t w = H->code[scan];
                if (HALMAT_IS_OP(w) && HALMAT_POPCODE(w) == POP_AFOR) {
                    uint32_t n = HALMAT_NUMOP(w);
                    if (n >= 1)
                        first_val = halmat_resolve_operand(H, H->code[scan + 1]);
                    found_afor = 1;
                    break;
                }
                if (HALMAT_IS_OP(w)) break; /* non-AFOR = no discrete values */
                scan++;
            }

            if (!found_afor) { ADVANCE(); return HALMAT_OK; }

            if (loop_var < HALMAT_MAX_SYT) {
                H->syt[loop_var].val = first_val;
                H->syt[loop_var].allocated = 1;
            }

            loop_info_t *loop = &H->loops[H->loop_depth++];
            loop->flow_num = flow_num;
            loop->cmp_addr = H->pc;
            loop->tag = tag;
            loop->is_discrete = 1;
            loop->discrete_idx = 0;

            scan = H->pc + numop + 1;
            while (scan < H->code_len) {
                uint32_t w = H->code[scan];
                if (HALMAT_IS_OP(w) && HALMAT_POPCODE(w) == POP_AFOR) {
                    uint32_t n = HALMAT_NUMOP(w);
                    scan += n + 1;
                } else {
                    break;
                }
            }
            H->pc = scan;
            return HALMAT_OK;
        }

        if (numop < 3) { ADVANCE(); return HALMAT_OK; }

        halmat_val_t init_val = halmat_resolve_operand(H, H->code[H->pc + 3]);
        halmat_val_t final_val = (numop >= 4) ?
            halmat_resolve_operand(H, H->code[H->pc + 4]) : init_val;
        halmat_val_t incr_val;
        memset(&incr_val, 0, sizeof(incr_val));
        if (numop >= 5) {
            incr_val = halmat_resolve_operand(H, H->code[H->pc + 5]);
        } else {
            incr_val.type = HTYPE_SCALAR;
            incr_val.v.scalar = 1.0;
        }

        if (loop_var < HALMAT_MAX_SYT) {
            H->syt[loop_var].val.type = HTYPE_SCALAR;
            H->syt[loop_var].val.v.scalar = init_val.v.scalar;
            H->syt[loop_var].allocated = 1;
        }

        loop_info_t *loop = &H->loops[H->loop_depth++];
        loop->flow_num = flow_num;
        loop->cmp_addr = H->pc;
        loop->tag = tag;
        loop->is_discrete = 0;
        loop->discrete_idx = 0;

        double cur = init_val.v.scalar;
        double fin = final_val.v.scalar;
        double inc = incr_val.v.scalar;
        if ((inc > 0 && cur > fin) || (inc < 0 && cur < fin)) {
            uint32_t exit_addr = scan_forward(H, H->pc + numop + 1,
                                              POP_DFOR, POP_EFOR);
            if (exit_addr < H->code_len) {
                uint32_t n = HALMAT_NUMOP(H->code[exit_addr]);
                H->pc = exit_addr + n + 1;
            } else {
                ADVANCE();
            }
            H->loop_depth--;
            return HALMAT_OK;
        }

        ADVANCE();
        return HALMAT_OK;
    }

    case POP_EFOR: {
        if (H->loop_depth == 0) { ADVANCE(); return HALMAT_OK; }
        loop_info_t *loop = &H->loops[H->loop_depth - 1];

        uint32_t dfor_addr = loop->cmp_addr;
        uint32_t dfor_numop = HALMAT_NUMOP(H->code[dfor_addr]);
        uint32_t loop_var = HALMAT_DATA(H->code[dfor_addr + 2]);

        if (loop->is_discrete) {
            loop->discrete_idx++;

            uint32_t scan = dfor_addr + dfor_numop + 1;
            uint32_t afor_idx = 0;
            uint32_t body_start = scan;
            int found = 0;

            while (scan < H->code_len) {
                uint32_t w = H->code[scan];
                if (HALMAT_IS_OP(w) && HALMAT_POPCODE(w) == POP_AFOR) {
                    uint32_t n = HALMAT_NUMOP(w);
                    if (afor_idx == loop->discrete_idx) {
                        if (n >= 1 && loop_var < HALMAT_MAX_SYT) {
                            H->syt[loop_var].val =
                                halmat_resolve_operand(H, H->code[scan + 1]);
                        }
                        found = 1;
                    }
                    afor_idx++;
                    scan += n + 1;
                    body_start = scan;
                } else {
                    break;
                }
            }

            if (!found) {
                H->loop_depth--;
                ADVANCE();
                return HALMAT_OK;
            }

            H->pc = body_start;
            return HALMAT_OK;
        }

        halmat_val_t final_val = (dfor_numop >= 4) ?
            halmat_resolve_operand(H, H->code[dfor_addr + 4]) :
            halmat_resolve_operand(H, H->code[dfor_addr + 3]);
        halmat_val_t incr_val;
        memset(&incr_val, 0, sizeof(incr_val));
        if (dfor_numop >= 5) {
            incr_val = halmat_resolve_operand(H, H->code[dfor_addr + 5]);
        } else {
            incr_val.type = HTYPE_SCALAR;
            incr_val.v.scalar = 1.0;
        }

        if (loop_var < HALMAT_MAX_SYT) {
            H->syt[loop_var].val.v.scalar += incr_val.v.scalar;
            double cur = H->syt[loop_var].val.v.scalar;
            double fin = final_val.v.scalar;
            double inc = incr_val.v.scalar;

            int done;
            if (inc > 0)
                done = (cur > fin);
            else if (inc < 0)
                done = (cur < fin);
            else
                done = 1; /* zero increment — prevent infinite loop */

            if (done) {
                H->loop_depth--;
                ADVANCE();
                return HALMAT_OK;
            }
        }

        H->pc = dfor_addr + dfor_numop + 1;
        return HALMAT_OK;
    }

    case POP_CFOR:
    case POP_AFOR:
        ADVANCE();
        return HALMAT_OK;

    case POP_DCAS: {
        if (numop < 2) { ADVANCE(); return HALMAT_OK; }
        halmat_val_t sel = halmat_resolve_operand(H, H->code[H->pc + 2]);
        int case_val = (sel.type == HTYPE_SCALAR)
                       ? (int)sel.v.scalar : sel.v.integer;

        uint32_t scan = H->pc + numop + 1;
        int case_idx = 0;
        int found = 0;
        uint32_t target = 0;

        while (scan < H->code_len) {
            uint32_t w = H->code[scan];
            if (HALMAT_IS_OP(w)) {
                uint32_t pop = HALMAT_POPCODE(w);
                uint32_t n   = HALMAT_NUMOP(w);
                if (pop == POP_ECAS) {
                    if (!found) {
                        H->pc = scan + n + 1;
                        return HALMAT_OK;
                    }
                    break;
                }
                if (pop == POP_CLBL) {
                    if (case_idx == case_val && !found) {
                        found = 1;
                        target = scan + n + 1;
                    }
                    case_idx++;
                }
                scan += n + 1;
            } else {
                scan++;
            }
        }

        if (found) {
            H->pc = target;
        } else {
            ADVANCE();
        }
        return HALMAT_OK;
    }

    case POP_CLBL: {
        /* Skip remaining cases — scan forward for ECAS */
        if (numop < 1) { ADVANCE(); return HALMAT_OK; }
        uint32_t exit_flow = HALMAT_DATA(H->code[H->pc + 1]);
        ADVANCE();
        {
            uint32_t scan = H->pc;
            while (scan < H->code_len) {
                uint32_t w = H->code[scan];
                if (HALMAT_IS_OP(w)) {
                    uint32_t pop = HALMAT_POPCODE(w);
                    uint32_t n   = HALMAT_NUMOP(w);
                    if (pop == POP_ECAS) {
                        H->pc = scan + n + 1;
                        return HALMAT_OK;
                    }
                    scan += n + 1;
                } else {
                    scan++;
                }
            }
        }
        (void)exit_flow;
        return HALMAT_OK;
    }

    case POP_ECAS:
        ADVANCE();
        return HALMAT_OK;

    case POP_XXST: {
        H->io.nargs = 0;
        H->io.active = 1;
        H->io.is_call = (tag != 0);
        ADVANCE();
        return HALMAT_OK;
    }

    case POP_XXAR: {
        if (numop >= 1 && H->io.active && H->io.nargs < HALMAT_MAX_IO_ARGS) {
            uint32_t ow = H->code[H->pc + 1];
            halmat_val_t val = halmat_resolve_operand(H, ow);
            uint8_t arg_type = (uint8_t)HALMAT_TAG1(ow);

            if (arg_type == 6 && val.type == HTYPE_SCALAR) {
                val.type = HTYPE_INTEGER;
                val.v.integer = (int32_t)val.v.scalar;
            }

            H->io.args[H->io.nargs] = val;
            H->io.arg_types[H->io.nargs] = arg_type;
            H->io.nargs++;
        }
        ADVANCE();
        return HALMAT_OK;
    }

    case POP_WRIT: {
        int channel = 6;
        if (numop >= 1)
            channel = (int)HALMAT_DATA(H->code[H->pc + 1]);
        halmat_io_write(H, channel, H->io.args, H->io.arg_types, H->io.nargs);
        ADVANCE();
        return HALMAT_OK;
    }

    case POP_READ: {
        ADVANCE();
        return HALMAT_OK;
    }

    case POP_RDAL:
    case POP_FILE:
        ADVANCE();
        return HALMAT_OK;

    case POP_XXND:
        H->io.active = 0;
        ADVANCE();
        return HALMAT_OK;

    case POP_PDEF:
    case POP_FDEF: {
        /* Skip body when not called */
        uint32_t exit = scan_forward(H, H->pc + numop + 1, POP_PDEF, POP_CLOS);
        if (exit >= H->code_len)
            exit = scan_forward(H, H->pc + numop + 1, POP_FDEF, POP_CLOS);
        if (exit < H->code_len) {
            uint32_t n = HALMAT_NUMOP(H->code[exit]);
            H->pc = exit + n + 1;
        } else {
            ADVANCE();
        }
        return HALMAT_OK;
    }

    case POP_FCAL:
    case POP_PCAL: {
        if (numop < 1) { ADVANCE(); return HALMAT_OK; }
        uint32_t target_syt = HALMAT_DATA(H->code[H->pc + 1]);

        if (H->frame_depth >= HALMAT_MAX_FRAMES)
            return HALMAT_ERR_STACK;
        call_frame_t *f = &H->frames[H->frame_depth++];
        f->return_pc = H->pc + numop + 1;
        f->call_addr = H->pc;

        /* Args → consecutive SYT entries after the func/proc SYT */
        for (int i = 0; i < H->io.nargs && i < 16; i++) {
            uint32_t param_syt = target_syt + 1 + (uint32_t)i;
            if (param_syt < HALMAT_MAX_SYT) {
                H->syt[param_syt].val = H->io.args[i];
                H->syt[param_syt].allocated = 1;
            }
        }

        for (uint32_t blk = 0; blk < H->num_blocks; blk++) {
            uint32_t base = blk * HALMAT_BLOCK_WORDS;
            uint32_t af = (H->code[base + 1] >> 16) & 0xFFFF;
            uint32_t scan = base + 2;
            while (scan <= base + af) {
                uint32_t w = H->code[scan];
                if (HALMAT_IS_OP(w)) {
                    uint32_t pop2 = HALMAT_POPCODE(w);
                    uint32_t n   = HALMAT_NUMOP(w);
                    if ((pop2 == POP_PDEF || pop2 == POP_FDEF) && n >= 1) {
                        uint32_t def_syt = HALMAT_DATA(H->code[scan + 1]);
                        if (def_syt == target_syt) {
                            H->pc = scan + n + 1;
                            return HALMAT_OK;
                        }
                    }
                    scan += n + 1;
                } else {
                    scan++;
                }
            }
        }

        H->frame_depth--;
        ADVANCE();
        return HALMAT_OK;
    }

    case POP_RTRN: {
        if (numop >= 1 && H->frame_depth > 0) {
            halmat_val_t ret = halmat_resolve_operand(H, H->code[H->pc + 1]);
            halmat_store_vac(H, H->frames[H->frame_depth - 1].call_addr, ret);
        }
        if (H->frame_depth > 0) {
            call_frame_t *f = &H->frames[--H->frame_depth];
            H->pc = f->return_pc;
        } else {
            ADVANCE();
        }
        return HALMAT_OK;
    }

    case POP_IDEF:
    case POP_ICLS:
    case POP_TDCL:
        ADVANCE();
        return HALMAT_OK;

    case POP_DSUB:
    case POP_TSUB:
    case POP_ADLP:
    case POP_DLPE:
    case POP_IDLP:
        ADVANCE();
        return HALMAT_OK;

    case POP_SFST:
    case POP_SFND:
    case POP_SFAR:
    case POP_BFNC:
    case POP_LFNC:
    case POP_TNEQ:
    case POP_TEQU:
    case POP_TASN:
    case POP_NASN:
        ADVANCE();
        return HALMAT_OK;

    case 0x034: /* WAIT */
    case 0x035: /* SGNL */
    case 0x036: /* CANC */
    case 0x037: /* TERM */
    case 0x038: /* PRIO */
    case 0x039: /* SCHD */
    case 0x03C: /* ERON */
    case 0x03D: /* ERSE */
    case 0x040: /* MSHP */
    case 0x041: /* VSHP */
    case 0x042: /* SSHP */
    case 0x043: /* ISHP */
    case 0x059: /* PMHD */
    case 0x05A: /* PMAR */
    case 0x05B: /* PMIN */
    case 0x055: /* NNEQ */
    case 0x056: /* NEQU */
        ADVANCE();
        return HALMAT_OK;

    default:
        fprintf(stderr, "halmat_class0: unknown popcode 0x%03X at PC=%u\n",
                popcode, H->pc);
        ADVANCE();
        return HALMAT_OK;
    }
}
