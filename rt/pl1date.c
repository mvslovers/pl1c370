/* pl1date.c -- the DATE and TIME built-in functions.
 *
 * DATE is yymmdd, TIME hhmmssttt (GC28-8201, Section G), from the
 * TIME macro (rt/pl1time.asm): the date comes as 0CYYDDDF, the time
 * as HHMMSSth, so the milliseconds are the hundredths times ten,
 * the increments of this machine.  Both are characters in a buffer
 * of their own until the next reference. */
#include "pl1rt.h"

void pl1_tod(int out[2]) asm("PL1TOD");

static char date_buf[6], time_buf[9];

/* the decimal digit d of a packed word, d = 0 the leftmost */
static int digit(unsigned w, int d)
{
    return (int)((w >> (28 - 4 * d)) & 0xF);
}

static char *pl1_date_of(unsigned w);

char *pl1_date(void)
{
    int t[2];
    pl1_tod(t);
    return pl1_date_of((unsigned)t[1]);
}

/* yymmdd of a date 0CYYDDDF */
static char *pl1_date_of(unsigned w)
{
    static const int mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int yy, ddd, m = 0;

    yy = digit(w, 2) * 10 + digit(w, 3);
    ddd = digit(w, 4) * 100 + digit(w, 5) * 10 + digit(w, 6);
    while (m < 11) {
        int n = mdays[m] + (m == 1 && yy % 4 == 0);
        if (ddd <= n)
            break;
        ddd -= n;
        m++;
    }
    m++;
    date_buf[0] = (char)('0' + yy / 10);
    date_buf[1] = (char)('0' + yy % 10);
    date_buf[2] = (char)('0' + m / 10);
    date_buf[3] = (char)('0' + m % 10);
    date_buf[4] = (char)('0' + ddd / 10);
    date_buf[5] = (char)('0' + ddd % 10);
    return date_buf;
}

char *pl1_time(void)
{
    int t[2];
    unsigned w;

    pl1_tod(t);
    w = (unsigned)t[0];
    /* HHMMSSth, then a zero for the milliseconds */
    for (int i = 0; i < 8; i++)
        time_buf[i] = (char)('0' + digit(w, i));
    time_buf[8] = '0';
    return time_buf;
}

/* DATETIME (PL/I for MVS & VM, SC26-3114-01): yyyymmddhhmmssttt from one
 * TIME macro, so date and time belong together; the century from the
 * C digit of 0CYYDDDF (0: 19, 1: 20) */
char *pl1_datetime(void)
{
    static char dt_buf[17];
    int t[2];
    unsigned w;
    char *d;

    pl1_tod(t);
    d = pl1_date_of((unsigned)t[1]);
    w = (unsigned)t[1];
    dt_buf[0] = (char)('1' + (digit(w, 1) >= 1));
    dt_buf[1] = (char)(digit(w, 1) >= 1 ? '0' : '9');
    for (int i = 0; i < 6; i++)
        dt_buf[2 + i] = d[i];
    w = (unsigned)t[0];
    for (int i = 0; i < 8; i++)
        dt_buf[8 + i] = (char)('0' + digit(w, i));
    dt_buf[16] = '0';
    return dt_buf;
}
