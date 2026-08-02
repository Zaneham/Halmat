#include "halmat.h"
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

static uint32_t read_be32(FILE *fp)
{
    uint8_t buf[4];
    if (fread(buf, 1, 4, fp) != 4)
        return 0;
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |  (uint32_t)buf[3];
}

void halmat_init(halmat_t *H)
{
    memset(H, 0, sizeof(*H));
    H->num_blanks = 5; /* United Space Alliance default */
}

int halmat_load(halmat_t *H, const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "halmat_load: cannot open %s\n", filename);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint32_t nblocks = (uint32_t)((size + HALMAT_BLOCK_BYTES - 1) / HALMAT_BLOCK_BYTES);
    if (nblocks > HALMAT_MAX_BLOCKS) {
        fprintf(stderr, "halmat_load: too many blocks (%u > %d)\n",
                nblocks, HALMAT_MAX_BLOCKS);
        fclose(fp);
        return -1;
    }

    for (uint32_t blk = 0; blk < nblocks; blk++) {
        uint32_t base = blk * HALMAT_BLOCK_WORDS;
        for (uint32_t i = 0; i < HALMAT_BLOCK_WORDS; i++) {
            H->code[base + i] = read_be32(fp);
        }
    }

    H->num_blocks = nblocks;
    H->code_len = nblocks * HALMAT_BLOCK_WORDS;
    H->pc = 2;  /* First operator is at word 2 (after metadata) */

    fclose(fp);
    return 0;
}

#define LIT_PAGE_SIZE 130

int halmat_load_litfile(halmat_t *H, const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint32_t npages = (uint32_t)(size / (LIT_PAGE_SIZE * 3 * 4));
    uint32_t total = npages * LIT_PAGE_SIZE;

    if (total > HALMAT_MAX_LIT)
        total = HALMAT_MAX_LIT;

    /* Three parallel arrays per page: lit1 (type), lit2 (hi), lit3 (lo) */
    for (uint32_t pg = 0; pg < npages; pg++) {
        uint32_t base = pg * LIT_PAGE_SIZE;
        for (uint32_t i = 0; i < LIT_PAGE_SIZE; i++) {
            uint32_t idx = base + i;
            int32_t w = (int32_t)read_be32(fp);
            if (idx < HALMAT_MAX_LIT)
                H->lit[idx].lit1 = w;
        }
        for (uint32_t i = 0; i < LIT_PAGE_SIZE; i++) {
            uint32_t idx = base + i;
            int32_t w = (int32_t)read_be32(fp);
            if (idx < HALMAT_MAX_LIT)
                H->lit[idx].lit2 = w;
        }
        for (uint32_t i = 0; i < LIT_PAGE_SIZE; i++) {
            uint32_t idx = base + i;
            int32_t w = (int32_t)read_be32(fp);
            if (idx < HALMAT_MAX_LIT)
                H->lit[idx].lit3 = w;
        }
    }

    for (uint32_t i = 0; i < total && i < HALMAT_MAX_LIT; i++)
        H->lit[i].type = (uint8_t)(H->lit[i].lit1 & 0xFF);

    H->lit_count = total;
    fclose(fp);
    return 0;
}

void halmat_build_flow_table(halmat_t *H)
{
    /* Pre-scan for LBL operators to build flow number → address mapping.
     * Loop targets (DTST, DFOR, DSMP, DCAS) are registered at runtime. */
    for (uint32_t blk = 0; blk < H->num_blocks; blk++) {
        uint32_t base = blk * HALMAT_BLOCK_WORDS;
        uint32_t atom_fault = (H->code[base + 1] >> 16) & 0xFFFF;
        uint32_t i = base + 2;
        uint32_t end = base + atom_fault;

        while (i <= end) {
            uint32_t w = H->code[i];
            if (HALMAT_IS_OP(w)) {
                uint32_t pop = HALMAT_POPCODE(w);
                uint32_t numop = HALMAT_NUMOP(w);

                if (pop == POP_LBL && numop >= 1) {
                    uint32_t operand = H->code[i + 1];
                    uint32_t flow_num = HALMAT_DATA(operand);
                    if (flow_num < HALMAT_MAX_FLOW)
                        H->flow[flow_num] = i;
                }

                i += numop + 1;
            } else {
                i++;
            }
        }
    }
}

int halmat_load_strings(halmat_t *H, const char *source_file)
{
    FILE *fp = fopen(source_file, "r");
    if (!fp) return -1;

    char source[16384];
    size_t slen = fread(source, 1, sizeof(source) - 1, fp);
    source[slen] = '\0';
    fclose(fp);

    char *strings[256];
    int   str_lens[256];
    int   nstrings = 0;

    char *p = source;
    while (*p && nstrings < 256) {
        char *q = strchr(p, '\'');
        if (!q) break;
        char *end = q + 1;
        while (*end) {
            if (*end == '\'') {
                if (*(end + 1) == '\'') {
                    end += 2;  /* skip escaped quote */
                    continue;
                }
                break;
            }
            end++;
        }
        if (*end != '\'') break;

        int len = (int)(end - q - 1);
        if (len > 0) {
            strings[nstrings] = q + 1;
            str_lens[nstrings] = len;
            nstrings++;
        }
        p = end + 1;
    }

    int str_idx = 0;
    H->lit_str_pool_used = 1; /* reserve offset 0 as "not loaded" sentinel */

    for (uint32_t i = 0; i < H->lit_count && str_idx < nstrings; i++) {
        if (H->lit[i].lit1 != 0) continue; /* not CHAR type */
        if (H->lit[i].lit2 == 0) continue; /* null/unused entry */

        int expected_len = (int)(((H->lit[i].lit2 >> 24) & 0xFF) + 1);

        /* Try to match: lengths should agree */
        if (str_lens[str_idx] == expected_len) {
            uint32_t off = H->lit_str_pool_used;
            if (off + expected_len + 1 <= HALMAT_LIT_STR_POOL) {
                memcpy(H->lit_str_pool + off, strings[str_idx], expected_len);
                H->lit_str_pool[off + expected_len] = '\0';
                H->lit_str_off[i] = (uint16_t)off;
                H->lit_str_len[i] = (uint16_t)expected_len;
                H->lit_str_pool_used = off + expected_len + 1;
            }
            str_idx++;
        } else {
            /* Length mismatch — skip this source string and try next */
            str_idx++;
            i--; /* retry this LIT entry with the next source string */
        }
    }

    return 0;
}

/* ---- COMMON0 Memory Image ---- */

static const uint8_t ebc2asc[256] = {
    0x00,0x01,0x02,0x03,0x9C,0x09,0x86,0x7F,
    0x97,0x8D,0x8E,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x9D,0x85,0x08,0x87,
    0x18,0x19,0x92,0x8F,0x1C,0x1D,0x1E,0x1F,
    0x80,0x81,0x82,0x83,0x84,0x0A,0x17,0x1B,
    0x88,0x89,0x8A,0x8B,0x8C,0x05,0x06,0x07,
    0x90,0x91,0x16,0x93,0x94,0x95,0x96,0x04,
    0x98,0x99,0x9A,0x9B,0x14,0x15,0x9E,0x1A,
    0x20,0xA0,0xE2,0xE4,0xE0,0xE1,0xE3,0xE5,
    0xE7,0xF1,0xA2,0x2E,0x3C,0x28,0x2B,0x7C,
    0x26,0xE9,0xEA,0xEB,0xE8,0xED,0xEE,0xEF,
    0xEC,0xDF,0x21,0x24,0x2A,0x29,0x3B,0xAC,
    0x2D,0x2F,0xC2,0xC4,0xC0,0xC1,0xC3,0xC5,
    0xC7,0xD1,0xA6,0x2C,0x25,0x5F,0x3E,0x3F,
    0xF8,0xC9,0xCA,0xCB,0xC8,0xCD,0xCE,0xCF,
    0xCC,0x60,0x3A,0x23,0x40,0x27,0x3D,0x22,
    0xD8,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
    0x68,0x69,0xAB,0xBB,0xF0,0xFD,0xFE,0xB1,
    0xB0,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,0x70,
    0x71,0x72,0xAA,0xBA,0xE6,0xB8,0xC6,0xA4,
    0xB5,0x7E,0x73,0x74,0x75,0x76,0x77,0x78,
    0x79,0x7A,0xA1,0xBF,0xD0,0x5B,0xDE,0xAE,
    0x5E,0xA3,0xA5,0xB7,0xA9,0xA7,0xB6,0xBC,
    0xBD,0xBE,0xDD,0xA8,0xAF,0x5D,0xB4,0xD7,
    0x7B,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
    0x48,0x49,0xAD,0xF4,0xF6,0xF2,0xF3,0xF5,
    0x7D,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,
    0x51,0x52,0xB9,0xFB,0xFC,0xF9,0xFA,0xFF,
    0x5C,0xF7,0x53,0x54,0x55,0x56,0x57,0x58,
    0x59,0x5A,0xB2,0xD4,0xD6,0xD2,0xD3,0xD5,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
    0x38,0x39,0xB3,0xDB,0xDC,0xD9,0xDA,0x9F,
};

int halmat_load_common0(halmat_t *H, const char *filename)
{
    size_t flen = strlen(filename);
    int is_gz = (flen > 3 && strcmp(filename + flen - 3, ".gz") == 0);

    H->mem_image = (uint8_t *)malloc(HALMAT_MEM_SIZE);
    if (!H->mem_image) return -1;
    memset(H->mem_image, 0, HALMAT_MEM_SIZE);

#ifdef HAVE_ZLIB
    if (is_gz) {
        gzFile gz = gzopen(filename, "rb");
        if (!gz) { free(H->mem_image); H->mem_image = NULL; return -2; }
        int nread = gzread(gz, H->mem_image, HALMAT_MEM_SIZE);
        gzclose(gz);
        (void)nread; /* empty is valid */
        H->mem_image_loaded = 1;
        return 0;
    }
#else
    if (is_gz) {
        char cmd[600];
        snprintf(cmd, sizeof(cmd), "gzip -dc \"%s\" 2>/dev/null", filename);
        FILE *fp = popen(cmd, "r");
        if (!fp) { free(H->mem_image); H->mem_image = NULL; return -2; }
        size_t nread = fread(H->mem_image, 1, HALMAT_MEM_SIZE, fp);
        pclose(fp);
        (void)nread; /* empty is valid */
        H->mem_image_loaded = 1;
        return 0;
    }
#endif

    FILE *fp = fopen(filename, "rb");
    if (!fp) { free(H->mem_image); H->mem_image = NULL; return -2; }
    size_t nread = fread(H->mem_image, 1, HALMAT_MEM_SIZE, fp);
    fclose(fp);
    /* Empty file is valid (no CHARACTER literals). Memory stays zeroed. */
    H->mem_image_loaded = 1;
    (void)nread;
    return 0;
}

void halmat_free_common0(halmat_t *H)
{
    if (H->mem_image) {
        free(H->mem_image);
        H->mem_image = NULL;
    }
    H->mem_image_loaded = 0;
}

/* COMMON0.out is the text TSV dump of the compiler's symbol table.
 * Without it we can't distinguish SCALAR from SCALAR DOUBLE or
 * INTEGER from INTEGER DOUBLE — the HALMAT operand words don't carry
 * precision, only the SYM_FLAGS in this file do. Format documented by
 * Ron Burkey's unHALMAT.py. */

#define SYM_FLAG_SINGLE 0x00800000u
#define SYM_FLAG_DOUBLE 0x00400000u

static int field_at(const char *line, int idx, char *out, int out_sz)
{
    int cur = 0;
    const char *p = line;
    while (cur < idx) {
        const char *t = strchr(p, '\t');
        if (!t) return -1;
        p = t + 1;
        cur++;
    }
    const char *end = strchr(p, '\t');
    if (!end) end = p + strlen(p);
    /* strip trailing CR/LF on last field */
    while (end > p && (end[-1] == '\n' || end[-1] == '\r')) end--;
    int n = (int)(end - p);
    if (n >= out_sz) n = out_sz - 1;
    memcpy(out, p, n);
    out[n] = '\0';
    return n;
}

static uint8_t map_sym_type(uint32_t st)
{
    switch (st & 0xFF) {
    case 0x01: return HTYPE_BIT;
    case 0x02: return HTYPE_CHAR;
    case 0x03: return HTYPE_MATRIX;
    case 0x04: return HTYPE_VECTOR;
    case 0x05: return HTYPE_SCALAR;
    case 0x06: return HTYPE_INTEGER;
    case 0x09: return HTYPE_EVENT;
    case 0x0A: return HTYPE_STRUCT;
    default:   return HTYPE_NONE;
    }
}

int halmat_load_symtab(halmat_t *H, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;

    char line[1024];
    int  in_symtab = 0;
    int  cur_idx = -1;

    while (fgets(line, sizeof(line), fp)) {
        if (!in_symtab) {
            if (line[0] != '/') continue;
            in_symtab = 1;
        }

        char f0[8], f1[64];
        if (field_at(line, 0, f0, sizeof(f0)) < 0) break;

        if (f0[0] == '/') {
            char f2[16], f3[16];
            if (field_at(line, 1, f1, sizeof(f1)) < 0) break;
            if (field_at(line, 2, f2, sizeof(f2)) < 0) break;
            if (field_at(line, 3, f3, sizeof(f3)) < 0) break;
            if (strcmp(f1, "SYMuTAB") != 0 || strcmp(f3, "BASED") != 0)
                break;
            cur_idx = atoi(f2);
            if (cur_idx < 0 || cur_idx >= HALMAT_MAX_SYT) {
                cur_idx = -1;
                continue;
            }
            if ((uint32_t)cur_idx >= H->syt_count)
                H->syt_count = (uint32_t)cur_idx + 1;
        } else if (f0[0] == '.' && cur_idx >= 0) {
            if (field_at(line, 1, f1, sizeof(f1)) < 0) continue;
            char fv[64];
            if (field_at(line, 4, fv, sizeof(fv)) < 0) continue;

            syt_entry_t *e = &H->syt[cur_idx];
            if (strcmp(f1, "SYM_TYPE") == 0) {
                uint32_t st = (uint32_t)strtoul(fv, NULL, 16);
                uint8_t  ht = map_sym_type(st);
                if (ht != HTYPE_NONE) {
                    e->val.type = ht;
                    e->declared = 1;
                }
            } else if (strcmp(f1, "SYM_FLAGS") == 0) {
                uint32_t sf = (uint32_t)strtoul(fv, NULL, 16);
                if (sf & SYM_FLAG_DOUBLE)      e->val.precision = HPREC_DOUBLE;
                else if (sf & SYM_FLAG_SINGLE) e->val.precision = HPREC_SINGLE;
            } else if (strcmp(f1, "SYM_LENGTH") == 0) {
                /* MATRIX packs its shape as rows<<8|cols — see MULTIPLY.xpl's
                   SHL(PSEUDO_LENGTH,8) juggling. VECTOR is a plain length. */
                uint32_t sl = (uint32_t)strtoul(fv, NULL, 16);
                e->val.rows = (uint8_t)((sl >> 8) & 0xFF);
                e->val.cols = (uint8_t)(sl & 0xFF);
                e->val.blen = (uint8_t)(sl & 0xFF);
            }
        } else {
            break;
        }
    }

    /* Language Spec 4.5: MATRIX with no size specification is 3x3, VECTOR
       is a 3-vector. SYM_LENGTH may arrive before SYM_TYPE, so reconcile
       shape against type once the whole table is in. */
    for (uint32_t i = 0; i < H->syt_count; i++) {
        halmat_val_t *v = &H->syt[i].val;
        if (v->type == HTYPE_MATRIX) {
            if (v->rows == 0 || v->cols == 0) { v->rows = 3; v->cols = 3; }
        } else if (v->type == HTYPE_VECTOR) {
            if (v->cols != 0) v->rows = v->cols;   /* packed as a plain length */
            if (v->rows == 0) v->rows = 3;
            v->cols = 1;
        } else {
            v->rows = v->cols = 0;
        }
        if (v->type != HTYPE_BIT) v->blen = 0;
    }

    fclose(fp);
    return 0;
}

void halmat_decode_char_lit(halmat_t *H, uint32_t lit_idx, char *buf, int *len)
{
    if (lit_idx >= H->lit_count) {
        *len = 0;
        buf[0] = '\0';
        return;
    }

    uint32_t lit2 = (uint32_t)H->lit[lit_idx].lit2;
    int slen = (int)(((lit2 >> 24) & 0xFF) + 1);

    /* COMMON0 memory image */
    if (H->mem_image_loaded) {
        uint32_t addr = lit2 & 0x00FFFFFF;
        if (addr + (uint32_t)slen <= HALMAT_MEM_SIZE) {
            for (int i = 0; i < slen; i++)
                buf[i] = (char)ebc2asc[H->mem_image[addr + i]];
            buf[slen] = '\0';
            *len = slen;
            return;
        }
    }

    /* SOURCECO.txt fallback */
    if (H->lit_str_off[lit_idx] > 0 && H->lit_str_len[lit_idx] > 0) {
        int plen = H->lit_str_len[lit_idx];
        memcpy(buf, H->lit_str_pool + H->lit_str_off[lit_idx], plen);
        buf[plen] = '\0';
        *len = plen;
        return;
    }

    /* Last resort: embedded lit2 bytes */
    int pos = 0;
    if (pos < slen) buf[pos++] = (char)((lit2 >> 16) & 0xFF);
    if (pos < slen) buf[pos++] = (char)((lit2 >> 8) & 0xFF);
    if (pos < slen) buf[pos++] = (char)(lit2 & 0xFF);

    uint32_t ext = 1;
    while (pos < slen) {
        uint32_t idx = lit_idx + ext;
        if (idx >= H->lit_count) break;
        uint32_t w = (uint32_t)H->lit[idx].lit2;
        if (pos < slen) buf[pos++] = (char)((w >> 24) & 0xFF);
        if (pos < slen) buf[pos++] = (char)((w >> 16) & 0xFF);
        if (pos < slen) buf[pos++] = (char)((w >> 8) & 0xFF);
        if (pos < slen) buf[pos++] = (char)(w & 0xFF);
        ext++;
    }

    *len = slen;
    buf[slen] = '\0';
}
