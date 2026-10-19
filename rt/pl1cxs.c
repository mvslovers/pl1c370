/* pl1cxs.c -- SIN, COS, SINH and COSH of a COMPLEX FLOAT value (#2 C3,
 * rt/pl1cx.h).  C28-6590-0:
 *   SIN(z)  = SIN(x)*COSH(y) + COS(x)*SINH(y)I
 *   COS(z)  = COS(x)*COSH(y) - SIN(x)*SINH(y)I
 *   SINH(z) = COS(y)*SINH(x) + SIN(y)*COSH(x)I
 *   COSH(z) = COS(y)*COSH(x) + SIN(y)*SINH(x)I
 * the circular functions' argument too large being the real SIN
 * routine's error (ONCODE 1506, 1507; JOB00148). */
#include "pl1cx.h"

/* k: 0 SIN, 1 COS, 2 SINH, 3 COSH */
static void trig(double x, double y, double *r, int lo, int k)
{
    double a = k < 2 ? x : y, h = k < 2 ? y : x, s, c, sh, ch;
    cx_sinchk(a, lo, 0x0103);
    s = cx_sin(a);
    c = cx_cos(a);
    shls(&h, &sh);
    shlc(&h, &ch);
    switch (k) {
    case 0: r[0] = s * ch; r[1] = c * sh; break;
    case 1: r[0] = c * ch; r[1] = -(s * sh); break;
    case 2: r[0] = c * sh; r[1] = s * ch; break;
    default: r[0] = c * ch; r[1] = s * sh; break;
    }
}

static void zsin(double x, double y, double *r, int lo) { trig(x, y, r, lo, 0); }
static void zcos(double x, double y, double *r, int lo) { trig(x, y, r, lo, 1); }
static void zsinh(double x, double y, double *r, int lo) { trig(x, y, r, lo, 2); }
static void zcosh(double x, double y, double *r, int lo) { trig(x, y, r, lo, 3); }

CX_ENTRIES(cxss, cxsl, "PL1CXSS", "PL1CXSL", zsin)
CX_ENTRIES(cxcs, cxcl, "PL1CXCS", "PL1CXCL", zcos)
CX_ENTRIES(cxys, cxyl, "PL1CXYS", "PL1CXYL", zsinh)
CX_ENTRIES(cxks, cxkl, "PL1CXKS", "PL1CXKL", zcosh)
