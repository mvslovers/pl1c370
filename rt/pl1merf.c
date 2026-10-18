/* pl1merf.c -- ERF, ERFC (rt/pl1mth.h; the method and the accuracy:
 * internals/research/pli/tier14-measurement.md). */
#include "pl1mth.h"

static const xf X_TWOSQPI = {0, 1, 0x906EBA8214DB688DULL};
static const xf X_INVSQPI = {0, 0, 0x906EBA8214DB688DULL};
static const xf X_HALF = {0, 0, 0x8000000000000000ULL};
static const xf X_R10 = {0, -3, 0xCCCCCCCCCCCCCCCDULL};

static const dbl T_ERF[20] = {
    {{0x2E17E6B8, 0x1382CD42}}, {{0xAF22648D, 0x579C37BB}},
    {{0x302F57AE, 0x44B1F6DC}}, {{0xB13E3680, 0xE76694CF}},
    {{0x324DDF08, 0x443C820C}}, {{0xB35C9C72, 0xF9618FB1}},
    {{0x34685CE8, 0x59FEE933}}, {{0xB56F1894, 0x3EC508C2}},
    {{0x366F55DF, 0x99634081}}, {{0xB768A27B, 0x9F903DCE}},
    {{0x385BD123, 0x84FA1784}}, {{0xB94AD9CC, 0x42AA7CE9}},
    {{0x3A385C60, 0xA90E166F}}, {{0xBB26EEF8, 0xAAAD199A}},
    {{0x3C187A00, 0x187A0018}}, {{0xBCDDEBBC, 0x99A77855}},
    {{0x3D700700, 0x70070070}}, {{0xBE31A603, 0x1A6031A6}},
    {{0x3F12F684, 0xBDA12F68}}, {{0xBF618618, 0x61861862}},
};

/* a*a as hi + lo exactly (the low half of the 128-bit product) */
static xf xsqr2(xf a, xf *lo)
{
    u64 hi, l;
    int e = 2 * a.e;
    if (!a.f) {
        *lo = X_ZERO;
        return X_ZERO;
    }
    mul128(a.f, a.f, &hi, &l);
    if (!(hi >> 63)) {
        hi = (hi << 1) | (l >> 63);
        l <<= 1;
        e--;
    }
    *lo = xnorm(0, e - 64, l);
    return xnorm(0, e, hi);
}

/* EXP(-a*a)/SQRT(pi) over Laplace's continued fraction
 * a + (1/2)/(a + 1/(a + (3/2)/(a + ...))), for a >= 1: ERFC(a).  The
 * fraction is evaluated from the bottom, its last ten steps in extended
 * precision. */
static xf erfc_cf(double a)
{
    int n = a < 1.5 ? 300 : a < 2.0 ? 150 : a < 3.0 ? 80 : a < 5.0 ? 50 : a < 8.0 ? 40 : 30;
    double k = a;
    xf A = xz(a), K, lo, sq;
    for (int i = n; i > 10; i--)
        k = a + (0.5 * i) / k;
    K = xz(k);
    for (int i = 10; i >= 1; i--)
        K = xadd(A, xdiv(xmul(xi(i), X_HALF), K));
    sq = xsqr2(A, &lo);
    if (xd(sq) > -EXPMIN)
        return X_ZERO;
    return xdiv(xmul(xmul(exp_x(xneg(sq)), xsub(X_ONE, lo)), X_INVSQPI), K);
}

/* ERF(a) for 0 <= a < 1: 2/SQRT(pi)*(a - a**3/3 + a**5/10 - ...) */
static xf erf_series(double a)
{
    double z = a * a;
    xf A = xz(a), a3 = xmul(xmul(A, A), A), a5 = xmul(xmul(a3, A), A);
    xf s = xadd(xadd(xsub(A, xmul(a3, X_THIRD)), xmul(a5, X_R10)),
                xz(a * z * z * z * poly(z, T_ERF, 20)));
    return xmul(s, X_TWOSQPI);
}

static xf erf_x(double x)
{
    double a = x < 0 ? -x : x;
    xf v = a < 1.0 ? erf_series(a) : xsub(X_ONE, erfc_cf(a));
    return x < 0 ? xneg(v) : v;
}

static xf erfc_x(double x)
{
    double a = x < 0 ? -x : x;
    xf v = a < 1.0 ? xsub(X_ONE, erf_series(a)) : erfc_cf(a);
    return x < 0 ? xsub(X_TWO, v) : v;
}

void efsf(const float *x, float *r) ENTRY("EFSF");
void efsf(const float *x, float *r)
{
    unsigned m = mask_off();
    *r = xs(erf_x(*x));
    mask_on(m);
}

void efsc(const float *x, float *r) ENTRY("EFSC");
void efsc(const float *x, float *r)
{
    unsigned m = mask_off();
    *r = xs(erfc_x(*x));
    mask_on(m);
}

void eflf(const double *x, double *r) ENTRY("EFLF");
void eflf(const double *x, double *r)
{
    unsigned m = mask_off();
    *r = xd(erf_x(*x));
    mask_on(m);
}

void eflc(const double *x, double *r) ENTRY("EFLC");
void eflc(const double *x, double *r)
{
    unsigned m = mask_off();
    *r = xd(erfc_x(*x));
    mask_on(m);
}
