#include "halmat.h"
#include "halmat_io.h"

int halmat_io_init(halmat_t *H)
{
    (void)H;
    return 0;
}

void halmat_io_shutdown(halmat_t *H)
{
    (void)H;
}

int halmat_io_write(halmat_t *H, int channel, halmat_val_t *args,
                    uint8_t *arg_types, int nargs)
{
    (void)H;
    (void)channel;

    for (int i = 0; i < nargs; i++) {
        switch (arg_types[i]) {
        case 2:
            if (args[i].type == HTYPE_CHAR)
                printf("%.*s", (int)args[i].v.string.len, args[i].v.string.data);
            break;
        case 5:
            if (args[i].type == HTYPE_INTEGER)
                printf(" %d", args[i].v.integer);
            else
                printf(" %g", args[i].v.scalar);
            break;
        case 6:
            if (args[i].type == HTYPE_INTEGER)
                printf(" %d", args[i].v.integer);
            else
                printf(" %d", (int)args[i].v.scalar);
            break;
        default:
            /* fallback */
            if (args[i].type == HTYPE_INTEGER)
                printf(" %d", args[i].v.integer);
            else if (args[i].type == HTYPE_SCALAR)
                printf(" %g", args[i].v.scalar);
            else if (args[i].type == HTYPE_CHAR)
                printf("%.*s", (int)args[i].v.string.len, args[i].v.string.data);
            break;
        }
    }
    printf("\n");
    return 0;
}

int halmat_io_read(halmat_t *H, int channel, halmat_val_t *dest)
{
    (void)H;
    (void)channel;

    dest->type = HTYPE_INTEGER;
    if (scanf("%d", &dest->v.integer) != 1)
        dest->v.integer = 0;
    return 0;
}

int halmat_io_file(halmat_t *H, int unit, int op, void *buf, int len)
{
    (void)H;
    (void)unit;
    (void)op;
    (void)buf;
    (void)len;
    return 0;
}
