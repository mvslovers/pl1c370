/* pl1c.h -- PL1C/370: shared declarations of the compiler. */
#ifndef PL1C_H
#define PL1C_H

#include <stdio.h>

#include "../rt/pl1abi.h"   /* what the compiler and the run time agree on */

/* ---- diagnostics ---------------------------------------------------- */

extern const char *src_name;
extern int n_errors;

void fatal(const char *fmt, ...);
void error_at(int line, const char *fmt, ...);
void *xmalloc(size_t n);
char *xstrdup(const char *s);

/* ---- tokens --------------------------------------------------------- */

enum tok {
    T_EOF, T_IDENT, T_NUM, T_STR, T_BITC,
    T_LP, T_RP, T_COMMA, T_SEMI, T_COLON, T_DOT,
    T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE, T_NLT, T_NGT,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_POWER,
    T_AND, T_OR, T_NOT, T_CAT, T_PCT, T_ARROW
};

struct token {
    enum tok kind;
    char    *text;     /* identifier (upper case) or number text */
    char    *str;      /* string constant value; bit constant digits */
    int      len;      /* string constant length */
    int      line;     /* source line of the first character */
    int      pos;      /* offset in the source stream */
};

struct lexer {
    char         *s;       /* the stream: columns 2..72 of every record */
    int          *lineof;  /* record number of each stream position */
    int           n;
    int           i;
    struct token *toks;
    int           ntok, cap;
};

void lex_file(struct lexer *lx, const char *path);
extern const char *include_dirs[16];
int pp_include(struct lexer *lx, int i, int line);
void preprocess(struct lexer *lx);
extern int n_include_dirs;
extern const char *dep_paths[256];  /* the %INCLUDE members read, for -MD */
extern int n_dep_paths;
extern int opt_stack;       /* -stack=N: the C stack, bytes (0: LIBC/370's default,
                               -1: the compiler's bound, #177) */
extern int opt_plist;       /* -plist: R1 to the main procedure unchanged (IHESAPB/D) */
extern int opt_char48;      /* -char48: PL/I (F)'s CHAR48, the 48-character set */
extern int opt_lang_f;      /* -lang=F: PL/I (F)'s language only */
extern int opt_interrupt;   /* -interrupt: the Optimizer's INTERRUPT, attention raises ATTENTION */
extern int opt_sprog;       /* LANGLVL(SPROG): pointer arithmetic (PL/I for MVS & VM) */
extern const unsigned char lat_to_ebc[256], ebc_to_lat[256];  /* CP037 (ebcdic.c) */

/* The language levels (docs/language-levels.md): L_F PL/I (F), the
 * oracle; L_OPT the Optimizing Compiler's (GC33-0009-4); L_MVS PL/I for
 * MVS & VM's (SC26-3114-01), not the Optimizer's; L_ISO ANSI X3.53-1976
 * only. */
enum level { L_F, L_OPT, L_MVS, L_ISO };
/* A feature beyond PL/I (F): an error with -lang=F, as PL/I (F) refuses
 * it; every such feature calls it where it is recognized */
void beyond_f(int line, enum level lv, const char *what);

#define MAXCALLARGS 16

/* ENVIRONMENT options without a value (sym env_opts) */
#define EO_CONSECUTIVE 1
#define EO_LEAVE       2
#define EO_REWIND      4
#define EO_CTLASA      8
#define EO_CTL360     16
#define EO_COBOL      32
#define EO_INDEXAREA  64
#define EO_NOWRITE   128
#define EO_GENKEY    256
#define EO_TRKOFL    512

/* ---- types ---------------------------------------------------------- */

/* TY_PIC: numeric character data, the characters of a FIXED DECIMAL(p,q)
 * value edited by t.pic; a character-string picture is TY_CHAR with
 * t.pic set. */
/* TY_LABEL: a label value, A(the label's stub) and A(its block's frame).
 * TY_VBIT: BIT(n) VARYING, a halfword of the length in bits and the
 * bytes.  A bit expression's p is its greatest length. */
/* TY_ENTRY: an entry value, A(the procedure) and A(its parent's frame);
 * TY_FILE: a file, A(its descriptor). */
/* TY_AREA: an area of p bytes, 16 bytes of control information before
 * them (rt/pl1area.c); TY_OFFSET: an offset in the area t.area, a word. */
enum tkind { TY_NONE, TY_FIXBIN, TY_FIXDEC, TY_CHAR, TY_VCHAR, TY_BIT, TY_FLTDEC, TY_FLTBIN, TY_PTR,
             TY_PIC, TY_LABEL, TY_VBIT, TY_ENTRY, TY_FILE, TY_AREA, TY_OFFSET,
             /* COMPLEX (#2): the real part, then the imaginary part, each
              * stored as the real kind; arith() is false for them, so code
              * that knows nothing of complex refuses them */
             TY_CFIXBIN, TY_CFIXDEC, TY_CFLTDEC, TY_CFLTBIN,
             TY_EVENT /* (#3): PL1_EV_... of rt/pl1abi.h */ };

#define MAXBITS 4096     /* the longest bit string (frame temporaries) */

struct expr;

/* ANSI's generic description (CM38, CM39): the attributes an argument
 * must have; '*' takes any argument (Tier 77) */
struct gdesc {
    unsigned char star, fixed, flt, bin, dec, chr, bit, var, nonvar, ptr, pic, label,
                  entry, file, area, offs, prec, scale, dims;
    short pmin, pmax, qmin, qmax;
};
struct gelem {             /* a member written name WHEN (...) */
    int          when;     /* 1: WHEN given; 0: an F-style member (ENTRY) */
    int          n;
    struct gdesc d[16];
};

struct type {
    enum tkind k;
    int p, q;          /* precision and scale (arithmetic), length (string) */
    const char *pic;   /* a PICTURE: its characters, repetitions expanded */
    struct expr *area; /* TY_OFFSET: the area named in OFFSET(a), 0 for NULLO */
    int optional;      /* a parameter descriptor's OPTIONAL (Tier 90) */
};

int pic_length(struct type t);  /* the characters of a TY_PIC */
static inline int is_cplx(struct type t) { return t.k >= TY_CFIXBIN && t.k <= TY_CFLTBIN; }
/* the type of either part of a complex value, and the complex type of
 * a real arithmetic one */
static inline struct type part_type(struct type t)
{
    t.k = t.k == TY_CFIXBIN ? TY_FIXBIN : t.k == TY_CFIXDEC ? TY_FIXDEC :
          t.k == TY_CFLTDEC ? TY_FLTDEC : t.k == TY_CFLTBIN ? TY_FLTBIN : t.k;
    return t;
}
static inline struct type cplx_type(struct type t)
{
    t.k = t.k == TY_FIXBIN ? TY_CFIXBIN : t.k == TY_FIXDEC ? TY_CFIXDEC :
          t.k == TY_FLTDEC ? TY_CFLTDEC : t.k == TY_FLTBIN ? TY_CFLTBIN : t.k;
    return t;
}
static inline int is_bit(struct type t) { return t.k == TY_BIT || t.k == TY_VBIT; }
/* a condition qualified by a file */
#define FILE_COND(c) ((c) == C_ENDFILE || (c) == C_ENDPAGE || (c) == C_UNDF || (c) == C_RECORD || \
                      (c) == C_KEY || (c) == C_NAME || (c) == C_TRANSMIT)

/* ---- symbols -------------------------------------------------------- */

/* The data attributes a DECLARE wrote, before any default: what the
 * DEFAULT statement completes (parse.c). */
struct rawattr {
    int fixed, flt, bin, dec, chr, bit, var, ptr, haspic;
    int prec, scale, clen, hasclen;
    struct type pict;
};

/* A DEFAULT statement's simple specification (GC33-0009-4): its range
 * -- identifier prefixes, letter ranges, or all (*) -- the attributes,
 * and the VALUE list (types with their precision or length). */
struct dflt {
    struct { char lo, hi; const char *id; } r[16];
    int          nr, all;
    struct rawattr a;
    struct type  vt[8];
    int          nv;
    struct dflt *next;
};

/* ST_CTL: CONTROLLED, generations stacked on a static anchor (gen.c). */
enum storage { ST_AUTO, ST_STATIC, ST_BASED, ST_CTL };

#define MAXDIMS 8
#define MAXINIT 256

struct init_item {
    int rep;               /* the iteration factor, a constant ... */
    struct expr *repx;     /* ... or an expression (AUTOMATIC, Q5INIF) */
    struct expr *e;        /* the value; NULL for '*': left as it is (Q1INIA) */
};

/* A declared name: a scalar, an array, a structure or a member of one.
 * Only a level-1 name (root == itself) owns storage; a member lives at
 * moff inside one element of its parent. */
struct sym {
    char        *name;
    struct type  t;        /* elementary type (TY_NONE for a structure) */
    enum storage st;
    int          off;      /* frame offset (AUTOMATIC root) */
    int          label;    /* static label number (STATIC root) */
    struct expr *based;    /* BASED(p): the pointer reference */
    struct expr *like;     /* LIKE x: the structure (expanded by resolve) */
    struct expr *defined;  /* DEFINED base: the base reference (lower.c) */
    /* x REFER(y): the leading upper bound (refer_dim 0) or the string
     * length (-1) of the last element of a based structure; x is set
     * into the member y by ALLOCATE, every reference takes it from y */
    struct expr *refer_from, *refer_to;
    struct sym  *refer;    /* y (resolve.c) */
    int          refer_dim, refer_unit; /* bytes per unit of the bound (lower.c) */
    int          position; /* POSITION(n), 0 if not given */
    int          overlay;  /* a string defined on a string: shares its bytes */
    struct stmt *initcall; /* INITIAL CALL p(args): the CALL, made at allocation */
    struct expr *init;     /* a single INITIAL value ... */
    struct init_item *inits; /* ... or the list of an array */
    int          ninits;
    int          implicit;
    int          synthetic; /* a loop variable made by the compiler */
    int          param;    /* a parameter: its position + 1 */
    int          ansi_parm, ansi_struct, ansi_member; /* ANSI's PARAMETER, STRUCTURE,
                                                       * MEMBER, checked (Tier 74) */
    int          is_cond;  /* DCL name CONDITION (ANSI) */
    /* a file's ENVIRONMENT options beyond REGIONAL and INDEXED (#215):
     * the record format (F, V, VS, VBS, U; "" none), block and record
     * size, BUFFERS, NCP, and the options without a value as EO_* bits */
    char         env_recfm[4];
    int          env_blk, env_rec, env_bufs, env_ncp, env_iarea, env_opts;
    int          asm_opt;  /* an ENTRY with OPTIONS(ASSEMBLER) (Tier 89) */
    int          byvalue;  /* ... OPTIONS(BYVALUE): the arguments' values (Tier 90) */
    int          retcode;  /* ... OPTIONS(RETCODE): R15 kept for PLIRETV */
    const char  *extname;  /* EXTERNAL('name'): the name outside */
    int          var_ef;   /* an ENTRY or FILE variable (ANSI's VARIABLE): data
                            * holding the entry's value or the descriptor's
                            * address (Tier 75) */
    int          is_entry; /* DCL name ENTRY / RETURNS: the caller's view */
    int          is_file;  /* a file constant: FL_* attributes */
    int          file_label; /* its descriptor @F<n> in the module */
    int          has_desc;
    struct type  desc[MAXCALLARGS];
    int          ndesc;
    unsigned char desc_dims[MAXCALLARGS]; /* a GENERIC member's: dimensions of each */
    struct sym **gen;      /* GENERIC: the members, entry declarations (Tier 67) */
    struct gelem *gel;     /* ... and ANSI's WHEN descriptions, one per member */
    int          ngen;
    struct proc *gen_callee; /* a member: the procedure it names (resolve.c) */
    int          gen_entryno;
    int          has_ret;
    struct type  ret;
    struct proc *owner;    /* the block that declares it */
    int          line;
    int          level;
    struct sym  *parent, *child, *sibling, *root;
    struct sym  *cpart[2]; /* a complex variable's parts as symbols of their own:
                              its storage, the part's type (check.c cx_sym) */
    int          ndims;    /* dimensions declared at this level */
    int          lb[MAXDIMS], hb[MAXDIMS];
    int          adjust;   /* AUTOMATIC bounds from expressions (Tier 81): a
                            * descriptor in the frame as an A(*) parameter's,
                            * the data on the stack past the frame */
    struct expr *lbx[MAXDIMS], *hbx[MAXDIMS];
    int          size;     /* bytes of one element */
    int          align;
    int          phase;    /* a structure: its offset from a doubleword */
    int          tail;     /* an array of structures: padding after the last */
    int          moff;     /* offset in one element of the parent */
    /* the same mapping in bits (lower.c), for unaligned bit strings,
     * which PL/I (F) packs (Q1BMAP): element size, alignment, phase,
     * offset in the parent's element, an array's element stride */
    int          bsize, balign, bphase, bmoff, bstride;
    int          mbit;     /* bit offset after moff (0..7) */
    int          packed;   /* a BIT leaf reached at a bit offset (gen.c) */
    int          tbits;    /* a level-1 item with packed bits: its bits,
                            * from the first byte of its storage */
    int          aligned;  /* ALIGNED */
    int          unal;     /* UNALIGNED written (for -lang=F) */
    struct sym  *next;
    struct rawattr raw;    /* the DECLARE's data attributes, for DEFAULT */
    int          area_dflt; /* AREA without a size: DEFAULT VALUE(AREA(n)) may give it */
    int          is_builtin; /* DCL name BUILTIN: the built-in function */
    int          external; /* EXTERNAL: one storage for the name, in every
                            * external procedure of the source (gen.c) */
    int          star;     /* CHAR(*) or BIT(*) parameter: t.p is a bound, the
                            * length comes with the argument (gen.c) */
    int          stardims; /* an array parameter A(*): the bounds come with the
                            * argument, in its descriptor (gen.c) */
    int          hasraw;
};

/* ---- expressions ---------------------------------------------------- */

/* E_CAST: l converted to t as an assignment converts (FIXED, FLOAT, ...). */
/* E_ISUB: iSUB in the base reference of a DEFINED item (len = i).
 * E_LABEL: a label constant (len = its id, callee = its block). */
/* E_ENTRY: a procedure passed as an argument (callee). */
enum ekind { E_NUM, E_STR, E_BITC, E_VAR, E_NEG, E_NOT, E_BIN, E_CALL, E_FCALL, E_CONV, E_CAST,
             E_ISUB, E_LABEL, E_ENTRY,
             E_STAR /* a subscript *: a cross section (lower.c) */ };

/* Built-in functions. */
/* The longest ONSOURCE an expression sees. */
#define ONSOURCE_MAX 256

enum bfn { B_LENGTH, B_SUBSTR, B_INDEX, B_MOD, B_ABS, B_MAX, B_MIN,
           B_DIM, B_LBOUND, B_HBOUND, B_ONCODE, B_ONCHAR, B_ONSOURCE,
           B_NULL, B_ADDR, B_STG, B_CSTG, B_VERIFY, B_TRANSLATE, B_REPEAT, B_HIGH, B_LOW, B_UNSPEC,
           B_SIGN, B_FLOOR, B_CEIL, B_TRUNC, B_ROUND, B_FIXED, B_FLOAT, B_BINARY,
           B_DECIMAL, B_PRECISION, B_ADD, B_MULTIPLY, B_DIVIDE,
           B_SQRT, B_EXP, B_LOG, B_LOG10, B_LOG2, B_SIN, B_COS, B_TAN, B_SIND, B_COSD,
           B_TAND, B_ATAN, B_ATAND, B_SINH, B_COSH, B_TANH, B_ATANH, B_ERF, B_ERFC,
           B_PICVAL /* the arithmetic value of a picture (lower.c) */, B_ALLOCATION,
           B_ONFILE, B_CHAR, B_BIT, B_NULLO, B_EMPTY, B_ONKEY, B_BOOL,
           B_SUM, B_PROD, B_POLY, B_ALL, B_ANY /* array functions: loops (lower.c) */,
           B_ONLOC, B_ONCOUNT, B_DATE, B_TIME, B_COUNT, B_LINENO, B_DATAFIELD,
           B_AFTER, B_BEFORE, B_DECAT, B_REVERSE, B_COPY, B_COLLATE /* ANSI */,
           B_ASIN, B_ACOS, B_SUBTRACT, B_DOT, B_EVERY, B_SOME, B_OFFSETF, B_POINTERF,
           B_PAGENO, B_VALID, B_STRING,
           /* PL/I for MVS & VM (Tier 86) */
           B_SYSNULL, B_PTRADD, B_PTRVALUE, B_BINVALUE, B_DATETIME,
           /* the Optimizer's (Tier 87) */
           B_PLIRETC, B_PLIRETV, B_ENTRYADDR,
           /* the Optimizer's dump (Tier 105) */
           B_PLIDUMP,
           /* the Optimizer's sort interface (Tier 107) */
           B_PLISRTA, B_PLISRTB, B_PLISRTC, B_PLISRTD,
           /* checkpoint and restart (Tier 108) */
           B_PLICKPT, B_PLIREST, B_PLICANC,
           /* COMPLEX (#2); B_CPXSTR, made by check.c, is a complex
            * value as a character string from its two parts */
           B_REAL, B_IMAG, B_CONJG, B_COMPLEX, B_CPXSTR,
           /* ... B_CXOP a complex * / ** done by the run time, B_CXABS
            * ABS of a complex value, B_CXFN a mathematical function of
            * a complex value, op the function (check.c cx_call, cx_fn,
            * gen.c gen_cxop) */
           B_CXOP, B_CXABS, B_CXFN,
           /* events (#3) */
           B_COMPLETION, B_STATUS };

struct expr {
    int          paren;    /* a reference written in parentheses, (X) */
    int          nullargs; /* F(): a function reference with no arguments */
    enum ekind   k;
    int          op;       /* enum tok of a binary operator */
    struct expr *l, *r;
    struct sym  *sym;
    enum bfn     fn;       /* E_CALL */
    struct expr *arg[MAXCALLARGS];
    int          nargs;
    struct expr *sub[MAXCALLARGS]; /* E_VAR: subscripts, outermost first */
    int          nsubs;
    char       **qn;       /* E_VAR before resolution: the qualified name */
    int          nq;
    struct proc *scope;    /* ... and the block it was written in */
    struct expr *qual;     /* E_VAR: the pointer of P -> X, if written */
    struct proc *callee;   /* E_FCALL: the procedure */
    struct sym  *entry;    /* E_FCALL: the ENTRY declaration seen, if any */
    struct sym  *generic;  /* E_FCALL of a GENERIC name: check.c chooses the member */
    unsigned char byref[MAXCALLARGS];  /* E_FCALL: argument passed by reference */
    struct type  argt[MAXCALLARGS];    /* ... else: the type of its dummy */
    char        *num;      /* digits of a constant, no point */
    int          e10;      /* a FLOAT constant: num * 10**e10 */
    char        *str;      /* string constant; bit constant digits */
    int          len;
    struct type  t;
    int          line;
    int          picstr;   /* E_VAR of a picture: its character-string value */
    int          bare;     /* E_FCALL: written without an argument list */
    int          entryno;  /* E_FCALL: the secondary entry called, 0 the primary */
    int          imag;     /* E_NUM: an imaginary constant, 2I (#2) */
    int          part;     /* E_VAR of a complex variable: 1 its real part, 2 its
                            * imaginary part, 0 the whole (check.c, gen_place) */
    struct sym  *cxt;      /* a complex * / ** (E_BIN): the temporary its run-time
                            * call leaves the value in, once made (check.c cx_call);
                            * B_CXOP: the same, e->l the part read from it */
};

/* ---- statements ----------------------------------------------------- */

enum skind { S_NULL, S_ASSIGN, S_IF, S_GROUP, S_WHILE, S_ITER, S_PUT,
             S_CALL, S_RETURN, S_GOTO, S_GET, S_OPEN, S_CLOSE, S_ON, S_ITEM,
             S_READ, S_WRITE, S_DELETE, S_SIGNAL, S_REVERT, S_BEGIN, S_STOP, S_ALLOC, S_FREE,
             S_REWRITE, S_ENTRY, S_SELECT, S_WHEN, S_LEAVE, S_LOCATE, S_FORMAT,
             S_CHECK, /* CHECK(cond_name): body the system action's PUT DATA (lower.c) */
             S_ADJUST, /* the bounds and storage of an adjustable array, sym in target */
             S_FETCH, S_RELEASE, /* the entries in more[] (Tier 91) */
             S_DISPLAY /* e the message, e2 the REPLY target or NULL */,
             S_WAIT,  /* the events in more[], e the count or NULL (#3) */
             S_DELAY  /* e the milliseconds */ };

/* Conditions; the numbers are the run time's too (rt/pl1cond.c). */
/* enum cond: rt/pl1abi.h */

/* Enabled conditions (condition prefixes); EN_SET marks a computed mask. */
#define EN_SIZE  1
#define EN_SUBRG 2
#define EN_STRG  4
#define EN_ZDIV  8
#define EN_FOFL  16
#define EN_CONV  32
#define EN_UFL   64
#define EN_OFL   128
#define EN_SET   256
#define EN_STRZ  512      /* STRINGSIZE, disabled by default as in GC33-0009-4 and
                           * SC26-3114-01 (ANSI 6.4.1.2 enables it) */
#define EN_DEFAULT (EN_ZDIV | EN_FOFL | EN_CONV | EN_UFL | EN_OFL | EN_SET)

/* File attributes (also the flag word of a file descriptor). */

/* Format items of an EDIT format list. */
/* E(w,d,s) carries d + 256 * s in d. */
enum fkind { F_END, F_A, F_AW, F_F, F_X, F_COL, F_SKIP, F_PAGE, F_LINE, F_E, F_P, F_B, F_BW,
             F_REP,                  /* (expression) item: w the count, d the items */
             F_TAB,                  /* ANSI's TAB(w): w tab positions on (Tier 72) */
             F_C,                    /* C(item[,item]): the two data items follow it */
             F_R };                  /* R(label): replaced by its FORMAT (parse.c) */
struct fmt { enum fkind k; int w, d; const char *pic; /* F_P */
             struct sym *ws, *ds;    /* a width or d given by an expression: the
                                      * variable the statement assigns it to */
             int sd; };              /* E(w,d,s) with d an expression: s, or -1 */
#define MAXFMT 256

/* One file of an OPEN or CLOSE statement. */
struct fopen { struct expr *file; int attrs; char *title;
               struct expr *linesize, *pagesize;   /* LINESIZE(e), PAGESIZE(e) */
               struct expr *tabs[16]; int ntabs; }; /* ANSI's TAB(e, ...) */
#define MAXFILES 8

#define MAXLABELS 4

struct put_item { struct expr *e; struct expr *in; struct put_item *next;  /* in: IN(area) */
                  struct stmt *loop;         /* (items DO v = ...): an S_ITER, its items sub */
                  struct put_item *sub; };

/* One specification of an iterative DO: e1 [TO e2] [BY e3] [WHILE(w)].
 * check.c makes the rest: v2, v3 hold TO and BY, evaluated once; gt and
 * lt compare the control variable with v2, neg tests v3 < 0, incr is
 * the control variable plus v3 (or 1). */
struct dospec {
    struct expr   *e1, *e2, *e3, *w;
    struct expr   *u, *rep;      /* UNTIL(u), REPEAT rep (Optimizer) */
    struct sym    *v2, *v3;
    struct expr   *gt, *lt, *neg, *incr;
    struct dospec *next;
};

struct stmt {
    int          snap;     /* ON ... SNAP */
    int          byname;   /* an assignment ..., BY NAME */
    struct expr *more[16]; /* A, B, C = e: the targets after the first */
    int          nmore;
    enum skind   k;
    int          no;       /* statement number, as PL/I (F) counts */
    int          line;
    char        *text;     /* source text, for the listing comment */
    struct sym  *target;   /* S_ITER: the control variable */
    struct expr *lhs;      /* S_ASSIGN: the target reference */
    struct expr *e, *e2, *e3;
    struct expr **rets;    /* S_RETURN, entries of other attributes: the
                              value for entry k, or NULL (check.c) */
    struct stmt *body, *els;
    struct stmt *next;
    int          skip;     /* PUT: -1 none, else lines */
    int          page;
    struct put_item *items;
    char        *labels[MAXLABELS];
    int          nlabels;
    int          label_id[MAXLABELS];
    char        *goto_name;  /* S_GOTO */
    int          goto_id;
    struct expr *iter_ref;   /* S_ITER: the control variable as written */
    struct dospec *specs;    /* S_ITER as written; general: the full form (check.c) */
    int          general;
    struct expr *file;       /* S_PUT, S_GET, S_ON...: the file (NULL: default) */
    int          edit;       /* S_PUT, S_GET: EDIT, else LIST */
    int          data;       /* ... DATA: 1 with a list, 2 without */
    struct expr *string;     /* ... the STRING option's string */
    struct fmt  *fmts;       /* the format list, F_END-terminated */
    int          nfmts;
    struct fopen files[MAXFILES]; /* S_OPEN, S_CLOSE */
    int          nfiles;
    enum cond    cond;       /* S_ON, S_SIGNAL, S_REVERT */
    char        *cond_name;  /* ... C_CONDITION: the name */
    struct proc *unit;       /* S_ON: the on-unit, a block of its own (NULL: SYSTEM) */
    int          on_slot;    /* S_ON, S_REVERT: frame offset of the establishment */
    struct proc *goto_proc;  /* S_GOTO: the block of the label */
    int          pon, poff;  /* its condition prefix: EN_ bits enabled, disabled */
    int          mask;       /* the conditions enabled in it (EN_SET if known) */
    struct stmt *owner_io;   /* S_ITEM: the PUT or GET it belongs to */
    int          get;        /* S_ITEM: a GET target (else a PUT value) */
    int          check_code; /* S_CHECK: ONCODE 510 a label or entry, 511 a variable */
    struct expr *ignore;     /* S_READ: IGNORE(n), records skipped */
    int          copy;       /* S_GET: COPY, the fields to SYSPRINT */
    struct expr *copyfile;   /* ... or to this file: ANSI's COPY(file) */
    int          cxpair;     /* S_ITEM of PUT EDIT: 1 a complex value's real part,
                                2 its imaginary part (check.c, rt PL1ECPX) */
};

#define MAXPARAMS MAXCALLARGS

/* A secondary entry point (ENTRY statement): its parameters' positions
 * in the procedure's canonical parameter list. */
struct pentry {
    char        *name;
    int          nparams;
    int          canon[MAXCALLARGS];
    int          has_ret;
    struct type  ret;
    int          label;      /* gen.c: where the ENTRY statement stands */
};

struct proc {
    char        *name;
    struct sym  *syms;
    struct dflt *defaults; /* its DEFAULT statements */
    struct stmt *body;
    struct proc *parent, *child, *sibling;
    char        *pnames[MAXPARAMS];
    struct sym  *params[MAXPARAMS];
    int          nparams;    /* all parameters, the entries' too */
    int          nprim;      /* those of the PROCEDURE statement */
    struct pentry *entries;  /* ENTRY statements, entry k at [k-1] */
    int          nentries;
    int          entoff;     /* gen.c: entry number, canonical list */
    int          has_ret;
    struct type  ret;
    int          ret_main;   /* check.c: the PROCEDURE statement has RETURNS */
    int          ret_all;    /* every entry point returns a value */
    int          ret_multi;  /* entry points return other attributes */
    int          recursive;
    int          id;         /* code label @P<id>; 0 is the main procedure */
    int          depth;      /* 0 for an external procedure */
    int          is_main;    /* OPTIONS(MAIN) */
    int          noexecops;  /* OPTIONS(MAIN NOEXECOPS) */
    int          asm_opt, retcode;  /* its ENTRY's OPTIONS(ASSEMBLER), (RETCODE) */
    int          fetched;    /* named in a FETCH or RELEASE: loaded at run time */
    int          byvalue;    /* OPTIONS(BYVALUE): a MAIN procedure's, or its ENTRY's */
    const char  *extname;   /* its ENTRY's EXTERNAL('name') */
    int          as_value;   /* used as a value (an ENTRY variable or argument):
                                its string parameters by descriptor, as an
                                external procedure's (gen.c ext_string_par) */
    int          is_stub;    /* an external procedure compiled apart: only its
                              * ENTRY declaration is known, called through V() */
    int          nodesc;     /* ... declared without parameter descriptors */
    struct proc *next_ext;   /* the next external procedure of the source */
    int          is_unit;    /* an on-unit */
    int          is_block;   /* a BEGIN block */
    int          pon, poff;  /* its condition prefix */
    char       **checks;     /* ... its CHECK(name, ...) */
    int          nchecks;
    int          mask;       /* the conditions enabled in it */
    void        *labels;     /* resolve.c: the labels of the block */
    int         *vlabels;    /* ... those used as values: gen.c makes them stubs */
    int          nvlabels;
    int          line;
};

struct proc *parse_program(struct lexer *lx);
void resolve_program(struct proc *p);
void lower_program(struct proc *p);
int  cum_dims(struct sym *s);
struct sym *new_loop_var(struct proc *p);
/* parse.c: the attributes DEFAULT and the standard rules give s (an
 * implicit declaration: scope is the main procedure) */
void default_sym(struct sym *s, struct proc *scope);
void check_program(struct proc *p);
struct type elem_type(struct expr *e);
extern int uses_count;
extern int uses_strz;     /* ON STRINGSIZE or a STRINGSIZE prefix: bits cut in
                           * registers are checked */
struct type reduce_type(struct type t, struct type x);
void gen_program(struct proc *p, FILE *out);

int dec_bytes(int p);

/* PL/I (F) conversions between the bases (GC28-8201-4):
 * FIXED DECIMAL(p,q) -> FIXED BINARY(1+CEIL(p*3.32), CEIL(q*3.32)),
 * FIXED BINARY(p,q)  -> FIXED DECIMAL(1+CEIL(p/3.32), CEIL(q/3.32)). */
int bin_prec(struct type t);
int bin_scale(struct type t);
int dec_prec(struct type t);
int dec_scale(struct type t);
int arith(struct type t);
int is_float(struct type t);
int flt_long(struct type t);
int flt_digits(struct type t);
struct type float_common(struct type a, struct type b);
int is_string(struct type t);
int cplx_init(struct expr *v, struct expr **re, struct expr **im);

#endif
