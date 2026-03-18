/* hm_abend.h — mainframe-style ABEND dumps for yaHALMAT
 *
 * When things go sideways in the emulator, we produce a dump that
 * would make an MVS operator weep with nostalgia. System codes
 * for hardware signals, user codes for HALMAT runtime errors.
 */
#ifndef HM_ABEND_H
#define HM_ABEND_H

#include <stdint.h>
#include <stdio.h>
#include "halmat.h"

/* ---- System Codes (signal-mapped) ---- */
#define HM_S0C1   0x0C1   /* OPERATION EXCEPTION  (SIGILL)  */
#define HM_S0C4   0x0C4   /* PROTECTION EXCEPTION (SIGSEGV) */
#define HM_S0C7   0x0C7   /* DATA EXCEPTION       (SIGFPE)  */
#define HM_S0CB   0x0CB   /* MACHINE CHECK        (SIGABRT) */

/* ---- User Codes (HALMAT_ERR_* mapped) ---- */
#define HM_U0101  0x1101  /* UNKNOWN ERROR    */
#define HM_U0102  0x1102  /* BAD OPCODE       */
#define HM_U0103  0x1103  /* BAD QUALIFIER    */
#define HM_U0104  0x1104  /* POOL OVERFLOW    */
#define HM_U0105  0x1105  /* I/O ERROR        */
#define HM_U0106  0x1106  /* STACK OVERFLOW   */
#define HM_U0107  0x1107  /* ARRAY BOUNDS     */
#define HM_U0108  0x1108  /* DIVISION BY ZERO */

/* ---- ABEND context ---- */
typedef struct {
    uint16_t     code;     /* abend code (HM_Sxxx or HM_Uxxx) */
    uint8_t      dump;     /* 1 = --dump flag active            */
    uint8_t      _pad;
    const char  *src;      /* halmat.bin path                   */
    halmat_t    *hm;       /* pointer to live halmat_t          */
} hm_abctx_t;

/* Initialise context (installs signal handlers) */
void        hm_ainit(hm_abctx_t *A);

/* Full formatted dump to file (fprintf-based, not signal-safe) */
void        hm_adump(const hm_abctx_t *A, FILE *out);

/* Set code and dump — for explicit abend from engine errors */
void        hm_abend(hm_abctx_t *A, uint16_t code);

/* Map HALMAT_ERR_* → HM_U01xx */
uint16_t    hm_etou(int err);

/* Human-readable tag string: "S0C4", "U0108", etc. */
const char *hm_atag(uint16_t code);

/* Human-readable description: "PROTECTION EXCEPTION", etc. */
const char *hm_aname(uint16_t code);

#endif /* HM_ABEND_H */
