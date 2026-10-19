/* pl1cxp.c -- COMPLEX FLOAT z**n, n an integer (#2, rt/pl1cx.h).
 *
 * As C28-6590-0 gives it for PL/I (F): z**0 is 1 for z ¬= 0; otherwise
 * the result starts as z and is squared, and multiplied by z, for each
 * bit of ABS(n) after the first, through the multiplication routine;
 * for n < 0 it is then divided into 1+0I by the division routine.  z = 0
 * with n <= 0 is an error: ONCODE 1554 short, 1555 long, PL/I (F)'s
 * message (JOB00079). */
#include "pl1cx.h"

void cxps(const float *a, const float *b, int n, float *r, int m)
{
    float z0 = *a, z1 = *b, t[2] = {*a, *b};
    unsigned k = n < 0 ? 0u - (unsigned)n : (unsigned)n, bit = 0x80000000u;
    if (n <= 0 && z0 == 0.0f && z1 == 0.0f) {
        pl1_math_error(0x0114);
        return;
    }
    if (n == 0) {
        r[0] = 1.0f;
        r[1] = 0.0f;
        return;
    }
    while (!(k & bit))
        bit >>= 1;
    for (bit >>= 1; bit; bit >>= 1) {
        cxms(&t[0], &t[1], &t[0], &t[1], t, m);
        if (k & bit)
            cxms(&t[0], &t[1], &z0, &z1, t, m);
    }
    if (n < 0) {
        static const float one = 1.0f, zero = 0.0f;
        cxds(&one, &zero, &t[0], &t[1], t, m);
    }
    r[0] = t[0];
    r[1] = t[1];
}

void cxpl(const double *a, const double *b, int n, double *r, int m)
{
    double z0 = *a, z1 = *b, t[2] = {*a, *b};
    unsigned k = n < 0 ? 0u - (unsigned)n : (unsigned)n, bit = 0x80000000u;
    if (n <= 0 && z0 == 0.0 && z1 == 0.0) {
        pl1_math_error(0x0115);
        return;
    }
    if (n == 0) {
        r[0] = 1.0;
        r[1] = 0.0;
        return;
    }
    while (!(k & bit))
        bit >>= 1;
    for (bit >>= 1; bit; bit >>= 1) {
        cxml(&t[0], &t[1], &t[0], &t[1], t, m);
        if (k & bit)
            cxml(&t[0], &t[1], &z0, &z1, t, m);
    }
    if (n < 0) {
        static const double one = 1.0, zero = 0.0;
        cxdl(&one, &zero, &t[0], &t[1], t, m);
    }
    r[0] = t[0];
    r[1] = t[1];
}
