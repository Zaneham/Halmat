/* IBM System/360 hex float: sign(1) + exp(7, base-16, bias 64) + frac(24|56) */

#include "halmat.h"
#include <math.h>

double ibm_float_to_double(uint32_t w)
{
    double sign = (w & 0x80000000u) ? -1.0 : 1.0;
    int    exp  = (int)((w >> 24) & 0x7F);
    uint32_t frac = w & 0x00FFFFFFu;

    if (frac == 0)
        return 0.0;

    double mantissa = (double)frac / 16777216.0;  /* 2^24 */
    return sign * mantissa * pow(16.0, (double)(exp - 64));
}

double ibm_double_to_double(uint32_t w_hi, uint32_t w_lo)
{
    double sign = (w_hi & 0x80000000u) ? -1.0 : 1.0;
    int    exp  = (int)((w_hi >> 24) & 0x7F);
    uint32_t frac_hi = w_hi & 0x00FFFFFFu;
    uint32_t frac_lo = w_lo;

    if (frac_hi == 0 && frac_lo == 0)
        return 0.0;

    double mantissa = ((double)frac_hi * 4294967296.0 + (double)frac_lo)
                      / 72057594037927936.0;  /* 2^56 */
    return sign * mantissa * pow(16.0, (double)(exp - 64));
}

/* ---- CHARACTER <-> arithmetic (Language Spec App. D) ---- */

static int skipb(const char *s, int i, int len)
{
    while (i < len && s[i] == ' ') i++;
    return i;
}

/* App. D: a CHARACTER converts to INTEGER only if it reads as a signed
   whole number. No point, no exponent, no imbedded blanks. */
int32_t halmat_cint(const char *s, int len, int *ok)
{
    int i = skipb(s, 0, len);
    int neg = 0, digits = 0;
    int64_t acc = 0;

    *ok = 0;
    if (i < len && (s[i] == '+' || s[i] == '-')) neg = (s[i++] == '-');
    while (i < len && s[i] >= '0' && s[i] <= '9') {
        acc = acc * 10 + (s[i++] - '0');
        if (acc > (neg ? 2147483648LL : 2147483647LL)) return 0;  /* App. D: too large for the destination */
        digits++;
    }
    if (digits == 0 || skipb(s, i, len) != len) return 0;

    *ok = 1;
    return (int32_t)(neg ? -acc : acc);
}

/* App. D + 2.3.3: a CHARACTER converts to SCALAR if it reads as an
   arithmetic literal — [sign] ddd[.ddd] followed by any run of B/E/H
   exponents, which chain multiplicatively ('0.123E16B-3'). */
double halmat_cnum(const char *s, int len, int *ok)
{
    int i = skipb(s, 0, len);
    int neg = 0, digits = 0;
    double val = 0.0, frac = 0.1;

    *ok = 0;
    if (i < len && (s[i] == '+' || s[i] == '-')) neg = (s[i++] == '-');
    while (i < len && s[i] >= '0' && s[i] <= '9') {
        val = val * 10.0 + (s[i++] - '0');
        digits++;
    }
    if (i < len && s[i] == '.') {
        i++;
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            val += (s[i++] - '0') * frac;
            frac *= 0.1;
            digits++;
        }
    }
    if (digits == 0) return 0.0;

    while (i < len && (s[i] == 'B' || s[i] == 'E' || s[i] == 'H')) {
        double base = (s[i] == 'B') ? 2.0 : (s[i] == 'E') ? 10.0 : 16.0;
        int pneg = 0, pdig = 0, power = 0;
        i++;
        if (i < len && (s[i] == '+' || s[i] == '-')) pneg = (s[i++] == '-');
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            power = power * 10 + (s[i++] - '0');
            if (power > 9999) return 0.0;
            pdig++;
        }
        if (pdig == 0) return 0.0;
        val *= pow(base, (double)(pneg ? -power : power));
    }
    if (skipb(s, i, len) != len) return 0.0;

    *ok = 1;
    return neg ? -val : val;
}

/* App. D: SCALAR renders as a fixed-length ' d.dddddddE+dd' field (leading
   blank when positive), except exact zero, which is ' 0.0' blank-padded.
   Widths are from HAL/S-FC User's Manual p. 6-2. */
int halmat_sfmt(char *buf, int n, double v, uint8_t prec)
{
    int wide  = (prec == HPREC_DOUBLE);
    int width = wide ? 23 : 14;

    if (v == 0.0)
        return snprintf(buf, (size_t)n, "%-*s", width, " 0.0");
    if (wide)
        return snprintf(buf, (size_t)n, "% .16E", v);
    return snprintf(buf, (size_t)n, "% .7E", v);
}
