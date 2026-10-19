/* pl1cxa.c -- ABS of a COMPLEX FLOAT value (#2, rt/pl1cx.h).
 *
 * As C28-6590-0 gives it for PL/I (F): with m the larger and n the
 * smaller of ABS(a) and ABS(b), m*SQRT(1+(n/m)**2), 0 for 0+0I; the
 * square root is the run time's SQRT (rt/pl1msqr.c).  Measured to the
 * bit against SQRT(a*a+b*b), which differs (JOB00075). */
#include "pl1cx.h"

void sqs(const float *x, float *r) asm("PL1SQS0");
void sql(const double *x, double *r) asm("PL1SQL0");

void cxas(const float *a, const float *b, float *r, int m)
{
    float x = *a < 0.0f ? -*a : *a, y = *b < 0.0f ? -*b : *b, q, qq, t, s;
    if (x < y) {
        t = x;
        x = y;
        y = t;
    }
    if (x == 0.0f) {
        *r = 0.0f;
        return;
    }
    q = y / x;
    qq = q * q;
    t = 1.0f + qq;
    sqs(&t, &s);
    *r = x * s;
    if ((q == 0.0f && y != 0.0f) + (qq == 0.0f && q != 0.0f))
        cx_ufl((q == 0.0f && y != 0.0f) + (qq == 0.0f && q != 0.0f), m);
}

void cxal(const double *a, const double *b, double *r, int m)
{
    double x = *a < 0.0 ? -*a : *a, y = *b < 0.0 ? -*b : *b, q, qq, t, s;
    if (x < y) {
        t = x;
        x = y;
        y = t;
    }
    if (x == 0.0) {
        *r = 0.0;
        return;
    }
    q = y / x;
    qq = q * q;
    t = 1.0 + qq;
    sql(&t, &s);
    *r = x * s;
    if ((q == 0.0 && y != 0.0) + (qq == 0.0 && q != 0.0))
        cx_ufl((q == 0.0 && y != 0.0) + (qq == 0.0 && q != 0.0), m);
}
