#ifndef HALMAT_DEBUG_H
#define HALMAT_DEBUG_H

#include "halmat.h"

int  halmat_debug_init(halmat_t *H);
void halmat_debug_prompt(halmat_t *H);
int  halmat_debug_check_breakpoint(halmat_t *H);
void halmat_debug_print_state(halmat_t *H, FILE *out);

#endif /* HALMAT_DEBUG_H */
