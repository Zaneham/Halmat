/* hm_abend.c — ABEND dump formatter for yaHALMAT
 *
 * Produces IBM S/370-style dumps when the emulator encounters
 * a runtime fault or receives a fatal signal. Two paths:
 *   1. hm_adump() — full fprintf formatting (non-signal context)
 *   2. hm_sigh()  — async-signal-safe minimal banner via write(2)
 *
 * Because if your emulator of a 1970s spacecraft computer doesn't
 * produce mainframe dumps, what exactly are you doing with your life?
 */
#include "hm_abend.h"
#include <signal.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#define WRITE_FD(fd, buf, n) _write(fd, buf, (unsigned)(n))
#else
#include <unistd.h>
#define WRITE_FD(fd, buf, n) write(fd, buf, n)
#endif

/* ---- Code Table ---- */

typedef struct { uint16_t code; const char *tag; const char *desc; } hm_ctab_t;

static const hm_ctab_t ctab[] = {
    { HM_S0C1, "S0C1", "OPERATION EXCEPTION"  },
    { HM_S0C4, "S0C4", "PROTECTION EXCEPTION" },
    { HM_S0C7, "S0C7", "DATA EXCEPTION"       },
    { HM_S0CB, "S0CB", "MACHINE CHECK"        },
    { HM_U0101, "U0101", "UNKNOWN ERROR"       },
    { HM_U0102, "U0102", "BAD OPCODE"          },
    { HM_U0103, "U0103", "BAD QUALIFIER"       },
    { HM_U0104, "U0104", "POOL OVERFLOW"       },
    { HM_U0105, "U0105", "I/O ERROR"           },
    { HM_U0106, "U0106", "STACK OVERFLOW"      },
    { HM_U0107, "U0107", "ARRAY BOUNDS"        },
    { HM_U0108, "U0108", "DIVISION BY ZERO"    },
    { 0, NULL, NULL }
};

static const hm_ctab_t *hm_find(uint16_t code)
{
    for (int i = 0; ctab[i].tag; i++)
        if (ctab[i].code == code) return &ctab[i];
    return NULL;
}

const char *hm_atag(uint16_t code)
{
    const hm_ctab_t *e = hm_find(code);
    return e ? e->tag : "????";
}

const char *hm_aname(uint16_t code)
{
    const hm_ctab_t *e = hm_find(code);
    return e ? e->desc : "UNKNOWN ABEND";
}

uint16_t hm_etou(int err)
{
    /* HALMAT_ERR_* are -1..-8 → HM_U0101..HM_U0108 */
    if (err >= -8 && err <= -1)
        return (uint16_t)(HM_U0101 + ((-err) - 1));
    return HM_U0101; /* fallback: unknown */
}

/* ---- Signal Handler (async-signal-safe) ---- */

static hm_abctx_t *g_abctx;

/* write(2) wrappers — no fprintf, no malloc, no fun */
static void wr_str(const char *s)
{
    if (!s) return;
    int n = 0;
    while (s[n]) n++;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    WRITE_FD(2, s, n);
#pragma GCC diagnostic pop
}

static void wr_uint(unsigned v)
{
    char buf[12];
    int i = 11;
    buf[i] = '\0';
    if (v == 0) { buf[--i] = '0'; }
    else { while (v > 0) { buf[--i] = '0' + (v % 10); v /= 10; } }
    wr_str(&buf[i]);
}

static void hm_sigh(int sig)
{
    if (!g_abctx || !g_abctx->hm) _exit(128 + sig);

    switch (sig) {
#ifndef _WIN32
    case SIGILL:  g_abctx->code = HM_S0C1; break;
#endif
    case SIGSEGV: g_abctx->code = HM_S0C4; break;
    case SIGFPE:  g_abctx->code = HM_S0C7; break;
    case SIGABRT: g_abctx->code = HM_S0CB; break;
    default:      g_abctx->code = HM_S0CB; break;
    }

    /* Minimal banner — this is all we dare do in a signal handler.
     * Full dumps are for calmer times, not mid-explosion. */
    wr_str("\n ============================================\n");
    wr_str("  ABEND "); wr_str(hm_atag(g_abctx->code));
    wr_str("    "); wr_str(hm_aname(g_abctx->code));
    wr_str("\n  yaHALMAT      PC=");
    wr_uint(g_abctx->hm->pc);
    wr_str("  Cycle "); wr_uint((unsigned)g_abctx->hm->cycle_count);
    wr_str("\n  Call depth "); wr_uint(g_abctx->hm->frame_depth);
    wr_str("  Loop depth "); wr_uint(g_abctx->hm->loop_depth);
    wr_str("\n ============================================\n");
    _exit(128 + sig);
}

void hm_ainit(hm_abctx_t *A)
{
    memset(A, 0, sizeof(*A));
    g_abctx = A;
    signal(SIGSEGV, hm_sigh);
    signal(SIGFPE,  hm_sigh);
    signal(SIGABRT, hm_sigh);
#ifndef _WIN32
    signal(SIGILL,  hm_sigh);
#endif
}

/* ---- Type Name Helper ---- */

static const char *hm_tname(uint8_t t)
{
    switch (t) {
    case HTYPE_NONE:    return "NONE";
    case HTYPE_BIT:     return "BIT";
    case HTYPE_CHAR:    return "CHAR";
    case HTYPE_MATRIX:  return "MATRIX";
    case HTYPE_VECTOR:  return "VECTOR";
    case HTYPE_SCALAR:  return "SCALAR";
    case HTYPE_INTEGER: return "INTEGER";
    case HTYPE_BOOLEAN: return "BOOLEAN";
    case HTYPE_EVENT:   return "EVENT";
    case HTYPE_STRUCT:  return "STRUCT";
    default:            return "???";
    }
}

/* ---- Error Name Helper ---- */

static const char *hm_ername(int err)
{
    switch (err) {
    case HALMAT_OK:            return "HALMAT_OK";
    case HALMAT_HALT:          return "HALMAT_HALT";
    case HALMAT_ERR_UNKNOWN:   return "HALMAT_ERR_UNKNOWN";
    case HALMAT_ERR_BAD_OP:    return "HALMAT_ERR_BAD_OP";
    case HALMAT_ERR_BAD_QUAL:  return "HALMAT_ERR_BAD_QUAL";
    case HALMAT_ERR_OVERFLOW:  return "HALMAT_ERR_OVERFLOW";
    case HALMAT_ERR_IO:        return "HALMAT_ERR_IO";
    case HALMAT_ERR_STACK:     return "HALMAT_ERR_STACK";
    case HALMAT_ERR_BOUNDS:    return "HALMAT_ERR_BOUNDS";
    case HALMAT_ERR_DIV_ZERO:  return "HALMAT_ERR_DIV_ZERO";
    default:                   return "???";
    }
}

/* ---- Pool Row ---- */

static void hm_pool(FILE *f, const char *name, unsigned used,
                     unsigned limit)
{
    unsigned pct = limit ? (used * 100 / limit) : 0;
    fprintf(f, "   %-18s %8u  %8u   %3u%%\n", name, used, limit, pct);
}

/* ---- Full Dump ---- */

void hm_adump(const hm_abctx_t *A, FILE *out)
{
    const halmat_t *H = A->hm;
    if (!H) return;

    /* Timestamp — because even crash dumps deserve punctuality */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[80];
    memcpy(ts, "0000-00-00 00:00:00", 20);
    if (tm)
        sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d",
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec);

    /* ---- Header ---- */
    fprintf(out,
        "\n"
        " ============================================================\n"
        "  ABEND %-8s %s\n"
        "  yaHALMAT      PC=%-5u  Cycle %-8u  %s\n"
        "  SOURCE %s\n"
        " ============================================================\n",
        hm_atag(A->code), hm_aname(A->code),
        H->pc, (unsigned)H->cycle_count, ts,
        A->src ? A->src : "(unknown)");

    /* ---- PSW ---- */
    fprintf(out,
        "\n PSW:\n"
        "   PC               %u\n"
        "   Statement        %u (from SMRK)\n"
        "   Cycles           %llu\n"
        "   Statements       %llu\n"
        "   Condition        %s\n"
        "   Halted           %d (%s)\n",
        H->pc,
        H->current_stmt,
        (unsigned long long)H->cycle_count,
        (unsigned long long)H->stmt_count,
        H->cond_true ? "TRUE" : "FALSE",
        H->halted, hm_ername(H->halted));

    /* ---- Current Instruction ---- */
    if (H->pc < H->code_len) {
        uint32_t w = H->code[H->pc];
        if (HALMAT_IS_OP(w)) {
            uint32_t pop  = HALMAT_POPCODE(w);
            uint32_t cls  = HALMAT_CLASS(w);
            uint32_t nop  = HALMAT_NUMOP(w);
            const char *nm = halmat_popcode_name(pop);

            fprintf(out, "\n CURRENT INSTRUCTION:\n");
            fprintf(out, "   [%u] %s  (%u ops, class %u)\n",
                    H->pc, nm ? nm : "???", nop, cls);

            /* Operands — decode each one so the poor sod reading
             * this dump can see what we were trying to chew on */
            for (uint32_t j = 1; j <= nop; j++) {
                uint32_t idx = H->pc + j;
                if (idx >= H->code_len) break;
                uint32_t ow = H->code[idx];
                if (!HALMAT_IS_OPERAND(ow)) break;
                uint32_t data = HALMAT_DATA(ow);
                uint32_t qual = HALMAT_QUAL(ow);
                const char *qn = halmat_qual_name(qual);
                fprintf(out, "     op%u: %s[%u]\n", j, qn, data);
            }
        }
    }

    /* ---- Call Stack ---- */
    fprintf(out, "\n CALL STACK (depth %u):\n", H->frame_depth);
    if (H->frame_depth == 0) {
        fprintf(out, "   (empty)\n");
    } else {
        uint32_t n = H->frame_depth;
        if (n > HALMAT_MAX_FRAMES) n = HALMAT_MAX_FRAMES;
        for (uint32_t i = 0; i < n; i++) {
            fprintf(out, "   #%-3u return=%-6u call=%-6u syt_base=%u\n",
                    i, H->frames[i].return_pc,
                    H->frames[i].call_addr,
                    H->frames[i].syt_base);
        }
    }

    /* ---- Loop Stack ---- */
    fprintf(out, "\n LOOP STACK (depth %u):\n", H->loop_depth);
    if (H->loop_depth == 0) {
        fprintf(out, "   (empty)\n");
    } else {
        uint32_t n = H->loop_depth;
        if (n > HALMAT_MAX_LOOPS) n = HALMAT_MAX_LOOPS;
        for (uint32_t i = 0; i < n; i++) {
            const char *kind = H->loops[i].is_discrete ? "FOR" :
                               (H->loops[i].tag == 0 ? "WHILE" : "UNTIL");
            fprintf(out, "   #%-3u flow=%-4u cmp=%-6u %s\n",
                    i, H->loops[i].flow_num,
                    H->loops[i].cmp_addr, kind);
        }
    }

    /* ---- Pool Occupancy ---- */
    fprintf(out,
        "\n POOL OCCUPANCY:\n"
        "   %-18s %8s  %8s   %s\n",
        "Pool", "Used", "Limit", "%Full");

    hm_pool(out, "code words",  H->code_len,         HALMAT_MAX_CODE);
    hm_pool(out, "symbols",     H->syt_count,        HALMAT_MAX_SYT);
    hm_pool(out, "literals",    H->lit_count,         HALMAT_MAX_LIT);
    hm_pool(out, "data bytes",  H->data_used,         HALMAT_DATA_SIZE);
    hm_pool(out, "flow table",  H->code_len ? HALMAT_MAX_FLOW : 0,
                                              HALMAT_MAX_FLOW);
    hm_pool(out, "call frames", H->frame_depth,       HALMAT_MAX_FRAMES);
    hm_pool(out, "loop stack",  H->loop_depth,        HALMAT_MAX_LOOPS);
    hm_pool(out, "lit strings", H->lit_str_pool_used,  HALMAT_LIT_STR_POOL);

    /* ---- Active Symbols ---- */
    {
        /* Count allocated symbols first */
        uint32_t cnt = 0;
        for (uint32_t i = 0; i < HALMAT_MAX_SYT && cnt < 9999; i++)
            if (H->syt[i].allocated) cnt++;

        fprintf(out, "\n ACTIVE SYMBOLS (%u, showing max 32):\n", cnt);

        uint32_t shown = 0;
        for (uint32_t i = 0; i < HALMAT_MAX_SYT && shown < 32; i++) {
            if (!H->syt[i].allocated) continue;
            const halmat_val_t *v = &H->syt[i].val;
            fprintf(out, "   SYT[%-4u] %-9s ", i, hm_tname(v->type));
            switch (v->type) {
            case HTYPE_INTEGER:
                fprintf(out, "%d", v->v.integer);
                break;
            case HTYPE_SCALAR:
                fprintf(out, "%g", v->v.scalar);
                break;
            case HTYPE_BIT:
                fprintf(out, "0x%X", v->v.bits);
                break;
            case HTYPE_CHAR:
                fprintf(out, "\"%.*s\"",
                        (int)v->v.string.len, v->v.string.data);
                break;
            case HTYPE_BOOLEAN:
                fprintf(out, "%s", v->v.integer ? "TRUE" : "FALSE");
                break;
            case HTYPE_VECTOR:
                fprintf(out, "(%u)", v->rows);
                break;
            case HTYPE_MATRIX:
                fprintf(out, "(%ux%u)", v->rows, v->cols);
                break;
            default:
                fprintf(out, "-");
                break;
            }
            fprintf(out, "\n");
            shown++;
        }
        if (cnt > 32)
            fprintf(out, "   ... (%u more)\n", cnt - 32);
    }

    /* ---- Footer ---- */
    fprintf(out,
        "\n"
        " ============================================================\n"
        "  END OF DUMP    %-8s %s\n"
        " ============================================================\n",
        hm_atag(A->code),
        A->src ? A->src : "(unknown)");
}

/* ---- Explicit ABEND ---- */

void hm_abend(hm_abctx_t *A, uint16_t code)
{
    A->code = code;
    if (A->dump)
        hm_adump(A, stderr);
}
