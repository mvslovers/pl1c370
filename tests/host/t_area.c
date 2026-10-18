/* t_area.c -- allocation in an area against PL/I (F): the control words
 * and offsets of the probe Q5AFRE (JOB00420), the counts of
 * Q2AFIT (JOB00416) and the free element of Q3ALAY (JOB00418). */
#include <stdio.h>
#include <string.h>

#include "../../rt/pl1rt.h"

static unsigned char area[16 + 200 + 8] __attribute__((aligned(8)));
static int fails;

static unsigned word(int i)
{
    const unsigned char *p = area + 4 * i;
    return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3];
}

static void init(unsigned size)
{
    memset(area, 0, sizeof area);
    area[1] = (unsigned char)(size >> 16);
    area[2] = (unsigned char)(size >> 8);
    area[3] = (unsigned char)size;
}

static unsigned alloc(int n)
{
    unsigned char *p = pl1_area_alloc(area, n);
    return p ? (unsigned)(p - area) : 0;
}

static void freeat(unsigned off, int n)
{
    pl1_area_free(area, area + off, n);
}

static void expect(const char *step, unsigned w0, unsigned w1, unsigned w2, const unsigned *o,
                   const unsigned *want)
{
    int bad = word(0) != w0 || word(1) != w1 || word(2) != w2 || word(3) != 0;
    for (int i = 0; i < 6; i++)
        bad |= o[i] != want[i];
    if (bad) {
        fails++;
        printf("FAIL %-9s %08X %u %u %u |", step, word(0), word(1), word(2), word(3));
        for (int i = 0; i < 6; i++)
            printf(" %u", o[i]);
        printf("\n");
    }
}

int main(void)
{
    unsigned o[6];
    static const unsigned first[6] = { 16, 24, 40, 64, 72, 104 },
                          later[6] = { 16, 24, 24, 64, 72, 104 };
    const unsigned F = 0x80000000u | 200;

    init(200);
    o[0] = alloc(8); o[1] = alloc(16); o[2] = alloc(24);
    o[3] = alloc(8); o[4] = alloc(32); o[5] = alloc(8);
    expect("SIX", 200, 112, 0, o, first);
    freeat(o[1], 16);    expect("FREE 2", F, 112, 24, o, first);
    freeat(o[3], 8);     expect("FREE 4", F, 112, 24, o, first);
    o[3] = alloc(8);     expect("ALLOC 8", F, 112, 24, o, first);
    freeat(o[2], 24);    expect("FREE 3", F, 112, 24, o, first);
    o[2] = alloc(12);    expect("ALLOC 12", F, 112, 40, o, later);
    freeat(o[5], 8);     expect("FREE 6", F, 104, 40, o, later);
    freeat(o[4], 32);    expect("FREE 5", F, 72, 40, o, later);
    freeat(o[3], 8);     expect("FREE 4", F, 40, 12, o, later);
    freeat(o[2], 12);    expect("FREE 3", F, 24, 12, o, later);
    freeat(o[0], 8);     expect("FREE 1", F, 16, 12, o, later);
    o[0] = alloc(8);     expect("ALLOC 8", F, 24, 12, o, later);

    /* Q2AFIT: allocations of n bytes in an AREA(200) (the AREA
     * condition, with no on-unit, would end the program: stop before) */
    {
        static const int n[] = { 1, 8, 9, 16, 17, 24, 25, 100 },
                         want[] = { 25, 25, 12, 12, 8, 8, 6, 1 };
        for (int i = 0; i < 8; i++) {
            int k = 0;
            init(200);
            while (word(1) == 0 || word(1) + ((n[i] + 7) & ~7) <= 216) {
                alloc(n[i]);
                k++;
            }
            if (k != want[i]) {
                fails++;
                printf("FAIL fit %d: %d, want %d\n", n[i], k, want[i]);
            }
        }
    }

    /* Q3ALAY: the free element in an AREA(64): length, then 12 */
    init(64);
    o[0] = alloc(8); o[1] = alloc(8); o[2] = alloc(8);
    freeat(o[1], 8);
    if (word(0) != (0x80000000u | 64) || word(1) != 40 || word(2) != 24 || word(6) != 8 ||
        word(7) != 12) {
        fails++;
        printf("FAIL Q3 FREE2\n");
    }
    printf("t_area: %s\n", fails ? "FAILED" : "all passed");
    return fails != 0;
}
