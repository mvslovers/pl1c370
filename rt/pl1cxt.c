/* pl1cxt.c -- TAN and TANH of a COMPLEX FLOAT value (#2 C3, rt/pl1cx.h).
 *
 * C28-6590-0: with t = TAN(x), h = TANH(y) and d = 1 + (t*h)**2,
 * TAN(z) = t*(1-h*h)/d + h*(1+t*t)/d I, and TANH(z) = -(TAN(zI))I; the
 * argument of TAN too large being the real TAN routine's error (ONCODE
 * 1508, 1509; JOB00148). */
#include "pl1cx.h"

static void ztan(double x, double y, double *r, int lo)
{
    double t, h, d;
    cx_sinchk(x, lo, 0x0104);
    tnlr(&x, &t);
    thl(&y, &h);
    d = 1.0 + (t * h) * (t * h);
    r[0] = t * (1.0 - h * h) / d;
    r[1] = h * (1.0 + t * t) / d;
}

static void ztanh(double x, double y, double *r, int lo)
{
    double w[2];
    ztan(-y, x, w, lo);         /* TAN(zI), zI = -y + xI */
    r[0] = w[1];
    r[1] = -w[0];
}

CX_ENTRIES(cxts, cxtl, "PL1CXTS", "PL1CXTL", ztan)
CX_ENTRIES(cxhs, cxhl, "PL1CXHS", "PL1CXHL", ztanh)
