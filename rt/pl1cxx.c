/* pl1cxx.c -- z1**z2 of COMPLEX FLOAT values, the general case (#2 C3,
 * rt/pl1cx.h).
 *
 * C28-6590-0: for z1 = 0 the result is 0 if REAL(z2) > 0 and
 * IMAG(z2) = 0, otherwise an error (IHE210I, ONCODE 1556 short, 1557
 * long; JOB00148, JOB00150); for z1 ¬= 0 it is EXP(z2*LOG(z1)). */
#include "pl1cx.h"

static void cxx(double a, double b, double c, double d, double *r, int lo)
{
    double l[2];
    if (a == 0.0 && b == 0.0) {
        if (c > 0.0 && d == 0.0) {
            r[0] = r[1] = 0.0;
            return;
        }
        pl1_math_error(lo ? 0x0119 : 0x0118);
    }
    cx_log(a, b, l, lo);
    cx_exp(c * l[0] - d * l[1], d * l[0] + c * l[1], r, lo);
}

void cxxs(const float *a, const float *b, const float *c, const float *d, float *r, int m)
    asm("PL1CXXS");
void cxxl(const double *a, const double *b, const double *c, const double *d, double *r,
          int m) asm("PL1CXXL");

void cxxs(const float *a, const float *b, const float *c, const float *d, float *r, int m)
{
    double o[2];
    (void)m;
    cxx(*a, *b, *c, *d, o, 0);
    r[0] = (float)o[0];
    r[1] = (float)o[1];
}

void cxxl(const double *a, const double *b, const double *c, const double *d, double *r, int m)
{
    (void)m;
    cxx(*a, *b, *c, *d, r, 1);
}
