#ifndef HALMAT_TYPES_H
#define HALMAT_TYPES_H

#include <stdint.h>
enum {
    HTYPE_NONE    = 0,
    HTYPE_BIT     = 1,
    HTYPE_CHAR    = 2,
    HTYPE_MATRIX  = 3,
    HTYPE_VECTOR  = 4,
    HTYPE_SCALAR  = 5,
    HTYPE_INTEGER = 6,
    HTYPE_BOOLEAN = 7,
    HTYPE_EVENT   = 9,
    HTYPE_STRUCT  = 10
};

typedef struct {
    uint8_t  type;
    uint8_t  rows;
    uint8_t  cols;
    uint8_t  _pad;
    union {
        int32_t  integer;
        double   scalar;
        double   vector[64];
        double   matrix[64];
        struct {
            char     data[256];
            uint16_t len;
        } string;
        uint32_t bits;
    } v;
} halmat_val_t;

typedef struct {
    halmat_val_t val;
    uint8_t      allocated;
    uint8_t      _pad[3];
} syt_entry_t;

typedef struct {
    uint8_t  type;       /* 0=CHAR, 1=ARITH, 2=BIT, 5=DOUBLE */
    uint8_t  _pad[3];
    int32_t  lit1;
    int32_t  lit2;
    int32_t  lit3;
} lit_entry_t;

typedef struct {
    uint32_t return_pc;
    uint32_t call_addr;    /* FCAL/PCAL address, for storing return VAC */
    uint32_t syt_base;
    uint32_t vac_snapshot;
} call_frame_t;

typedef struct {
    uint32_t addr;
    uint32_t stmt;
    int      enabled;
} breakpoint_t;

typedef struct {
    uint32_t flow_num;
    uint32_t cmp_addr;       /* DFOR/DTST address (loop-back target) */
    uint32_t tag;            /* 0=WHILE, 1=UNTIL */
    uint32_t discrete_idx;
    uint32_t is_discrete;
} loop_info_t;

#define HALMAT_MAX_IO_ARGS 64
typedef struct {
    halmat_val_t args[HALMAT_MAX_IO_ARGS];
    uint8_t      arg_types[HALMAT_MAX_IO_ARGS];
    int          nargs;
    int          active;
    int          is_call;
} io_list_t;

#endif /* HALMAT_TYPES_H */
