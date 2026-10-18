/* Host test: scaled binary conversions against values PL/I (F) printed
 * (the probe P4DIV, JOB00071). */
#include <stdio.h>
#include <string.h>
#include "../../rt/pl1rt.h"

static int fails;

static void fmt(int n, int p, int q, const char *want)
{
    char b[64];
    int w = pl1_format_fixbin(b, n, p, q);
    b[w] = '\0';
    if (strcmp(b, want) != 0) {
        printf("FAIL fixbin n=%d p=%d q=%d: [%s] want [%s]\n", n, p, q, b, want);
        fails++;
    }
}

static void btod(int n, int q, int qd, const char *want_hex)
{
    unsigned char d[8];
    char hex[17];
    pl1_bin_to_dec(n, q, qd, d);
    for (int i = 0; i < 8; i++)
        sprintf(hex + 2 * i, "%02X", d[i]);
    if (strcmp(hex, want_hex) != 0) {
        printf("FAIL btod n=%d q=%d qd=%d: %s want %s\n", n, q, qd, hex, want_hex);
        fails++;
    }
}

int main(void)
{
    /* I/J = 7/2, FIXED BINARY(31,16): "       3.50000" (14 wide) */
    fmt((7 << 16) / 2, 31, 16, "       3.50000");
    fmt(-(7 << 16) / 2, 31, 16, "      -3.50000");
    /* J/K = 2/1000 at scale 16: trunc(131072/1000) = 131 -> 0.00199 */
    fmt(131072 / 1000, 31, 16, "       0.00199");
    /* -7/3 at scale 16: -152917 -> -2.33332 */
    fmt(-152917, 31, 16, "      -2.33332");
    /* D50/J = 100/2 at scale 13: 409600 -> 50.0000 */
    fmt(409600, 31, 13, "       50.0000");
    fmt(142, 31, 0, "           142");
    /* -3.5 at scale 16 to 2 decimal places: -3.50 -> 000000000000350D */
    btod(-229376, 16, 2, "000000000000350D");
    btod(229376, 16, 5, "000000000350000C");
    btod(0, 16, 5, "000000000000000C");
    if (fails == 0)
        printf("t_scaled: all passed\n");
    return fails != 0;
}
