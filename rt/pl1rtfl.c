/* pl1rtfl.c -- FLOAT conversions and PICTURE assignment of PL1C/370's run
 * time, out of pl1rt.c: a module links them (and with them pl1flt.c and
 * pl1pic.c) only when it uses them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pl1rt.h"
#include "pl1int.h"
#include "pl1flt.h"
#include "pl1pic.h"

/* ---- FLOAT ---------------------------------------------------------- */

/* A float as PUT LIST and the conversion to CHARACTER write it: p digits
 * of the exact value, truncated, as [-]d.ddddE+xx, right-justified in a
 * field of p+6 (JOB00156); FLOAT DEC(1) has no point. */
int pl1i_flt_field(char *field, const unsigned char *v, int longf, int p)
{
    char d[40], text[48];
    int e, neg, t = 0, w = p + 6;

    pl1f_to_digits(v, longf, p, 0, d, &e, &neg);
    if (neg)
        text[t++] = '-';
    text[t++] = d[0];
    if (p > 1)
        text[t++] = '.';
    for (int i = 1; i < p; i++)
        text[t++] = d[i];
    text[t++] = 'E';
    text[t++] = e < 0 ? '-' : '+';
    if (e < 0)
        e = -e;
    text[t++] = (char)('0' + e / 10);
    text[t++] = (char)('0' + e % 10);
    if (t > w)
        w = t;
    memset(field, ' ', (size_t)w);
    memcpy(field + w - t, text, (size_t)t);
    return w;
}

/* A number written with digits, sign, point and exponent (blanks around
 * it) as a float, the way the library converts it (IHEVPA).  implied:
 * the decimal places of an E or F field written without a point.  A
 * character that does not fit raises CONVERSION with code; so does an
 * empty field when blank_error is set (an E field, ONCODE 607, JOB00157);
 * otherwise it is 0. */
void pl1i_parse_float(char *t, int n, int implied, int code, int blank_error, int longf,
                      unsigned char *out)
{
    char d[24];
    int nd, q, point, neg, e, eneg, i, any;

again:
    nd = q = point = neg = e = eneg = any = 0;
    i = 0;
    while (i < n && t[i] == ' ')
        i++;
    if (i < n && (t[i] == '+' || t[i] == '-'))
        neg = t[i++] == '-';
    for (; i < n; i++) {
        char c = t[i];
        if (c >= '0' && c <= '9') {
            any = 1;
            if (nd == 0 && c == '0') {
                if (point)
                    q++;
                continue;                /* leading zeros */
            }
            if (nd == 16)
                break;                   /* PL/I (F) takes 16 digits */
            d[nd++] = c;
            if (point)
                q++;
        } else if (c == '.' && !point) {
            point = 1;
        } else {
            break;
        }
    }
    if (i < n && t[i] == 'E' && any) {
        int ed = 0;
        i++;
        if (i < n && (t[i] == '+' || t[i] == '-'))
            eneg = t[i++] == '-';
        while (i < n && t[i] >= '0' && t[i] <= '9' && ed < 3) {
            e = e * 10 + (t[i++] - '0');
            ed++;
        }
        if (ed == 0) {
            if (pl1i_conversion(code, t, n, i < n ? i : n - 1))
                goto again;
            memset(out, 0, longf ? 8 : 4);
            return;
        }
    }
    while (i < n && t[i] == ' ')
        i++;
    if (i < n || (!any && (blank_error || point || neg))) {
        if (pl1i_conversion(code, t, n, i < n ? i : 0))
            goto again;
        memset(out, 0, longf ? 8 : 4);
        return;
    }
    if (!point)
        q = implied;
    if (pl1f_from_decimal(d, nd, (eneg ? -e : e) - q, neg, longf, out) != 0) {
        /* beyond the range of a float: OVERFLOW is not supported */
        memset(out, 0, longf ? 8 : 4);
    }
}

void pl1_bin_to_float(int v, int q, int longf, unsigned char *dst)
{
    pl1f_from_binary(v, q, longf, dst);
}

void pl1_dec_to_float(const unsigned char *packed, int q, int longf, unsigned char *dst)
{
    char d[16];
    int neg, nd = pl1i_dec_digits(d, packed, 15, &neg);
    pl1f_from_decimal(d, nd, -q, neg, longf, dst);
}

void pl1_char_to_float(char *s, int len, int longf, unsigned char *dst)
{
    pl1i_parse_float(s, len, 0, 612, 0, longf, dst);
}

int pl1_float_to_bin(const unsigned char *src, int longf, int q)
{
    return (int)pl1f_to_binary(src, longf, q);
}

/* to FIXED DECIMAL(15,q) in 8 bytes, truncated, the rightmost 15 digits */
void pl1_float_to_dec(const unsigned char *src, int longf, int q, unsigned char *dst)
{
    char d[16];
    int neg, nd = pl1f_to_fixed(src, longf, q, 0, d, 15, &neg), zero = 1;
    memset(dst, 0, 8);
    for (int i = nd - 1, pos = 14; i >= 0; i--, pos--) {
        int v = d[i] - '0';
        if (v)
            zero = 0;
        if ((pos & 1) == 0)
            dst[pos / 2] |= (unsigned char)(v << 4);
        else
            dst[pos / 2] |= (unsigned char)v;
    }
    dst[7] |= (unsigned char)(neg && !zero ? 0x0D : 0x0C);
}

int pl1_float_to_char(char *dst, const unsigned char *src, int longf, int p)
{
    return pl1i_flt_field(dst, src, longf, p);
}

void pl1_float_power(const unsigned char *x, int longf, int n, unsigned char *dst)
{
    if (pl1f_power(x, longf, n, dst) != 0)
        pl1_signal(C_ERROR, NULL, 9);   /* 0 ** n, n <= 0: not measured */
}

void pl1_float_int(const unsigned char *x, int longf, int mode, unsigned char *dst)
{
    pl1f_integer(x, longf, mode, dst);
}

/* An assignment to a character-string picture (X, A, 9): the first n
 * characters of the source, blanks after it, each as its position
 * allows, else CONVERSION, ONCODE 618, the whole source as ONSOURCE
 * (JOB00290, JOB00292); then they are the target's.  A is a letter,
 * $ # @ or a blank, 9 a digit or a blank (Language Reference, section
 * D; $ measured in JOB00292). */
void pl1_pic_assign(char *t, const char *pic, int n, const char *src, int len)
{
    char small[256], *s = len <= (int)sizeof small ? small : malloc((size_t)len);
    if (!s) {
        fputs("PL1C/370: out of storage\n", stderr);
        exit(16);
    }
    memcpy(s, src, (size_t)len);
    for (int i = 0; i < n; i++) {
        char c = i < len ? s[i] : ' ';
        int ok = pic[i] == 'X' ||
                 (pic[i] == 'A' && (c == ' ' || c == '$' || c == '#' || c == '@' ||
                                    (c >= 'A' && c <= 'I') || (c >= 'J' && c <= 'R') ||
                                    (c >= 'S' && c <= 'Z'))) ||
                 (pic[i] == '9' && (c == ' ' || (c >= '0' && c <= '9')));
        if (!ok) {
            if (!pl1i_conversion(618, s, len, i))
                break;                   /* NOCONVERSION: as it is */
            i = -1;                      /* ONSOURCE changed: again */
        }
    }
    for (int i = 0; i < n; i++)
        t[i] = i < len ? s[i] : ' ';
    if (s != small)
        free(s);
}

