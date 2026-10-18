/* pl1flt.c -- conversions of System/370 floating point, as PL/I (F) does
 * them.
 *
 * PL/I (F)'s library converts between decimal and hexadecimal floating
 * point with long floating-point arithmetic and a table of powers of ten,
 * and truncates at every step; the compiler does the same for constants
 * (measured, internals/research/pli/tier7-measurement.md):
 *
 *  - decimal to float (IHEVPA): the decimal integer I, below 2**56 (one
 *    digit dropped if not), becomes a float exactly; then it is divided
 *    (or multiplied) by 10**(n mod 10) and by 10**10k, k = 1..7, from the
 *    table, until 10**n is used up.  The compiler first pads the digits of
 *    a constant to seventeen.
 *  - float to decimal (IHEVFA): the float is divided or multiplied by
 *    powers of ten from the same table until its characteristic is 78;
 *    its fraction is then the decimal integer, the powers used its scale.
 *  - MD and DD truncate their exact result; so does everything here.
 *
 * This file is compiled into the run time and into the compiler, so it
 * uses nothing but C99 and fixed-size arrays -- never a C double.
 *
 * A float is 4 (short) or 8 (long) bytes: sign bit, a characteristic of
 * 7 bits (exponent of 16, excess 64), and a fraction of 6 or 14 hex
 * digits, normalized (its first hex digit is not 0) unless it is zero.
 */
#include <string.h>

#include "pl1flt.h"

/* ---- big unsigned integers ---------------------------------------------- */

#define NW 40                    /* 1280 bits: 10**78 * 2**56 * 16**64 fit */

typedef struct { unsigned int w[NW]; } big;   /* little-endian words */

static void b_set(big *a, unsigned long long v)
{
    memset(a, 0, sizeof *a);
    a->w[0] = (unsigned int)v;
    a->w[1] = (unsigned int)(v >> 32);
}

static int b_zero(const big *a)
{
    for (int i = 0; i < NW; i++)
        if (a->w[i])
            return 0;
    return 1;
}

static void b_mul_small(big *a, unsigned int m)
{
    unsigned long long c = 0;
    for (int i = 0; i < NW; i++) {
        c += (unsigned long long)a->w[i] * m;
        a->w[i] = (unsigned int)c;
        c >>= 32;
    }
}

static void b_shl(big *a, int n)
{
    int ws = n / 32, bs = n % 32;
    for (int i = NW - 1; i >= 0; i--) {
        unsigned int v = i - ws >= 0 ? a->w[i - ws] << bs : 0;
        if (bs && i - ws - 1 >= 0)
            v |= a->w[i - ws - 1] >> (32 - bs);
        a->w[i] = v;
    }
}

static int b_cmp(const big *a, const big *b)
{
    for (int i = NW - 1; i >= 0; i--)
        if (a->w[i] != b->w[i])
            return a->w[i] < b->w[i] ? -1 : 1;
    return 0;
}

static void b_sub(big *a, const big *b)          /* a -= b, a >= b */
{
    long long c = 0;
    for (int i = 0; i < NW; i++) {
        c += (long long)a->w[i] - b->w[i];
        a->w[i] = (unsigned int)c;
        c = c < 0 ? -1 : 0;
    }
}


/* ---- an exact ratio as a float, truncated --------------------------------- */

/* num/den (den > 0) as a float of nbits fraction bits (24 or 56), towards
 * zero.  Returns 0, or 1 for overflow, -1 for underflow (out set to 0). */
static int from_ratio(big *num, big *den, int neg, int longf, unsigned char *out)
{
    int nbits = longf ? 56 : 24, c = 64;
    unsigned long long f = 0;
    big t;

    memset(out, 0, longf ? 8 : 4);
    if (b_zero(num))
        return 0;
    /* the characteristic: 16**(c-65) <= num/den < 16**(c-64); first
     * raise c until num/den < 16**(c-64) */
    for (;;) {
        t = *den;
        b_shl(&t, 4 * (c - 64));
        if (b_cmp(num, &t) < 0)
            break;
        if (++c > 127)
            return 1;
    }
    /* now num/den < 16**(c-64); lower c while num/den < 16**(c-65) */
    for (;;) {
        big a = *num, b = *den;
        int k = c - 65;
        if (k >= 0)
            b_shl(&b, 4 * k);
        else
            b_shl(&a, -4 * k);
        if (b_cmp(&a, &b) >= 0)
            break;
        c--;
        if (c < -20)
            return -1;
    }
    if (c > 127)
        return 1;
    if (c < 0)
        return -1;
    /* fraction: floor(num/den * 2**nbits / 16**(c-64)), bit by bit */
    {
        big a = *num, b = *den;
        int sh = nbits - 4 * (c - 64);    /* num/den * 2**sh */
        if (sh >= 0)
            b_shl(&a, sh);
        else
            b_shl(&b, -sh);
        /* quotient < 2**nbits: find it bit by bit */
        for (int i = nbits - 1; i >= 0; i--) {
            big s = b;
            b_shl(&s, i);
            if (b_cmp(&a, &s) >= 0) {
                b_sub(&a, &s);
                f |= 1ULL << i;
            }
        }
    }
    out[0] = (unsigned char)((neg ? 0x80 : 0) | c);
    for (int i = 1; i < (longf ? 8 : 4); i++)
        out[i] = (unsigned char)(f >> (8 * ((longf ? 7 : 3) - i)));
    return 0;
}

/* ---- long floating-point arithmetic, truncating ------------------------- */

/* A long float as fraction (56 bits) and characteristic; sign apart. */
struct hf { unsigned long long f; int c; };

static struct hf hf_of(const unsigned char *b)
{
    struct hf x;
    x.c = b[0] & 0x7F;
    x.f = 0;
    for (int i = 1; i < 8; i++)
        x.f = (x.f << 8) | b[i];
    return x;
}

static struct hf hf_ratio(big *num, big *den)
{
    unsigned char b[8];
    struct hf x;
    if (from_ratio(num, den, 0, 1, b) != 0)
        memset(b, 0, 8);         /* out of range: never with the table */
    x = hf_of(b);
    return x;
}

/* f * 2**(4e) as a big number over den (both set) */
static void hf_parts(unsigned long long f, int e, big *num, big *den)
{
    b_set(num, f);
    b_set(den, 1);
    if (e > 0)
        b_shl(num, 4 * e);
    else if (e < 0)
        b_shl(den, -4 * e);
}

/* MD: the exact product, truncated to 14 hex digits */
static struct hf hf_mul(struct hf a, struct hf b)
{
    big num, den, t;
    if (!a.f || !b.f)
        return a.f ? b : a;
    hf_parts(a.f, a.c + b.c - 128, &num, &den);
    t = num;                     /* num * b.f, in 32-bit halves */
    b_mul_small(&num, (unsigned int)b.f);
    b_mul_small(&t, (unsigned int)(b.f >> 32));
    b_shl(&t, 32);
    {
        unsigned long long c = 0;
        for (int i = 0; i < NW; i++) {
            c += (unsigned long long)num.w[i] + t.w[i];
            num.w[i] = (unsigned int)c;
            c >>= 32;
        }
    }
    b_shl(&den, 112);
    return hf_ratio(&num, &den);
}

/* DD: the exact quotient, truncated */
static struct hf hf_div(struct hf a, struct hf b)
{
    big num, den;
    if (!a.f)
        return a;
    hf_parts(a.f, a.c - b.c, &num, &den);
    {
        big d;
        b_set(&d, b.f);
        /* num / (den * b.f) */
        for (int i = 0; i < NW; i++)
            if (den.w[i]) {
                /* den is a power of two: shift d by its exponent */
                int sh = 32 * i;
                unsigned int w = den.w[i];
                while (w > 1) { w >>= 1; sh++; }
                b_shl(&d, sh);
                break;
            }
        return hf_ratio(&num, &d);
    }
}

/* Powers of ten as long floats, rounded to the nearest: 10**1 to 10**9,
 * and 10**10 to 10**70 in steps of ten. */
static const unsigned char pow1[10][8] = {
    {0}, {0x41,0xA0}, {0x42,0x64}, {0x43,0x3E,0x80}, {0x44,0x27,0x10},
    {0x45,0x18,0x6A}, {0x45,0xF4,0x24}, {0x46,0x98,0x96,0x80},
    {0x47,0x5F,0x5E,0x10}, {0x48,0x3B,0x9A,0xCA},
};
static const unsigned char pow10[8][8] = {
    {0}, {0x49,0x25,0x40,0xBE,0x40,0x00,0x00,0x00},
    {0x51,0x56,0xBC,0x75,0xE2,0xD6,0x31,0x00},
    {0x59,0xC9,0xF2,0xC9,0xCD,0x04,0x67,0x4F},
    {0x62,0x1D,0x63,0x29,0xF1,0xC3,0x5C,0xA5},
    {0x6A,0x44,0x6C,0x3B,0x15,0xF9,0x92,0x67},
    {0x72,0x9F,0x4F,0x27,0x26,0x17,0x9A,0x22},
    {0x7B,0x17,0x2E,0xBA,0xD6,0xDD,0xC7,0x3D},
};

static struct hf scale(struct hf x, const unsigned char *p, int divide)
{
    struct hf t = hf_of(p);
    return divide ? hf_div(x, t) : hf_mul(x, t);
}

/* ---- decimal to float (IHEVPA) ------------------------------------------- */

/* I * 10**-q as a long float in out; returns 0, 1 for overflow (an
 * underflow gives 0, as with the program mask PL/I (F) sets). */
static int ihevpa(unsigned long long I, int q, int neg, unsigned char *out)
{
    struct hf x;
    big num, den;
    int n, k;

    memset(out, 0, 8);
    if (I == 0)
        return 0;
    if (q < -59) {
        while (q < -59) {
            if (I >= 10000000000000000ULL)
                return 1;
            I *= 10;
            q++;
        }
        if (I >= (1ULL << 56))
            return 1;
    } else if (q > -59) {
        if (I >= (1ULL << 56)) {
            I /= 10;
            q--;
        }
    } else if (I >= (1ULL << 56)) {
        return 1;
    }
    hf_parts(I, 0, &num, &den);
    x = hf_ratio(&num, &den);
    n = q < 0 ? -q : q;
    if (n % 10)
        x = scale(x, pow1[n % 10], q > 0);
    for (n /= 10; n > 0; n -= k) {
        k = n > 7 ? 7 : n;
        x = scale(x, pow10[k], q > 0);
        if (!x.f)
            return 0;            /* underflow */
    }
    if (x.c > 127 || x.c < 0)
        return x.c > 127 ? 1 : 0;
    out[0] = (unsigned char)((neg ? 0x80 : 0) | x.c);
    for (int i = 1; i < 8; i++)
        out[i] = (unsigned char)(x.f >> (8 * (7 - i)));
    return 0;
}

static void to_short(unsigned char *b, int longf)
{
    if (!longf && (b[1] | b[2] | b[3]) == 0)
        memset(b, 0, 4);         /* a long value that is 0 in short form */
}

int pl1f_from_decimal(const char *d, int nd, int e10, int neg, int longf, unsigned char *out)
{
    unsigned char b[8];
    unsigned long long I = 0;
    int r;
    while (nd > 0 && d[0] == '0') {
        d++;
        nd--;
    }
    while (nd > 17) {            /* the rightmost digits do not count */
        nd--;
        e10++;
    }
    for (int i = 0; i < nd; i++)
        I = I * 10 + (unsigned long long)(d[i] - '0');
    r = ihevpa(I, -e10, neg, b);
    memcpy(out, b, longf ? 8 : 4);
    to_short(out, longf);
    return r;
}

int pl1f_constant(const char *d, int nd, int e10, int neg, int longf, unsigned char *out)
{
    /* the compiler: the digits padded to seventeen */
    char p[17];
    while (nd > 0 && d[0] == '0') {
        d++;
        nd--;
    }
    if (nd <= 0 || nd > 17) {
        memset(out, 0, longf ? 8 : 4);
        return nd <= 0 ? 0 : 2;
    }
    memcpy(p, d, (size_t)nd);
    while (nd < 17) {
        p[nd++] = '0';
        e10--;
    }
    return pl1f_from_decimal(p, nd, e10, neg, longf, out);
}

int pl1f_from_binary(long long v, int q, int longf, unsigned char *out)
{
    big num, den;
    int neg = v < 0;
    unsigned long long u = neg ? (unsigned long long)(-(v + 1)) + 1 : (unsigned long long)v;
    b_set(&num, u);
    b_set(&den, 1);
    if (q > 0)
        b_shl(&den, q);
    else if (q < 0)
        b_shl(&num, -q);
    return from_ratio(&num, &den, neg, longf, out);
}

/* ---- float to decimal (IHEVFA) ------------------------------------------- */

/* |value| = I * 10**-s; I has 16 or 17 digits unless the value is 0. */
static unsigned long long ihevfa(const unsigned char *in, int longf, int *s)
{
    unsigned char b[8];
    struct hf x;
    memset(b, 0, 8);
    memcpy(b, in, longf ? 8 : 4);
    b[0] &= 0x7F;
    x = hf_of(b);
    *s = 0;
    if (!x.f)
        return 0;
    for (;;) {
        int e = x.c - 78, n;
        if (e == 0)
            break;
        n = ((e < 0 ? -e : e) * 19728 + 8192) >> 14;
        if (n > 78)
            n = 78;
        *s += e > 0 ? -n : n;
        if (n % 10)
            x = scale(x, pow1[n % 10], e > 0);
        if (n / 10)
            x = scale(x, pow10[n / 10], e > 0);
    }
    return x.f;
}

static int udigits(unsigned long long v, char *d)
{
    char t[24];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    for (int i = 0; i < n; i++)
        d[i] = t[n - 1 - i];
    return n;
}

/* Round the digits d[0..n-1] up by one unit of the last; returns 1 when
 * they overflow into a new leading 1. */
static int round_up(char *d, int n)
{
    int i = n - 1;
    while (i >= 0 && d[i] == '9')
        d[i--] = '0';
    if (i >= 0) {
        d[i]++;
        return 0;
    }
    if (n > 0)
        d[0] = '1';
    return 1;
}

int pl1f_to_digits(const unsigned char *in, int longf, int n, int round, char *d, int *e10, int *neg)
{
    char id[24];
    int s, L;
    unsigned long long I = ihevfa(in, longf, &s);

    *neg = (in[0] & 0x80) != 0;
    if (!I) {
        memset(d, '0', (size_t)n);
        *e10 = 0;
        *neg = 0;
        return 0;
    }
    L = udigits(I, id);
    for (int i = 0; i < n; i++)
        d[i] = i < L ? id[i] : '0';
    *e10 = L - 1 - s;
    if (round && n < L && id[n] >= '5' && round_up(d, n))
        (*e10)++;
    return 1;
}

int pl1f_to_fixed(const unsigned char *in, int longf, int q, int round, char *d, int maxd, int *neg)
{
    char id[24], t[120];
    int s, L, nd;
    unsigned long long I = ihevfa(in, longf, &s);

    *neg = (in[0] & 0x80) != 0;
    if (!I) {
        *neg = 0;
        d[0] = '0';
        return 1;
    }
    L = udigits(I, id);
    /* I * 10**(q-s) */
    if (q - s >= 0) {
        int z = q - s > 90 ? 90 : q - s;
        memcpy(t, id, (size_t)L);
        memset(t + L, '0', (size_t)z);
        nd = L + z;
    } else {
        int drop = s - q;
        nd = L - drop;
        if (nd <= 0) {
            /* below one unit: 0, or 1 when rounding takes it there */
            t[0] = round && nd == 0 && id[0] >= '5' ? '1' : '0';
            nd = 1;
        } else {
            memcpy(t, id, (size_t)nd);
            if (round && id[nd] >= '5' && round_up(t, nd)) {
                memmove(t + 1, t, (size_t)nd);
                t[0] = '1';
                nd++;
            }
        }
    }
    if (nd > maxd) {
        memcpy(d, t + nd - maxd, (size_t)maxd);
        return maxd;
    }
    memcpy(d, t, (size_t)nd);
    return nd;
}

long long pl1f_to_binary(const unsigned char *in, int longf, int q)
{
    int nbits = longf ? 56 : 24, c = in[0] & 0x7F, sh;
    unsigned long long f = 0, v;
    for (int i = 1; i < (longf ? 8 : 4); i++)
        f = (f << 8) | in[i];
    /* value * 2**q = f * 2**(4(c-64) + q - nbits) */
    sh = 4 * (c - 64) + q - nbits;
    if (sh <= -64)
        v = 0;
    else if (sh < 0)
        v = f >> -sh;
    else if (sh < 64)
        v = f << sh;             /* the low bits, as PL/I (F) keeps them */
    else
        v = 0;
    return (in[0] & 0x80) ? -(long long)v : (long long)v;
}

/* ---- x ** n and the integer part ------------------------------------------ */

static void hf_store(struct hf x, int neg, int longf, unsigned char *out)
{
    unsigned char b[8];
    memset(b, 0, 8);
    if (x.f && x.c >= 0 && x.c <= 127) {
        b[0] = (unsigned char)((neg ? 0x80 : 0) | x.c);
        for (int i = 1; i < 8; i++)
            b[i] = (unsigned char)(x.f >> (8 * (7 - i)));
    }
    memcpy(out, b, longf ? 8 : 4);
}

/* a short operation keeps the first word of its result */
static struct hf hf_short(struct hf x)
{
    x.f &= 0xFFFFFF00000000ULL;
    return x;
}

int pl1f_power(const unsigned char *in, int longf, int n, unsigned char *out)
{
    unsigned char b[8];
    struct hf x, r, one;
    unsigned int m, bits;
    int neg;

    memset(b, 0, 8);
    memcpy(b, in, longf ? 8 : 4);
    neg = (b[0] & 0x80) != 0;
    b[0] &= 0x7F;
    x = hf_of(b);
    one.f = 1ULL << 52;          /* 1.0: X'4110000000000000' */
    one.c = 65;
    if (!x.f) {
        hf_store(x, 0, longf, out);
        return n > 0 ? 0 : 1;    /* 0 ** n, n <= 0: an error */
    }
    if (n == 0) {
        hf_store(one, 0, longf, out);
        return 0;
    }
    m = (unsigned int)(n < 0 ? -n : n);
    /* the bits of |n| from the second highest down: square, and
     * multiply by x where the bit is 1, as PL/I (F) computes x**n */
    for (bits = 1; (m >> bits) != 0; bits++)
        ;
    r = x;
    for (int i = (int)bits - 2; i >= 0; i--) {
        r = hf_mul(r, r);
        if (!longf)
            r = hf_short(r);
        if ((m >> i) & 1) {
            r = hf_mul(r, x);
            if (!longf)
                r = hf_short(r);
        }
    }
    if (n < 0) {
        r = hf_div(one, r);
        if (!longf)
            r = hf_short(r);
    }
    /* the sign: x**n is negative for a negative x and an odd n */
    hf_store(r, neg && (m & 1), longf, out);
    return 0;
}

void pl1f_integer(const unsigned char *in, int longf, int mode, unsigned char *out)
{
    unsigned char b[8];
    int c = in[0] & 0x7F, neg = (in[0] & 0x80) != 0;
    long long v;

    memset(b, 0, 8);
    memcpy(b, in, longf ? 8 : 4);
    if ((b[1] | b[2] | b[3] | b[4] | b[5] | b[6] | b[7]) == 0 || c >= 64 + (longf ? 14 : 6)) {
        /* zero (a negative one too) or no fraction digits: unchanged */
        memcpy(out, in, longf ? 8 : 4);
        return;
    }
    v = pl1f_to_binary(in, longf, 0);        /* towards zero */
    pl1f_from_binary(v, 0, longf, b);
    if (memcmp(b, in, longf ? 8 : 4) != 0) {
        /* there was a fraction: FLOOR of a negative, CEIL of a positive step */
        if (mode == 0 && neg)
            v--;
        else if (mode == 1 && !neg)
            v++;
        pl1f_from_binary(v, 0, longf, b);
    }
    memcpy(out, b, longf ? 8 : 4);
}
