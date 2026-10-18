/* pl1mth.h -- what the members of the mathematical functions share
 * (rt/pl1mxf.c the machine and the extended format, rt/pl1m*.c the
 * functions).  Not part of the ABI: the run time's own. */
#ifndef PL1MTH_H
#define PL1MTH_H
#include "pl1rt.h"

typedef union { unsigned w[2]; double d; } dbl;
typedef unsigned long long u64;

/* A value f * 2**(e-64) with a 64-bit fraction f, normalized so that
 * its top bit is set (f = 0 for zero). */
typedef struct { int neg, e; u64 f; } xf;

#define ENTRY(n) asm("PL1" n)
#define X_ZERO ((xf){0, 0, 0})

/* the constants (rt/pl1mxf.c): exact System/370 values, decimal in the
 * comments there */
struct pl1mk {
    dbl invln2;
    dbl twoopi;
    dbl tan15;
    dbl sqrth;
    dbl expmax;
    dbl expmin;
    dbl snsmax;
    dbl snsmaxd;
    dbl snlmax;
    dbl snlmaxd;
    xf x_ln2a;
    xf x_ln2b;
    xf x_lg2a;
    xf x_lg2b;
    xf x_log2e;
    xf x_log10e;
    xf x_pio2a;
    xf x_pio2b;
    xf x_pio2c;
    xf x_pio2;
    xf x_pio6;
    xf x_pi;
    xf x_deg2rad;
    xf x_rad2deg;
    xf x_sqrt3;
    xf x_twosqpi;
    xf x_invsqpi;
    xf x_one;
    xf x_two;
    xf x_half;
    xf x_third;
    xf x_sixth;
    xf x_r24;
    xf x_r120;
    xf x_r10;
};
extern const struct pl1mk pl1mk asm("PLMK");
#define INVLN2 (pl1mk.invln2.d)
#define TWOOPI (pl1mk.twoopi.d)
#define TAN15 (pl1mk.tan15.d)
#define SQRTH (pl1mk.sqrth.d)
#define EXPMAX (pl1mk.expmax.d)
#define EXPMIN (pl1mk.expmin.d)
#define SNSMAX (pl1mk.snsmax.d)
#define SNSMAXD (pl1mk.snsmaxd.d)
#define SNLMAX (pl1mk.snlmax.d)
#define SNLMAXD (pl1mk.snlmaxd.d)
#define X_LN2A (pl1mk.x_ln2a)
#define X_LN2B (pl1mk.x_ln2b)
#define X_LG2A (pl1mk.x_lg2a)
#define X_LG2B (pl1mk.x_lg2b)
#define X_LOG2E (pl1mk.x_log2e)
#define X_LOG10E (pl1mk.x_log10e)
#define X_PIO2A (pl1mk.x_pio2a)
#define X_PIO2B (pl1mk.x_pio2b)
#define X_PIO2C (pl1mk.x_pio2c)
#define X_PIO2 (pl1mk.x_pio2)
#define X_PIO6 (pl1mk.x_pio6)
#define X_PI (pl1mk.x_pi)
#define X_DEG2RAD (pl1mk.x_deg2rad)
#define X_RAD2DEG (pl1mk.x_rad2deg)
#define X_SQRT3 (pl1mk.x_sqrt3)
#define X_TWOSQPI (pl1mk.x_twosqpi)
#define X_INVSQPI (pl1mk.x_invsqpi)
#define X_ONE (pl1mk.x_one)
#define X_TWO (pl1mk.x_two)
#define X_HALF (pl1mk.x_half)
#define X_THIRD (pl1mk.x_third)
#define X_SIXTH (pl1mk.x_sixth)
#define X_R24 (pl1mk.x_r24)
#define X_R120 (pl1mk.x_r120)
#define X_R10 (pl1mk.x_r10)

/* rt/pl1mxf.c */
unsigned mask_off(void) asm("PLMOFF");
void mask_on(unsigned r) asm("PLMON");
void overflow(void) asm("PLMOVF");
int hexp(double x) asm("PLMHXP");
double add_exp(double x, int n) asm("PLMADE");
double trunc_d(double x) asm("PLMTRC");
double nint_d(double x) asm("PLMNIN");
int mod4(double n) asm("PLMMD4");
double poly(double z, const dbl *c, int n) asm("PLMPLY");
xf xnorm(int neg, int e, u64 f) asm("PLMXNR");
xf xz(double d) asm("PLMXZ");
xf xi(int n) asm("PLMXI");
xf xneg(xf a) asm("PLMXNG");
xf xscale(xf a, int k) asm("PLMXSC");
xf xadd(xf a, xf b) asm("PLMXAD");
xf xsub(xf a, xf b) asm("PLMXSB");
xf xmul(xf a, xf b) asm("PLMXML");
xf xsqr2(xf a, xf *lo) asm("PLMXSQ");
double xd(xf a) asm("PLMXD");
float xs(xf a) asm("PLMXS");
xf xdiv(xf a, xf b) asm("PLMXDV");
/* rt/pl1mexp.c, rt/pl1mlog.c */
xf exp_x(xf t) asm("PLMEXP");
int exp_ok(double x) asm("PLMEOK");
xf logm_x(xf x, int *kp) asm("PLMLGM");
xf log_x(xf x) asm("PLMLOG");

#endif
