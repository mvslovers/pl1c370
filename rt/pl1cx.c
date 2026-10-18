/* pl1cx.c -- COMPLEX values as characters (#227).
 *
 * A complex value becomes its real part as an arithmetic value becomes
 * a string (the PUT LIST field, leading blanks kept), then its
 * imaginary part without leading blanks and with its sign always
 * there, then I (GC28-8201-4, "Complex to Character-String"; measured:
 * 1.7299E+01+1.5000E+00I, 0.133+0.007I, -12.5-3.2I, 7-8I, t227
 * Q2CLIST), blanks after it up to the length of the two fields and
 * the I (PUT DATA shows them before its semicolon, `E=        7-8I       ;`).
 * PUT LIST, PUT DATA and assignment to a character string
 * all take this.  A member of its own, so that a program without
 * complex values links nothing of it. */
#include <string.h>
#include "pl1rt.h"

int pl1_cpx_join(char *dst, const char *re, int rl, const char *im, int il)
{
    int n = rl, full = rl + il + 1;
    memmove(dst, re, (size_t)rl);
    while (il > 0 && *im == ' ') {
        im++;
        il--;
    }
    if (il == 0 || (*im != '-' && *im != '+'))
        dst[n++] = '+';
    memmove(dst + n, im, (size_t)il);
    n += il;
    dst[n++] = 'I';
    while (n < full)
        dst[n++] = ' ';          /* PL/I (F)'s length: the two fields and I */
    return n;
}
