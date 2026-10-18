/* pl1pic.c -- numeric character data (PICTURE), as PL/I (F) edits it.
 *
 * A picture is passed as its characters and their number, repetition
 * factors expanded, V included (gen.c): C'ZZZV.99', 7.  The value
 * comes and goes as packed decimal of 15 digits (8 bytes), the integer
 * value * 10**q for the picture's q digits after the V.
 *
 * The rules are the Language Reference's (GC28-8201-4, section D), each
 * one measured on PL/I (F) V5.5 (JOB00288): digit
 * positions 9 Y T I R, zero suppression Z *, insertion characters , . /
 * B, static and drifting $ S + -, CR and DB.
 */
#include <string.h>

#include "pl1pic.h"

/* the characters of an overpunched digit: plus (12 zone), minus (11) */
static const char over_plus[] = "{ABCDEFGHI";
static const char over_minus[] = "}JKLMNOPQR";

enum { P_DIG, P_Y, P_T, P_I, P_R, P_Z, P_STAR, P_INS, P_B, P_STATIC, P_DSYM, P_DDIG, P_CR, P_DB,
       P_LIT };

/* A sterling picture's characters that are never suppressed, as
 * sterling_form() writes them: the field separators . / B H, and P */
static const char lit_in[] = "olbhp", lit_out[] = "./ SD";

struct pos { char c; unsigned char kind; };

/* The positions of the picture (V left out); *vidx the position after
 * the V (the number of positions if there is none), *nd the number of
 * digit positions, [*da, *db] the drifting string.  Returns the number. */
static int positions(const char *pic, int n, struct pos *ps, int *nd, int *vidx, char *drift,
                     int *da, int *db)
{
    int np = 0, cnt[4] = { 0, 0, 0, 0 };
    const char *dchars = "$S+-";
    *drift = 0;
    *nd = 0;
    *vidx = -1;
    for (int i = 0; i < n; i++) {
        const char *d = strchr(dchars, pic[i]);
        if (d && pic[i])
            cnt[d - dchars]++;
    }
    for (int k = 0; k < 4; k++)
        if (cnt[k] > 1)
            *drift = dchars[k];
    *da = *db = -1;
    for (int i = 0; i < n; i++) {
        char c = pic[i];
        struct pos *p = &ps[np];
        p->c = c;
        if (c == 'V') {
            *vidx = np;
            continue;
        }
        if (c == '9') p->kind = P_DIG;
        else if (c == 'Y') p->kind = P_Y;
        else if (c == 'T') p->kind = P_T;
        else if (c == 'I') p->kind = P_I;
        else if (c == 'R') p->kind = P_R;
        else if (c == 'Z') p->kind = P_Z;
        else if (c == '*') p->kind = P_STAR;
        else if (c == 'B') p->kind = P_B;
        else if (c == ',' || c == '.' || c == '/') p->kind = P_INS;
        else if (c && strchr(lit_in, c)) p->kind = P_LIT;
        else if (c == 'C' && i + 1 < n && pic[i + 1] == 'R') { p->kind = P_CR; i++; np++; ps[np] = *p; }
        else if (c == 'D' && i + 1 < n && pic[i + 1] == 'B') { p->kind = P_DB; i++; np++; ps[np] = *p; }
        else if (c == *drift) {
            if (*da < 0) {
                p->kind = P_DSYM;
                *da = np;
            } else {
                p->kind = P_DDIG;
            }
            *db = np;
        }
        else p->kind = P_STATIC;
        if (p->kind <= P_STAR || p->kind == P_DDIG)
            (*nd)++;
        np++;
    }
    if (*vidx < 0)
        *vidx = np;
    /* insertion characters right after the last drifting character
     * belong to the drifting string */
    if (*db >= 0)
        while (*db + 1 < np && (ps[*db + 1].kind == P_INS || ps[*db + 1].kind == P_B))
            (*db)++;
    return np;
}

static int is_digit_pos(int k)
{
    return k <= P_STAR || k == P_DDIG;
}

static void edit_digits(const char *digit, int neg, int sig, const char *pic, int n, char *dst);
static int sterling_form(const char *pic, int n, char *tp, int f[4], char kind[2]);
static void sterling_edit(const unsigned char *val, const char *pic, int n, char *dst);

void pl1_pic_edit(const unsigned char *val, const char *pic, int n, char *dst)
{
    char digit[16];
    int nd = pl1_pic_digits(pic, n);

    if (n > 0 && pic[0] == 'G') {
        sterling_edit(val, pic, n, dst);
        return;
    }
    /* the low nd of the 15 digits (more are lost: SIZE, undefined) */
    for (int i = 0; i < nd; i++) {
        int j = 15 - nd + i, b = val[j / 2];
        digit[i] = (char)('0' + (j % 2 ? b & 15 : b >> 4));
    }
    edit_digits(digit, (val[7] & 15) == 0x0B || (val[7] & 15) == 0x0D, 0, pic, n, dst);
}

/* The picture's digit positions filled with digit[] (a sterling field
 * of one position holds a character of its own), the sign neg; sig set:
 * significant from the start, no zero suppressed. */
static void edit_digits(const char *digit, int neg, int sig, const char *pic, int n, char *dst)
{
    struct pos ps[260];
    char drift, fill = ' ';
    int nd, vidx, da, db, np = positions(pic, n, ps, &nd, &vidx, &drift, &da, &db);
    int allzero = 1, supp_all = 1;
    int k = 0, sympos = -1;

    for (int i = 0; i < nd; i++)
        if (digit[i] != '0')
            allzero = 0;
    for (int i = 0; i < np; i++) {
        if (ps[i].kind == P_STAR)
            fill = '*';
        if (is_digit_pos(ps[i].kind) && ps[i].kind != P_Z && ps[i].kind != P_STAR &&
            ps[i].kind != P_DDIG)
            supp_all = 0;
    }
    if (supp_all && allzero && !sig) {
        /* every digit suppressed: the whole field blank, or asterisks --
         * but for a sterling picture's separators (P2 of 0: "  .  .  D") */
        for (int i = 0; i < np; i++)
            dst[i] = ps[i].kind == P_LIT ? lit_out[strchr(lit_in, ps[i].c) - lit_in] : fill;
        return;
    }
    if (da >= 0) {
        /* the drifting symbol: left of the first significant digit of the
         * drifting string, else at its end */
        int kk = 0;
        for (int i = 0; i < np && sympos < 0; i++) {
            if (!is_digit_pos(ps[i].kind))
                continue;
            if (ps[i].kind == P_DDIG && digit[kk] != '0')
                sympos = i - 1;
            kk++;
        }
        if (sympos < 0)
            sympos = db;
    }
    for (int i = 0; i < np; i++) {
        struct pos *p = &ps[i];
        char d = is_digit_pos(p->kind) ? digit[k] : 0;
        if (i == vidx)
            sig = 1;   /* unless all is suppressed (above), the fraction shows,
                        * and an insertion character right after the V */
        if (da >= 0 && i >= da && i <= db) {
            if (i == sympos)
                dst[i] = drift == '$' ? '$' : drift == 'S' ? (neg ? '-' : '+') :
                         drift == '+' ? (neg ? ' ' : '+') : (neg ? '-' : ' ');
            else if (i < sympos)
                dst[i] = ' ';
            else
                dst[i] = p->kind == P_DDIG ? d : p->kind == P_B ? ' ' : p->c;
            if (p->kind == P_DDIG) {
                k++;
                if (i > sympos)
                    sig = 1;
            }
            continue;
        }
        switch (p->kind) {
        case P_DIG:
            dst[i] = d;
            sig = 1;
            k++;
            break;
        case P_Y:
            dst[i] = d == '0' ? ' ' : d;
            if (d != '0')
                sig = 1;
            k++;
            break;
        case P_T: case P_I: case P_R: {
            int over = p->kind == P_T || (p->kind == P_I && !neg) || (p->kind == P_R && neg);
            dst[i] = over ? (neg ? over_minus : over_plus)[d - '0'] : d;
            sig = 1;
            k++;
            break;
        }
        case P_Z: case P_STAR:
            if (!sig && d == '0') {
                dst[i] = p->kind == P_STAR ? '*' : ' ';
            } else {
                dst[i] = d;
                sig = 1;
            }
            k++;
            break;
        case P_INS: {
            /* after a significant digit, the character; else as the digit
             * position before it -- an asterisk, a blank -- and with no
             * digit position before it at all, the character */
            int j = i - 1;
            while (j >= 0 && !is_digit_pos(ps[j].kind))
                j--;
            dst[i] = sig || j < 0 ? p->c : ps[j].kind == P_STAR ? '*' : ' ';
            break;
        }
        case P_B:
            dst[i] = ' ';
            break;
        case P_LIT:
            dst[i] = lit_out[strchr(lit_in, p->c) - lit_in];
            break;
        case P_STATIC:
            dst[i] = p->c == '$' ? '$' : p->c == 'S' ? (neg ? '-' : '+') :
                     p->c == '+' ? (neg ? ' ' : '+') : (neg ? '-' : ' ');
            break;
        case P_CR: case P_DB:
            dst[i] = neg ? (p->kind == P_CR ? "CR" : "DB")[i > 0 && ps[i - 1].kind == p->kind] : ' ';
            break;
        default:
            dst[i] = p->c;
        }
    }
}

int pl1_pic_length(const char *pic, int n)
{
    int len = 0;
    char tp[260], kind[2];
    int f[4];
    if (n > 0 && pic[0] == 'G') {
        n = sterling_form(pic, n, tp, f, kind);
        pic = tp;
    }
    for (int i = 0; i < n; i++)
        if (pic[i] != 'V')
            len++;
    return len;
}

/* The arithmetic value of a character string edited by the picture.
 * The sign is read as the picture wrote it: a blank in a + position is
 * minus, as is an I digit without its overpunch (and an R digit without
 * one is plus). */
static void sterling_value(const char *src, const char *pic, int n, unsigned char *val);
static int value_digits(const char *src, const char *pic, int n, char *digit, char *raw);

void pl1_pic_value(const char *src, const char *pic, int n, unsigned char *val)
{
    char digit[16];
    int nd, neg;

    if (n > 0 && pic[0] == 'G') {
        sterling_value(src, pic, n, val);
        return;
    }
    neg = value_digits(src, pic, n, digit, NULL);
    nd = pl1_pic_digits(pic, n);
    memset(val, 0, 8);
    for (int i = 0; i < nd; i++) {
        int j = 15 - nd + i;
        if (j % 2)
            val[j / 2] |= (unsigned char)(digit[i] - '0');
        else
            val[j / 2] |= (unsigned char)((digit[i] - '0') << 4);
    }
    val[7] |= neg ? 0x0D : 0x0C;
}

/* The digits of the string at the picture's digit positions (a
 * suppressed one 0), each position's own character in raw[] if given;
 * returns the sign, 1 for minus. */
static int value_digits(const char *src, const char *pic, int n, char *digit, char *raw)
{
    struct pos ps[260];
    char drift;
    int nd, vidx, da, db, np = positions(pic, n, ps, &nd, &vidx, &drift, &da, &db), neg = 0, k = 0;
    int plus_seen = 0;

    for (int i = 0; i < np; i++) {
        struct pos *p = &ps[i];
        char c = src[i];
        int in_drift = da >= 0 && i >= da && i <= db;
        if (in_drift && c == '+')
            plus_seen = 1;
        if (in_drift && c == '-')
            neg = 1;
        if (is_digit_pos(p->kind)) {
            const char *o;
            if (c >= '0' && c <= '9') {
                digit[k] = c;
                if (p->kind == P_I)
                    neg = 1;
            } else if (c && (o = strchr(over_plus, c)) != NULL) {
                digit[k] = (char)('0' + (o - over_plus));
            } else if (c && (o = strchr(over_minus, c)) != NULL) {
                digit[k] = (char)('0' + (o - over_minus));
                neg = 1;
            } else {
                digit[k] = '0';          /* suppressed: blank, *, a symbol */
            }
            if (raw)
                raw[k] = c;
            k++;
        } else if (p->kind == P_STATIC && p->c == '+' && c == ' ') {
            neg = 1;
        } else if (!in_drift && (p->kind == P_STATIC || p->kind == P_DSYM) && c == '-') {
            neg = 1;
        } else if ((p->kind == P_CR && c == 'C') || (p->kind == P_DB && c == 'D')) {
            neg = 1;
        }
    }
    if (drift == '+' && !plus_seen)
        neg = 1;
    return neg;
}

/* The number of digit positions after the V. */
int pl1_pic_scale(const char *pic, int n)
{
    struct pos ps[260];
    char drift;
    int nd, vidx, da, db, np, q = 0;
    if (n > 0 && pic[0] == 'G') {
        char tp[260], kind[2];
        int f[4];
        sterling_form(pic, n, tp, f, kind);
        return f[3];
    }
    np = positions(pic, n, ps, &nd, &vidx, &drift, &da, &db);
    for (int i = vidx; i < np; i++)
        if (is_digit_pos(ps[i].kind))
            q++;
    return q;
}

/* Edit the number with digits d[0..nd), q of them after the point, by
 * the picture (the P format item): its digits at the picture's scale,
 * the low fifteen kept.  Returns the length of the field. */
int pl1_pic_edit_digits(const char *d, int nd, int q, int neg, const char *pic, int n, char *dst)
{
    unsigned char val[8];
    char dig[64];
    int m = 0, qp = pl1_pic_scale(pic, n);
    for (int i = 0; i < nd - (q > qp ? q - qp : 0) && m < 60; i++)
        dig[m++] = d[i];
    for (int i = q; i < qp && m < 60; i++)
        dig[m++] = '0';
    memset(val, 0, sizeof val);
    for (int i = 0; i < 15 && i < m; i++) {
        int j = 14 - i, v = dig[m - 1 - i] - '0';
        val[j / 2] |= (unsigned char)(j % 2 ? v : v << 4);
    }
    val[7] |= neg ? 0x0D : 0x0C;
    pl1_pic_edit(val, pic, n, dst);
    return pl1_pic_length(pic, n);
}


/* The number of digit positions: 9 Y T I R Z *, and a drifting string's
 * characters but its first. */
int pl1_pic_digits(const char *pic, int n)
{
    struct pos ps[260];
    char drift;
    int nd, vidx, da, db;
    if (n > 0 && pic[0] == 'G') {
        /* a value in pence: the pounds' digits and three, the pence's
         * fraction (Q2STL: GM99M/99M/99 is FIXED(5), GMZ9M.99M.99V99
         * FIXED(7,2)) */
        char tp[260], kind[2];
        int f[4];
        sterling_form(pic, n, tp, f, kind);
        return f[0] + 3 + f[3];
    }
    positions(pic, n, ps, &nd, &vidx, &drift, &da, &db);
    return nd;
}

/* ---- sterling pictures (GC28-8201-4, section D, "Sterling Pictures") --
 *
 * G [edit 1] M pounds M [separator 1] shillings M [separator 2] pence
 * [edit 2].  The value is pence.  sterling_form() makes it a picture of
 * the kind above: G and M left out, the separators and P literal
 * characters (lit_in), the single-position fields 8, 7 and 6 a 9 whose
 * digit is the field's character.  f[0..3] the digit positions of the
 * pounds, shillings, pence and the pence's fraction; kind[0] '8' or 0,
 * kind[1] '7', '6' or 0.  Measured on PL/I (F) V5.5: Q1STL (JOB01230),
 * Q2STL to Q4STL (JOB01978, JOB01982, JOB01984). */
static int sterling_form(const char *pic, int n, char *tp, int f[4], char kind[2])
{
    int k = 0, field = -1, sep = 0, frac = 0, cnt[4] = { 0, 0, 0, 0 };
    f[0] = f[1] = f[2] = f[3] = 0;
    kind[0] = kind[1] = 0;
    for (int i = 1; i < n && k < 255; i++) {
        char c = pic[i];
        if (c == 'M') {
            field++;
            sep = field > 0;
            continue;
        }
        if ((c == 'C' && i + 1 < n && pic[i + 1] == 'R') ||
            (c == 'D' && i + 1 < n && pic[i + 1] == 'B')) {
            /* CR, DB: no digit position */
            tp[k++] = c;
            tp[k++] = pic[++i];
            continue;
        }
        if (sep && strchr("/.BH", c)) {
            tp[k++] = c == '/' ? 'l' : c == '.' ? 'o' : c == 'B' ? 'b' : 'h';
            continue;
        }
        sep = 0;
        if (field == 1 && c == '8') {
            kind[0] = '8';
            tp[k++] = '9';
            f[1]++;
            continue;
        }
        if (field == 2 && (c == '7' || c == '6')) {
            kind[1] = c;
            tp[k++] = '9';
            f[2]++;
            continue;
        }
        if (field == 2 && c == 'V')
            frac = 1;
        if (field == 2 && c == 'P') {
            tp[k++] = 'p';
            continue;
        }
        if (field >= 0 && strchr("9ZY*TIR", c))
            f[field == 2 && frac ? 3 : field]++;
        if (field == 0 && strchr("$+-S", c))
            cnt[strchr("$+-S", c) - "$+-S"]++;
        tp[k++] = c;
    }
    /* a drifting string in the pounds: its characters but the first */
    for (int d = 0; d < 4; d++)
        if (cnt[d] > 1)
            f[0] += cnt[d] - 1;
    tp[k] = 0;
    return k;
}

static char enc_field(int v, char kind)
{
    if (v < 10)
        return (char)('0' + v);
    if (kind == '8')
        return v == 10 ? '&' : (char)('A' + v - 11);   /* 11 to 19: A to I */
    if (kind == '7')
        return v == 10 ? '&' : '-';
    return v == 10 ? '-' : '&';                          /* 6, IBM's */
}

static int dec_field(char c, char kind)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (kind == '8')
        return c == '&' ? 10 : c >= 'A' && c <= 'I' ? 11 + (c - 'A') : 0;
    if (kind == '7')
        return c == '&' ? 10 : c == '-' ? 11 : 0;
    return c == '-' ? 10 : c == '&' ? 11 : 0;
}

/* v's low m digits into d, right to left */
static void put_digits(char *d, int m, long long v)
{
    for (int i = m - 1; i >= 0; i--, v /= 10)
        d[i] = (char)('0' + (int)(v % 10));
}

static void sterling_edit(const unsigned char *val, const char *pic, int n, char *dst)
{
    char tp[260], kind[2], digit[64];
    int f[4], tn = sterling_form(pic, n, tp, f, kind), m = 0, sig = 0;
    long long ip = 0, fr = 0, l, pow = 1;
    for (int j = 0; j < 15; j++) {
        int dg = j % 2 ? val[j / 2] & 15 : val[j / 2] >> 4;
        if (j < 15 - f[3])
            ip = ip * 10 + dg;
        else
            fr = fr * 10 + dg;
    }
    l = ip / 240;
    for (int i = 0; i < f[0]; i++)
        pow *= 10;
    /* pounds lost (SIZE): PL/I (F) suppresses no zero then (Q4STL) */
    if (l >= pow)
        sig = 1;
    put_digits(digit, f[0], l);
    m = f[0];
    if (kind[0])
        digit[m++] = enc_field((int)(ip % 240 / 12), '8');
    else {
        put_digits(digit + m, f[1], ip % 240 / 12);
        m += f[1];
    }
    if (kind[1])
        digit[m++] = enc_field((int)(ip % 12), kind[1]);
    else {
        put_digits(digit + m, f[2], ip % 12);
        m += f[2];
    }
    put_digits(digit + m, f[3], fr);
    edit_digits(digit, (val[7] & 15) == 0x0B || (val[7] & 15) == 0x0D, sig, tp, tn, dst);
}

static void sterling_value(const char *src, const char *pic, int n, unsigned char *val)
{
    char tp[260], kind[2], digit[64], raw[64];
    int f[4], tn = sterling_form(pic, n, tp, f, kind), neg, k = 0;
    long long l = 0, sh = 0, pe = 0, fr = 0, v;
    neg = value_digits(src, tp, tn, digit, raw);
    for (int i = 0; i < f[0]; i++)
        l = l * 10 + (digit[k++] - '0');
    if (kind[0])
        sh = dec_field(raw[k++], '8');
    else
        for (int i = 0; i < f[1]; i++)
            sh = sh * 10 + (digit[k++] - '0');
    if (kind[1])
        pe = dec_field(raw[k++], kind[1]);
    else
        for (int i = 0; i < f[2]; i++)
            pe = pe * 10 + (digit[k++] - '0');
    for (int i = 0; i < f[3]; i++)
        fr = fr * 10 + (digit[k++] - '0');
    v = l * 240 + sh * 12 + pe;
    for (int i = 0; i < f[3]; i++)
        v *= 10;
    v += fr;
    memset(val, 0, 8);
    for (int j = 14; j >= 0; j--, v /= 10)
        val[j / 2] |= (unsigned char)(j % 2 ? v % 10 : (v % 10) << 4);
    val[7] |= neg ? 0x0D : 0x0C;
}
