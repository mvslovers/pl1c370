/* pl1cxe.c -- EXP of a COMPLEX FLOAT value (#2 C3, rt/pl1cx.h).
 *
 * C28-6590-0: EXP(x)*COS(y) + EXP(x)*SIN(y)I; ABS(y) too large for SIN
 * is the real SIN routine's error (ONCODE 1506, 1507; JOB00148). */
#include "pl1cx.h"

void cx_exp(double x, double y, double *r, int lo)
{
    double e;
    cx_sinchk(y, lo, 0x0103);
    exl(&x, &e);
    r[0] = e * cx_cos(y);
    r[1] = e * cx_sin(y);
}

CX_ENTRIES(cxes, cxel, "PL1CXES", "PL1CXEL", cx_exp)
