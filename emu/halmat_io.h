/* Replaceable at link time. See halmat_io_null.c for stub template. */

#ifndef HALMAT_IO_H
#define HALMAT_IO_H

#include "halmat.h"

int  halmat_io_init(halmat_t *H);
void halmat_io_shutdown(halmat_t *H);
int  halmat_io_write(halmat_t *H, int channel, halmat_val_t *args,
                     uint8_t *arg_types, int nargs);
int  halmat_io_read(halmat_t *H, int channel, halmat_val_t *dest);
int  halmat_io_file(halmat_t *H, int unit, int op, void *buf, int len);

#endif /* HALMAT_IO_H */
