/* pl1matn.c -- ATAN, ATAND, with one and two arguments (rt/pl1mth.h; the method and the accuracy:
 * internals/research/pli/tier14-measurement.md). */
#include "pl1mth.h"

static const dbl T_ATN[14] = {
    {{0xBF842108, 0x42108421}}, {{0x3F8D3DCB, 0x08D3DCB1}},
    {{0xBF97B425, 0xED097B42}}, {{0x3FA3D70A, 0x3D70A3D7}},
    {{0xBFB21642, 0xC8590B21}}, {{0x3FC30C30, 0xC30C30C3}},
    {{0xBFD79435, 0xE50D7943}}, {{0x3FF0F0F0, 0xF0F0F0F1}},
    {{0xC0111111, 0x11111111}}, {{0x4013B13B, 0x13B13B14}},
    {{0xC01745D1, 0x745D1746}}, {{0x401C71C7, 0x1C71C71C}},
    {{0xC0249249, 0x24924925}}, {{0x40333333, 0x33333333}},
};

/* ATAN(a) for a >= 0: 1/a above 1, a turned by pi/6 above TAN(pi/12),
 * then a - a**3/3 + a**5*(1/5 - ...). */
static xf atan_k(xf a)
{
    int flip = 0, six = 0;
    double ad, z;
    xf y;
    if (xd(a) > 1.0) {
        a = xdiv(X_ONE, a);
        flip = 1;
    }
    if (xd(a) > TAN15) {
        a = xdiv(xsub(xmul(X_SQRT3, a), X_ONE), xadd(a, X_SQRT3));
        six = 1;
    }
    ad = xd(a);
    z = ad * ad;
    y = xadd(xsub(a, xmul(xmul(xmul(a, a), a), X_THIRD)), xz(ad * z * z * poly(z, T_ATN, 14)));
    if (six)
        y = xadd(X_PIO6, y);
    if (flip)
        y = xsub(X_PIO2, y);
    return y;
}

static xf atan2_x(double y, double x)
{
    xf v;
    if (x == 0.0 || hexp(y) - hexp(x) > 15)
        v = X_PIO2;
    else {
        v = atan_k(xdiv(xz(y < 0 ? -y : y), xz(x < 0 ? -x : x)));
        if (x < 0)
            v = xsub(X_PI, v);
    }
    return y < 0 ? xneg(v) : v;
}

/* ATAN(x) and ATAN(y,x), radians or degrees */
static xf atan_any(double y, const double *x, int deg)
{
    xf v;
    if (x)
        v = atan2_x(y, *x);
    else {
        v = atan_k(xz(y < 0 ? -y : y));
        if (y < 0)
            v = xneg(v);
    }
    return deg ? xmul(v, X_RAD2DEG) : v;
}

static void atans(const float *y, const float *x, float *r, int deg)
{
    unsigned m;
    double xd_;
    if (x && *x == 0 && *y == 0) {
        pl1_math_error(0x0105);
        return;
    }
    xd_ = x ? *x : 0;
    m = mask_off();
    *r = xs(atan_any(*y, x ? &xd_ : 0, deg));
    mask_on(m);
}

static void atanl_(const double *y, const double *x, double *r, int deg)
{
    unsigned m;
    if (x && *x == 0 && *y == 0) {
        pl1_math_error(0x010D);
        return;
    }
    m = mask_off();
    *r = xd(atan_any(*y, x, deg));
    mask_on(m);
}

void ats1(const float *x, float *r) ENTRY("ATS1");
void ats1(const float *x, float *r) { atans(x, 0, r, 0); }
void ats3(const float *x, float *r) ENTRY("ATS3");
void ats3(const float *x, float *r) { atans(x, 0, r, 1); }
void ats2(const float *y, const float *x, float *r) ENTRY("ATS2");
void ats2(const float *y, const float *x, float *r) { atans(y, x, r, 0); }
void ats4(const float *y, const float *x, float *r) ENTRY("ATS4");
void ats4(const float *y, const float *x, float *r) { atans(y, x, r, 1); }
void atl1(const double *x, double *r) ENTRY("ATL1");
void atl1(const double *x, double *r) { atanl_(x, 0, r, 0); }
void atl3(const double *x, double *r) ENTRY("ATL3");
void atl3(const double *x, double *r) { atanl_(x, 0, r, 1); }
void atl2(const double *y, const double *x, double *r) ENTRY("ATL2");
void atl2(const double *y, const double *x, double *r) { atanl_(y, x, r, 0); }
void atl4(const double *y, const double *x, double *r) ENTRY("ATL4");
void atl4(const double *y, const double *x, double *r) { atanl_(y, x, r, 1); }
