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

/* the constants more than one family uses (rt/pl1mk.c): exact
 * System/370 values, decimal in the comments there; each family keeps
 * its own others */
struct pl1mk {
    dbl expmax;
    dbl expmin;
    xf x_ln2a;
    xf x_ln2b;
    xf x_one;
    xf x_two;
    xf x_third;
    xf x_sixth;
};
extern const struct pl1mk pl1mk asm("PLMK");
#define EXPMAX (pl1mk.expmax.d)
#define EXPMIN (pl1mk.expmin.d)
#define X_LN2A (pl1mk.x_ln2a)
#define X_LN2B (pl1mk.x_ln2b)
#define X_ONE (pl1mk.x_one)
#define X_TWO (pl1mk.x_two)
#define X_THIRD (pl1mk.x_third)
#define X_SIXTH (pl1mk.x_sixth)

/* the machine, and the small operations of the extended format:
 * inline, being shorter than a call */
/* Mask exponent underflow and significance; returns the old mask. */
static inline unsigned mask_off(void)
{
    unsigned r;
    __asm__ volatile("BALR %0,0" : "=r"(r));
    __asm__ volatile("SPM %0" : : "r"(r & 0xFCFFFFFF));
    return r;
}

static inline void mask_on(unsigned r)
{
    __asm__ volatile("SPM %0" : : "r"(r));
}

/* The hexadecimal exponent of x (x = 16**e * f, 1/16 <= f < 1). */
static inline int hexp(double x)
{
    dbl u;
    u.d = x;
    return (int)((u.w[0] >> 24) & 0x7F) - 64;
}

/* x with its exponent changed by n (no range check). */
static inline double add_exp(double x, int n)
{
    dbl u;
    u.d = x;
    u.w[0] += (unsigned)n << 24;
    return u.d;
}

static inline xf xneg(xf a)
{
    a.neg = !a.neg;
    return a;
}

static inline xf xscale(xf a, int k)
{
    if (a.f)
        a.e += k;
    return a;
}

/* rt/pl1mxf.c */
xf xnorm(int neg, int e, u64 f) asm("PLMXNR");
xf xz(double d) asm("PLMXZ");
xf xadd(xf a, xf b) asm("PLMXAD");
xf xsub(xf a, xf b) asm("PLMXSB");
void mul128(u64 a, u64 b, u64 *hi, u64 *lo) asm("PLMMUL");
xf xmul(xf a, xf b) asm("PLMXML");
double xd(xf a) asm("PLMXD");
float xs(xf a) asm("PLMXS");
xf xdiv(xf a, xf b) asm("PLMXDV");
/* rt/pl1movf.c */
void overflow(void) asm("PLMOVF");
/* rt/pl1mint.c */
double trunc_d(double x) asm("PLMTRC");
double nint_d(double x) asm("PLMNIN");
int mod4(double n) asm("PLMMD4");
/* rt/pl1mply.c */
double poly(double z, const dbl *c, int n) asm("PLMPLY");
xf xi(int n) asm("PLMXI");
/* rt/pl1mexp.c, rt/pl1mlog.c */
xf exp_x(xf t) asm("PLMEXP");
int exp_ok(double x) asm("PLMEOK");
xf logm_x(xf x, int *kp) asm("PLMLGM");
xf log_x(xf x) asm("PLMLOG");

#endif
