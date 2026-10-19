/* pl1rt.c -- conversions and string operations of PL1C/370's run time.
 *
 * Input and output are in pl1io.c.  The conversion rules are measured
 * against PL/I (F) V5.5: internals/research/pli/putlist-measurement.md and
 * tier1-measurement.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pl1rt.h"

#include "pl1int.h"
#include "pl1flt.h"

/* d holds nd decimal digits of a number with q of them after the point.
 * Right-justify it in a field of p+3 characters at field; return the
 * width. */
int pl1i_format_number(char *field, const char *d, int nd, int p, int q, int neg)
{
    char text[48];
    int  t = 0, w = p + 3, i, ip = nd - q;

    if (neg)
        text[t++] = '-';
    i = 0;
    while (i < ip - 1 && d[i] == '0')
        i++;                        /* leading zeros of the integer part */
    if (ip <= 0)
        text[t++] = '0';
    for (; i < ip; i++)
        text[t++] = d[i];
    if (q > 0) {
        text[t++] = '.';
        for (i = ip; i < 0; i++)
            text[t++] = '0';
        for (i = ip < 0 ? 0 : ip; i < nd; i++)
            text[t++] = d[i];
    }
    if (t > w)
        w = t;
    memset(field, ' ', (size_t)w);
    memcpy(field + w - t, text, (size_t)t);
    return w;
}

int pl1i_ceil_div332(int x) { return (x * 100 + 331) / 332; }

/* Decimal digits of the scaled binary n/2**q with qd fraction digits,
 * truncated: the integer part, then the fraction digit by digit. */
int pl1i_scaled_digits(char *d, int n, int q, int qd, int *neg)
{
    unsigned int u = n < 0 ? 0u - (unsigned int)n : (unsigned int)n;
    unsigned int ip = q >= 32 ? 0 : u >> q;
    unsigned long long frac = q == 0 ? 0 : (unsigned long long)(u & ((1u << q) - 1));
    char rev[16];
    int nd = 0, k, zero;

    do {
        rev[nd++] = (char)('0' + ip % 10);
        ip /= 10;
    } while (ip != 0);
    for (k = 0; k < nd; k++)
        d[k] = rev[nd - 1 - k];
    for (k = 0; k < qd; k++) {
        frac *= 10;
        d[nd++] = (char)('0' + (int)(frac >> q));
        frac &= (1ull << q) - 1;
    }
    zero = 1;
    for (k = 0; k < nd; k++)
        if (d[k] != '0')
            zero = 0;
    *neg = n < 0 && !zero;
    return nd;
}



int pl1_format_fixbin(char *dst, int n, int p, int q)
{
    char d[40];
    int neg, qd = pl1i_ceil_div332(q);
    int nd = pl1i_scaled_digits(d, n, q, qd, &neg);

    return pl1i_format_number(dst, d, nd, 1 + pl1i_ceil_div332(p), qd, neg);
}

void pl1_bin_to_dec(int n, int q, int qd, unsigned char *dst)
{
    char d[40];
    int neg, nd = pl1i_scaled_digits(d, n, q, qd, &neg), i, k;

    /* 15 digits and the sign in 8 bytes, the digits right-aligned. */
    memset(dst, 0, 8);
    dst[7] = neg ? 0x0D : 0x0C;
    for (i = nd - 1, k = 14; i >= 0 && k >= 0; i--, k--) {
        int v = d[i] - '0';
        /* nibble k of the 16 (the 16th is the sign) is the high half of
         * byte k/2 when k is even, the low half when it is odd */
        if ((k & 1) == 0)
            dst[k / 2] |= (unsigned char)(v << 4);
        else
            dst[k / 2] |= (unsigned char)v;
    }
}

int pl1i_dec_digits(char *d, const unsigned char *packed, int p, int *neg)
{
    int nbytes = p / 2 + 1, nd = 0, i, zero = 1;

    /* 2*nbytes-1 digit nibbles before the sign; an even p has one
     * padding nibble in front. */
    for (i = 0; i < 2 * nbytes - 1; i++) {
        int nib = (i % 2 == 0) ? packed[i / 2] >> 4 : packed[i / 2] & 0x0F;
        if (i == 0 && p % 2 == 0)
            continue;
        d[nd++] = (char)('0' + nib);
        if (nib != 0)
            zero = 0;
    }
    i = packed[nbytes - 1] & 0x0F;
    *neg = !zero && (i == 0x0D || i == 0x0B);
    return nd;
}

int pl1_format_fixdec(char *dst, const unsigned char *packed, int p, int q)
{
    char d[32];
    int neg, nd = pl1i_dec_digits(d, packed, p, &neg);

    return pl1i_format_number(dst, d, nd, p, q, neg);
}


int pl1_concat(char *dst, const char *a, int al, const char *b, int bl)
{
    memmove(dst, a, (size_t)al);
    memmove(dst + al, b, (size_t)bl);
    return al + bl;
}

int pl1_index(const char *s, int sl, const char *t, int tl)
{
    if (tl == 0 || tl > sl)
        return 0;
    for (int i = 0; i + tl <= sl; i++)
        if (memcmp(s + i, t, (size_t)tl) == 0)
            return i + 1;
    return 0;
}


void pl1_assign_char(char *dst, int dlen, const char *src, int slen)
{
    int n = slen < dlen ? slen : dlen;
    if (slen > dlen)
        pl1i_strsize();                  /* ANSI's STRINGSIZE, cut after it */
    memmove(dst, src, (size_t)n);
    if (n < dlen)
        memset(dst + n, ' ', (size_t)(dlen - n));
}

void pl1_assign_varchar(void *dst, int maxlen, const char *src, int slen)
{
    unsigned char *b = dst;
    int n = slen < maxlen ? slen : maxlen;
    if (slen > maxlen)
        pl1i_strsize();
    memmove(b + 2, src, (size_t)n);
    b[0] = (unsigned char)(n >> 8);
    b[1] = (unsigned char)n;
}

int pl1_compare_char(const char *a, int alen, const char *b, int blen)
{
    int n = alen > blen ? alen : blen;

    for (int i = 0; i < n; i++) {
        unsigned char x = (unsigned char)(i < alen ? a[i] : ' ');
        unsigned char y = (unsigned char)(i < blen ? b[i] : ' ');
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

/* PL/I (F) converts a string to a number by its digits, sign and point,
 * truncating to the target's scale; anything else raises CONVERSION
 * (ONCODE 612; IHE604I with no on-unit).  A blank string is zero. */
int (*pl1x_cxsplit)(const char *, int, int *, int *, int *, int *);

/* A string holding a complex constant, in a program with COMPLEX data:
 * 1 if s[0..len) is one, its real part in *rs, *rn (rn 0: 0); 0 if it
 * is no complex constant; otherwise CONVERSION at the character that is
 * wrong, and -1 to try again after the on-unit, -2 if it is disabled */
int pl1i_cpx_real(char *s, int len, int code, int *rs, int *rn)
{
    int is, in, k;
    if (!pl1x_cxsplit)
        return 0;
    k = pl1x_cxsplit(s, len, rs, rn, &is, &in);
    if (k < 0)
        return in ? 1 : 0;
    {
        /* only what looks complex: an I at the end, or a central sign */
        int b = len, c = 0;
        while (b > 0 && s[b - 1] == ' ')
            b--;
        for (int i = 1; i < b; i++)
            if ((s[i] == '+' || s[i] == '-') && s[i - 1] != 'E' && s[i - 1] != ' ')
                c = 1;
        if (!(b > 0 && s[b - 1] == 'I') && !c)
            return 0;
    }
    return pl1i_conversion(code, s, len, k) ? -1 : -2;
}

void pl1_char_to_dec(char *s, int len, int q, unsigned char *dst)
{
    char d[40];
    int nd, sq, point, neg, k, zero = 1;

again:
    {
        int rs, rn, c = pl1i_cpx_real(s, len, 612, &rs, &rn);
        if (c == -1)
            goto again;
        if (c == 1) {
            pl1_char_to_dec(rn ? s + rs : (char *)"0", rn ? rn : 1, q, dst);
            return;
        }
        if (c == -2) {
            memset(dst, 0, 8);
            dst[7] = 0x0C;
            return;
        }
    }
    nd = sq = point = neg = 0;
    for (int i = 0; i < len; i++) {
        char c = s[i];
        if (c == ' ')
            continue;
        if (c == '-' && nd == 0 && !point) { neg = 1; continue; }
        if (c == '+' && nd == 0 && !point) continue;
        if (c == '.' && !point) { point = 1; continue; }
        if (c >= '0' && c <= '9') {
            if (nd < 39) {
                d[nd++] = c;
                if (point) sq++;
            }
            continue;
        }
        /* CONVERSION, ONCODE 612 (JOB00138); the on-unit changed s */
        if (pl1i_conversion(612, s, len, i))
            goto again;
        memset(dst, 0, 8);                /* NOCONVERSION: the value undefined */
        dst[7] = 0x0C;
        return;
    }
    /* digits at scale q, truncated; the rightmost 15 */
    memset(dst, 0, 8);
    k = nd - sq + q;                      /* digits to keep */
    for (int i = k - 1, pos = 14; i >= 0 && pos >= 0; i--, pos--) {
        int v = i < nd ? d[i] - '0' : 0;
        if (v)
            zero = 0;
        if ((pos & 1) == 0)
            dst[pos / 2] |= (unsigned char)(v << 4);
        else
            dst[pos / 2] |= (unsigned char)v;
    }
    dst[7] |= (unsigned char)(neg && !zero ? 0x0D : 0x0C);
}


/* ---- storage --------------------------------------------------------- */

void *pl1_allocate(int size)
{
    void *p;
    /* no storage: ANSI's STORAGE condition, then again (7.2.5) */
    while (!(p = calloc(1, (size_t)(size > 0 ? size : 1))))
        pl1i_storage();
    return p;
}

void pl1_free(void *p)
{
    free(p);
}

/* ---- built-in functions ---------------------------------------------- */

/* VERIFY(s,c): the position of the first character of s not in c, 0 if
 * there is none (JOB00182: VERIFY('', 'A') is 0). */
int pl1_verify(const char *s, int sl, const char *c, int cl)
{
    for (int i = 0; i < sl; i++)
        if (!memchr(c, s[i], (size_t)cl))
            return i + 1;
    return 0;
}

/* TRANSLATE(s,r,p) in place: a character of s found in p (the first
 * occurrence counts) becomes the one at that position of r, blank beyond
 * it; pl < 0: p is the whole collating set X'00' to X'FF'. */
void pl1_translate(char *s, int sl, const char *r, int rl, const char *p, int pl)
{
    for (int i = 0; i < sl; i++) {
        int k;
        if (pl < 0) {
            k = (unsigned char)s[i];
        } else {
            const char *f = memchr(p, s[i], (size_t)pl);
            if (!f)
                continue;
            k = (int)(f - p);
        }
        s[i] = k < rl ? r[k] : ' ';
    }
}

/* REPEAT(s,n): s n+1 times. */
int pl1_repeat(char *dst, const char *s, int sl, int n)
{
    for (int i = 0; i <= n; i++)
        memcpy(dst + i * sl, s, (size_t)sl);
    return sl * (n + 1);
}

void pl1_fill(char *dst, int n, int byte)
{
    memset(dst, byte, (size_t)n);
}

/* ---- bit strings longer than a register, and VARYING ones ------------
 * Lengths in bits, the bytes most significant bit first; a result is
 * written in whole bytes, the bits after its length zero.  A shorter
 * operand counts as padded with zeros (JOB00336). */

static int bit_at(const unsigned char *a, int i)
{
    return (a[i / 8] >> (7 - i % 8)) & 1;
}

static void bit_set(unsigned char *d, int i, int v)
{
    if (v)
        d[i / 8] |= (unsigned char)(0x80 >> (i % 8));
    else
        d[i / 8] &= (unsigned char)~(0x80 >> (i % 8));
}

/* & (op 0) or | (op 1); returns the length, the longer one's */
int pl1_bit_op(unsigned char *d, int op, const unsigned char *a, int la, const unsigned char *b,
               int lb)
{
    int n = la > lb ? la : lb;
    memset(d, 0, (size_t)((n + 7) / 8));
    for (int i = 0; i < n; i++) {
        int x = i < la && bit_at(a, i), y = i < lb && bit_at(b, i);
        bit_set(d, i, op ? (x | y) : (x & y));
    }
    return n;
}

void pl1_bit_not(unsigned char *d, const unsigned char *a, int la)
{
    memset(d, 0, (size_t)((la + 7) / 8));
    for (int i = 0; i < la; i++)
        bit_set(d, i, !bit_at(a, i));
}

int pl1_bit_cat(unsigned char *d, const unsigned char *a, int la, const unsigned char *b, int lb)
{
    unsigned char *t = malloc((size_t)((la + lb + 7) / 8 + 1));
    if (!t) {
        fputs("PL1C/370: out of storage\n", stderr);
        exit(16);
    }
    memset(t, 0, (size_t)((la + lb + 7) / 8 + 1));
    for (int i = 0; i < la; i++)
        bit_set(t, i, bit_at(a, i));
    for (int i = 0; i < lb; i++)
        bit_set(t, la + i, bit_at(b, i));
    memcpy(d, t, (size_t)((la + lb + 7) / 8));
    free(t);
    return la + lb;
}

/* SUBSTR(a, i [, j]): j < 0 for "to the end"; positions outside the
 * string are not checked (STRINGRANGE disabled), they are cut off */
int pl1_bit_sub(unsigned char *d, const unsigned char *a, int la, int i, int j)
{
    int n;
    if (j < 0)
        j = la - i + 1;
    if (i < 1) {
        j += i - 1;
        i = 1;
    }
    if (i - 1 + j > la)
        j = la - i + 1;
    n = j < 0 ? 0 : j;
    {
        unsigned char *t = malloc((size_t)((n + 7) / 8 + 1));
        if (!t) {
            fputs("PL1C/370: out of storage\n", stderr);
            exit(16);
        }
        memset(t, 0, (size_t)((n + 7) / 8 + 1));
        for (int k = 0; k < n; k++)
            bit_set(t, k, bit_at(a, i - 1 + k));
        memcpy(d, t, (size_t)((n + 7) / 8 + (n == 0)));
        free(t);
    }
    return n;
}

int pl1_bit_cmp(const unsigned char *a, int la, const unsigned char *b, int lb)
{
    int n = la > lb ? la : lb;
    for (int i = 0; i < n; i++) {
        int x = i < la && bit_at(a, i), y = i < lb && bit_at(b, i);
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

int pl1_bit_any(const unsigned char *a, int la)
{
    for (int i = 0; i < la; i++)
        if (bit_at(a, i))
            return 1;
    return 0;
}

/* Assignment: a fixed target is padded with zeros or truncated; a
 * VARYING one (dmax its greatest length) takes the source's length */
void pl1_bit_assign(unsigned char *d, int dmax, int varying, const unsigned char *s, int ls)
{
    int n = varying ? (ls < dmax ? ls : dmax) : dmax;
    unsigned char *t;
    if (ls > dmax)
        pl1i_strsize();                  /* ANSI's STRINGSIZE */
    t = malloc((size_t)((dmax + 7) / 8 + 1));
    if (!t) {
        fputs("PL1C/370: out of storage\n", stderr);
        exit(16);
    }
    memset(t, 0, (size_t)((dmax + 7) / 8 + 1));
    for (int i = 0; i < n && i < ls; i++)
        bit_set(t, i, bit_at(s, i));
    if (varying) {
        d[0] = (unsigned char)(n >> 8);
        d[1] = (unsigned char)n;
        memcpy(d + 2, t, (size_t)((dmax + 7) / 8));
    } else {
        memcpy(d, t, (size_t)((dmax + 7) / 8));
    }
    free(t);
}

/* BOOL(a, b, z): bit i is z's bit for the pair (a[i], b[i]) -- 00, 01, 10,
 * 11 the first to fourth bit of z; the shorter operand padded with zeros;
 * the length of the longer (Q5BYN) */
int pl1_bit_bool(unsigned char *d, const unsigned char *a, const unsigned char *b, int lab,
                 const unsigned char *z, int lz)
{
    int la = lab >> 16, lb = lab & 0xFFFF, n = la > lb ? la : lb;
    memset(d, 0, (size_t)((n + 7) / 8 + 1));
    for (int i = 0; i < n; i++) {
        int k = 2 * (i < la ? bit_at(a, i) : 0) + (i < lb ? bit_at(b, i) : 0);
        bit_set(d, i, k < lz ? bit_at(z, k) : 0);
    }
    return n;
}

void pl1_bit_get(unsigned char *d, const unsigned char *s, int off, int n)
{
    memset(d, 0, (size_t)((n + 7) / 8));
    for (int i = 0; i < n; i++)
        bit_set(d, i, bit_at(s, off + i));
}

void pl1_bit_put(unsigned char *d, int off, int n, const unsigned char *s, int ls)
{
    if (ls > n)
        pl1i_strsize();
    for (int i = 0; i < n; i++)
        bit_set(d, off + i, i < ls ? bit_at(s, i) : 0);
}

/* The characters '1' and '0' of the bits */
void pl1_bit_to_char(char *d, const unsigned char *a, int la)
{
    for (int i = 0; i < la; i++)
        d[i] = bit_at(a, i) ? '1' : '0';
}

/* The bits' value, the low 32 of it */
int pl1_bit_to_int(const unsigned char *a, int la)
{
    unsigned int v = 0;
    for (int i = 0; i < la; i++)
        v = (v << 1) | (unsigned int)bit_at(a, i);
    return (int)v;
}

/* SUBSTR(b, i [, j]) = s for a bit string b of bl bits: j < 0 to its
 * end; s padded with zeros or cut to that length (JOB00379) */
void pl1_bit_setsub(unsigned char *b, int bl, int i, int j, const unsigned char *s, int sl)
{
    if (j < 0)
        j = bl - i + 1;
    for (int k = 0; k < j; k++) {
        int pos = i - 1 + k;
        if (pos < 0 || pos >= bl)
            continue;
        bit_set(b, pos, k < sl && bit_at(s, k));
    }
}

/* The binary digits of |v|, its low n bits (BIT of a FIXED value) */
void pl1_int_to_bit(unsigned char *d, int v, int n)
{
    unsigned int u = v < 0 ? (unsigned int)-v : (unsigned int)v;
    memset(d, 0, (size_t)((n + 7) / 8));
    for (int i = 0; i < n; i++)
        bit_set(d, i, n - 1 - i < 32 && ((u >> (n - 1 - i)) & 1));
}

/* '1' and '0' characters as bits */
void pl1_char_to_bit(unsigned char *d, const char *s, int len)
{
    memset(d, 0, (size_t)((len + 7) / 8 + (len == 0)));
    for (int i = 0; i < len; i++)
        bit_set(d, i, s[i] == '1');
}
