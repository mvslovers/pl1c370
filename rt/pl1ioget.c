/* pl1ioget.c -- GET LIST, GET EDIT and GET DATA of PL1C/370's run time,
 * out of pl1io.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "pl1rt.h"
#include "pl1pic.h"
#include "pl1int.h"
#include "pl1flt.h"
#include "pl1iof.h"

/* ---- GET --------------------------------------------------------------- */

static struct pfile *in_file(void)
{
    if (!cur->open)
        open_file(cur, 0);
    return cur;
}

/* Make a record current; 0 at end of file. */
static int need_record(struct pfile *f)
{
    if (f->have && f->rpos < f->rlen)
        return 1;
    if (f->have && f->rpos >= f->rlen && mode == M_GETEDIT)
        return 1;
    for (;;) {
        char *p;
        if (f->eof)
            return 0;
        if (f->isstring) {
            f->eof = 1;            /* a STRING is one record */
            return 0;
        }
        p = fgets(f->rec, sizeof f->recb, f->fp);
        if (!p) {
            f->eof = 1;
            return 0;
        }
        f->rlen = (int)strcspn(f->rec, "\n");
        f->rpos = 0;
        f->have = 1;
        if (mode != M_GETLIST || f->rlen > 0)
            return 1;
    }
}

static int next_record(struct pfile *f)
{
    f->have = 0;
    return need_record(f);
}

void pl1_get_skip(int n)
{
    struct pfile *f = in_file();
    for (; n > 0; n--) {
        f->have = 0;
        if (!need_record(f))
            return;
    }
}

/* The next LIST item: a quoted string or a run of characters up to a
 * blank or comma, across records.  Returns its length, -1 at end of
 * file; *quoted says what kind. */
/* GET ... COPY of a LIST item: its characters as they stand in the
 * record, quotes included (Q1IO) */
static void copy_raw(struct pfile *f, const char *rec, int st)
{
    if (pl1i_copy && !data_mode && f->rec == rec && f->rpos >= st)
        pl1i_copyline(rec + st, f->rpos - st);
}

static int list_item(char *out, int max, int *quoted, int *bit)
{
    struct pfile *f = in_file();
    int n = 0, st;
    const char *rec;

    *quoted = *bit = 0;
    for (;;) {
        if (!f->have || f->rpos >= f->rlen) {
            if (!next_record(f))
                return -1;
            continue;
        }
        if (f->rec[f->rpos] == ' ' || f->rec[f->rpos] == ',') {
            f->rpos++;
            continue;
        }
        break;
    }
    rec = f->rec;
    st = f->rpos;
    if (f->rec[f->rpos] == '\'') {
        *quoted = 1;
        f->rpos++;
        for (;;) {
            if (f->rpos >= f->rlen) {
                if (!next_record(f))
                    break;
                continue;
            }
            if (f->rec[f->rpos] == '\'') {
                if (f->rpos + 1 < f->rlen && f->rec[f->rpos + 1] == '\'') {
                    if (n < max) out[n++] = '\'';
                    f->rpos += 2;
                    continue;
                }
                f->rpos++;
                break;
            }
            if (n < max)
                out[n++] = f->rec[f->rpos];
            f->rpos++;
        }
        if (f->rpos < f->rlen && f->rec[f->rpos] == 'B') {
            *bit = 1;
            f->rpos++;
        }
        copy_raw(f, rec, st);
        return n;
    }
    while (f->rpos < f->rlen && f->rec[f->rpos] != ' ' && f->rec[f->rpos] != ',' &&
           !(data_mode && f->rec[f->rpos] == ';')) {
        if (n < max)
            out[n++] = f->rec[f->rpos];
        f->rpos++;
    }
    copy_raw(f, rec, st);
    return n;
}

/* The next EDIT field: run control items, then take the width of the
 * data item.  Returns the length, -1 at end of file. */
static const char *last_pic;     /* the picture of the last P field */
static int last_picn;

static int c_second;             /* a C item's second field is to be read */

static int edit_field(char *out, int max, int *kind, int *dd)
{
    struct pfile *f = in_file();
    for (;;) {
        int k = next_data(1);
        const short *it;
        if (k < 0) {
            fputs("PL1C/370: GET EDIT format list has no data format item\n", stderr);
            exit(16);
        }
        it = fmt + 3 * k;
        switch (it[0]) {
        case K_X:
            if (!need_record(f))
                return -1;
            f->rpos += it[1];
            fi = k + 1;
            continue;
        case K_COL:
            if (!need_record(f))
                return -1;
            if (f->rpos > it[1] - 1 && !next_record(f))
                return -1;
            f->rpos = it[1] - 1;
            fi = k + 1;
            continue;
        case K_TAB:
            fi = k + 1;         /* an output format: no effect on input */
            continue;
        case K_SKIP:
        case K_LINE:
            for (int i = 0; i < (it[0] == K_SKIP ? it[1] : 1); i++)
                if (!next_record(f))
                    return -1;
            fi = k + 1;
            continue;
        case K_PAGE:
            fi = k + 1;
            continue;
        case K_C:
            /* C(item,item): the first field now, the second after it
             * (get_num) */
            c_second = 1;
            fi = k + 1;
            continue;
        default: {
            int w = it[1], n = 0;
            if (it[0] == K_P) {
                /* P'picture': as many characters as the picture edits */
                last_pic = (const char *)fmt + it[2];
                last_picn = it[1];
                w = pl1_pic_length(last_pic, last_picn);
            }
            if (it[0] == K_A) {
                fputs("PL1C/370: A without a width in GET EDIT is not supported\n", stderr);
                exit(16);
            }
            *kind = it[0];
            *dd = it[2];
            if (!need_record(f))
                return -1;
            while (n < w) {
                if (f->rpos >= f->rlen) {
                    if (!next_record(f)) {
                        if (n == 0)
                            return -1;
                        break;
                    }
                    continue;
                }
                if (n < max)
                    out[n] = f->rec[f->rpos];
                n++;
                f->rpos++;
            }
            fi = k + 1;
            return n < max ? n : max;
        }
        }
    }
}

/* Fetch the next item as text: LIST or EDIT.  *implied: decimal places of
 * an F(w,d) field without a point. */
static int last_kind;            /* the format item of the last EDIT field */

static int get_item(char *out, int max, int *quoted, int *implied)
{
    int bit, kind = 0, dd = 0, n;
    *implied = 0;
    last_kind = 0;
    if (mode == M_GETEDIT) {
        n = edit_field(out, max, &kind, &dd);
        if (n >= 0 && pl1i_copy)
            pl1i_copyline(out, n);          /* COPY: the field */
        if (n > 0 && (kind == K_B || kind == K_BW) && (dd & 255) > 1) {
            /* ANSI's B2, B3, B4: each digit its m bits (8.7.1.9) */
            char d[260];
            int m = dd & 255, k = 0;
            for (int i = 0; i < n; i++) {
                char c = out[i];
                int v = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                if (v < 0)
                    continue;               /* the blanks around the digits */
                for (int b = 0; b < m && k < (int)sizeof d; b++)
                    d[k++] = (v >> (m - 1 - b)) & 1 ? '1' : '0';
            }
            n = k < max ? k : max;
            memcpy(out, d, (size_t)n);
        }
        if (n > 0 && kind == K_P) {
            /* the field's value as the picture reads it, as a number
             * (GET EDIT (P3) (P'GM99M/99M/99'): 12/03/04 is 2920, Q3STL) */
            unsigned char val[8];
            char num[40];
            int q = pl1_pic_scale(last_pic, last_picn), k = 0;
            pl1_pic_value(out, last_pic, last_picn, val);
            if ((val[7] & 15) == 0x0D)
                num[k++] = '-';
            for (int j = 0; j < 15; j++) {
                if (j == 15 - q)
                    num[k++] = '.';
                num[k++] = (char)('0' + (j % 2 ? val[j / 2] & 15 : val[j / 2] >> 4));
            }
            n = k < max ? k : max;
            memcpy(out, num, (size_t)n);
            last_kind = kind;
            *quoted = 0;
            return n;
        }
        last_kind = kind;
        *quoted = 1;                    /* the field is the value as it is */
        if (kind == K_F || kind == K_E) {
            *quoted = 0;
            *implied = dd & 255;
        }
        return n;
    }
    return list_item(out, max, quoted, &bit);
}

/* A complex target's parts (#2 C4b): PL1GCPX says that the next two
 * targets are the real and the imaginary part.  GET LIST and GET DATA
 * read one complex constant for both; GET EDIT two fields of C(item,
 * item), or one field of another item for the real part and 0 for the
 * imaginary part.  A real target takes the real part of a complex
 * constant, and with C reads both fields, the second for nothing
 * (Q11CGE2, JOB00248). */
static int gpair;                /* 1 the real part next, 2 the imaginary part */
static char gim[260];            /* ... its text */
static int gimn, gimq, gimimp, gimkind;

void pl1_get_cpx(void) asm("PL1GCPX");
void pl1_get_cpx(void)
{
    gpair = 1;
    c_second = 0;
}

static int get_num(char *t, int max, int *quoted, int *implied)
{
    int n;
    if (gpair == 2) {
        gpair = 0;
        memcpy(t, gim, (size_t)gimn);
        *quoted = gimq;
        *implied = gimimp;
        last_kind = gimkind;
        return gimn;
    }
    n = get_item(t, max, quoted, implied);
    if (n < 0) {
        gpair = c_second = 0;
        return n;
    }
    if (mode == M_GETEDIT) {
        if (c_second) {
            int q2, i2, k2 = last_kind;
            c_second = 0;
            gimn = get_item(gim, sizeof gim, &q2, &i2);
            if (gimn < 0)
                gimn = 0;
            gimq = q2;
            gimimp = i2;
            gimkind = last_kind;
            last_kind = k2;
            if (gpair == 1)
                gpair = 2;
        } else if (gpair == 1) {
            gim[0] = '0';
            gimn = 1;
            gimq = gimimp = gimkind = 0;
            gpair = 2;
        }
        return n;
    }
    if (!*quoted && pl1x_cxsplit) {
        int rs, rn, is, in, k = pl1x_cxsplit(t, n, &rs, &rn, &is, &in);
        if (gpair == 1) {
            while (k >= 0) {
                if (!pl1i_conversion(613, t, n, k)) {
                    rn = in = 0;
                    break;
                }
                k = pl1x_cxsplit(t, n, &rs, &rn, &is, &in);
            }
            if (in) {
                memcpy(gim, t + is, (size_t)in);
                gimn = in;
            } else {
                gim[0] = '0';
                gimn = 1;
            }
            gimq = gimimp = gimkind = 0;
            gpair = 2;
        } else if (k >= 0 || !in) {
            return n;               /* a real constant, or none: parse_number */
        }
        if (rn) {
            memmove(t, t + rs, (size_t)rn);
            n = rn;
        } else {
            t[0] = '0';
            n = 1;
        }
    }
    return n;
}

/* Parse a decimal number: digits, sign, scale.  Returns the digit count. */
static int parse_number(char *t, int n, char *d, int *q, int *neg, int implied)
{
    int nd, point;
again:
    nd = point = 0;
    *q = 0;
    *neg = 0;
    for (int i = 0; i < n; i++) {
        char c = t[i];
        if (c == ' ')
            continue;
        if (c == '-') { *neg = 1; continue; }
        if (c == '+') continue;
        if (c == '.') { point = 1; continue; }
        if (c >= '0' && c <= '9') {
            if (nd < 31) {
                d[nd++] = c;
                if (point) (*q)++;
            }
            continue;
        }
        if (c == 'E' && nd > 0) {
            /* a FLOAT constant into a FIXED target: its exponent moves
             * the scale (T187SGT: 1E2 is 100) */
            int k = i + 1, e = 0, eneg = 0, ed = 0;
            if (k < n && (t[k] == '+' || t[k] == '-'))
                eneg = t[k++] == '-';
            for (; k < n && t[k] >= '0' && t[k] <= '9'; k++, ed = 1)
                e = 10 * e + (t[k] - '0');
            while (k < n && t[k] == ' ')
                k++;
            if (ed && k == n) {
                *q -= eneg ? -e : e;
                while (*q < 0 && nd < 31) {
                    d[nd++] = '0';
                    (*q)++;
                }
                if (*q >= 0)
                    break;
            }
        }
        /* CONVERSION, ONCODE 613 for a GET item (JOB00139) */
        if (pl1i_conversion(613, t, n, i))
            goto again;
        d[0] = '0';                       /* NOCONVERSION: the value undefined */
        *q = 0;
        *neg = 0;
        return 1;
    }
    if (!point && implied)
        *q = implied;
    if (nd == 0) {
        d[nd++] = '0';
    }
    return nd;
}

/* p the precision, plus 256 times the scale of a scaled binary: the
 * value times 2**scale, truncated (T187SGT) */
int pl1_get_fixbin(void *target, int p)
{
    char t[260], d[40];
    int quoted, implied, q, neg, n = get_num(t, sizeof t, &quoted, &implied);
    long long v = 0;
    int nd, qb = p >> 8;

    p &= 255;
    if (n < 0)
        return 1;
    nd = parse_number(t, n, d, &q, &neg, implied);
    for (int i = 0; i < nd - q; i++)
        v = v * 10 + (d[i] - '0');
    if (qb) {
        /* the fraction's bits: its digits doubled, the carry each time */
        char f[72];
        int nf = 0;
        for (int i = nd; i < q && nf < 40; i++)
            f[nf++] = 0;                /* 1E-5: zeros before the digits */
        for (int i = nd - q < 0 ? 0 : nd - q; i < nd; i++)
            f[nf++] = (char)(d[i] - '0');
        for (int b = 0; b < qb; b++) {
            int c = 0;
            for (int i = nf - 1; i >= 0; i--) {
                int x = 2 * f[i] + c;
                f[i] = (char)(x % 10);
                c = x / 10;
            }
            v = 2 * v + c;
        }
    }
    if (neg)
        v = -v;
    if (p <= 15) {
        short h = (short)v;
        memcpy(target, &h, 2);
    } else {
        int w = (int)v;
        memcpy(target, &w, 4);
    }
    return 0;
}

int pl1_get_fixdec(void *target, int p, int q)
{
    char t[260], d[40], dig[40];
    int quoted, implied, sq, neg, n = get_num(t, sizeof t, &quoted, &implied);
    int nd, k, nb = p / 2 + 1;
    unsigned char *b = target;

    if (n < 0)
        return 1;
    nd = parse_number(t, n, d, &sq, &neg, implied);
    /* to scale q, truncated; then the rightmost p digits */
    k = 0;
    for (int i = 0; i < nd - sq + q && i < 39; i++)
        dig[k++] = i < nd ? d[i] : '0';
    if (k == 0)
        dig[k++] = '0';
    memset(b, 0, (size_t)nb);
    for (int i = k - 1, pos = 2 * nb - 2; i >= 0 && pos >= 0; i--, pos--) {
        int v = dig[i] - '0';
        if (pos % 2 == 0)
            b[pos / 2] |= (unsigned char)(v << 4);
        else
            b[pos / 2] |= (unsigned char)v;
    }
    {
        int zero = 1;
        for (int i = 0; i < k; i++)
            if (dig[i] != '0')
                zero = 0;
        b[nb - 1] |= (unsigned char)((neg && !zero) ? 0x0D : 0x0C);
    }
    return 0;
}

/* A LIST item without quotes is an arithmetic constant: PL/I (F) cannot
 * take WXYZ for a CHARACTER target (IHE604I, CONVERSION; JOB00106). */
static void check_unquoted(char *t, int n, int quoted)
{
    if (quoted || mode != M_GETLIST)
        return;
    for (int i = 0; i < n; i++)
        if (!((t[i] >= '0' && t[i] <= '9') || t[i] == '.' || t[i] == '-' || t[i] == '+')) {
            if (!pl1i_conversion(613, t, n, i))
                return;
            i = -1;                       /* changed by the on-unit: again */
        }
}

/* GET into FLOAT: an E field that is blank raises CONVERSION (607), an
 * F field that is blank is 0 (JOB00157). */
int pl1_get_float(void *target, int longf)
{
    char t[260];
    int quoted, implied, n = get_num(t, sizeof t, &quoted, &implied);
    if (n < 0)
        return 1;
    pl1i_parse_float(t, n, implied, last_kind == K_E || last_kind == K_F ? 607 : 613,
                     last_kind == K_E, longf, target);
    return 0;
}

/* GET DATA: name=value pairs up to the semicolon.  The table has an entry
 * of six words for each name the statement lists (gen.c): A(AL1(n),
 * C'qualified name'), the address of the variable (its first element),
 * kind << 16 | p (kind as the compiler's enum tkind), q << 16 | ndims,
 * lb << 16 | hb, and the bytes from one element to the next. */
static int data_char(struct pfile *f)
{
    for (;;) {
        if (!f->have || f->rpos >= f->rlen) {
            if (!next_record(f))
                return -1;
            continue;
        }
        return (unsigned char)f->rec[f->rpos];
    }
}

static int get_typed(void *target, int kind, int p, int q)
{
    switch (kind) {
    case 1: return pl1_get_fixbin(target, p | q << 8);
    case 2: return pl1_get_fixdec(target, p, q);
    case 3: return pl1_get_char(target, p);
    case 4: return pl1_get_varchar(target, p);
    case 5: return pl1_get_bit(target, p);
    case 6: return pl1_get_float(target, p > 6);
    case 7: return pl1_get_float(target, p > 21);
    case 16: case 17: case 18: case 19: {
        /* COMPLEX FIXED BINARY, FIXED DECIMAL, FLOAT DECIMAL, FLOAT
         * BINARY (enum tkind): the parts, each as the real kind */
        int rk = kind == 16 ? 1 : kind == 17 ? 2 : kind == 18 ? 6 : 7;
        int sz = rk == 1 ? (p <= 15 ? 2 : 4) : rk == 2 ? p / 2 + 1 :
                 (rk == 6 ? p > 6 : p > 21) ? 8 : 4;
        pl1_get_cpx();
        if (get_typed(target, rk, p, q))
            return 1;
        return get_typed((char *)target + sz, rk, p, q);
    }
    default: return 1;
    }
}

/* The data field from fstart of the record frec to the position now,
 * and its delimiter, for DATAFIELD (Q1NAME). */
static void field_end(struct pfile *f, const char *frec, int fstart)
{
    int end = f->rpos;
    if (f->rec != frec || end < fstart) {
        frec = f->rec;                    /* a field across records: its end */
        fstart = 0;
    }
    if (end < f->rlen && (f->rec[end] == ',' || f->rec[end] == ' ' || f->rec[end] == ';'))
        end++;
    pl1i_datafield(frec + fstart, end - fstart);
    if (pl1i_copy)
        pl1i_copyline(frec + fstart, end - fstart);   /* COPY: the field (Q1IO) */
}

int pl1_get_data(const int *table, int n)
{
    struct pfile *f = in_file();
    int c, rc = 0;

    data_mode = 1;
    for (;;) {
        char name[64];
        int k = 0, sub = 0, hassub = 0, found = -1, fstart;
        const char *frec;
        /* blanks and commas before a name; ';' ends the statement */
        while ((c = data_char(f)) == ' ' || c == ',')
            f->rpos++;
        if (c < 0) {
            rc = 1;
            break;
        }
        if (c == ';') {
            f->rpos++;
            break;
        }
        frec = f->rec;
        fstart = f->rpos;
        while ((c = data_char(f)) >= 0 && c != '=' && c != '(' && c != ' ' && c != ';') {
            if (k < 63)
                name[k++] = (char)c;
            f->rpos++;
        }
        name[k] = '\0';
        while ((c = data_char(f)) == ' ')
            f->rpos++;
        if (c == '(') {
            int neg = 0;
            f->rpos++;
            hassub = 1;
            while ((c = data_char(f)) == ' ')
                f->rpos++;
            if (c == '-') { neg = 1; f->rpos++; }
            while ((c = data_char(f)) >= '0' && c <= '9') {
                sub = sub * 10 + (c - '0');
                f->rpos++;
            }
            while ((c = data_char(f)) == ' ' || c == ')')
                f->rpos++;
            if (neg)
                sub = -sub;
        }
        while ((c = data_char(f)) == ' ')
            f->rpos++;
        if (c == '=')
            f->rpos++;
        /* the entry: its name fully qualified -- PL/I (F) does not take
         * R.Y as Y (JOB00205) */
        for (int i = 0; i < n && found < 0; i++) {
            const unsigned char *nm = (const unsigned char *)(intptr_t)table[6 * i];
            if (nm[0] == k && memcmp(nm + 1, name, (size_t)k) == 0)
                found = i;
        }
        if (found < 0) {
            /* NAME: the field is read, then the condition raised -- its
             * system action a message, the field skipped; on a STRING,
             * ERROR (IHE027I, JOB00205) */
            char t[260];
            int qd, bt;
            if (f->isstring)
                pl1i_errmsg("IHE027I GET STRING - UNRECOGNISABLE DATA NAME");
            if (list_item(t, sizeof t, &qd, &bt) < 0) {
                rc = 1;
                break;
            }
            field_end(f, frec, fstart);
            pl1i_filecond(C_NAME, 10, f->desc);
            data_mode = 1;            /* a PUT in the on-unit reset it */
            continue;
        }
        {
            const int *e = table + 6 * found;
            char *addr = (char *)(intptr_t)e[1];
            int kind = (unsigned)e[2] >> 16, p = e[2] & 0xFFFF;
            int q = (short)((unsigned)e[3] >> 16), nd = e[3] & 0xFFFF;
            int lb = (short)((unsigned)e[4] >> 16), hb = (short)(e[4] & 0xFFFF);
            if (nd && hassub && sub >= lb && sub <= hb)
                addr += (sub - lb) * e[5];
            pl1_icount++;             /* COUNT */
            if (get_typed(addr, kind, p, q)) {
                /* the end of the file after name=: ERROR (JOB00207) */
                char m[96];
                snprintf(m, sizeof m, "IHE038I FILE %s - ENDFILE FOUND UNEXPECTEDLY IN MIDDLE "
                         "OF DATA ITEM", f->dd);
                pl1i_errmsg(m);
            }
            field_end(f, frec, fstart);
        }
    }
    data_mode = 0;
    return rc;
}

int pl1_get_char(char *target, int len)
{
    char t[260];
    int quoted, implied, n = get_item(t, sizeof t, &quoted, &implied);
    if (n < 0)
        return 1;
    check_unquoted(t, n, quoted);
    pl1_assign_char(target, len, t, n);
    return 0;
}

int pl1_get_varchar(void *target, int maxlen)
{
    char t[260];
    int quoted, implied, n = get_item(t, sizeof t, &quoted, &implied);
    if (n < 0)
        return 1;
    check_unquoted(t, n, quoted);
    pl1_assign_varchar(target, maxlen, t, n);
    return 0;
}

/* BIT VARYING: the field's bits, as many as fit (Q1BFMT) */
int pl1_get_vbit(void *target, int maxbits)
{
    char t[260];
    int quoted, implied, n = get_item(t, sizeof t, &quoted, &implied);
    unsigned char *b = target;
    if (n < 0)
        return 1;
    if (n > maxbits)
        n = maxbits;
    memset(b, 0, (size_t)(2 + (maxbits + 7) / 8));
    b[0] = (unsigned char)(n >> 8);
    b[1] = (unsigned char)n;
    for (int i = 0; i < n; i++)
        if (t[i] == '1')
            b[2 + i / 8] |= (unsigned char)(0x80 >> (i % 8));
    return 0;
}

int pl1_get_bit(void *target, int nbits)
{
    char t[260];
    int quoted, implied, n = get_item(t, sizeof t, &quoted, &implied);
    unsigned char *b = target;
    if (n < 0)
        return 1;
    memset(b, 0, (size_t)((nbits + 7) / 8));
    for (int i = 0; i < n && i < nbits; i++)
        if (t[i] == '1')
            b[i / 8] |= (unsigned char)(0x80 >> (i % 8));
    return 0;
}

