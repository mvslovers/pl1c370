/* pl1io.c -- stream input and output of PL1C/370's run time.
 *
 * The rules are measured against PL/I (F) V5.5, not taken from the manual:
 * internals/research/pli/putlist-measurement.md (PUT LIST to a PRINT file)
 * and tier4-measurement.md (PUT EDIT, GET, files that are not PRINT).
 *
 *  - A PRINT file (SYSPRINT) is RECFM=VBA; the first byte of a record is
 *    the ASA control character, written here (LIBC/370 passes it through).
 *    It starts on a new page: the first record carries '1'.  A line holds
 *    120 positions; LIST items start at tab positions 1, 25, 49, 73, 97.
 *  - A file that is not PRINT has no control characters and no tabs: LIST
 *    items are separated by one blank, strings are written in quotes.
 *  - PUT EDIT places each item at the current position, as its format item
 *    says; F(w,d) rounds, and a field too narrow keeps its rightmost
 *    digits.
 *  - GET LIST reads items separated by blanks or commas across records;
 *    GET EDIT reads fields of the widths its formats give.
 *
 * A file is a descriptor in the program (32 bytes: the DD name, the
 * attribute flags, a word for this run time's state); generated code
 * selects one before each GET or PUT statement.
 */
#include <errno.h>
#ifdef __MVS__
#include <mvs/file.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef __MVS__
#include <mvs/dd.h>
#endif

#include "pl1rt.h"
#include "pl1pic.h"
#include "pl1int.h"
#include "pl1flt.h"

#include "pl1iof.h"

static void write_check(struct pfile *f);

static struct pfile *files, sysprint_f;
struct pfile *cur;
void (*pl1x_edit)(const char *, int, const char *, int, int, int, int);
int mode;
const short *fmt;
int fi;
int pl1x_bits;
short rst[MAXREP][3];
int rsp;
static int finishing;

/* PUT DATA: the "name=" an item goes with, and the data mode itself;
 * GET DATA: ';' ends a value as a blank does */
static char dprefix[320];
static int dplen;
int data_mode;

/* ---- files ------------------------------------------------------------ */

static void line_buffer(struct pfile *f, int width)
{
    if (!f->line || width > f->width) {
        free(f->line);
        f->line = malloc((size_t)width + 1);
        if (!f->line) {
            fputs("PL1C/370: out of storage for a line\n", stderr);
            exit(16);
        }
    }
    f->width = width;
    f->rec = f->recb;
}

/* A file's line size before OPEN's LINESIZE: for a PRINT file with a
 * record format in its ENVIRONMENT the record's, less the control
 * character and a V record's RDW (F(41): 40, JOB01874); else 120. */
static int default_width(const struct pfile *f)
{
    const struct desc *d = f->desc;
    int w;
    if (!d || !d->recfm || !(d->flags & FL_PRINT))
        return LINESIZE;
    w = d->lrecl - 1 - ((d->recfm & PL1_RECFM_U) == PL1_RECFM_V ? 4 : 0);
    return w > 0 ? w : LINESIZE;
}

static void reset_output(struct pfile *f)
{
    if (!f->line)
        line_buffer(f, default_width(f));
    memset(f->line, ' ', (size_t)f->width + 1);
    f->end = 0;
    f->pstart = -1;
    f->asa = '1';
    f->pageno = 0;
}

static struct pfile *new_file(const char *dd, int flags, void *desc)
{
    struct pfile *f = calloc(1, sizeof *f);
    if (!f) {
        fputs("PL1C/370: out of storage for a file\n", stderr);
        exit(16);
    }
    memcpy(f->dd, dd, 8);
    f->dd[8] = '\0';
    for (int i = 7; i >= 0 && f->dd[i] == ' '; i--)
        f->dd[i] = '\0';
    f->flags = flags;
    /* before reset_output: a PRINT file's line size comes from the
     * ENVIRONMENT in its descriptor (default_width; t247env) */
    f->desc = desc;
    reset_output(f);
    f->next = files;
    files = f;
    return f;
}

struct pfile *of(void *d)
{
    struct desc *ds = d;
    if (!ds->h) {
        if (memcmp(ds->name, "SYSPRINT", 8) == 0) {
            ds->h = &sysprint_f;
            if (!sysprint_f.dd[0]) {
                memcpy(sysprint_f.dd, "SYSPRINT", 9);
                sysprint_f.flags = ds->flags | FL_PRINT | FL_OUTPUT;
                reset_output(&sysprint_f);
                sysprint_f.next = files;
                files = &sysprint_f;
            }
        } else {
            ds->h = new_file(ds->name, ds->flags, d);
        }
    }
    ds->h->desc = d;
    return ds->h;
}

static void finish_all(void);
static void flush_locate(struct pfile *f);

/* The DCB of the file's ENVIRONMENT (#215) as LIBC/370's open options,
 * which it places in the DCB before OPEN: they win over the DD and the
 * data set, as PL/I (F)'s do (JOB01876). */
static void env_mode(struct pfile *f, char *mode, size_t n)
{
    const struct desc *d = f->desc;
    char rf[8];
    int k = 0, r;

    if (!d || !d->recfm)
        return;
    r = d->recfm;
    rf[k++] = (r & PL1_RECFM_U) == PL1_RECFM_U ? 'u' : r & PL1_RECFM_F ? 'f' : 'v';
    if (r & PL1_RECFM_B) rf[k++] = 'b';
    if (r & PL1_RECFM_S) rf[k++] = 's';
    if (r & PL1_RECFM_A) rf[k++] = 'a';
    if (r & PL1_RECFM_M) rf[k++] = 'm';
    rf[k] = 0;
    snprintf(mode + strlen(mode), n - strlen(mode), ",recfm=%s,lrecl=%d,blksize=%d", rf,
             d->lrecl, d->blksize);
}

/* SYSPRINT has no DD.  Asked of the job's DD chain, not of stdout:
 * LIBC/370 opens the standard streams on first use (2.15.0), and even a
 * look at stdout's flags would open it -- and allocate a SYSOUT data
 * set where PL/I (F) has none. */
static int no_sysprint_dd(void)
{
#ifdef __MVS__
    return get_dsab(NULL, "SYSPRINT") == NULL;
#else
    return 0;
#endif
}

void open_file(struct pfile *f, int output)
{
    char name[16];

    if (f->open)
        return;
    if (f->flags & (FL_REGIONAL | FL_INDEXED)) {
        /* pl1iokey.c, through the descriptor (the compiler's V(PL1KHOK)) */
        f->khook = f->desc ? ((struct desc *)f->desc)->khook : NULL;
        if (!f->khook) {
            fprintf(stderr, "PL1C/370: file %s: no KEYED support linked\n", f->dd);
            exit(16);
        }
        f->khook(f, 0, output, NULL, 0, NULL);
        return;
    }
    if (!finishing) {
        static int registered;
        if (!registered) {
            registered = 1;
            atexit(finish_all);
        }
    }
    /* SYSPRINT and SYSIN are already open as stdout and stdin (the C
     * start-up holds the DDs; a second open of SYSIN fails, JOB00117), so
     * a RECORD file on them goes through those streams too. */
    if (strcmp(f->dd, "SYSPRINT") == 0 && output) {
        /* no SYSPRINT DD: UNDEFINEDFILE, its message on the console
         * (#220, JOB02085) */
        if (no_sysprint_dd()) {
            pl1i_nosysprint = 1;
            f->fp = NULL;
        } else {
            f->fp = stdout;
            f->isstd = 1;
        }
    } else if (strcmp(f->dd, "SYSIN") == 0 && !output) {
        f->fp = stdin;
        f->isstd = 1;
    } else {
        snprintf(name, sizeof name, "DD:%s", f->dd);
        if (f->flags & FL_UPDATE) {
            /* QSAM opened UPDAT (rt/pl1qsam.asm): READ, then REWRITE */
            char dd[8];
            memset(dd, ' ', sizeof dd);
            memcpy(dd, f->dd, strlen(f->dd) > 8 ? 8 : strlen(f->dd));
            if (!f->qdcb)
                f->qdcb = malloc(128);
            if (!f->qdcb || pl1_qopen(f->qdcb, dd) != 0) {
                if (f->desc)
                    pl1i_filecond(C_UNDF, 84, f->desc);
                fprintf(stderr, "PL1C/370: file %s cannot be opened for UPDATE (DD %s "
                        "missing?)\n", f->dd, f->dd);
                exit(16);
            }
            f->flags |= FL_INPUT;
            f->open = 1;
            f->eof = 0;
            f->have = 0;
            f->qrec = NULL;
            f->qmode = 1;
            f->fp = NULL;
            return;
        } else {
            char mode[64];
            snprintf(mode, sizeof mode, "%s", f->flags & FL_RECORD ?
                     (output ? "wb,record" : "rb,record") : (output ? "w" : "r"));
            env_mode(f, mode, sizeof mode);
            f->fp = fopen(name, mode);
        }
    }
    if (!f->fp) {
        /* UNDEFINEDFILE, ONCODE 84 (Q2UNDF); a normal return of the
         * on-unit is not measured: the program ends */
        if (f->desc)
            pl1i_filecond(C_UNDF, 84, f->desc);
        fprintf(stderr, "PL1C/370: file %s cannot be opened (DD %s missing?)\n", f->dd, f->dd);
        exit(16);
    }
    if (output && !(f->flags & FL_INPUT))
        f->flags |= FL_OUTPUT;
    if (!output)
        f->flags |= FL_INPUT;
    f->open = 1;
    f->eof = 0;
    f->have = 0;
    reset_output(f);
}

/* SYSPRINT when nothing selected a file (the host tests). */
static struct desc sysprint_desc = {
    { 'S', 'Y', 'S', 'P', 'R', 'I', 'N', 'T' }, FL_PRINT | FL_OUTPUT | FL_STREAM, NULL, NULL, 0, 0, 0
};

struct pfile *out_file(void)
{
    if (!cur)
        cur = of(&sysprint_desc);
    if (!cur->open)
        open_file(cur, 1);
    return cur;
}

#ifndef _FILE_RECFM_TYPE
/* the host tests: no QSAM (rt/pl1qsam.asm is for MVS) */
int pl1_qopen(void *dcb, const char *dd8) { (void)dcb; (void)dd8; return 1; }
int pl1_qopeni(void *dcb, const char *dd8) { (void)dcb; (void)dd8; return 1; }
int pl1_iopen(void *a, const char *dd8, int mode) { (void)a; (void)dd8; (void)mode; return 1; }
void pl1_iinfo(void *a, int *v) { (void)a; v[0] = v[1] = v[2] = v[3] = 0; }
int pl1_iput(void *a, void *rec) { (void)a; (void)rec; return 0; }
int pl1_iget(void *a, unsigned char **rec) { (void)a; (void)rec; return 1; }
void pl1_iputx(void *a) { (void)a; }
int pl1_ibisam(void *a, int op, void *area, void *key, unsigned char **rec) { (void)a; (void)op; (void)area; (void)key; (void)rec; return 0x8000; }
int pl1_isetk(void *a, void *key) { (void)a; (void)key; return 1; }
void pl1_iclose(void *a) { (void)a; }
int pl1_bopen(void *a, const char *dd8, int mode) { (void)a; (void)dd8; (void)mode; return 1; }
int pl1_bwrite(void *a, void *buf, int dummy) { (void)a; (void)buf; (void)dummy; return 8; }
int pl1_bdread(void *a, void *buf, int block) { (void)a; (void)buf; (void)block; return 1; }
int pl1_bdwrite(void *a, void *buf, int block) { (void)a; (void)buf; (void)block; return 1; }
void pl1_bclose(void *a) { (void)a; }
int pl1_qget(void *dcb, unsigned char **rec) { (void)dcb; (void)rec; return 1; }
void pl1_qputx(void *dcb) { (void)dcb; }
void pl1_qclose(void *dcb) { (void)dcb; }
int pl1_kopen(void *a, const char *dd8, int mode, int limct) { (void)a; (void)dd8; (void)mode; (void)limct; return 1; }
int pl1_kio(void *a, int op, void *key, void *data, int *ref) { (void)a; (void)op; (void)key; (void)data; (void)ref; return 1; }
#define __fx37(fp) ((void)(fp), 0u)   /* libc370's, MVS only */
#endif

static int is_print(struct pfile *f) { return (f->flags & FL_PRINT) != 0; }

/* Write the current line as a record.  A PRINT file starts a new page
 * when the record would print below line 60: PL/I (F) puts '1' on every
 * 60th record of single-spaced output (JOB00110). */
void emit(struct pfile *f)
{
    int n = f->end;
    if (is_print(f)) {
        /* the blank after a LIST or DATA item stays, and with its ASA
         * character a record is at least 10 bytes (JOB00347, JOB00348) */
        if (f->tail && n < f->width)
            n++;
        if (n < 9)
            n = 9;
    } else if (n < 10) {
        /* a record of a file that is not PRINT is at least 10 bytes,
         * blank-padded (JOB00128) */
        n = 10;
    }
    if (is_print(f)) {
        int adv = f->asa == '0' ? 2 : f->asa == '-' ? 3 : f->asa == '+' ? 0 : 1;
        int ps = f->pagesize > 0 ? f->pagesize : PAGESIZE;
        /* past the page: a new one, ENDPAGE's system action -- unless its
         * on-unit ran and started none (JOB00368) */
        if (f->asa != '1' && f->lineno + adv > ps && !f->pgdone)
            f->asa = '1';
        if (f->asa == '1') {
            f->pgdone = 0;
            if (f->started)            /* PAGENO: the first page is 1 */
                f->pageno = (f->pageno ? f->pageno : 1) + 1;
        }
        f->lineno = f->asa == '1' ? 1 : f->lineno + adv;
        fputc(f->asa, f->fp);
    }
    fwrite(f->line, 1, (size_t)n, f->fp);
    fputc('\n', f->fp);
    write_check(f);
    memset(f->line, ' ', (size_t)f->width + 1);
    f->end = 0;
    f->pstart = -1;
    f->asa = ' ';
    f->started = 1;
    f->tail = 0;
}

/* COUNT: the items of the GET or PUT running, which the generated code
 * counts in pl1_icount, on the file count_file; each file keeps its
 * count when the next statement starts */
int pl1_icount asm("PL1ICNT");
static struct pfile *count_file;

static void count_to(struct pfile *f)
{
    if (count_file)
        count_file->count = pl1_icount;
    count_file = f;
    pl1_icount = 0;
}

int pl1_count(void *d)
{
    struct pfile *f = of(d);
    return f == count_file ? pl1_icount : f->count;
}

/* LINENO: the line the next item goes to -- the line of the record
 * being built (Q1MISC: after SKIP(0) the line printed last) */
int pl1_lineno(void *d)
{
    struct pfile *f = of(d);
    int adv = f->asa == '0' ? 2 : f->asa == '-' ? 3 : f->asa == '+' ? 0 : 1;
    int ps = f->pagesize > 0 ? f->pagesize : PAGESIZE;
    /* past the page, the record goes to line 1 of the next, as emit
     * puts it there (Q1LSZ, JOB01120: 1 after line 6 of PAGESIZE(6)) */
    if (f->asa == '1' || (f->lineno + adv > ps && !f->pgdone))
        return 1;
    return f->lineno + adv;
}

/* PAGENO (ANSI 9.4.4.62): the number of the page the next item goes to
 * -- one more after PAGE than before, as LINENO counts */
int pl1_pageno(void *d)
{
    struct pfile *f = of(d);
    int n = f->pageno ? f->pageno : 1;
    return f->asa == '1' && f->started ? n + 1 : n;
}

/* PAGENO(f) = n (7.5.4.4): that page's number n, the next ones n + 1 ... */
void pl1_set_pageno(void *d, int n)
{
    struct pfile *f = of(d);
    f->pageno = f->asa == '1' && f->started ? n - 1 : n;
}

void pl1_select(void *d, int m)
{
    /* stream I/O polls for an attention (SC33-0006-7, the INTERRUPT
     * option: "during the execution of stream I/O for all modules") */
    if (pl1_attn)
        pl1_attn_poll();
    pl1i_copy = 0;
    cur = of(d);
    count_to(cur);
    mode = m;
    fmt = NULL;
    fi = 0;
    rsp = 0;
    data_mode = 0;
    dplen = 0;
}

/* ---- the STRING option ------------------------------------------------- */

/* A STRING is a file of one record that is not PRINT: kind 0 a CHARACTER
 * string of len, 1 a VARYING one (its length prefix at s, len its
 * maximum). */
static struct pfile string_f;
static char *s_target;
static int s_kind, s_max;

void pl1_select_string(void *s, int len, int kind, int m)
{
    pl1i_copy = 0;
    struct pfile *f = &string_f;
    cur = f;
    count_to(NULL);
    mode = m;
    fmt = NULL;
    fi = 0;
    rsp = 0;
    data_mode = 0;
    dplen = 0;
    f->isstring = 1;
    f->open = 1;
    f->eof = 0;
    memcpy(f->dd, "STRING", 7);
    if (m == M_PUTLIST || m == M_PUTEDIT) {
        f->flags = FL_STREAM | FL_OUTPUT;
        line_buffer(f, len > 0 ? len : 1);
        reset_output(f);
        f->asa = ' ';
        s_target = s;
        s_kind = kind;
        s_max = len;
    } else {
        const unsigned char *b = s;
        int n = kind ? (b[0] << 8) | b[1] : len;
        f->flags = FL_STREAM | FL_INPUT;
        line_buffer(f, n > 0 ? n : 1);
        f->rec = f->line;
        memcpy(f->rec, kind ? (const char *)b + 2 : (const char *)s, (size_t)n);
        f->rlen = n;
        f->rpos = 0;
        f->have = 1;
    }
}

/* The end of PUT STRING: the record becomes the string, blank-padded, or
 * a VARYING string of its length. */
void pl1_string_end(void)
{
    struct pfile *f = &string_f;
    if (s_kind == 0) {
        memcpy(s_target, f->line, (size_t)s_max);
    } else {
        int n = f->end < s_max ? f->end : s_max;
        unsigned char *b = (unsigned char *)s_target;
        b[0] = (unsigned char)(n >> 8);
        b[1] = (unsigned char)n;
        memcpy(b + 2, f->line, (size_t)n);
    }
    cur = NULL;
}

/* ---- data-directed ------------------------------------------------------ */

/* The name of the next PUT DATA item, with the values of its subscripts. */
void pl1_data_name(const char *name, int len, int nsubs, int s1, int s2, int s3)
{
    int sub[3] = { s1, s2, s3 };
    int n = len > 280 ? 280 : len;
    data_mode = 1;
    memcpy(dprefix, name, (size_t)n);
    for (int i = 0; i < nsubs && i < 3; i++)
        n += snprintf(dprefix + n, sizeof dprefix - (size_t)n, "%c%d", i ? ',' : '(', sub[i]);
    if (nsubs)
        dprefix[n++] = ')';
    dprefix[n++] = '=';
    dplen = n;
}

/* The end of PUT DATA: a semicolon after the last item (JOB00197). */
void pl1_data_end(void)
{
    struct pfile *f = out_file();
    if (is_print(f) || f->end == 0 || f->line[f->end - 1] != ' ') {
        if (f->end >= f->width && !f->isstring)
            emit(f);
        if (f->end < f->width)
            f->line[f->end++] = ';';
        f->tail = 0;
    } else {
        f->line[f->end - 1] = ';';     /* in place of the blank after the item,
                                         * none after it (JOB00201) */
    }
    f->pstart = f->end - 1;
    data_mode = 0;
    dplen = 0;
}

void pl1_open(void *d, int attrs, const char *title, int tlen)
{
    struct pfile *f = of(d);
    if (title && tlen > 0) {
        memset(f->dd, 0, sizeof f->dd);
        memcpy(f->dd, title, (size_t)(tlen > 8 ? 8 : tlen));
    }
    /* each OPEN: the declared attributes and its own (JOB00355: OUTPUT,
     * then UPDATE, then INPUT on one file); LINESIZE, PAGESIZE and TAB
     * come after it (pl1_open_opts) */
    f->pagesize = 0;
    f->ntabs = 0;
    if (!f->isstring)
        line_buffer(f, default_width(f));
    f->flags = ((struct desc *)d)->flags | attrs;
    open_file(f, !(f->flags & (FL_INPUT | FL_UPDATE)));
}

/* OPEN's LINESIZE(ls), PAGESIZE(ps) (0: not given) and ANSI's TAB(t1,
 * ...): a line of ls positions (Q1LSZ, JOB01120: tab positions 1, 25, 49
 * in LINESIZE(50)), ps lines to a page. */
void pl1_open_opts(void *d, int ls, int ps, const int *tabs, int ntabs)
{
    struct pfile *f = of(d);
    if (ls > 0) {
        line_buffer(f, ls);
        memset(f->line, ' ', (size_t)f->width + 1);
    }
    if (ps > 0)
        f->pagesize = ps;
    f->ntabs = 0;
    for (int i = 0; i < ntabs && i < 16; i++)
        if (tabs[i] > 0 && (f->ntabs == 0 || tabs[i] > f->tabs[f->ntabs - 1]))
            f->tabs[f->ntabs++] = (short)tabs[i];
}

/* ANSI 8.7.2.10.1 output-tab: the column (from 0) of the first tab
 * position t with t - 1 > cc in the line, or -1 */
static int next_tab(struct pfile *f, int cc)
{
    for (int i = 0; i < f->ntabs; i++)
        if (f->tabs[i] - 1 > cc && f->tabs[i] - 1 < f->width)
            return f->tabs[i] - 1;
    return -1;
}

/* ANSI 8.7.2.10 tab(w): w tab positions on, or a new line at its first
 * tab position.  Without a TAB option the positions are PL/I (F)'s
 * LIST positions, 1, 25, 49, ... */
void pl1i_tab(struct pfile *f, int w)
{
    int cc = f->end, n = 0, c = cc;
    short dflt[16];
    int saved = f->ntabs;
    if (w <= 0)
        return;
    if (!saved) {
        for (int t = 1; t - 1 < f->width && f->ntabs < 16; t += TAB)
            dflt[f->ntabs++] = (short)t;
        memcpy(f->tabs, dflt, sizeof dflt);
    }
    for (int i = 0; i < f->ntabs; i++)
        if (f->tabs[i] - 1 > cc && f->tabs[i] - 1 < f->width)
            n++;
    if (n >= w) {
        while (w-- > 0)
            c = next_tab(f, c);
        f->end = c;
    } else {
        emit(f);
        f->end = f->tabs[0] != 1 ? next_tab(f, 0) : 0;
        if (f->end < 0)
            f->end = 0;
    }
    f->ntabs = saved;
}

void pl1_close(void *d)
{
    struct pfile *f = of(d);
    if (!f->open)
        return;
    flush_locate(f);
    if (f->rkind != R_NONE) {
        f->khook(f, 1, 0, NULL, 0, NULL);
        return;
    }
    if (f->qdcb && (f->flags & FL_UPDATE)) {
        pl1_qclose(f->qdcb);
        f->qmode = 0;            /* a later OPEN INPUT reads a stream again */
        f->open = 0;
        return;
    }
    if ((f->flags & FL_OUTPUT) && (is_print(f) || f->end > 0 || f->pstart >= 0))
        emit(f);
    if (f->isstd)
        fflush(f->fp);
    else
        fclose(f->fp);
    f->open = 0;
    f->pagesize = 0;           /* an implicit OPEN next takes the defaults */
    f->ntabs = 0;
    if (f->line && !f->isstring)
        line_buffer(f, default_width(f));
}

static void finish_all(void)
{
    static int done;
    if (done)
        return;                   /* once: explicitly, or at exit */
    done = 1;
    finishing = 1;
    for (struct pfile *f = files; f; f = f->next) {
        if (f->open)
            flush_locate(f);
        if (f->open && f->rkind == R_CREATE)
            f->khook(f, 1, 0, NULL, 0, NULL);  /* the rest of the space, dummies */
        if (!f->open || !(f->flags & FL_OUTPUT))
            continue;
        if (is_print(f) || f->end > 0 || f->pstart >= 0)
            emit(f);              /* a PRINT file's last line, empty or not */
        fflush(f->fp);
    }
}

struct pfile *pl1i_files(void)
{
    return files;
}

void pl1_finish(void)
{
    finish_all();
}

/* ---- PUT: line control ----------------------------------------------- */

void pl1_skip(int n)
{
    struct pfile *f = out_file();
    if (!is_print(f)) {
        /* not PRINT: one record per line skipped, no control characters */
        emit(f);
        for (; n > 1; n--)
            emit(f);
        return;
    }
    emit(f);
    switch (n) {
    case 0:  f->asa = '+'; break;
    case 1:  f->asa = ' '; break;
    case 2:  f->asa = '0'; break;
    case 3:  f->asa = '-'; break;
    default:
        /* as many empty records with '-' as leave at most three lines,
         * then the rest on the next record: SKIP(4) '-' ' ' (JOB00067),
         * SKIP(5) '-' '0', (6) '-' '-', (7) '-' '-' ' ', (10) '-' '-' '-' ' '
         * (Q4SKIP, JOB00232) */
        for (; n > 3; n -= 3) {
            f->asa = '-';
            emit(f);
        }
        f->asa = n == 1 ? ' ' : n == 2 ? '0' : '-';
        break;
    }
    {
        /* the new line past the page: ENDPAGE, if an on-unit is there
         * (JOB00368); without one the record starts a new page */
        int adv = f->asa == '0' ? 2 : f->asa == '-' ? 3 : f->asa == '+' ? 0 : 1;
        int ps = f->pagesize > 0 ? f->pagesize : PAGESIZE;
        if (!f->pgdone && f->asa != '1' && f->lineno + adv > ps && f->desc) {
            f->pgdone = 1;
            if (!pl1i_endpage(f->desc))
                f->pgdone = 0;
        }
    }
}

/* LINE(n) of a PRINT file (the option and the format item): the n-th
 * line of the page; n <= 0 is 1.  The current line itself: as SKIP(0),
 * written over, data on it or not.  A line already passed, or one past
 * the page: lines skipped until the next would be past the page, then
 * ENDPAGE as SKIP raises it (Q3LINE, Q4SKIP, JOB00229, JOB00232). */
void pl1_line(int n)
{
    struct pfile *f = out_file();
    int adv, ps, cur;
    if (!is_print(f)) {
        pl1_skip(1);
        return;
    }
    if (n <= 0)
        n = 1;
    adv = f->asa == '0' ? 2 : f->asa == '-' ? 3 : f->asa == '+' ? 0 : 1;
    ps = f->pagesize > 0 ? f->pagesize : PAGESIZE;
    cur = f->asa == '1' ? 1 : f->lineno + adv;
    if (n == cur)
        pl1_skip(0);
    else if (n > cur && n <= ps)
        pl1_skip(n - cur);
    else
        pl1_skip(ps + 1 - cur);
}

void pl1_page(void)
{
    /* the current line is written, empty or not: at the start of the
     * file an empty record, as after a SKIP (JOB00368) */
    struct pfile *f = out_file();
    emit(f);
    f->asa = '1';
}

/* ---- PUT LIST ---------------------------------------------------------- */

int pl1i_place_num;

/* Place one item of len characters.  PRINT: at the first tab position
 * that is not before the end of the previous item and lies after its
 * start (so an empty item still takes a tab position).  Not PRINT: one
 * blank after the previous item. */
void place(const char *p, int len)
{
    struct pfile *f = out_file();
    int col;
    char item[2 * LINESIZE + 340];

    if (dplen) {
        /* PUT DATA: name=value is one item */
        int n = len > (int)sizeof item - dplen ? (int)sizeof item - dplen : len;
        memcpy(item, dprefix, (size_t)dplen);
        memcpy(item + dplen, p, (size_t)n);
        p = item;
        len = dplen + n;
        dplen = 0;
    }
    if (is_print(f)) {
        /* the first tab position after the end of the previous item; the
         * positions run on past the end of the line, so an item that
         * ended in position 120 puts the next one at 25 on the next line
         * (Q2FILL, JOB00451) */
        col = f->pstart < 0 ? 0 : (f->end / TAB + 1) * TAB;
        if (f->ntabs && f->pstart >= 0) {
            /* ANSI's TAB option: the next tab position, or a new line
             * and its first (8.7.2.10) */
            col = next_tab(f, f->end);
            if (col < 0 && !f->isstring) {
                emit(f);
                col = f->tabs[0] != 1 ? next_tab(f, 0) : 0;
            }
            if (col < 0)
                col = 0;
        } else if (col >= f->width && !f->isstring) {
            emit(f);
            col -= f->width;
        }
        /* a number that does not fit in the rest of the line starts the
         * next one; a string goes on there (Q1LSZ, JOB01120) */
        if (pl1i_place_num && col > 0 && len > f->width - col && len <= f->width &&
            !f->isstring) {
            emit(f);
            col = 0;
        }
        /* an item longer than the rest of the line goes on in the first
         * position of the next one; it is never moved (Q2FILL A4) */
        while (len > f->width - col && !f->isstring) {
            int n = f->width - col;
            memcpy(f->line + col, p, (size_t)n);
            f->end = f->width;
            emit(f);
            p += n;
            len -= n;
            col = 0;
        }
    } else {
        col = f->end;          /* the previous item ended with its blank */
        if (col + len > f->width && col > 0 && !f->isstring) {
            emit(f);
            col = 0;
        }
    }
    if (len > f->width - col)
        len = f->width - col;      /* not measured: a STRING target, files not PRINT */
    if (len < 0)
        len = 0;
    memcpy(f->line + col, p, (size_t)len);
    f->pstart = col;
    f->end = col + len;
    if (!is_print(f) && f->end < f->width)
        f->end++;              /* not PRINT: each item is followed by a blank */
    f->tail = is_print(f);     /* PRINT: so is the last one in the record */
}

/* A string item: quoted (quotes doubled) on a file that is not PRINT. */
static void place_string(const char *p, int len, int bit)
{
    struct pfile *f = out_file();
    if (is_print(f) && !bit && !data_mode) {
        place(p, len);
        return;
    }
    {
        char b[2 * LINESIZE + 4];
        int k = 0;
        b[k++] = '\'';
        for (int i = 0; i < len && k < 2 * LINESIZE; i++) {
            if (p[i] == '\'' && !bit)
                b[k++] = '\'';
            b[k++] = p[i];
        }
        b[k++] = '\'';
        if (bit)
            b[k++] = 'B';
        place(b, k);
    }
}


void pl1_list_char(const char *p, int len)
{
    if (mode == M_PUTEDIT) {
        pl1x_edit(p, len, NULL, 0, 0, 0, 0);
        return;
    }
    place_string(p, len, 0);
}

/* A number already made into its characters: a complex value
 * (rt/pl1cx.c), placed as the other numbers are, without quotes */
void pl1_list_number(const char *p, int len)
{
    pl1i_place_num = 1;          /* moved to the next line whole if it does not fit */
    place(p, len);
    pl1i_place_num = 0;
}

void pl1_list_varchar(const void *v)
{
    const unsigned char *b = v;
    pl1_list_char((const char *)b + 2, (b[0] << 8) | b[1]);
}

void pl1_list_fixbin(int value, int prec)
{
    pl1_list_fixbin_scaled(value, prec, 0);
}

void pl1_list_fixbin_scaled(int n, int p, int q)
{
    char d[40], field[48];
    int neg, qd = pl1i_ceil_div332(q);
    int nd = pl1i_scaled_digits(d, n, q, qd, &neg);
    int pd = 1 + pl1i_ceil_div332(p);         /* FIXED BINARY(p) as decimal */

    if (mode == M_PUTEDIT) {
        int w = pl1i_format_number(field, d, nd, pd, qd, neg);
        pl1x_bits = p - q > 0 ? p - q : 0;      /* FIXED BINARY(p,q) to BIT(p-q) */
        pl1x_edit(field, w, d, nd, qd, neg, 1);
        pl1x_bits = 0;
        return;
    }
    pl1i_place_num = 1;
    place(field, pl1i_format_number(field, d, nd, pd, qd, neg));
    pl1i_place_num = 0;
}

void pl1_list_fixdec(const unsigned char *packed, int p, int q)
{
    /* every digit of the field: an even precision leaves room for one
     * more, and PL/I (F) prints it (DEC(4) holding 10000, JOB00140) */
    char d[32], field[48];
    int neg, nd = pl1i_dec_digits(d, packed, p | 1, &neg);
    int w = pl1i_format_number(field, d, nd, p, q, neg);

    if (mode == M_PUTEDIT) {
        /* FIXED DECIMAL(p,q) to BIT(ceil((p-q)*3.32)), at most 31 bits
         * (Q1FMT; T184BFM: FIXED DEC(15) gives 31) */
        pl1x_bits = p - q > 0 ? ((p - q) * 332 + 99) / 100 : 0;
        if (pl1x_bits > 31)
            pl1x_bits = 31;
        pl1x_edit(field, w, d, nd, q, neg, 1);
        pl1x_bits = 0;
        return;
    }
    pl1i_place_num = 1;
    place(field, w);
    pl1i_place_num = 0;
}

void pl1_list_bit(unsigned int v, int n)
{
    char b[40];
    for (int i = 0; i < n; i++)
        b[i] = (v & (0x80000000u >> i)) ? '1' : '0';
    if (mode == M_PUTEDIT) {
        pl1x_edit(b, n, NULL, 0, 0, 0, 0);
        return;
    }
    place_string(b, n, 1);
}

/* A bit string in storage (longer than a register, or VARYING) */
void pl1_list_bits(const unsigned char *a, int n)
{
    char *b = malloc((size_t)n + 1);
    if (!b) {
        fputs("PL1C/370: out of storage\n", stderr);
        exit(16);
    }
    for (int i = 0; i < n; i++)
        b[i] = (a[i / 8] >> (7 - i % 8)) & 1 ? '1' : '0';
    if (mode == M_PUTEDIT)
        pl1x_edit(b, n, NULL, 0, 0, 0, 0);
    else
        place_string(b, n, 1);
    free(b);
}

/* ---- the system action's messages ------------------------------------ */

/* On SYSPRINT: the line begun is ended, the (now) empty line written, the
 * message written as a record of its own, and a new empty line begun --
 * so a message has a blank line before it, and output after it starts on
 * a line of its own (JOB00137/138). */
int pl1i_nosysprint;

void pl1i_message(const char *text)
{
    struct pfile *f;

    if (pl1i_nosysprint || no_sysprint_dd()) {
        /* the run time's own message: the console, no UNDEFINEDFILE
         * (T253NSZ: IHE330I) */
        pl1i_console(text);
        return;
    }
    f = of(&sysprint_desc);

    if (!f->open)
        open_file(f, 1);
    if (f->end > 0 || f->pstart >= 0)
        emit(f);
    emit(f);
    /* a message longer than a line goes on, indented by 8, after the
     * last blank that fits (IHE212I, JOB00224) */
    for (int ind = 0;;) {
        int n = (int)strlen(text), cut = n;
        if (ind + n > LINESIZE) {
            cut = LINESIZE - ind;
            while (cut > 0 && text[cut] != ' ')
                cut--;
            if (cut == 0)
                cut = LINESIZE - ind;
        }
        f->lineno++;
        fputc(' ', f->fp);
        for (int i = 0; i < ind; i++)
            fputc(' ', f->fp);
        fwrite(text, 1, (size_t)cut, f->fp);
        fputc('\n', f->fp);
        text += cut;
        while (*text == ' ')
            text++;
        if (!*text)
            break;
        ind = 8;
    }
}

/* CHECK's system action writes the value as PUT DATA does, on a line
 * of its own after an empty one, as a message (Q1CHK) */
void pl1_check_begin(void)
{
    struct pfile *f = of(&sysprint_desc);
    if (!f->open)
        open_file(f, 1);
    if (f->end > 0 || f->pstart >= 0)
        emit(f);
    emit(f);
}

void pl1_check_end(void)
{
    struct pfile *f = of(&sysprint_desc);
    if (f->end > 0 || f->pstart >= 0)
        emit(f);
}

void pl1i_iosave(struct pl1i_iostate *s)
{
    s->cur = cur;
    s->mode = mode;
    s->fmt = fmt;
    s->fi = fi;
    s->rsp = rsp;
    memcpy(s->rst, rst, sizeof rst);
}

void pl1i_iorest(const struct pl1i_iostate *s)
{
    cur = s->cur;
    mode = s->mode;
    fmt = s->fmt;
    fi = s->fi;
    rsp = s->rsp;
    memcpy(rst, s->rst, sizeof rst);
}

/* ---- RECORD files ----------------------------------------------------- */

/* One record of a LIBC/370 record stream: a V record carries its RDW
 * (length including the four bytes); an F record is LRECL long.  From
 * the heap, at the first record: a static buffer of this size was in
 * every load module, record I/O or not. */
#define RECBUF 32768
static unsigned char *recbuf;

static void need_recbuf(void)
{
    if (!recbuf) {
        recbuf = malloc(RECBUF);
        if (!recbuf) {
            fputs("PL1C/370: out of storage for a record\n", stderr);
            exit(16);
        }
    }
}

static int is_v(FILE *fp)
{
#ifdef _FILE_RECFM_TYPE
    return (fp->recfm & _FILE_RECFM_TYPE) == _FILE_RECFM_V;
#else
    (void)fp;
    return 0;
#endif
}

static int read_record(void *d, const unsigned char **data)
{
    struct pfile *f = of(d);
    size_t n;
    if (!f->open)
        open_file(f, 0);
    need_recbuf();
    if (f->isstd) {
        /* stdin: a text stream; an F record comes with its trailing
         * blanks and a newline */
        if (f->eof || !fgets((char *)recbuf, RECBUF, f->fp)) {
            f->eof = 1;
            return -1;
        }
        *data = recbuf;
        return (int)strcspn((char *)recbuf, "\n");
    }
    if (f->rkind == R_ISEQ || f->rkind == R_KSEQ)
        return f->khook(f, 2, 0, NULL, 0, data);      /* INDEXED, REGIONAL(2), (3) */
    if (f->qdcb && f->qmode) {
        unsigned char *r;
        /* DCBRECFM X'40': V, the record with its RDW; else DCBLRECL, or
         * DCBBLKSI where no LRECL is given (a REGIONAL data set) */
        if (f->eof || pl1_qget(f->qdcb, &r) != 0) {
            f->eof = 1;
            return -1;
        }
        f->trel++;
        if (f->qdcb[36] & 0x40) {
            f->qrec = r + 4;
            f->qlen = ((r[0] << 8) | r[1]) - 4;
        } else {
            f->qrec = r;
            f->qlen = (f->qdcb[82] << 8) | f->qdcb[83];
            if (f->qlen == 0)
                f->qlen = (f->qdcb[62] << 8) | f->qdcb[63];
        }
        *data = f->qrec;
        return f->qlen;
    }
    /* one record; LIBC/370 2.14.0 returns its length (the RDW's
     * included on V), so a U record reads back at the length it was
     * written (libc370#559, t247env) */
    if (f->eof || (n = fread(recbuf, 1, RECBUF, f->fp)) == 0) {
        f->eof = 1;
        return -1;
    }
    if (is_v(f->fp)) {
        *data = recbuf + 4;
        return ((recbuf[0] << 8) | recbuf[1]) - 4;
    }
    *data = recbuf;
    return (int)n;
}

static void put_record(struct pfile *f, const void *source, int len);

/* the record LOCATE left in its buffer: written now */
static void flush_locate(struct pfile *f)
{
    if (f->locpend) {
        f->locpend = 0;
        if (f->lockeyed) {
            f->lockeyed = 0;
            f->khook(f, 4, 0, f->locstart, f->loclen, NULL);
            return;
        }
        put_record(f, f->locstart, f->loclen);
    }
}

void pl1x_flushloc(struct pfile *f)
{
    flush_locate(f);
}

/* READ ... IGNORE(n): n records read and dropped; 1 at the end of the
 * file (ENDFILE) */
int pl1_read_ignore(void *d, int n)
{
    const unsigned char *data;
    for (int i = 0; i < n; i++)
        if (read_record(d, &data) < 0)
            return 1;
    return 0;
}

/* GET ... COPY: each field as read, a line of its own on SYSPRINT after
 * the line being built (Q1IO); pl1_get_copy turns it on until the next
 * GET or PUT */
int pl1i_copy asm("PL1ICPY");
static void *copy_desc;

/* desc: ANSI's COPY(file), NULL for SYSPRINT */
void pl1_get_copy(void *desc)
{
    pl1i_copy = 1;
    copy_desc = desc;
}

void pl1i_copyline(const char *p, int n)
{
    struct pfile *f = of(copy_desc ? copy_desc : &sysprint_desc);
    if (!f->open)
        open_file(f, 1);
    if (f->end > 0 || f->pstart >= 0)
        emit(f);
    if (n > f->width)
        n = f->width;
    memcpy(f->line, p, (size_t)(n > 0 ? n : 0));
    f->end = n > 0 ? n : 0;
    emit(f);
}

int pl1_read_set(void *d, void **p)
{
    const unsigned char *data;
    if (read_record(d, &data) < 0)
        return 1;
    *p = (void *)data;
    return 0;
}

void pl1_rewrite_set(void *d)
{
    struct pfile *f = of(d);
    if (f->open && (f->rkind == R_ISEQ || f->rkind == R_KSEQ) && (f->flags & FL_UPDATE) && f->qrec) {
        f->khook(f, 3, 0, f->qrec, f->qlen, NULL);
        return;
    }
    if (!f->open || !(f->flags & FL_UPDATE) || !f->qrec || !f->qdcb) {
        fprintf(stderr, "PL1C/370: REWRITE of %.8s without a record READ for UPDATE\n",
                f->dd);
        exit(16);
    }
    pl1_qputx(f->qdcb);          /* the record in the buffer, changed there */
    f->qrec = NULL;
}

void pl1_locate(void *d, int len, int phase, void **p)
{
    struct pfile *f = of(d);
    if (!f->open)
        open_file(f, 1);
    flush_locate(f);
    if (!f->locbuf || f->loclen < len) {
        free(f->locbuf);
        f->locbuf = malloc((size_t)len + 16);
        if (!f->locbuf) {
            fputs("PL1C/370: out of storage for a record\n", stderr);
            exit(16);
        }
    }
    memset(f->locbuf, 0, (size_t)len + 16);
    f->locstart = f->locbuf + phase;
    f->loclen = len;
    f->locpend = 1;
    *p = f->locstart;
}

int pl1_read(void *d, void *target, int len)
{
    const unsigned char *data;
    int n = read_record(d, &data);
    if (n < 0)
        return 1;
    memcpy(target, data, (size_t)(n < len ? n : len));
    if (n > len)
        /* a variable smaller than the record: RECORD, ONCODE 21; after
         * a normal return the program goes on with what fitted (Q2UNDF) */
        pl1i_filecond(C_RECORD, 21, d);
    return 0;
}

int pl1_read_varying(void *d, void *target, int maxlen)
{
    const unsigned char *data;
    int n = read_record(d, &data);
    if (n < 0)
        return 1;
    pl1_assign_varchar(target, maxlen, (const char *)data, n);
    return 0;
}

static void put_record(struct pfile *f, const void *source, int len);

void pl1_write(void *d, const void *source, int len)
{
    struct pfile *f = of(d);
    if (!f->open)
        open_file(f, 1);
    flush_locate(f);
    put_record(f, source, len);
}

/* REWRITE: the record just read, replaced in place (an UPDAT DCB) */
void pl1_rewrite(void *d, const void *source, int len)
{
    struct pfile *f = of(d);
    if (f->open && (f->rkind == R_ISEQ || f->rkind == R_KSEQ) && (f->flags & FL_UPDATE) && f->qrec) {
        f->khook(f, 3, 0, source, len, NULL);
        return;
    }
    if (!f->open || !(f->flags & FL_UPDATE) || !f->qrec || !f->qdcb) {
        fprintf(stderr, "PL1C/370: REWRITE of %.8s without a record READ for UPDATE\n",
                f->dd);
        exit(16);
    }
    /* in place: the record keeps its length */
    memcpy(f->qrec, source, (size_t)(len < f->qlen ? len : f->qlen));
    pl1_qputx(f->qdcb);
    f->qrec = NULL;
}

void pl1_rewrite_varying(void *d, const void *source, int maxlen)
{
    const unsigned char *b = source;
    (void)maxlen;
    pl1_rewrite(d, b + 2, (b[0] << 8) | b[1]);
}

/* A write that failed.  PL/I (F) abends when a data set is full (D37,
 * JOB01328); libc370 turns the x37 into ENOSPC instead (its x37 exit,
 * libc370 #176).  So the run time abends itself, with the code
 * libc370's __fx37 derives as end-of-volume would (2.12.0, #539):
 * D37-04 without secondary space, B37-04 with it used up.  A PDS
 * member whose secondary space is used up abends E37 in MVS itself,
 * before libc370 sees it (libc370#543).  Another failure (EIO, a bad
 * track) abends B37 as before, not measured against PL/I (F)'s
 * TRANSMIT.  The ESTAE (rt/pl1cond.c) writes IHE012I. */
FILE *pl1i_failed_fp;                  /* the file whose write failed */

static void write_check(struct pfile *f)
{
    if (ferror(f->fp)) {
        unsigned code = __fx37(f->fp);   /* 0x00sss004 */
        pl1i_failed_fp = f->fp;
        pl1i_sysabend(code ? (code >> 12) & 0xFFF : errno == ENOSPC ? 0xD37 : 0xB37);
    }
}

static void put_record(struct pfile *f, const void *source, int len)
{
    need_recbuf();
    if (f->isstd) {
        fwrite(source, (size_t)len, 1, f->fp);   /* stdout: one line */
        fputc('\n', f->fp);
    } else if (is_v(f->fp)) {
        recbuf[0] = (unsigned char)((len + 4) >> 8);
        recbuf[1] = (unsigned char)(len + 4);
        recbuf[2] = recbuf[3] = 0;
        memcpy(recbuf + 4, source, (size_t)len);
        fwrite(recbuf, (size_t)len + 4, 1, f->fp);
    } else {
        fwrite(source, (size_t)len, 1, f->fp);
    }
    write_check(f);
}

void pl1_write_varying(void *d, const void *source, int maxlen)
{
    const unsigned char *b = source;
    int len = (b[0] << 8) | b[1];
    (void)maxlen;
    pl1_write(d, b + 2, len);
}


