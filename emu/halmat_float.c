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
