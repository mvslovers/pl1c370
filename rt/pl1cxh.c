/* pl1cxh.c -- ATAN and ATANH of a COMPLEX FLOAT value (#2 C3, rt/pl1cx.h).
 *
 * C28-6590-0: ATANH(z) = ATANH(2*x/(1+x*x+y*y))/2 +
 * ATAN(2*y, 1-x*x-y*y)/2 I, and ATAN(z) = -(ATANH(zI))I.  z = +1 or -1
 * for ATANH, +I or -I for ATAN, is an error: IHE211I, ONCODE 1558 short,
 * 1559 long (JOB00148, JOB00150). */
#include "pl1cx.h"

static void zatanh(double x, double y, double *r, int lo)
{
    double q = x * x + y * y, a, b, c;
    if (y == 0.0 && (x == 1.0 || x == -1.0))
        pl1_math_error(lo ? 0x0117 : 0x0116);
    a = 2.0 * x / (1.0 + q);
    htl(&a, &r[0]);
    r[0] *= 0.5;
    b = 2.0 * y;
    c = 1.0 - q;
    atl2(&b, &c, &r[1]);
    r[1] *= 0.5;
}

static void zatan(double x, double y, double *r, int lo)
{
    double w[2];
    zatanh(-y, x, w, lo);       /* ATANH(zI), zI = -y + xI */
    r[0] = w[1];
    r[1] = -w[0];
}

CX_ENTRIES(cxzs, cxzl, "PL1CXZS", "PL1CXZL", zatanh)
CX_ENTRIES(cxns, cxnl, "PL1CXNS", "PL1CXNL", zatan)
