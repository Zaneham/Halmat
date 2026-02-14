#include "halmat.h"
#include "halmat_io.h"

/* EBCDIC Code Page 037 -> ASCII.
 * IBM S/360 encoding used by the HAL/S-FC compiler. */
static const uint8_t ebcdic_to_ascii[256] = {
    0x00,0x01,0x02,0x03,0x9C,0x09,0x86,0x7F,
    0x97,0x8D,0x8E,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x9D,0x85,0x08,0x87,
    0x18,0x19,0x92,0x8F,0x1C,0x1D,0x1E,0x1F,
    0x80,0x81,0x82,0x83,0x84,0x0A,0x17,0x1B,
    0x88,0x89,0x8A,0x8B,0x8C,0x05,0x06,0x07,
    0x90,0x91,0x16,0x93,0x94,0x95,0x96,0x04,
    0x98,0x99,0x9A,0x9B,0x14,0x15,0x9E,0x1A,
    0x20,0xA0,0xE2,0xE4,0xE0,0xE1,0xE3,0xE5, /* 40: SP */
    0xE7,0xF1,0xA2,0x2E,0x3C,0x28,0x2B,0x7C, /* 4B: . < ( + | */
    0x26,0xE9,0xEA,0xEB,0xE8,0xED,0xEE,0xEF, /* 50: & */
    0xEC,0xDF,0x21,0x24,0x2A,0x29,0x3B,0xAC, /* 5A: ! $ * ) ; */
    0x2D,0x2F,0xC2,0xC4,0xC0,0xC1,0xC3,0xC5, /* 60: - / */
    0xC7,0xD1,0xA6,0x2C,0x25,0x5F,0x3E,0x3F, /* 6B: , % _ > ? */
    0xF8,0xC9,0xCA,0xCB,0xC8,0xCD,0xCE,0xCF, /* 70: 0xF8 = ø */
    0xCC,0x60,0x3A,0x23,0x40,0x27,0x3D,0x22, /* 79: ` : # @ ' = " */
    0xD8,0x61,0x62,0x63,0x64,0x65,0x66,0x67, /* 80: 0xD8 = Ø  81-89: a-i */
    0x68,0x69,0xAB,0xBB,0xF0,0xFD,0xFE,0xB1,
    0xB0,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,0x70, /* 91-99: j-r */
    0x71,0x72,0xAA,0xBA,0xE6,0xB8,0xC6,0xA4,
    0xB5,0x7E,0x73,0x74,0x75,0x76,0x77,0x78, /* A1: ~  A2-A9: s-z */
    0x79,0x7A,0xA1,0xBF,0xD0,0x5B,0xDE,0xAE, /* AD: [ */
    0x5E,0xA3,0xA5,0xB7,0xA9,0xA7,0xB6,0xBC, /* B0: ^ */
    0xBD,0xBE,0xDD,0xA8,0xAF,0x5D,0xB4,0xD7, /* BD: ] */
    0x7B,0x41,0x42,0x43,0x44,0x45,0x46,0x47, /* C0: {  C1-C9: A-I */
    0x48,0x49,0xAD,0xF4,0xF6,0xF2,0xF3,0xF5,
    0x7D,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50, /* D0: }  D1-D9: J-R */
    0x51,0x52,0xB9,0xFB,0xFC,0xF9,0xFA,0xFF,
    0x5C,0xF7,0x53,0x54,0x55,0x56,0x57,0x58, /* E0: \  E2-E9: S-Z */
    0x59,0x5A,0xB2,0xD4,0xD6,0xD2,0xD3,0xD5,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37, /* F0-F9: 0-9 */
    0x38,0x39,0xB3,0xDB,0xDC,0xD9,0xDA,0x9F
};

static void translate_buf(char *buf, int len, const uint8_t *table)
{
    for (int i = 0; i < len; i++)
        buf[i] = (char)table[(uint8_t)buf[i]];
}

static FILE *unit_fp(halmat_t *H, int unit, const char *mode)
{
    if (unit < 0 || unit >= HALMAT_MAX_UNITS)
        return NULL;
    halmat_unit_t *u = &H->units[unit];
    if (u->fp) {
        /* Reopen if mode changed (e.g. "r" -> "w") */
        if (u->is_open && u->mode[0] && u->mode[0] != mode[0]) {
            fclose(u->fp);
            u->fp = fopen(u->path, mode);
            if (u->fp)
                snprintf(u->mode, sizeof(u->mode), "%s", mode);
            else
                u->is_open = 0;
        }
        return u->fp;
    }
    /* Lazy open from stored path */
    if (u->path[0]) {
        u->fp = fopen(u->path, mode);
        if (u->fp) {
            u->is_open = 1;
            snprintf(u->mode, sizeof(u->mode), "%s", mode);
        }
        return u->fp;
    }
    /* Defaults: 5=stdin, 6=stdout */
    if (unit == 5) return stdin;
    if (unit == 6) return stdout;
    return NULL;
}

int halmat_io_init(halmat_t *H)
{
    (void)H;
    return 0;
}

void halmat_io_shutdown(halmat_t *H)
{
    for (int i = 0; i < HALMAT_MAX_UNITS; i++) {
        if (H->units[i].is_open && H->units[i].fp) {
            fclose(H->units[i].fp);
            H->units[i].fp = NULL;
            H->units[i].is_open = 0;
        }
    }
}

static void write_char(halmat_t *H, FILE *fp, const char *data, int len)
{
    if (H->translate_ebcdic) {
        char buf[256];
        while (len > 0) {
            int chunk = (len > 256) ? 256 : len;
            memcpy(buf, data, chunk);
            translate_buf(buf, chunk, ebcdic_to_ascii);
            fprintf(fp, "%.*s", chunk, buf);
            data += chunk;
            len -= chunk;
        }
    } else {
        fprintf(fp, "%.*s", len, data);
    }
}

int halmat_io_write(halmat_t *H, int channel, halmat_val_t *args,
                    uint8_t *arg_types, int nargs)
{
    FILE *fp = unit_fp(H, channel, "w");
    if (!fp) fp = stdout;

    for (int i = 0; i < nargs; i++) {
        switch (arg_types[i]) {
        case 2:
            if (args[i].type == HTYPE_CHAR)
                write_char(H, fp, args[i].v.string.data, (int)args[i].v.string.len);
            break;
        case 5: {
            double v = (args[i].type == HTYPE_INTEGER)
                       ? (double)args[i].v.integer : args[i].v.scalar;
            if (v == 0.0)
                fprintf(fp, " 0.0");
            else
                fprintf(fp, "% .7E", v);
            break;
        }
        case 6: {
            int32_t v = (args[i].type == HTYPE_INTEGER)
                        ? args[i].v.integer : (int32_t)args[i].v.scalar;
            fprintf(fp, "%11d", v);
            break;
        }
        default:
            if (args[i].type == HTYPE_INTEGER)
                fprintf(fp, "%11d", args[i].v.integer);
            else if (args[i].type == HTYPE_SCALAR) {
                if (args[i].v.scalar == 0.0)
                    fprintf(fp, " 0.0");
                else
                    fprintf(fp, "% .7E", args[i].v.scalar);
            } else if (args[i].type == HTYPE_CHAR)
                write_char(H, fp, args[i].v.string.data, (int)args[i].v.string.len);
            break;
        }
    }
    fprintf(fp, "\n");
    return 0;
}

int halmat_io_read(halmat_t *H, int channel, halmat_val_t *dest)
{
    FILE *fp = unit_fp(H, channel, "r");
    if (!fp) fp = stdin;

    dest->type = HTYPE_INTEGER;
    if (fscanf(fp, "%d", &dest->v.integer) != 1)
        dest->v.integer = 0;
    return 0;
}

int halmat_io_file(halmat_t *H, int unit, int op, void *buf, int len)
{
    (void)H; (void)unit; (void)op; (void)buf; (void)len;
    return 0;
}
