#include "halmat.h"

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
    if (!fp) {
        fprintf(stderr, "halmat_load_litfile: cannot open %s\n", filename);
        return -1;
    }

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

/* litfile.bin only has string descriptors, not the actual bytes.
 * Recover them by extracting single-quoted strings from HAL/S source
 * and matching to CHAR entries by order of appearance. */
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

void halmat_decode_char_lit(halmat_t *H, uint32_t lit_idx, char *buf, int *len)
{
    if (lit_idx >= H->lit_count) {
        *len = 0;
        buf[0] = '\0';
        return;
    }

    if (H->lit_str_off[lit_idx] > 0 && H->lit_str_len[lit_idx] > 0) {
        int slen = H->lit_str_len[lit_idx];
        memcpy(buf, H->lit_str_pool + H->lit_str_off[lit_idx], slen);
        buf[slen] = '\0';
        *len = slen;
        return;
    }

    /* Fallback: read from lit2 lower bytes */
    uint32_t lit2 = (uint32_t)H->lit[lit_idx].lit2;
    int slen = (int)(((lit2 >> 24) & 0xFF) + 1);

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
