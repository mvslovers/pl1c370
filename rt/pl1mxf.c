/* pl1mxf.c -- the mathematical built-in functions of PL1C/370 in
 * short and long floating point: SQRT, EXP, LOG, LOG2, LOG10, SIN, COS,
 * SIND, COSD, TAN, TAND, ATAN, ATAND, SINH, COSH, TANH, ATANH, ERF,
 * ERFC, and x ** y.
 *
 * Each routine is called by the generated code with R1 -> A(argument)
 * [, A(second argument)], A(result).  A short routine computes in long
 * precision and rounds; a long one works in long precision throughout,
 * with argument reduction by split constants where cancellation would
 * otherwise cost digits.  The arguments that PL/I declares in error
 * (GC28-8201-4, the IHE2xxI messages of GC28-6594-7) end in
 * pl1_math_error() with the same codes and messages as PL/I (F).  An
 * EXP that exceeds the largest number raises OVERFLOW by an exponent
 * overflow, as a floating-point operation would; underflow and loss of
 * significance inside a routine are masked.
 *
 * The constants are exact System/370 hexadecimal values (comments give
 * their decimal value); the series coefficients are 1/n! and 1/(2j+1)
 * rounded to long precision.
 *
 * The functions are members of their own, so that a program links only
 * what it calls: pl1msqr.c SQRT, pl1mexp.c EXP, pl1mlog.c LOG, LOG2 and
 * LOG10, pl1msin.c SIN, COS and TAN, pl1matn.c ATAN, pl1mhyp.c SINH,
 * COSH and TANH, pl1mhta.c ATANH, pl1merf.c ERF and ERFC, pl1mpow.c
 * x ** y.  This member holds what they share: the constants, the
 * machine and the extended format. */
#include "pl1mth.h"

const struct pl1mk pl1mk = {
    .invln2 = {{0x41171547, 0x652B82FE}},   /* 1.4426950408889634 */
    .twoopi = {{0x40A2F983, 0x6E4E4415}},   /* 0.6366197723675814 */
    .tan15 = {{0x40449851, 0x7A7B3559}},   /* 0.2679491924311227 */
    .sqrth = {{0x40B504F3, 0x33F9DE65}},   /* 0.7071067811865476 */
    .expmax = {{0x42AEAC4F, 0x97F2883C}},   /* 174.67308950110623 */
    .expmin = {{0xC2B437E0, 0x57B116B8}},   /* -180.21826694558578 */
    .snsmax = {{0x45C90FDA, 0xA22168C2}},   /* 823549.6645826427 */
    .snsmaxd = {{0x472D0000, 0x00000000}},   /* 47185920.0 */
    .snlmax = {{0x4DC90FDA, 0xA22168C2}},   /* 3537118876014220.0 */
    .snlmaxd = {{0x4F2D0000, 0x00000000}},   /* 2.0266198323167232e+17 */
    .x_ln2a = {0, 0, 0xB17217F7D1CF7800ULL},
    .x_ln2b = {0, -55, 0xD5E4F1D9CC01F97BULL},
    .x_lg2a = {0, -1, 0x9A209A84FBCFF600ULL},
    .x_lg2b = {0, -56, 0xCC47C4ACD605BE49ULL},
    .x_log2e = {0, 1, 0xB8AA3B295C17F0BCULL},
    .x_log10e = {0, -1, 0xDE5BD8A937287195ULL},
    .x_pio2a = {0, 1, 0xC90FDAA221600000ULL},
    .x_pio2b = {0, -43, 0x8C234C4C66200000ULL},
    .x_pio2c = {0, -87, 0x8B80DC1CD129024EULL},
    .x_pio2 = {0, 1, 0xC90FDAA22168C235ULL},
    .x_pio6 = {0, 0, 0x860A91C16B9B2C23ULL},
    .x_pi = {0, 2, 0xC90FDAA22168C235ULL},
    .x_deg2rad = {0, -5, 0x8EFA351294E9C8AEULL},
    .x_rad2deg = {0, 6, 0xE52EE0D31E0FBDC3ULL},
    .x_sqrt3 = {0, 1, 0xDDB3D742C265539EULL},
    .x_twosqpi = {0, 1, 0x906EBA8214DB688DULL},
    .x_invsqpi = {0, 0, 0x906EBA8214DB688DULL},
    .x_one = {0, 1, 0x8000000000000000ULL},
    .x_two = {0, 2, 0x8000000000000000ULL},
    .x_half = {0, 0, 0x8000000000000000ULL},
    .x_third = {0, -1, 0xAAAAAAAAAAAAAAABULL},
    .x_sixth = {0, -2, 0xAAAAAAAAAAAAAAABULL},
    .x_r24 = {0, -4, 0xAAAAAAAAAAAAAAABULL},
    .x_r120 = {0, -6, 0x8888888888888889ULL},
    .x_r10 = {0, -3, 0xCCCCCCCCCCCCCCCDULL},
};

/* --- the machine ------------------------------------------------- */

/* Mask exponent underflow and significance; returns the old mask. */
unsigned mask_off(void)
{
    unsigned r;
    __asm__ volatile("BALR %0,0" : "=r"(r));
    __asm__ volatile("SPM %0" : : "r"(r & 0xFCFFFFFF));
    return r;
}

void mask_on(unsigned r)
{
    __asm__ volatile("SPM %0" : : "r"(r));
}

/* An exponent overflow, which the program interruption exit turns into
 * the OVERFLOW condition. */
void overflow(void)
{
    static const dbl big = {{0x7FFFFFFF, 0xFFFFFFFF}};
    double x = big.d;
    __asm__ volatile("ADR %0,%0" : "+f"(x));
}

/* The hexadecimal exponent of x (x = 16**e * f, 1/16 <= f < 1). */
int hexp(double x)
{
    dbl u;
    u.d = x;
    return (int)((u.w[0] >> 24) & 0x7F) - 64;
}

/* x with its exponent changed by n (no range check). */
double add_exp(double x, int n)
{
    dbl u;
    u.d = x;
    u.w[0] += (unsigned)n << 24;
    return u.d;
}

/* The integer part of x, exactly. */
double trunc_d(double x)
{
    dbl u;
    int e = hexp(x), bits;
    u.d = x;
    if (e <= 0)
        return 0.0;
    if (e >= 14)
        return x;
    bits = 56 - 4 * e;               /* fraction bits right of the point */
    if (bits >= 32) {
        u.w[1] = 0;
        u.w[0] &= ~((1u << (bits - 32)) - 1);
    } else
        u.w[1] &= ~((1u << bits) - 1);
    return u.d;
}

/* The nearest integer to x. */
double nint_d(double x)
{
    return x < 0 ? -trunc_d(0.5 - x) : trunc_d(x + 0.5);
}

/* n mod 4 for an integral n. */
int mod4(double n)
{
    double r = n - 4.0 * trunc_d(n * 0.25);
    int q = r < 0 ? -(int)-r : (int)r;
    return q & 3;
}

double poly(double z, const dbl *c, int n)
{
    double p = c[0].d;
    for (int i = 1; i < n; i++)
        p = p * z + c[i].d;
    return p;
}

/* --- extended precision ------------------------------------------
 * A value f * 2**(e-64) with a 64-bit fraction f, normalized so that
 * its top bit is set (f = 0 for zero).  The leading terms of each
 * function are computed in it, the small tails in long floating point,
 * and the result is rounded once. */


xf xnorm(int neg, int e, u64 f)
{
    xf r;
    if (!f)
        return X_ZERO;
    while (!(f >> 63)) {
        f <<= 1;
        e--;
    }
    r.neg = neg;
    r.e = e;
    r.f = f;
    return r;
}

xf xz(double d)
{
    dbl u;
    u.d = d;
    return xnorm(u.w[0] >> 31, 4 * (int)((u.w[0] >> 24) & 0x7F) - 256 + 8,
                 (u64)(u.w[0] & 0x00FFFFFF) << 32 | (u64)u.w[1]);
}

xf xi(int n)
{
    return xnorm(n < 0, 64, (u64)(n < 0 ? -n : n));
}

xf xneg(xf a)
{
    a.neg = !a.neg;
    return a;
}

xf xscale(xf a, int k)
{
    if (a.f)
        a.e += k;
    return a;
}

/* ABS(a) >= ABS(b) */
static int xge(xf a, xf b)
{
    if (!b.f)
        return 1;
    if (!a.f)
        return 0;
    return a.e > b.e || (a.e == b.e && a.f >= b.f);
}

xf xadd(xf a, xf b)
{
    int d;
    u64 bf;
    if (!b.f)
        return a;
    if (!a.f)
        return b;
    if (!xge(a, b)) {
        xf t = a;
        a = b;
        b = t;
    }
    d = a.e - b.e;
    if (d >= 64)
        return a;
    bf = b.f >> d;
    if (a.neg == b.neg) {
        u64 sum = a.f + bf;
        if (sum < a.f)                   /* carry out of bit 63 */
            return xnorm(a.neg, a.e + 1, (sum >> 1) | ((u64)1 << 63));
        return xnorm(a.neg, a.e, sum);
    }
    return xnorm(a.neg, a.e, a.f - bf);
}

xf xsub(xf a, xf b)
{
    return xadd(a, xneg(b));
}

/* the 128-bit product of the fractions, high and low halves */
static void mul128(u64 a, u64 b, u64 *hi, u64 *lo)
{
    u64 ah = a >> 32, al = a & 0xFFFFFFFFu, bh = b >> 32, bl = b & 0xFFFFFFFFu;
    u64 hh = ah * bh, hl = ah * bl, lh = al * bh, ll = al * bl;
    u64 mid = (ll >> 32) + (hl & 0xFFFFFFFFu) + (lh & 0xFFFFFFFFu);
    *lo = (mid << 32) | (ll & 0xFFFFFFFFu);
    *hi = hh + (hl >> 32) + (lh >> 32) + (mid >> 32);
}

xf xmul(xf a, xf b)
{
    u64 hi, lo;
    if (!a.f || !b.f)
        return X_ZERO;
    mul128(a.f, b.f, &hi, &lo);
    if (!(hi >> 63))
        return xnorm(a.neg != b.neg, a.e + b.e - 1, (hi << 1) | (lo >> 63));
    return xnorm(a.neg != b.neg, a.e + b.e, hi);
}

/* a*a as hi + lo exactly (the low half of the 128-bit product) */
xf xsqr2(xf a, xf *lo)
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

/* to long floating point, rounded to the nearest; bits 56 or 24 */
static void xround(xf a, int bits, unsigned *w0, unsigned *w1)
{
    int h, sh, c;
    u64 fr, rem;
    *w0 = *w1 = 0;
    if (!a.f)
        return;
    h = a.e >= 0 ? (a.e + 3) / 4 : -((-a.e) / 4);    /* ceil(e/4) */
    sh = 64 - bits + 4 * h - a.e;
    fr = a.f >> sh;
    rem = a.f & (((u64)1 << sh) - 1);
    if (rem >= (u64)1 << (sh - 1))
        fr++;
    if (fr >> bits) {
        fr >>= 4;
        h++;
    }
    c = h + 64;
    if (c < 0)
        return;                          /* below the smallest number */
    if (c > 127) {                       /* not reached: the callers check */
        c = 127;
        fr = ((u64)1 << bits) - 1;
    }
    if (bits == 56) {
        *w0 = (a.neg ? 0x80000000u : 0) | (unsigned)c << 24 | (unsigned)(fr >> 32);
        *w1 = (unsigned)fr;
    } else
        *w0 = (a.neg ? 0x80000000u : 0) | (unsigned)c << 24 | (unsigned)fr;
}

double xd(xf a)
{
    dbl u;
    xround(a, 56, &u.w[0], &u.w[1]);
    return u.d;
}

float xs(xf a)
{
    union { unsigned w; float f; } u;
    unsigned w1;
    xround(a, 24, &u.w, &w1);
    return u.f;
}

xf xdiv(xf a, xf b)
{
    double bd = xd(b);
    xf q = xz(xd(a) / bd);
    xf r = xsub(a, xmul(q, b));
    return xadd(q, xz(xd(r) / bd));
}

