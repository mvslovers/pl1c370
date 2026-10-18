/* pl1dump.c -- the PLIDUMP built-in subroutine (#225).
 *
 * CALL PLIDUMP [(options [, identification])] writes a dump of the
 * program in PL/I terms to the DD PLIDUMP, or PL1DUMP (SC33-0006-7,
 * pp. 288-290; SC33-0025-3, chapter 12):
 *   T  a trace of the active procedures, BEGIN blocks and on-units --
 *      for an on-unit its condition and the condition built-in
 *      functions it could use (NT: none);
 *   F  the open files, their attributes and buffers (NF: none);
 *   H  a hexadecimal dump of the active blocks' frames (NH: none);
 *   B  with T, the hexadecimal save areas of the frames traced (NB: none);
 *   C  execution continues; S the program ends, FINISH not raised, with
 *      1000 plus PLIRETC's code; E (exit from the task) is S here, as
 *      there is one task; A and O (all tasks, this task) are the same.
 * The defaults are T F C A NH NB.  A letter not among these is ignored
 * (the manual says nothing).  The identification, up to 90 characters,
 * is printed under the heading.
 *
 * The layout is PL1C/370's own: the Optimizer's names its control
 * blocks (TCA, DSA), which this run time does not have.  The frames are
 * the DSAs: an on-unit is the block whose frame lies above the C frame
 * of the activation that called it (pl1i_unit_at).  Without a DD:
 * IBM013I, no dump, and the options still decide whether the program
 * goes on (SC33-0027-5). */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "pl1rt.h"
#include "pl1int.h"
#include "pl1iof.h"
#ifdef __MVS__
#include <mvs/dd.h>
#endif

#define ID_MAX   90
#define FR_NAB   76                    /* the next available byte (PDPPRLG) */

struct pinfo {                         /* pl1cond.c's: gen.c gen_pinfo */
    char  *entry;
    unsigned char len;
    char   name[31];
};

extern char *pl1_curframe asm("PL1CURF");   /* the running block's frame (pl1cond.c) */

static FILE *dfp;
static int tried;                      /* the DD opened, or found missing */

static void line(char asa, const char *text)
{
    fputc(asa, dfp);
    fputs(text, dfp);
    fputc('\n', dfp);
}

/* 32 bytes a line: address, offset, eight words, the characters */
static void hex(const unsigned char *p, int n)
{
    static const char xd[] = "0123456789ABCDEF";
    char l[132];
    for (int off = 0; off < n; off += 32) {
        int k = snprintf(l, sizeof l, "%06X %05X ",
                         (unsigned)(size_t)(p + off) & 0xFFFFFFu, (unsigned)off);
        int m = n - off < 32 ? n - off : 32;
        for (int i = 0; i < 32; i++) {
            if (i % 4 == 0)
                l[k++] = ' ';
            l[k++] = i < m ? xd[p[off + i] >> 4] : ' ';
            l[k++] = i < m ? xd[p[off + i] & 15] : ' ';
        }
        l[k++] = ' ';
        l[k++] = ' ';
        for (int i = 0; i < m; i++)
            l[k++] = isprint(p[off + i]) ? (char)p[off + i] : '.';
        l[k] = 0;
        line(' ', l);
    }
}

static const char *const abbr[] = {
    "", "ENDF", "ZDIV", "FOFL", "CONV", "ERR", "FIN", "COND", "UFL", "SIZE", "SUBG",
    "STRG", "OFL", "ENDP", "UNDF", "REC", "AREA", "KEY", "NAME", "CHCK", "TMIT", "STRZ",
    "STOR", "ATTN"
};

static void unit_values(const struct pl1i_unit *u)
{
    char m[160];
    snprintf(m, sizeof m, "      ONCODE   %d", u->oncode);
    line(' ', m);
    if (u->cond == C_CONVERSION && u->src) {
        snprintf(m, sizeof m, "      ONSOURCE =%.*s", u->srclen > 100 ? 100 : u->srclen, u->src);
        line(' ', m);
        if (u->idx >= 0 && u->idx < u->srclen) {
            snprintf(m, sizeof m, "      ONCHAR   =%c", u->src[u->idx]);
            line(' ', m);
        }
    }
    if ((u->cond == C_KEY || u->cond == C_RECORD) && u->keylen > 0) {
        snprintf(m, sizeof m, "      ONKEY    =%.*s", u->keylen > 100 ? 100 : u->keylen, u->key);
        line(' ', m);
    }
    snprintf(m, sizeof m, "      ONLOC    =%.*s", u->loclen, u->loc);
    line(' ', m);
}

static void trace(int hexa, int blocks)
{
    char m[160];
    const char *what = "PLIDUMP";
    line('0', "* * * CALLING TRACE * * *");
    for (char *fr = pl1_curframe; fr; fr = *(char **)(fr + PL1_FR_CURF)) {
        const struct pinfo *pi = *(const struct pinfo **)(fr + PL1_FR_PINFO);
        char *below = *(char **)(fr + PL1_FR_CURF);
        unsigned off;
        struct pl1i_unit u;
        int unit = 0;
        if (!pi)
            break;
        off = (unsigned)(*(char **)(fr + 12) - pi->entry) & 0xFFFFFFu;
        if (pi->len & PL1_PI_BLOCK)
            unit = pl1i_unit_at(below ? below : fr - 1, fr, &u);
        if (unit)
            snprintf(m, sizeof m, " %s WAS CALLED FROM OFFSET +%06X IN A %s ON-UNIT OF "
                     "PROCEDURE %.*s", what, off,
                     u.cond > 0 && u.cond < (int)(sizeof abbr / sizeof abbr[0]) ?
                     abbr[u.cond] : "", pi->len & PL1_PI_LEN, pi->name);
        else
            snprintf(m, sizeof m, " %s WAS CALLED FROM OFFSET +%06X IN %s %.*s", what, off,
                     pi->len & PL1_PI_BLOCK ? "A BEGIN BLOCK OF PROCEDURE" : "PROCEDURE",
                     pi->len & PL1_PI_LEN, pi->name);
        line(' ', m);
        if (hexa || blocks) {
            snprintf(m, sizeof m, "      (ENTRY ADDRESS %06X, FRAME ADDRESS %06X)",
                     (unsigned)(size_t)pi->entry & 0xFFFFFFu, (unsigned)(size_t)fr & 0xFFFFFFu);
            line(' ', m);
        }
        if (unit)
            unit_values(&u);
        if (blocks)
            hex((const unsigned char *)fr, PL1_FR_VARS);
        what = "WHICH";
        if ((pi->len & (PL1_PI_MAIN | PL1_PI_BLOCK)) == PL1_PI_MAIN)
            break;                    /* the main procedure, not a block in it */
    }
    line(' ', "* * * END OF CALLING TRACE * * *");
}

static void file_info(void)
{
    char m[160];
    line('0', "* * * FILES * * *");
    for (struct pfile *f = pl1i_files(); f; f = f->next) {
        int fl = f->flags;
        if (!f->open)
            continue;
        snprintf(m, sizeof m, " FILE %s: %s %s%s%s%s%s%s%s", f->dd,
                 fl & FL_RECORD ? "RECORD" : "STREAM",
                 fl & FL_UPDATE ? "UPDATE" : fl & FL_OUTPUT ? "OUTPUT" : "INPUT",
                 fl & FL_DIRECT ? " DIRECT" : " SEQUENTIAL", fl & FL_KEYED ? " KEYED" : "",
                 fl & FL_PRINT ? " PRINT" : "",
                 fl & FL_REG3 ? " REGIONAL(3)" : fl & FL_REG2 ? " REGIONAL(2)" :
                 fl & FL_REGIONAL ? " REGIONAL(1)" : "",
                 fl & FL_INDEXED ? " INDEXED" : "", f->eof ? " (AT ITS END)" : "");
        line(' ', m);
        if (fl & FL_PRINT) {
            snprintf(m, sizeof m, "      LINESIZE %d  PAGESIZE %d  LINENO %d  PAGENO %d",
                     f->width, f->pagesize ? f->pagesize : PAGESIZE, f->lineno, f->pageno);
            line(' ', m);
        }
        if (!(fl & FL_RECORD) && (fl & FL_OUTPUT) && f->line && f->end > 0) {
            line(' ', "      THE LINE BEING BUILT");
            hex((const unsigned char *)f->line, f->end);
        } else if (!(fl & FL_RECORD) && !(fl & FL_OUTPUT) && f->rec && f->rlen > 0) {
            line(' ', "      THE RECORD READ LAST");
            hex((const unsigned char *)f->rec, f->rlen);
        }
    }
    line(' ', "* * * END OF FILES * * *");
}

/* each active block's frame, from its start to its next available byte */
static void frames(void)
{
    char m[80];
    line('0', "* * * FRAMES OF THE ACTIVE BLOCKS * * *");
    for (char *fr = pl1_curframe; fr; fr = *(char **)(fr + PL1_FR_CURF)) {
        const struct pinfo *pi = *(const struct pinfo **)(fr + PL1_FR_PINFO);
        char *nab = *(char **)(fr + FR_NAB);
        int n = (int)(((unsigned)(size_t)nab - (unsigned)(size_t)fr) & 0xFFFFFFu);
        if (!pi)
            break;
        if (n < PL1_FR_VARS || n > 0x10000)
            n = PL1_FR_VARS;
        snprintf(m, sizeof m, " %s OF PROCEDURE %.*s",
                 pi->len & PL1_PI_BLOCK ? "BLOCK" : "FRAME", pi->len & PL1_PI_LEN, pi->name);
        line('0', m);
        hex((const unsigned char *)fr, n);
        if ((pi->len & (PL1_PI_MAIN | PL1_PI_BLOCK)) == PL1_PI_MAIN)
            break;                    /* the main procedure, not a block in it */
    }
}

static int has_dd(void)
{
#ifdef __MVS__
    return get_dsab(NULL, "PLIDUMP") || get_dsab(NULL, "PL1DUMP");
#else
    return 1;
#endif
}

void pl1_plidump(const char *opt, int olen, const char *id, int idlen)
{
    int t = 1, f = 1, h = 0, b = 0, stop = 0, neg = 0;
    char m[160];

    for (int i = 0; i < olen; i++) {
        int c = toupper((unsigned char)opt[i]);
        switch (c) {
        case 'N': neg = 1; continue;
        case 'T': t = !neg; break;
        case 'F': f = !neg; break;
        case 'H': h = !neg; break;
        case 'B': b = !neg; break;
        case 'S': case 'E': stop = 1; break;
        case 'C': stop = 0; break;
        default: break;               /* A, O, and what is not an option */
        }
        neg = 0;
    }
    /* opened by the first dump, the next ones follow it; closed at
     * the end of the program */
    if (!dfp && !tried && has_dd()) {
#ifdef __MVS__
        dfp = fopen(get_dsab(NULL, "PLIDUMP") ? "DD:PLIDUMP" : "DD:PL1DUMP",
                    "w,recfm=vba,lrecl=125,blksize=129");
#else
        dfp = fopen("PLIDUMP", "w");
#endif
    }
    tried = 1;
    if (!dfp) {
        pl1i_message("IBM013I NO SUITABLE PLIDUMP DD CARD.");
    } else {
        line('1', "                                        * * * PL/I DUMP * * *");
        if (idlen >= 0) {
            snprintf(m, sizeof m, " USER IDENTIFIER: %.*s", idlen > ID_MAX ? ID_MAX : idlen, id);
            line(' ', m);
        }
        if (t)
            trace(h, b);
        if (f)
            file_info();
        if (h)
            frames();
        line('0', "                                        * * * END OF PL/I DUMP * * *");
        fflush(dfp);
    }
    if (stop)
        pl1i_quit(1000);
}
