/* pl1sort.c -- CALL PLISRTA to PLISRTD, the sort program (#233).
 *
 * The program's arguments (SC33-0006-7, chapter 12) arrive in the block
 * of rt/pl1abi.h (PL1_SA_...).  From them PL1SORT builds the parameter
 * list of the sort interface PL/I (F) uses (Tier 107,
 * internals/design/sort-checkpoint.md) and LINKs to SORT
 * (rt/pl1srt.asm):
 *
 *   H'32', A(SORT statement), A(its last byte), A(RECORD statement),
 *   A(its last byte), A(E15) or 0, A(E35) or 0, C'SORT' with the
 *   prefix over it (four characters at most), F(storage)
 *
 * The sort's return code goes into the program's variable unchanged.
 * The sort of MVS 3.8j abends where the Optimizer's manual says 16 or
 * 20 -- a DD missing, U0056 -- and the variable is then never set
 * (JOB02400).  The program's SPIE is off while the sort runs, as with
 * PL/I (F), and on while an exit's procedure runs.
 *
 * The exits, as PL/I (F) runs them (JOB02399-02415):
 *   E15 is called for each record and returns it.  Its code is
 *   PLIRETC's, not reset between calls: 12 gives the record to the
 *   sort, anything else ends the input (8).  A record of TYPE=F is the
 *   result cut or padded with blanks to the record length (the
 *   Optimizer's manual, SC33-0006-7 p. 322; PL/I (F) leaves two bytes
 *   of junk after a VARYING one); of TYPE=V the result's
 *   length, with the RDW in front.
 *   E35 gets each record, in place: TYPE=V without the RDW.  PLIRETC is
 *   0 before each call; the sort always gets 4, and after an 8 the
 *   remaining records are passed by without a call.  At the end of the
 *   data (the record's address 0) the procedure is not called.
 * The record lengths are those of the RECORD statement: for E15 l2,
 * else l1; for E35 of TYPE=F the first of l3, l2, l1 given.
 *
 * Leaving an exit otherwise -- GO TO a block below the sort's caller,
 * STOP, ERROR's end of the program -- abandons the sort, as PL/I (F)
 * does (JOB02411-02413): pl1_goto and the end paths call sort_leave, and
 * PL1SABN (rt/pl1srt.asm) keeps where that was and answers the sort as
 * though the exit had returned.  From then on the exits answer at once
 * without PL/I (E15 8; E35 8 at the end of the data, else 4), so the
 * sort ends; after its LINK the run time goes back to the GO TO or the
 * end, which then runs under the program's own RB.  The return code
 * variable is not set. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pl1rt.h"
#include "pl1int.h"

struct sortlist {
    unsigned short fill;               /* so that the words are aligned */
    unsigned short len;                /* 32: eight words follow */
    const char *sort, *sortend, *rec, *recend;
    void *e15, *e35;
    char prefix[4];
    int storage;
};

extern int pl1_retcode asm("PL1RTCD");  /* PLIRETC's code (pl1cond.c) */
int pl1_srtlink(unsigned *r1) asm("PL1SRTL");
void pl1_srte15(void) asm("PL1SE15");
void pl1_srte35(void) asm("PL1SE35");
void pl1_callproc(void *entry, unsigned *alist) asm("PL1CALP");
int pl1_srtexit(int e35, unsigned *sl, char **r1) asm("PL1SRTX");
void pl1_srtabn(void) asm("PL1SABN");
extern char *pl1_srtsa asm("PL1SRSA");   /* the save area the sort was given */
extern void (*pl1i_sortleave)(void *frame) asm("PL1SRTHK");   /* pl1cond.c */

/* Every frame of the exits lies above the sort's save area: a GO TO to
 * a frame below it, or the end (NULL), leaves the sort */
static void sort_leave(void *frame)
{
    if (!frame || (char *)frame < pl1_srtsa)
        pl1_srtabn();
}

static struct {
    const unsigned *e15, *e35;         /* their entry values */
    int e15ret, e35par;                /* PL1_SA_E15RET, PL1_SA_E35PAR */
    int typev;                         /* TYPE=V */
    int l15, l35;                      /* the record lengths for E15, E35 */
    char *res, *rec;                   /* E15's result, the record made of it */
    int stop;                          /* E35 said 8 */
} srt;

/* TYPE= and LENGTH=(l1,l2,l3,...) of the RECORD statement */
static void record(const char *r, int n, int len[3])
{
    len[0] = len[1] = len[2] = 0;
    srt.typev = 0;
    for (int i = 0; i + 5 < n; i++)
        if (memcmp(r + i, "TYPE=", 5) == 0)
            srt.typev = r[i + 5] == 'V' || r[i + 5] == 'D';
    for (int i = 0; i + 7 < n; i++)
        if (memcmp(r + i, "LENGTH=", 7) == 0) {
            int k = 0;
            i += 7;
            if (i < n && r[i] == '(')
                i++;
            for (; i < n && k < 3 && r[i] != ')' && r[i] != ' '; i++) {
                if (r[i] == ',')
                    k++;
                else if (r[i] >= '0' && r[i] <= '9')
                    len[k] = len[k] * 10 + (r[i] - '0');
            }
            break;
        }
}

int pl1_srtexit(int e35, unsigned *sl, char **r1)
{
    unsigned al[3];
    if (!e35) {
        int n, max = srt.typev ? srt.l15 - 4 : srt.l15;
        const char *d;
        al[0] = srt.e15[1];
        al[1] = (unsigned)(size_t)srt.res;
        pl1_pie_on();
        pl1_callproc((void *)(size_t)srt.e15[0], al);
        pl1_pie_off();
        *r1 = srt.rec;
        if (pl1_retcode != 12)
            return 8;
        if (srt.e15ret & PL1_SA_VARYING) {
            n = *(unsigned short *)srt.res;
            d = srt.res + 2;
        } else {
            n = srt.e15ret & 0xFFFF;
            d = srt.res;
        }
        if (n > max)
            n = max;
        if (srt.typev) {
            if (!(srt.e15ret & PL1_SA_VARYING)) {
                memset(srt.rec + 4 + n, ' ', (size_t)(max - n));
                n = max;
            }
            memcpy(srt.rec + 4, d, (size_t)n);
            srt.rec[0] = (char)((n + 4) >> 8);
            srt.rec[1] = (char)(n + 4);
            srt.rec[2] = srt.rec[3] = 0;
        } else {
            memcpy(srt.rec, d, (size_t)n);
            memset(srt.rec + n, ' ', (size_t)(max - n));
        }
        return 12;
    } else {
        char *rec = (char *)(size_t)sl[0];
        struct { char *a; int l; } desc;
        if (!rec)
            return 8;                  /* the end of the data */
        if (srt.stop)
            return 4;
        ((unsigned char *)sl)[11] = 4;   /* the code the sort gets */
        if (srt.typev) {
            desc.l = ((unsigned char)rec[0] << 8 | (unsigned char)rec[1]) - 4;
            desc.a = rec + 4;
        } else {
            desc.l = srt.l35;
            desc.a = rec;
        }
        if (desc.l > srt.e35par >> 8)
            desc.l = srt.e35par >> 8;      /* the parameter's bound (check.c) */
        al[0] = srt.e35[1];
        al[1] = 0;
        al[2] = (unsigned)(size_t)(srt.e35par & PL1_SA_BYDESC ? (char *)&desc : desc.a);
        pl1_retcode = 0;
        pl1_pie_on();
        pl1_callproc((void *)(size_t)srt.e35[0], al);
        pl1_pie_off();
        if (pl1_retcode == 8)
            srt.stop = 1;
        return 4;
    }
}

void pl1_plisrt(const char *a)
{
    struct sortlist l;
    unsigned w;
    int plen = *(const int *)(a + PL1_SA_PREFIX + 4), rc;
    int slen = *(const int *)(a + PL1_SA_SORT + 4);
    int rlen = *(const int *)(a + PL1_SA_RECORD + 4);

    memset(&l, 0, sizeof l);
    l.len = 32;
    l.sort = *(const char *const *)(a + PL1_SA_SORT);
    l.sortend = l.sort + slen - 1;
    l.rec = *(const char *const *)(a + PL1_SA_RECORD);
    l.recend = l.rec + rlen - 1;
    memcpy(l.prefix, "SORT", 4);
    if (plen > 0)
        memcpy(l.prefix, *(const char *const *)(a + PL1_SA_PREFIX), plen < 4 ? plen : 4);
    l.storage = *(const int *)(a + PL1_SA_STORAGE);
    free(srt.res);                     /* of a sort that was abandoned */
    free(srt.rec);
    memset(&srt, 0, sizeof srt);
    srt.e15 = *(const unsigned *const *)(a + PL1_SA_E15);
    srt.e35 = *(const unsigned *const *)(a + PL1_SA_E35);
    if (srt.e15 || srt.e35) {
        int len[3];
        record(l.rec, rlen, len);
        srt.l15 = len[1] ? len[1] : len[0];
        srt.l35 = len[2] ? len[2] : len[1] ? len[1] : len[0];
    }
    if (srt.e15) {
        srt.e15ret = *(const int *)(a + PL1_SA_E15RET);
        srt.res = malloc((size_t)(srt.e15ret & 0xFFFF) + 2);
        srt.rec = malloc((size_t)srt.l15 + 4);
        if (!srt.res || !srt.rec) {
            fputs("PL1C/370: out of storage for the sort\n", stderr);
            exit(16);
        }
        l.e15 = (void *)pl1_srte15;
    }
    if (srt.e35) {
        srt.e35par = *(const int *)(a + PL1_SA_E35PAR);
        l.e35 = (void *)pl1_srte35;
    }
    w = 0x80000000u | (unsigned)(size_t)&l.len;
#ifdef __MVS__
    pl1_pie_off();
    pl1i_sortleave = sort_leave;
    rc = pl1_srtlink(&w);              /* abandoned: no return here */
    pl1i_sortleave = NULL;
    pl1_pie_on();
#else
    (void)w;
    rc = 16;                           /* the host tests: no sort */
#endif
    free(srt.res);
    free(srt.rec);
    srt.res = srt.rec = NULL;
    **(int *const *)(a + PL1_SA_RETCODE) = rc;
}
