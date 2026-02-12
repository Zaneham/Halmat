#include "halmat.h"
#include "halmat_debug.h"

int halmat_debug_init(halmat_t *H)
{
    H->debug_mode = 1;
    H->single_step = 1;
    H->bp_count = 0;
    return 0;
}

int halmat_debug_check_breakpoint(halmat_t *H)
{
    for (uint32_t i = 0; i < H->bp_count; i++) {
        if (!H->breakpoints[i].enabled)
            continue;
        if (H->breakpoints[i].addr == H->pc)
            return 1;
        if (H->breakpoints[i].stmt > 0 &&
            H->breakpoints[i].stmt == H->current_stmt)
            return 1;
    }
    return 0;
}

void halmat_debug_print_state(halmat_t *H, FILE *out)
{
    fprintf(out, "PC=%u  STMT=%u  CYCLES=%llu  FRAMES=%u  LOOPS=%u  COND=%d\n",
            H->pc, H->current_stmt,
            (unsigned long long)H->cycle_count,
            H->frame_depth, H->loop_depth, H->cond_true);

    if (H->pc < H->code_len) {
        uint32_t w = H->code[H->pc];
        if (HALMAT_IS_OP(w)) {
            const char *name = halmat_popcode_name(HALMAT_POPCODE(w));
            fprintf(out, "  -> %08X  %s  (numop=%u tag=%u)\n",
                    w, name ? name : "???",
                    HALMAT_NUMOP(w), HALMAT_TAG(w));
        }
    }
}

void halmat_debug_prompt(halmat_t *H)
{
    char line[256];

    for (;;) {
        halmat_debug_print_state(H, stdout);
        printf("(halmat) ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            H->halted = 1;
            return;
        }

        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n')
            line[--len] = '\0';

        if (len == 0 || strcmp(line, "s") == 0 || strcmp(line, "step") == 0)
            return;

        if (strcmp(line, "c") == 0 || strcmp(line, "continue") == 0) {
            H->single_step = 0;
            return;
        }

        if (strcmp(line, "q") == 0 || strcmp(line, "quit") == 0) {
            H->halted = 1;
            return;
        }

        if (strncmp(line, "b ", 2) == 0 || strncmp(line, "break ", 6) == 0) {
            char *arg = strchr(line, ' ') + 1;
            if (H->bp_count < 64) {
                uint32_t addr = (uint32_t)strtoul(arg, NULL, 0);
                H->breakpoints[H->bp_count].addr = addr;
                H->breakpoints[H->bp_count].stmt = 0;
                H->breakpoints[H->bp_count].enabled = 1;
                printf("Breakpoint %u at address %u\n", H->bp_count, addr);
                H->bp_count++;
            }
            continue;
        }

        if (strncmp(line, "bs ", 3) == 0) {
            char *arg = line + 3;
            if (H->bp_count < 64) {
                uint32_t stmt = (uint32_t)strtoul(arg, NULL, 0);
                H->breakpoints[H->bp_count].addr = 0;
                H->breakpoints[H->bp_count].stmt = stmt;
                H->breakpoints[H->bp_count].enabled = 1;
                printf("Breakpoint %u at statement %u\n", H->bp_count, stmt);
                H->bp_count++;
            }
            continue;
        }

        if (strcmp(line, "info") == 0 || strcmp(line, "i") == 0) {
            printf("Breakpoints:\n");
            for (uint32_t i = 0; i < H->bp_count; i++) {
                printf("  #%u: addr=%u stmt=%u %s\n",
                       i, H->breakpoints[i].addr,
                       H->breakpoints[i].stmt,
                       H->breakpoints[i].enabled ? "enabled" : "disabled");
            }
            continue;
        }

        if (strncmp(line, "x ", 2) == 0 || strncmp(line, "syt ", 4) == 0) {
            char *arg = strchr(line, ' ') + 1;
            uint32_t idx = (uint32_t)strtoul(arg, NULL, 0);
            if (idx < HALMAT_MAX_SYT && H->syt[idx].allocated) {
                halmat_val_t *v = &H->syt[idx].val;
                printf("SYT(%u): type=%u ", idx, v->type);
                switch (v->type) {
                case HTYPE_INTEGER: printf("= %d\n", v->v.integer); break;
                case HTYPE_SCALAR:  printf("= %g\n", v->v.scalar); break;
                case HTYPE_CHAR:
                    printf("= \"%.*s\"\n", (int)v->v.string.len, v->v.string.data);
                    break;
                case HTYPE_BIT:     printf("= 0x%X\n", v->v.bits); break;
                default:            printf("= ?\n"); break;
                }
            } else {
                printf("SYT(%u): not allocated\n", idx);
            }
            continue;
        }

        if (strcmp(line, "disasm") == 0 || strcmp(line, "d") == 0) {
            halmat_disasm_word(H, H->pc, stdout);
            continue;
        }

        printf("Commands: s(tep) c(ontinue) q(uit) b <addr> bs <stmt> "
               "i(nfo) x <syt> d(isasm)\n");
    }
}

void halmat_disasm_word(halmat_t *H, uint32_t addr, FILE *out)
{
    if (addr >= H->code_len) return;
    uint32_t w = H->code[addr];
    if (!HALMAT_IS_OP(w)) {
        fprintf(out, "  %4u: %08X  (operand)\n", addr, w);
        return;
    }
    uint32_t pop = HALMAT_POPCODE(w);
    uint32_t numop = HALMAT_NUMOP(w);
    uint32_t cls = HALMAT_CLASS(w);
    const char *name = halmat_popcode_name(pop);
    fprintf(out, "  %4u: %08X  %s/%s  (%u ops)\n",
            addr, w, halmat_class_name(cls), name ? name : "???", numop);
    for (uint32_t j = 1; j <= numop && (addr + j) < H->code_len; j++) {
        uint32_t ow = H->code[addr + j];
        if (HALMAT_IS_OPERAND(ow)) {
            fprintf(out, "        %08X    %s(%u)\n",
                    ow, halmat_qual_name(HALMAT_QUAL(ow)), HALMAT_DATA(ow));
        }
    }
}
