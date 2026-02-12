#include "halmat.h"
#include "halmat_io.h"

int halmat_io_init(halmat_t *H)    { (void)H; return 0; }
void halmat_io_shutdown(halmat_t *H) { (void)H; }

int halmat_io_write(halmat_t *H, int channel, halmat_val_t *args,
                    uint8_t *arg_types, int nargs)
{
    (void)H; (void)channel; (void)args; (void)arg_types; (void)nargs;
    return 0;
}

int halmat_io_read(halmat_t *H, int channel, halmat_val_t *dest)
{
    (void)H; (void)channel; (void)dest;
    return 0;
}

int halmat_io_file(halmat_t *H, int unit, int op, void *buf, int len)
{
    (void)H; (void)unit; (void)op; (void)buf; (void)len;
    return 0;
}
