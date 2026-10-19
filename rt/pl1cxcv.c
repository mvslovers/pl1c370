/* pl1cxcv.c -- a string to a complex value (#2 C4b, check.c B_CXCVT): split
 * by rt/pl1cxsp.c, CONVERSION for what PL/I (F) refuses, the parts then
 * converted as a string to a real value is. */
#include <stddef.h>
#include "pl1rt.h"
#include "pl1int.h"

int pl1i_cpx_split(const char *t, int n, int *rs, int *rn, int *is, int *in) asm("PL1CXSP");
void pl1_cpx_conv(char *s, int len, unsigned char *r, int desc) asm("PL1CXCV");

/* A string to a complex temporary (check.c B_CXCVT): desc 256 + 1 for
 * long FLOAT parts, 256 for short ones, else the scale q of FIXED
 * DECIMAL(15,q) parts. */
void pl1_cpx_conv(char *s, int len, unsigned char *r, int desc)
{
    int rs, rn, is, in, k, fl = desc & 256, lo = desc & 1;
    char zero[1] = {'0'};
    while ((k = pl1i_cpx_split(s, len, &rs, &rn, &is, &in)) >= 0)
        if (!pl1i_conversion(fl ? 618 : 612, s, len, k)) {
            rs = is = 0;
            rn = in = 0;
            break;
        }
    if (fl) {
        pl1_char_to_float(rn ? s + rs : zero, rn ? rn : 1, lo, r);
        pl1_char_to_float(in ? s + is : zero, in ? in : 1, lo, r + (lo ? 8 : 4));
    } else {
        int q = (signed char)desc;
        pl1_char_to_dec(rn ? s + rs : zero, rn ? rn : 1, q, r);
        pl1_char_to_dec(in ? s + is : zero, in ? in : 1, q, r + 8);
    }
}
