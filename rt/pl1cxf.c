/* pl1cxf.c -- COMPLEX FIXED multiplication, division and ABS (#2, C2b).
 *
 * As C28-6590-0 gives the methods for PL/I (F): with z1 = a+bI and
 * z2 = c+dI, the product is a*c-b*d + (b*c+a*d)I, exact; the quotient
 * is (a*c+b*d)/(c*c+d*d) + (b*c-a*d)/(c*c+d*d)I, each part truncated
 * toward zero at the result's scale; ABS is SQRT(a*a+b*b) truncated.
 * PL1C/370 computes all three exactly in a wide integer, where PL/I
 * (F)'s ABS is sometimes one unit of the last place high
 * (internals/research/pli/c2-measurement.md section 3).
 *
 * The compiler passes each part as the generated code holds it: a
 * FIXED DECIMAL part as the address of an 8-byte packed value (15
 * digits), a FIXED BINARY part as a fullword; the result goes to the
 * complex temporary at its own precision (packed of p/2+1 bytes, a
 * halfword for BINARY(15) or less), ABS to an 8-byte packed value or a
 * fullword.  The wide integer has 16-bit digits, so that no 64-bit
 * arithmetic (and no helper routine for it) is needed.  The last argument describes the scales: q1 in bits 0-7,
 * q2 in 8-15, the result's q in 16-23 (signed), its precision p in
 * 24-28, FIXEDOVERFLOW enabled in bit 29, ZERODIVIDE in bit 30.  A
 * result part beyond p digits (bits) raises FIXEDOVERFLOW, as PL/I (F)
 * does once for a product whose real part has 16 digits, and keeps its
 * low digits (JOB00077); a divisor 0+0I raises ZERODIVIDE twice, once
 * for each part, and gives 0. */
#include <stddef.h>
#include "pl1rt.h"

#define NL 16                   /* 256 bits: 10**30 * 10**15 fits */
typedef struct { unsigned short w[NL]; } bn;
typedef struct { int neg; bn m; } sbn;

#define DF_FOFL (1 << 29)
#define DF_ZDIV (1 << 30)

void cxmd(const unsigned char *a, const unsigned char *b, const unsigned char *c,
          const unsigned char *d, unsigned char *r, int df) asm("PL1CXMD");
void cxdd(const unsigned char *a, const unsigned char *b, const unsigned char *c,
          const unsigned char *d, unsigned char *r, int df) asm("PL1CXDD");
void cxmb(int a, int b, int c, int d, void *r, int df) asm("PL1CXMB");
void cxdb(int a, int b, int c, int d, void *r, int df) asm("PL1CXDB");
void cxad(const unsigned char *a, const unsigned char *b, unsigned char *r, int df)
    asm("PL1CXAD");
void cxab(int a, int b, int *r, int df) asm("PL1CXAB");

/* ---- a wide unsigned integer ---- */

static void bn_set(bn *x, unsigned v)
{
    for (int i = 0; i < NL; i++)
        x->w[i] = 0;
    x->w[0] = (unsigned short)v;
    x->w[1] = (unsigned short)(v >> 16);
}

static int bn_zero(const bn *x)
{
    for (int i = 0; i < NL; i++)
        if (x->w[i])
            return 0;
    return 1;
}

/* x = x * m + k, m and k below 65536 */
static void bn_mul(bn *x, unsigned m, unsigned k)
{
    for (int i = 0; i < NL; i++) {
        k += x->w[i] * m;
        x->w[i] = (unsigned short)k;
        k >>= 16;
    }
}

/* x = x / d, d below 65536; the remainder */
static unsigned bn_divs(bn *x, unsigned d)
{
    unsigned r = 0;
    for (int i = NL - 1; i >= 0; i--) {
        r = r << 16 | x->w[i];
        x->w[i] = (unsigned short)(r / d);
        r %= d;
    }
    return r;
}

static int bn_cmp(const bn *x, const bn *y)
{
    for (int i = NL - 1; i >= 0; i--)
        if (x->w[i] != y->w[i])
            return x->w[i] < y->w[i] ? -1 : 1;
    return 0;
}

static void bn_add(bn *x, const bn *y)
{
    unsigned c = 0;
    for (int i = 0; i < NL; i++) {
        c += (unsigned)x->w[i] + y->w[i];
        x->w[i] = (unsigned short)c;
        c >>= 16;
    }
}

/* x -= y, x >= y */
static void bn_sub(bn *x, const bn *y)
{
    unsigned b = 0;
    for (int i = 0; i < NL; i++) {
        unsigned t = (unsigned)x->w[i] - y->w[i] - b;
        x->w[i] = (unsigned short)t;
        b = t >> 31;
    }
}

/* r = x * y, both below 2**128 */
static void bn_mulb(bn *r, const bn *x, const bn *y)
{
    bn_set(r, 0);
    for (int i = 0; i < NL / 2; i++) {
        unsigned c = 0;
        for (int j = 0; j < NL / 2; j++) {
            c += (unsigned)r->w[i + j] + (unsigned)x->w[i] * y->w[j];
            r->w[i + j] = (unsigned short)c;
            c >>= 16;
        }
        r->w[i + NL / 2] = (unsigned short)c;
    }
}

static int bn_bit(const bn *x, int i)
{
    return (x->w[i / 16] >> (i % 16)) & 1;
}

static int bn_top(const bn *x)
{
    int i = NL * 16 - 1;
    while (i >= 0 && !bn_bit(x, i))
        i--;
    return i;
}

/* q = n / d, truncated; d nonzero */
static void bn_div(bn *q, const bn *n, const bn *d)
{
    bn r;
    bn_set(&r, 0);
    bn_set(q, 0);
    for (int i = bn_top(n); i >= 0; i--) {
        bn_mul(&r, 2, (unsigned)bn_bit(n, i));
        if (bn_cmp(&r, d) >= 0) {
            bn_sub(&r, d);
            q->w[i / 16] |= (unsigned short)(1u << (i % 16));
        }
    }
}

/* ---- with a sign ---- */

/* x += y */
static void s_add(sbn *x, const sbn *y)
{
    if (x->neg == y->neg) {
        bn_add(&x->m, &y->m);
    } else if (bn_cmp(&x->m, &y->m) >= 0) {
        bn_sub(&x->m, &y->m);
    } else {
        bn t = y->m;
        bn_sub(&t, &x->m);
        x->m = t;
        x->neg = y->neg;
    }
}

/* r = x * y, negated if neg */
static void s_mul(sbn *r, const sbn *x, const sbn *y, int neg)
{
    bn_mulb(&r->m, &x->m, &y->m);
    r->neg = (x->neg != y->neg) != neg;
}

/* ---- the parts as the generated code holds them ---- */

static void from_dec(sbn *v, const unsigned char *p)
{
    bn_set(&v->m, 0);
    for (int i = 0; i < 8; i++) {
        bn_mul(&v->m, 10, p[i] >> 4);
        if (i < 7)
            bn_mul(&v->m, 10, p[i] & 15);
    }
    v->neg = (p[7] & 15) == 0xD || (p[7] & 15) == 0xB;
}

static void from_bin(sbn *v, int x)
{
    v->neg = x < 0;
    bn_set(&v->m, x < 0 ? 0u - (unsigned)x : (unsigned)x);
}

/* the low 2n-1 digits of v into n bytes, sign C or D */
static void to_dec(unsigned char *p, int n, const sbn *v)
{
    bn m = v->m;
    for (int i = n - 1; i >= 0; i--) {
        unsigned lo = i == n - 1 ? (v->neg && !bn_zero(&v->m) ? 0xD : 0xC) : bn_divs(&m, 10);
        p[i] = (unsigned char)(bn_divs(&m, 10) << 4 | lo);
    }
}

/* the low 31 bits of v, with its sign */
static int to_bin(const sbn *v)
{
    int x = (int)((unsigned)v->m.w[0] | ((unsigned)v->m.w[1] & 0x7FFF) << 16);
    return v->neg ? -x : x;
}

/* v * base**k, truncated toward zero for k < 0 */
static void rescale(bn *v, unsigned base, int k)
{
    for (; k > 0; k--)
        bn_mul(v, base, 0);
    for (; k < 0; k++)
        bn_divs(v, base);
}

/* FIXEDOVERFLOW if v has more than p digits (bits) */
static void check(const sbn *v, unsigned base, int p, int df)
{
    bn lim;
    bn_set(&lim, 1);
    rescale(&lim, base, p);
    if (bn_cmp(&v->m, &lim) >= 0 && (df & DF_FOFL))
        pl1_signal(C_FIXEDOVERFLOW, NULL, 310);
}

static int sc(int df, int k)
{
    return (signed char)(df >> (8 * k));
}

/* (a+bI) * or / (c+dI), scales q1 and q2, into re and im at scale q */
static void cx_md(const sbn *a, const sbn *b, const sbn *c, const sbn *d, int div,
                  unsigned base, int df, sbn *re, sbn *im)
{
    int q1 = sc(df, 0), q2 = sc(df, 1), q = sc(df, 2), k;
    sbn t, den;
    s_mul(re, a, c, 0);
    s_mul(&t, b, d, !div);
    s_add(re, &t);                    /* a*c-b*d, or a*c+b*d */
    s_mul(im, b, c, 0);
    s_mul(&t, a, d, div);
    s_add(im, &t);                    /* b*c+a*d, or b*c-a*d */
    if (!div) {
        rescale(&re->m, base, q - q1 - q2);
        rescale(&im->m, base, q - q1 - q2);
        return;
    }
    /* each divided by c*c+d*d, at scale q: times base**(q+q2-q1) */
    s_mul(&den, c, c, 0);
    s_mul(&t, d, d, 0);
    s_add(&den, &t);
    if (bn_zero(&den.m)) {
        if (df & DF_ZDIV) {
            pl1_signal(C_ZERODIVIDE, NULL, 320);
            pl1_signal(C_ZERODIVIDE, NULL, 320);
        }
        re->neg = im->neg = 0;
        bn_set(&re->m, 0);
        bn_set(&im->m, 0);
        return;
    }
    k = q + q2 - q1;
    if (k >= 0) {
        rescale(&re->m, base, k);
        rescale(&im->m, base, k);
    } else {
        rescale(&den.m, base, -k);
    }
    t.m = re->m;
    bn_div(&re->m, &t.m, &den.m);
    t.m = im->m;
    bn_div(&im->m, &t.m, &den.m);
}

static void dec_md(const unsigned char *a, const unsigned char *b, const unsigned char *c,
                   const unsigned char *d, unsigned char *r, int df, int div)
{
    sbn x[4], re, im;
    int p = (df >> 24) & 31, n = p / 2 + 1;
    from_dec(&x[0], a);
    from_dec(&x[1], b);
    from_dec(&x[2], c);
    from_dec(&x[3], d);
    cx_md(&x[0], &x[1], &x[2], &x[3], div, 10, df, &re, &im);
    check(&re, 10, p, df);
    check(&im, 10, p, df);
    to_dec(r, n, &re);
    to_dec(r + n, n, &im);
}

static void bin_md(int a, int b, int c, int d, void *r, int df, int div)
{
    sbn x[4], re, im;
    int p = (df >> 24) & 31;
    from_bin(&x[0], a);
    from_bin(&x[1], b);
    from_bin(&x[2], c);
    from_bin(&x[3], d);
    cx_md(&x[0], &x[1], &x[2], &x[3], div, 2, df, &re, &im);
    check(&re, 2, p, df);
    check(&im, 2, p, df);
    if (p <= 15) {
        ((short *)r)[0] = (short)to_bin(&re);
        ((short *)r)[1] = (short)to_bin(&im);
    } else {
        ((int *)r)[0] = to_bin(&re);
        ((int *)r)[1] = to_bin(&im);
    }
}

void cxmd(const unsigned char *a, const unsigned char *b, const unsigned char *c,
          const unsigned char *d, unsigned char *r, int df)
{
    dec_md(a, b, c, d, r, df, 0);
}

void cxdd(const unsigned char *a, const unsigned char *b, const unsigned char *c,
          const unsigned char *d, unsigned char *r, int df)
{
    dec_md(a, b, c, d, r, df, 1);
}

void cxmb(int a, int b, int c, int d, void *r, int df)
{
    bin_md(a, b, c, d, r, df, 0);
}

void cxdb(int a, int b, int c, int d, void *r, int df)
{
    bin_md(a, b, c, d, r, df, 1);
}

/* ABS: floor(SQRT(a*a+b*b)), the scale kept, digit by digit in base 4 */
static void cx_abs(const sbn *a, const sbn *b, int base, int df, sbn *v)
{
    sbn s, t;
    bn bit, x;
    int k;
    s_mul(&s, a, a, 0);
    s_mul(&t, b, b, 0);
    s_add(&s, &t);
    bn_set(&v->m, 0);
    v->neg = 0;
    k = bn_top(&s.m);
    if (k >= 0) {
        bn_set(&bit, 1);
        for (k &= ~1; k > 0; k--)
            bn_mul(&bit, 2, 0);
        while (!bn_zero(&bit)) {
            x = v->m;
            bn_add(&x, &bit);
            bn_divs(&v->m, 2);
            if (bn_cmp(&s.m, &x) >= 0) {
                bn_sub(&s.m, &x);
                bn_add(&v->m, &bit);
            }
            bn_divs(&bit, 4);
        }
    }
    check(v, (unsigned)base, (df >> 24) & 31, df);
}

void cxad(const unsigned char *a, const unsigned char *b, unsigned char *r, int df)
{
    sbn x, y, v;
    from_dec(&x, a);
    from_dec(&y, b);
    cx_abs(&x, &y, 10, df, &v);
    to_dec(r, 8, &v);
}

void cxab(int a, int b, int *r, int df)
{
    sbn x, y, v;
    from_bin(&x, a);
    from_bin(&y, b);
    cx_abs(&x, &y, 2, df, &v);
    *r = to_bin(&v);
}
