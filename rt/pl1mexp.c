/* pl1mexp.c -- EXP, and the EXP the other functions use (rt/pl1mth.h; the method and the accuracy:
 * internals/research/pli/tier14-measurement.md). */
#include "pl1mth.h"

static const dbl k_invln2 = {{0x41171547, 0x652B82FE}};   /* 1.4426950408889634 */
#define INVLN2 (k_invln2.d)

static const dbl T_EXP[16] = {
    {{0x33B413C3, 0x1DCBECBC}}, {{0x34CA963B, 0x81856A53}},
    {{0x35D73F9F, 0x399DC0F9}}, {{0x36D73F9F, 0x399DC0F9}},
    {{0x37C9CBA5, 0x4603E4E9}}, {{0x38B09230, 0x9D43684C}},
    {{0x398F76C7, 0x7FC6C4BE}}, {{0x3A6B9915, 0x9FD5138E}},
    {{0x3B49F93E, 0xDDE27D72}}, {{0x3C2E3BC7, 0x4AAD8E67}},
    {{0x3D1A01A0, 0x1A01A01A}}, {{0x3DD00D00, 0xD00D00D0}},
    {{0x3E5B05B0, 0x5B05B05B}}, {{0x3F222222, 0x22222222}},
    {{0x3FAAAAAA, 0xAAAAAAAB}}, {{0x402AAAAA, 0xAAAAAAAB}},
};

/* EXP(t): t = n*LN2 + r, ABS(r) <= LN2/2, EXP(t) = 2**n * (1 + r + r**2/2
 * + r**3*(1/6 + ...)).  An underflow gives zero when rounded. */
xf exp_x(xf t)
{
    double n = nint_d(xd(t) * INVLN2), rd;
    xf N = xz(n), r, r2;
    int k;
    r = xsub(xsub(t, xmul(N, X_LN2A)), xmul(N, X_LN2B));
    rd = xd(r);
    r2 = xmul(r, r);
    r = xadd(xadd(r, xscale(r2, -1)), xz(rd * rd * rd * poly(rd, T_EXP, 16)));
    k = n < 0 ? -(int)-n : (int)n;
    return xscale(xadd(X_ONE, r), k);
}

/* EXP: above EXPMAX an exponent overflow and no result; 1 when the
 * result is to be computed */
int exp_ok(double x)
{
    if (x > EXPMAX) {
        overflow();
        return 0;
    }
    return 1;
}

void exs(const float *x, float *r) ENTRY("EXS0");
void exs(const float *x, float *r)
{
    unsigned m;
    if (!exp_ok(*x))
        return;
    m = mask_off();
    *r = *x < EXPMIN ? 0 : xs(exp_x(xz(*x)));
    mask_on(m);
}

void exl(const double *x, double *r) ENTRY("EXL0");
void exl(const double *x, double *r)
{
    unsigned m;
    if (!exp_ok(*x))
        return;
    m = mask_off();
    *r = *x < EXPMIN ? 0 : xd(exp_x(xz(*x)));
    mask_on(m);
}
