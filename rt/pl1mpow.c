/* pl1mpow.c -- x ** y with a FLOAT exponent (rt/pl1mth.h; the method and the accuracy:
 * internals/research/pli/tier14-measurement.md). */
#include "pl1mth.h"

/* x ** y = EXP(y*LOG(x)); x = 0 is 0 for y > 0 and an error otherwise,
 * y = 0 gives 1, a negative x is LOG's error.  Returns 1 with *v set,
 * 0 after an error or an overflow. */
static int pow_x(double x, double y, xf *v, int lng)
{
    unsigned m;
    xf t;
    double td;
    if (x == 0) {
        if (y > 0) {
            *v = X_ZERO;
            return 1;
        }
        pl1_math_error(lng ? 0x0113 : 0x0112);
        return 0;
    }
    if (y == 0) {
        *v = X_ONE;
        return 1;
    }
    if (x < 0) {
        pl1_math_error(lng ? 0x010A : 0x0102);
        return 0;
    }
    m = mask_off();
    t = xmul(xz(y), log_x(xz(x)));
    td = xd(t);
    if (td <= EXPMAX)
        *v = td < EXPMIN ? X_ZERO : exp_x(t);
    mask_on(m);
    if (td > EXPMAX) {
        overflow();
        return 0;
    }
    return 1;
}

void xxs(const float *x, const float *y, float *r) ENTRY("XXS0");
void xxs(const float *x, const float *y, float *r)
{
    xf v;
    unsigned m;
    if (pow_x(*x, *y, &v, 0)) {
        m = mask_off();
        *r = xs(v);
        mask_on(m);
    }
}

void xxl(const double *x, const double *y, double *r) ENTRY("XXL0");
void xxl(const double *x, const double *y, double *r)
{
    xf v;
    unsigned m;
    if (pow_x(*x, *y, &v, 1)) {
        m = mask_off();
        *r = xd(v);
        mask_on(m);
    }
}
