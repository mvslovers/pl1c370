/* pl1mhta.c -- ATANH (rt/pl1mth.h; the method and the accuracy:
 * internals/research/pli/tier14-measurement.md). */
#include "pl1mth.h"

static const dbl T_LOG[13] = {
    {{0x3F97B425, 0xED097B42}}, {{0x3FA3D70A, 0x3D70A3D7}},
    {{0x3FB21642, 0xC8590B21}}, {{0x3FC30C30, 0xC30C30C3}},
    {{0x3FD79435, 0xE50D7943}}, {{0x3FF0F0F0, 0xF0F0F0F1}},
    {{0x40111111, 0x11111111}}, {{0x4013B13B, 0x13B13B14}},
    {{0x401745D1, 0x745D1746}}, {{0x401C71C7, 0x1C71C71C}},
    {{0x40249249, 0x24924925}}, {{0x40333333, 0x33333333}},
    {{0x40555555, 0x55555555}},
};

/* ATANH(x) for ABS(x) < 1 */
static xf atanh_x(double x)
{
    double a = x < 0 ? -x : x;
    xf A = xz(a), v;
    if (a <= 0.2) {
        double z = a * a;
        v = xadd(xadd(A, xmul(xmul(xmul(A, A), A), X_THIRD)),
                 xz(a * z * z * poly(z, T_LOG, 12)));
    } else
        v = xscale(log_x(xdiv(xadd(X_ONE, A), xsub(X_ONE, A))), -1);
    return x < 0 ? xneg(v) : v;
}

void hts(const float *x, float *r) ENTRY("HTS0");
void hts(const float *x, float *r)
{
    unsigned m;
    if (*x >= 1 || *x <= -1) {
        pl1_math_error(0x0107);
        return;
    }
    m = mask_off();
    *r = xs(atanh_x(*x));
    mask_on(m);
}

void htl(const double *x, double *r) ENTRY("HTL0");
void htl(const double *x, double *r)
{
    unsigned m;
    if (*x >= 1 || *x <= -1) {
        pl1_math_error(0x010F);
        return;
    }
    m = mask_off();
    *r = xd(atanh_x(*x));
    mask_on(m);
}
