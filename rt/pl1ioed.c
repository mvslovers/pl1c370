/* pl1ioed.c -- PUT EDIT and GET EDIT formats, PICTURE and FLOAT
 * output of PL1C/370's run time, out of pl1io.c: a module links them only
 * when it uses them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "pl1rt.h"
#include "pl1pic.h"
#include "pl1int.h"
#include "pl1flt.h"
#include "pl1iof.h"

/* ---- PUT EDIT ---------------------------------------------------------- */

/* The format list in the program: halfwords kind, w, d per item, ended
 * by kind 0 (F_END).  Kinds as the compiler numbers them. */

static void edit_value(const char *text, int tlen, const char *d, int nd, int q, int neg,
                       int numeric);

void pl1_format_begin(const short *list)
{
    pl1x_edit = edit_value;
    fmt = list;
    fi = 0;
    rsp = 0;
}

/* Write text at the current position of the output line. */
static void put_text(const char *t, int len)
{
    struct pfile *f = out_file();
    while (len > 0) {
        int room = f->width - f->end, n;
        if (room <= 0) {
            if (f->isstring)
                return;            /* beyond the string: STRINGSIZE, not supported */
            emit(f);
            continue;
        }
        n = len < room ? len : room;
        memcpy(f->line + f->end, t, (size_t)n);
        f->end += n;
        f->pstart = f->end;
        f->tail = 0;
        t += n;
        len -= n;
    }
}

static void blanks(int n)
{
    char b[LINESIZE];
    memset(b, ' ', sizeof b);
    while (n > 0) {
        int k = n > LINESIZE ? LINESIZE : n;
        put_text(b, k);
        n -= k;
    }
}

/* Execute control items from the cursor on; stop at a data item (return
 * its index) or, when wrap is 0, at the end of the list (return -1). */
int next_data(int wrap)
{
    int guard = 0;
    for (;;) {
        const short *it;
        if (!fmt)
            return -1;
        if (rsp && fi == rst[rsp - 1][1]) {
            /* the end of a repeated group: again, or past it */
            if (--rst[rsp - 1][2] > 0)
                fi = rst[rsp - 1][0];
            else
                rsp--;
            continue;
        }
        it = fmt + 3 * fi;
        if (it[0] == K_REP) {
            /* (n) group: n times, none for n <= 0 (Q1FMT) */
            if (it[1] <= 0) {
                fi += 1 + it[2];
            } else {
                /* at most MAXREP deep: parse.c refuses more */
                rst[rsp][0] = (short)(fi + 1);
                rst[rsp][1] = (short)(fi + 1 + it[2]);
                rst[rsp][2] = it[1];
                rsp++;
                fi++;
            }
            continue;
        }
        if (it[0] == K_END) {
            if (!wrap || fi == 0 || ++guard > 1)
                return -1;
            fi = 0;
            continue;
        }
        if (mode == M_GETEDIT)
            return fi;                  /* GET EDIT handles control items itself */
        switch (it[0]) {
        case K_A: case K_AW: case K_F: case K_E: case K_P: case K_B: case K_BW:
            return fi;
        case K_X:
            blanks(it[1]);
            break;
        case K_COL: {
            struct pfile *f = out_file();
            if (f->end > it[1] - 1)
                emit(f);
            blanks(it[1] - 1 - f->end);
            break;
        }
        case K_TAB:
            pl1i_tab(out_file(), it[1]);       /* ANSI 8.7.2.10 */
            break;
        case K_SKIP:
            pl1_skip(it[1]);
            break;
        case K_PAGE:
            pl1_page();
            break;
        case K_LINE:
            pl1_skip(1);
            break;
        }
        fi++;
    }
}

/* F(w,d) of the number with digits d[0..nd), q after the point. */
static int f_field(char *out, int w, int dd, const char *d, int nd, int q, int neg)
{
    char digs[64], text[80];
    int n = 0, t = 0, ip, zero = 1;

    for (int i = 0; i < nd && n < 60; i++)
        digs[n++] = d[i];
    if (q > dd) {
        /* round half up on the magnitude at position dd */
        int cut = n - (q - dd), carry;
        carry = cut < n && digs[cut] >= '5';
        n = cut;
        for (int i = n - 1; i >= 0 && carry; i--) {
            if (digs[i] == '9') {
                digs[i] = '0';
            } else {
                digs[i]++;
                carry = 0;
            }
        }
        if (carry) {
            memmove(digs + 1, digs, (size_t)n);
            digs[0] = '1';
            n++;
        }
    } else {
        for (int i = q; i < dd; i++)
            digs[n++] = '0';
    }
    for (int i = 0; i < n; i++)
        if (digs[i] != '0')
            zero = 0;
    ip = n - dd;
    if (neg && !zero)
        text[t++] = '-';
    {
        int i = 0;
        while (i < ip - 1 && digs[i] == '0')
            i++;
        if (ip <= 0)
            text[t++] = '0';
        for (; i < ip; i++)
            text[t++] = digs[i];
    }
    if (dd > 0) {
        text[t++] = '.';
        for (int i = ip; i < 0; i++)
            text[t++] = '0';
        for (int i = ip < 0 ? 0 : ip; i < n; i++)
            text[t++] = digs[i];
    }
    if (t >= w) {
        memcpy(out, text + t - w, (size_t)w);   /* too narrow: rightmost w */
    } else {
        memset(out, ' ', (size_t)(w - t));
        memcpy(out + w - t, text, (size_t)t);
    }
    return w;
}

static int e_text(char *out, int w, int dd, int sd, const char *dg, int e, int neg, int zero);

/* E(w,d,s) of a decimal number, digits d[0..nd), q after the point: its
 * s significant digits, rounded half up (Q1FEXP: 12.345 in E(12,2) is
 * 1.23E+01).  A carry out of the first digit is lost, the exponent kept,
 * as PL/I (F) does (t151efx: -999.96 in E(10,2) is -0.00E+02) */
static int e_field_dec(char *out, int w, int dd, int sd, const char *d, int nd, int q, int neg)
{
    char dg[24];
    int i0 = 0, e;
    while (i0 < nd && d[i0] == '0')
        i0++;
    if (sd > 20)
        sd = 20;
    if (i0 == nd) {
        memset(dg, '0', (size_t)sd);
        return e_text(out, w, dd, sd, dg, 0, 0, 1);
    }
    e = nd - q - 1 - i0;
    for (int i = 0; i < sd; i++)
        dg[i] = i0 + i < nd ? d[i0 + i] : '0';
    if (i0 + sd < nd && d[i0 + sd] >= '5') {
        int i = sd - 1;
        while (i >= 0 && dg[i] == '9')
            dg[i--] = '0';
        if (i >= 0)
            dg[i]++;
    }
    return e_text(out, w, dd, sd, dg, e, neg, 0);
}

/* A character string's value as decimal digits, for E and F (Q1FMT:
 * '1.5E2' is 150, ' -42.5' is -42.5): blanks, a sign, digits with a
 * point, an exponent, blanks.  0 when it is no number. */
static int str_number(const char *t, int n, char *dg, int *nd, int *q, int *neg)
{
    int i = 0, k = 0, frac = 0, point = 0, e = 0, eneg = 0, digits = 0;
    *neg = 0;
    while (i < n && t[i] == ' ')
        i++;
    if (i < n && (t[i] == '+' || t[i] == '-'))
        *neg = t[i++] == '-';
    for (; i < n; i++) {
        if (t[i] >= '0' && t[i] <= '9') {
            if (k < 31) {
                dg[k++] = t[i];
                frac += point;
            } else if (!point) {
                return 0;
            }
            digits = 1;
        } else if (t[i] == '.' && !point) {
            point = 1;
        } else {
            break;
        }
    }
    if (!digits)
        return 0;
    if (i < n && t[i] == 'E') {
        int ed = 0;
        i++;
        if (i < n && (t[i] == '+' || t[i] == '-'))
            eneg = t[i++] == '-';
        for (; i < n && t[i] >= '0' && t[i] <= '9'; i++, ed = 1)
            e = 10 * e + (t[i] - '0');
        if (!ed)
            return 0;
    }
    while (i < n && t[i] == ' ')
        i++;
    if (i < n)
        return 0;
    *q = frac - (eneg ? -e : e);
    while (*q < 0 && k < 31) {
        dg[k++] = '0';
        (*q)++;
    }
    if (*q < 0)
        return 0;
    *nd = k;
    return 1;
}

static void edit_value(const char *text, int tlen, const char *d, int nd, int q, int neg,
                       int numeric)
{
    int k = next_data(1);
    const short *it;
    char field[260], sdig[32], bits[64], dg[1100];

    if (k < 0) {
        fputs("PL1C/370: PUT EDIT format list has no data format item\n", stderr);
        exit(16);
    }
    it = fmt + 3 * k;
    if ((it[0] == K_B || it[0] == K_BW) && numeric) {
        /* the number as a bit string first, as PL/I (F) converts it: the
         * integer part of its absolute value in pl1x_bits bits (Q1FMT:
         * B(8) of 5 is 0101, of 2.75 0010) */
        char iv[32];
        int ni = nd - q > 0 ? nd - q : 0;
        if (it[0] == K_B) {
            /* B without a width takes only a bit string (T183FCV) */
            pl1i_errmsg("IHE703I B FORMAT W UNSPECIFIED AND LIST ITEM NOT TYPE STRING");
            fi = k + 1;
            return;
        }
        if (pl1x_bits <= 0 || pl1x_bits > 63 || ni > 31) {
            fputs("PL1C/370: B format for this arithmetic value is not supported\n", stderr);
            exit(16);
        }
        for (int i = 0; i < ni; i++)
            iv[i] = (char)(d[i] - '0');
        /* the bits from the right: the digits divided by 2 again and again */
        for (int b = pl1x_bits - 1; b >= 0; b--) {
            int r = 0;
            for (int i = 0; i < ni; i++) {
                int x = 10 * r + iv[i];
                iv[i] = (char)(x / 2);
                r = x % 2;
            }
            bits[b] = r ? '1' : '0';
        }
        text = bits;
        tlen = pl1x_bits;
        numeric = 0;
    }
    if (!numeric && (it[0] == K_E || it[0] == K_F)) {
        /* a character string converted to a number first (Q1FMT) */
        if (!str_number(text, tlen, sdig, &nd, &q, &neg)) {
            fputs("PL1C/370: E or F format for a string that is no number is not "
                  "supported\n", stderr);
            exit(16);
        }
        d = sdig;
        numeric = 1;
    }
    if ((it[0] == K_B || it[0] == K_BW) && it[2] > 1 && !numeric) {
        /* ANSI's B2, B3, B4 (8.7.2.4): the bits, zeros added to a whole
         * number of digits, as digits of m bits each */
        static const char dig[] = "0123456789ABCDEF";
        int m = it[2], k = 0;
        for (int i = 0; i < tlen; i += m) {
            int v = 0;
            for (int b = 0; b < m; b++)
                v = 2 * v + (i + b < tlen && text[i + b] == '1');
            if (k < (int)sizeof dg)
                dg[k++] = dig[v];
        }
        text = dg;
        tlen = k;
    }
    switch (it[0]) {
    case K_A:
    case K_B:                     /* a bit string's characters, as A */
        put_text(text, tlen);
        break;
    case K_BW:
        if (tlen > it[1])
            pl1i_strsize();       /* ANSI: more digits than w, then cut */
        /* fall through */
    case K_AW: {
        int w = it[1] > 256 ? 256 : it[1];
        memset(field, ' ', (size_t)w);
        memcpy(field, text, (size_t)(tlen < w ? tlen : w));
        put_text(field, w);
        break;
    }
    case K_E:
        if (!numeric) {
            fputs("PL1C/370: E format for a string is not supported\n", stderr);
            exit(16);
        }
        put_text(field, e_field_dec(field, it[1] > 250 ? 250 : it[1], it[2] & 255, it[2] >> 8,
                                    d, nd, q, neg));
        break;
    case K_F:
        if (!numeric) {
            fputs("PL1C/370: F format for a string is not supported\n", stderr);
            exit(16);
        }
        put_text(field, f_field(field, it[1] > 250 ? 250 : it[1], it[2], d, nd, q, neg));
        break;
    case K_P:
        /* P'picture': it[1] characters, it[2] bytes from the list's start */
        if (!numeric) {
            fputs("PL1C/370: P format for a string is not supported\n", stderr);
            exit(16);
        }
        put_text(field, pl1_pic_edit_digits(d, nd, q, neg, (const char *)fmt + it[2], it[1],
                                            field));
        break;
    }
    fi = k + 1;
}

/* A numeric picture: its characters for LIST, DATA (no quotes, JOB00290)
 * and the A format, its arithmetic value for the others. */
void pl1_list_pic(const char *chars, int len, const unsigned char *packed, int q)
{
    if (mode == M_PUTEDIT) {
        char d[32];
        int neg, nd = pl1i_dec_digits(d, packed, 15, &neg);
        edit_value(chars, len, d, nd, q, neg, 1);
        return;
    }
    pl1i_place_num = 1;
    place(chars, len);
    pl1i_place_num = 0;
}

/* ---- FLOAT ------------------------------------------------------------ */

/* E(w,d,s): s significant digits rounded, d of them after the point,
 * the exponent so that s-d digits stand before it (JOB00156/158:
 * E(12,3,5) of -12345.6789 is -12.346E+03). */
static int e_field(char *out, int w, int dd, int sd, const unsigned char *v, int longf)
{
    char dg[24];
    int e, neg;

    pl1f_to_digits(v, longf, sd, 1, dg, &e, &neg);
    return e_text(out, w, dd, sd, dg, e, neg, dg[0] == '0');
}

/* The E field of sd digits dg, the first at exponent e */
static int e_text(char *out, int w, int dd, int sd, const char *dg, int e, int neg, int zero)
{
    char text[48];
    int t = 0, ip = sd - dd, x;

    if (neg)
        text[t++] = '-';
    for (int i = 0; i < ip; i++)
        text[t++] = dg[i];
    if (dd > 0) {
        if (ip == 0)
            text[t++] = '0';
        text[t++] = '.';
        for (int i = ip; i < sd; i++)
            text[t++] = dg[i];
    }
    x = e - (ip - 1);
    if (zero)
        x = 0;
    text[t++] = 'E';
    text[t++] = x < 0 ? '-' : '+';
    if (x < 0)
        x = -x;
    text[t++] = (char)('0' + x / 10 % 10);
    text[t++] = (char)('0' + x % 10);
    if (t >= w) {
        memcpy(out, text + t - w, (size_t)w);
    } else {
        memset(out, ' ', (size_t)(w - t));
        memcpy(out + w - t, text, (size_t)t);
    }
    return w;
}

void pl1_list_float(const unsigned char *v, int longf, int p)
{
    char field[260];
    int k;
    const short *it;

    if (mode != M_PUTEDIT) {
        pl1i_place_num = 1;
        place(field, pl1i_flt_field(field, v, longf, p));
        pl1i_place_num = 0;
        return;
    }
    k = next_data(1);
    if (k < 0) {
        fputs("PL1C/370: PUT EDIT format list has no data format item\n", stderr);
        exit(16);
    }
    it = fmt + 3 * k;
    switch (it[0]) {
    case K_E:
        put_text(field, e_field(field, it[1] > 250 ? 250 : it[1], it[2] & 255, it[2] >> 8, v,
                                longf));
        break;
    case K_F: {
        /* the value rounded to d places, then as F(w,d) of a decimal */
        char dg[64];
        int neg, nd = pl1f_to_fixed(v, longf, it[2], 1, dg, 60, &neg);
        put_text(field, f_field(field, it[1] > 250 ? 250 : it[1], it[2], dg, nd, it[2], neg));
        break;
    }
    case K_P: {
        /* the value at the picture's scale, truncated as an assignment
         * converts it, then edited */
        const char *pic = (const char *)fmt + it[2];
        char dg[64];
        int neg, q = pl1_pic_scale(pic, it[1]);
        int nd = pl1f_to_fixed(v, longf, q, 0, dg, 60, &neg);
        put_text(field, pl1_pic_edit_digits(dg, nd, q, neg, pic, it[1], field));
        break;
    }
    default: {
        /* A: the string a float converts to */
        int n = pl1i_flt_field(field, v, longf, p);
        fi = k;
        edit_value(field, n, NULL, 0, 0, 0, 0);
        return;
    }
    }
    fi = k + 1;
}

void pl1_format_end(void)
{
    if (mode == M_PUTEDIT)
        next_data(0);
    fmt = NULL;
}

