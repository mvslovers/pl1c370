/* pl1iof.h -- the internals the parts of PL1C/370's I/O run time share
 * (pl1io.c, pl1ioed.c, pl1ioget.c, pl1iokey.c).  Each shared name has an
 * external name of its own: PL1X... */
#ifndef PL1IOF_H
#define PL1IOF_H

#define LINESIZE 120
#define TAB      24
#define PAGESIZE 60      /* lines per page of a PRINT file (JOB00110) */

enum { M_PUTLIST, M_PUTEDIT, M_GETLIST, M_GETEDIT };

/* KEYED files (pl1iokey.c): op 0 open (arg: output), 1 close, 2 read
 * a SEQUENTIAL INDEXED record (*data), 3 rewrite it (src, len), 4 write
 * the record LOCATE KEYFROM left (src, len) */
struct pfile;
typedef int (*pl1x_khook)(struct pfile *f, int op, int arg, const void *src, int len,
                          const unsigned char **data);

struct pfile {
    char  dd[9];
    int   flags;
    FILE *fp;
    int   isstd, open, eof;
    unsigned char *qdcb;       /* UPDATE: the DCB of rt/pl1qsam.asm */
    unsigned char *qrec;       /* ... the record GET located, its data */
    int   qlen;
    int   qmode;               /* qdcb open: 1 UPDATE, 2 INPUT (REGIONAL) */
    /* REGIONAL(1): rt/pl1bdam.asm's area, the record buffer and its
     * length (reglen, BLKSIZE), what the file does (R_*), the next region of a
     * SEQUENTIAL OUTPUT file, the region of the record read last + 1 */
    unsigned char *rarea, *rbuf;
    int   reglen, rkind, rnext, trel;
    /* INDEXED: KEYLEN and LRECL, the last key written (creation), the
     * key of the record read last (its data is qrec), a key buffer */
    int   keylen, ilrecl, haslast;
    int   irkp, iblk, ilog, idoff, iread; /* INDEXED: RKP, blocked, LRECL, where
                                  the program's data starts in the logical record,
                                  a READ KU just done (blocked REWRITE) */
    unsigned char *irecp;      /* ... the logical record BISAM read */
    int   rel, rpt;            /* REGIONAL(2), (3): DCBREL, blocks or tracks;
                                  records per track */
    unsigned char *lastkey, *irec, *kbuf;
    /* output */
    char *line;          /* width + 1 positions */
    int   width;         /* LINESIZE, or the length of a STRING target */
    int   end, pstart, started;
    int   tail;                /* PRINT: a LIST or DATA item ended the line */
    int   pagesize;            /* PRINT: lines per page (PAGESIZE, 60) */
    int   pgdone;              /* ENDPAGE's on-unit ran for this page */
    void *desc;                /* the descriptor, for ENDPAGE */
    char  asa;
    int   lineno;        /* PRINT: the line the last record printed on */
    int   count;         /* COUNT: the items of its last GET or PUT */
    int   pageno;        /* PAGENO: 1 when opened, one more for each new page */
    short tabs[16];      /* ANSI's TAB option: the tab positions, from 1 */
    int   ntabs;
    /* input */
    char *rec;           /* recb, or the STRING source */
    char  recb[264];
    int   isstring;
    int   rlen, rpos, have;
    pl1x_khook khook;    /* KEYED: the hook of its descriptor */
    /* LOCATE: the buffer of the record located, written by the next
     * WRITE, LOCATE or CLOSE */
    unsigned char *locbuf, *locstart;
    int   loclen, locpend;
    int   lockeyed;            /* ... with KEYFROM, its key in lockey (INDEXED) */
    unsigned char *lockey;
    struct pfile *next;
};


struct desc {                    /* as the compiler lays it out */
    char  name[8];
    int   flags;
    struct pfile *h;
    pl1x_khook khook;            /* V(PL1KHOK) for a REGIONAL or INDEXED file */
    int   recfm, lrecl, blksize; /* the ENVIRONMENT's DCB, recfm 0 none
                                    (rt/pl1abi.h PL1_RECFM_*) */
};

enum { R_NONE, R_CREATE, R_DIRECT, R_SEQ, R_ICREATE, R_IDIRECT, R_ISEQ,
       R_KCREATE, R_KDIRECT, R_KSEQ };      /* R_K...: REGIONAL(2) and (3) */

/* BISAM puts an unblocked record 16 bytes into the area: the system's
 * prefix, the overflow link among it (JOB00522) */
#define BDATA 16

enum { K_END, K_A, K_AW, K_F, K_X, K_COL, K_SKIP, K_PAGE, K_LINE, K_E, K_P, K_B, K_BW,
       K_REP,     /* (expression) item: its count, then the length of the group */
       K_TAB,     /* ANSI's TAB(w) (Tier 72) */
       K_C };     /* C(item,item): its two data items follow (#2 C4) */
#define MAXREP 8
extern short rst[MAXREP][3] asm("PL1XRST");  /* the repetitions running: start,
                                                end, count left */
extern int rsp asm("PL1XRSP");

void pl1x_flushloc(struct pfile *f) asm("PL1XFLOC");   /* pl1io.c */

/* shared state and helpers (pl1io.c unless noted) */
extern struct pfile *cur asm("PL1XCUR");
extern int mode asm("PL1XMODE");
extern const short *fmt asm("PL1XFMT");      /* the format list of the statement */
extern int fi asm("PL1XFI");
extern int pl1x_bits asm("PL1XBITS");   /* a FIXED value's length as a bit
                                           string, for the B format; else 0 */
extern int data_mode asm("PL1XDATM");
struct pfile *of(void *d) asm("PL1XOF");
void open_file(struct pfile *f, int output) asm("PL1XOPNF");
struct pfile *out_file(void) asm("PL1XOUTF");
void emit(struct pfile *f) asm("PL1XEMIT");
void place(const char *p, int len) asm("PL1XPLAC");
/* PUT EDIT's placing of a value: set by pl1_format_begin (pl1ioed.c),
 * so that a program without EDIT does not link it */
extern void (*pl1x_edit)(const char *text, int tlen, const char *d, int nd, int q, int neg,
                         int numeric) asm("PL1XEDIT");
int next_data(int wrap) asm("PL1XNXTD");      /* pl1ioed.c */

void pl1i_tab(struct pfile *f, int w) asm("PL1ITAB");
extern int pl1i_place_num asm("PL1IPNM");
void pl1i_sysabend(unsigned code)                     asm("PL1ISAB");
extern FILE *pl1i_failed_fp                           asm("PL1IFFP");

#endif
