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
 * x ** y.  This member holds what every one of them needs: the
 * machine and the extended format.  What only some need is in members
 * of its own: pl1mk.c the constants several families share (each family
 * keeps its others), pl1mint.c the integral parts, pl1mply.c the
 * polynomials, pl1movf.c OVERFLOW. */
#include "pl1mth.h"


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

/* ABS(a) >= ABS(b) */
static int xge(xf a, xf b)
{
    if (!b.f)
        return 1;
    if (!a.f)
        return 0;
    return a.e > b.e || (a.e == b.e && a.f >= b.f);
}

/* not inlined into xsub: one copy of the addition */
__attribute__((noinline)) xf xadd(xf a, xf b)
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

/* the 128-bit product of the fractions, high and low halves (also
 * ERF's exact square) */
__attribute__((noinline)) void mul128(u64 a, u64 b, u64 *hi, u64 *lo)
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

