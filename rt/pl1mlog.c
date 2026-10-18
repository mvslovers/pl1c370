/* pl1mlog.c -- LOG, LOG2, LOG10, and the LOG the other functions use (rt/pl1mth.h; the method and the accuracy:
 * internals/research/pli/tier14-measurement.md). */
#include "pl1mth.h"

static const dbl k_sqrth = {{0x40B504F3, 0x33F9DE65}};   /* 0.7071067811865476 */
#define SQRTH (k_sqrth.d)
static const xf X_LG2A = {0, -1, 0x9A209A84FBCFF600ULL};
static const xf X_LG2B = {0, -56, 0xCC47C4ACD605BE49ULL};
static const xf X_LOG2E = {0, 1, 0xB8AA3B295C17F0BCULL};
static const xf X_LOG10E = {0, -1, 0xDE5BD8A937287195ULL};

static const dbl T_LOG[13] = {
    {{0x3F97B425, 0xED097B42}}, {{0x3FA3D70A, 0x3D70A3D7}},
    {{0x3FB21642, 0xC8590B21}}, {{0x3FC30C30, 0xC30C30C3}},
    {{0x3FD79435, 0xE50D7943}}, {{0x3FF0F0F0, 0xF0F0F0F1}},
    {{0x40111111, 0x11111111}}, {{0x4013B13B, 0x13B13B14}},
    {{0x401745D1, 0x745D1746}}, {{0x401C71C7, 0x1C71C71C}},
    {{0x40249249, 0x24924925}}, {{0x40333333, 0x33333333}},
    {{0x40555555, 0x55555555}},
};

/* LOG(x), x > 0: x = 2**k * m, SQRT(1/2) <= m < SQRT(2),
 * LOG(m) = 2s + 2s*(s**2/3 + s**4/5 + ...), s = (m-1)/(m+1). */
xf logm_x(xf x, int *kp)
{
    xf m = x, s;
    int k = x.e;
    double sd, z;
    m.e = 0;                             /* 1/2 <= m < 1 */
    if (xd(m) < SQRTH) {
        m.e = 1;
        k--;
    }
    s = xdiv(xsub(m, X_ONE), xadd(m, X_ONE));
    sd = xd(s);
    z = sd * sd;
    *kp = k;
    return xadd(xscale(s, 1), xz(2.0 * sd * z * poly(z, T_LOG, 13)));
}

xf log_x(xf x)
{
    int k;
    xf lm = logm_x(x, &k), K = xi(k);
    return xadd(xadd(xmul(K, X_LN2A), lm), xmul(K, X_LN2B));
}

/* LOG (which 0), LOG2 (1), LOG10 (2) */
static xf logw_x(double x, int which)
{
    int k;
    xf lm = logm_x(xz(x), &k), K = xi(k);
    if (which == 1)
        return xadd(K, xmul(lm, X_LOG2E));
    if (which == 2)
        return xadd(xadd(xmul(K, X_LG2A), xmul(lm, X_LOG10E)), xmul(K, X_LG2B));
    return xadd(xadd(xmul(K, X_LN2A), lm), xmul(K, X_LN2B));
}

static void logs(const float *x, float *r, int which)
{
    unsigned m;
    if (*x <= 0) {
        pl1_math_error(0x0102);
        return;
    }
    m = mask_off();
    *r = xs(logw_x(*x, which));
    mask_on(m);
}

static void logl_(const double *x, double *r, int which)
{
    unsigned m;
    if (*x <= 0) {
        pl1_math_error(0x010A);
        return;
    }
    m = mask_off();
    *r = xd(logw_x(*x, which));
    mask_on(m);
}

void lnse(const float *x, float *r) ENTRY("LNSE");
void lnse(const float *x, float *r) { logs(x, r, 0); }
void lns2(const float *x, float *r) ENTRY("LNS2");
void lns2(const float *x, float *r) { logs(x, r, 1); }
void lnsd(const float *x, float *r) ENTRY("LNSD");
void lnsd(const float *x, float *r) { logs(x, r, 2); }
void lnle(const double *x, double *r) ENTRY("LNLE");
void lnle(const double *x, double *r) { logl_(x, r, 0); }
void lnl2(const double *x, double *r) ENTRY("LNL2");
void lnl2(const double *x, double *r) { logl_(x, r, 1); }
void lnld(const double *x, double *r) ENTRY("LNLD");
void lnld(const double *x, double *r) { logl_(x, r, 2); }
