/* pl1cxd.c -- COMPLEX FLOAT division (#2, rt/pl1cx.h).
 *
 * Smith's method, as C28-6590-0 gives it for PL/I (F): if ABS(c) >=
 * ABS(d), q = d/c and the parts are (a+b*q)/(c+d*q) and
 * (b-a*q)/(c+d*q); otherwise the operands are taken as (b-aI)/(d-cI),
 * which is the first case.  The comparison is made in short precision
 * in both routines; for normalized values the first word, its sign
 * off, orders as the magnitude does.  A divisor 0+0I divides three
 * times by zero: ZERODIVIDE three times, as in PL/I (F) (JOB00075). */
#include "pl1cx.h"

#define UF(p, x, y) ((p) == 0 && (x) != 0 && (y) != 0)

typedef union { float f; unsigned w; } fw;
typedef union { double d; unsigned w[2]; } dw;

void cxds(const float *a, const float *b, const float *c, const float *d, float *r, int m)
{
    float x = *a, y = *b, u = *c, v = *d, q, dq, den, yq, xq, nr, ni;
    fw cu = {u}, cv = {v};
    int n;
    if ((cu.w & 0x7FFFFFFFu) < (cv.w & 0x7FFFFFFFu)) {
        float t = x;
        x = y;
        y = -t;
        t = u;
        u = v;
        v = -t;
    }
    q = v / u;
    dq = v * q;
    den = u + dq;
    yq = y * q;
    xq = x * q;
    nr = x + yq;
    ni = y - xq;
    r[0] = nr / den;
    r[1] = ni / den;
    n = UF(q, v, u) + UF(dq, v, q) + UF(yq, y, q) + UF(xq, x, q) + UF(r[0], nr, den) +
        UF(r[1], ni, den);
    if (n)
        cx_ufl(n, m);
}

void cxdl(const double *a, const double *b, const double *c, const double *d, double *r, int m)
{
    double x = *a, y = *b, u = *c, v = *d, q, dq, den, yq, xq, nr, ni;
    dw cu = {u}, cv = {v};
    int n;
    if ((cu.w[0] & 0x7FFFFFFFu) < (cv.w[0] & 0x7FFFFFFFu)) {
        double t = x;
        x = y;
        y = -t;
        t = u;
        u = v;
        v = -t;
    }
    q = v / u;
    dq = v * q;
    den = u + dq;
    yq = y * q;
    xq = x * q;
    nr = x + yq;
    ni = y - xq;
    r[0] = nr / den;
    r[1] = ni / den;
    n = UF(q, v, u) + UF(dq, v, q) + UF(yq, y, q) + UF(xq, x, q) + UF(r[0], nr, den) +
        UF(r[1], ni, den);
    if (n)
        cx_ufl(n, m);
}
