/* t_pic.c -- picture editing against PL/I (F)'s output: the pictures,
 * values and edited strings of the probe Q1PIC (JOB00288),
 * each value assigned from FIXED DECIMAL(15,5). */
#include <stdio.h>
#include <string.h>

#include "../../rt/pl1pic.h"

static const struct { const char *pic, *value, *want; } cases[] = {
    {"99999", "12345", "12345"},
    {"99999V", "12345", "12345"},
    {"999V99", "12345", "34500"},
    {"V99999", "12345", "00000"},
    {"99999", "1234567", "34567"},
    {"99999", "123", "00123"},
    {"999V99", "123.45", "12345"},
    {"9V9", "12345.67", "56"},
    {"99999", "123.45", "00123"},
    {"ZZZ99", "12345", "12345"},
    {"ZZZ99", "00100", "  100"},
    {"ZZZ99", "00000", "   00"},
    {"ZZZZZ", "00100", "  100"},
    {"ZZZZZ", "00000", "     "},
    {"ZZZ99", "1.23", "   01"},
    {"ZZZV99", "1.23", "  123"},
    {"ZZZV99", "12345", "34500"},
    {"ZZZVZZ", "0", "     "},
    {"*****", "100", "**100"},
    {"*****", "0", "*****"},
    {"***V**", "0.01", "***01"},
    {"YYYYY", "100", "  1  "},
    {"9Y9Y9", "10203", "1 2 3"},
    {"9,999", "1234", "1,234"},
    {"9,999V.99", "1234.56", "1,234.56"},
    {"ZZ.VZZ", "12.34", "12.34"},
    {"ZZ.VZZ", "0.03", "   03"},
    {"ZZV.ZZ", "0.03", "  .03"},
    {"ZZV.ZZ", "12.34", "12.34"},
    {"ZZV.ZZ", "0", "     "},
    {"9,999,999.V99", "1234567.89", "1,234,567.89"},
    {"**,999V.99", "12345.67", "12,345.67"},
    {"**,999V.99", "123.45", "***123.45"},
    {"9.999.999V,99", "1234567.89", "1.234.567,89"},
    {"99/99/99", "123456", "12/34/56"},
    {"99.9/99.9", "123456", "12.3/45.6"},
    {"ZZ/ZZ/ZZ", "1234", "   12/34"},
    {"ZZ/ZZ/ZZ", "12", "      12"},
    {"ZZ/ZZ/ZZ", "0", "        "},
    {"**/**/**", "0", "********"},
    {"99B99B99", "123456", "12 34 56"},
    {"9BB9BB9", "123", "1  2  3"},
    {"9BB/9BB", "12", "1  /2  "},
    {"$999V.99", "123.45", "$123.45"},
    {"$ZZZV.99", "1.23", "$  1.23"},
    {"$ZZZV.ZZ", "0", "       "},
    {"$$$.$$", "0", "      "},
    {"$$$9V.99", "123.45", "$123.45"},
    {"$$$9V.99", "1.23", "  $1.23"},
    {"99$", "12", "12$"},
    {"$$$,999", "12", "   $012"},
    {"$$$,999", "1234", " $1,234"},
    {"S999V.99", "123.45", "+123.45"},
    {"S999V.99", "-123.45", "-123.45"},
    {"+999V.99", "-123.45", " 123.45"},
    {"-999V.99", "123.45", " 123.45"},
    {"999V.99S", "123.45", "123.45+"},
    {"++B+9V.99", "1.23", "   +1.23"},
    {"---9V.99", "1.23", "   1.23"},
    {"SSS9V.99", "-1.23", "  -1.23"},
    {"$Z.99CR", "-1.23", "$  01CR"},
    {"$ZZV.99CR", "12.34", "$12.34  "},
    {"$ZZV.99DB", "-12.34", "$12.34DB"},
    {"$ZZV.99DB", "12.34", "$12.34  "},
    {"999I", "1021", "102A"},
    {"Z99R", "-1021", "102J"},
    {"99T9", "1021", "10B1"},
    {"99T9", "-1021", "10K1"},
    {"999R", "1021", "1021"},
    {"999I", "-1021", "1021"},
    {"999V99", "123.45", "12345"},
    {"ZZZZ9", "7", "    7"},
    {"S99999", "-42", "-00042"},
    {"$$$9V.99", "5.5", "  $5.50"},
    {"ZZZ9", "-25", "  25"},
    {"9999", "-25", "0025"},
    {"--,--9", "-1234", "-1,234"},
    {"$$,$$9V.99", "0.07", "    $0.07"},
    {"ZZZ,ZZ9V.99", "0.5", "      0.50"},
    {"ZZZ,ZZ9V.99", "1234.5", "  1,234.50"},
    {"***,**9V.99", "-12.5", "*****12.50"},
    {"SZZ9", "5", "+  5"},
    {"SZZ9", "-5", "-  5"},
    {"ZZ9-", "-5", "  5-"},
    {"ZZ9+", "5", "  5+"},
    {"ZZ9CR", "5", "  5  "},
    {"9V.999", "0.1234", "0.123"},
    {"V.999", "0.5", ".500"},
    {"ZZZ.ZZ", "0.5", "      "},
    {"ZZZ.99", "0.5", "    00"},
};

/* the packed value of v (a decimal constant) * 10**q, truncated */
static void packed(const char *v, int q, unsigned char *out)
{
    char dig[40];
    int n = 0, neg = 0, frac = -1;
    for (const char *s = v; *s; s++) {
        if (*s == '-') neg = 1;
        else if (*s == '.') frac = 0;
        else {
            if (frac >= 0) {
                if (frac == q) continue;
                frac++;
            }
            dig[n++] = *s;
        }
    }
    for (; (frac < 0 ? 0 : frac) < q; frac = frac < 0 ? 1 : frac + 1)
        dig[n++] = '0';
    memset(out, 0, 8);
    for (int i = 0; i < 15 && i < n; i++) {
        int j = 14 - i, d = dig[n - 1 - i] - '0';
        out[j / 2] |= (unsigned char)(j % 2 ? d : d << 4);
    }
    out[7] |= neg ? 0x0D : 0x0C;
}

int main(void)
{
    int failed = 0;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        unsigned char val[8], back[8];
        const char *pic = cases[i].pic;
        int n = (int)strlen(pic);
        char out[256], out2[256];
        int q = 0, after = 0, len;
        for (const char *p = cases[i].pic; *p; p++) {
            if ((p[0] == 'C' && p[1] == 'R') || (p[0] == 'D' && p[1] == 'B')) p++;
            else if (*p == 'V') after = 1;
            else if (after && strchr("9ZY*TIR", *p)) q++;
        }
        packed(cases[i].value, q, val);
        len = pl1_pic_length(pic, n);
        pl1_pic_edit(val, pic, n, out);
        out[len] = '\0';
        if (strcmp(out, cases[i].want) != 0) {
            printf("FAIL %s %s: got '%s' want '%s'\n", cases[i].pic, cases[i].value, out,
                   cases[i].want);
            failed++;
            continue;
        }
        /* de-editing gives back a value that edits the same */
        pl1_pic_value(out, pic, n, back);
        pl1_pic_edit(back, pic, n, out2);
        out2[len] = '\0';
        if (strcmp(out2, out) != 0) {
            printf("FAIL value of '%s' (%s): edits as '%s'\n", out, cases[i].pic, out2);
            failed++;
        }
    }
    printf("t_pic: %s\n", failed ? "FAILED" : "all passed");
    return failed != 0;
}
