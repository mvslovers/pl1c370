/* pl1mhyp.c -- SINH, COSH, TANH (rt/pl1mth.h; the method and the accuracy:
 * internals/research/pli/tier14-measurement.md). */
#include "pl1mth.h"

static const xf X_R120 = {0, -6, 0x8888888888888889ULL};

static const dbl T_SNH[9] = {
    {{0x2E2EC368, 0x262C7034}}, {{0x305C6E3B, 0xDB73D5C6}},
    {{0x3297A4DA, 0x340A0AB9}}, {{0x34CA963B, 0x81856A53}},
    {{0x36D73F9F, 0x399DC0F9}}, {{0x38B09230, 0x9D43684C}},
    {{0x3A6B9915, 0x9FD5138E}}, {{0x3C2E3BC7, 0x4AAD8E67}},
    {{0x3DD00D00, 0xD00D00D0}},
};

static const dbl T_EM1[21] = {
    {{0x2D1F2CF0, 0x1972F578}}, {{0x2E2EC368, 0x262C7034}},
    {{0x2F4338E5, 0xB6DFE14A}}, {{0x305C6E3B, 0xDB73D5C6}},
    {{0x317950AE, 0x90080894}}, {{0x3297A4DA, 0x340A0AB9}},
    {{0x33B413C3, 0x1DCBECBC}}, {{0x34CA963B, 0x81856A53}},
    {{0x35D73F9F, 0x399DC0F9}}, {{0x36D73F9F, 0x399DC0F9}},
    {{0x37C9CBA5, 0x4603E4E9}}, {{0x38B09230, 0x9D43684C}},
    {{0x398F76C7, 0x7FC6C4BE}}, {{0x3A6B9915, 0x9FD5138E}},
    {{0x3B49F93E, 0xDDE27D72}}, {{0x3C2E3BC7, 0x4AAD8E67}},
    {{0x3D1A01A0, 0x1A01A01A}}, {{0x3DD00D00, 0xD00D00D0}},
    {{0x3E5B05B0, 0x5B05B05B}}, {{0x3F222222, 0x22222222}},
    {{0x3FAAAAAA, 0xAAAAAAAB}},
};

static xf sinh_x(double x)
{
    double a = x < 0 ? -x : x;
    xf A = xz(a), v;
    if (a < 1.0) {
        double z = a * a;
        xf a3 = xmul(xmul(A, A), A), a5 = xmul(xmul(a3, A), A);
        v = xadd(xadd(xadd(A, xmul(a3, X_SIXTH)), xmul(a5, X_R120)),
                 xz(a * z * z * z * poly(z, T_SNH, 9)));
    } else {
        xf e = exp_x(A);
        v = xscale(xsub(e, xdiv(X_ONE, e)), -1);
    }
    return x < 0 ? xneg(v) : v;
}

static xf cosh_x(double x)
{
    xf e = exp_x(xz(x < 0 ? -x : x));
    return xscale(xadd(e, xdiv(X_ONE, e)), -1);
}

/* EXP(t) - 1 for 0 <= t <= 1.1 */
static xf expm1_x(xf t)
{
    double td = xd(t);
    xf t2 = xmul(t, t), t3 = xmul(t2, t);
    return xadd(xadd(xadd(t, xscale(t2, -1)), xmul(t3, X_SIXTH)),
                xz(td * td * td * td * poly(td, T_EM1, 21)));
}

static xf tanh_x(double x, double big)
{
    double a = x < 0 ? -x : x;
    xf v;
    if (a >= big)
        v = X_ONE;
    else if (a < 0.55) {
        xf m = expm1_x(xscale(xz(a), 1));
        v = xdiv(m, xadd(m, X_TWO));
    } else
        v = xsub(X_ONE, xdiv(X_TWO, xadd(exp_x(xscale(xz(a), 1)), X_ONE)));
    return x < 0 ? xneg(v) : v;
}

/* SINH, COSH: an argument beyond EXP's range overflows */
static void hyps(const float *x, float *r, int cosh)
{
    unsigned m;
    if (!exp_ok(*x < 0 ? -*x : *x))
        return;
    m = mask_off();
    *r = xs(cosh ? cosh_x(*x) : sinh_x(*x));
    mask_on(m);
}

static void hypl(const double *x, double *r, int cosh)
{
    unsigned m;
    if (!exp_ok(*x < 0 ? -*x : *x))
        return;
    m = mask_off();
    *r = xd(cosh ? cosh_x(*x) : sinh_x(*x));
    mask_on(m);
}

void shss(const float *x, float *r) ENTRY("SHSS");
void shss(const float *x, float *r) { hyps(x, r, 0); }
void shsc(const float *x, float *r) ENTRY("SHSC");
void shsc(const float *x, float *r) { hyps(x, r, 1); }
void shls(const double *x, double *r) ENTRY("SHLS");
void shls(const double *x, double *r) { hypl(x, r, 0); }
void shlc(const double *x, double *r) ENTRY("SHLC");
void shlc(const double *x, double *r) { hypl(x, r, 1); }

void ths(const float *x, float *r) ENTRY("THS0");
void ths(const float *x, float *r)
{
    unsigned m = mask_off();
    *r = xs(tanh_x(*x, 9.1));
    mask_on(m);
}

void thl(const double *x, double *r) ENTRY("THL0");
void thl(const double *x, double *r)
{
    unsigned m = mask_off();
    *r = xd(tanh_x(*x, 20.1));
    mask_on(m);
}
