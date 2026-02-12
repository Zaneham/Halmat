#ifndef HALMAT_H
#define HALMAT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "halmat_types.h"

#define HALMAT_BLOCK_WORDS  1800
#define HALMAT_BLOCK_BYTES  (HALMAT_BLOCK_WORDS * 4)
#define HALMAT_MAX_BLOCKS   256
#define HALMAT_MAX_CODE     (HALMAT_BLOCK_WORDS * HALMAT_MAX_BLOCKS)
#define HALMAT_MAX_SYT      4096
#define HALMAT_MAX_LIT      4096
#define HALMAT_MAX_VAC      4096        /* power of 2, direct-mapped */
#define HALMAT_MAX_FLOW     2048
#define HALMAT_MAX_FRAMES   256
#define HALMAT_MAX_LOOPS    64
#define HALMAT_DATA_SIZE    (1 << 20)   /* 1 MB data segment */
#define HALMAT_LIT_STR_POOL 16384       /* character literal string pool */
#define HALMAT_MAX_UNITS    16

/* Operator word: [TAG:8][NUMOP:8][CLASS:4][OPCODE:8][COPT:3][0:1] */
#define HALMAT_IS_OP(w)       (((w) & 1) == 0)
#define HALMAT_TAG(w)         (((w) >> 24) & 0xFF)
#define HALMAT_NUMOP(w)       (((w) >> 16) & 0xFF)
#define HALMAT_POPCODE(w)     (((w) >> 4) & 0xFFF)
#define HALMAT_CLASS(w)       (((w) >> 12) & 0xF)
#define HALMAT_OPCODE(w)      (((w) >> 4) & 0xFF)
#define HALMAT_COPT(w)        (((w) >> 1) & 0x7)

/* Operand word: [DATA:16][TAG1:8][QUAL:4][TAG2:3][1:1] */
#define HALMAT_IS_OPERAND(w)  (((w) & 1) == 1)
#define HALMAT_DATA(w)        (((w) >> 16) & 0xFFFF)
#define HALMAT_TAG1(w)        (((w) >> 8) & 0xFF)
#define HALMAT_QUAL(w)        (((w) >> 4) & 0xF)
#define HALMAT_TAG2(w)        (((w) >> 1) & 0x7)

#define QUAL_NONE  0
#define QUAL_SYT   1    /* Symbol table pointer */
#define QUAL_INL   2    /* Internal flow number */
#define QUAL_VAC   3    /* Virtual accumulator */
#define QUAL_XPT   4    /* Extended pointer */
#define QUAL_LIT   5    /* Literal table pointer */
#define QUAL_IMD   6    /* Immediate value */
#define QUAL_AST   7    /* Asterisk pointer */
#define QUAL_CSZ   8    /* Component size */
#define QUAL_ASZ   9    /* Array/copy size */
#define QUAL_OFF  10    /* Offset value */

/* Class 0: Control/Subscript */
#define POP_NOP   0x000
#define POP_EXTN  0x001
#define POP_XREC  0x002
#define POP_IMRK  0x003
#define POP_SMRK  0x004
#define POP_PXRC  0x005
#define POP_IFHD  0x007
#define POP_LBL   0x008
#define POP_BRA   0x009
#define POP_FBRA  0x00A
#define POP_DCAS  0x00B
#define POP_ECAS  0x00C
#define POP_CLBL  0x00D
#define POP_DTST  0x00E
#define POP_ETST  0x00F
#define POP_DFOR  0x010
#define POP_EFOR  0x011
#define POP_CFOR  0x012
#define POP_DSMP  0x013
#define POP_ESMP  0x014
#define POP_AFOR  0x015
#define POP_CTST  0x016
#define POP_ADLP  0x017
#define POP_DLPE  0x018
#define POP_DSUB  0x019
#define POP_IDLP  0x01A
#define POP_TSUB  0x01B
#define POP_PCAL  0x01D
#define POP_FCAL  0x01E
#define POP_READ  0x01F
#define POP_RDAL  0x020
#define POP_WRIT  0x021
#define POP_FILE  0x022
#define POP_XXST  0x025
#define POP_XXND  0x026
#define POP_XXAR  0x027
#define POP_TDEF  0x02A
#define POP_MDEF  0x02B
#define POP_FDEF  0x02C
#define POP_PDEF  0x02D
#define POP_UDEF  0x02E
#define POP_CDEF  0x02F
#define POP_CLOS  0x030
#define POP_EDCL  0x031
#define POP_RTRN  0x032
#define POP_TDCL  0x033
#define POP_SFST  0x045
#define POP_SFND  0x046
#define POP_SFAR  0x047
#define POP_BFNC  0x04A
#define POP_LFNC  0x04B
#define POP_TNEQ  0x04D
#define POP_TEQU  0x04E
#define POP_TASN  0x04F
#define POP_IDEF  0x051
#define POP_ICLS  0x052
#define POP_NASN  0x057

/* Class 1: Bit */
#define POP_BASN  0x101
#define POP_BAND  0x102
#define POP_BOR   0x103
#define POP_BNOT  0x104
#define POP_BCAT  0x105
#define POP_BTOB  0x121
#define POP_ITOB  0x1C1

/* Class 2: Character */
#define POP_CASN  0x201
#define POP_CCAT  0x202
#define POP_BTOC  0x221
#define POP_CTOC  0x241
#define POP_STOC  0x2A1
#define POP_ITOC  0x2C1

/* Class 3: Matrix */
#define POP_MASN  0x301
#define POP_MADD  0x362
#define POP_MSUB  0x363
#define POP_MNEG  0x344
#define POP_MMPR  0x368
#define POP_MSPR  0x3A5
#define POP_MSDV  0x3A6
#define POP_MTRA  0x329
#define POP_MDET  0x371
#define POP_MIDN  0x373
#define POP_MTOM  0x341
#define POP_MINV  0x3CA
#define POP_VVPR  0x387

/* Class 4: Vector */
#define POP_VASN  0x401
#define POP_VADD  0x482
#define POP_VSUB  0x483
#define POP_VNEG  0x444
#define POP_VMPR  0x46D
#define POP_VSPR  0x4A5
#define POP_VCRS  0x48B
#define POP_VTOV  0x441
#define POP_MVPR  0x46C
#define POP_VDOT  0x58E

/* Class 5: Scalar */
#define POP_SASN  0x501
#define POP_BTOS  0x521
#define POP_CTOS  0x541
#define POP_SIEX  0x571
#define POP_SPEX  0x572
#define POP_STOS  0x5A1
#define POP_SADD  0x5AB
#define POP_SSUB  0x5AC
#define POP_SSPR  0x5AD
#define POP_SSDV  0x5AE
#define POP_SEXP  0x5AF
#define POP_SNEG  0x5B0
#define POP_ITOS  0x5C1

/* Class 6: Integer */
#define POP_IASN  0x601
#define POP_BTOI  0x621
#define POP_CTOI  0x641
#define POP_STOI  0x6A1
#define POP_ITOI  0x6C1
#define POP_IADD  0x6CB
#define POP_ISUB  0x6CC
#define POP_IIPR  0x6CD
#define POP_INEG  0x6D0
#define POP_IPEX  0x6D2

/* Class 7: Conditional */
#define POP_BTRU  0x720
#define POP_BNEQ  0x725
#define POP_BEQU  0x726
#define POP_CNEQ  0x745
#define POP_CEQU  0x746
#define POP_CNGT  0x747
#define POP_CGT   0x748
#define POP_CNLT  0x749
#define POP_CLT   0x74A
#define POP_MNEQ  0x765
#define POP_MEQU  0x766
#define POP_VNEQ  0x785
#define POP_VEQU  0x786
#define POP_SNEQ  0x7A5
#define POP_SEQU  0x7A6
#define POP_SNGT  0x7A7
#define POP_SGT   0x7A8
#define POP_SNLT  0x7A9
#define POP_SLT   0x7AA
#define POP_INEQ  0x7C5
#define POP_IEQU  0x7C6
#define POP_INGT  0x7C7
#define POP_IGT   0x7C8
#define POP_INLT  0x7C9
#define POP_ILT   0x7CA
#define POP_CAND  0x7E2
#define POP_COR   0x7E3
#define POP_CNOT  0x7E4

/* Class 8: Initialization */
#define POP_STRI  0x801
#define POP_SLRI  0x802
#define POP_ELRI  0x803
#define POP_ETRI  0x804
#define POP_BINT  0x821
#define POP_CINT  0x841
#define POP_MINT  0x861
#define POP_VINT  0x881
#define POP_SINT  0x8A1
#define POP_IINT  0x8C1
#define POP_NINT  0x8E1
#define POP_TINT  0x8E2
#define POP_EINT  0x8E3

#define HALMAT_OK              0
#define HALMAT_HALT            1
#define HALMAT_ERR_UNKNOWN    -1
#define HALMAT_ERR_BAD_OP     -2
#define HALMAT_ERR_BAD_QUAL   -3
#define HALMAT_ERR_OVERFLOW   -4
#define HALMAT_ERR_IO         -5
#define HALMAT_ERR_STACK      -6
#define HALMAT_ERR_BOUNDS     -7
#define HALMAT_ERR_DIV_ZERO   -8

#define VAC_SLOT(addr) ((addr) & (HALMAT_MAX_VAC - 1))

typedef struct {
    FILE    *fp;
    int      is_open;       /* 1 = we fopened it (need fclose) */
    char     path[512];     /* empty = not configured */
} halmat_unit_t;

typedef struct {
    uint32_t    code[HALMAT_MAX_CODE];
    uint32_t    code_len;
    uint32_t    num_blocks;

    uint32_t    pc;
    int         halted;             /* 0=running, 1=normal, -1=error */

    syt_entry_t syt[HALMAT_MAX_SYT];
    uint32_t    syt_count;

    lit_entry_t lit[HALMAT_MAX_LIT];
    uint32_t    lit_count;

    /* String pool: actual char bytes recovered from HAL/S source */
    char     lit_str_pool[HALMAT_LIT_STR_POOL];
    uint32_t lit_str_pool_used;
    uint16_t lit_str_off[HALMAT_MAX_LIT];   /* offset into pool, 0 = not loaded */
    uint16_t lit_str_len[HALMAT_MAX_LIT];

    uint8_t     data[HALMAT_DATA_SIZE];
    uint32_t    data_used;

    halmat_val_t vac[HALMAT_MAX_VAC];       /* direct-mapped by code address */
    int          cond_true;                 /* set by Class 7 comparisons */

    call_frame_t frames[HALMAT_MAX_FRAMES];
    uint32_t     frame_depth;

    loop_info_t  loops[HALMAT_MAX_LOOPS];
    uint32_t     loop_depth;

    uint32_t    flow[HALMAT_MAX_FLOW];      /* flow number → code offset */
    io_list_t   io;

    halmat_unit_t units[HALMAT_MAX_UNITS];
    int           translate_ebcdic;

    uint64_t    cycle_count;
    uint64_t    stmt_count;
    uint32_t    current_stmt;               /* from SMRK TAG */

    int         debug_mode;
    int         single_step;
    breakpoint_t breakpoints[64];
    uint32_t     bp_count;
} halmat_t;

double ibm_float_to_double(uint32_t w);
double ibm_double_to_double(uint32_t w_hi, uint32_t w_lo);

int  halmat_load(halmat_t *H, const char *filename);
int  halmat_load_litfile(halmat_t *H, const char *filename);
int  halmat_load_strings(halmat_t *H, const char *source_file);
void halmat_build_flow_table(halmat_t *H);
void halmat_init(halmat_t *H);

const char *halmat_popcode_name(uint32_t popcode);
const char *halmat_class_name(uint32_t cls);
const char *halmat_qual_name(uint32_t qual);
void halmat_disasm(halmat_t *H, FILE *out);
void halmat_disasm_word(halmat_t *H, uint32_t addr, FILE *out);

halmat_val_t halmat_resolve_operand(halmat_t *H, uint32_t operand_word);
void         halmat_store_vac(halmat_t *H, uint32_t addr, halmat_val_t val);
int          halmat_step(halmat_t *H);
int          halmat_run(halmat_t *H);

int halmat_exec_class0(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag);
int halmat_exec_class1(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag);
int halmat_exec_class2(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag);
int halmat_exec_class3(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag);
int halmat_exec_class4(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag);
int halmat_exec_class5(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag);
int halmat_exec_class6(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag);
int halmat_exec_class7(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag);
int halmat_exec_class8(halmat_t *H, uint32_t popcode, uint32_t numop, uint32_t tag);

void halmat_decode_char_lit(halmat_t *H, uint32_t lit_idx, char *buf, int *len);

#endif /* HALMAT_H */
