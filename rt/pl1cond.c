/* pl1cond.c -- conditions and on-units of PL1C/370's run time.
 *
 * Measured against PL/I (F) V5.5 (internals/research/pli/
 * tier6-measurement.md):
 *
 *  - An ON statement establishes an on-unit for the block activation that
 *    executes it; a called procedure inherits it, and what the procedure
 *    establishes itself ends when it returns.  REVERT ends it earlier.
 *  - An on-unit is a block of its own (the compiler makes it an internal
 *    procedure of the establishing block), so it runs on a frame of its
 *    own wherever the condition is raised: in a called procedure or inside
 *    this run time.  Its static link is the establishing frame.
 *  - With no on-unit, or after ON ... SYSTEM, the system action applies:
 *    for ZERODIVIDE, FIXEDOVERFLOW, CONVERSION and ENDFILE a message on
 *    SYSPRINT, then ERROR; for CONDITION(name) the message alone; for
 *    ERROR the end of the program (FINISH, step code 2000).
 *
 * Each establishment is a record in the establishing frame (the compiler
 * reserves it and clears it at block entry); the established ones form a
 * chain, newest first.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __MVS__
#include <mvs/recovery.h>
#include <mvs/file.h>
#include <mvs/crt.h>
#endif

#include "pl1rt.h"
#include "pl1int.h"
#include "pl1iof.h"

struct pl1_est {                 /* as the compiler lays it out, 28 bytes */
    struct pl1_est *next;
    int    cond;
    const void *qual;            /* ENDFILE: the file; CONDITION: the name */
    void  *unit;                 /* the on-unit's entry; 0: SYSTEM */
    char  *frame;                /* the establishing frame */
    int    linked;
    int    snap;                 /* ON ... SNAP */
};

/* One on-unit running: what ONCODE, ONSOURCE and ONCHAR answer. */
struct pinfo;
struct act {
    struct act *prev;
    int    cond, oncode;
    char  *src;                  /* CONVERSION: the field converted */
    int    srclen, idx;
    char   key[256];             /* KEY, RECORD: ONKEY */
    int    keylen;
    const struct pinfo *loc;     /* ONLOC: the block that raised it */
};

/* What a procedure's frame holds for this run time (gen.c): the
 * procedure's descriptor -- its entry address and name. */
struct pinfo {
    char  *entry;
    unsigned char len;
    char   name[31];
};
#define FRAME_PINFO PL1_FR_PINFO

static struct pl1_est *chain;
static struct act *acts;
static int terminating;

/* The frame of the PL/I block running now; gen.c keeps it. */
char *pl1_curframe asm("PL1CURF");

/* NOCONVERSION (1) and NOOVERFLOW (2) in effect: set by the generated
 * code (gen.c NOCV) */
char pl1_noconv asm("PL1NOCV");
/* The ABI stamp every external procedure references (rt/pl1abi.h) */
int pl1_abi_stamp asm(PL1_ABI_STAMP) = 1;
/* The return code of PLIRETC, which PLIRETV reads (the Optimizer's,
 * SC33-0006-7 "Return Codes"): 0 to 999, added to the thousands of the
 * way the program ended -- 0 at its normal end (the main procedure
 * returns it), 1000 after STOP or EXIT, 2000 after ERROR. */
int pl1_retcode asm("PL1RTCD");
/* An attention interruption is pending (rt/pl1abi.h): set by the
 * attention exit, tested where the program polls */
volatile unsigned char pl1_attn asm("PL1ATTN");
static int signaling;                /* raise_cond from a SIGNAL statement */

/* the PSW address of the interruption being handled (pl1_program_check) */
static unsigned pic_addr;

/* ---- the chain -------------------------------------------------------- */

static void unlink_est(struct pl1_est *e)
{
    for (struct pl1_est **p = &chain; *p; p = &(*p)->next)
        if (*p == e) {
            *p = e->next;
            break;
        }
    e->linked = 0;
}

void pl1_on(void *est, int cond, const void *qual, void *unit, void *frame)
{
    struct pl1_est *e = est;
    if (e->linked)
        unlink_est(e);           /* again: it becomes the newest */
    e->cond = cond & 0xFFFF;
    e->snap = cond >> 16 & 1;
    e->qual = qual;
    e->unit = unit;
    e->frame = frame;
    e->next = chain;
    e->linked = 1;
    chain = e;
}

void pl1_revert(void *est)
{
    struct pl1_est *e = est;
    if (e->linked)
        unlink_est(e);
}

/* The block whose frame this is ends. */
void pl1_block_exit(void *frame)
{
    for (struct pl1_est **p = &chain; *p;) {
        if ((*p)->frame == frame) {
            (*p)->linked = 0;
            *p = (*p)->next;
        } else {
            p = &(*p)->next;
        }
    }
}

/* While a sort runs (rt/pl1sort.c): called where control would leave
 * an exit procedure -- a GO TO to a block below the sort's caller, or
 * the end of the program -- with the target frame, NULL for the end.
 * It finishes the sort first and comes back here afterwards. */
void (*pl1i_sortleave)(void *frame) asm("PL1SRTHK");

/* the event whose EVENT I/O condition is being raised (pl1i_evraise) */
unsigned char *pl1i_evpend asm("PL1EVPND");

/* GO TO a label of the block whose frame this is: every block above it
 * ends, on-units running there included.  The C stack grows upwards, so
 * "above" is a higher address. */
void pl1_goto(void *frame)
{
    char *f = frame;
    if (pl1i_sortleave)
        pl1i_sortleave(frame);
    if (pl1i_evpend) {
        pl1i_evpend[PL1_EV_COMPL] = 0x80;   /* out of the on-unit of an EVENT I/O */
        pl1i_evpend = NULL;
    }
    for (struct pl1_est **p = &chain; *p;) {
        if ((*p)->frame > f) {
            (*p)->linked = 0;
            *p = (*p)->next;
        } else {
            p = &(*p)->next;
        }
    }
    while (acts && (char *)acts > f)
        acts = acts->prev;
    pl1_curframe = f;
    pl1_noconv = f[PL1_FR_NOCV + 1];             /* the label's block's own (gen.c NOCV) */
    pic_addr = 0;                    /* out of an on-unit an interruption raised */
}

static int same_qual(int cond, const void *a, const void *b)
{
    if (cond == C_CONDITION || cond == C_CHECK) {
        const unsigned char *x = a, *y = b;
        return x[0] == y[0] && memcmp(x + 1, y + 1, x[0]) == 0;
    }
    return (cond != C_ENDFILE && cond != C_ENDPAGE && cond != C_UNDF && cond != C_RECORD &&
            cond != C_KEY && cond != C_NAME && cond != C_TRANSMIT) || a == b;
}

static struct pl1_est *established(int cond, const void *qual)
{
    for (struct pl1_est *e = chain; e; e = e->next)
        if (e->cond == cond && same_qual(cond, e->qual, qual))
            return e;
    return NULL;
}

/* ---- messages --------------------------------------------------------- */

/* "<text> AT OFFSET +nnnnn FROM ENTRY POINT <name>": the offset of the
 * return address of the call that raised the condition, which the
 * running block's frame holds in its save area (R14 at +12). */
/* SNAP (Q1SNAP, Q2SNAP): after the message of the place, the procedures
 * that called the one running, up to the main procedure -- BEGIN blocks
 * and on-units left out -- along the frames' saved PL1CURF */
#define FRAME_CURF PL1_FR_CURF
static int snap_trace;

static void snap_callers(void)
{
    char *fr = pl1_curframe;
    int seen = 0;
    char m[80];
    for (; fr; fr = *(char **)(fr + FRAME_CURF)) {
        const struct pinfo *pi = *(const struct pinfo **)(fr + FRAME_PINFO);
        if (!pi || (pi->len & PL1_PI_BLOCK))
            continue;                 /* a BEGIN block, an on-unit */
        if (seen++) {
            snprintf(m, sizeof m, "    CALLED FROM PROCEDURE WITH ENTRY POINT %.*s",
                     pi->len & PL1_PI_LEN, pi->name);
            pl1i_message(m);
        }
        if (pi->len & PL1_PI_MAIN)
            break;                    /* the main procedure */
    }
}

static void message(const char *text)
{
    char m[200];
    const struct pinfo *pi = NULL;
    unsigned off = 0;

    if (pl1_curframe) {
        pi = *(const struct pinfo **)(pl1_curframe + FRAME_PINFO);
        off = (unsigned)((*(char **)(pl1_curframe + 12) - pi->entry)) & 0xFFFFFFu;
        /* an interruption in the block's own code: its address (a
         * procedure has at most 12 KB); in a routine it called, the call */
        if (pic_addr && ((pic_addr - (unsigned)(size_t)pi->entry) & 0xFFFFFFu) < 0x10000u)
            off = (pic_addr - (unsigned)(size_t)pi->entry) & 0xFFFFFFu;
    }
    snprintf(m, sizeof m, "%s AT OFFSET +%05X FROM ENTRY POINT %.*s", text, off,
             pi ? pi->len & PL1_PI_LEN : 0, pi ? pi->name : "");
    pl1i_message(m);
    if (snap_trace) {
        snap_trace = 0;
        snap_callers();
    }
}

/* ---- abnormal termination by the system ------------------------------ */

/* As PL/I (F)'s IHESAP issues a STAE (GC28-6594-7, p. 204), the main
 * procedure's prologue establishes an ESTAE (libc370's __estae).  Its
 * exit writes IHE012I to SYSPRINT -- the job name, the offset in the
 * PL/I block and the completion code -- and lets the abend go on: no
 * ERROR or FINISH on-unit runs (Q1ABN, JOB01328, JOB01330). */
#ifdef __MVS__
static int in_abend;

#ifdef PL1_STACKHW
static void hw_report(void);
#endif

static int abend_exit(SDWA *sdwa, void *udata)
{
    char m[160], job[9];
    const struct pinfo *pi = NULL;
    unsigned off = 0, cc, psw;
    const char *tiot;

    (void)udata;
    if (in_abend)
        return 0;                       /* an abend in here: let it go */
    in_abend = 1;
    if (sdwa->SDWAERRD & SDWACLUP)
        return 0;                       /* the task ends: no C run time work */
    if (pl1i_failed_fp == stdout) {
        /* SYSPRINT itself is full: no message there (PL/I (F) writes it
         * on the console then, not done), and closed without the write
         * that failed (libc370's __fabandon) */
        __fabandon(stdout);
        return 0;
    }
    tiot = *(const char **)(*(char **)0x21C + 12);   /* PSATOLD, TCBTIO */
    memcpy(job, tiot, 8);
    job[8] = 0;
    cc = ((unsigned char)sdwa->SDWACMPC[0] << 16) | ((unsigned char)sdwa->SDWACMPC[1] << 8) |
         (unsigned char)sdwa->SDWACMPC[2];
    psw = ((unsigned char)sdwa->SDWANXTP[0] << 16) | ((unsigned char)sdwa->SDWANXTP[1] << 8) |
          (unsigned char)sdwa->SDWANXTP[2];
    if (pl1_curframe) {
        pi = *(const struct pinfo **)(pl1_curframe + FRAME_PINFO);
        off = (unsigned)((*(char **)(pl1_curframe + 12) - pi->entry)) & 0xFFFFFFu;
        if (((psw - (unsigned)(size_t)pi->entry) & 0xFFFFFFu) < 0x10000u)
            off = (psw - (unsigned)(size_t)pi->entry) & 0xFFFFFFu;
    }
    if (cc >> 12)
        snprintf(m, sizeof m, "IHE012I %-8s ABENDED AT OFFSET %06X FROM ENTRY POINT %.*s "
                 "WITH CC %03X (SYSTEM)", job, off, pi ? pi->len & PL1_PI_LEN : 0,
                 pi ? pi->name : "", cc >> 12);
    else      /* not measured: the user code as PL/I (F) would write it */
        snprintf(m, sizeof m, "IHE012I %-8s ABENDED AT OFFSET %06X FROM ENTRY POINT %.*s "
                 "WITH CC %04u (USER)", job, off, pi ? pi->len & PL1_PI_LEN : 0,
                 pi ? pi->name : "", cc & 0xFFF);
    pl1i_message(m);
    /* closed, not flushed: after fflush the last block was not on the
     * data set when the abend went on (JOB01354); after fclose it is
     * (JOB01357) */
    fclose(stdout);
#ifdef PL1_STACKHW
    hw_report();                        /* the deepest point: this exit's own */
#endif
    return 0;                           /* percolate: the abend goes on */
}

/* ABEND with a system completion code (SVC 13; the code in bits 8 to
 * 19 of R1, as the ABEND macro's SYSTEM option puts it) */
void pl1i_sysabend(unsigned code)
{
    unsigned r1 = (code & 0xFFF) << 12;
    __asm__ __volatile__("L\t1,%0\n\tSVC\t13" : : "m"(r1) : "1");
}

static void recovery_off(void)
{
    __estae(ESTAE_DELETE, NULL, NULL);
    pl1_pie_off();
}

#ifdef PL1_STACKHW
/* A measuring build (#177): the stack above this frame filled with a
 * pattern at the start, and at the end the highest byte used reported
 * by WTO, PL1STACK used OF length.  LIBC/370 obtains the PPA and the
 * stack in one piece, the stack after the PPA (mvs/crt.h). */
#include <mvs/crt.h>
#include <mvs/wto.h>
static unsigned char *hw_base, *hw_lo, *hw_hi;

static void hw_report(void)
{
    unsigned char *p = hw_hi;
    if (!hw_hi)
        return;
    while (p > hw_lo && p[-1] == 0xA5)
        p--;
    wtof("PL1STACK %u OF %u", (unsigned)(p - hw_base), (unsigned)(hw_hi - hw_base));
}

static void hw_mark(void)
{
    CLIBPPA *ppa = __ppaget();
    char here;
    hw_base = (unsigned char *)ppa + ((sizeof *ppa + 7) & ~7u);
    hw_hi = hw_base + ppa->ppastkln;
    hw_lo = (unsigned char *)&here + 2048;
    if ((unsigned char *)&here < hw_base || hw_lo >= hw_hi) {
        wtof("PL1STACK ? (frame %p, stack %p length %u)", (void *)&here, (void *)hw_base,
             ppa->ppastkln);
        return;
    }
    memset(hw_lo, 0xA5, (size_t)(hw_hi - hw_lo));
    atexit(hw_report);
}
#endif

void pl1_recovery_on(void)
{
#ifdef PL1_STACKHW
    hw_mark();
#endif
    if (__estae(ESTAE_CREATE, (void *)abend_exit, NULL) == 0)
        atexit(recovery_off);
    else
        atexit(pl1_pie_off);
}
#else
/* the host tests: no ESTAE, no SPIE, no SVC 13 */
void pl1i_sysabend(unsigned code)
{
    fprintf(stderr, "ABEND S%03X\n", code & 0xFFF);
    abort();
}

void pl1_recovery_on(void)
{
}
#endif

/* ---- raising ---------------------------------------------------------- */

static void raise_cond(int cond, const void *qual, int oncode, char *src, int srclen, int idx);

/* ---- ATTENTION (#226) -------------------------------------------------- */

/* INTERRUPT (-interrupt): the attention exit (rt/pl1attn.asm), once,
 * when the program runs under a TSO terminal -- LIBC/370's start-up
 * has asked (PPAFLAG_TSOFG).  In batch, and under the terminal monitor
 * program in batch, nothing: no attention can come there.  Cancelled at
 * the end, as the exit is in the module. */
#ifdef __MVS__
static void attn_off(void)
{
    pl1_stax_off();
}
#endif

void pl1_attn_on(void)
{
#ifdef __MVS__
    static int done;
    if (done)
        return;
    done = 1;
    if (__ppaget()->ppaflag & PPAFLAG_TSOFG) {
        pl1_stax_on();
        atexit(attn_off);
    }
#endif
}

/* An attention came: ATTENTION, ONCODE 1400 (GC33-0009-4, p. 386); with
 * no on-unit, nothing (system_action) */
void pl1_attn_poll(void)
{
    if (pl1_attn) {
        pl1_attn = 0;
        raise_cond(C_ATTENTION, NULL, 1400, NULL, 0, 0);
    }
}

/* The end of the program after ERROR: FINISH, then step code 2000. */
static void terminate(int oncode)
{
    if (!terminating) {
        terminating = 1;
        raise_cond(C_FINISH, NULL, oncode, NULL, 0, 0);
    }
    if (pl1i_sortleave)
        pl1i_sortleave(NULL);
    pl1_finish();
    exit(2000 + pl1_retcode);
}

static void cond_error(int oncode)
{
    /* ERROR in the ERROR on-unit: the end.  Seen from the on-units
     * running, so that a GO TO out of the ERROR on-unit ends it. */
    for (struct act *a = acts; a; a = a->prev)
        if (a->cond == C_ERROR)
            terminate(oncode);
    raise_cond(C_ERROR, NULL, oncode, NULL, 0, 0);
    terminate(oncode);           /* the on-unit returned normally */
}

static void system_action(int cond, const void *qual, int oncode)
{
    char t[128];
    switch (cond) {
    case C_ZERODIVIDE:
        message("IHE330I ZERODIVIDE");
        break;
    case C_FIXEDOVERFLOW:
        message("IHE320I FIXEDOVERFLOW");
        break;
    case C_CONVERSION:
        message("IHE604I ERROR IN CONVERSION FROM CHARACTER STRING TO ARITHMETIC");
        break;
    case C_UNDERFLOW:
        /* the result is 0, and the program goes on (JOB00162) */
        message("IHE340I UNDERFLOW");
        return;
    case C_SIZE:
        message("IHE310I SIZE");          /* then ERROR (JOB00211) */
        break;
    case C_SUBRG:
        message("IHE500I SUBSCRIPTRANGE");
        break;
    case C_STRG:
        message("IHE350I STRINGRANGE");   /* and the program goes on (JOB00210) */
        return;
    case C_OVERFLOW:
        message("IHE300I OVERFLOW");      /* then ERROR (JOB00224) */
        break;
    case C_ENDPAGE:
        return;                          /* the next record starts the page */
    case C_ATTENTION:
        /* "the condition is effectively ignored, and there is no
         * interruption in the flow of control" (GC33-0009-4, p. 393) */
        return;
    case C_AREA:
        /* then ERROR (JOB00419) */
        message(oncode == 360 ? "IHE360I AREA CONDITION RAISED IN ALLOCATE STATEMENT" :
                oncode == 361 ? "IHE361I AREA CONDITION RAISED IN ASSIGNMENT STATEMENT" :
                "IHE362I AREA SIGNALED");
        break;
    case C_UNDF:
    case C_RECORD: {
        /* then ERROR (JOB00361, Q2UNDF) */
        const char *n = qual;
        int l = 8;
        while (l > 0 && n[l - 1] == ' ')
            l--;
        snprintf(t, sizeof t, cond == C_UNDF ?
                 "IHE150I FILE %.*s - CANNOT BE OPENED (INCORRECT OR NO DD CARD)" :
                 "IHE111I FILE %.*s - RECORD VARIABLE SMALLER THAN RECORD SIZE", l, n);
        message(t);
        break;
    }
    case C_KEY: {
        /* then ERROR (JOB00430) */
        /* 50 to 54 and 56 measured (JOB00430, JOB00498); 55 and 57 after
         * the ONCODE table of GC28-8201-4, not measured */
        static const char *const text[] = {
            "KEY CONDITION SIGNALED", "KEYED RECORD NOT FOUND",
            "ATTEMPT TO ADD DUPLICATE KEY", "KEY SEQUENCE ERROR", "KEY CONVERSION ERROR",
            "KEY SPECIFICATION ERROR",
            "KEYED RELATIVE RECORD/TRACK OUTSIDE DATA SET LIMIT",
            "NO SPACE AVAILABLE TO ADD KEYED RECORD"
        };
        const char *n = qual;
        int l = 8;
        while (l > 0 && n[l - 1] == ' ')
            l--;
        snprintf(t, sizeof t, "IHE13%dI FILE %.*s - %s", oncode - 50, l, n,
                 oncode >= 50 && oncode <= 57 ? text[oncode - 50] : "KEY");
        message(t);
        break;
    }
    case C_STRINGSIZE:
        /* a message, and the program goes on (GC33-0009-4 Section H,
         * SC26-3114-01 chapter 15); the Optimizer's IBM440I, IBM441I
         * (SC33-0027-5) */
        message(signaling ? "IBM440I 'STRINGSIZE' CONDITION RAISED BY 'SIGNAL' STATEMENT"
                          : "IBM441I 'STRINGSIZE' CONDITION RAISED");
        return;
    case C_STORAGE:
        /* ANSI 6.4.4: implementation-defined -- a message, then ERROR */
        message("PL1C/370 STORAGE CONDITION RAISED");
        break;
    case C_TRANSMIT: {
        /* then ERROR (Q1IO): PL1C/370 raises it by SIGNAL only */
        const char *n = qual;
        int l = 8;
        while (l > 0 && n[l - 1] == ' ')
            l--;
        snprintf(t, sizeof t, "IHE122I FILE %.*s - TRANSMIT CONDITION SIGNALED", l, n);
        message(t);
        break;
    }
    case C_NAME: {
        /* the field is skipped, and the GET goes on (JOB00990) */
        const char *n = qual;
        int l = 8;
        while (l > 0 && n[l - 1] == ' ')
            l--;
        snprintf(t, sizeof t, "IHE100I FILE %.*s - UNRECOGNISABLE DATA NAME", l, n);
        message(t);
        return;
    }
    case C_ENDFILE: {
        const char *n = qual;
        int l = 8;
        while (l > 0 && n[l - 1] == ' ')
            l--;
        snprintf(t, sizeof t, "IHE140I FILE %.*s - END OF FILE ENCOUNTERED", l, n);
        message(t);
        break;
    }
    case C_CONDITION: {
        /* PL/I (F) names the condition only up to seven characters
         * (JOB00140/141) */
        const unsigned char *n = qual;
        if (n[0] <= 7)
            snprintf(t, sizeof t, "IHE501I CONDITION %.*s", n[0], (const char *)n + 1);
        else
            snprintf(t, sizeof t, "IHE501I CONDITION");
        message(t);
        return;
    }
    case C_ERROR:
        terminate(oncode);
        return;
    case C_FINISH:
    default:
        return;
    }
    cond_error(oncode);
}

/* ONFILE: the file of the last file condition */
static char onfile[9];
static int onfile_len;

char *pl1_onfile_addr(void) { return onfile; }

/* ONKEY: what the keyed run time gives before its KEY and RECORD
 * conditions, until the statement ends (a null string: SIGNAL) */
static char pendkey[256];
static int pendkeylen;

void pl1_setkey(const char *key, int len)
{
    if (len > (int)sizeof pendkey)
        len = sizeof pendkey;
    pendkeylen = len > 0 ? len : 0;
    if (pendkeylen)
        memcpy(pendkey, key, (size_t)pendkeylen);
}

char *pl1_onkey_addr(void) { return acts ? acts->key : pendkey; }
int pl1_onkey_len(void) { return acts ? acts->keylen : 0; }
int pl1_onfile_len(void) { return onfile_len; }

static void raise_cond(int cond, const void *qual, int oncode, char *src, int srclen, int idx)
{
    struct pl1_est *e = established(cond, qual);
    if (qual && (cond == C_ENDFILE || cond == C_ENDPAGE || cond == C_UNDF || cond == C_RECORD ||
                 cond == C_KEY || cond == C_NAME || cond == C_TRANSMIT)) {
        const char *n = qual;
        int l = 8;
        while (l > 0 && n[l - 1] == ' ')
            l--;
        memcpy(onfile, n, (size_t)l);
        onfile_len = l;
    }
    struct act a;
    struct pl1i_iostate io;
    char *frame = pl1_curframe;

    if (e && e->snap) {
        /* SNAP: with an on-unit the place first; with SYSTEM the system
         * action's message is the place (Q4SNAP) */
        static const char *const tag[] = {
            "", "ENDF", "ZDIV", "FOFL", "CONV", "ERR", "FIN", "COND", "UFLO", "SIZE",
            "SUBR", "STRG", "OFLO", "ENDP", "UNDF", "RECD", "AREA", "KEY"
        };
        snap_trace = 1;
        if (e->unit) {
            char t[40];
            snprintf(t, sizeof t, "CONDITION %-4s OCCURRED", cond == C_ATTENTION ? "ATTN" :
                     cond > 0 && cond < (int)(sizeof tag / sizeof tag[0]) ? tag[cond] : "");
            message(t);
        }
    }
    if (!e || !e->unit) {
        system_action(cond, qual, oncode);
        snap_trace = 0;
        return;
    }
    a.prev = acts;
    a.cond = cond;
    a.oncode = oncode;
    a.src = src;
    a.srclen = srclen;
    a.idx = idx;
    a.keylen = 0;
    if (cond == C_KEY || cond == C_RECORD) {
        a.keylen = pendkeylen;
        memcpy(a.key, pendkey, (size_t)pendkeylen);
    }
    a.loc = frame ? *(const struct pinfo **)(frame + FRAME_PINFO) : NULL;
    acts = &a;
    pl1i_iosave(&io);
    ((void (*)(void *))e->unit)(e->frame);
    pl1i_iorest(&io);
    pl1_curframe = frame;
    acts = a.prev;
}

/* ENDPAGE of a PRINT file: its on-unit, if one is established (the
 * system action, a new page, is rt/pl1io.c's).  Returns 1 if it ran. */
/* UNDEFINEDFILE, RECORD: the condition for the file; returns after a
 * normal return of an on-unit */
/* ---- EVENT I/O (#228) ------------------------------------------------- */

/* While a READ, WRITE or REWRITE with EVENT runs (PL1EVIO to PL1EVEN),
 * its file conditions go into the event instead of being raised; the
 * WAIT that completes the event raises the first of them
 * (pl1i_evraise), the event still incomplete while its on-unit runs,
 * complete after it -- or after a GO TO out of it (pl1_goto), as PL/I
 * (F) has it (t228e Q1EIO) */
static unsigned char *evio;

void pl1_evio_begin(unsigned char *ev)
{
    *(volatile unsigned *)(ev + PL1_EV_ECB) = 0x40000000u;   /* posted: done at once */
    ev[PL1_EV_COMPL] = 0;
    ev[PL1_EV_ACTIVE] = 0x80;
    ev[PL1_EV_COND] = 0;
    evio = ev;
}

void pl1_evio_end(void)
{
    evio = NULL;
}

static int evio_take(int cond, int oncode, const void *desc)
{
    if (!evio || !(cond == C_ENDFILE || cond == C_KEY || cond == C_RECORD ||
                   cond == C_TRANSMIT))
        return 0;
    if (!evio[PL1_EV_COND]) {
        evio[PL1_EV_COND] = (unsigned char)cond;
        evio[PL1_EV_ONCODE] = (unsigned char)(oncode >> 8);
        evio[PL1_EV_ONCODE + 1] = (unsigned char)oncode;
        *(const void **)(evio + PL1_EV_DESC) = desc;
    }
    return 1;
}

void pl1i_evraise(unsigned char *ev)
{
    int cond = ev[PL1_EV_COND];
    ev[PL1_EV_ACTIVE] = 0;
    if (cond) {
        ev[PL1_EV_COND] = 0;
        pl1i_evpend = ev;
        raise_cond(cond, *(void **)(ev + PL1_EV_DESC),
                   ev[PL1_EV_ONCODE] << 8 | ev[PL1_EV_ONCODE + 1], NULL, 0, 0);
        pl1i_evpend = NULL;
    }
    ev[PL1_EV_COMPL] = 0x80;
}

void pl1i_filecond(int cond, int oncode, void *desc)
{
    if (evio_take(cond, oncode, desc))
        return;
    raise_cond(cond, desc, oncode, NULL, 0, 0);
}

/* AREA, raised by an allocation (360) or an assignment (361); returns
 * after a normal return of an on-unit */
void pl1i_areacond(int oncode)
{
    raise_cond(C_AREA, NULL, oncode, NULL, 0, 0);
}

int pl1i_endpage(void *desc)
{
    struct pl1_est *e = established(C_ENDPAGE, desc);
    if (!e || !e->unit)
        return 0;
    raise_cond(C_ENDPAGE, desc, 90, NULL, 0, 0);
    return 1;
}

void pl1_signal(int cond, const void *qual, int oncode)
{
    if (evio_take(cond, oncode, qual))
        return;                  /* ENDFILE of a READ ... EVENT: at the WAIT */
    if (cond == C_ERROR)
        cond_error(oncode);      /* the program ends after the on-unit */
    else {
        int saved = signaling;
        signaling = 1;
        raise_cond(cond, qual, oncode, NULL, 0, 0);
        signaling = saved;
    }
}


/* An error of a mathematical routine (rt/pl1m*.c): the code it
 * gives in line, PL/I (F)'s message and ONCODE for it (JOB00224, JOB00226:
 * the long routine is the short one's ONCODE + 1); then ERROR. */
static const struct { int code; const char *text; int oncode; } math_errors[] = {
    { 0x0100, "IHE200I IHESQS X LT 0 IN SQRT (X)", 1500 },
    { 0x0108, "IHE200I IHESQL X LT 0 IN SQRT (X)", 1501 },
    { 0x0102, "IHE202I IHELNS X LT OR = 0 IN LOG(X) OR LOG2(X) OR LOG10(X)", 1504 },
    { 0x010A, "IHE202I IHELNL X LT OR = 0 IN LOG(X) OR LOG2(X) OR LOG10(X)", 1505 },
    { 0x0103, "IHE212I IHESNS ABS(X) GE (2**18)*K IN SIN(X) OR COS(X) (K=PI) OR SIND(X) OR "
              "COSD(X) (K=180)", 1506 },
    { 0x010B, "IHE203I IHESNL ABS(X) GE (2**50)*K IN SIN(X) OR COS(X) (K=PI) OR SIND(X) OR "
              "COSD(X) (K=180)", 1507 },
    { 0x0104, "IHE213I IHETNS ABS(X) GE (2**18)*K IN TAN(X) (K=PI) OR TAND(X) (K=180)", 1508 },
    { 0x010C, "IHE204I IHETNL ABS(X) GE (2**50)*K IN TAN(X) (K=PI) OR TAND(X) (K=180)", 1509 },
    { 0x0105, "IHE206I IHEATS X=Y=0 IN ATAN(Y,X) OR ATAND(Y,X)", 1510 },
    { 0x010D, "IHE206I IHEATL X=Y=0 IN ATAN(Y,X) OR ATAND(Y,X)", 1511 },
    { 0x0107, "IHE208I IHEHTS ABS(X) GT OR = 1 IN ATANH(X)", 1514 },
    { 0x010F, "IHE208I IHEHTL ABS(X) GT OR = 1 IN ATANH(X)", 1515 },
    { 0x0112, "IHE209I IHEXXS X=0, Y LE 0 IN X**Y", 1552 },
    { 0x0113, "IHE209I IHEXXL X=0, Y LE 0 IN X**Y", 1553 },
};

/* A program interruption, from the exit (rt/pl1pie.asm): 12 exponent
 * overflow, OVERFLOW (JOB00259); 15 floating-point divide, which only a
 * library routine meets (the generated code tests the divisor),
 * ZERODIVIDE (JOB00253).  A normal return goes on after the
 * instruction, with the result the machine left. */
void pl1_program_check(int code, unsigned addr)
{
    unsigned saved = pic_addr;
    pic_addr = addr;
    if (code == 12) {
        if (!(pl1_noconv & PL1_NOCV_OFL))
            raise_cond(C_OVERFLOW, NULL, 300, NULL, 0, 0);
    } else
        raise_cond(C_ZERODIVIDE, NULL, 320, NULL, 0, 0);
    pic_addr = saved;
}

void pl1_math_error(int code)
{
    for (size_t i = 0; i < sizeof math_errors / sizeof math_errors[0]; i++)
        if (math_errors[i].code == code) {
            message(math_errors[i].text);
            cond_error(math_errors[i].oncode);
        }
    {
        char t[64];
        snprintf(t, sizeof t, "IHE2NNI MATHEMATICAL ERROR CODE %04X", code);
        message(t);
        cond_error(1500);
    }
}

/* SUBSCRIPTRANGE found by the generated code: after a normal return of
 * the on-unit the program cannot go on with that subscript -- ERROR (not
 * measured: the probes leave the on-unit by GO TO). */
void pl1_subscript_range(int cond, const void *qual, int oncode)
{
    raise_cond(cond, qual, oncode, NULL, 0, 0);
    cond_error(oncode);
}

/* SUBSTR(s,i[,j]) under STRINGRANGE: out of s, the condition; then the
 * part inside s (JOB00210: SUBSTR('ABCDEF',5,4) is 'EF', (0,2) is 'A',
 * (7) and (3,-1) are ''). out[0] is the start, out[1] the length. */
void pl1_substr_range(int len, int i, int j, int hasj, int *out)
{
    int end;
    if (hasj ? (i < 1 || j < 0 || i + j - 1 > len) : (i < 1 || i > len))
        raise_cond(C_STRG, NULL, 350, NULL, 0, 0);
    end = hasj ? i + j - 1 : len;
    if (end > len)
        end = len;
    if (i < 1)
        i = 1;
    out[0] = i;
    out[1] = end >= i && (!hasj || j > 0) ? end - i + 1 : 0;
}

/* A character of src cannot be converted (at idx): CONVERSION, and after
 * a normal return of the on-unit the caller converts again -- the on-unit
 * must have changed the field through ONSOURCE or ONCHAR (IHE799I
 * otherwise, JOB00139). */

int pl1i_conversion(int oncode, char *src, int len, int idx)
{
    char small[64], *copy;
    int same;

    if (pl1_noconv & PL1_NOCV_CONV)
        return 0;                         /* disabled: no condition */
    copy = len <= (int)sizeof small ? small : malloc((size_t)len);

    if (!copy) {
        fputs("PL1C/370: out of storage\n", stderr);
        exit(16);
    }
    memcpy(copy, src, (size_t)len);
    raise_cond(C_CONVERSION, NULL, oncode, src, len, idx);
    same = memcmp(copy, src, (size_t)len) == 0;
    if (copy != small)
        free(copy);
    if (same) {
        message("IHE799I RETURN ATTEMPTED FROM CONVERSION ON-UNIT BUT SOURCE FIELD NOT "
                "MODIFIED");
        cond_error(oncode);
    }
    return 1;
}

/* The message of an error the run time finds, then ERROR (GET STRING:
 * an unknown name). */
void pl1i_errmsg(const char *text)
{
    message(text);
    cond_error(1);
}

/* The same with the ONCODE given (IHE025I, 1005: DISPLAY) */
void pl1i_errmsgc(int oncode, const char *text)
{
    message(text);
    cond_error(oncode);
}

/* An I/O error of a file: its message, then ERROR (Q2IXB: IHE029I with
 * ONCODE 1009, IHE030I with 1007) */
void pl1i_fileerr(int oncode, int msgno, const char *text, void *desc)
{
    char t[128];
    const char *n = desc;
    int l = 8;
    while (l > 0 && n[l - 1] == ' ')
        l--;
    snprintf(t, sizeof t, "IHE%03dI FILE %.*s - %s", msgno, l, n, text);
    message(t);
    cond_error(oncode);
}

/* The normal end of the main procedure: FINISH (ONCODE 4, JOB00137). */
void pl1_end_main(void)
{
    if (!terminating) {
        terminating = 1;
        raise_cond(C_FINISH, NULL, 4, NULL, 0, 0);
    }
}

void pl1_pliretc(int n)
{
    /* "a return code greater than 999 ... is set to 999" (the message
     * the manual names is not given) */
    pl1_retcode = n > 999 ? 999 : n < 0 ? 0 : n;
}

/* PLIDUMP's S and E: the end, FINISH not raised (SC33-0006-7, p. 289),
 * the step code 1000 (p. 290) */
void pl1i_quit(int thousands)
{
    terminating = 1;
    if (pl1i_sortleave)
        pl1i_sortleave(NULL);
    pl1_finish();
    exit(thousands + pl1_retcode);
}

int pl1i_unit_at(const char *lo, const char *hi, struct pl1i_unit *u)
{
    for (struct act *a = acts; a; a = a->prev)
        if ((const char *)a > lo && (const char *)a < hi) {
            u->cond = a->cond;
            u->oncode = a->oncode;
            u->src = a->src;
            u->srclen = a->srclen;
            u->idx = a->idx;
            u->key = a->key;
            u->keylen = a->keylen;
            u->loc = a->loc ? a->loc->name : "";
            u->loclen = a->loc ? a->loc->len & PL1_PI_LEN : 0;
            return 1;
        }
    return 0;
}

/* STOP and EXIT: FINISH with ONCODE 4, then the end; PL/I (F) ends the
 * step with 1000 (JOB00171). */
void pl1_stop(void)
{
    if (!terminating) {
        terminating = 1;
        raise_cond(C_FINISH, NULL, 4, NULL, 0, 0);
    }
    if (pl1i_sortleave)
        pl1i_sortleave(NULL);
    pl1_finish();
    exit(1000 + pl1_retcode);
}

/* ---- ONCODE, ONSOURCE, ONCHAR ----------------------------------------- */

int pl1_oncode(void)
{
    return acts ? acts->oncode : 0;
}

static struct act *conv_act(void)
{
    for (struct act *a = acts; a; a = a->prev)
        if (a->cond == C_CONVERSION)
            return a->src ? a : NULL;
    return NULL;
}

static char blank[1] = { ' ' };

char *pl1_onsource_addr(void)
{
    struct act *a = conv_act();
    return a ? a->src : blank;
}

int pl1_onsource_len(void)
{
    struct act *a = conv_act();
    return a ? a->srclen : 0;
}

/* CHECK(name), qual AL1(n),C'name', ONCODE 510 for a label or entry,
 * 511 for a variable: its on-unit runs (0), or the system action -- a
 * label's or entry's name written as a message (0), a variable's value
 * for the caller to write (1) (Q1CHK). */
int pl1_check(const unsigned char *qual, int oncode)
{
    struct pl1_est *e = established(C_CHECK, qual);
    if (e && e->unit) {
        raise_cond(C_CHECK, qual, oncode, NULL, 0, 0);
        return 0;
    }
    if (oncode == 510) {
        char t[40];
        snprintf(t, sizeof t, "%.*s", qual[0], (const char *)qual + 1);
        pl1i_message(t);
        return 0;
    }
    return 1;
}

/* STRINGSIZE: a string cut to its target's length, while the condition
 * is enabled (a STRINGSIZE prefix: bit 4 of PL1NOCV clear).  The value
 * is cut after a normal return, or after the system action's message. */
void pl1i_strsize(void)
{
    if (pl1_noconv & PL1_NOCV_STRZ)
        return;
    raise_cond(C_STRINGSIZE, NULL, 150, NULL, 0, 0);
}

/* STORAGE (ANSI 7.2.5): ALLOCATE finds no storage.  1 after a normal
 * return of the on-unit: the allocation is tried again. */
int pl1i_storage(void)
{
    raise_cond(C_STORAGE, NULL, 454, NULL, 0, 0);
    return 1;
}

/* DATAFIELD: in an on-unit for NAME, the data field GET DATA read last,
 * its delimiter included (Q1NAME: 'X=5,', and after SIGNAL NAME the
 * field before it); elsewhere a null string */
static char datafield[256];
static int datafield_len;

void pl1i_datafield(const char *p, int len)
{
    if (len > (int)sizeof datafield)
        len = sizeof datafield;
    datafield_len = len > 0 ? len : 0;
    memcpy(datafield, p, (size_t)datafield_len);
}

char *pl1_datafield_addr(void) { return datafield; }

int pl1_datafield_len(void)
{
    return acts && acts->cond == C_NAME ? datafield_len : 0;
}

/* ONLOC: the entry point by which the block that raised the condition
 * was entered (Q1MISC), a BEGIN block's or on-unit's the procedure's
 * it is written in; out of an on-unit a null string */
char *pl1_onloc_addr(void)
{
    return acts && acts->loc ? (char *)acts->loc->name : blank;
}

int pl1_onloc_len(void)
{
    return acts && acts->loc ? acts->loc->len & PL1_PI_LEN : 0;
}

char *pl1_onchar_addr(void)
{
    struct act *a = conv_act();
    return a ? a->src + a->idx : blank;
}

/* ONSOURCE = s: the field converted takes s, as a CHARACTER variable of
 * its length would -- the conversion is then done again from it. */
void pl1_set_onsource(const char *s, int len)
{
    struct act *a = conv_act();
    if (!a)
        return;
    pl1_assign_char(a->src, a->srclen, s, len);
}

void pl1_set_onchar(const char *s, int len)
{
    struct act *a = conv_act();
    if (!a)
        return;
    a->src[a->idx] = len > 0 ? s[0] : ' ';
}
