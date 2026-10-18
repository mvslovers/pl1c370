/* pl1msqr.c -- SQRT (rt/pl1mth.h; the method and the accuracy:
 * internals/research/pli/tier14-measurement.md). */
#include "pl1mth.h"

/* SQRT(x), x > 0: Newton's iteration in long precision from a linear
 * guess on the fraction, then one correction (x - y*y)/(2y) in
 * extended precision. */
static xf sqrt_x(double x)
{
    int e = hexp(x), p;
    double g, y;
    xf Y, R;
    /* x = 16**(2p) * g, 1/256 <= g < 1 */
    p = (e & 1) ? (e + 1) / 2 : e / 2;
    g = add_exp(x, -2 * p);
    y = 0.22 + 0.89 * g;
    for (int i = 0; i < 7; i++)
        y = 0.5 * (y + g / y);
    y = add_exp(y, p);
    Y = xz(y);
    R = xsub(xz(x), xmul(Y, Y));
    return xadd(Y, xscale(xdiv(R, Y), -1));
}

void sqs(const float *x, float *r) ENTRY("SQS0");
void sqs(const float *x, float *r)
{
    unsigned m;
    if (*x <= 0) {
        if (*x == 0) {
            *r = *x;
            return;
        }
        pl1_math_error(0x0100);
        return;
    }
    m = mask_off();
    *r = xs(sqrt_x(*x));
    mask_on(m);
}

void sql(const double *x, double *r) ENTRY("SQL0");
void sql(const double *x, double *r)
{
    unsigned m;
    if (*x <= 0) {
        if (*x == 0) {
            *r = *x;
            return;
        }
        pl1_math_error(0x0108);
        return;
    }
    m = mask_off();
    *r = xd(sqrt_x(*x));
    mask_on(m);
}
