/* pl1msin.c -- SIN, COS, SIND, COSD, TAN, TAND (rt/pl1mth.h; the method and the accuracy:
 * internals/research/pli/tier14-measurement.md). */
#include "pl1mth.h"

static const dbl T_SIN[9] = {
    {{0x305C6E3B, 0xDB73D5C6}}, {{0xB297A4DA, 0x340A0AB9}},
    {{0x34CA963B, 0x81856A53}}, {{0xB6D73F9F, 0x399DC0F9}},
    {{0x38B09230, 0x9D43684C}}, {{0xBA6B9915, 0x9FD5138E}},
    {{0x3C2E3BC7, 0x4AAD8E67}}, {{0xBDD00D00, 0xD00D00D0}},
    {{0x3F222222, 0x22222222}},
};

static const dbl T_COS[9] = {
    {{0xAF4338E5, 0xB6DFE14A}}, {{0x317950AE, 0x90080894}},
    {{0xB3B413C3, 0x1DCBECBC}}, {{0x35D73F9F, 0x399DC0F9}},
    {{0xB7C9CBA5, 0x4603E4E9}}, {{0x398F76C7, 0x7FC6C4BE}},
    {{0xBB49F93E, 0xDDE27D72}}, {{0x3D1A01A0, 0x1A01A01A}},
    {{0xBE5B05B0, 0x5B05B05B}},
};

/* x reduced by multiples of pi/2 (deg: of 90 degrees): the remainder
 * in radians, *q the multiple mod 4; *zero when it is an exact zero. */
static xf reduce(double x, int deg, int *q, int *zero)
{
    double n;
    xf N, r;
    if (deg) {
        n = nint_d(x * (1.0 / 90.0));
        r = xsub(xz(x), xmul(xz(n), xi(90)));
        *zero = !r.f;
        r = xmul(r, X_DEG2RAD);
    } else {
        n = nint_d(x * TWOOPI);
        N = xz(n);
        r = xsub(xsub(xsub(xz(x), xmul(N, X_PIO2A)), xmul(N, X_PIO2B)),
                 xmul(N, X_PIO2C));
        *zero = 0;
    }
    *q = mod4(n);
    return r;
}

/* SIN(r) = r - r**3/6 + r**5*(1/120 - ...) for ABS(r) <= pi/4 */
static xf sin_k(xf r)
{
    double rd = xd(r), z = rd * rd;
    xf r3 = xmul(xmul(r, r), r);
    return xadd(xsub(r, xmul(r3, X_SIXTH)), xz(rd * z * z * poly(z, T_SIN, 9)));
}

/* COS(r) = 1 - r**2/2 + r**4/24 - r**6*(1/720 - ...) */
static xf cos_k(xf r)
{
    double rd = xd(r), z = rd * rd;
    xf r2 = xmul(r, r), r4 = xmul(r2, r2);
    return xadd(xadd(xsub(X_ONE, xscale(r2, -1)), xmul(r4, X_R24)),
                xz(z * z * z * poly(z, T_COS, 9)));
}

static xf sin_x(double x, int deg)
{
    int q, zero;
    xf r = reduce(x, deg, &q, &zero), v;
    if (zero)
        v = q & 1 ? X_ONE : X_ZERO;
    else
        v = q & 1 ? cos_k(r) : sin_k(r);
    return q & 2 ? xneg(v) : v;
}

static xf cos_x(double x, int deg)
{
    int q, zero;
    xf r = reduce(x, deg, &q, &zero), v;
    if (zero)
        v = q & 1 ? X_ZERO : X_ONE;
    else
        v = q & 1 ? sin_k(r) : cos_k(r);
    return (q == 1 || q == 2) ? xneg(v) : v;
}

/* TAN; *ovf when it is infinite (TAND of an odd multiple of 90). */
static xf tan_x(double x, int deg, int *ovf)
{
    int q, zero;
    xf r = reduce(x, deg, &q, &zero), s, c;
    *ovf = 0;
    if (zero) {
        *ovf = q & 1;
        return X_ZERO;
    }
    s = sin_k(r);
    c = cos_k(r);
    return q & 1 ? xneg(xdiv(c, s)) : xdiv(s, c);
}

/* SIN, COS, SIND, COSD, TAN, TAND: ABS(x) below 2**18 (long: 2**50)
 * times pi or 180 */
static int trig_ok(double x, int lng, int deg, int code)
{
    double a = x < 0 ? -x : x;
    double lim = lng ? (deg ? SNLMAXD : SNLMAX) : (deg ? SNSMAXD : SNSMAX);
    if (a >= lim) {
        pl1_math_error(code);
        return 0;
    }
    return 1;
}

static void sincs(const float *x, float *r, int cos, int deg)
{
    unsigned m;
    if (!trig_ok(*x, 0, deg, 0x0103))
        return;
    m = mask_off();
    *r = xs(cos ? cos_x(*x, deg) : sin_x(*x, deg));
    mask_on(m);
}

static void sincl(const double *x, double *r, int cos, int deg)
{
    unsigned m;
    if (!trig_ok(*x, 1, deg, 0x010B))
        return;
    m = mask_off();
    *r = xd(cos ? cos_x(*x, deg) : sin_x(*x, deg));
    mask_on(m);
}

void snss(const float *x, float *r) ENTRY("SNSS");
void snss(const float *x, float *r) { sincs(x, r, 0, 0); }
void snsc(const float *x, float *r) ENTRY("SNSC");
void snsc(const float *x, float *r) { sincs(x, r, 1, 0); }
void snsz(const float *x, float *r) ENTRY("SNSZ");
void snsz(const float *x, float *r) { sincs(x, r, 0, 1); }
void snsk(const float *x, float *r) ENTRY("SNSK");
void snsk(const float *x, float *r) { sincs(x, r, 1, 1); }
void snls(const double *x, double *r) ENTRY("SNLS");
void snls(const double *x, double *r) { sincl(x, r, 0, 0); }
void snlc(const double *x, double *r) ENTRY("SNLC");
void snlc(const double *x, double *r) { sincl(x, r, 1, 0); }
void snlz(const double *x, double *r) ENTRY("SNLZ");
void snlz(const double *x, double *r) { sincl(x, r, 0, 1); }
void snlk(const double *x, double *r) ENTRY("SNLK");
void snlk(const double *x, double *r) { sincl(x, r, 1, 1); }

/* an infinite TAND overflows */
static void tans(const float *x, float *r, int deg)
{
    unsigned m;
    int ovf;
    float v;
    if (!trig_ok(*x, 0, deg, 0x0104))
        return;
    m = mask_off();
    v = xs(tan_x(*x, deg, &ovf));
    mask_on(m);
    if (ovf)
        overflow();
    else
        *r = v;
}

static void tanl_(const double *x, double *r, int deg)
{
    unsigned m;
    int ovf;
    double v;
    if (!trig_ok(*x, 1, deg, 0x010C))
        return;
    m = mask_off();
    v = xd(tan_x(*x, deg, &ovf));
    mask_on(m);
    if (ovf)
        overflow();
    else
        *r = v;
}

void tnsr(const float *x, float *r) ENTRY("TNSR");
void tnsr(const float *x, float *r) { tans(x, r, 0); }
void tnsd(const float *x, float *r) ENTRY("TNSD");
void tnsd(const float *x, float *r) { tans(x, r, 1); }
void tnlr(const double *x, double *r) ENTRY("TNLR");
void tnlr(const double *x, double *r) { tanl_(x, r, 0); }
void tnld(const double *x, double *r) ENTRY("TNLD");
void tnld(const double *x, double *r) { tanl_(x, r, 1); }
