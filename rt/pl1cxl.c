/* pl1cxl.c -- LOG of a COMPLEX FLOAT value (#2 C3, rt/pl1cx.h).
 *
 * C28-6590-0: LOG(x*x+y*y)/2 + ATAN(y,x)I, the exponents of x and y
 * changed first where the squares would overflow or underflow.  ATAN is
 * taken first: LOG(0) is the real ATAN(Y,X) routine's error (ONCODE
 * 1510, 1511), as in PL/I (F) (JOB00148, JOB00150). */
#include "pl1cx.h"

void cx_log(double x, double y, double *r, int lo)
{
    double m = x < 0 ? -x : x, n = y < 0 ? -y : y, s = 0.0, t;
    if (x == 0.0 && y == 0.0)
        pl1_math_error(lo ? 0x010D : 0x0105);
    atl2(&y, &x, &r[1]);
    if (n > m)
        m = n;
    /* m scaled into [1/16, 16) by powers of 16, exact */
    while (m >= 16.0) {
        m /= 16.0;
        x /= 16.0;
        y /= 16.0;
        s += 1.0;
    }
    while (m < 0.0625) {
        m *= 16.0;
        x *= 16.0;
        y *= 16.0;
        s -= 1.0;
    }
    t = x * x + y * y;
    lnle(&t, &t);
    /* + s * LOG(16) */
    r[0] = t * 0.5 + s * 2.7725887222397812;
}

CX_ENTRIES(cxls, cxll, "PL1CXLS", "PL1CXLL", cx_log)
