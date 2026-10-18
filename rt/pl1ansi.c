/* pl1ansi.c -- built-in functions of ANSI X3.53-1976 that PL/I (F) does
 * not have (9.4.4): the string functions AFTER, BEFORE, DECAT, REVERSE,
 * COPY and COLLATE.  Each writes its result to dst, which holds the
 * longest result the compiler allows, and returns the length.
 * Character strings only. */
#include <string.h>
#include "pl1rt.h"
#include "pl1pic.h"

/* the position (from 0) of the leftmost c in s, -1 if none */
static int find(const char *s, int sl, const char *c, int cl)
{
    for (int i = 0; i + cl <= sl; i++)
        if (memcmp(s + i, c, (size_t)cl) == 0)
            return i;
    return -1;
}

/* AFTER(s, c), 9.4.4.5: what follows the leftmost c; s itself for a
 * null c, a null string if c does not occur */
int pl1_after(char *dst, const char *s, int sl, const char *c, int cl)
{
    int i, n;
    if (sl <= 0)
        return 0;
    if (cl <= 0) {
        memcpy(dst, s, (size_t)sl);
        return sl;
    }
    i = find(s, sl, c, cl);
    if (i < 0)
        return 0;
    n = sl - (i + cl);
    memcpy(dst, s + i + cl, (size_t)n);
    return n;
}

/* BEFORE(s, c), 9.4.4.11: what precedes the leftmost c; a null string
 * for a null c, s itself if c does not occur */
int pl1_before(char *dst, const char *s, int sl, const char *c, int cl)
{
    int i;
    if (sl <= 0 || cl <= 0)
        return 0;
    i = find(s, sl, c, cl);
    if (i < 0)
        i = sl;
    memcpy(dst, s, (size_t)i);
    return i;
}

/* DECAT(s, c, p), 9.4.4.25: s split at its leftmost c into the part
 * before, c and the part after; the parts whose bit in p (BIT(3), at pa)
 * is 1, concatenated */
int pl1_decat(char *dst, const char *s, int sl, const char *c, int cl,
              const unsigned char *pa)
{
    const char *part[3];
    int len[3], n = 0;
    if (sl <= 0) {
        len[0] = len[1] = len[2] = 0;
    } else if (cl <= 0) {
        len[0] = len[1] = 0;
        len[2] = sl;
    } else {
        int i = find(s, sl, c, cl);
        if (i < 0) {
            len[0] = sl;
            len[1] = len[2] = 0;
        } else {
            len[0] = i;
            len[1] = cl;
            len[2] = sl - i - cl;
        }
    }
    part[0] = s;
    part[1] = s + len[0];
    part[2] = s + len[0] + len[1];
    for (int k = 0; k < 3; k++)
        if (pa[0] & (0x80 >> k)) {
            memcpy(dst + n, part[k], (size_t)len[k]);
            n += len[k];
        }
    return n;
}

/* REVERSE(s), 9.4.4.67 */
int pl1_reverse(char *dst, const char *s, int sl)
{
    for (int i = 0; i < sl; i++)
        dst[i] = s[sl - 1 - i];
    return sl > 0 ? sl : 0;
}

/* COPY(s, n), 9.4.4.20: n copies of s (REPEAT(s, n) has n + 1), at most
 * max characters -- the length the compiler allows for a count that is
 * not a constant */
int pl1_copy(char *dst, const char *s, int sl, int n, int max)
{
    int len = 0;
    for (int k = 0; k < n && len + sl <= max; k++) {
        memcpy(dst + len, s, (size_t)sl);
        len += sl;
    }
    return len;
}

/* COLLATE, 9.4.4.17: every character once, in the collating sequence --
 * here all 256 of EBCDIC, the language characters and the
 * implementation's extralingual ones, in the order of their codes */
char *pl1_collate(void)
{
    static char t[256];
    if (!t[255])
        for (int i = 0; i < 256; i++)
            t[i] = (char)i;
    return t;
}

/* VALID(x), 9.4.4.86: whether the characters s of a picture variable
 * are a value of its picture.  A numeric picture: the value they give,
 * edited by the picture again, is the same characters -- the field
 * PL/I (F)'s editing would write.  A character picture: each position
 * holds what its picture character allows (X anything, A a letter or a
 * blank, 9 a digit or a blank).  '1'B or '0'B to dst. */
void pl1_valid(unsigned char *dst, const char *s, int sl, const char *pic, int pn,
               int numeric)
{
    int ok = 1;
    if (numeric) {
        unsigned char val[8];
        char again[260];
        int n = pl1_pic_length(pic, pn);
        if (n != sl || n > (int)sizeof again) {
            ok = 0;
        } else {
            pl1_pic_value(s, pic, pn, val);
            pl1_pic_edit(val, pic, pn, again);
            ok = memcmp(again, s, (size_t)n) == 0;
        }
    } else {
        for (int i = 0; i < sl && i < pn && ok; i++) {
            char c = s[i], p = pic[i];
            if (p == 'A')
                ok = c == ' ' || (c >= 'A' && c <= 'I') || (c >= 'J' && c <= 'R') ||
                     (c >= 'S' && c <= 'Z');
            else if (p == '9')
                ok = c == ' ' || (c >= '0' && c <= '9');
        }
    }
    dst[0] = ok ? 0x80 : 0;
}

/* ---- the same functions of bit strings (Tier 69) ----------------------
 * A bit string at a, l bits from the first bit of a's first byte.  The
 * bits are spread to one character each, the character function does
 * the work, and the result is packed again to dst; the length in bits
 * comes back.  At most MAXB bits, as the compiler's longest. */
#define MAXB 4096

static char ub[3][MAXB];

static int unpack(char *u, const unsigned char *a, int l)
{
    if (l > MAXB)
        l = MAXB;
    for (int i = 0; i < l; i++)
        u[i] = (char)((a[i / 8] >> (7 - i % 8)) & 1);
    return l > 0 ? l : 0;
}

static int pack(unsigned char *dst, const char *u, int l)
{
    for (int i = 0; i < (l + 7) / 8; i++)
        dst[i] = 0;
    for (int i = 0; i < l; i++)
        if (u[i])
            dst[i / 8] |= (unsigned char)(0x80 >> (i % 8));
    return l;
}

int pl1_after_b(unsigned char *dst, const unsigned char *s, int sl, const unsigned char *c,
                int cl)
{
    int n1 = unpack(ub[0], s, sl), n2 = unpack(ub[1], c, cl);
    return pack(dst, ub[2], pl1_after(ub[2], ub[0], n1, ub[1], n2));
}

/* INDEX(s, c) of two bit strings: the bit position of c in s */
int pl1_index_b(const unsigned char *s, int sl, const unsigned char *c, int cl)
{
    int n1 = unpack(ub[0], s, sl), n2 = unpack(ub[1], c, cl);
    return pl1_index(ub[0], n1, ub[1], n2);
}

int pl1_before_b(unsigned char *dst, const unsigned char *s, int sl, const unsigned char *c,
                 int cl)
{
    int n1 = unpack(ub[0], s, sl), n2 = unpack(ub[1], c, cl);
    return pack(dst, ub[2], pl1_before(ub[2], ub[0], n1, ub[1], n2));
}

int pl1_decat_b(unsigned char *dst, const unsigned char *s, int sl, const unsigned char *c,
                int cl, const unsigned char *pa)
{
    int n1 = unpack(ub[0], s, sl), n2 = unpack(ub[1], c, cl);
    return pack(dst, ub[2], pl1_decat(ub[2], ub[0], n1, ub[1], n2, pa));
}

int pl1_reverse_b(unsigned char *dst, const unsigned char *s, int sl)
{
    int n = unpack(ub[0], s, sl);
    return pack(dst, ub[2], pl1_reverse(ub[2], ub[0], n));
}

int pl1_copy_b(unsigned char *dst, const unsigned char *s, int sl, int n, int max)
{
    int k = unpack(ub[0], s, sl);
    return pack(dst, ub[2], pl1_copy(ub[2], ub[0], k, n, max > MAXB ? MAXB : max));
}
