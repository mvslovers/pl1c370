/* pl1cons.c -- the run time's messages when SYSPRINT has no DD.
 *
 * PL/I (F) writes them on the console then: a WTO with route code 11
 * and descriptor code 7 (GC28-6594-7, "Communication with Program
 * during Execution"), the job name after the message number, the text
 * in lines of 72 characters (IHE150I in JOB02085, #220).  A member of
 * its own: pl1i_message() refers to it from every program. */
#include <stdio.h>
#include <string.h>
#include "pl1rt.h"
#include "pl1int.h"

#ifndef __MVS__
/* the host tests: standard error */
void pl1i_console(const char *text)
{
    fprintf(stderr, "%s\n", text);
}
#else
#include <mvs/dd.h>

#define MCS_RD   0x8000                 /* route and descriptor codes given */
#define DESC_7   0x0200                 /* descriptor code 7 */
#define ROUTE_11 0x0020

static void svc35(void *list)
{
    __asm__ __volatile__("L\t1,%0\n\tSVC\t35" : : "m"(list) : "0", "1", "14", "15");
}

static void put_half(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

void pl1i_console(const char *text)
{
    char m[256];
    unsigned list[(4 + 72 + 4 + 3) / 4];
    unsigned char *w = (unsigned char *)list;
    const char *job = __jobname();
    int n;

    if (strlen(text) > 8 && text[7] == ' ')
        n = snprintf(m, sizeof m, "%.7s %-8.8s %s", text, job ? job : "", text + 8);
    else
        n = snprintf(m, sizeof m, "%s", text);
    if (n > (int)sizeof m - 1)
        n = (int)sizeof m - 1;
    for (int k = 0; k < n; k += 72) {
        int len = n - k > 72 ? 72 : n - k;
        put_half(w, (unsigned)len + 4);
        put_half(w + 2, MCS_RD);
        memcpy(w + 4, m + k, (size_t)len);
        put_half(w + 4 + len, DESC_7);
        put_half(w + 6 + len, ROUTE_11);
        svc35(list);
    }
}
#endif
