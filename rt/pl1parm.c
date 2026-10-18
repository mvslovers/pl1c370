/* pl1parm.c -- the parameter of a main procedure.
 *
 * PL/I (F) passes a main procedure the PARM as a string: IHESAPA takes
 * the first word of the list R1 points to as the address of the PARM
 * area, a halfword length and then the characters, and passes a string
 * dope vector of it -- the characters' address (the area + 2), the
 * maximum length 100 and the current length.  A CHAR(100) VARYING
 * parameter is the PARM; a parameter of another type lies over the
 * vector, so a POINTER gets the area's address + 2 (JOB01232, JOB01233,
 * internals/research/pli/main-parameter-measurement.md).
 *
 * libc370's startup hands the area to __premain() before main(); this
 * member is linked only by a main procedure that has a parameter, which
 * refers to PL1MPRM. */
#include <stddef.h>
#include <string.h>

#include "pl1rt.h"

static char *parm_area;
static void **r1_list;

int __premain(char *parm, char *pgmname, void **pgmr1)
{
    (void)pgmname;
    parm_area = parm;
    r1_list = pgmr1;                /* R1 at the program's entry */
    return 0;
}

/* The argument list of a main procedure that takes R1 unchanged, as
 * PL/I (F)'s IHESAPB and IHESAPD pass it (JOB01267): the two words for
 * the run time, then R1's first n words, the end mark dropped. */
void *pl1_main_plist(int n)
{
    static void *list[2 + 16];
    static unsigned vals[16];
    int byvalue = n >> 16;              /* OPTIONS(MAIN BYVALUE): 65536 + n */
    n &= 0xFFFF;
    list[0] = NULL;
    list[1] = NULL;
    for (int i = 0; i < n && i < 16; i++) {
        if (byvalue) {
            /* each word is the parameter's value, as it is (SC26-3114-01:
             * "the parameter list is passed as is") */
            vals[i] = r1_list ? (unsigned)r1_list[i] : 0;
            list[2 + i] = &vals[i];
        } else {
            list[2 + i] = r1_list ? (void *)((unsigned)r1_list[i] & 0x7FFFFFFF) : NULL;
        }
    }
    return list;
}

/* The argument list of the main procedure: two words for the run time
 * (no parent frame, no result), then A(the parameter).  A VARYING
 * parameter (varying 1) is the PARM area itself (halfword length,
 * characters, as PL1C/370 lays out VARYING); any other gets the dope
 * vector.  With varying 2 the PARM is run-time options, '/', the
 * argument, as the Optimizing Compiler and PL/I for MVS & VM take it
 * without NOEXECOPS (SC33-0006-7, SC26-3113-01): the parameter is what
 * follows the first '/', nothing without one; the options are not
 * used. */
void *pl1_main_parm(int varying)
{
    static struct { char *chars; short max, cur; } sdv;
    static struct { short len; char c[100]; } arg;
    static void *list[3];
    char *a = parm_area;
    sdv.chars = a + 2;
    sdv.max = 100;
    sdv.cur = a ? (short)(((unsigned char)a[0] << 8) | (unsigned char)a[1]) : 0;
    list[0] = NULL;
    list[1] = NULL;
    list[2] = varying ? (void *)a : (void *)&sdv;
    if (varying == 2) {
        int i = 0;
        while (i < sdv.cur && a[2 + i] != '/')
            i++;
        arg.len = 0;
        if (i < sdv.cur) {
            arg.len = (short)(sdv.cur - i - 1);
            if (arg.len > 100)
                arg.len = 100;
            memcpy(arg.c, a + 3 + i, (size_t)arg.len);
        }
        list[2] = &arg;
    }
    return list;
}
