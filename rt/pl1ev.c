/* pl1ev.c -- WAIT on event variables (#3, tiers 0 and 1).
 *
 * The generated code passes a list of (address, number of events
 * there), an array's elements one after another, and the count: how
 * many must be complete, -1 for all.  A count of 0 or less, or one
 * that enough complete events meet, returns at once; a count beyond
 * the events is all of them (t228 Q3EVT).
 *
 * An active event is one a DISPLAY ... REPLY ... EVENT started: the
 * WTOR posts its ECB (PL1_EV_ECB).  WAIT makes a posted one complete,
 * and while too few are complete it waits (SVC 1) for one of the
 * active ones.  An incomplete event that is not active can never
 * complete, since there is one task: if the count cannot be met
 * without such events, IHE550I and ERROR with ONCODE 3900, as PL/I (F)
 * does (Q2EVT). */
#include <stddef.h>
#include "pl1rt.h"
#include "pl1int.h"

#define MAXWAIT 64                      /* active events waited for at once */

struct evref {
    unsigned char *ev;
    int n;
};

static void wait_one(unsigned *list)
{
#ifdef __MVS__
    /* WAIT 1,ECBLIST=: the list's address negated in R1 (the WAIT macro) */
    __asm__ __volatile__("LA\t0,1\n\tL\t1,%0\n\tLCR\t1,1\n\tSVC\t1" : : "m"(list)
                         : "0", "1", "14", "15");
#else
    (void)list;
#endif
}

void pl1_wait(const void *list, int n, int count)
{
    const struct evref *l = list;
    for (;;) {
        int total = 0, done = 0, nact = 0;
        unsigned ecbs[MAXWAIT];
        for (int i = 0; i < n; i++)
            for (int k = 0; k < l[i].n; k++) {
                unsigned char *e = l[i].ev + k * PL1_EV_SIZE;
                total++;
                if ((e[PL1_EV_ACTIVE] & 0x80) && (e[PL1_EV_ECB] & 0x40))
                    pl1i_evraise(e);   /* posted: complete, and an EVENT I/O's condition */
                if (e[PL1_EV_COMPL] & 0x80)
                    done++;
                else if ((e[PL1_EV_ACTIVE] & 0x80) && nact < MAXWAIT)
                    ecbs[nact++] = (unsigned)(size_t)(e + PL1_EV_ECB);
            }
        if (count < 0 || count > total)
            count = total;
        if (count <= 0 || done >= count)
            return;
        if (done + nact < count) {
            pl1i_errmsgc(3900, "IHE550I ATTEMPT TO WAIT ON AN INACTIVE AND INCOMPLETE EVENT");
            return;
        }
        ecbs[nact - 1] |= 0x80000000u;
        wait_one(ecbs);
    }
}
