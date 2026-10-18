/* t_float.c -- the exact conversions against PL/I (F)'s output
 * (JOB00156-160): values assigned from constants and
 * printed by PUT LIST, no arithmetic in between. */
#include <stdio.h>
#include <string.h>

#include "../../rt/pl1flt.h"
#include "../../rt/pl1int.h"

static int failed;

/* digits/exponent as written: "314159" e-5 is 3.14159 */
static void list(const char *dig, int e10, int neg, int longf, int p, const char *want)
{
    unsigned char v[8];
    char field[48];
    int w;
    pl1f_constant(dig, (int)strlen(dig), e10, neg, longf, v);
    w = pl1i_flt_field(field, v, longf, p);
    field[w] = '\0';
    if (strcmp(field + w - (int)strlen(want), want) != 0 || (w > (int)strlen(want) &&
        field[w - strlen(want) - 1] != ' ')) {
        printf("FAIL %s e%d: got '%s' want '%s'\n", dig, e10, field, want);
        failed++;
    }
}

static void bin(long long iv, int longf_from, const char *want)
{
    unsigned char s[8], l[8];
    char field[48];
    int w;
    pl1f_from_binary(iv, 0, longf_from, s);
    memset(l, 0, 8);
    memcpy(l, s, longf_from ? 8 : 4);
    w = pl1i_flt_field(field, l, 1, 16);
    field[w] = '\0';
    if (strstr(field, want) == NULL) {
        printf("FAIL bin %lld: got '%s' want '%s'\n", iv, field, want);
        failed++;
    }
}

static void hex(const unsigned char *v, char *h)
{
    for (int i = 0; i < 8; i++)
        sprintf(h + 2 * i, "%02X", v[i]);
}

/* the bits of a constant (compiler) or a string converted (run time) */
static void bits(int constant, const char *m, int e10, const char *want)
{
    unsigned char v[8];
    char h[17];
    if (constant)
        pl1f_constant(m, (int)strlen(m), e10, 0, 1, v);
    else
        pl1f_from_decimal(m, (int)strlen(m), e10, 0, 1, v);
    hex(v, h);
    if (strcmp(h, want) != 0) {
        printf("FAIL %s %se%d: got %s want %s\n", constant ? "constant" : "string", m, e10, h,
               want);
        failed++;
    }
}

/* PUT LIST of a long float given by its bits */
static void print_bits(const char *h, const char *want)
{
    unsigned char v[8];
    char field[48];
    int w;
    for (int i = 0; i < 8; i++) {
        unsigned int x;
        sscanf(h + 2 * i, "%2x", &x);
        v[i] = (unsigned char)x;
    }
    w = pl1i_flt_field(field, v, 1, 16);
    field[w] = '\0';
    if (strstr(field, want) == NULL) {
        printf("FAIL print %s: got '%s' want '%s'\n", h, field, want);
        failed++;
    }
}

/* 1.1 ** n printed with p digits (JOB00182, G3POW) */
static void power(int longf, int n, int p, const char *want)
{
    unsigned char x[8], r[8];
    char field[48];
    int w;
    memset(x, 0, 8);
    pl1f_constant("11", 2, -1, 0, longf, x);
    pl1f_power(x, longf, n, r);
    w = pl1i_flt_field(field, r, longf, p);
    field[w] = '\0';
    if (strstr(field, want) == NULL) {
        printf("FAIL 1.1**%d: got '%s' want '%s'\n", n, field, want);
        failed++;
    }
}

int main(void)
{
    power(0, 2, 6, "1.20999E+00");
    power(0, 10, 6, "2.59372E+00");
    power(1, 10, 16, "2.593742460099993E+00");
    power(1, 0, 16, "1.000000000000000E+00");
    power(0, -2, 6, "8.26447E-01");
    power(0, 3, 6, "1.33099E+00");
    power(1, 3, 16, "1.330999999999999E+00");
    /* JOB00159, JOB00160: constants and strings, as UNSPEC shows them */
    bits(1, "123456789012345", -51, "222A029A65112875");
    bits(1, "7", 45, "67139E40F23AF10A");
    bits(1, "3", -25, "2C5CD872E18F7989");
    bits(1, "1", 25, "5584595161401484");
    bits(1, "17", -16, "347A7F770558495F");
    bits(1, "123", 30, "5B6107A6F5811DA2");
    bits(1, "45", -73, "057F36866CD27C78");
    bits(1, "987654321", -63, "1418379B4D90DCE0");
    bits(1, "2", 75, "7F46BF5BB0385043");
    bits(1, "1", -70, "06B0AF48EC79ACDF");
    bits(1, "99999995", -7, "419FFFFF79C842FA");
    bits(1, "1", -1, "4019999999999999");
    bits(1, "314159265358979", -14, "413243F6A8885A21");
    bits(0, "123456789012345", -51, "222A029A65112875");
    bits(0, "7", 45, "67139E40F23AF10A");
    bits(0, "3", -25, "2C5CD872E18F7987");
    /* JOB00159: long floats set by UNSPEC, printed */
    print_bits("419FFFFF79C842FA", "9.999999499999998E+00");
    print_bits("4110000000000000", "1.000000000000000E+00");
    print_bits("411FFFFFFFFFFFFF", "1.999999999999999E+00");
    print_bits("40FFFFFFFFFFFFFF", "9.999999999999998E-01");
    print_bits("4E2386F26FC10000", "1.000000000000000E+16");
    list("1", 0, 0, 0, 6, "1.00000E+00");
    list("1", 0, 0, 1, 16, "1.000000000000000E+00");
    list("1", 0, 0, 0, 1, "1E+00");
    list("314159", -5, 0, 0, 6, "3.14158E+00");
    list("314159265358979", -14, 0, 1, 16, "3.141592653589789E+00");
    list("123456", -9, 1, 0, 6, "-1.23455E-04");
    list("123456", -9, 1, 1, 16, "-1.234559999999999E-04");
    list("123456789", 0, 0, 0, 6, "1.23456E+08");
    list("123456789", 0, 0, 1, 16, "1.234567890000000E+08");
    list("0", 0, 0, 0, 6, "0.00000E+00");
    list("1", 70, 0, 0, 6, "9.99999E+69");
    list("1", -70, 0, 1, 16, "9.999999999999998E-71");
    list("25", 9, 0, 0, 6, "2.49999E+10");
    list("25", 9, 0, 0, 2, "2.4E+10");
    list("125", -4, 0, 0, 3, "1.24E-02");
    list("1", -1, 0, 0, 6, "9.99999E-02");
    list("1", -1, 0, 1, 16, "9.999999999999999E-02");
    list("1", -1, 0, 0, 7, "9.999996E-02");
    list("9999995", -1, 0, 0, 6, "9.99999E+05");
    list("99999995", -7, 0, 1, 16, "9.999999499999998E+00");
    list("33333333333333", -14, 0, 1, 16, "3.333333333333299E-01");
    bin(2147483647, 0, "2.147483392000000E+09");
    bin(16777217, 0, "1.677721600000000E+07");
    bin(-16777219, 0, "-1.677721600000000E+07");
    if (failed)
        return 1;
    puts("t_float: all passed");
    return 0;
}
