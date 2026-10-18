/* Host test: replay P3LIST (tests/host/P3LIST.pli) against the
 * run time and compare with what PL/I (F) wrote (P3LIST.oracle, JOB00055). */
#include <stdio.h>
#include <string.h>
#include "../../rt/pl1rt.h"

/* Packed decimal of the digits in s ("-0.125" style), p digits, scale q. */
static void pack(unsigned char *out, const char *s, int p, int q)
{
    char d[32];
    int nd = 0, neg = 0, frac = -1, i, nb = p / 2 + 1;
    for (; *s; s++) {
        if (*s == '-') neg = 1;
        else if (*s == '.') frac = 0;
        else { d[nd++] = *s; if (frac >= 0) frac++; }
    }
    if (frac < 0) frac = 0;
    while (frac < q) { d[nd++] = '0'; frac++; }
    char all[32];
    int pad = 2 * nb - 1 - nd;
    for (i = 0; i < pad; i++) all[i] = '0';
    memcpy(all + pad, d, (size_t)nd);
    for (i = 0; i < nb; i++) {
        int hi = all[2 * i] - '0';
        int lo = (2 * i + 1 < 2 * nb - 1) ? all[2 * i + 1] - '0' : (neg ? 0xD : 0xC);
        out[i] = (unsigned char)((hi << 4) | lo);
    }
}

static void s(const char *t) { pl1_list_char(t, (int)strlen(t)); }
static void dec(const char *v, int p, int q) { unsigned char b[16]; pack(b, v, p, q); pl1_list_fixdec(b, p, q); }

int main(void)
{
    unsigned char vv[40];
    s("A"); s("B");
    pl1_skip(1); s("C5="); s("AB CD"); s("CE="); s(" ");
    pl1_skip(1); s("V="); vv[0] = 0; vv[1] = 7; memcpy(vv + 2, "VARYING", 7); pl1_list_varchar(vv);
    s("VE="); vv[1] = 0; pl1_list_varchar(vv); s("END");
    pl1_skip(1); s("CL="); s("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123"); s("NEXT");
    pl1_skip(1); s("H0"); pl1_list_fixbin(0, 15);
    pl1_skip(1); s("H7"); pl1_list_fixbin(7, 15);
    pl1_skip(1); s("HM7"); pl1_list_fixbin(-7, 15);
    pl1_skip(1); s("HMAX"); pl1_list_fixbin(32767, 15);
    pl1_skip(1); s("HMIN"); pl1_list_fixbin(-32767, 15);
    pl1_skip(1); s("W0"); pl1_list_fixbin(0, 31);
    pl1_skip(1); s("W"); pl1_list_fixbin(123456, 31);
    pl1_skip(1); s("WM"); pl1_list_fixbin(-123456, 31);
    pl1_skip(1); s("WMAX"); pl1_list_fixbin(2147483647, 31);
    pl1_skip(1); s("WMIN"); pl1_list_fixbin(-2147483647, 31);
    pl1_skip(1); s("D50 0"); dec("0", 5, 0);
    pl1_skip(1); s("D50"); dec("12345", 5, 0);
    pl1_skip(1); s("D50M"); dec("-42", 5, 0);
    pl1_skip(1); s("D72"); dec("12345.67", 7, 2);
    pl1_skip(1); s("D72M"); dec("-0.05", 7, 2);
    pl1_skip(1); s("D72 0"); dec("0", 7, 2);
    pl1_skip(1); s("D150"); dec("999999999999999", 15, 0);
    pl1_skip(1); s("D33M"); dec("-.125", 3, 3);
    pl1_skip(1); s("D11"); dec("5", 1, 0);
    pl1_skip(1); s("LIT"); dec("42", 2, 0); dec("-3", 1, 0); dec("1.50", 3, 2); s("IT'S");
    pl1_skip(2); s("AFTER SKIP2");
    pl1_skip(0); s("OVERPRINT");
    pl1_skip(1); s("ROW");
    for (int i = 1; i <= 12; i++) pl1_list_fixbin(i, 15);
    pl1_page(); s("NEW PAGE");
    pl1_skip(1); s("LAST");
    pl1_finish();
    return 0;
}
