/* gen.c -- S/370 assembler for PL1C/370.
 *
 * The shape is the one tests/mvs/seam/p0hello.asm proved on MVS: the main
 * procedure is the C function MAIN behind @@MAIN -> @@CRT0, its frame comes
 * from the C stack (PDPPRLG/PDPEPIL), and the run time is called with the
 * linkage of CC/370 -- R1 addresses a list of argument values built at
 * offset 88 of the frame.
 *
 * Frame:  0-87 header, 88-111 argument list for the run time (six words),
 * 112 the static link, 116 the argument list of this invocation, then the
 * AUTOMATIC variables, then temporaries (doubleword aligned).
 *
 * Internal procedures: R1 addresses a list of words -- 0 the frame of the
 * procedure's parent block (static link), 4 the address of the result area
 * (functions), 8 onwards the address of each argument.
 *
 * Registers: R12 code base, R13 frame, R2-R9 expression values, R0/R1
 * scratch (MR uses the pair), R14/R15 linkage.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "pl1c.h"
#include "../rt/pl1flt.h"

/* the frame's words: rt/pl1abi.h */
#define ARGS     PL1_FR_ARGS
#define MAXARGS  6
#define SLINK    PL1_FR_SLINK   /* the frame of the enclosing block (static link) */
#define ALIST    PL1_FR_ALIST   /* this invocation's argument list (R1 at entry) */
/* R11 and R10 from R12: the second and third 4 KB of a procedure */
#define BASES "         LA    11,4095(,12)\n         LA    11,1(,11)\n" \
              "         LA    10,4095(,11)\n         LA    10,1(,10)\n"
#define CURF     PL1_FR_CURF    /* the run time's current frame at entry (PL1CURF) */
#define PINFO    PL1_FR_PINFO   /* the procedure's descriptor, for messages */
#define STUB14   PL1_FR_STUB14  /* R14 while a raise stub runs */
#define NOCV     PL1_FR_NOCV    /* the caller's NOCONVERSION flag, and (133) this block's own */
#define VARS     PL1_FR_VARS

/* ---- output buffer: the prologue needs the frame size, known last --- */

static char  *buf;
static size_t blen, bcap;

static void out(const char *fmt, ...)
{
    va_list ap;
    char line[512];
    int n;

    va_start(ap, fmt);
    n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n < 0 || n >= (int)sizeof line)
        fatal("internal: output line too long");
    if (blen + (size_t)n + 2 > bcap) {
        bcap = bcap ? bcap * 2 : 65536;
        buf = realloc(buf, bcap);
        if (!buf)
            fatal("out of memory");
    }
    memcpy(buf + blen, line, (size_t)n);
    blen += (size_t)n;
    buf[blen++] = '\n';
}

/* An instruction: opcode and operands in the usual columns. */
/* The size of the procedure being generated, estimated: its instructions
 * and literals must fit the 12 KB its three base registers address. */
static int code_bytes, lit_bytes;
#define MAXLITS 1024
static char *lits[MAXLITS];
static int nlits;

/* a literal operand counts once, as LTORG pools it */
static void count_literal(const char *eq)
{
    char text[64];
    int n = 0, q = 0;
    while (eq[n] && n < 63 && (q || (eq[n] != ',' && eq[n] != ' '))) {
        if (eq[n] == '\'')
            q = !q;
        text[n] = eq[n];
        n++;
    }
    text[n] = '\0';
    for (int i = 0; i < nlits; i++)
        if (strcmp(lits[i], text) == 0)
            return;
    if (nlits < MAXLITS)
        lits[nlits++] = xstrdup(text);
    lit_bytes += 8;
}

static int op_length(const char *op)
{
    static const char *ss[] = { "MVC", "CLC", "ZAP", "AP", "SP", "MP", "DP", "CP", "SRP", "MVN",
                                "MVZ", "NC", "OC", "XC", "TR", "TRT", "ED", "EDMK", "PACK",
                                "UNPK", "MVO", NULL };
    size_t n = strlen(op);
    for (int i = 0; ss[i]; i++)
        if (strcmp(op, ss[i]) == 0)
            return 6;
    if (n >= 2 && op[n - 1] == 'R' && strcmp(op, "BXLE") != 0)
        return 2;                         /* LR, BALR, LTDR, ... */
    if (strcmp(op, "SPM") == 0)
        return 2;
    return 4;
}

static void ins(const char *label, const char *op, const char *fmt, ...)
{
    char opnd[400];
    va_list ap;
    const char *eq;

    va_start(ap, fmt);
    vsnprintf(opnd, sizeof opnd, fmt, ap);
    va_end(ap);
    code_bytes += op_length(op);
    if ((eq = strchr(opnd, '=')) != NULL)
        count_literal(eq);
    out("%-8s %-5s %s", label ? label : "", op, opnd);
}

static int nlabel;
static int new_label(void) { return ++nlabel; }
static int gen_fcall(struct expr *e);
static int type_size(struct type t);
static int total_size(struct sym *s);
static void here(int l) { out("@L%-6d DS    0H", l); }

/* ---- frame ---------------------------------------------------------- */

static int vars_end;          /* first byte after the variables */
static int tmp_base, tmp_top, tmp_hwm;
static int high_end;           /* the end of the variables from 4096 on, or 0 */

static int align(int v, int a) { return (v + a - 1) / a * a; }

static int tmp(int size, int al)
{
    int off = align(tmp_top, al);
    tmp_top = off + size;
    if (tmp_top > tmp_hwm)
        tmp_hwm = tmp_top;
    if (tmp_top > 4095)
        fatal("%s: frame larger than 4095 bytes -- not supported yet", src_name);
    return off;
}

/* ---- registers ------------------------------------------------------ */

static int reg_busy[16];

static int reg_get(void)
{
    for (int r = 2; r <= 9; r++)
        if (!reg_busy[r]) {
            reg_busy[r] = 1;
            return r;
        }
    fatal("%s: expression too complex (out of registers)", src_name);
    return 0;
}

static void reg_free(int r) { reg_busy[r] = 0; }

/* ---- storage operands ----------------------------------------------- */

static const char *tss(int off, int len)
{
    static char b[4][32];
    static int k;
    char *r = b[k++ & 3];
    snprintf(r, 32, "%d(%d,13)", off, len);
    return r;
}

/* ---- places: where an element is ---------------------------------- */

static struct proc *CURP;      /* the procedure being generated */
static int need_size, need_subrg;        /* raise stubs for SIZE, SUBSCRIPTRANGE */
static int cur_mask = EN_DEFAULT;        /* the conditions enabled where we are */

/* An element: a base register (13 for this frame, or one loaded with the
 * address of an enclosing frame, a STATIC item or an argument) plus off,
 * plus an index register when subscripts are not constant.  Once
 * "based", ireg holds the address itself. */
struct place { int base; int off; int ireg; int based;
               int bitoff, bitreg; };      /* packed bits: the bit after off, or in bitreg */

static int gen_int(struct expr *e);
static int const_value(struct expr *e, int *v);

static int dim_extent(struct sym *s, int i) { return s->hb[i] - s->lb[i] + 1; }

/* A register with the frame of block b, an ancestor of this procedure
 * (13 if it is this procedure). */
static int frame_reg(struct proc *b)
{
    int r;
    struct proc *x = CURP;
    if (b == CURP)
        return 13;
    r = reg_get();
    ins(NULL, "L", "%d,%d(,13)", r, SLINK);
    for (x = CURP->parent; x && x != b; x = x->parent)
        ins(NULL, "L", "%d,%d(,%d)", r, SLINK, r);
    if (!x)
        fatal("internal: block %s is not enclosing", b->name);
    return r;
}

static int gen_ptr(struct expr *e);
static int star_len(struct sym *s);
static int refer_len(struct sym *s, int ra);
static struct sym *refer_of(struct sym *x);
static int root_off(struct sym *s);
static int star_desc(struct sym *s);
static int callee_par_varying(struct expr *e, int i);
static struct sym *callee_par(struct expr *e, int i);
static void star_subscripts(struct expr *e, struct sym *x, int k, struct place *p);
static void area_init(int base, int off, int n);
static int area_addr(struct expr *a);

/* An external name as the linkage editor sees it: one of more than
 * eight characters is cut to its first four and last three, as PL/I (F)
 * cuts it (IEM2867I: COMPUTEVALUE is COMPLUE, Q2LONG) */
static const char *ext_name(const char *n)
{
    static char b[4][12];
    static int k;
    char *r;
    size_t len = strlen(n);
    if (len <= 8)
        return n;
    r = b[k++ & 3];
    snprintf(r, 12, "%.4s%s", n, n + len - 3);
    return r;
}

/* The name of an EXTERNAL variable's section, and of a file's
 * descriptor ('#' and seven characters of the file's name): the same in
 * every external procedure, compiled together or apart */
static const char *ext_sect(struct sym *s, int file)
{
    static char b[4][12];
    static int k;
    char *r = b[k++ & 3];
    if (file)
        snprintf(r, 12, "#%.7s", s->name);
    else
        snprintf(r, 12, "%s", ext_name(s->name));
    return r;
}

/* A string parameter of an external procedure, CHAR(n), CHAR(n)
 * VARYING or BIT(n), not an array: it is passed by a descriptor,
 * A(the data) and its length, as a CHAR(*) one is, so that a caller
 * compiled apart, which knows only its ENTRY declaration, reaches it
 * whatever length that declares (Q1SUC compiled apart, JOB00877) */
static int ext_string_par(struct sym *s)
{
    return s && s->param && s->root == s && !s->ndims && !s->star && !s->stardims &&
           !s->packed && s->owner && (s->owner->depth == 0 || s->owner->as_value) &&
           !s->owner->is_main && (s->t.k == TY_CHAR || s->t.k == TY_VCHAR || s->t.k == TY_BIT);
}

/* A string argument of a call to an external procedure whose parameter
 * is not known (a contextual CALL, an ENTRY without descriptors, an
 * empty descriptor), or of a call through an ENTRY variable or
 * parameter: its descriptor too, with its own length (t162ctx, #284) */
static int ext_string_arg(struct expr *e, int i)
{
    struct proc *p = e->callee;
    struct sym *par = callee_par(e, i);
    struct expr *a = e->arg[i];
    struct type t = e->argt[i];
    if (a->k == E_VAR && a->sym->ndims && a->nsubs == 0)
        return 0;                       /* a whole array: as it is */
    if (!p)
        /* through an ENTRY variable or parameter: the procedure may be
         * external or used as a value, both take strings by descriptor
         * (#284) */
        return t.k == TY_CHAR || t.k == TY_VCHAR || t.k == TY_BIT;
    if ((p->depth != 0 && !p->as_value) || p->is_main || (par && par->t.k != TY_NONE))
        return 0;
    return t.k == TY_CHAR || t.k == TY_VCHAR || t.k == TY_BIT;
}

static struct place gen_place(struct expr *e)
{
    struct sym *chain[32];
    int n = 0, k = 0;
    struct place p;
    struct sym *root = e->sym->root;

    for (struct sym *x = e->sym; x; x = x->parent)
        chain[n++] = x;
    p.off = 0;
    p.ireg = 0;
    p.based = 0;
    p.bitoff = 0;
    p.bitreg = 0;
    if (root->st == ST_BASED) {
        /* the storage the pointer addresses */
        p.base = gen_ptr(e->qual ? e->qual : root->based);
    } else if (root->st == ST_CTL) {
        /* the latest generation: 8 bytes after its block's start, a
         * structure at its phase from there */
        p.base = reg_get();
        ins(NULL, "L", "%d,=A(@V%d)", p.base, root->label);
        ins(NULL, "L", "%d,0(,%d)", p.base, p.base);
        p.off = 8 + (root->child ? root->phase : 0);
    } else if (root->st == ST_STATIC && root->external) {
        /* its section, the data at the structure's phase in it */
        p.base = reg_get();
        ins(NULL, "L", "%d,=A(%s)", p.base, ext_sect(root, 0));
        p.off = root->child ? root->phase : 0;
    } else if (root->st == ST_STATIC) {
        p.base = reg_get();
        ins(NULL, "L", "%d,=A(@V%d)", p.base, root->label);
    } else if (root->param) {
        int fb = frame_reg(root->owner);
        p.base = fb == 13 ? reg_get() : fb;
        ins(NULL, "L", "%d,%d(,%d)", p.base, ALIST, fb);
        ins(NULL, "L", "%d,%d(,%d)", p.base, 8 + 4 * (root->param - 1), p.base);
        if ((root->star && root->t.k != TY_AREA) || root->stardims || ext_string_par(root))
            /* CHAR(*), BIT(*), A(*), a string parameter of an external
             * procedure: the argument list has its descriptor, A(data),
             * F(length), ... (gen_fcall) */
            ins(NULL, "L", "%d,0(,%d)", p.base, p.base);
    } else if (root->adjust) {
        /* an adjustable array: A(data) from its descriptor (S_ADJUST) */
        int fb = frame_reg(root->owner);
        p.base = fb == 13 ? reg_get() : fb;
        ins(NULL, "L", "%d,%d(,%d)", p.base, root->off, fb);
    } else {
        p.base = frame_reg(root->owner);
        p.off = root->off;
    }
    if (e->sym->packed) {
        /* an unaligned bit string at a bit offset (lower.c): the offset
         * in bits, then its bytes into off and ireg, the rest the bit */
        int cbits = 0, rb = 0;
        for (int i = n - 1; i >= 0; i--) {
            struct sym *x = chain[i];
            int stride = x->bstride, strides[MAXDIMS];
            if (x != root)
                cbits += 8 * x->moff + x->mbit;
            for (int d = x->ndims - 1; d >= 0; d--) {
                strides[d] = stride;
                stride *= dim_extent(x, d);
            }
            if (e->nsubs == 0)
                continue;
            for (int d = 0; d < x->ndims; d++, k++) {
                struct expr *sub = e->sub[k];
                int v, r;
                if (const_value(sub, &v)) {
                    if ((cur_mask & EN_SUBRG) && (v < x->lb[d] || v > x->hb[d])) {
                        need_subrg = 1;
                        ins(NULL, "BAL", "14,@B%d", CURP->id);
                    }
                    cbits += (v - x->lb[d]) * strides[d];
                    continue;
                }
                r = gen_int(sub);
                if (cur_mask & EN_SUBRG) {
                    int ok = new_label(), bad = new_label();
                    need_subrg = 1;
                    ins(NULL, "C", "%d,=F'%d'", r, x->lb[d]);
                    ins(NULL, "BC", "4,@L%d", bad);
                    ins(NULL, "C", "%d,=F'%d'", r, x->hb[d]);
                    ins(NULL, "BC", "12,@L%d", ok);
                    here(bad);
                    ins(NULL, "BAL", "14,@B%d", CURP->id);
                    here(ok);
                }
                if (x->lb[d] != 0)
                    ins(NULL, "S", "%d,=F'%d'", r, x->lb[d]);
                if (strides[d] != 1) {
                    ins(NULL, "LR", "1,%d", r);
                    ins(NULL, "M", "0,=F'%d'", strides[d]);
                    ins(NULL, "LR", "%d,1", r);
                }
                if (rb) {
                    ins(NULL, "AR", "%d,%d", rb, r);
                    reg_free(r);
                } else {
                    rb = r;
                }
            }
        }
        p.off += cbits / 8;
        if (rb) {
            int by = reg_get();
            if (cbits % 8)
                ins(NULL, "LA", "%d,%d(,%d)", rb, cbits % 8, rb);
            ins(NULL, "LR", "%d,%d", by, rb);
            ins(NULL, "SRL", "%d,3", by);
            ins(NULL, "N", "%d,=F'7'", rb);
            p.ireg = by;
            p.bitreg = rb;
        } else {
            p.bitoff = cbits % 8;
        }
        if (p.off > 4095 || p.off < 0) {
            int r = p.base == 13 ? reg_get() : p.base;
            ins(NULL, "LR", "%d,%d", r, p.base);
            ins(NULL, "A", "%d,=F'%d'", r, p.off);
            p.base = r;
            p.off = 0;
        }
        return p;
    }
    for (int i = n - 1; i >= 0; i--) {
        struct sym *x = chain[i];
        int stride = x->size;
        int strides[MAXDIMS];
        p.off += x->moff;
        if (x == root && (root->param || root->adjust) &&
            (root->stardims || (root->star && root->ndims))) {
            if (e->nsubs) {
                star_subscripts(e, x, k, &p);
                k += x->ndims;
            }
            continue;
        }
        for (int d = x->ndims - 1; d >= 0; d--) {
            strides[d] = stride;
            stride *= dim_extent(x, d);
        }
        if (e->nsubs == 0)
            continue;                     /* a whole array: its first byte */
        for (int d = 0; d < x->ndims; d++, k++) {
            struct expr *sub = e->sub[k];
            int v;
            if (const_value(sub, &v)) {
                if ((cur_mask & EN_SUBRG) && (v < x->lb[d] || v > x->hb[d])) {
                    need_subrg = 1;
                    ins(NULL, "BAL", "14,@B%d", CURP->id);
                }
                p.off += (v - x->lb[d]) * strides[d];
                continue;
            }
            {
                int r = gen_int(sub);
                if (cur_mask & EN_SUBRG) {
                    /* SUBSCRIPTRANGE: each subscript in its bounds (JOB00210) */
                    int ok = new_label(), bad = new_label();
                    need_subrg = 1;
                    ins(NULL, "C", "%d,=F'%d'", r, x->lb[d]);
                    ins(NULL, "BC", "4,@L%d", bad);
                    ins(NULL, "C", "%d,=F'%d'", r, x->hb[d]);
                    ins(NULL, "BC", "12,@L%d", ok);
                    here(bad);
                    ins(NULL, "BAL", "14,@B%d", CURP->id);
                    here(ok);
                }
                if (x->lb[d] != 0)
                    ins(NULL, "S", "%d,=F'%d'", r, x->lb[d]);
                if (strides[d] != 1) {
                    if (strides[d] <= 32767) {
                        ins(NULL, "MH", "%d,=H'%d'", r, strides[d]);
                    } else {
                        ins(NULL, "LR", "1,%d", r);
                        ins(NULL, "M", "0,=F'%d'", strides[d]);
                        ins(NULL, "LR", "%d,1", r);
                    }
                }
                if (p.ireg) {
                    ins(NULL, "AR", "%d,%d", p.ireg, r);
                    reg_free(r);
                } else {
                    p.ireg = r;
                }
            }
        }
    }
    if (e->part == 2)
        p.off += type_size(e->sym->t);  /* the imaginary part (#2, check.c cx_sym) */
    if (p.off > 4095 || p.off < 0) {
        /* displacement out of range: fold it into the base */
        int r = p.base == 13 ? reg_get() : p.base;
        ins(NULL, "LR", "%d,%d", r, p.base);
        ins(NULL, "A", "%d,=F'%d'", r, p.off);
        p.base = r;
        p.off = 0;
    }
    return p;
}

/* RX operand. */
static const char *p_rx(struct place *p, int plus)
{
    static char b[4][48];
    static int k;
    char *r = b[k++ & 3];
    if (p->based)
        snprintf(r, 48, "%d(,%d)", plus, p->ireg);
    else if (p->ireg)
        snprintf(r, 48, "%d(%d,%d)", p->off + plus, p->ireg, p->base);
    else
        snprintf(r, 48, "%d(,%d)", p->off + plus, p->base);
    return r;
}

/* Turn an indexed place into a based one: its address in the register. */
static void p_base(struct place *p)
{
    if (p->ireg && !p->based) {
        ins(NULL, "LA", "%d,%s", p->ireg, p_rx(p, 0));
        p->based = 1;
        p->off = 0;
        if (p->base != 13) {
            reg_free(p->base);
            p->base = 13;
        }
    }
}

/* RS/SI operand (no index). */
static const char *p_rs(struct place *p, int plus)
{
    static char b[4][48];
    static int k;
    char *r = b[k++ & 3];
    p_base(p);
    if (p->based)
        snprintf(r, 48, "%d(%d)", plus, p->ireg);
    else
        snprintf(r, 48, "%d(%d)", p->off + plus, p->base);
    return r;
}

/* SS operand with a length. */
static const char *p_ss(struct place *p, int plus, int len)
{
    static char b[4][48];
    static int k;
    char *r = b[k++ & 3];
    p_base(p);
    if (p->based)
        snprintf(r, 48, "%d(%d,%d)", plus, len, p->ireg);
    else
        snprintf(r, 48, "%d(%d,%d)", p->off + plus, len, p->base);
    return r;
}

/* A register the caller may overwrite once it has used the operand: one
 * of the place's own, else a new one. */
static int p_take(struct place *p)
{
    if (p->ireg)
        return p->ireg;
    if (p->base != 13)
        return p->base;
    return reg_get();
}

/* Release the place's registers, except keep. */
static void p_free_except(struct place *p, int keep)
{
    if (p->ireg && p->ireg != keep)
        reg_free(p->ireg);
    if (p->base != 13 && p->base != keep)
        reg_free(p->base);
}

static void p_free(struct place *p)
{
    p_free_except(p, 0);
}

/* ---- string constants ----------------------------------------------- */

struct strlit { int label; char *s; int len; struct strlit *next; };
static struct strlit *strs;

static int str_label(const char *s, int len)
{
    for (struct strlit *p = strs; p; p = p->next)
        if (p->len == len && memcmp(p->s, s, (size_t)len) == 0)
            return p->label;
    {
        struct strlit *p = xmalloc(sizeof *p);
        p->label = new_label();
        p->s = xmalloc((size_t)len + 1);
        memcpy(p->s, s, (size_t)len);
        p->len = len;
        p->next = strs;
        strs = p;
        return p->label;
    }
}

/* DC C'...' operand text: quotes and ampersands doubled. */
static void dc_chars(const char *label, const char *s, int len)
{
    int i = 0;
    if (len == 0) {
        out("%-8s DS    0C", label ? label : "");
        return;
    }
    while (i < len) {
        char text[160];
        int n = 0, take = 0;
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c > 0x7E) {
            /* no printable ASCII (an X constant's byte, the NOT sign):
             * its CP037 code, as as370 would make it of the character */
            while (i + take < len && n < 100 &&
                   ((unsigned char)s[i + take] < 0x20 || (unsigned char)s[i + take] > 0x7E))
                n += sprintf(text + n, "%02X", lat_to_ebc[(unsigned char)s[i + take++]]);
            out("%-8s DC    X'%s'", i == 0 && label ? label : "", text);
            i += take;
            continue;
        }
        while (i + take < len && n < 50 && (unsigned char)s[i + take] >= 0x20 &&
               (unsigned char)s[i + take] <= 0x7E) {
            char ch = s[i + take++];
            if (ch == '\'' || ch == '&')
                text[n++] = ch;
            text[n++] = ch;
        }
        text[n] = '\0';
        out("%-8s DC    C'%s'", i == 0 && label ? label : "", text);
        i += take;
    }
}

/* Packed decimal literal for digits d (no point), p digits, negative. */
static const char *pl_lit(const char *d, int p, int neg)
{
    static char b[4][48];
    static int k;
    char *r = b[k++ & 3];
    snprintf(r, 48, "=PL%d'%s%s'", dec_bytes(p), neg ? "-" : "", d);
    return r;
}

/* The C stack the program needs (#177): every procedure's frame added
 * up, an upper bound for a call chain that enters no procedure twice;
 * stk_unknown when that does not hold or cannot be seen -- RECURSIVE,
 * an adjustable array on the stack, a call to a routine compiled apart
 * or through an ENTRY variable. */
static int stk_frames, stk_unknown;
#define STK_RESERVE 32768     /* the run time's and LIBC/370's frames below the
                                 PL/I ones: at most 7,708 bytes measured over
                                 the suite (startup-measurement.md) */
#define STK_DEFAULT 262144    /* LIBC/370's @@CRT0 */

/* ---- calls into the run time ---------------------------------------- */

static int nargs;

static void arg_reg(int r)
{
    if (nargs == MAXARGS)
        fatal("internal: too many arguments");
    ins(NULL, "ST", "%d,%d(,13)", r, ARGS + 4 * nargs++);
}

static void arg_const(int v)
{
    if (nargs == MAXARGS)
        fatal("internal: too many arguments");
    ins(NULL, "MVC", "%d(4,13),=F'%d'", ARGS + 4 * nargs++, v);
}

static void call(const char *name)
{
    ins(NULL, "LA", "1,%d(,13)", ARGS);
    ins(NULL, "L", "15,=V(%s)", name);
    ins(NULL, "BALR", "14,15");
    nargs = 0;
}

/* -interrupt: an attention interruption pending raises ATTENTION here --
 * at the end of each pass of a loop and at a GO TO (SC33-0006-7, the
 * INTERRUPT option: "at branching points"); the flag is the attention
 * exit's (rt/pl1attn.asm) */
static void poll(void)
{
    int l;
    if (!opt_interrupt)
        return;
    l = new_label();
    ins(NULL, "L", "15,=V(PL1ATTN)");
    ins(NULL, "CLI", "0(15),0");
    ins(NULL, "BE", "@L%d", l);
    call("PL1APOL");
    here(l);
}

static int uses_complex;          /* the source has COMPLEX data (gen_program) */
static int need_zd, need_fofl, need_ufl;  /* the raise stubs the procedure needs */

/* ZERODIVIDE or FIXEDOVERFLOW at this point: through a stub of the
 * procedure, which keeps every register. */
static void raise_zd(void)
{
    if (!(cur_mask & EN_ZDIV))
        return;                           /* NOZERODIVIDE: the result undefined */
    need_zd = 1;
    ins(NULL, "BAL", "14,@Z%d", CURP->id);
}

/* UNDERFLOW: a product or quotient of floats in temporary t that is 0
 * although its operands a (and b, unless b < 0) are not -- with the
 * program mask clear the machine makes an exponent underflow 0. */
static void check_ufl(int t, int a, int b, int lo)
{
    int ok;
    if (!(cur_mask & EN_UFL))
        return;
    ok = new_label();
    need_ufl = 1;
    ins(NULL, lo ? "LD" : "LE", "2,%d(,13)", t);
    ins(NULL, lo ? "LTDR" : "LTER", "2,2");
    ins(NULL, "BC", "7,@L%d", ok);
    ins(NULL, lo ? "LD" : "LE", "2,%d(,13)", a);
    ins(NULL, lo ? "LTDR" : "LTER", "2,2");
    ins(NULL, "BC", "8,@L%d", ok);
    if (b >= 0) {
        ins(NULL, lo ? "LD" : "LE", "2,%d(,13)", b);
        ins(NULL, lo ? "LTDR" : "LTER", "2,2");
        ins(NULL, "BC", "8,@L%d", ok);
    }
    ins(NULL, "BAL", "14,@U%d", CURP->id);
    here(ok);
}

/* FIXEDOVERFLOW if the last instruction overflowed (condition code 3). */
static void check_fofl(void)
{
    if (!(cur_mask & EN_FOFL))
        return;
    need_fofl = 1;
    ins(NULL, "BC", "14,*+8");
    ins(NULL, "BAL", "14,@O%d", CURP->id);
}

static int gen_flt(struct expr *e, struct type t);
static struct place gen_place(struct expr *e);

/* A complex * / ** of the run time (check.c cx_call, rt/pl1cx.h): the
 * operands' parts into temporaries, then the call with their addresses
 * (for ** the integer exponent's value), the address of the complex
 * temporary e->cxt for the result and whether UNDERFLOW is enabled.
 * The caller then reads the part e->l from the temporary. */
/* The address of a FLOAT operand of a run-time call in a register: a
 * variable of the call's length where it is, any other value in a
 * temporary. */
static int flt_arg_addr(struct expr *x, struct type t)
{
    int r;
    if (x->k == E_VAR && x->sym && is_float(x->t) && flt_long(x->t) == flt_long(t)) {
        struct place p = gen_place(x);
        r = p_take(&p);
        ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
        p_free_except(&p, r);
    } else {
        int o = gen_flt(x, t);
        r = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", r, o);
    }
    return r;
}

static int gen_dec(struct expr *e);
static int gen_bin(struct expr *e);

/* the conditions a FIXED complex routine raises, and the scales and
 * precision it works with (rt/pl1cxf.c) */
static int cx_fixed_desc(int q1, int q2, struct type t)
{
    return (q1 & 255) | (q2 & 255) << 8 | (t.q & 255) << 16 | t.p << 24 |
           (cur_mask & EN_FOFL ? 1 << 29 : 0) | (cur_mask & EN_ZDIV ? 1 << 30 : 0);
}

/* a FIXED part for a complex routine: DECIMAL the address of its
 * 8-byte packed value, BINARY the value */
static int fix_arg(struct expr *x)
{
    int r;
    if (x->t.k == TY_FIXBIN)
        return gen_bin(x);
    r = reg_get();
    ins(NULL, "LA", "%d,%d(,13)", r, gen_dec(x));
    return r;
}

static void gen_string(struct expr *e, int *ra, int *rl);

static void gen_cxop(struct expr *e)
{
    int a[4], n = e->nargs, r;
    int lo = flt_long(e->t), fx = !is_float(e->t);
    struct expr v;
    struct place p;
    if (e->fn == B_CXFC) {
        /* a function's complex value, from its result area into the
         * temporary the parts are read from (check.c cx_part) */
        int res = gen_fcall(e->arg[0]), n2 = type_size(e->cxt->t);
        memset(&v, 0, sizeof v);
        v.k = E_VAR;
        v.sym = e->cxt;
        v.t = e->cxt->t;
        p = gen_place(&v);
        ins(NULL, "MVC", "%s,%d(13)", p_ss(&p, 0, n2), res);
        p_free(&p);
        return;
    }
    if (e->fn == B_CXCVT) {
        /* a string to the complex temporary (rt/pl1cxsp.c): A(string),
         * its length, A(temporary), FLOAT 256 (+1 long) or the scale */
        int ra, rl;
        gen_string(e->arg[0], &ra, &rl);
        arg_reg(ra);
        arg_reg(rl);
        reg_free(ra);
        reg_free(rl);
        memset(&v, 0, sizeof v);
        v.k = E_VAR;
        v.sym = e->cxt;
        v.t = e->cxt->t;
        p = gen_place(&v);
        r = p_take(&p);
        ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
        p_free_except(&p, r);
        arg_reg(r);
        reg_free(r);
        arg_const(is_float(e->t) ? 256 + lo : e->t.q & 255);
        call("PL1CXCV");
        return;
    }
    for (int i = 0; i < n; i++)
        a[i] = fx ? fix_arg(e->arg[i]) :
               e->fn == B_CXOP && e->op == T_POWER && n == 3 && i == 2 ? gen_int(e->arg[i]) :
               flt_arg_addr(e->arg[i], e->t);
    for (int i = 0; i < n; i++) {
        arg_reg(a[i]);
        reg_free(a[i]);
    }
    memset(&v, 0, sizeof v);
    v.k = E_VAR;
    v.sym = e->cxt;
    v.t = e->cxt->t;
    p = gen_place(&v);
    r = p_take(&p);
    ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
    p_free_except(&p, r);
    arg_reg(r);
    reg_free(r);
    if (e->fn == B_CXFN) {
        static const struct { enum bfn f; const char *s, *l; } fns[] = {
            { B_SQRT, "PL1CXQS", "PL1CXQL" }, { B_EXP, "PL1CXES", "PL1CXEL" },
            { B_LOG, "PL1CXLS", "PL1CXLL" }, { B_SIN, "PL1CXSS", "PL1CXSL" },
            { B_COS, "PL1CXCS", "PL1CXCL" }, { B_SINH, "PL1CXYS", "PL1CXYL" },
            { B_COSH, "PL1CXKS", "PL1CXKL" }, { B_TAN, "PL1CXTS", "PL1CXTL" },
            { B_TANH, "PL1CXHS", "PL1CXHL" }, { B_ATAN, "PL1CXNS", "PL1CXNL" },
            { B_ATANH, "PL1CXZS", "PL1CXZL" },
        };
        arg_const(0);
        for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
            if ((int)fns[i].f == e->op) {
                call(lo ? fns[i].l : fns[i].s);
                return;
            }
        fatal("internal: no routine for this complex function");
    }
    if (fx) {
        int dec = e->t.k == TY_FIXDEC;
        arg_const(cx_fixed_desc(e->arg[0]->t.q, e->arg[2]->t.q, e->t));
        call(e->op == T_STAR ? (dec ? "PL1CXMD" : "PL1CXMB") : (dec ? "PL1CXDD" : "PL1CXDB"));
        return;
    }
    arg_const(cur_mask & EN_UFL ? 1 : 0);
    call(e->op == T_STAR ? (lo ? "PL1CXML" : "PL1CXMS") :
         e->op == T_SLASH ? (lo ? "PL1CXDL" : "PL1CXDS") :
         n == 4 ? (lo ? "PL1CXXL" : "PL1CXXS") : (lo ? "PL1CXPL" : "PL1CXPS"));
}

/* ---- constants ------------------------------------------------------ */

/* Value of an arithmetic constant (or its negation) as a scaled binary
 * with scale qb: trunc(v * 2**qb), as the conversion to binary does. */
static int const_bin(struct expr *e, int qb, int *v)
{
    int neg = 0;
    if (e->k == E_NEG) { neg = 1; e = e->l; }
    if (e->k != E_NUM)
        return 0;
    {
        long double x = 0;
        long long n;
        for (const char *d = e->num; *d; d++)
            x = x * 10 + (*d - '0');
        for (int i = 0; i < e->t.q; i++)
            x /= 10;
        for (int i = 0; i < qb; i++)
            x *= 2;
        n = (long long)x;              /* truncates toward zero */
        if (n > 2147483647LL)
            fatal("%s(%d): constant too large for FIXED BINARY(31)", src_name, e->line);
        *v = (int)(neg ? -n : n);
    }
    return 1;
}

static int const_value(struct expr *e, int *v)
{
    return const_bin(e, 0, v);
}

static void load_const(int r, int v)
{
    if (v >= 0 && v <= 4095)
        ins(NULL, "LA", "%d,%d", r, v);
    else
        ins(NULL, "L", "%d,=F'%d'", r, v);
}

/* ---- arithmetic: binary --------------------------------------------- */

static int gen_dec(struct expr *e);
static int gen_flt(struct expr *e, struct type t);
struct target;
static void gen_store(struct target *tg, struct type tt, struct expr *e);
static int gen_cast(struct expr *e);
static int type_size(struct type t);
static void gen_string(struct expr *e, int *ra, int *rl);
static void gen_bits(struct expr *e, int *ra, int *rl);
static int bit_reg_ok(struct expr *e);
static int gen_bitval(struct expr *e);
static int file_desc(struct expr *f);

/* Change the scale of a binary value in register r.  A smaller scale
 * truncates toward zero, as PL/I (F) does (-3.5 becomes -3). */
static void bin_rescale(int r, int from, int to)
{
    if (to > from) {
        ins(NULL, "SLA", "%d,%d", r, to - from);
    } else if (to < from) {
        int k = from - to, l = new_label();
        ins(NULL, "LTR", "%d,%d", r, r);
        ins(NULL, "BC", "10,@L%d", l);
        ins(NULL, "A", "%d,=F'%d'", r, (1 << k) - 1);
        here(l);
        ins(NULL, "SRA", "%d,%d", r, k);
    }
}

static int gen_bin(struct expr *e);

/* Value as an integer in a register (scale 0). */
static int gen_int(struct expr *e)
{
    int r = gen_bin(e);
    bin_rescale(r, bin_scale(e->t), 0);
    return r;
}

/* Value of an arithmetic expression as a FIXED BINARY scaled by
 * bin_scale(e->t), in a register. */
static int gen_bin_at(struct expr *e, int qs);

static int gen_bin(struct expr *e)
{
    return gen_bin_at(e, bin_scale(e->t));
}

/* gen_bin, a decimal value converted to the binary scale qs: an
 * assignment converts it to its target's scale directly (Q2SCNV: 1.1
 * to FIXED BIN(31,16) is 72089/65536, not 17/16 * 4096/4096) */
static int gen_bin_at(struct expr *e, int qs)
{
    int r, v;

    if (e->k == E_CALL && (e->fn == B_CXOP || e->fn == B_CXFN || e->fn == B_CXCVT || e->fn == B_CXFC)) {
        gen_cxop(e);
        e = e->l;
    }
    if (e->k == E_CALL && e->fn == B_ROUND && e->t.k == TY_FIXBIN) {
        /* ROUND of FIXED BINARY: the magnitude plus half a unit of binary
         * digit n, cut there; then to the result's scale (check.c) */
        struct expr *a = e->arg[1];
        int n = 0, q = e->arg[0]->t.q, neg = a->k == E_NEG, l1 = new_label(), l2 = new_label();
        for (const char *d = (neg ? a->l : a)->num; *d; d++)
            n = n * 10 + (*d - '0');
        if (neg)
            n = -n;
        r = gen_bin(e->arg[0]);
        if (n < q) {
            int k = q - n;
            unsigned half = 1u << (k - 1), mask = ~((1u << k) - 1u);
            ins(NULL, "LTR", "%d,%d", r, r);
            ins(NULL, "BC", "4,@L%d", l1);
            ins(NULL, "A", "%d,=F'%d'", r, (int)half);
            ins(NULL, "N", "%d,=F'%d'", r, (int)mask);
            ins(NULL, "B", "@L%d", l2);
            here(l1);
            ins(NULL, "LCR", "%d,%d", r, r);
            ins(NULL, "A", "%d,=F'%d'", r, (int)half);
            ins(NULL, "N", "%d,=F'%d'", r, (int)mask);
            ins(NULL, "LCR", "%d,%d", r, r);
            here(l2);
        }
        bin_rescale(r, q, qs);
        return r;
    }
    if (e->k == E_CALL && e->fn == B_CXABS && e->t.k == TY_FIXBIN) {
        /* ABS of COMPLEX FIXED BINARY: the parts' values, A(result) */
        int a = gen_bin(e->arg[0]), b = gen_bin(e->arg[1]), t = tmp(4, 4);
        arg_reg(a);
        arg_reg(b);
        reg_free(a);
        reg_free(b);
        r = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", r, t);
        arg_reg(r);
        arg_const(cx_fixed_desc(e->arg[0]->t.q, 0, e->t));
        call("PL1CXAB");
        ins(NULL, "L", "%d,%d(,13)", r, t);
        bin_rescale(r, e->t.q, qs);
        return r;
    }

    if (is_bit(e->t)) {
        /* a bit string's value (JOB00336) */
        int ra, rl;
        gen_bits(e, &ra, &rl);
        arg_reg(ra);
        arg_reg(rl);
        reg_free(ra);
        reg_free(rl);
        call("PL1BTOI");
        r = reg_get();
        ins(NULL, "LR", "%d,15", r);
        return r;
    }

    if (const_bin(e, qs, &v)) {
        r = reg_get();
        load_const(r, v);
        return r;
    }
    if (is_float(e->t)) {
        /* FLOAT to binary: truncated, the low 32 bits kept */
        int t = gen_flt(e, e->t);
        r = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", r, t);
        arg_reg(r);
        arg_const(flt_long(e->t));
        arg_const(qs);
        call("PL1FTOB");
        ins(NULL, "LR", "%d,15", r);
        return r;
    }
    if (e->t.k == TY_FIXDEC) {
        /* FIXED DECIMAL(p,q) to FIXED BINARY(.., CEIL(q*3.32)):
         * trunc(value * 2**qs), through packed arithmetic and CVB. */
        int t = gen_dec(e);
        int dw = tmp(8, 8);
        if (qs > 46)
            fatal("%s(%d): a decimal fraction of more than 13 digits cannot meet a binary value yet",
                  src_name, e->line);
        if (qs > 0 || e->t.q > 0) {
            /* times 2**qs, then the decimal fraction shifted off; qs is
             * 0 for an integer target (gen_bin_at: I = P, t2dec) */
            int w = tmp(16, 1);
            ins(NULL, "ZAP", "%s,%s", tss(w, 16), tss(t, 8));
            if (qs > 0) {
                char pow2[24];
                snprintf(pow2, sizeof pow2, "=P'%ld'", 1L << qs);
                ins(NULL, "MP", "%s,%s", tss(w, 16), pow2);
            }
            if (e->t.q > 0)
                ins(NULL, "SRP", "%s,%d,0", tss(w, 16), 64 - e->t.q);
            ins(NULL, "ZAP", "%s,%s", tss(dw, 8), tss(w, 16));
        } else {
            ins(NULL, "ZAP", "%s,%s", tss(dw, 8), tss(t, 8));
        }
        r = reg_get();
        ins(NULL, "CVB", "%d,%d(,13)", r, dw);
        return r;
    }
    switch (e->k) {
    case E_VAR: {
        struct place p = gen_place(e);
        r = p_take(&p);
        ins(NULL, e->sym->t.p <= 15 ? "LH" : "L", "%d,%s", r, p_rx(&p, 0));
        p_free_except(&p, r);
        return r;
    }
    case E_FCALL: {
        int res = gen_fcall(e);
        r = reg_get();
        ins(NULL, e->t.p <= 15 ? "LH" : "L", "%d,%d(,13)", r, res);
        return r;
    }
    case E_NEG:
        r = gen_bin(e->l);
        ins(NULL, "LCR", "%d,%d", r, r);
        check_fofl();
        return r;
    case E_BIN: {
        int a = gen_bin(e->l), b = gen_bin(e->r);
        int sa = bin_scale(e->l->t), sb = bin_scale(e->r->t);
        switch (e->op) {
        case T_PLUS:
        case T_MINUS:
            bin_rescale(a, sa, qs);
            bin_rescale(b, sb, qs);
            ins(NULL, e->op == T_PLUS ? "AR" : "SR", "%d,%d", a, b);
            check_fofl();
            break;
        case T_STAR:
            /* the product must fit a word: SLDA 0,32 overflows if not */
            ins(NULL, "LR", "1,%d", a);
            ins(NULL, "MR", "0,%d", b);
            ins(NULL, "SLDA", "0,32");
            check_fofl();
            ins(NULL, "LR", "%d,0", a);
            break;
        case T_SLASH: {
            /* (a * 2**s) / b, s = qs - sa + sb; DR truncates toward zero */
            int sh = qs - sa + sb, ok = new_label(), done = new_label();
            ins(NULL, "LTR", "%d,%d", b, b);
            ins(NULL, "BC", "7,@L%d", ok);
            raise_zd();                   /* the quotient is undefined */
            ins(NULL, "B", "@L%d", done);
            here(ok);
            ins(NULL, "LR", "0,%d", a);
            ins(NULL, "SRDA", "0,32");
            if (sh > 0)
                ins(NULL, "SLDA", "0,%d", sh);
            ins(NULL, "DR", "0,%d", b);
            ins(NULL, "LR", "%d,1", a);
            here(done);
            break;
        }
        default:
            fatal("internal: binary operator %d", e->op);
        }
        reg_free(b);
        return a;
    }
    case E_CAST: {
        int t = gen_cast(e);
        r = reg_get();
        ins(NULL, e->t.p <= 15 ? "LH" : "L", "%d,%d(,13)", r, t);
        return r;
    }
    case E_CALL:
        switch (e->fn) {
        case B_STATUS: {
            /* an event's status, a fullword (rt/pl1abi.h PL1_EV_STATUS) */
            struct place p = gen_place(e->arg[0]);
            int r = p_take(&p);
            ins(NULL, "L", "%d,%s", r, p_rx(&p, PL1_EV_STATUS));
            p_free_except(&p, r);
            return r;
        }
        case B_VERIFY: {
            int a1, l1, a2, l2;
            gen_string(e->arg[0], &a1, &l1);
            gen_string(e->arg[1], &a2, &l2);
            arg_reg(a1); arg_reg(l1); arg_reg(a2); arg_reg(l2);
            call("PL1VERI");
            reg_free(l1); reg_free(a2); reg_free(l2);
            ins(NULL, "LR", "%d,15", a1);
            return a1;
        }
        case B_SIGN: {
            /* -1, 0 or 1 by the sign of the argument */
            struct expr *x = e->arg[0];
            int neg = new_label(), pos = new_label(), done = new_label();
            if (is_float(x->t)) {
                int f = gen_flt(x, x->t), lo = flt_long(x->t);
                ins(NULL, lo ? "LD" : "LE", "0,%d(,13)", f);
                ins(NULL, lo ? "LTDR" : "LTER", "0,0");
            } else if (x->t.k == TY_FIXDEC) {
                int d = gen_dec(x);
                ins(NULL, "CP", "%s,=P'0'", tss(d, 8));
            } else {
                int b = gen_bin(x);
                ins(NULL, "LTR", "%d,%d", b, b);
                reg_free(b);
            }
            r = reg_get();
            ins(NULL, "BC", "4,@L%d", neg);
            ins(NULL, "BC", "2,@L%d", pos);
            ins(NULL, "SR", "%d,%d", r, r);
            ins(NULL, "B", "@L%d", done);
            here(neg);
            ins(NULL, "L", "%d,=F'-1'", r);
            ins(NULL, "B", "@L%d", done);
            here(pos);
            ins(NULL, "LA", "%d,1", r);
            here(done);
            return r;
        }
        case B_FLOOR:
        case B_CEIL:
        case B_TRUNC: {
            /* the scaled binary at its scale q, to scale 0 */
            struct expr *x = e->arg[0];
            int q = bin_scale(x->t);
            r = gen_bin(x);
            if (q > 0) {
                if (e->fn == B_TRUNC) {
                    bin_rescale(r, q, 0);
                } else {
                    if (e->fn == B_CEIL)
                        ins(NULL, "A", "%d,=F'%d'", r, (1 << q) - 1);
                    ins(NULL, "SRA", "%d,%d", r, q);   /* towards minus infinity */
                }
            }
            return r;
        }
        case B_STG:
        case B_CSTG: {
            /* the bytes as mapped; CURRENTSTORAGE of a varying string its
             * prefix and current length, of an area its control bytes and
             * current extent (SC26-3114-01, CURRENTSTORAGE) */
            struct expr *s = e->arg[0];
            struct sym *x = s->sym;
            int whole = s->nsubs == 0 && x->ndims > 0;
            if (e->fn == B_CSTG && !whole && !x->child &&
                (x->t.k == TY_VCHAR || x->t.k == TY_VBIT)) {
                struct place p = gen_place(s);
                r = p_take(&p);
                ins(NULL, "LH", "%d,%s", r, p_rx(&p, 0));
                p_free_except(&p, r);
                if (x->t.k == TY_VBIT) {
                    ins(NULL, "LA", "%d,7(,%d)", r, r);
                    ins(NULL, "SRL", "%d,3", r);
                }
                ins(NULL, "LA", "%d,2(,%d)", r, r);
                return r;
            }
            if (e->fn == B_CSTG && !whole && !x->child && x->t.k == TY_AREA) {
                struct place p = gen_place(s);
                int a = p_take(&p);
                ins(NULL, "LA", "%d,%s", a, p_rx(&p, 0));
                p_free_except(&p, a);
                arg_reg(a);
                reg_free(a);
                call("PL1ACST");
                r = reg_get();
                ins(NULL, "LR", "%d,15", r);
                return r;
            }
            r = reg_get();
            load_const(r, whole || (x->child && s->nsubs == 0) ? total_size(x) :
                          x->child ? x->size - x->tail : type_size(x->t));
            return r;
        }
        case B_LENGTH: {
            struct expr *s = e->arg[0];
            if (s->k == E_VAR && (s->t.k == TY_VCHAR || s->t.k == TY_VBIT)) {
                struct place p = gen_place(s);
                r = p_take(&p);
                ins(NULL, "LH", "%d,%s", r, p_rx(&p, 0));
                p_free_except(&p, r);
                return r;
            }
            if (s->k == E_VAR && s->sym->star && (s->t.k == TY_CHAR || s->t.k == TY_BIT))
                return star_len(s->sym);
            if (s->k == E_VAR && s->sym->refer && s->sym->refer_dim < 0) {
                int ra, rl;
                gen_string(s, &ra, &rl);
                reg_free(ra);
                return rl;
            }
            if (s->k == E_VAR && (s->t.k == TY_CHAR || s->t.k == TY_BIT)) {
                r = reg_get();
                load_const(r, s->t.p);
                return r;
            }
            if (is_bit(s->t)) {
                int ra, rl;
                gen_bits(s, &ra, &rl);
                reg_free(ra);
                return rl;
            }
            {
                int ra, rl;
                gen_string(s, &ra, &rl);
                reg_free(ra);
                return rl;
            }
        }
        case B_INDEX: {
            int a1, l1, a2, l2, bits = is_bit(e->arg[0]->t);
            if (bits) {
                gen_bits(e->arg[0], &a1, &l1);
                gen_bits(e->arg[1], &a2, &l2);
            } else {
                gen_string(e->arg[0], &a1, &l1);
                gen_string(e->arg[1], &a2, &l2);
            }
            arg_reg(a1); arg_reg(l1); arg_reg(a2); arg_reg(l2);
            call(bits ? "PL1INDB" : "PL1INDX");
            reg_free(l1); reg_free(a2); reg_free(l2);
            ins(NULL, "LR", "%d,15", a1);
            return a1;
        }
        case B_ABS:
            r = gen_bin(e->arg[0]);
            bin_rescale(r, bin_scale(e->arg[0]->t), qs);
            ins(NULL, "LPR", "%d,%d", r, r);
            return r;
        case B_MOD: {
            /* PL/I (F): the result lies in [0,|y|) (JOB00096: MOD(17,-5) = 2,
             * MOD(-17,-5) = 3).  DR leaves the remainder with the sign of
             * x; a negative one gets |y| added. */
            int x = gen_bin(e->arg[0]), y = gen_bin(e->arg[1]), l = new_label();
            int ok = new_label();
            ins(NULL, "LTR", "%d,%d", y, y);
            ins(NULL, "BC", "7,@L%d", ok);
            raise_zd();                   /* MOD(x,0): ZERODIVIDE (JOB00137) */
            ins(NULL, "SR", "0,0");
            ins(NULL, "B", "@L%d", l);
            here(ok);
            ins(NULL, "LR", "0,%d", x);
            ins(NULL, "SRDA", "0,32");
            ins(NULL, "DR", "0,%d", y);
            ins(NULL, "LTR", "0,0");
            ins(NULL, "BC", "10,@L%d", l);
            ins(NULL, "LPR", "1,%d", y);
            ins(NULL, "AR", "0,1");
            here(l);
            ins(NULL, "LR", "%d,0", x);
            reg_free(y);
            return x;
        }
        case B_MAX:
        case B_MIN: {
            int a = gen_bin(e->arg[0]);
            bin_rescale(a, bin_scale(e->arg[0]->t), qs);
            for (int i = 1; i < e->nargs; i++) {
                int b = gen_bin(e->arg[i]), l = new_label();
                bin_rescale(b, bin_scale(e->arg[i]->t), qs);
                ins(NULL, "CR", "%d,%d", a, b);
                ins(NULL, "BC", "%d,@L%d", e->fn == B_MAX ? 10 : 12, l);
                ins(NULL, "LR", "%d,%d", a, b);
                here(l);
                reg_free(b);
            }
            return a;
        }
        case B_ONCODE:
            call("PL1ONCD");
            r = reg_get();
            ins(NULL, "LR", "%d,15", r);
            return r;
        case B_ONCOUNT:
            r = reg_get();
            ins(NULL, "SR", "%d,%d", r, r);
            return r;
        case B_COUNT:
        case B_LINENO:
        case B_PAGENO:
            r = file_desc(e->arg[0]);
            arg_reg(r);
            reg_free(r);
            call(e->fn == B_COUNT ? "PL1COUN" : e->fn == B_LINENO ? "PL1LINE" : "PL1PGNO");
            r = reg_get();
            ins(NULL, "LR", "%d,15", r);
            return r;
        case B_BINVALUE:
            return gen_ptr(e->arg[0]);   /* the pointer's bits as FIXED BIN(31) */
        case B_PLIRETV:
            r = reg_get();
            ins(NULL, "L", "%d,=V(PL1RTCD)", r);
            ins(NULL, "L", "%d,0(,%d)", r, r);
            return r;
        case B_DIM:
        case B_LBOUND:
        case B_HBOUND: {
            /* of an A(*) parameter (the others lower.c folded): from the
             * descriptor, A(data), F(element length), then F(lb), F(hb)
             * for each dimension */
            int d = 8 + 8 * (atoi(e->arg[1]->num) - 1), dr = star_desc(e->arg[0]->sym);
            r = reg_get();
            ins(NULL, "L", "%d,%d(,%d)", r, e->fn == B_LBOUND ? d : d + 4, dr);
            if (e->fn == B_DIM) {
                ins(NULL, "S", "%d,%d(,%d)", r, d, dr);
                ins(NULL, "LA", "%d,1(,%d)", r, r);
            }
            reg_free(dr);
            return r;
        }
        case B_SUBSTR:
        case B_ONCHAR:
        case B_ONSOURCE:
        case B_NULL:
        case B_ADDR:
        case B_TRANSLATE:
        case B_REPEAT:
        case B_HIGH:
        case B_LOW:
        case B_UNSPEC:
        case B_ROUND:
        case B_FIXED:
        case B_FLOAT:
        case B_BINARY:
        case B_DECIMAL:
        case B_PRECISION:
        case B_ADD:
        case B_MULTIPLY:
        case B_DIVIDE:
        case B_SQRT: case B_EXP: case B_LOG: case B_LOG10: case B_LOG2: case B_SIN: case B_COS:
        case B_TAN: case B_SIND: case B_COSD: case B_TAND: case B_ATAN: case B_ATAND:
        case B_SINH: case B_COSH: case B_TANH: case B_ATANH: case B_ERF: case B_ERFC:
        case B_PICVAL: case B_ALLOCATION: case B_ONFILE: case B_ONKEY: case B_BOOL: case B_CHAR: case B_BIT: case B_NULLO: case B_EMPTY:
        case B_SUM: case B_PROD: case B_POLY: case B_ALL: case B_ANY:
        case B_ONLOC: case B_DATE: case B_TIME: case B_DATAFIELD:
        case B_AFTER: case B_BEFORE: case B_DECAT: case B_REVERSE: case B_COPY: case B_COLLATE:
        case B_ASIN: case B_ACOS: case B_SUBTRACT: case B_DOT: case B_EVERY: case B_SOME:
        case B_OFFSETF: case B_POINTERF: case B_VALID: case B_STRING:
        case B_SYSNULL: case B_PTRADD: case B_PTRVALUE: case B_DATETIME: case B_PLIRETC:
        case B_ENTRYADDR: case B_PLIDUMP:
        case B_PLISRTA: case B_PLISRTB: case B_PLISRTC: case B_PLISRTD:
        case B_PLICKPT: case B_PLIREST: case B_PLICANC:
        case B_COMPLETION:
        case B_REAL: case B_IMAG: case B_CONJG: case B_COMPLEX: case B_CPXSTR: case B_CXOP:
        case B_CXABS: case B_CXFN: case B_CXCVT: case B_CXPAIR: case B_CXFC:
            break;
        }
        break;
    default:
        break;
    }
    fatal("%s(%d): internal: no binary code for this expression", src_name, e->line);
    return 0;
}

/* ---- arithmetic: decimal -------------------------------------------- */

/* Align a packed temporary of scale from to scale to. */
static void rescale(int t, int from, int to)
{
    if (to > from)
        ins(NULL, "SRP", "%s,%d,0", tss(t, 8), to - from);
    else if (to < from)
        ins(NULL, "SRP", "%s,%d,0", tss(t, 8), 64 - (from - to));
}

/* Value of an arithmetic expression as FIXED DECIMAL in an 8-byte packed
 * temporary, at scale dec_scale(e->t).  Returns the frame offset. */
/* FLOAT in the temporary f to FIXED DECIMAL(15,q) in a new one,
 * truncated. */
static int flt_to_dec(int f, int longf, int q)
{
    int t = tmp(8, 1), r = reg_get(), d = reg_get();
    ins(NULL, "LA", "%d,%d(,13)", r, f);
    ins(NULL, "LA", "%d,%d(,13)", d, t);
    arg_reg(r);
    arg_const(longf);
    arg_const(q);
    arg_reg(d);
    reg_free(r);
    reg_free(d);
    call("PL1FTOD");
    return t;
}

/* The address of a picture's characters, a literal, in register r. */
static void pic_literal(int r, const char *pic)
{
    ins(NULL, "LA", "%d,=C'%s'", r, pic);
}

static int gen_dec(struct expr *e)
{
    int t;

    if (e->k == E_CALL && (e->fn == B_CXOP || e->fn == B_CXFN || e->fn == B_CXCVT || e->fn == B_CXFC)) {
        gen_cxop(e);
        e = e->l;
    }
    if (e->k == E_CALL && e->fn == B_CXABS && e->t.k == TY_FIXDEC) {
        /* ABS of COMPLEX FIXED DECIMAL: A(parts), A(result) */
        int a = fix_arg(e->arg[0]), b = fix_arg(e->arg[1]), r;
        arg_reg(a);
        arg_reg(b);
        reg_free(a);
        reg_free(b);
        t = tmp(8, 1);
        r = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", r, t);
        arg_reg(r);
        reg_free(r);
        arg_const(cx_fixed_desc(e->arg[0]->t.q, 0, e->t));
        call("PL1CXAD");
        return t;
    }

    if (is_float(e->t))
        return flt_to_dec(gen_flt(e, e->t), flt_long(e->t), 0);
    t = tmp(8, 1);

    if (e->t.k == TY_FIXBIN) {
        int r = gen_bin(e);
        if (e->t.q == 0) {
            int dw = tmp(8, 8);
            ins(NULL, "CVD", "%d,%d(,13)", r, dw);
            ins(NULL, "ZAP", "%s,%s", tss(t, 8), tss(dw, 8));
        } else {
            /* scaled binary: the run time truncates the binary fraction
             * to CEIL(q/3.32) decimal digits */
            int ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", ra, t);
            arg_reg(r);
            arg_const(e->t.q);
            arg_const(dec_scale(e->t));
            arg_reg(ra);
            call("PL1BTOD");
            reg_free(ra);
        }
        reg_free(r);
        return t;
    }
    switch (e->k) {
    case E_CAST: {
        int c = gen_cast(e);
        ins(NULL, "ZAP", "%s,%s", tss(t, 8), tss(c, dec_bytes(e->t.p)));
        return t;
    }
    case E_CONV: {
        /* a string to FIXED DECIMAL(15,q): the run time parses it */
        int ra, rl, rd;
        gen_string(e->l, &ra, &rl);
        rd = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", rd, t);
        arg_reg(ra);
        arg_reg(rl);
        arg_const(e->t.q);
        arg_reg(rd);
        call("PL1CTOD");
        reg_free(ra);
        reg_free(rl);
        reg_free(rd);
        return t;
    }
    case E_NUM:
        ins(NULL, "ZAP", "%s,%s", tss(t, 8), pl_lit(e->num, e->t.p, 0));
        return t;
    case E_VAR: {
        struct place p = gen_place(e);
        ins(NULL, "ZAP", "%s,%s", tss(t, 8), p_ss(&p, 0, dec_bytes(e->sym->t.p)));
        p_free(&p);
        return t;
    }
    case E_FCALL: {
        int res = gen_fcall(e);
        ins(NULL, "ZAP", "%s,%s", tss(t, 8), tss(res, dec_bytes(e->t.p)));
        return t;
    }
    case E_NEG:
        if (e->l->k == E_NUM) {
            ins(NULL, "ZAP", "%s,%s", tss(t, 8), pl_lit(e->l->num, e->l->t.p, 1));
            return t;
        }
        {
            int c = gen_dec(e->l);
            ins(NULL, "ZAP", "%s,=P'0'", tss(t, 8));
            ins(NULL, "SP", "%s,%s", tss(t, 8), tss(c, 8));
        }
        return t;
    case E_BIN: {
        int a = gen_dec(e->l), b = gen_dec(e->r);
        if (e->op == T_STAR) {
            int w = tmp(16, 1);
            ins(NULL, "ZAP", "%s,%s", tss(w, 16), tss(a, 8));
            ins(NULL, "MP", "%s,%s", tss(w, 16), tss(b, 8));
            ins(NULL, "ZAP", "%s,%s", tss(t, 8), tss(w, 16));
            check_fofl();                 /* beyond 15 digits (JOB00140) */
            return t;
        }
        if (e->op == T_SLASH) {
            /* Shift the dividend so that quotient scale = dividend scale
             * - divisor scale; DP truncates toward zero. */
            int w = tmp(16, 1), ok = new_label(), done = new_label();
            int sh = e->t.q + e->r->t.q - e->l->t.q;
            ins(NULL, "CP", "%s,=P'0'", tss(b, 8));
            ins(NULL, "BC", "7,@L%d", ok);
            raise_zd();
            ins(NULL, "ZAP", "%s,=P'0'", tss(t, 8));
            ins(NULL, "B", "@L%d", done);
            here(ok);
            ins(NULL, "ZAP", "%s,%s", tss(w, 16), tss(a, 8));
            if (sh > 0)
                ins(NULL, "SRP", "%s,%d,0", tss(w, 16), sh);
            else if (sh < 0)
                ins(NULL, "SRP", "%s,%d,0", tss(w, 16), 64 + sh);
            ins(NULL, "DP", "%s,%s", tss(w, 16), tss(b, 8));
            ins(NULL, "ZAP", "%s,%s", tss(t, 8), tss(w, 8));
            here(done);
            return t;
        }
        rescale(a, e->l->t.q, e->t.q);
        rescale(b, e->r->t.q, e->t.q);
        ins(NULL, "ZAP", "%s,%s", tss(t, 8), tss(a, 8));
        ins(NULL, e->op == T_PLUS ? "AP" : "SP", "%s,%s", tss(t, 8), tss(b, 8));
        check_fofl();
        return t;
    }
    case E_CALL:
        switch (e->fn) {
        case B_PICVAL: {
            /* the arithmetic value of a numeric picture (rt/pl1pic.c) */
            struct expr *x = e->arg[0];
            struct place p = gen_place(x);
            int ra = p_take(&p), rp = reg_get(), rt = reg_get();
            ins(NULL, "LA", "%d,%s", ra, p_rx(&p, 0));
            p_free_except(&p, ra);
            pic_literal(rp, x->sym->t.pic);
            ins(NULL, "LA", "%d,%d(,13)", rt, t);
            arg_reg(ra);
            arg_reg(rp);
            arg_const((int)strlen(x->sym->t.pic));
            arg_reg(rt);
            reg_free(ra);
            reg_free(rp);
            reg_free(rt);
            call("PL1PVAL");
            return t;
        }
        case B_FLOOR:
        case B_CEIL:
        case B_TRUNC: {
            /* shift the fraction out (towards zero), then step down
             * (FLOOR, negative) or up (CEIL, positive) when it was not 0 */
            struct expr *x = e->arg[0];
            int q = dec_scale(x->t), c = gen_dec(x), w = tmp(8, 1), done = new_label();
            ins(NULL, "ZAP", "%s,%s", tss(t, 8), tss(c, 8));
            if (q == 0)
                return t;
            ins(NULL, "SRP", "%s,%d,0", tss(t, 8), 64 - q);
            if (e->fn == B_TRUNC)
                return t;
            ins(NULL, "ZAP", "%s,%s", tss(w, 8), tss(t, 8));
            ins(NULL, "SRP", "%s,%d,0", tss(w, 8), q);
            ins(NULL, "CP", "%s,%s", tss(w, 8), tss(c, 8));
            ins(NULL, "BC", "8,@L%d", done);          /* no fraction */
            ins(NULL, "CP", "%s,=P'0'", tss(c, 8));
            ins(NULL, "BC", "%d,@L%d", e->fn == B_FLOOR ? 10 : 12, done);
            ins(NULL, e->fn == B_FLOOR ? "SP" : "AP", "%s,=P'1'", tss(t, 8));
            here(done);
            return t;
        }
        case B_ROUND: {
            /* SRP rounds half away from zero; then back to scale q */
            struct expr *x = e->arg[0];
            int q = x->t.q, n = 0, c = gen_dec(x);
            struct expr *a = e->arg[1];
            int neg = a->k == E_NEG;
            for (const char *d = (neg ? a->l : a)->num; *d; d++)
                n = n * 10 + (*d - '0');
            if (neg)
                n = -n;
            ins(NULL, "ZAP", "%s,%s", tss(t, 8), tss(c, 8));
            if (n < q) {
                ins(NULL, "SRP", "%s,%d,5", tss(t, 8), 64 - (q - n));
                q = n;                      /* now at scale n */
            }
            /* to the result's scale: q (PL/I (F)) or n (check.c) */
            if (e->t.q > q)
                ins(NULL, "SRP", "%s,%d,0", tss(t, 8), e->t.q - q);
            else if (e->t.q < q)
                ins(NULL, "SRP", "%s,%d,0", tss(t, 8), 64 - (q - e->t.q));
            return t;
        }
        case B_ABS: {
            int c = gen_dec(e->arg[0]);
            ins(NULL, "ZAP", "%s,%s", tss(t, 8), tss(c, 8));
            ins(NULL, "OI", "%d(13),X'0F'", t + 7);
            ins(NULL, "NI", "%d(13),X'FC'", t + 7);
            return t;
        }
        case B_MOD: {
            /* the remainder of DP has the sign of the dividend; a negative
             * one gets |y| added: the result lies in [0,|y|) */
            int x = gen_dec(e->arg[0]), y = gen_dec(e->arg[1]), w = tmp(16, 1);
            int lend = new_label(), ok = new_label();
            ins(NULL, "CP", "%s,=P'0'", tss(y, 8));
            ins(NULL, "BC", "7,@L%d", ok);
            raise_zd();
            ins(NULL, "ZAP", "%s,=P'0'", tss(t, 8));
            ins(NULL, "B", "@L%d", lend);
            here(ok);
            ins(NULL, "ZAP", "%s,%s", tss(w, 16), tss(x, 8));
            ins(NULL, "DP", "%s,%s", tss(w, 16), tss(y, 8));
            ins(NULL, "ZAP", "%s,%s", tss(t, 8), tss(w + 8, 8));
            ins(NULL, "BC", "10,@L%d", lend);       /* zero or positive */
            ins(NULL, "OI", "%d(13),X'0F'", y + 7);
            ins(NULL, "NI", "%d(13),X'FC'", y + 7);  /* |y| */
            ins(NULL, "AP", "%s,%s", tss(t, 8), tss(y, 8));
            here(lend);
            return t;
        }
        case B_MAX:
        case B_MIN: {
            int a = gen_dec(e->arg[0]);
            rescale(a, e->arg[0]->t.q, e->t.q);
            ins(NULL, "ZAP", "%s,%s", tss(t, 8), tss(a, 8));
            for (int i = 1; i < e->nargs; i++) {
                int b = gen_dec(e->arg[i]), l = new_label();
                rescale(b, e->arg[i]->t.q, e->t.q);
                ins(NULL, "CP", "%s,%s", tss(t, 8), tss(b, 8));
                ins(NULL, "BC", "%d,@L%d", e->fn == B_MAX ? 10 : 12, l);
                ins(NULL, "ZAP", "%s,%s", tss(t, 8), tss(b, 8));
                here(l);
            }
            return t;
        }
        default:
            break;
        }
        break;
    default:
        break;
    }
    fatal("%s(%d): internal: no decimal code for this expression", src_name, e->line);
    return 0;
}

/* ---- arithmetic: floating point ------------------------------------- */

/* A FLOAT value lives in an 8-byte temporary: a short one in its first 4
 * bytes.  Operations load it into FPR 0 (and 2) and store the result:
 * nothing stays in a floating-point register across a call. */

static const char *fl(int longf, const char *shortop, const char *longop)
{
    return longf ? longop : shortop;
}

/* The bytes of a constant converted as the compiler does it, to the
 * length the context needs (JOB00156: B = 1E-70 is converted long). */
static void flt_constant(struct expr *e, int longf, unsigned char *b)
{
    int neg = 0;
    memset(b, 0, 8);
    if (e->k == E_NEG) {
        neg = 1;
        e = e->l;
    }
    if (pl1f_constant(e->num, (int)strlen(e->num), is_float(e->t) ? e->e10 : -e->t.q, neg,
                      longf, b) != 0)
        error_at(e->line, "this constant is beyond the range of FLOAT");
}

static const char *flt_lit(const unsigned char *b, int longf)
{
    static char lit[4][24];
    static int k;
    char *r = lit[k++ & 3];
    int n = longf ? 8 : 4;
    r[0] = '=';
    r[1] = 'X';
    r[2] = '\'';
    for (int i = 0; i < n; i++)
        snprintf(r + 3 + 2 * i, 3, "%02X", b[i]);
    strcat(r, "'");
    return r;
}

/* A float of length from in temporary t made of length to: long to short
 * keeps the first word (truncation, as PL/I (F) does), short to long gets
 * a zero second word (exact). */
static void flt_length(int t, int from, int to)
{
    if (!from && to)
        ins(NULL, "XC", "%d(4,13),%d(13)", t + 4, t + 4);
}

/* FIXED or a string to FLOAT through the run time: the decimal integer
 * scaled by the table, as the library does (IHEVPA). */
static int flt_convert(struct expr *e, int longf)
{
    int t = tmp(8, 8), r, d;
    if (e->k == E_CONV && is_float(e->t)) {
        /* a string to FLOAT: assigned, or met by a FLOAT operand */
        int ra, rl;
        gen_string(e->l, &ra, &rl);
        d = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", d, t);
        arg_reg(ra);
        arg_reg(rl);
        arg_const(longf);
        arg_reg(d);
        reg_free(ra);
        reg_free(rl);
        reg_free(d);
        call("PL1CTOF");
        return t;
    }
    if (e->t.k == TY_FIXBIN) {
        r = gen_bin(e);
        d = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", d, t);
        arg_reg(r);
        arg_const(bin_scale(e->t));
        arg_const(longf);
        arg_reg(d);
        reg_free(r);
        reg_free(d);
        call("PL1BTOF");
        return t;
    }
    {
        int v = gen_dec(e);
        r = reg_get();
        d = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", r, v);
        ins(NULL, "LA", "%d,%d(,13)", d, t);
        arg_reg(r);
        arg_const(dec_scale(e->t));
        arg_const(longf);
        arg_reg(d);
        reg_free(r);
        reg_free(d);
        call("PL1DTOF");
        return t;
    }
}

/* The entry of a mathematical function's routine, short or long. */
static const char *math_entry(enum bfn fn, int lo)
{
    switch (fn) {
    case B_SQRT:  return lo ? "PL1SQL0" : "PL1SQS0";
    case B_EXP:   return lo ? "PL1EXL0" : "PL1EXS0";
    case B_LOG:   return lo ? "PL1LNLE" : "PL1LNSE";
    case B_LOG10: return lo ? "PL1LNLD" : "PL1LNSD";
    case B_LOG2:  return lo ? "PL1LNL2" : "PL1LNS2";
    case B_SIN:   return lo ? "PL1SNLS" : "PL1SNSS";
    case B_COS:   return lo ? "PL1SNLC" : "PL1SNSC";
    case B_SIND:  return lo ? "PL1SNLZ" : "PL1SNSZ";
    case B_COSD:  return lo ? "PL1SNLK" : "PL1SNSK";
    case B_TAN:   return lo ? "PL1TNLR" : "PL1TNSR";
    case B_TAND:  return lo ? "PL1TNLD" : "PL1TNSD";
    case B_ATAN:  return lo ? "PL1ATL1" : "PL1ATS1";
    case B_ATAND: return lo ? "PL1ATL3" : "PL1ATS3";
    case B_SINH:  return lo ? "PL1SHLS" : "PL1SHSS";
    case B_COSH:  return lo ? "PL1SHLC" : "PL1SHSC";
    case B_TANH:  return lo ? "PL1THL0" : "PL1THS0";
    case B_ATANH: return lo ? "PL1HTL0" : "PL1HTS0";
    case B_ERF:   return lo ? "PL1EFLF" : "PL1EFSF";
    case B_ERFC:  return lo ? "PL1EFLC" : "PL1EFSC";
    default: break;
    }
    fatal("internal: no routine for this function");
    return NULL;
}

/* Value of an arithmetic expression as a FLOAT of the length of type t,
 * in an 8-byte temporary; returns its frame offset. */
static int gen_flt(struct expr *e, struct type tt)
{
    int L = flt_long(tt), t;

    if (e->k == E_CALL && (e->fn == B_CXOP || e->fn == B_CXFN || e->fn == B_CXCVT || e->fn == B_CXFC)) {
        gen_cxop(e);
        e = e->l;
    }

    if (e->k == E_NUM || (e->k == E_NEG && e->l->k == E_NUM)) {
        unsigned char b[8];
        flt_constant(e, L, b);
        t = tmp(8, 8);
        ins(NULL, "MVC", "%d(%d,13),%s", t, L ? 8 : 4, flt_lit(b, L));
        return t;
    }
    if (!is_float(e->t))
        return flt_convert(e, L);
    switch (e->k) {
    case E_CAST:
        t = gen_cast(e);
        flt_length(t, flt_long(e->t), L);
        return t;
    case E_CONV:
        return flt_convert(e, L);
    case E_VAR: {
        struct place p = gen_place(e);
        int lv = flt_long(e->sym->t);
        t = tmp(8, 8);
        if (L && !lv)
            ins(NULL, "SDR", "0,0");
        ins(NULL, lv && L ? "LD" : "LE", "0,%s", p_rx(&p, 0));
        p_free(&p);
        ins(NULL, L ? "STD" : "STE", "0,%d(,13)", t);
        return t;
    }
    case E_FCALL: {
        int res = gen_fcall(e);
        t = tmp(8, 8);
        ins(NULL, "MVC", "%d(%d,13),%d(13)", t, flt_long(e->t) ? 8 : 4, res);
        flt_length(t, flt_long(e->t), L);
        return t;
    }
    case E_NEG: {
        int lo = flt_long(e->t);
        t = gen_flt(e->l, e->t);
        ins(NULL, fl(lo, "LE", "LD"), "0,%d(,13)", t);
        ins(NULL, fl(lo, "LCER", "LCDR"), "0,0");
        ins(NULL, fl(lo, "STE", "STD"), "0,%d(,13)", t);
        flt_length(t, lo, L);
        return t;
    }
    case E_BIN: {
        int lo = flt_long(e->t);
        int a, b;
        if (e->op == T_POWER && (is_float(e->r->t) || e->r->t.q != 0)) {
            /* x ** y: EXP(y*LOG(x)) (rt/pl1m*.c) */
            int x = gen_flt(e->l, e->t), y = gen_flt(e->r, e->t), r = reg_get();
            t = tmp(8, 8);
            ins(NULL, "LA", "%d,%d(,13)", r, x);
            arg_reg(r);
            ins(NULL, "LA", "%d,%d(,13)", r, y);
            arg_reg(r);
            ins(NULL, "LA", "%d,%d(,13)", r, t);
            arg_reg(r);
            reg_free(r);
            call(lo ? "PL1XXL0" : "PL1XXS0");
            flt_length(t, lo, L);
            return t;
        }
        if (e->op == T_POWER) {
            /* x ** n by the run time, as IHEXIS/IHEXIL compute it */
            int n, r, d;
            a = gen_flt(e->l, e->t);
            n = gen_bin(e->r);
            t = tmp(8, 8);
            r = reg_get();
            d = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", r, a);
            ins(NULL, "LA", "%d,%d(,13)", d, t);
            arg_reg(r);
            arg_const(lo);
            arg_reg(n);
            arg_reg(d);
            reg_free(r);
            reg_free(n);
            reg_free(d);
            call("PL1PWRF");
            flt_length(t, lo, L);
            return t;
        }
        a = gen_flt(e->l, e->t);
        b = gen_flt(e->r, e->t);
        t = tmp(8, 8);
        if (e->op == T_SLASH) {
            int ok = new_label(), done = new_label();
            ins(NULL, fl(lo, "LE", "LD"), "2,%d(,13)", b);
            ins(NULL, fl(lo, "LTER", "LTDR"), "2,2");
            ins(NULL, "BC", "7,@L%d", ok);
            raise_zd();                   /* ONCODE 320 (JOB00156) */
            ins(NULL, "XC", "%d(8,13),%d(13)", t, t);
            ins(NULL, "B", "@L%d", done);
            here(ok);
            ins(NULL, fl(lo, "LE", "LD"), "0,%d(,13)", a);
            ins(NULL, fl(lo, "DER", "DDR"), "0,2");
            ins(NULL, fl(lo, "STE", "STD"), "0,%d(,13)", t);
            check_ufl(t, a, -1, lo);
            here(done);
        } else {
            const char *op = e->op == T_PLUS ? fl(lo, "AE", "AD") : e->op == T_MINUS ?
                             fl(lo, "SE", "SD") : fl(lo, "ME", "MD");
            if (e->op != T_PLUS && e->op != T_MINUS && e->op != T_STAR)
                fatal("internal: FLOAT operator %d", e->op);
            ins(NULL, fl(lo, "LE", "LD"), "0,%d(,13)", a);
            ins(NULL, op, "0,%d(,13)", b);
            /* a short product is long in the register: STE keeps its
             * first word, as PL/I (F) does (JOB00158) */
            ins(NULL, fl(lo, "STE", "STD"), "0,%d(,13)", t);
            if (e->op == T_STAR)
                check_ufl(t, a, b, lo);
        }
        flt_length(t, lo, L);
        return t;
    }
    case E_CALL: {
        int lo = flt_long(e->t);
        switch (e->fn) {
        case B_SQRT: case B_EXP: case B_LOG: case B_LOG10: case B_LOG2: case B_SIN:
        case B_COS: case B_TAN: case B_SIND: case B_COSD: case B_TAND: case B_ATAN:
        case B_ATAND: case B_SINH: case B_COSH: case B_TANH: case B_ATANH: case B_ERF:
        case B_ERFC: {
            /* the run time's routine, short or long (rt/pl1m*.c), called
             * with A(argument)[, A(argument)], A(result); ATAN(y,x) and
             * ATAND(y,x) have entries of their own */
            int a[2], r, n = e->nargs;
            const char *name = math_entry(e->fn, lo);
            for (int i = 0; i < n; i++)
                a[i] = gen_flt(e->arg[i], e->t);
            t = tmp(8, 8);
            r = reg_get();
            for (int i = 0; i < n; i++) {
                ins(NULL, "LA", "%d,%d(,13)", r, a[i]);
                arg_reg(r);
            }
            ins(NULL, "LA", "%d,%d(,13)", r, t);
            arg_reg(r);
            reg_free(r);
            if (n == 2)
                name = e->fn == B_ATAN ? (lo ? "PL1ATL2" : "PL1ATS2")
                                       : (lo ? "PL1ATL4" : "PL1ATS4");
            call(name);
            flt_length(t, lo, L);
            return t;
        }
        case B_CXABS: {
            /* ABS of a complex value: A(real part), A(imaginary part),
             * A(result), UNDERFLOW enabled (rt/pl1cxa.c) */
            int a0 = flt_arg_addr(e->arg[0], e->t), a1 = flt_arg_addr(e->arg[1], e->t), r;
            arg_reg(a0);
            arg_reg(a1);
            reg_free(a0);
            reg_free(a1);
            t = tmp(8, 8);
            r = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", r, t);
            arg_reg(r);
            reg_free(r);
            arg_const(cur_mask & EN_UFL ? 1 : 0);
            call(lo ? "PL1CXAL" : "PL1CXAS");
            flt_length(t, lo, L);
            return t;
        }
        case B_ROUND:
            /* the rightmost bit of the fraction set to 1 (check.c) */
            t = gen_flt(e->arg[0], e->t);
            ins(NULL, "OI", "%d(13),X'01'", t + (lo ? 7 : 3));
            flt_length(t, lo, L);
            return t;
        case B_FLOOR:
        case B_CEIL:
        case B_TRUNC: {
            int f = gen_flt(e->arg[0], e->t), r = reg_get(), d = reg_get();
            t = tmp(8, 8);
            ins(NULL, "LA", "%d,%d(,13)", r, f);
            ins(NULL, "LA", "%d,%d(,13)", d, t);
            arg_reg(r);
            arg_const(lo);
            arg_const(e->fn == B_FLOOR ? 0 : e->fn == B_CEIL ? 1 : 2);
            arg_reg(d);
            reg_free(r);
            reg_free(d);
            call("PL1FINT");
            flt_length(t, lo, L);
            return t;
        }
        case B_ABS:
            t = gen_flt(e->arg[0], e->t);
            ins(NULL, "NI", "%d(13),X'7F'", t);
            flt_length(t, lo, L);
            return t;
        case B_MAX:
        case B_MIN:
            t = gen_flt(e->arg[0], e->t);
            for (int i = 1; i < e->nargs; i++) {
                int b = gen_flt(e->arg[i], e->t), l = new_label();
                ins(NULL, fl(lo, "LE", "LD"), "0,%d(,13)", t);
                ins(NULL, fl(lo, "CE", "CD"), "0,%d(,13)", b);
                ins(NULL, "BC", "%d,@L%d", e->fn == B_MAX ? 10 : 12, l);
                ins(NULL, "MVC", "%d(8,13),%d(13)", t, b);
                here(l);
            }
            flt_length(t, lo, L);
            return t;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
    fatal("%s(%d): internal: no FLOAT code for this expression", src_name, e->line);
    return 0;
}

/* ---- strings -------------------------------------------------------- */

/* Address and length of a string value into two registers.  Computed
 * strings live in temporaries of the frame. */
static void gen_string(struct expr *e, int *ra, int *rl)
{
    if (e->k == E_CALL && (e->fn == B_CXOP || e->fn == B_CXFN || e->fn == B_CXCVT || e->fn == B_CXFC)) {
        gen_cxop(e);
        e = e->l;
    }
    if (is_bit(e->t)) {
        /* a bit string as characters '1' and '0' (JOB00336) */
        int a, l, t = tmp(e->t.p ? e->t.p : 1, 1);
        gen_bits(e, &a, &l);
        *ra = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", *ra, t);
        arg_reg(*ra);
        arg_reg(a);
        arg_reg(l);
        reg_free(a);
        call("PL1BTOC");
        *rl = l;
        return;
    }
    if (is_float(e->t)) {
        /* as PUT LIST writes it: p+6 characters (JOB00156) */
        int f = gen_flt(e, e->t), t = tmp(flt_digits(e->t) + 6, 1), r = reg_get();
        *ra = reg_get();
        *rl = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", *ra, t);
        ins(NULL, "LA", "%d,%d(,13)", r, f);
        arg_reg(*ra);
        arg_reg(r);
        arg_const(flt_long(e->t));
        arg_const(flt_digits(e->t));
        reg_free(r);
        call("PL1FTOC");
        ins(NULL, "LR", "%d,15", *rl);
        return;
    }
    if (arith(e->t)) {
        /* a number as a string: the PUT LIST field, p+3 characters */
        int w = dec_prec(e->t) + 3, t = tmp(w, 1);
        if (e->t.k == TY_FIXBIN) {
            int r = gen_bin(e);
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            arg_reg(*ra);
            arg_reg(r);
            arg_const(e->t.p);
            arg_const(e->t.q);
            call("PL1FFXB");
            *rl = r;
        } else {
            int r;
            if (e->k == E_VAR) {
                struct place p = gen_place(e);
                r = p_take(&p);
                ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
                p_free_except(&p, r);
            } else {
                int d = gen_dec(e);
                r = reg_get();
                ins(NULL, "LA", "%d,%d(,13)", r, d + 8 - dec_bytes(e->t.p));
            }
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            arg_reg(*ra);
            arg_reg(r);
            arg_const(e->t.p);
            arg_const(e->t.q);
            call("PL1FFXD");
            *rl = r;
        }
        ins(NULL, "LR", "%d,15", *rl);
        return;
    }
    switch (e->k) {
    case E_FCALL: {
        int res = gen_fcall(e);
        *ra = reg_get();
        *rl = reg_get();
        if (e->t.k == TY_VCHAR) {
            ins(NULL, "LH", "%d,%d(,13)", *rl, res);
            ins(NULL, "LA", "%d,%d(,13)", *ra, res + 2);
        } else {
            ins(NULL, "LA", "%d,%d(,13)", *ra, res);
            load_const(*rl, e->t.p);
        }
        return;
    }
    case E_STR:
        *ra = reg_get();
        *rl = reg_get();
        ins(NULL, "L", "%d,=A(@L%d)", *ra, str_label(e->str, e->len));
        load_const(*rl, e->len);
        return;
    case E_VAR: {
        struct place p = gen_place(e);
        *ra = p_take(&p);
        *rl = reg_get();
        if (e->sym->t.k == TY_VCHAR) {
            ins(NULL, "LH", "%d,%s", *rl, p_rx(&p, 0));
            ins(NULL, "LA", "%d,%s", *ra, p_rx(&p, 2));
        } else {
            ins(NULL, "LA", "%d,%s", *ra, p_rx(&p, 0));
            if (e->sym->star) {
                reg_free(*rl);
                p_free_except(&p, *ra);
                *rl = star_len(e->sym);
                return;
            }
            if (e->sym->refer && e->sym->refer_dim < 0) {
                reg_free(*rl);
                p_free_except(&p, *ra);
                *rl = refer_len(e->sym, *ra);
                return;
            }
            load_const(*rl, e->picstr ? e->t.p : e->sym->t.p);
        }
        p_free_except(&p, *ra);
        return;
    }
    case E_BIN:
        if (e->op == T_CAT) {
            int a1, l1, a2, l2, t = tmp(e->t.p, 1);
            gen_string(e->l, &a1, &l1);
            gen_string(e->r, &a2, &l2);
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            arg_reg(*ra); arg_reg(a1); arg_reg(l1); arg_reg(a2); arg_reg(l2);
            call("PL1CAT");
            reg_free(a1); reg_free(a2); reg_free(l2);
            *rl = l1;
            ins(NULL, "LR", "%d,15", *rl);
            return;
        }
        break;
    case E_CALL:
        if (e->fn == B_CPXSTR) {
            /* a complex value as a string: the parts as strings, joined
             * real||signed imaginary||'I' (rt/pl1cx.c) */
            int a1, l1, a2, l2, t = tmp(e->t.p, 1);
            gen_string(e->arg[0], &a1, &l1);
            gen_string(e->arg[1], &a2, &l2);
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            arg_reg(*ra); arg_reg(a1); arg_reg(l1); arg_reg(a2); arg_reg(l2);
            call("PL1CPXJ");
            reg_free(a1); reg_free(a2); reg_free(l2);
            *rl = l1;
            ins(NULL, "LR", "%d,15", *rl);
            return;
        }
        if (e->fn == B_HIGH || e->fn == B_LOW) {
            int t = tmp(e->t.p, 1);
            *ra = reg_get();
            *rl = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            load_const(*rl, e->t.p);
            arg_reg(*ra);
            arg_reg(*rl);
            arg_const(e->fn == B_HIGH ? 255 : 0);
            call("PL1FILL");
            return;
        }
        if (e->fn == B_STRING) {
            /* STRING(x): x's characters where x lies (Q1STR) */
            struct expr *a = e->arg[0];
            if (a->k != E_VAR || (!a->sym->child && a->nsubs == cum_dims(a->sym))) {
                gen_string(a, ra, rl);
                return;
            }
            {
                struct place p = gen_place(a);
                *ra = p_take(&p);
                ins(NULL, "LA", "%d,%s", *ra, p_rx(&p, 0));
                p_free_except(&p, *ra);
                *rl = reg_get();
                load_const(*rl, e->t.p);
            }
            return;
        }
        if (e->fn == B_COLLATE) {
            call("PL1COLL");
            *ra = reg_get();
            ins(NULL, "LR", "%d,15", *ra);
            *rl = reg_get();
            load_const(*rl, 256);
            return;
        }
        if (e->fn == B_AFTER || e->fn == B_BEFORE || e->fn == B_DECAT || e->fn == B_REVERSE ||
            e->fn == B_COPY) {
            /* rt/pl1ansi.c: the result in a temporary, its length back */
            int a, l, a2 = 0, l2 = 0, a3 = 0, l3 = 0, n = 0, t = tmp(e->t.p ? e->t.p : 1, 1);
            gen_string(e->arg[0], &a, &l);
            if (e->fn == B_AFTER || e->fn == B_BEFORE || e->fn == B_DECAT)
                gen_string(e->arg[1], &a2, &l2);
            if (e->fn == B_DECAT)
                gen_bits(e->arg[2], &a3, &l3);
            if (e->fn == B_COPY)
                n = gen_int(e->arg[1]);
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            arg_reg(*ra);
            arg_reg(a);
            arg_reg(l);
            reg_free(a);
            reg_free(l);
            if (a2) {
                arg_reg(a2);
                arg_reg(l2);
                reg_free(a2);
                reg_free(l2);
            }
            if (a3) {
                arg_reg(a3);           /* BIT(3): its address */
                reg_free(a3);
                reg_free(l3);
            }
            if (e->fn == B_COPY) {
                arg_reg(n);
                reg_free(n);
                arg_const(e->t.p);
            }
            call(e->fn == B_AFTER ? "PL1AFTR" : e->fn == B_BEFORE ? "PL1BEFR" :
                 e->fn == B_DECAT ? "PL1DCAT" : e->fn == B_REVERSE ? "PL1REVR" : "PL1COPY");
            *rl = reg_get();
            ins(NULL, "LR", "%d,15", *rl);
            return;
        }
        if (e->fn == B_REPEAT) {
            int a, l, t = tmp(e->t.p, 1), n = 0;
            for (const char *d = e->arg[1]->num; *d; d++)
                n = n * 10 + (*d - '0');
            gen_string(e->arg[0], &a, &l);
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            arg_reg(*ra);
            arg_reg(a);
            arg_reg(l);
            arg_const(n);
            reg_free(a);
            call("PL1REPT");
            *rl = l;
            ins(NULL, "LR", "%d,15", *rl);
            return;
        }
        if (e->fn == B_TRANSLATE) {
            /* the source copied, then translated in place */
            int a, l, t = tmp(e->t.p, 1), r2, l2, r3, l3;
            gen_string(e->arg[0], &a, &l);
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            arg_reg(*ra);
            arg_reg(l);
            arg_reg(a);
            arg_reg(l);
            call("PL1ACHR");
            reg_free(a);
            gen_string(e->arg[1], &r2, &l2);
            if (e->nargs == 3) {
                gen_string(e->arg[2], &r3, &l3);
            } else {
                r3 = reg_get();
                l3 = reg_get();
                ins(NULL, "SR", "%d,%d", r3, r3);
                ins(NULL, "L", "%d,=F'-1'", l3);      /* the whole collating set */
            }
            arg_reg(*ra);
            arg_reg(l);
            arg_reg(r2);
            arg_reg(l2);
            arg_reg(r3);
            arg_reg(l3);
            reg_free(r2); reg_free(l2); reg_free(r3); reg_free(l3);
            call("PL1TRAN");
            *rl = l;
            return;
        }
        if (e->fn == B_CHAR) {
            /* CHAR(x [, n]): x as a string, then n characters */
            int a, l;
            gen_string(e->arg[0], &a, &l);
            if (e->nargs == 2) {
                int t = tmp(e->t.p ? e->t.p : 1, 1);
                *ra = reg_get();
                ins(NULL, "LA", "%d,%d(,13)", *ra, t);
                arg_reg(*ra);
                arg_const(e->t.p);
                arg_reg(a);
                arg_reg(l);
                reg_free(a);
                reg_free(l);
                call("PL1ACHR");
                *rl = reg_get();
                load_const(*rl, e->t.p);
            } else {
                *ra = a;
                *rl = l;
            }
            return;
        }
        if (e->fn == B_DATE || e->fn == B_TIME || e->fn == B_DATETIME) {
            call(e->fn == B_DATE ? "PL1DATE" : e->fn == B_TIME ? "PL1TIME" : "PL1DTTM");
            *ra = reg_get();
            ins(NULL, "LR", "%d,15", *ra);
            *rl = reg_get();
            load_const(*rl, e->t.p);
            return;
        }
        if (e->fn == B_ONFILE || e->fn == B_ONKEY || e->fn == B_ONLOC || e->fn == B_DATAFIELD) {
            call(e->fn == B_ONKEY ? "PL1ONKA" : e->fn == B_ONLOC ? "PL1ONLA" :
                 e->fn == B_DATAFIELD ? "PL1DFLA" : "PL1ONFA");
            *ra = reg_get();
            ins(NULL, "LR", "%d,15", *ra);
            call(e->fn == B_ONKEY ? "PL1ONKL" : e->fn == B_ONLOC ? "PL1ONLL" :
                 e->fn == B_DATAFIELD ? "PL1DFLL" : "PL1ONFL");
            *rl = reg_get();
            ins(NULL, "LR", "%d,15", *rl);
            return;
        }
        if (e->fn == B_ONSOURCE || e->fn == B_ONCHAR) {
            call(e->fn == B_ONCHAR ? "PL1ONCA" : "PL1ONSA");
            *ra = reg_get();
            ins(NULL, "LR", "%d,15", *ra);
            *rl = reg_get();
            if (e->fn == B_ONCHAR) {
                load_const(*rl, 1);
            } else {
                call("PL1ONSL");
                ins(NULL, "LR", "%d,15", *rl);
            }
            return;
        }
        if (e->fn == B_SUBSTR && (cur_mask & EN_STRG)) {
            /* STRINGRANGE: the run time checks and cuts to the string
             * (JOB00210) */
            int ri, rj, t = tmp(8, 4), r;
            gen_string(e->arg[0], ra, rl);
            ri = gen_int(e->arg[1]);
            if (e->nargs == 3) {
                rj = gen_int(e->arg[2]);
            } else {
                rj = reg_get();
                ins(NULL, "SR", "%d,%d", rj, rj);
            }
            r = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", r, t);
            arg_reg(*rl);
            arg_reg(ri);
            arg_reg(rj);
            arg_const(e->nargs == 3);
            arg_reg(r);
            reg_free(ri);
            reg_free(rj);
            reg_free(r);
            call("PL1SBSR");
            ins(NULL, "A", "%d,%d(,13)", *ra, t);
            ins(NULL, "BCTR", "%d,0", *ra);
            ins(NULL, "L", "%d,%d(,13)", *rl, t + 4);
            return;
        }
        if (e->fn == B_SUBSTR) {
            int ri;
            gen_string(e->arg[0], ra, rl);
            ri = gen_int(e->arg[1]);
            ins(NULL, "AR", "%d,%d", *ra, ri);
            ins(NULL, "BCTR", "%d,0", *ra);
            if (e->nargs == 3) {
                int rj = gen_int(e->arg[2]);
                ins(NULL, "LR", "%d,%d", *rl, rj);
                reg_free(rj);
            } else {
                ins(NULL, "SR", "%d,%d", *rl, ri);
                ins(NULL, "LA", "%d,1(,%d)", *rl, *rl);
            }
            reg_free(ri);
            return;
        }
        break;
    default:
        break;
    }
    fatal("%s(%d): this string expression is not supported yet", src_name, e->line);
}

/* ---- bit strings ---------------------------------------------------- */

/* Bit strings of up to 32 bits are held left-aligned in a register, the
 * bits after the length zero; storage is CEIL(n/8) bytes. */
static unsigned int bit_mask(int n)
{
    return n >= 32 ? 0xFFFFFFFFu : ~(0xFFFFFFFFu >> n);
}

static int icm_mask(int n)
{
    static const int m[5] = { 0, 8, 12, 14, 15 };
    return m[(n + 7) / 8];
}

static void gen_jump(struct expr *e, int label, int want);

/* Whether a bit expression is computed in a register: of at most 32 bits,
 * from operands that are too.  Others are computed in storage. */
static int bit_reg_ok(struct expr *e)
{
    if (e->t.k != TY_BIT || e->t.p > 32)
        return 0;
    switch (e->k) {
    case E_BITC:
        return e->len <= 32;
    case E_VAR:
        return e->sym->t.k == TY_BIT && e->sym->t.p <= 32 && !e->sym->star && !e->sym->packed;
    case E_FCALL:
        return 1;
    case E_CALL:
        return e->fn == B_ALLOCATION || e->fn == B_COMPLETION ||
               (e->fn == B_UNSPEC && !(e->arg[0]->k == E_VAR && e->arg[0]->sym->packed));
    case E_NOT:
        return bit_reg_ok(e->l);
    case E_BIN:
        if (e->op == T_AND || e->op == T_OR)
            return bit_reg_ok(e->l) && bit_reg_ok(e->r);
        return e->op != T_CAT;             /* a comparison */
    default:
        return 0;
    }
}

static int bytes_of(int bits)
{
    return bits > 0 ? (bits + 7) / 8 : 1;
}

/* The byte address and the bit offset of a packed bit string, in
 * registers (gen_place). */
static void packed_addr(struct expr *e, int *ra, int *rbit)
{
    struct place p = gen_place(e);
    *ra = p_take(&p);
    ins(NULL, "LA", "%d,%s", *ra, p_rx(&p, 0));
    p_free_except(&p, *ra);
    if (p.bitreg) {
        *rbit = p.bitreg;
    } else {
        *rbit = reg_get();
        load_const(*rbit, p.bitoff);
    }
}

/* A bit string in storage: its address and its length in bits, in
 * registers (rt/pl1rt.c computes the operations). */
static int unspec_tmp(struct expr *x);

static int unspec_direct(struct expr *x)
{
    return x->k == E_VAR || is_string(x->t);
}

static void gen_bits(struct expr *e, int *ra, int *rl)
{
    if (bit_reg_ok(e) && e->k != E_VAR) {     /* a variable: its own storage */
        int r = gen_bitval(e), t = tmp(4, 4);
        ins(NULL, "ST", "%d,%d(,13)", r, t);
        reg_free(r);
        *ra = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", *ra, t);
        *rl = reg_get();
        load_const(*rl, e->t.p);
        return;
    }
    switch (e->k) {
    case E_BITC: {
        char hex[2 * MAXBITS / 8 + 4];
        int n = bytes_of(e->len), k = 0;
        for (int b = 0; b < n; b++) {
            int v = 0;
            for (int i = 0; i < 8; i++)
                if (8 * b + i < e->len && e->str[8 * b + i] == '1')
                    v |= 0x80 >> i;
            k += snprintf(hex + k, sizeof hex - (size_t)k, "%02X", v);
        }
        *ra = reg_get();
        *rl = reg_get();
        ins(NULL, "LA", "%d,=X'%s'", *ra, hex);
        load_const(*rl, e->len);
        return;
    }
    case E_VAR: {
        struct place p;
        if (e->sym->packed) {
            /* packed: its bits copied to an aligned temporary */
            int a, b, t = tmp(bytes_of(e->sym->t.p), 1);
            packed_addr(e, &a, &b);
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            arg_reg(*ra);
            arg_reg(a);
            arg_reg(b);
            arg_const(e->sym->t.p);
            reg_free(a);
            reg_free(b);
            call("PL1BGET");
            *rl = reg_get();
            load_const(*rl, e->sym->t.p);
            return;
        }
        p = gen_place(e);
        *ra = p_take(&p);
        *rl = reg_get();
        if (e->sym->t.k == TY_VBIT) {
            ins(NULL, "LH", "%d,%s", *rl, p_rx(&p, 0));
            ins(NULL, "LA", "%d,%s", *ra, p_rx(&p, 2));
        } else if (e->sym->star) {
            /* BIT(*): the length the argument has */
            reg_free(*rl);
            ins(NULL, "LA", "%d,%s", *ra, p_rx(&p, 0));
            p_free_except(&p, *ra);
            *rl = star_len(e->sym);
            return;
        } else {
            ins(NULL, "LA", "%d,%s", *ra, p_rx(&p, 0));
            load_const(*rl, e->sym->t.p);
        }
        p_free_except(&p, *ra);
        return;
    }
    case E_NOT: {
        int a, l, t = tmp(bytes_of(e->t.p), 1);
        gen_bits(e->l, &a, &l);
        *ra = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", *ra, t);
        arg_reg(*ra);
        arg_reg(a);
        arg_reg(l);
        reg_free(a);
        call("PL1BNOT");
        *rl = l;
        return;
    }
    case E_BIN:
        if (e->op == T_AND || e->op == T_OR || e->op == T_CAT) {
            int a1, l1, a2, l2, t = tmp(bytes_of(e->t.p), 1);
            gen_bits(e->l, &a1, &l1);
            gen_bits(e->r, &a2, &l2);
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            arg_reg(*ra);
            if (e->op != T_CAT)
                arg_const(e->op == T_OR);
            arg_reg(a1); arg_reg(l1); arg_reg(a2); arg_reg(l2);
            call(e->op == T_CAT ? "PL1BCAT" : "PL1BOP");
            reg_free(a1); reg_free(a2); reg_free(l2);
            *rl = l1;
            ins(NULL, "LR", "%d,15", *rl);
            return;
        }
        break;
    case E_CALL:
        if (e->fn == B_AFTER || e->fn == B_BEFORE || e->fn == B_DECAT || e->fn == B_REVERSE ||
            e->fn == B_COPY) {
            /* ANSI's string functions of bit strings (rt/pl1ansi.c,
             * Tier 69): the result in a temporary, its bits back */
            int a, l, a2 = 0, l2 = 0, a3 = 0, l3 = 0, n = 0;
            int t = tmp(bytes_of(e->t.p), 1);
            gen_bits(e->arg[0], &a, &l);
            if (e->fn == B_AFTER || e->fn == B_BEFORE || e->fn == B_DECAT)
                gen_bits(e->arg[1], &a2, &l2);
            if (e->fn == B_DECAT)
                gen_bits(e->arg[2], &a3, &l3);
            if (e->fn == B_COPY)
                n = gen_int(e->arg[1]);
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            arg_reg(*ra);
            arg_reg(a);
            arg_reg(l);
            reg_free(a);
            reg_free(l);
            if (a2) {
                arg_reg(a2);
                arg_reg(l2);
                reg_free(a2);
                reg_free(l2);
            }
            if (a3) {
                arg_reg(a3);
                reg_free(a3);
                reg_free(l3);
            }
            if (e->fn == B_COPY) {
                arg_reg(n);
                reg_free(n);
                arg_const(e->t.p);
            }
            call(e->fn == B_AFTER ? "PL1AFTB" : e->fn == B_BEFORE ? "PL1BEFB" :
                 e->fn == B_DECAT ? "PL1DCAB" : e->fn == B_REVERSE ? "PL1REVB" : "PL1COPB");
            *rl = reg_get();
            ins(NULL, "LR", "%d,15", *rl);
            return;
        }
        if (e->fn == B_STRING) {
            /* STRING(x) of packed bits: where x lies, all its bits */
            struct expr *a = e->arg[0];
            if (a->k != E_VAR || (!a->sym->child && a->nsubs == cum_dims(a->sym))) {
                gen_bits(a, ra, rl);
                return;
            }
            {
                struct place p = gen_place(a);
                *ra = p_take(&p);
                ins(NULL, "LA", "%d,%s", *ra, p_rx(&p, 0));
                p_free_except(&p, *ra);
                *rl = reg_get();
                load_const(*rl, e->t.p);
            }
            return;
        }
        if (e->fn == B_VALID) {
            /* VALID(x): rt/pl1ansi.c writes '1'B or '0'B */
            struct sym *x = e->arg[0]->sym;
            int a, l, p = reg_get(), t = tmp(1, 1);
            gen_string(e->arg[0], &a, &l);
            ins(NULL, "L", "%d,=A(@L%d)", p, str_label(x->t.pic, (int)strlen(x->t.pic)));
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            arg_reg(*ra);
            arg_reg(a);
            arg_reg(l);
            arg_reg(p);
            arg_const((int)strlen(x->t.pic));
            arg_const(x->t.k == TY_PIC);
            reg_free(a);
            reg_free(l);
            reg_free(p);
            call("PL1VALD");
            *rl = reg_get();
            load_const(*rl, 1);
            return;
        }
        if (e->fn == B_BOOL) {
            /* BOOL(x, y, z) bit by bit (rt/pl1rt.c, Q5BYN) */
            int a1, l1, a2, l2, a3, l3, t = tmp(bytes_of(e->t.p), 1);
            gen_bits(e->arg[0], &a1, &l1);
            gen_bits(e->arg[1], &a2, &l2);
            gen_bits(e->arg[2], &a3, &l3);
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            ins(NULL, "SLL", "%d,16", l1);
            ins(NULL, "OR", "%d,%d", l1, l2);         /* both lengths in one word */
            arg_reg(*ra);
            arg_reg(a1); arg_reg(a2); arg_reg(l1); arg_reg(a3); arg_reg(l3);
            reg_free(a1); reg_free(l1); reg_free(a2); reg_free(l2); reg_free(a3); reg_free(l3);
            call("PL1BBOL");
            *rl = reg_get();
            ins(NULL, "LR", "%d,15", *rl);
            return;
        }
        if (e->fn == B_UNSPEC) {
            /* more than 32 bits: the variable's own bytes, or the string's */
            struct expr *x = e->arg[0];
            if (is_bit(x->t) && x->t.k == TY_BIT) {
                gen_bits(x, ra, rl);       /* a bit string: its bits */
                return;
            }
            if (!unspec_direct(x)) {
                int t = unspec_tmp(x);
                *ra = reg_get();
                ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            } else if (x->k == E_VAR) {
                struct place p = gen_place(x);
                *ra = p_take(&p);
                ins(NULL, "LA", "%d,%s", *ra, p_rx(&p, 0));
                p_free_except(&p, *ra);
            } else {
                int l;
                gen_string(x, ra, &l);
                reg_free(l);
            }
            *rl = reg_get();
            load_const(*rl, e->t.p);
            return;
        }
        if (e->fn == B_BIT) {
            /* BIT(x [, n]): the magnitude's binary digits, a string's
             * '1' and '0', then n bits cut or padded (JOB00379) */
            struct expr *x = e->arg[0];
            int a, l, t;
            if (is_bit(x->t)) {
                gen_bits(x, &a, &l);
            } else if (is_string(x->t)) {
                int sa, sl;
                gen_string(x, &sa, &sl);
                t = tmp(bytes_of(x->t.p), 1);
                a = reg_get();
                ins(NULL, "LA", "%d,%d(,13)", a, t);
                arg_reg(a);
                arg_reg(sa);
                arg_reg(sl);
                reg_free(sa);
                call("PL1CTOB");
                l = sl;
            } else {
                int v = gen_int(x), bits = x->t.k == TY_FIXBIN ? x->t.p : (x->t.p * 332 + 99) / 100;
                t = tmp(bytes_of(bits), 1);
                a = reg_get();
                ins(NULL, "LA", "%d,%d(,13)", a, t);
                arg_reg(a);
                arg_reg(v);
                arg_const(bits);
                reg_free(v);
                call("PL1ITOB");
                l = reg_get();
                load_const(l, bits);
            }
            if (e->nargs == 2) {
                int d = tmp(bytes_of(e->t.p), 1);
                *ra = reg_get();
                ins(NULL, "LA", "%d,%d(,13)", *ra, d);
                arg_reg(*ra);
                arg_const(e->t.p);
                arg_const(0);
                arg_reg(a);
                arg_reg(l);
                reg_free(a);
                reg_free(l);
                call("PL1BASG");
                *rl = reg_get();
                load_const(*rl, e->t.p);
            } else {
                *ra = a;
                *rl = l;
            }
            return;
        }
        if (e->fn == B_SUBSTR) {
            struct expr *x = e->arg[0];
            int a, l, i, j, t = tmp(bytes_of(x->t.p), 1);
            gen_bits(x, &a, &l);
            i = gen_int(e->arg[1]);
            if (e->nargs == 3) {
                j = gen_int(e->arg[2]);
            } else {
                j = reg_get();
                load_const(j, -1);
            }
            *ra = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", *ra, t);
            arg_reg(*ra); arg_reg(a); arg_reg(l); arg_reg(i); arg_reg(j);
            call("PL1BSUB");
            reg_free(a); reg_free(i); reg_free(j);
            *rl = l;
            ins(NULL, "LR", "%d,15", *rl);
            return;
        }
        break;
    default:
        break;
    }
    fatal("%s(%d): this bit expression is not supported yet", src_name, e->line);
}

static int gen_bitval(struct expr *e)
{
    int r;

    if (is_bit(e->t) && !bit_reg_ok(e)) {
        /* computed in storage: its first 32 bits, those after its
         * length cleared */
        int a, l, m;
        if (e->t.p > 32)
            fatal("%s(%d): internal: a long bit string in a register", src_name, e->line);
        gen_bits(e, &a, &l);
        r = reg_get();
        m = reg_get();
        ins(NULL, "SR", "%d,%d", r, r);
        ins(NULL, "ICM", "%d,%d,0(%d)", r, icm_mask(e->t.p), a);
        ins(NULL, "L", "%d,=F'-1'", m);
        ins(NULL, "SRL", "%d,0(%d)", m, l);
        ins(NULL, "X", "%d,=F'-1'", m);
        ins(NULL, "NR", "%d,%d", r, m);
        reg_free(m);
        reg_free(a);
        reg_free(l);
        return r;
    }
    switch (e->k) {
    case E_BITC: {
        unsigned int v = 0;
        for (int i = 0; i < e->len; i++)
            if (e->str[i] == '1')
                v |= 0x80000000u >> i;
        r = reg_get();
        if (v == 0)
            ins(NULL, "SR", "%d,%d", r, r);
        else
            ins(NULL, "L", "%d,=X'%08X'", r, v);
        return r;
    }
    case E_VAR: {
        struct place p = gen_place(e);
        const char *op = p_rs(&p, 0);
        r = reg_get();
        ins(NULL, "SR", "%d,%d", r, r);
        ins(NULL, "ICM", "%d,%d,%s", r, icm_mask(e->sym->t.p), op);
        p_free(&p);
        if (e->sym->t.p % 8)
            ins(NULL, "N", "%d,=X'%08X'", r, bit_mask(e->sym->t.p));
        return r;
    }
    case E_FCALL: {
        int res = gen_fcall(e);
        r = reg_get();
        ins(NULL, "SR", "%d,%d", r, r);
        ins(NULL, "ICM", "%d,%d,%d(13)", r, icm_mask(e->t.p), res);
        return r;
    }
    case E_CALL:
        if (e->fn == B_COMPLETION) {
            /* an event's completion: X'80' complete (PL1_EV_COMPL) */
            struct place p = gen_place(e->arg[0]);
            r = reg_get();
            ins(NULL, "SR", "%d,%d", r, r);
            ins(NULL, "ICM", "%d,8,%s", r, p_rs(&p, PL1_EV_COMPL));
            p_free(&p);
            ins(NULL, "N", "%d,=X'80000000'", r);
            return r;
        }
        if (e->fn == B_ALLOCATION) {
            /* '1'B while the CONTROLLED variable has a generation */
            int l = new_label();
            r = reg_get();
            ins(NULL, "L", "%d,=A(@V%d)", r, e->arg[0]->sym->label);
            ins(NULL, "L", "%d,0(,%d)", r, r);
            ins(NULL, "LTR", "%d,%d", r, r);
            ins(NULL, "BC", "8,@L%d", l);
            ins(NULL, "L", "%d,=X'80000000'", r);
            here(l);
            return r;
        }
        if (e->fn == B_UNSPEC) {
            /* the bytes of the value, left-justified */
            struct expr *x = e->arg[0];
            if (is_bit(x->t) && !unspec_direct(x) && bit_reg_ok(x))
                return gen_bitval(x);      /* a bit string's bits, padded */
            if (!unspec_direct(x) && !is_bit(x->t)) {
                int t = unspec_tmp(x);
                r = reg_get();
                ins(NULL, "SR", "%d,%d", r, r);
                ins(NULL, "ICM", "%d,%d,%d(13)", r, icm_mask(e->t.p), t);
                return r;
            }
            r = reg_get();
            ins(NULL, "SR", "%d,%d", r, r);
            if (x->k == E_VAR) {
                struct place p = gen_place(x);
                ins(NULL, "ICM", "%d,%d,%s", r, icm_mask(e->t.p), p_rs(&p, 0));
                p_free(&p);
            } else {
                int a, l;
                gen_string(x, &a, &l);
                ins(NULL, "ICM", "%d,%d,0(%d)", r, icm_mask(e->t.p), a);
                reg_free(a);
                reg_free(l);
            }
            return r;
        }
        break;
    case E_NOT:
        r = gen_bitval(e->l);
        ins(NULL, "X", "%d,=X'%08X'", r, bit_mask(e->t.p));
        return r;
    case E_BIN:
        if (e->op == T_AND || e->op == T_OR) {
            int a = gen_bitval(e->l), b = gen_bitval(e->r);
            ins(NULL, e->op == T_AND ? "NR" : "OR", "%d,%d", a, b);
            reg_free(b);
            return a;
        }
        {
            /* a comparison as a value: '1'B or '0'B */
            int lt = new_label(), le = new_label();
            r = reg_get();
            gen_jump(e, lt, 1);
            ins(NULL, "SR", "%d,%d", r, r);
            ins(NULL, "B", "@L%d", le);
            here(lt);
            ins(NULL, "L", "%d,=X'80000000'", r);
            here(le);
            return r;
        }
    default:
        break;
    }
    fatal("%s(%d): this bit expression is not supported yet", src_name, e->line);
    return 0;
}

/* ---- assignment ----------------------------------------------------- */

/* Where an assignment goes: a reference, a temporary of this frame (a
 * dummy argument), or the result area of this function (RETURN). */
enum tkind2 { TG_REF, TG_TEMP, TG_RESULT };
struct target { enum tkind2 k; struct expr *lhs; int off; };

static struct place target_place(struct target *t)
{
    struct place p;
    if (t->k == TG_REF)
        return gen_place(t->lhs);
    p.ireg = 0;
    p.based = 0;
    p.bitoff = 0;
    p.bitreg = 0;
    if (t->k == TG_TEMP) {
        p.base = 13;
        p.off = t->off;
    } else {
        p.base = reg_get();
        p.off = t->off;
        ins(NULL, "L", "%d,%d(,13)", p.base, ALIST);
        ins(NULL, "L", "%d,4(,%d)", p.base, p.base);
    }
    return p;
}

/* SIZE: a decimal value in temporary v (at the target's scale) beyond p
 * digits raises SIZE; the target then keeps p digits (JOB00211). */
static void size_lim(int v, const char *lim);

static void size_dec(int v, int p)
{
    char lim[24];
    snprintf(lim, sizeof lim, "1%0*d", p, 0);
    size_lim(v, lim);
}

/* ... beyond the limit lim, a decimal constant (exclusive) */
static void size_lim(int v, const char *lim)
{
    int ok = new_label(), bad = new_label();
    if (!(cur_mask & EN_SIZE))
        return;
    need_size = 1;
    ins(NULL, "CP", "%s,=P'%s'", tss(v, 8), lim);
    ins(NULL, "BC", "10,@L%d", bad);
    ins(NULL, "CP", "%s,=P'-%s'", tss(v, 8), lim);
    ins(NULL, "BC", "2,@L%d", ok);
    here(bad);
    ins(NULL, "BAL", "14,@S%d", CURP->id);
    here(ok);
}

/* ... and a binary one in register r beyond p bits: then the low p bits,
 * the sign kept (100000 to FIXED BIN(15) is 1696, JOB00211). */
static void size_bin(int r, int p)
{
    int ok = new_label(), bad = new_label(), pos = new_label();
    if (!(cur_mask & EN_SIZE) || p >= 31)
        return;
    need_size = 1;
    ins(NULL, "C", "%d,=F'%d'", r, (1 << p) - 1);
    ins(NULL, "BC", "2,@L%d", bad);
    ins(NULL, "C", "%d,=F'%d'", r, -((1 << p) - 1));
    ins(NULL, "BC", "10,@L%d", ok);
    here(bad);
    ins(NULL, "BAL", "14,@S%d", CURP->id);
    ins(NULL, "LTR", "%d,%d", r, r);
    ins(NULL, "BC", "10,@L%d", pos);
    ins(NULL, "LCR", "%d,%d", r, r);
    ins(NULL, "N", "%d,=F'%d'", r, (1 << p) - 1);
    ins(NULL, "LCR", "%d,%d", r, r);
    ins(NULL, "B", "@L%d", ok);
    here(pos);
    ins(NULL, "N", "%d,=F'%d'", r, (1 << p) - 1);
    here(ok);
}

/* the target keeps p digits under SIZE: an even p has one more nibble */
static void size_nibble(struct place *p, int prec)
{
    if ((cur_mask & EN_SIZE) && prec % 2 == 0)
        ins(NULL, "NI", "%s,X'0F'", p_rs(p, 0));
}

/* Assign e to the target of type tt, converting as PL/I does. */
static int in_place;            /* the store ends an in-place addition */

/* A label value in a temporary of 8 bytes: A(stub), A(frame). */
static int gen_labelval(struct expr *e)
{
    int t = tmp(8, 4);
    if (e->k == E_LABEL) {
        int f = frame_reg(e->callee), r = reg_get();
        ins(NULL, "L", "%d,=A(@J%d)", r, e->len);
        ins(NULL, "ST", "%d,%d(,13)", r, t);
        ins(NULL, "ST", "%d,%d(,13)", f, t + 4);
        reg_free(r);
        if (f != 13)
            reg_free(f);
    } else {
        struct place p = gen_place(e);
        ins(NULL, "MVC", "%d(8,13),%s", t, p_ss(&p, 0, 8));
        p_free(&p);
    }
    return t;
}

/* The address of a target in a register. */
static int target_addr(struct target *tg)
{
    struct place p = target_place(tg);
    int r = p_take(&p);
    ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
    p_free_except(&p, r);
    return r;
}

/* An entry value in a temporary of the frame, its offset returned:
 * A(the procedure), A(the frame of its parent block) */
static int entry_value(struct expr *e)
{
    int v = tmp(8, 4);
    if (e->k == E_ENTRY) {
        int r = reg_get(), f;
        ins(NULL, "L", "%d,=A(@P%d)", r, e->callee->id);
        ins(NULL, "ST", "%d,%d(,13)", r, v);
        if (!e->callee->parent) {
            /* an external procedure: no parent frame */
            ins(NULL, "XC", "%d(4,13),%d(13)", v + 4, v + 4);
        } else {
            f = e->callee->parent == CURP ? 13 : frame_reg(e->callee->parent);
            ins(NULL, "ST", "%d,%d(,13)", f, v + 4);
            if (f != 13)
                reg_free(f);
        }
        reg_free(r);
    } else {
        struct place q = gen_place(e);
        ins(NULL, "MVC", "%d(8,13),%s", v, p_ss(&q, 0, 8));
        p_free(&q);
    }
    return v;
}

static void gen_store(struct target *tg, struct type tt, struct expr *e);

/* CALL PLISRTx: the argument block of rt/pl1abi.h (PL1_SA_...)
 * in the frame, its address to PL1SORT (rt/pl1sort.c).  The message
 * level and the technique are not passed: the sort of MVS 3.8j takes
 * neither (Tier 107, internals/design/sort-checkpoint.md). */
static void gen_sort(struct expr *e)
{
    int nx = e->fn == B_PLISRTA ? 0 : e->fn == B_PLISRTD ? 2 : 1;
    int blk = tmp(PL1_SA_SIZE, 4), a, l, r;
    struct place p;
    gen_string(e->arg[0], &a, &l);
    ins(NULL, "ST", "%d,%d(,13)", a, blk + PL1_SA_SORT);
    ins(NULL, "ST", "%d,%d(,13)", l, blk + PL1_SA_SORT + 4);
    reg_free(a);
    reg_free(l);
    gen_string(e->arg[1], &a, &l);
    ins(NULL, "ST", "%d,%d(,13)", a, blk + PL1_SA_RECORD);
    ins(NULL, "ST", "%d,%d(,13)", l, blk + PL1_SA_RECORD + 4);
    reg_free(a);
    reg_free(l);
    r = gen_int(e->arg[2]);
    ins(NULL, "ST", "%d,%d(,13)", r, blk + PL1_SA_STORAGE);
    reg_free(r);
    p = gen_place(e->arg[3]);
    r = p_take(&p);
    ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
    p_free_except(&p, r);
    ins(NULL, "ST", "%d,%d(,13)", r, blk + PL1_SA_RETCODE);
    reg_free(r);
    ins(NULL, "XC", "%d(8,13),%d(13)", blk + PL1_SA_E15, blk + PL1_SA_E15);
    for (int i = 0; i < nx; i++) {
        int e35 = e->fn == B_PLISRTC || i == 1;
        struct proc *x = e->arg[4 + i]->callee;
        int v = entry_value(e->arg[4 + i]);
        r = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", r, v);
        ins(NULL, "ST", "%d,%d(,13)", r, blk + (e35 ? PL1_SA_E35 : PL1_SA_E15));
        reg_free(r);
        if (e35)
            /* by a descriptor, with the length a record may have there */
            ins(NULL, "MVC", "%d(4,13),=F'%d'", blk + PL1_SA_E35PAR,
                x->params[0]->star || ext_string_par(x->params[0]) ?
                PL1_SA_BYDESC | x->params[0]->t.p << 8 : 0);
        else
            ins(NULL, "MVC", "%d(4,13),=F'%d'", blk + PL1_SA_E15RET,
                x->ret.p | (x->ret.k == TY_VCHAR ? PL1_SA_VARYING : 0));
    }
    if (e->nargs > 4 + nx) {
        gen_string(e->arg[4 + nx], &a, &l);
        ins(NULL, "ST", "%d,%d(,13)", a, blk + PL1_SA_PREFIX);
        ins(NULL, "ST", "%d,%d(,13)", l, blk + PL1_SA_PREFIX + 4);
        reg_free(a);
        reg_free(l);
    } else {
        ins(NULL, "MVC", "%d(4,13),=F'-1'", blk + PL1_SA_PREFIX + 4);
    }
    r = reg_get();
    ins(NULL, "LA", "%d,%d(,13)", r, blk);
    arg_reg(r);
    reg_free(r);
    call("PL1SORT");
}

static void gen_store(struct target *tg, struct type tt, struct expr *e)
{
    if (e && e->k == E_CALL && e->fn == B_CXPAIR && tg->k != TG_REF) {
        /* a complex dummy or result: the real part, then the imaginary
         * part after it (check.c cx_pair) */
        struct target t2 = *tg;
        struct type pt = part_type(tt);
        gen_store(tg, pt, e->arg[0]);
        t2.off += type_size(pt);
        gen_store(&t2, pt, e->arg[1]);
        return;
    }
    if (e && e->k == E_CALL && (e->fn == B_CXOP || e->fn == B_CXFN || e->fn == B_CXCVT || e->fn == B_CXFC)) {
        gen_cxop(e);
        e = e->l;
    }
    if (tt.k == TY_EVENT) {
        /* an event from an event: its completion and status (t228 Q1EVT) */
        struct place q = gen_place(e), p;
        int v = tmp(PL1_EV_SIZE, 4);
        ins(NULL, "MVC", "%d(%d,13),%s", v, PL1_EV_SIZE, p_ss(&q, 0, PL1_EV_SIZE));
        p_free(&q);
        p = target_place(tg);
        ins(NULL, "MVC", "%s,%d(13)", p_ss(&p, 0, PL1_EV_SIZE), v);
        p_free(&p);
        return;
    }
    if (tt.k == TY_ENTRY) {
        int v = entry_value(e);
        struct place p = target_place(tg);
        ins(NULL, "MVC", "%s,%d(13)", p_ss(&p, 0, 8), v);
        p_free(&p);
        return;
    }
    if (tt.k == TY_FILE) {
        /* a FILE variable: the descriptor's address (Tier 75) */
        int r = file_desc(e);
        struct place p = target_place(tg);
        ins(NULL, "ST", "%d,%s", r, p_rx(&p, 0));
        reg_free(r);
        p_free(&p);
        return;
    }
    if (tt.k == TY_LABEL) {
        int v = gen_labelval(e);
        struct place p = target_place(tg);
        ins(NULL, "MVC", "%s,%d(13)", p_ss(&p, 0, 8), v);
        p_free(&p);
        return;
    }
    if (tt.k == TY_PIC) {
        /* a numeric picture: the value as FIXED DECIMAL(15,q), the low p
         * digits edited (rt/pl1pic.c, JOB00288) */
        int v = tmp(8, 1), rv, rp, rd;
        struct target tm;
        struct type dt;
        memset(&tm, 0, sizeof tm);
        tm.k = TG_TEMP;
        tm.off = v;
        memset(&dt, 0, sizeof dt);
        dt.k = TY_FIXDEC;
        dt.p = 15;
        dt.q = tt.q;
        gen_store(&tm, dt, e);
        if (tt.pic[0] == 'G') {
            /* a sterling picture holds less than 240 * 10**(pounds
             * digits) pence: 240000 into GMZZ9M8M7 is SIZE (Q4STL) */
            char lim[24];
            snprintf(lim, sizeof lim, "240%0*d", tt.p - 3, 0);
            size_lim(v, lim);
        } else {
            size_dec(v, tt.p);
        }
        rd = target_addr(tg);
        rv = reg_get();
        rp = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", rv, v);
        pic_literal(rp, tt.pic);
        arg_reg(rv);
        arg_reg(rp);
        arg_const((int)strlen(tt.pic));
        arg_reg(rd);
        reg_free(rv);
        reg_free(rp);
        reg_free(rd);
        call("PL1PEDT");
        return;
    }
    if (tt.k == TY_CHAR && tt.pic) {
        /* a character-string picture: the source checked character by
         * character, CONVERSION 618 with the whole source as ONSOURCE
         * (JOB00290, JOB00292), then assigned */
        int ra, rl, rd, rp;
        gen_string(e, &ra, &rl);
        rd = target_addr(tg);
        rp = reg_get();
        pic_literal(rp, tt.pic);
        arg_reg(rd);
        arg_reg(rp);
        arg_const((int)strlen(tt.pic));
        arg_reg(ra);
        arg_reg(rl);
        reg_free(rd);
        reg_free(rp);
        reg_free(ra);
        reg_free(rl);
        call("PL1PASG");
        return;
    }
    if (tt.k == TY_CHAR || tt.k == TY_VCHAR) {
        int ra, rl, rd;
        struct place p;
        gen_string(e, &ra, &rl);
        p = target_place(tg);
        rd = p_take(&p);
        ins(NULL, "LA", "%d,%s", rd, p_rx(&p, 0));
        p_free_except(&p, rd);
        if (tg->k == TG_REF && tg->lhs->k == E_VAR && tg->lhs->sym->star) {
            /* a CHAR(*) parameter: its length, or VARYING maximum, comes
             * with the argument */
            int ln = star_len(tg->lhs->sym);
            arg_reg(rd);
            arg_reg(ln);
            reg_free(ln);
        } else if (tg->k == TG_REF && tg->lhs->k == E_VAR && tg->lhs->sym->refer &&
                   tg->lhs->sym->refer_dim < 0) {
            /* CHAR(x REFER(y)): the length y has now */
            int ln = refer_len(tg->lhs->sym, rd);
            arg_reg(rd);
            arg_reg(ln);
            reg_free(ln);
        } else {
            arg_reg(rd);
            arg_const(tt.p);
        }
        arg_reg(ra);
        arg_reg(rl);
        call(tt.k == TY_CHAR ? "PL1ACHR" : "PL1AVCH");
        reg_free(ra);
        reg_free(rl);
        reg_free(rd);
        return;
    }
    if (tt.k == TY_BIT && tg->k == TG_REF && tg->lhs->k == E_VAR && tg->lhs->sym->packed) {
        /* a packed target: its bits only, those around kept */
        int a, l, d, b;
        gen_bits(e, &a, &l);
        packed_addr(tg->lhs, &d, &b);
        arg_reg(d);
        arg_reg(b);
        arg_const(tt.p);
        arg_reg(a);
        arg_reg(l);
        reg_free(d);
        reg_free(b);
        reg_free(a);
        reg_free(l);
        call("PL1BPUT");
        return;
    }
    if (tt.k == TY_VBIT || (tt.k == TY_BIT && (tt.p > 32 || !bit_reg_ok(e) ||
                                                (tg->k == TG_REF && tg->lhs->k == E_VAR &&
                                                 tg->lhs->sym->star)))) {
        /* in storage: padded or cut, a VARYING target takes the length;
         * a BIT(*) parameter has the length of its argument */
        int a, l, d, sl = 0;
        gen_bits(e, &a, &l);
        d = target_addr(tg);
        if (tg->k == TG_REF && tg->lhs->k == E_VAR && tg->lhs->sym->star)
            sl = star_len(tg->lhs->sym);
        arg_reg(d);
        if (sl) {
            arg_reg(sl);
            reg_free(sl);
        } else {
            arg_const(tt.p);
        }
        arg_const(tt.k == TY_VBIT);
        arg_reg(a);
        arg_reg(l);
        reg_free(d);
        reg_free(a);
        reg_free(l);
        call("PL1BASG");
        return;
    }
    if (tt.k == TY_BIT) {
        int r;
        struct place p;
        if (e->t.p > tt.p && uses_strz)
            call("PL1SZCK");                /* STRINGSIZE: cut in the register */
        r = gen_bitval(e);
        p = target_place(tg);
        if (tt.p % 8 || e->t.p > tt.p)
            ins(NULL, "N", "%d,=X'%08X'", r, bit_mask(tt.p));
        if (tt.p % 8 && tg->k == TG_REF && tg->lhs->sym && tg->lhs->sym->overlay) {
            /* DEFINED on a longer string: the base's bits after it stay */
            int o = reg_get();
            ins(NULL, "SR", "%d,%d", o, o);
            ins(NULL, "ICM", "%d,%d,%s", o, icm_mask(tt.p), p_rs(&p, 0));
            ins(NULL, "N", "%d,=X'%08X'", o, ~bit_mask(tt.p) & bit_mask((tt.p + 7) / 8 * 8));
            ins(NULL, "OR", "%d,%d", r, o);
            reg_free(o);
        }
        ins(NULL, "STCM", "%d,%d,%s", r, icm_mask(tt.p), p_rs(&p, 0));
        p_free(&p);
        reg_free(r);
        return;
    }
    if (tt.k == TY_AREA) {
        struct place p = target_place(tg);
        int t = p_take(&p);
        ins(NULL, "LA", "%d,%s", t, p_rx(&p, 0));
        p_free_except(&p, t);
        if (e->k == E_CALL && e->fn == B_EMPTY) {
            /* EMPTY: no extent, no free list; the size stays */
            ins(NULL, "MVI", "0(%d),0", t);
            ins(NULL, "XC", "4(12,%d),4(%d)", t, t);
            reg_free(t);
        } else {
            int a = area_addr(e);
            arg_reg(t);
            arg_reg(a);
            reg_free(t);
            reg_free(a);
            call("PL1AASG");
        }
        return;
    }
    if (tt.k == TY_PTR || tt.k == TY_OFFSET) {
        int r = gen_ptr(e);
        struct place p = target_place(tg);
        ins(NULL, "ST", "%d,%s", r, p_rx(&p, 0));
        p_free(&p);
        reg_free(r);
        return;
    }
    if (is_float(tt)) {
        int v = gen_flt(e, tt);
        struct place p = target_place(tg);
        ins(NULL, "MVC", "%s,%d(13)", p_ss(&p, 0, flt_long(tt) ? 8 : 4), v);
        p_free(&p);
        return;
    }
    if (is_float(e->t) && tt.k == TY_FIXDEC) {
        /* truncated to the target's scale (JOB00156: 2.5/3 is 0.83) */
        int v = flt_to_dec(gen_flt(e, e->t), flt_long(e->t), tt.q);
        struct place p;
        size_dec(v, tt.p);
        p = target_place(tg);
        ins(NULL, "ZAP", "%s,%s", p_ss(&p, 0, dec_bytes(tt.p)), tss(v, 8));
        size_nibble(&p, tt.p);
        p_free(&p);
        return;
    }
    if (is_float(e->t) && tt.k == TY_FIXBIN) {
        int t = gen_flt(e, e->t), r = reg_get();
        struct place p;
        ins(NULL, "LA", "%d,%d(,13)", r, t);
        arg_reg(r);
        arg_const(flt_long(e->t));
        arg_const(tt.q);
        call("PL1FTOB");
        ins(NULL, "LR", "%d,15", r);
        size_bin(r, tt.p);
        p = target_place(tg);
        ins(NULL, tt.p <= 15 ? "STH" : "ST", "%d,%s", r, p_rx(&p, 0));
        p_free(&p);
        reg_free(r);
        return;
    }
    if (tt.k == TY_FIXBIN) {
        int dec = e->t.k == TY_FIXDEC;
        int r = dec ? gen_bin_at(e, tt.q) : gen_bin(e);
        struct place p;
        bin_rescale(r, dec ? tt.q : bin_scale(e->t), tt.q);
        size_bin(r, tt.p);
        p = target_place(tg);
        ins(NULL, tt.p <= 15 ? "STH" : "ST", "%d,%s", r, p_rx(&p, 0));
        p_free(&p);
        reg_free(r);
        return;
    }
    /* FIXED DECIMAL target */
    {
        int v = gen_dec(e);
        struct place p;
        rescale(v, dec_scale(e->t), tt.q);
        size_dec(v, tt.p);
        p = target_place(tg);
        ins(NULL, "ZAP", "%s,%s", p_ss(&p, 0, dec_bytes(tt.p)), tss(v, 8));
        if (in_place)
            check_fofl();                 /* the CC of the ZAP */
        size_nibble(&p, tt.p);
        p_free(&p);
    }
}

/* l converted to the type of the E_CAST e, as an assignment converts,
 * in a temporary; returns its offset. */
static int gen_cast(struct expr *e)
{
    int n = type_size(e->t);
    int t = tmp(n < 8 ? 8 : n, 8);
    struct target tg = { TG_TEMP, NULL, t };
    gen_store(&tg, e->t, e->l);
    return t;
}

/* UNSPEC of an expression that is neither a variable nor a string
 * (UNSPEC(ADDR(P)), UNSPEC(X + 1)): the value assigned to a temporary
 * of its own type, whose bytes UNSPEC then takes; returns its offset */
static int unspec_tmp(struct expr *x)
{
    int n = type_size(x->t);
    int t = tmp(n < 8 ? 8 : n, 8);
    struct target tg = { TG_TEMP, NULL, t };
    gen_store(&tg, x->t, x);
    return t;
}

/* PL/I (F) computes X = X + Y, X = Y + X and X = X - Y in the field of X
 * when X is FIXED DECIMAL and Y decimal of the same scale, so a sum that
 * does not fit the bytes of X raises FIXEDOVERFLOW there -- elsewhere the
 * assignment truncates and raises nothing (JOB00140, JOB00149). */
static int same_scalar(struct expr *x, struct expr *lhs)
{
    return x->k == E_VAR && x->sym == lhs->sym && x->nsubs == 0;
}

/* as measured: constants and scalar variables */
static int simple_operand(struct expr *x)
{
    return x->k == E_NUM || (x->k == E_VAR && x->nsubs == 0);
}

static int is_in_place(struct expr *lhs, struct expr *e)
{
    struct type t = lhs->sym->t;
    if (t.k != TY_FIXDEC || lhs->nsubs || e->k != E_BIN ||
        (e->op != T_PLUS && e->op != T_MINUS) ||
        e->l->t.k != TY_FIXDEC || e->r->t.k != TY_FIXDEC ||
        e->l->t.q != t.q || e->r->t.q != t.q ||
        !simple_operand(e->l) || !simple_operand(e->r))
        return 0;
    return same_scalar(e->l, lhs) || (e->op == T_PLUS && same_scalar(e->r, lhs));
}

static void gen_assign(struct expr *lhs, struct expr *e)
{
    struct target t = { TG_REF, lhs, 0 };
    if (lhs->k == E_CALL && (lhs->fn == B_COMPLETION || lhs->fn == B_STATUS)) {
        /* COMPLETION(e) = bit, STATUS(e) = number (#3) */
        int r = lhs->fn == B_STATUS ? gen_bin(e) : gen_bitval(e);
        struct place p = gen_place(lhs->arg[0]);
        if (lhs->fn == B_STATUS)
            ins(NULL, "ST", "%d,%s", r, p_rx(&p, PL1_EV_STATUS));
        else
            ins(NULL, "STCM", "%d,8,%s", r, p_rs(&p, PL1_EV_COMPL));
        p_free(&p);
        reg_free(r);
        return;
    }
    if (lhs->k == E_CALL && lhs->fn == B_SUBSTR) {
        /* SUBSTR as a pseudovariable (JOB00379): that part of the string
         * takes the value, padded or cut to its length */
        if (is_bit(lhs->arg[0]->t)) {
            int sa, sl, ba, bl, i, j;
            gen_bits(e, &sa, &sl);
            gen_bits(lhs->arg[0], &ba, &bl);
            i = gen_int(lhs->arg[1]);
            if (lhs->nargs == 3) {
                j = gen_int(lhs->arg[2]);
            } else {
                j = reg_get();
                load_const(j, -1);
            }
            arg_reg(ba); arg_reg(bl); arg_reg(i); arg_reg(j); arg_reg(sa); arg_reg(sl);
            reg_free(ba); reg_free(bl); reg_free(i); reg_free(j); reg_free(sa); reg_free(sl);
            call("PL1BSET");
        } else {
            int sa, sl, da, dl;
            gen_string(e, &sa, &sl);
            gen_string(lhs, &da, &dl);
            arg_reg(da); arg_reg(dl); arg_reg(sa); arg_reg(sl);
            reg_free(da); reg_free(dl); reg_free(sa); reg_free(sl);
            call("PL1ACHR");
        }
        return;
    }
    if (lhs->k == E_VAR && is_in_place(lhs, e)) {
        in_place = 1;
        gen_store(&t, lhs->sym->t, e);
        in_place = 0;
        return;
    }
    if (lhs->k == E_CALL && (lhs->fn == B_STRING || lhs->fn == B_UNSPEC) &&
        (lhs->fn == B_UNSPEC || is_bit(lhs->t))) {
        /* UNSPEC(x) = b, STRING(x) = b of bits: the bits from x's first
         * byte on, padded or cut (Q2UNS) */
        struct expr *x = lhs->arg[0];
        int n = lhs->fn == B_STRING ? lhs->t.p : is_bit(x->t) ? x->t.p : 8 * type_size(x->t);
        int a, l, d, b = reg_get();
        struct place p;
        gen_bits(e, &a, &l);
        p = gen_place(x);
        d = p_take(&p);
        ins(NULL, "LA", "%d,%s", d, p_rx(&p, 0));
        p_free_except(&p, d);
        ins(NULL, "SR", "%d,%d", b, b);
        arg_reg(d);
        arg_reg(b);
        arg_const(n);
        arg_reg(a);
        arg_reg(l);
        reg_free(d);
        reg_free(b);
        reg_free(a);
        reg_free(l);
        call("PL1BPUT");
        return;
    }
    if (lhs->k == E_CALL && lhs->fn == B_STRING) {
        /* STRING(x) = s: x's characters take s, padded or cut (Q1STR) */
        int sa, sl, da, dl;
        gen_string(e, &sa, &sl);
        gen_string(lhs, &da, &dl);
        arg_reg(da); arg_reg(dl); arg_reg(sa); arg_reg(sl);
        reg_free(da); reg_free(dl); reg_free(sa); reg_free(sl);
        call("PL1ACHR");
        return;
    }
    if (lhs->k == E_CALL && lhs->fn == B_ENTRYADDR) {
        /* ENTRYADDR(v) = p: the entry variable invokes the code at p, with
         * no parent frame, as an external procedure has none */
        int r = gen_ptr(e);
        struct place p = gen_place(lhs->arg[0]);
        ins(NULL, "ST", "%d,%s", r, p_rx(&p, 0));
        ins(NULL, "XC", "%s,%s", p_ss(&p, 4, 4), p_ss(&p, 4, 4));
        p_free(&p);
        reg_free(r);
        return;
    }
    if (lhs->k == E_CALL && lhs->fn == B_PAGENO) {
        /* PAGENO(f) = n (ANSI 7.5.4.4): the page number from now on */
        int d = file_desc(lhs->arg[0]), n = gen_int(e);
        arg_reg(d);
        arg_reg(n);
        reg_free(d);
        reg_free(n);
        call("PL1SPGN");
        return;
    }
    if (lhs->k == E_CALL) {
        /* ONSOURCE = ..., ONCHAR = ...: the field converted changes */
        int ra, rl;
        gen_string(e, &ra, &rl);
        arg_reg(ra);
        arg_reg(rl);
        reg_free(ra);
        reg_free(rl);
        call(lhs->fn == B_ONCHAR ? "PL1SONC" : "PL1SONS");
        return;
    }
    gen_store(&t, lhs->sym->t, e);
}

/* A reference to a simple variable (a loop's control variable). */
static struct expr *simple_ref(struct sym *s)
{
    struct expr *e = xmalloc(sizeof *e);
    memset(e, 0, sizeof *e);
    e->k = E_VAR;
    e->sym = s;
    e->t = s->t;
    return e;
}

static int type_size(struct type t)
{
    switch (t.k) {
    case TY_FIXBIN: return t.p <= 15 ? 2 : 4;
    case TY_FIXDEC: return dec_bytes(t.p);
    case TY_CHAR:   return t.p;
    case TY_VCHAR:  return 2 + t.p;
    case TY_BIT:    return (t.p + 7) / 8;
    case TY_VBIT:   return 2 + (t.p + 7) / 8;
    case TY_FLTDEC:
    case TY_FLTBIN: return flt_long(t) ? 8 : 4;
    case TY_PTR:    return 4;
    case TY_PIC:    return pic_length(t);
    case TY_LABEL:  return 8;
    case TY_ENTRY:  return 8;
    case TY_FILE:   return 4;
    case TY_AREA:   return 16 + t.p;
    case TY_CFIXBIN: case TY_CFIXDEC: case TY_CFLTDEC: case TY_CFLTBIN:
        return 2 * type_size(part_type(t));
    case TY_EVENT:  return PL1_EV_SIZE;
    default:        return 4;
    }
}

/* A call of an internal procedure.  Returns the frame offset of the
 * result area (a function), else -1. */
/* A CALL of an OPTIONS(ASSEMBLER) entry (SC33-0006-7, chapter 11): R1 at
 * a list of addresses of the data themselves -- of a VARYING string its
 * length prefix, of an array its first element -- the last with its
 * high-order bit on; no words for the run time, no descriptors.  With
 * RETCODE, R15 on return is the PL/I return code (PLIRETV). */
/* The name of a fetched entry as LOAD takes it, CL8, in the literals */
static int fetch_name(struct proc *p)
{
    char n[9];
    snprintf(n, sizeof n, "%-8.8s", p->extname ? p->extname : ext_name(p->name));
    return str_label(n, 8);
}

static void gen_asmcall(struct expr *e)
{
    stk_unknown = 1;          /* an assembler or C routine: its own frames */
    struct proc *p = e->callee;
    int alist = tmp(4 * (e->nargs ? e->nargs : 1), 4), r, epa = -1;

    if (p->fetched) {
        /* named in a FETCH or RELEASE: its entry point from the run time,
         * which LOADs it if it is not loaded (rt/pl1fetch.asm) */
        epa = tmp(4, 4);
        r = reg_get();
        ins(NULL, "LA", "%d,@L%d", r, fetch_name(p));
        arg_reg(r);
        reg_free(r);
        call("PL1FTCH");
        ins(NULL, "ST", "15,%d(,13)", epa);
    }

    for (int i = 0; i < e->nargs; i++) {
        struct expr *a = e->arg[i];
        if (e->byref[i] == 6) {
            /* omitted (OPTIONAL): a word of zeros, its high-order bit on
             * when it is the last (SC26-3114-01) */
            if (i == e->nargs - 1)
                ins(NULL, "MVC", "%d(4,13),=X'80000000'", alist + 4 * i);
            else
                ins(NULL, "XC", "%d(4,13),%d(13)", alist + 4 * i, alist + 4 * i);
            continue;
        } else if (e->byref[i] == 5) {
            /* BYVALUE: the value itself */
            r = e->argt[i].k == TY_PTR ? gen_ptr(a) : gen_int(a);
        } else if (e->byref[i]) {
            struct place pl = gen_place(a);
            r = p_take(&pl);
            ins(NULL, "LA", "%d,%s", r, p_rx(&pl, 0));
            p_free_except(&pl, r);
        } else {
            struct target t;
            int d = tmp(type_size(e->argt[i]) < 8 ? 8 : type_size(e->argt[i]), 8);
            t.k = TG_TEMP;
            t.lhs = NULL;
            t.off = d;
            gen_store(&t, e->argt[i], a);
            r = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", r, d);
        }
        ins(NULL, "ST", "%d,%d(,13)", r, alist + 4 * i);
        reg_free(r);
    }
    if (e->nargs) {
        /* the last word's high-order bit: not in a list of values (BYVALUE),
         * and an omitted last argument has it already */
        if (!p->byvalue && e->byref[e->nargs - 1] != 6)
            ins(NULL, "OI", "%d(13),X'80'", alist + 4 * (e->nargs - 1));
        ins(NULL, "LA", "1,%d(,13)", alist);
    } else {
        ins(NULL, "SR", "1,1");
    }
    if (p->fetched)
        ins(NULL, "L", "15,%d(,13)", epa);
    else
        ins(NULL, "L", "15,=V(%s)", p->extname ? p->extname : ext_name(p->name));
    ins(NULL, "BALR", "14,15");
    if (p->retcode) {
        ins(NULL, "L", "14,=V(PL1RTCD)");
        ins(NULL, "ST", "15,0(,14)");
    }
}

static int gen_fcall(struct expr *e)
{
    struct proc *p = e->callee;
    int n = 2 + e->nargs, alist = tmp(4 * n, 4), res = -1, r, ev = -1;
    struct type ret;
    int has_ret;
    int backt[MAXCALLARGS];        /* byref 4: the copy of a packed argument */

    if (!p) {
        /* through an ENTRY parameter: its value has the procedure and
         * the frame for word 0 */
        struct expr ref;
        struct place q;
        memset(&ref, 0, sizeof ref);
        ref.k = E_VAR;
        ref.sym = e->entry;
        ref.t = e->entry->t;
        ref.line = e->line;
        ev = tmp(8, 4);
        q = gen_place(&ref);
        ins(NULL, "MVC", "%d(8,13),%s", ev, p_ss(&q, 0, 8));
        p_free(&q);
        ins(NULL, "MVC", "%d(4,13),%d(13)", alist, ev + 4);
        has_ret = e->entry->has_ret;
        ret = e->entry->ret;
    } else if (e->entryno && p->entries[e->entryno - 1].has_ret) {
        has_ret = 1;
        ret = p->entries[e->entryno - 1].ret;
    } else {
        has_ret = p->has_ret;
        ret = p->ret;
    }
    /* word 0: the frame of the callee's parent block; an external
     * procedure has none */
    if (!p) {
        /* set above */
    } else if (!p->parent) {
        ins(NULL, "XC", "%d(4,13),%d(13)", alist, alist);
    } else if (p->parent == CURP) {
        ins(NULL, "ST", "13,%d(,13)", alist);
    } else {
        r = frame_reg(p->parent);
        ins(NULL, "ST", "%d,%d(,13)", r, alist);
        reg_free(r);
    }
    /* word 1: the result area */
    if (has_ret) {
        res = tmp(type_size(ret) < 8 ? 8 : type_size(ret), 8);
        r = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", r, res);
        ins(NULL, "ST", "%d,%d(,13)", r, alist + 4);
        reg_free(r);
    } else {
        ins(NULL, "XC", "%d(4,13),%d(13)", alist + 4, alist + 4);
    }
    /* the arguments: an address each, of the argument or of its dummy */
    for (int i = 0; i < e->nargs; i++) {
        struct expr *a = e->arg[i];
        if (a->t.k == TY_FILE) {
            r = file_desc(a);               /* a file: its descriptor */
        } else if (e->byref[i] == 2) {
            /* a CHAR(*) parameter: a descriptor, A(the data) and its
             * length -- of a VARYING one A(the variable) and its maximum */
            int dsc = tmp(8, 4), ra, rl;
            struct sym *par = callee_par(e, i);
            if (par && par->t.k == TY_BIT) {
                gen_bits(a, &ra, &rl);         /* BIT(*): A(the bits), the length */
            } else if (a->t.k == TY_VCHAR && callee_par_varying(e, i)) {
                struct place pl = gen_place(a);
                ra = p_take(&pl);
                ins(NULL, "LA", "%d,%s", ra, p_rx(&pl, 0));
                p_free_except(&pl, ra);
                if (a->sym->star) {
                    rl = star_len(a->sym);     /* passed on: its maximum */
                } else {
                    rl = reg_get();
                    load_const(rl, a->sym->t.p);
                }
            } else {
                gen_string(a, &ra, &rl);
            }
            ins(NULL, "ST", "%d,%d(,13)", ra, dsc);
            ins(NULL, "ST", "%d,%d(,13)", rl, dsc + 4);
            reg_free(ra);
            reg_free(rl);
            r = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", r, dsc);
        } else if (e->byref[i] == 4) {
            /* packed bits: a copy, written back after the call */
            int ra, rb, t = tmp(bytes_of(a->sym->t.p) < 4 ? 4 : bytes_of(a->sym->t.p), 4);
            packed_addr(a, &ra, &rb);
            r = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", r, t);
            arg_reg(r);
            arg_reg(ra);
            arg_reg(rb);
            arg_const(a->sym->t.p);
            reg_free(ra);
            reg_free(rb);
            reg_free(r);
            call("PL1BGET");
            backt[i] = t;
            r = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", r, t);
        } else if (e->byref[i] == 3) {
            /* an A(*) or CHAR(*) array parameter: a descriptor, A(the
             * array), F(the element length), F(lb), F(hb) per dimension;
             * a parameter of the same kind passes its own on */
            struct sym *as = a->sym;
            if ((as->param || as->adjust) && (as->stardims || (as->star && as->ndims))) {
                r = star_desc(as);
            } else {
                int dsc = tmp(8 + 8 * as->ndims, 4), ra;
                struct place pl = gen_place(a);
                ra = p_take(&pl);
                ins(NULL, "LA", "%d,%s", ra, p_rx(&pl, 0));
                p_free_except(&pl, ra);
                ins(NULL, "ST", "%d,%d(,13)", ra, dsc);
                load_const(ra, as->t.k == TY_CHAR || as->t.k == TY_VCHAR ? as->t.p : as->size);
                ins(NULL, "ST", "%d,%d(,13)", ra, dsc + 4);
                for (int d = 0; d < as->ndims; d++) {
                    load_const(ra, as->lb[d]);
                    ins(NULL, "ST", "%d,%d(,13)", ra, dsc + 8 + 8 * d);
                    load_const(ra, as->hb[d]);
                    ins(NULL, "ST", "%d,%d(,13)", ra, dsc + 12 + 8 * d);
                }
                reg_free(ra);
                r = reg_get();
                ins(NULL, "LA", "%d,%d(,13)", r, dsc);
            }
        } else if (e->byref[i]) {
            struct place pl = gen_place(a);
            r = p_take(&pl);
            ins(NULL, "LA", "%d,%s", r, p_rx(&pl, 0));
            p_free_except(&pl, r);
        } else {
            struct target t;
            int d = tmp(type_size(e->argt[i]) < 8 ? 8 : type_size(e->argt[i]), 8);
            t.k = TG_TEMP;
            t.lhs = NULL;
            t.off = d;
            gen_store(&t, e->argt[i], a);
            r = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", r, d);
        }
        if (e->byref[i] != 2 && e->byref[i] != 3 && a->t.k != TY_FILE &&
            (ext_string_par(callee_par(e, i)) || ext_string_arg(e, i))) {
            /* the argument or its dummy, in a descriptor (ext_string_par) */
            int dsc = tmp(8, 4), rl = reg_get();
            struct sym *par = callee_par(e, i);
            ins(NULL, "ST", "%d,%d(,13)", r, dsc);
            load_const(rl, ext_string_par(par) ? par->t.p : e->argt[i].p);
            ins(NULL, "ST", "%d,%d(,13)", rl, dsc + 4);
            reg_free(rl);
            ins(NULL, "LA", "%d,%d(,13)", r, dsc);
        }
        ins(NULL, "ST", "%d,%d(,13)", r, alist + 8 + 4 * i);
        reg_free(r);
    }
    ins(NULL, "LA", "1,%d(,13)", alist);
    if (!p || p->nentries)
        ins(NULL, "LA", "0,%d", p ? e->entryno : 0);   /* the entry point (gen_proc) */
    if (!p || p->is_stub)
        stk_unknown = 1;      /* compiled apart, or an ENTRY variable */
    if (p && p->is_stub)
        ins(NULL, "L", "15,=V(%s)", ext_name(p->name));   /* compiled apart */
    else if (p)
        ins(NULL, "L", "15,=A(@P%d)", p->id);
    else
        ins(NULL, "L", "15,%d(,13)", ev);
    ins(NULL, "BALR", "14,15");
    for (int i = 0; i < e->nargs; i++) {
        if (e->byref[i] == 4) {
            int ra, rb, r2 = reg_get();
            packed_addr(e->arg[i], &ra, &rb);
            ins(NULL, "LA", "%d,%d(,13)", r2, backt[i]);
            arg_reg(ra);
            arg_reg(rb);
            arg_const(e->arg[i]->sym->t.p);
            arg_reg(r2);
            arg_const(e->arg[i]->sym->t.p);
            reg_free(ra);
            reg_free(rb);
            reg_free(r2);
            call("PL1BPUT");
        }
    }
    return res;
}

/* ---- conditions ----------------------------------------------------- */

static int is_cmp(int op)
{
    return op == T_EQ || op == T_NE || op == T_LT || op == T_LE || op == T_GT || op == T_GE;
}

/* Branch mask that is taken when "a op b" holds after a compare of a, b. */
static int true_mask(int op)
{
    switch (op) {
    case T_EQ: return 8;
    case T_NE: return 6;
    case T_LT: return 4;
    case T_GT: return 2;
    case T_LE: return 12;
    case T_GE: return 10;
    }
    return 0;
}

static int bit1(struct expr *e) { return e->t.k == TY_BIT && e->t.p == 1; }

/* NULL: PL/I (F)'s value for no address. */
#define NULL_PTR "=X'FF000000'"

/* A pointer value in a register. */
/* The length of a CHAR(*) parameter (its VARYING maximum): from the
 * descriptor its argument list points to (gen_fcall). */
static int star_len(struct sym *s)
{
    int fb = frame_reg(s->owner), r = reg_get();
    ins(NULL, "L", "%d,%d(,%d)", r, ALIST, fb);
    ins(NULL, "L", "%d,%d(,%d)", r, 8 + 4 * (s->param - 1), r);
    ins(NULL, "L", "%d,4(,%d)", r, r);
    if (fb != 13)
        reg_free(fb);
    return r;
}

/* ---- REFER ------------------------------------------------------------ */

/* The offset of member s from the start of its structure. */
static int root_off(struct sym *s)
{
    int o = 0;
    for (; s && s->parent; s = s->parent)
        o += s->moff;
    return o;
}

/* The member of structure x whose bound or length REFER gives. */
static struct sym *refer_of(struct sym *x)
{
    for (struct sym *c = x->child; c; c = c->sibling) {
        struct sym *r = c->refer ? c : refer_of(c);
        if (r)
            return r;
    }
    return NULL;
}

/* The length of a CHAR(x REFER(y)) string at the address in ra: y, in
 * the same allocation (Q1REF). */
static int refer_len(struct sym *s, int ra)
{
    int r = reg_get(), d = root_off(s) - root_off(s->refer);
    ins(NULL, "LR", "%d,%d", r, ra);
    if (d)
        ins(NULL, "S", "%d,=F'%d'", r, d);
    ins(NULL, s->refer->t.p <= 15 ? "LH" : "L", "%d,0(,%d)", r, r);
    return r;
}

/* The descriptor of a CHAR(*), BIT(*) or A(*) parameter, in a register. */
static int star_desc(struct sym *s)
{
    int fb = frame_reg(s->owner), r = reg_get();
    if (s->adjust) {
        /* an adjustable array: the descriptor in its block's frame */
        ins(NULL, "LA", "%d,%d(,%d)", r, s->off, fb);
        if (fb != 13)
            reg_free(fb);
        return r;
    }
    ins(NULL, "L", "%d,%d(,%d)", r, ALIST, fb);
    ins(NULL, "L", "%d,%d(,%d)", r, 8 + 4 * (s->param - 1), r);
    if (fb != 13)
        reg_free(fb);
    return r;
}

/* The subscripts of an element of an A(*) or CHAR(*) array parameter,
 * e->sub[k...], into p->ireg: the bounds and the element length come
 * from its descriptor (Horner over the extents). */
static void star_subscripts(struct expr *e, struct sym *x, int k, struct place *p)
{
    int dr = star_desc(x), r = 0;
    for (int d = 0; d < x->ndims; d++) {
        int s = gen_int(e->sub[k + d]), lo = 8 + 8 * d;
        if (cur_mask & EN_SUBRG) {
            int ok = new_label(), bad = new_label();
            need_subrg = 1;
            ins(NULL, "C", "%d,%d(,%d)", s, lo, dr);
            ins(NULL, "BC", "4,@L%d", bad);
            ins(NULL, "C", "%d,%d(,%d)", s, lo + 4, dr);
            ins(NULL, "BC", "12,@L%d", ok);
            here(bad);
            ins(NULL, "BAL", "14,@B%d", CURP->id);
            here(ok);
        }
        ins(NULL, "S", "%d,%d(,%d)", s, lo, dr);
        if (d == 0) {
            r = s;
            continue;
        }
        {
            int x2 = reg_get();
            ins(NULL, "L", "%d,%d(,%d)", x2, lo + 4, dr);
            ins(NULL, "S", "%d,%d(,%d)", x2, lo, dr);
            ins(NULL, "LA", "%d,1(,%d)", x2, x2);
            ins(NULL, "LR", "1,%d", r);
            ins(NULL, "MR", "0,%d", x2);
            ins(NULL, "LR", "%d,1", r);
            ins(NULL, "AR", "%d,%d", r, s);
            reg_free(x2);
            reg_free(s);
        }
    }
    if (x->star) {
        /* CHAR(*) elements: the length the argument has (VARYING: its
         * maximum and the halfword in front) */
        int el = reg_get();
        ins(NULL, "L", "%d,4(,%d)", el, dr);
        if (x->t.k == TY_VCHAR)
            ins(NULL, "LA", "%d,2(,%d)", el, el);
        ins(NULL, "LR", "1,%d", r);
        ins(NULL, "MR", "0,%d", el);
        ins(NULL, "LR", "%d,1", r);
        reg_free(el);
    } else if (x->size != 1) {
        if (x->size <= 32767) {
            ins(NULL, "MH", "%d,=H'%d'", r, x->size);
        } else {
            ins(NULL, "LR", "1,%d", r);
            ins(NULL, "M", "0,=F'%d'", x->size);
            ins(NULL, "LR", "%d,1", r);
        }
    }
    reg_free(dr);
    if (p->ireg) {
        ins(NULL, "AR", "%d,%d", p->ireg, r);
        reg_free(r);
    } else {
        p->ireg = r;
    }
}

/* the parameter argument i of the call e goes to, if the callee is known */
static struct sym *callee_par(struct expr *e, int i)
{
    struct proc *p = e->callee;
    if (!p)
        return NULL;
    if (e->entryno) {
        struct pentry *pe = &p->entries[e->entryno - 1];
        return i < pe->nparams ? p->params[pe->canon[i]] : NULL;
    }
    return i < p->nparams ? p->params[i] : NULL;
}

/* whether argument i of the call e goes to a CHAR(*) VARYING parameter */
static int callee_par_varying(struct expr *e, int i)
{
    struct proc *p = e->callee;
    struct sym *par;
    if (!p)
        return 0;
    if (e->entryno) {
        struct pentry *pe = &p->entries[e->entryno - 1];
        par = i < pe->nparams ? p->params[pe->canon[i]] : NULL;
    } else {
        par = i < p->nparams ? p->params[i] : NULL;
    }
    return par && par->t.k == TY_VCHAR;
}

/* The areas in a variable: each element of each AREA in it, as an
 * offset from the start of the variable's storage and a size (Q1AREA2:
 * a member, an array element, all initialized). */
#define MAXAREAS 256
static int area_off[MAXAREAS], area_size[MAXAREAS], nareas;

static int dims_count(struct sym *x)
{
    int n = 1;
    for (int d = 0; d < x->ndims; d++)
        n *= x->hb[d] - x->lb[d] + 1;
    return n;
}

static void collect_areas(struct sym *x, int off)
{
    int n = dims_count(x);
    for (int e = 0; e < n; e++) {
        int o = off + e * x->size;
        if (x->child) {
            for (struct sym *c = x->child; c; c = c->sibling)
                collect_areas(c, o + c->moff);
        } else if (x->t.k == TY_AREA) {
            if (nareas == MAXAREAS)
                fatal("%s: %s holds more than %d areas -- not supported yet", src_name,
                      x->root->name, MAXAREAS);
            area_off[nareas] = o;
            area_size[nareas] = x->t.p;
            nareas++;
        }
    }
}

/* the areas of root, from offset 0 of its storage (a structure from its
 * first byte, not its phase) */
static int find_areas(struct sym *root)
{
    nareas = 0;
    collect_areas(root, 0);
    return nareas;
}

static void area_init(int base, int off, int n);

/* every area of root at off(base): initialized */
static void areas_init(struct sym *root, int base, int off)
{
    find_areas(root);
    for (int i = 0; i < nareas; i++)
        area_init(base, off + area_off[i], area_size[i]);
}

/* An area allocated at off(base): its size, no extent, no free list
 * (GY28-6801-6, "The Area Variable"). */
static void area_init(int base, int off, int n)
{
    int t = reg_get(), b = base;
    if (off + 16 > 4095) {
        /* beyond a displacement: the address first */
        b = reg_get();
        ins(NULL, "L", "%d,=A(%d)", b, off);
        ins(NULL, "AR", "%d,%d", b, base);
        off = 0;
    }
    ins(NULL, "L", "%d,=F'%d'", t, n);
    ins(NULL, "ST", "%d,%d(,%d)", t, off, b);
    ins(NULL, "XC", "%d(12,%d),%d(%d)", off + 4, b, off + 4, b);
    if (b != base)
        reg_free(b);
    reg_free(t);
}

/* The address of an area variable. */
static int area_addr(struct expr *a)
{
    struct place p = gen_place(a);
    int r = p_take(&p);
    ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
    p_free_except(&p, r);
    return r;
}

/* A locator value: a pointer, or an offset (a word, the distance from
 * the start of its area, control information included: JOB00418). */
static int gen_ptr(struct expr *e)
{
    int r;
    if (e->k == E_CAST && e->t.k != e->l->t.k) {
        /* an offset as a pointer, a pointer as an offset: by the
         * offset's area, wherever it is now */
        struct expr *o = e->t.k == TY_OFFSET ? e : e->l;
        int a;
        r = gen_ptr(e->l);
        a = area_addr(o->t.area);
        ins(NULL, e->t.k == TY_OFFSET ? "SR" : "AR", "%d,%d", r, a);
        reg_free(a);
        return r;
    }
    if (e->k == E_CAST)
        return gen_ptr(e->l);
    if (e->k == E_CALL && (e->fn == B_NULL || e->fn == B_NULLO)) {
        r = reg_get();
        ins(NULL, "L", "%d,%s", r, NULL_PTR);
        return r;
    }
    if (e->k == E_CALL && e->fn == B_SYSNULL) {
        r = reg_get();
        ins(NULL, "SR", "%d,%d", r, r);
        return r;
    }
    if (e->k == E_CALL && e->fn == B_PTRADD) {
        int n;
        r = gen_ptr(e->arg[0]);
        n = gen_int(e->arg[1]);
        ins(NULL, "AR", "%d,%d", r, n);
        reg_free(n);
        return r;
    }
    if (e->k == E_CALL && e->fn == B_PTRVALUE)
        return gen_int(e->arg[0]);
    if (e->k == E_CALL && e->fn == B_ENTRYADDR) {
        struct expr *a = e->arg[0];
        r = reg_get();
        if (a->k == E_ENTRY && a->callee->is_stub)
            ins(NULL, "L", "%d,=V(%s)", r, a->callee->extname ? a->callee->extname :
                ext_name(a->callee->name));   /* compiled apart */
        else if (a->k == E_ENTRY)
            ins(NULL, "L", "%d,=A(@P%d)", r, a->callee->id);
        else {
            struct place p = gen_place(a);  /* an ENTRY variable: its first word */
            ins(NULL, "L", "%d,%s", r, p_rx(&p, 0));
            p_free(&p);
        }
        return r;
    }
    if (e->k == E_CALL && e->fn == B_ADDR) {
        struct place p = gen_place(e->arg[0]);
        r = p_take(&p);
        ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
        p_free_except(&p, r);
        return r;
    }
    if (e->k == E_FCALL) {
        int res = gen_fcall(e);
        r = reg_get();
        ins(NULL, "L", "%d,%d(,13)", r, res);
        return r;
    }
    if (e->k == E_VAR) {
        struct place p = gen_place(e);
        r = p_take(&p);
        ins(NULL, "L", "%d,%s", r, p_rx(&p, 0));
        p_free_except(&p, r);
        return r;
    }
    fatal("%s(%d): internal: no pointer code for this expression", src_name, e->line);
    return 0;
}

/* Branch to label when e has the truth value want.  A bit string is true
 * when any bit is 1.  & | NOT are taken as jumps only on single bits; on
 * longer strings they are bitwise, and the result is tested. */
static void gen_jump(struct expr *e, int label, int want)
{
    int mark = tmp_top;

    if (e->k == E_BIN && (e->op == T_AND || e->op == T_OR) && bit1(e->l) && bit1(e->r)) {
        int both = e->op == T_AND;
        if (want != both) {
            gen_jump(e->l, label, want);
            gen_jump(e->r, label, want);
        } else {
            int skip = new_label();
            gen_jump(e->l, skip, !want);
            gen_jump(e->r, label, want);
            here(skip);
        }
        return;
    }
    if (e->k == E_NOT && bit1(e->l)) {
        gen_jump(e->l, label, !want);
        return;
    }
    if (e->k == E_BIN && is_cmp(e->op)) {
        struct type a = e->l->t, b = e->r->t;
        int m = true_mask(e->op);

        if (is_string(a) && is_string(b)) {
            int a1, l1, a2, l2;
            gen_string(e->l, &a1, &l1);
            gen_string(e->r, &a2, &l2);
            arg_reg(a1); arg_reg(l1); arg_reg(a2); arg_reg(l2);
            call("PL1CMPC");
            reg_free(a1); reg_free(l1); reg_free(a2); reg_free(l2);
            ins(NULL, "LTR", "15,15");
        } else if (is_bit(a) && is_bit(b) && !(bit_reg_ok(e->l) && bit_reg_ok(e->r))) {
            /* in storage, the shorter padded with zeros (JOB00336) */
            int a1, l1, a2, l2;
            gen_bits(e->l, &a1, &l1);
            gen_bits(e->r, &a2, &l2);
            arg_reg(a1); arg_reg(l1); arg_reg(a2); arg_reg(l2);
            reg_free(a1); reg_free(l1); reg_free(a2); reg_free(l2);
            call("PL1BCMP");
            ins(NULL, "LTR", "15,15");
        } else if (a.k == TY_BIT && b.k == TY_BIT) {
            int x = gen_bitval(e->l), y = gen_bitval(e->r);
            ins(NULL, "CLR", "%d,%d", x, y);
            reg_free(x);
            reg_free(y);
        } else if (a.k == TY_PTR || a.k == TY_OFFSET) {
            int x = gen_ptr(e->l), y = gen_ptr(e->r);
            ins(NULL, "CLR", "%d,%d", x, y);
            reg_free(x);
            reg_free(y);
        } else if (is_float(a) || is_float(b)) {
            struct type c = float_common(a, b);
            int lo = flt_long(c), x = gen_flt(e->l, c), y = gen_flt(e->r, c);
            ins(NULL, lo ? "LD" : "LE", "0,%d(,13)", x);
            ins(NULL, lo ? "CD" : "CE", "0,%d(,13)", y);
        } else if (a.k == TY_FIXDEC && b.k == TY_FIXDEC) {
            int q = a.q > b.q ? a.q : b.q;
            int x = gen_dec(e->l), y = gen_dec(e->r);
            rescale(x, a.q, q);
            rescale(y, b.q, q);
            ins(NULL, "CP", "%s,%s", tss(x, 8), tss(y, 8));
        } else {
            int sa = bin_scale(a), sb = bin_scale(b), q = sa > sb ? sa : sb;
            int x = gen_bin(e->l), y = gen_bin(e->r);
            bin_rescale(x, sa, q);
            bin_rescale(y, sb, q);
            ins(NULL, "CR", "%d,%d", x, y);
            reg_free(x);
            reg_free(y);
        }
        tmp_top = mark;
        ins(NULL, "BC", "%d,@L%d", want ? m : 15 - m, label);
        return;
    }
    if (is_bit(e->t) && !bit_reg_ok(e)) {
        /* true when a bit is 1 (JOB00336: the null string is false) */
        int a, l;
        gen_bits(e, &a, &l);
        arg_reg(a);
        arg_reg(l);
        reg_free(a);
        reg_free(l);
        call("PL1BANY");
        ins(NULL, "LTR", "15,15");
        tmp_top = mark;
        ins(NULL, "BC", "%d,@L%d", want ? 7 : 8, label);
        return;
    }
    {
        int r = gen_bitval(e);
        ins(NULL, "LTR", "%d,%d", r, r);
        reg_free(r);
        tmp_top = mark;
        ins(NULL, "BC", "%d,@L%d", want ? 7 : 8, label);
    }
}

/* ---- statements ----------------------------------------------------- */

static void comment(struct stmt *s)
{
    const char *t = s->text ? s->text : "";
    int n = (int)strlen(t), i = 0, first = 1;
    while (i < n || first) {
        char chunk[64];
        int take = n - i > 60 ? 60 : n - i;
        memcpy(chunk, t + i, (size_t)take);
        chunk[take] = '\0';
        /* the listing comment is EBCDIC too: the NOT sign as '^' */
        for (int k = 0; k < take; k++)
            if ((unsigned char)chunk[k] >= 0x80)
                chunk[k] = '?';
        if (first)
            out("* %4d  %s", s->no, chunk);
        else
            out("*       %s", chunk);
        i += take;
        first = 0;
    }
}

static void gen_put_item(struct expr *e)
{
    int mark = tmp_top;

    if (e->t.k == TY_PIC) {
        /* a numeric picture: its characters and its value (rt/pl1io.c) */
        struct expr pv, *cv = xmalloc(sizeof *cv);
        int t, ra, rt;
        struct place p;
        *cv = *e;
        cv->picstr = 1;
        cv->t.k = TY_CHAR;
        cv->t.p = pic_length(e->sym->t);
        cv->t.pic = NULL;
        memset(&pv, 0, sizeof pv);
        pv.k = E_CALL;
        pv.fn = B_PICVAL;
        pv.nargs = 1;
        pv.arg[0] = cv;
        pv.t.k = TY_FIXDEC;
        pv.t.p = e->sym->t.p;
        pv.t.q = e->sym->t.q;
        pv.line = e->line;
        t = gen_dec(&pv);
        p = gen_place(e);
        ra = p_take(&p);
        ins(NULL, "LA", "%d,%s", ra, p_rx(&p, 0));
        p_free_except(&p, ra);
        rt = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", rt, t);
        arg_reg(ra);
        arg_const(pic_length(e->sym->t));
        arg_reg(rt);
        arg_const(e->sym->t.q);
        reg_free(ra);
        reg_free(rt);
        call("PL1LPIC");
        tmp_top = mark;
        return;
    }
    if (is_bit(e->t) && !bit_reg_ok(e)) {
        int a, l;
        gen_bits(e, &a, &l);
        arg_reg(a);
        arg_reg(l);
        reg_free(a);
        reg_free(l);
        call("PL1LBTS");
    } else if (e->t.k == TY_BIT) {
        int r = gen_bitval(e);
        arg_reg(r);
        reg_free(r);
        arg_const(e->t.p);
        call("PL1LBIT");
    } else if (e->k == E_CALL && e->fn == B_CPXSTR) {
        /* a complex value: one item, written as a number is (no quotes) */
        int ra, rl;
        gen_string(e, &ra, &rl);
        arg_reg(ra);
        arg_reg(rl);
        reg_free(ra);
        reg_free(rl);
        call("PL1LNUM");
    } else if (is_string(e->t)) {
        if (e->k == E_VAR && e->sym->t.k == TY_VCHAR) {
            struct place p = gen_place(e);
            int r = p_take(&p);
            ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
            p_free_except(&p, r);
            arg_reg(r);
            reg_free(r);
            call("PL1LVCH");
        } else {
            int ra, rl;
            gen_string(e, &ra, &rl);
            arg_reg(ra);
            arg_reg(rl);
            reg_free(ra);
            reg_free(rl);
            call("PL1LCHR");
        }
    } else if (is_float(e->t)) {
        int t = gen_flt(e, e->t), r = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", r, t);
        arg_reg(r);
        reg_free(r);
        arg_const(flt_long(e->t));
        arg_const(flt_digits(e->t));
        call("PL1LFLT");
    } else if (e->t.k == TY_FIXBIN) {
        int r = gen_bin(e);
        arg_reg(r);
        reg_free(r);
        arg_const(e->t.p);
        arg_const(e->t.q);
        call("PL1LFXS");
    } else {
        int r;
        if (e->k == E_VAR) {
            struct place p = gen_place(e);
            r = p_take(&p);
            ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
            p_free_except(&p, r);
        } else {
            int t = gen_dec(e);
            r = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", r, t + 8 - dec_bytes(e->t.p));
        }
        arg_reg(r);
        reg_free(r);
        arg_const(e->t.p);
        arg_const(e->t.q);
        call("PL1LFXD");
    }
    tmp_top = mark;
}

static int nstatic, nfile;

/* The establishments of the procedure being generated: a record of 24
 * bytes in the frame (rt/pl1cond.c) for each condition -- with its file or
 * name -- that an ON statement of the procedure establishes. */
#define MAXON 32
#define EST_SIZE 28
static struct est { enum cond c; struct sym *file; const char *name; int off; } ests[MAXON];
static int nests;

/* Names of CONDITION conditions, AL1(n),C'name', emitted with the data. */
struct cname { const char *name; int label; struct cname *next; };
static struct cname *cnames;

static int cname_label(const char *name)
{
    struct cname *c;
    for (c = cnames; c; c = c->next)
        if (strcmp(c->name, name) == 0)
            return c->label;
    c = xmalloc(sizeof *c);
    c->name = name;
    c->label = new_label();
    c->next = cnames;
    cnames = c;
    return c->label;
}

static int est_index(struct stmt *s)
{
    for (int i = 0; i < nests; i++)
        if (ests[i].c == s->cond &&
            (!FILE_COND(s->cond) || ests[i].file == s->file->sym) &&
            ((s->cond != C_CONDITION && s->cond != C_CHECK) ||
             strcmp(ests[i].name, s->cond_name) == 0))
            return i;
    return -1;
}

static void collect_ons(struct stmt *s)
{
    for (; s; s = s->next) {
        if (s->k == S_ON && est_index(s) < 0) {
            if (nests == MAXON)
                fatal("%s: too many conditions established in one procedure", src_name);
            ests[nests].c = s->cond;
            ests[nests].file = FILE_COND(s->cond) ? s->file->sym : NULL;
            ests[nests].name = s->cond_name;
            nests++;
        }
        collect_ons(s->body);
        collect_ons(s->els);
    }
}

/* Format tables of EDIT statements, emitted with the data. */
struct ftab { int label; struct fmt *f; struct ftab *next; };
static struct ftab *ftabs;

static int cur_eof;            /* where a GET item goes at end of file */

/* The address of a file's descriptor in a register. */
static int file_desc(struct expr *f)
{
    int r;
    if (f->sym->var_ef) {
        /* a FILE variable: it holds the descriptor's address */
        struct place q = gen_place(f);
        r = p_take(&q);
        ins(NULL, "L", "%d,%s", r, p_rx(&q, 0));
        p_free_except(&q, r);
        return r;
    }
    r = reg_get();
    if (f->sym->param) {
        /* a FILE parameter: its argument's address is the descriptor */
        int fb = frame_reg(f->sym->owner);
        ins(NULL, "L", "%d,%d(,%d)", r, ALIST, fb);
        ins(NULL, "L", "%d,%d(,%d)", r, 8 + 4 * (f->sym->param - 1), r);
        if (fb != 13)
            reg_free(fb);
        return r;
    }
    ins(NULL, "L", "%d,=A(%s)", r, ext_sect(f->sym, 1));
    return r;
}

static void select_file(struct expr *f, int mode)
{
    int r = file_desc(f);
    arg_reg(r);
    reg_free(r);
    arg_const(mode);
    call("PL1SEL");
}

/* The STRING option: the string as a file of one record. */
static void select_string(struct stmt *s, int mode)
{
    struct expr *x = s->string;
    if (s->k == S_PUT) {
        struct place p = gen_place(x);
        int r = p_take(&p);
        ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
        p_free_except(&p, r);
        arg_reg(r);
        reg_free(r);
        arg_const(x->sym->t.p);
        arg_const(x->sym->t.k == TY_VCHAR);
    } else {
        int ra, rl;
        gen_string(x, &ra, &rl);
        arg_reg(ra);
        arg_reg(rl);
        reg_free(ra);
        reg_free(rl);
        arg_const(0);
    }
    arg_const(mode);
    call("PL1SSEL");
}

/* The qualified name of x, as PUT and GET DATA write and read it. */
static void qual_name(struct sym *x, char *b, int n)
{
    if (x->parent) {
        qual_name(x->parent, b, n);
        strncat(b, ".", (size_t)n - strlen(b) - 1);
        strncat(b, x->name, (size_t)n - strlen(b) - 1);
    } else {
        snprintf(b, (size_t)n, "%s", x->name);
    }
}

/* PUT DATA: the name of the item and the values of its subscripts. */
static void data_name(struct expr *e)
{
    char name[300];
    int r, len, subs[3];
    if (e->k == E_CALL && e->fn == B_CPXSTR)
        e = e->arg[2];                  /* a complex variable (check.c cx_put) */
    if (e->k != E_VAR)
        fatal("%s(%d): PUT DATA takes variables", src_name, e->line);
    if (e->nsubs > 3)
        fatal("%s(%d): PUT DATA of more than 3 dimensions is not supported", src_name, e->line);
    qual_name(e->sym, name, sizeof name);
    len = (int)strlen(name);
    for (int i = 0; i < e->nsubs; i++)
        subs[i] = gen_int(e->sub[i]);
    r = reg_get();
    ins(NULL, "L", "%d,=A(@L%d)", r, str_label(name, len));
    arg_reg(r);
    reg_free(r);
    arg_const(len);
    arg_const(e->nsubs);
    for (int i = 0; i < 3; i++) {
        if (i < e->nsubs) {
            arg_reg(subs[i]);
            reg_free(subs[i]);
        } else {
            arg_const(0);
        }
    }
    call("PL1DNAM");
}

/* GET DATA: a table of the names it may assign (rt/pl1io.c), filled in the
 * frame, since the addresses are; a structure counts as its members. */
static int data_entries(struct sym *x, struct sym **out, int n)
{
    if (x->child) {
        for (struct sym *c = x->child; c; c = c->sibling)
            n = data_entries(c, out, n);
        return n;
    }
    if (n >= 64)
        fatal("%s: too many names in one GET DATA", src_name);
    out[n++] = x;
    return n;
}

static void gen_get_data(struct stmt *s)
{
    struct sym *list[64];
    int n = 0, t, r = reg_get();
    for (struct put_item *it = s->items; it; it = it->next)
        n = data_entries(it->e->sym, list, n);
    t = tmp(24 * n, 4);
    for (int i = 0; i < n; i++)
        if (list[i]->t.k == TY_PIC || list[i]->t.pic)
            fatal("%s(%d): GET DATA of a picture (%s) is not supported yet", src_name, s->line,
                  list[i]->name);
    for (int i = 0; i < n; i++) {
        struct sym *x = list[i];
        struct expr *ref = xmalloc(sizeof *ref);
        struct place p;
        char name[300];
        int o = t + 24 * i;
        qual_name(x, name, sizeof name);
        ins(NULL, "L", "%d,=A(@L%d)", r, cname_label(xstrdup(name)));
        ins(NULL, "ST", "%d,%d(,13)", r, o);
        memset(ref, 0, sizeof *ref);
        ref->k = E_VAR;
        ref->sym = x;
        ref->t = x->t;
        ref->line = s->line;
        p = gen_place(ref);                 /* the first element */
        {
            int a = p_take(&p);
            ins(NULL, "LA", "%d,%s", a, p_rx(&p, 0));
            p_free_except(&p, a);
            ins(NULL, "ST", "%d,%d(,13)", a, o + 4);
            reg_free(a);
        }
        ins(NULL, "MVC", "%d(4,13),=F'%d'", o + 8, (x->t.k << 16) | x->t.p);
        ins(NULL, "MVC", "%d(4,13),=F'%d'", o + 12, ((x->t.q & 0xFFFF) << 16) | x->ndims);
        ins(NULL, "MVC", "%d(4,13),=F'%d'", o + 16,
            x->ndims ? (int)(((unsigned)x->lb[0] << 16) | ((unsigned)x->hb[0] & 0xFFFF)) : 0);
        ins(NULL, "MVC", "%d(4,13),=F'%d'", o + 20, x->size);
    }
    ins(NULL, "LA", "%d,%d(,13)", r, t);
    arg_reg(r);
    reg_free(r);
    arg_const(n);
    call("PL1GDAT");
}

static void format_begin(struct stmt *s)
{
    struct ftab *t = xmalloc(sizeof *t);
    int r = reg_get();
    t->label = new_label();
    t->f = s->fmts;
    t->next = ftabs;
    ftabs = t;
    ins(NULL, "L", "%d,=A(@L%d)", r, t->label);
    /* widths and d given by expressions: their values, assigned just
     * before the statement (parse.c), into the table (Q1FEXP) */
    for (int i = 0; s->fmts[i].k != F_END; i++) {
        struct fmt *f = &s->fmts[i];
        for (int j = 0; j < 2; j++) {
            struct sym *v = j ? f->ds : f->ws;
            struct expr ev;
            int x;
            if (!v)
                continue;
            memset(&ev, 0, sizeof ev);
            ev.k = E_VAR;
            ev.sym = v;
            ev.t = v->t;
            ev.line = s->line;
            x = gen_int(&ev);
            if (j && f->k == F_E) {
                /* d + 256 * s, s given or d + 1 */
                if (f->sd >= 0) {
                    ins(NULL, "A", "%d,=F'%d'", x, 256 * f->sd);
                } else {
                    int y = reg_get();
                    ins(NULL, "LR", "%d,%d", y, x);
                    ins(NULL, "SLL", "%d,8", y);
                    ins(NULL, "AR", "%d,%d", x, y);
                    ins(NULL, "A", "%d,=F'256'", x);
                    reg_free(y);
                }
            }
            ins(NULL, "STH", "%d,%d(,%d)", x, 6 * i + (j ? 4 : 2), r);
            reg_free(x);
        }
    }
    arg_reg(r);
    reg_free(r);
    call("PL1FBEG");
}

/* One GET target: the run time stores the next item into it; at end of
 * file it returns 1. */
static void gen_get_item(struct expr *lhs)
{
    struct place p;
    int r;
    struct type t = lhs->sym->t;
    if (lhs->sym->packed) {
        /* packed bits: into a temporary, then into the bits */
        int tt = tmp(bytes_of(t.p), 1), a, b, s;
        r = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", r, tt);
        arg_reg(r);
        reg_free(r);
        arg_const(t.p);
        call("PL1GBIT");
        ins(NULL, "LTR", "15,15");
        ins(NULL, "BC", "7,@L%d", cur_eof);
        packed_addr(lhs, &a, &b);
        s = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", s, tt);
        arg_reg(a);
        arg_reg(b);
        arg_const(t.p);
        arg_reg(s);
        arg_const(t.p);
        reg_free(a);
        reg_free(b);
        reg_free(s);
        call("PL1BPUT");
        return;
    }
    if (t.k == TY_PIC) {
        /* a numeric picture: the value as FIXED DECIMAL(15,q), then
         * edited into the variable (Q3STL) */
        int v = tmp(8, 1), rp, rd;
        r = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", r, v);
        arg_reg(r);
        reg_free(r);
        arg_const(15);
        arg_const(t.q);
        call("PL1GFXD");
        ins(NULL, "LTR", "15,15");
        ins(NULL, "BC", "7,@L%d", cur_eof);
        p = gen_place(lhs);
        rd = p_take(&p);
        ins(NULL, "LA", "%d,%s", rd, p_rx(&p, 0));
        p_free_except(&p, rd);
        r = reg_get();
        rp = reg_get();
        ins(NULL, "LA", "%d,%d(,13)", r, v);
        pic_literal(rp, t.pic);
        arg_reg(r);
        arg_reg(rp);
        arg_const((int)strlen(t.pic));
        arg_reg(rd);
        reg_free(r);
        reg_free(rp);
        reg_free(rd);
        call("PL1PEDT");
        return;
    }
    p = gen_place(lhs);
    r = p_take(&p);
    ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
    p_free_except(&p, r);
    arg_reg(r);
    reg_free(r);
    switch (t.k) {
    case TY_FIXBIN: arg_const(t.p | t.q << 8); call("PL1GFXB"); break;
    case TY_FIXDEC: arg_const(t.p); arg_const(t.q); call("PL1GFXD"); break;
    case TY_CHAR:   arg_const(t.p); call("PL1GCHR"); break;
    case TY_VCHAR:  arg_const(t.p); call("PL1GVCH"); break;
    case TY_BIT:    arg_const(t.p); call("PL1GBIT"); break;
    case TY_VBIT:   arg_const(t.p); call("PL1GVBT"); break;
    case TY_FLTDEC:
    case TY_FLTBIN: arg_const(flt_long(t)); call("PL1GFLT"); break;
    default:
        fatal("internal: GET target type");
    }
    ins(NULL, "LTR", "15,15");
    ins(NULL, "BC", "7,@L%d", cur_eof);
}

/* The address of a condition's file or name in a register, or 0. */
static int qual_reg(struct stmt *s)
{
    int r;
    if (FILE_COND(s->cond))
        return file_desc(s->file);
    r = reg_get();
    if (s->cond == C_CONDITION || s->cond == C_CHECK)
        ins(NULL, "L", "%d,=A(@L%d)", r, cname_label(s->cond_name));
    else
        ins(NULL, "SR", "%d,%d", r, r);
    return r;
}

/* The ONCODE of SIGNAL for each condition, as PL/I (F) sets it
 * (JOB00137-140). */
static int signal_code(enum cond c)
{
    switch (c) {
    case C_ENDFILE:       return 70;
    case C_ENDPAGE:       return 90;
    case C_UNDF:          return 84;
    case C_RECORD:        return 21;
    case C_AREA:          return 362;
    case C_KEY:           return 50;
    case C_NAME:          return 10;
    case C_CHECK:         return 511;
    case C_TRANSMIT:      return 40;
    case C_STRINGSIZE:    return 150;    /* ANSI: implementation-defined */
    case C_STORAGE:       return 454;
    case C_ATTENTION:     return 1400;   /* GC33-0009-4, p. 386 */
    case C_ZERODIVIDE:    return 320;
    case C_FIXEDOVERFLOW: return 310;
    case C_CONVERSION:    return 600;
    case C_ERROR:         return 9;
    case C_FINISH:        return 4;
    case C_CONDITION:     return 500;
    case C_UNDERFLOW:     return 330;
    case C_SIZE:          return 340;
    case C_SUBRG:         return 520;
    case C_STRG:          return 350;
    case C_OVERFLOW:      return 300;
    case C_NONE:          break;
    }
    return 0;
}

/* End of file on fe: ENDFILE; after a normal return of the on-unit, the
 * statement after the GET or READ. */
static void eof_dispatch(struct expr *fe, int lafter)
{
    int r;
    if (!fe) {
        /* the end of a STRING: ERROR (not measured) */
        arg_const(C_ERROR);
        arg_const(0);
        arg_const(9);
        call("PL1SIG");
        ins(NULL, "B", "@L%d", lafter);
        return;
    }
    r = file_desc(fe);
    arg_const(C_ENDFILE);
    arg_reg(r);
    reg_free(r);
    arg_const(70);
    call("PL1SIG");
    ins(NULL, "B", "@L%d", lafter);
}

static void gen_stmts(struct stmt *s);

static void gen_stmt1(struct stmt *s);

/* The do-groups being generated, innermost last: where LEAVE goes. */
static struct { struct stmt *s; int lend; } loops[64];
static int nloops;

static void loop_push(struct stmt *s, int lend)
{
    if (nloops == 64)
        fatal("%s(%d): do-groups nested more than 64 deep", src_name, s->line);
    loops[nloops].s = s;
    loops[nloops].lend = lend;
    nloops++;
}

/* LEAVE [label]: the end of the innermost do-group, or of the one
 * whose DO has the label; never out of a block (GC33-0009-4) */
static void gen_leave(struct stmt *s)
{
    comment(s);
    for (int i = nloops - 1; i >= 0; i--) {
        struct stmt *g = loops[i].s;
        int hit = !s->goto_name;
        for (int k = 0; k < g->nlabels && !hit; k++)
            hit = strcmp(g->labels[k], s->goto_name) == 0;
        if (hit) {
            ins(NULL, "B", "@L%d", loops[i].lend);
            return;
        }
    }
    fatal("%s(%d): LEAVE%s%s outside %s", src_name, s->line, s->goto_name ? " " : "",
          s->goto_name ? s->goto_name : "", s->goto_name ? "a do-group of that label" :
          "a do-group");
}

/* The general iterative DO (check.c prepare_iter), as GC28-8201-4
 * section J expands it: per specification, TO and BY into their
 * temporaries, the control variable set, then the test -- against TO
 * by the sign of BY -- and WHILE; the body once after all of them, and
 * after it the increment of the specification that entered it. */
static void gen_iter(struct stmt *s)
{
    struct expr *cv = simple_ref(s->target);
    int lbody = new_label(), lend = new_label(), n = 0, k = 0, state = 0;
    int saved_base = tmp_base, incr[64];
    comment(s);
    loop_push(s, lend);
    for (struct dospec *d = s->specs; d; d = d->next)
        n++;
    if (n > 64)
        fatal("%s(%d): more than 64 specifications in one DO", src_name, s->line);
    if (n > 1) {
        state = tmp(4, 4);               /* the specification running */
        tmp_base = tmp_top;
    }
    for (struct dospec *d = s->specs; d; d = d->next, k++) {
        int ltest = new_label(), lnext = new_label();
        incr[k] = new_label();
        if (d->v2) {
            gen_assign(simple_ref(d->v2), d->e2);
            tmp_top = tmp_base;
        }
        if (d->v3) {
            gen_assign(simple_ref(d->v3), d->e3);
            tmp_top = tmp_base;
        }
        gen_assign(cv, d->e1);
        tmp_top = tmp_base;
        here(ltest);
        if (d->e2 && d->e3) {
            int lneg = new_label(), lw = new_label();
            gen_jump(d->neg, lneg, 1);
            gen_jump(d->gt, lnext, 1);
            ins(NULL, "B", "@L%d", lw);
            here(lneg);
            gen_jump(d->lt, lnext, 1);
            here(lw);
        } else if (d->e2) {
            gen_jump(d->gt, lnext, 1);
        }
        if (d->w)
            gen_jump(d->w, lnext, 0);
        tmp_top = tmp_base;
        if (n > 1)
            ins(NULL, "MVC", "%d(4,13),=F'%d'", state, k);
        ins(NULL, "B", "@L%d", lbody);
        here(incr[k]);
        if (d->u)
            gen_jump(d->u, lnext, 1);           /* UNTIL true: this one ends */
        tmp_top = tmp_base;
        if (d->rep) {
            gen_assign(cv, d->rep);             /* REPEAT */
            tmp_top = tmp_base;
            ins(NULL, "B", "@L%d", ltest);
        } else if (d->incr) {
            gen_assign(cv, d->incr);
            tmp_top = tmp_base;
            ins(NULL, "B", "@L%d", ltest);
        } else {
            ins(NULL, "B", "@L%d", lnext);      /* executed once */
        }
        here(lnext);
    }
    ins(NULL, "B", "@L%d", lend);
    here(lbody);
    gen_stmts(s->body);
    if (n == 1) {
        ins(NULL, "B", "@L%d", incr[0]);
    } else {
        int r = reg_get();
        ins(NULL, "L", "%d,%d(,13)", r, state);
        for (k = 0; k < n; k++) {
            ins(NULL, "C", "%d,=F'%d'", r, k);
            ins(NULL, "BC", "8,@L%d", incr[k]);
        }
        reg_free(r);
    }
    here(lend);
    nloops--;
    tmp_base = saved_base;
}

/* The run time's flags of a mask (PL1NOCV): 1 NOCONVERSION, 2
 * NOOVERFLOW, 4 NOSTRINGSIZE -- the conditions it tests itself. */
static int rt_flags(int mask)
{
    return (mask & EN_CONV ? 0 : PL1_NOCV_CONV) | (mask & EN_OFL ? 0 : PL1_NOCV_OFL) |
           (mask & EN_STRZ ? 0 : PL1_NOCV_STRZ);
}

/* A statement with the conditions enabled in it; NOCONVERSION and
 * NOOVERFLOW set the run time's flags for its duration. */
static void gen_stmt(struct stmt *s)
{
    int saved = cur_mask;
    for (int i = 0; i < s->nlabels; i++)
        out("@G%-6d DS    0H", s->label_id[i]);
    if (s->mask & EN_SET)
        cur_mask = s->mask;
    if ((cur_mask ^ saved) & (EN_CONV | EN_OFL | EN_STRZ)) {
        ins(NULL, "L", "15,=V(PL1NOCV)");
        ins(NULL, "MVI", "0(15),%d", rt_flags(cur_mask));
    }
    if ((s->k == S_READ || s->k == S_WRITE || s->k == S_REWRITE) && s->nmore) {
        /* EVENT I/O: the statement between PL1EVIO and PL1EVEN, which keep
         * its file conditions in the event for the WAIT (rt/pl1cond.c) */
        struct place p = gen_place(s->more[0]);
        int r = p_take(&p);
        tmp_top = tmp_base;
        ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
        p_free_except(&p, r);
        arg_reg(r);
        reg_free(r);
        call("PL1EVIO");
        gen_stmt1(s);
        call("PL1EVEN");
    } else {
        gen_stmt1(s);
    }
    if ((cur_mask ^ saved) & (EN_CONV | EN_OFL | EN_STRZ)) {
        ins(NULL, "L", "15,=V(PL1NOCV)");
        ins(NULL, "MVI", "0(15),%d", rt_flags(saved));
    }
    cur_mask = saved;
}

static void gen_stmt1(struct stmt *s)
{
    tmp_top = tmp_base;
    switch (s->k) {
    case S_CHECK: {
        /* CHECK(name): an on-unit, or the system action -- the name of a
         * label or entry written by the run time, a variable's value by
         * the PUT DATA of the body (Q1CHK) */
        int r = reg_get(), skip = new_label();
        comment(s);
        ins(NULL, "L", "%d,=A(@L%d)", r, cname_label(s->cond_name));
        arg_reg(r);
        reg_free(r);
        arg_const(s->check_code);
        call("PL1CHK");
        if (s->body) {
            ins(NULL, "LTR", "15,15");
            ins(NULL, "BC", "8,@L%d", skip);
            call("PL1CKB");
            gen_stmts(s->body);
            call("PL1CKE");
            here(skip);
        }
        break;
    }
    case S_NULL:
    case S_FORMAT:                        /* not executed: R takes its list */
        comment(s);
        break;
    case S_ADJUST: {
        /* an adjustable array at its block's entry: its bounds into the
         * descriptor (A(data), F(element length), F(lb), F(hb) ...), the
         * data on the stack past the frame -- the next available byte at
         * 76(13), which PDPPRLG set and the caller's frame keeps, so
         * leaving the block in any way gives it back (Tier 81) */
        struct sym *x = s->target;
        int off = x->off, n = reg_get();
        comment(s);
        stk_unknown = 1;      /* storage of a size known only at run time */
        load_const(n, 1);
        for (int d = 0; d < x->ndims; d++) {
            int lo = gen_int(x->lbx[d]), hi, z = new_label();
            ins(NULL, "ST", "%d,%d(,13)", lo, off + 8 + 8 * d);
            hi = gen_int(x->hbx[d]);
            ins(NULL, "ST", "%d,%d(,13)", hi, off + 12 + 8 * d);
            ins(NULL, "SR", "%d,%d", hi, lo);
            ins(NULL, "LA", "%d,1(,%d)", hi, hi);
            ins(NULL, "LTR", "%d,%d", hi, hi);
            ins(NULL, "BC", "2,@L%d", z);
            ins(NULL, "SR", "%d,%d", hi, hi);      /* no elements */
            here(z);
            ins(NULL, "LR", "1,%d", n);
            ins(NULL, "MR", "0,%d", hi);
            ins(NULL, "LR", "%d,1", n);
            reg_free(lo);
            reg_free(hi);
        }
        {
            int a = reg_get();
            ins(NULL, "LR", "1,%d", n);
            ins(NULL, "M", "0,=F'%d'", x->size);
            ins(NULL, "LA", "%d,7(,1)", n);
            ins(NULL, "N", "%d,=F'-8'", n);
            ins(NULL, "L", "%d,76(,13)", a);
            ins(NULL, "ST", "%d,%d(,13)", a, off);
            ins(NULL, "AR", "%d,%d", n, a);
            ins(NULL, "ST", "%d,76(,13)", n);
            load_const(a, x->size);
            ins(NULL, "ST", "%d,%d(,13)", a, off + 4);
            reg_free(a);
        }
        reg_free(n);
        break;
    }
    case S_ASSIGN:
        comment(s);
        gen_assign(s->lhs, s->e);
        break;
    case S_IF: {
        int lelse = new_label(), lend = new_label();
        comment(s);
        gen_jump(s->e, lelse, 0);
        if (s->body)
            gen_stmt(s->body);
        if (s->els) {
            ins(NULL, "B", "@L%d", lend);
            here(lelse);
            gen_stmt(s->els);
            here(lend);
        } else {
            here(lelse);
        }
        break;
    }
    case S_GROUP: {
        int lend = new_label();
        comment(s);
        loop_push(s, lend);
        gen_stmts(s->body);
        nloops--;
        here(lend);
        break;
    }
    case S_WHILE: {
        /* WHILE before each pass, UNTIL after it */
        int ltop = new_label(), lend = new_label();
        comment(s);
        loop_push(s, lend);
        here(ltop);
        if (s->e)
            gen_jump(s->e, lend, 0);
        gen_stmts(s->body);
        if (s->e2)
            gen_jump(s->e2, lend, 1);
        poll();
        ins(NULL, "B", "@L%d", ltop);
        here(lend);
        nloops--;
        break;
    }
    case S_FETCH:
    case S_RELEASE:
        /* LOAD or DELETE each entry's module (rt/pl1fetch.asm) */
        comment(s);
        for (int i = 0; i < s->nmore; i++) {
            int r = reg_get();
            ins(NULL, "LA", "%d,@L%d", r, fetch_name(s->more[i]->callee));
            arg_reg(r);
            reg_free(r);
            call(s->k == S_FETCH ? "PL1FTCH" : "PL1RLSE");
        }
        break;
    case S_LEAVE:
        gen_leave(s);
        break;
    case S_WHEN:
        break;                             /* in S_SELECT */
    case S_SELECT: {
        /* each WHEN's values in turn, the first that equals (or is true)
         * selects its action; none and no OTHERWISE: ERROR */
        int lend = new_label(), lother = new_label(), k = 0, nw = 0;
        int acts[256];
        comment(s);
        if (s->e) {
            gen_assign(simple_ref(s->target), s->e);
            tmp_top = tmp_base;
        }
        for (struct stmt *w = s->body; w; w = w->next) {
            if (nw == 256)
                fatal("%s(%d): more than 256 WHEN clauses", src_name, s->line);
            acts[nw] = new_label();
            for (struct put_item *it = w->items; it; it = it->next) {
                gen_jump(s->e ? it->in : it->e, acts[nw], 1);
                tmp_top = tmp_base;
            }
            nw++;
        }
        if (s->els) {
            ins(NULL, "B", "@L%d", lother);
        } else {
            /* not given by GC33-0009-4: ONCODE 3, as SIGNAL ERROR in place
             * of a statement in error */
            arg_const(C_ERROR);
            arg_const(0);
            arg_const(3);
            call("PL1SIG");
            ins(NULL, "B", "@L%d", lend);
        }
        for (struct stmt *w = s->body; w; w = w->next, k++) {
            here(acts[k]);
            gen_stmts(w->body);
            ins(NULL, "B", "@L%d", lend);
        }
        if (s->els) {
            here(lother);
            gen_stmts(s->els);
        }
        here(lend);
        break;
    }
    case S_ITER: {
        int ltop, lend, by = 1, to_const = 0, to_v = 0, to_off = 0;
        if (s->general) {
            gen_iter(s);
            break;
        }
        ltop = new_label();
        lend = new_label();
        loop_push(s, lend);
        {
        int saved_base = tmp_base, r;
        struct expr *cv = simple_ref(s->target);
        struct place pl;
        const char *ld = s->target->t.p <= 15 ? "LH" : "L";
        const char *st = s->target->t.p <= 15 ? "STH" : "ST";
        comment(s);
        if (s->e3)
            const_value(s->e3, &by);
        gen_assign(cv, s->e);
        tmp_top = tmp_base;
        if (const_value(s->e2, &to_v)) {
            to_const = 1;
        } else {
            r = gen_bin(s->e2);
            bin_rescale(r, bin_scale(s->e2->t), 0);
            to_off = tmp(4, 4);
            ins(NULL, "ST", "%d,%d(,13)", r, to_off);
            reg_free(r);
        }
        tmp_base = tmp_top;
        here(ltop);
        pl = gen_place(cv);
        r = p_take(&pl);
        ins(NULL, ld, "%d,%s", r, p_rx(&pl, 0));
        p_free_except(&pl, r);
        if (to_const)
            ins(NULL, "C", "%d,=F'%d'", r, to_v);
        else
            ins(NULL, "C", "%d,%d(,13)", r, to_off);
        reg_free(r);
        ins(NULL, "BC", "%d,@L%d", by > 0 ? 2 : 4, lend);
        gen_stmts(s->body);
        pl = gen_place(cv);
        r = reg_get();
        ins(NULL, ld, "%d,%s", r, p_rx(&pl, 0));
        ins(NULL, "A", "%d,=F'%d'", r, by);
        ins(NULL, st, "%d,%s", r, p_rx(&pl, 0));
        p_free(&pl);
        reg_free(r);
        poll();
        ins(NULL, "B", "@L%d", ltop);
        here(lend);
        tmp_base = saved_base;
        nloops--;
        }
        break;
    }
    case S_PUT:
        comment(s);
        if (s->string)
            select_string(s, s->edit ? 1 : 0);
        else
            select_file(s->file, s->edit ? 1 : 0);
        if (s->page)
            call("PL1PAGE");
        if (s->skip >= 0) {
            arg_const(s->skip);
            call("PL1SKIP");
        }
        if (s->edit)
            format_begin(s);
        gen_stmts(s->body);
        if (s->edit)
            call("PL1FEND");
        if (s->data)
            call("PL1DEND");
        if (s->string)
            call("PL1SEND");
        break;
    case S_GET: {
        int leof = new_label(), lafter = new_label(), saved = cur_eof;
        comment(s);
        if (s->string)
            select_string(s, s->edit ? 3 : 2);
        else
            select_file(s->file, s->edit ? 3 : 2);
        if (s->copy) {
            /* COPY: each field to SYSPRINT (Q1IO), or ANSI's COPY(file) */
            if (s->copyfile) {
                int r = file_desc(s->copyfile);
                arg_reg(r);
                reg_free(r);
            } else {
                arg_const(0);
            }
            call("PL1GCPY");
        }
        if (s->skip >= 0) {
            /* SKIP first, for DATA too (it was left out there: noticed
             * after a GET EDIT that read part of a record, t295cgt) */
            arg_const(s->skip);
            call("PL1GSKP");
        }
        if (s->data) {
            gen_get_data(s);
            ins(NULL, "LTR", "15,15");
            ins(NULL, "BC", "8,@L%d", lafter);
            here(leof);
            eof_dispatch(s->file, lafter);
            here(lafter);
            break;
        }
        if (s->edit)
            format_begin(s);
        cur_eof = leof;
        gen_stmts(s->body);
        cur_eof = saved;
        if (s->edit)
            call("PL1FEND");
        ins(NULL, "B", "@L%d", lafter);
        here(leof);
        if (s->edit)
            call("PL1FEND");
        eof_dispatch(s->file, lafter);
        here(lafter);
        break;
    }
    case S_LOCATE: {
        /* a buffer for the next record, of the variable's size; the
         * pointer to it, at the structure's phase */
        struct sym *x = s->lhs->sym;
        struct expr *pe = s->e3 ? s->e3 : x->based;
        int phase = x->child ? x->phase : 0, d, r, ka = 0, kl = 0;
        struct place pl;
        comment(s);
        if (s->e)
            gen_string(s->e, &ka, &kl);    /* KEYFROM: first, it may call */
        pl = gen_place(pe);
        r = p_take(&pl);
        ins(NULL, "LA", "%d,%s", r, p_rx(&pl, 0));
        p_free_except(&pl, r);
        if (x->child && refer_of(x))
            error_at(s->line, "LOCATE of a structure with REFER is not supported yet");
        d = file_desc(s->file);
        arg_reg(d);
        arg_const(total_size(x));
        arg_const(phase);
        arg_reg(r);
        reg_free(d);
        reg_free(r);
        if (s->e) {
            arg_reg(ka);
            arg_reg(kl);
            reg_free(ka);
            reg_free(kl);
        }
        call(s->e ? "PL1LOCK" : "PL1LOCT");
        break;
    }
    case S_READ:
    case S_WRITE:
    case S_REWRITE:
        if (!s->lhs) {
            int d, lafter = new_label();
            comment(s);
            d = file_desc(s->file);
            if (s->k == S_REWRITE) {
                arg_reg(d);
                reg_free(d);
                call("PL1RWST");
                break;
            }
            {
                int ka = 0, kl = 0, r;
                struct place pl;
                if (s->ignore && !s->e3) {
                    /* READ ... IGNORE(n): records skipped, ENDFILE past the
                     * last (Q1IO) */
                    int n = gen_int(s->ignore);
                    arg_reg(d);
                    arg_reg(n);
                    reg_free(d);
                    reg_free(n);
                    call("PL1RIGN");
                    ins(NULL, "LTR", "15,15");
                    ins(NULL, "BC", "8,@L%d", lafter);
                    eof_dispatch(s->file, lafter);
                    here(lafter);
                    break;
                }
                if (s->e) {
                    reg_free(d);
                    gen_string(s->e, &ka, &kl);   /* KEY: first, it may call */
                    d = file_desc(s->file);
                }
                pl = gen_place(s->e3);
                r = p_take(&pl);
                ins(NULL, "LA", "%d,%s", r, p_rx(&pl, 0));
                p_free_except(&pl, r);
                arg_reg(d);
                arg_reg(r);
                reg_free(d);
                reg_free(r);
                if (s->e) {
                    arg_reg(ka);
                    arg_reg(kl);
                    reg_free(ka);
                    reg_free(kl);
                    call("PL1RSKY");
                    break;
                }
                if (s->e2) {
                    struct place kp = gen_place(s->e2);
                    int kr = p_take(&kp);
                    ins(NULL, "LA", "%d,%s", kr, p_rx(&kp, 0));
                    p_free_except(&kp, kr);
                    arg_reg(kr);
                    arg_const(type_size(s->e2->t));
                    reg_free(kr);
                }
            }
            call(s->e2 ? "PL1RSKT" : "PL1RSET");
            ins(NULL, "LTR", "15,15");
            ins(NULL, "BC", "8,@L%d", lafter);
            eof_dispatch(s->file, lafter);
            here(lafter);
            break;
        }
        /* fall through */
    {
        /* the whole item INTO or FROM: its address and its length */
        struct expr *v = s->lhs;
        struct sym *x = v->sym;
        int len = x->child || v->nsubs < cum_dims(x)
                  ? (v->nsubs ? x->size : total_size(x)) : type_size(x->t);
        int varying = !x->child && x->t.k == TY_VCHAR && v->nsubs == cum_dims(x);
        int lafter = new_label(), leof = new_label();
        if (x->root->child && refer_of(x->root))
            error_at(s->line, "INTO or FROM a structure with REFER is not supported yet");
        struct place p;
        int r, d, ka = 0, kl = 0;
        comment(s);
        if (s->e)
            gen_string(s->e, &ka, &kl);   /* first: it may call routines */
        if (s->e2) {
            /* KEYTO: its address and length after the record's */
            struct place kp = gen_place(s->e2);
            ka = p_take(&kp);
            ins(NULL, "LA", "%d,%s", ka, p_rx(&kp, 0));
            p_free_except(&kp, ka);
        }
        p = gen_place(v);
        r = p_take(&p);
        ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
        p_free_except(&p, r);
        d = file_desc(s->file);
        arg_reg(d);
        arg_reg(r);
        reg_free(d);
        reg_free(r);
        arg_const(varying ? x->t.p : len);
        if (s->e || s->e2) {
            if (varying)
                fatal("%s(%d): keyed record I/O of a VARYING string is not supported yet",
                      src_name, s->line);
            arg_reg(ka);
            reg_free(ka);
            if (s->e) {
                arg_reg(kl);
                reg_free(kl);
            } else {
                arg_const(s->e2->t.p);
            }
            if (s->k != S_READ) {
                call(s->k == S_WRITE ? "PL1WRKY" : "PL1RWKY");
                break;
            }
            if (s->e) {
                call("PL1RDKY");
                break;
            }
            call("PL1RDKT");
            ins(NULL, "LTR", "15,15");
            ins(NULL, "BC", "8,@L%d", lafter);
            here(leof);
            eof_dispatch(s->file, lafter);
            here(lafter);
            break;
        }
        if (s->k == S_WRITE) {
            call(varying ? "PL1WRTV" : "PL1WRIT");
            break;
        }
        if (s->k == S_REWRITE) {
            call(varying ? "PL1RWRV" : "PL1RWRT");
            break;
        }
        call(varying ? "PL1READV" : "PL1READ");
        ins(NULL, "LTR", "15,15");
        ins(NULL, "BC", "8,@L%d", lafter);
        here(leof);
        eof_dispatch(s->file, lafter);
        here(lafter);
        break;
    }
    case S_DELETE: {
        int ka, kl, d;
        comment(s);
        if (!s->e) {
            /* the record read last (SEQUENTIAL UPDATE) */
            d = file_desc(s->file);
            arg_reg(d);
            reg_free(d);
            call("PL1DLSQ");
            break;
        }
        gen_string(s->e, &ka, &kl);
        d = file_desc(s->file);
        arg_reg(d);
        arg_reg(ka);
        arg_reg(kl);
        reg_free(d);
        reg_free(ka);
        reg_free(kl);
        call("PL1DLKY");
        break;
    }
    case S_ITEM:
        if (s->get && s->cxpair == 1) {
            /* a complex target's parts follow (rt/pl1ioget.c); the reader
             * of complex constants on (rt/pl1cxsp.c) */
            ins(NULL, "L", "15,=V(PL1CXON)");
            ins(NULL, "BALR", "14,15");
            ins(NULL, "L", "15,=V(PL1GCPX)");
            ins(NULL, "BALR", "14,15");
        }
        if (s->get)
            gen_get_item(s->lhs);
        else
        {
            if (s->owner_io && s->owner_io->data)
                data_name(s->e);
            if (s->cxpair == 1) {
                /* a complex value's parts follow (rt/pl1ioed.c) */
                ins(NULL, "L", "15,=V(PL1ECPX)");
                ins(NULL, "BALR", "14,15");
            }
            gen_put_item(s->e);
        }
        if (uses_count && s->cxpair != 2) {
            /* COUNT: one more item of this GET or PUT (rt/pl1io.c) */
            ins(NULL, "L", "15,=V(PL1ICNT)");
            ins(NULL, "LA", "0,1");
            ins(NULL, "A", "0,0(,15)");
            ins(NULL, "ST", "0,0(,15)");
        }
        break;
    case S_ON: {
        int i = est_index(s), q, r = reg_get();
        comment(s);
        if (s->cond == C_ATTENTION)
            ins(NULL, "L", "15,=V(PL1ATTN)");   /* the run time's (rt/pl1abi.h) */
        ins(NULL, "LA", "%d,%d(,13)", r, ests[i].off);
        q = qual_reg(s);
        arg_reg(r);
        arg_const(s->cond | (s->snap ? 0x10000 : 0));
        arg_reg(q);
        if (s->unit)
            ins(NULL, "L", "%d,=A(@P%d)", r, s->unit->id);
        else
            ins(NULL, "SR", "%d,%d", r, r);       /* SYSTEM */
        arg_reg(r);
        arg_reg(13);
        reg_free(r);
        reg_free(q);
        call("PL1ON");
        break;
    }
    case S_BEGIN:
        /* the block, with this frame as its static link */
        comment(s);
        ins(NULL, "ST", "13,%d(,13)", ARGS);
        ins(NULL, "LA", "1,%d(,13)", ARGS);
        ins(NULL, "L", "15,=A(@P%d)", s->unit->id);
        ins(NULL, "BALR", "14,15");
        break;
    case S_STOP:
        comment(s);
        call("PL1STOP");
        break;
    case S_WAIT: {
        /* PL1WAIT(A(list), entries, count): per event its address and
         * the number of events there, an array's elements in a row
         * (rt/pl1ev.c); the count -1 for all */
        int lst = tmp(8 * s->nmore, 4), r;
        comment(s);
        for (int i = 0; i < s->nmore; i++) {
            struct expr *x = s->more[i];
            struct place p = gen_place(x);
            int n = 1;
            r = p_take(&p);
            ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
            p_free_except(&p, r);
            ins(NULL, "ST", "%d,%d(,13)", r, lst + 8 * i);
            reg_free(r);
            if (x->nsubs == 0)                  /* a whole array (check.c) */
                for (int d = 0; d < x->sym->ndims; d++)
                    n *= x->sym->hb[d] - x->sym->lb[d] + 1;
            ins(NULL, "MVC", "%d(4,13),=F'%d'", lst + 8 * i + 4, n ? n : 1);
        }
        {
            /* the count first: computing it may call the run time */
            int c = s->e ? gen_int(s->e) : -1;
            r = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", r, lst);
            arg_reg(r);
            reg_free(r);
            arg_const(s->nmore);
            if (s->e) {
                arg_reg(c);
                reg_free(c);
            } else {
                arg_const(-1);
            }
        }
        call("PL1WAIT");
        break;
    }
    case S_DELAY: {
        int r;
        comment(s);
        r = gen_int(s->e);
        arg_reg(r);
        reg_free(r);
        call("PL1DLAY");
        break;
    }
    case S_DISPLAY: {
        /* PL1DSP(A(message), length, A(reply) or 0, its length, VARYING,
         * A(event) or 0) */
        int ra, rl, re = 0;
        comment(s);
        gen_string(s->e, &ra, &rl);
        if (s->e3) {
            struct place p = gen_place(s->e3);
            re = p_take(&p);
            ins(NULL, "LA", "%d,%s", re, p_rx(&p, 0));
            p_free_except(&p, re);
        }
        if (s->e2) {
            /* the reply's address before the arguments are stored: its
             * subscripts may call the run time */
            struct place p = gen_place(s->e2);
            int r = p_take(&p);
            ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
            p_free_except(&p, r);
            arg_reg(ra);
            arg_reg(rl);
            reg_free(ra);
            reg_free(rl);
            arg_reg(r);
            reg_free(r);
            arg_const(s->e2->t.p);
            arg_const(s->e2->t.k == TY_VCHAR);
        } else {
            arg_reg(ra);
            arg_reg(rl);
            reg_free(ra);
            reg_free(rl);
            arg_const(0);
            arg_const(0);
            arg_const(0);
        }
        if (re) {
            arg_reg(re);
            reg_free(re);
        } else {
            arg_const(0);
        }
        call("PL1DSP");
        break;
    }
    case S_ALLOC:
    case S_FREE:
        comment(s);
        for (struct put_item *it = s->items; it; it = it->next) {
            struct sym *x = it->e->sym;
            struct expr *pe = it->e->qual ? it->e->qual : x->based;
            /* a structure starts at its phase from a doubleword, as in a
             * frame; the run time's storage is doubleword aligned */
            int phase = x->child ? x->phase : 0;
            if (x->st == ST_CTL) {
                /* a generation: A(the one before), a word, the data */
                int a = reg_get(), b = reg_get();
                ins(NULL, "L", "%d,=A(@V%d)", a, x->label);
                if (s->k == S_ALLOC) {
                    arg_const(8 + total_size(x) + phase);
                    call("PL1ALOC");
                    ins(NULL, "MVC", "0(4,15),0(%d)", a);
                    ins(NULL, "ST", "15,0(,%d)", a);
                    {
                        int g = reg_get();
                        ins(NULL, "LR", "%d,15", g);
                        areas_init(x, g, 8 + phase);
                        reg_free(g);
                    }
                } else {
                    ins(NULL, "L", "%d,0(,%d)", b, a);
                    ins(NULL, "MVC", "0(4,%d),0(%d)", a, b);
                    arg_reg(b);
                    call("PL1FREE");
                }
                reg_free(a);
                reg_free(b);
                continue;
            }
            if (s->k == S_ALLOC) {
                struct place p;
                int r, n = 0;
                struct sym *rf = x->child ? refer_of(x) : NULL;
                if (rf && it->in)
                    error_at(s->line, "a structure with REFER in an area is not supported yet");
                if (rf) {
                    /* REFER: the units beyond the one laid out, x - base
                     * of them (Q1REF); x goes into the member after */
                    int base = rf->refer_dim < 0 ? 1 : rf->lb[rf->refer_dim], sz = reg_get();
                    n = gen_int(rf->refer_from);
                    ins(NULL, "LR", "%d,%d", sz, n);
                    if (base)
                        ins(NULL, "S", "%d,=F'%d'", sz, base);
                    if (rf->refer_unit != 1)
                        ins(NULL, "MH", "%d,=H'%d'", sz, rf->refer_unit);
                    ins(NULL, "A", "%d,=F'%d'", sz, total_size(x) + phase);
                    arg_reg(sz);
                    reg_free(sz);
                    call("PL1ALOC");
                } else if (it->in) {
                    /* in an area: after a normal return from an AREA
                     * on-unit the allocation is made again, in the area
                     * its reference now designates (GC28-8201-4) */
                    int again = new_label(), a;
                    here(again);
                    a = area_addr(it->in);
                    arg_reg(a);
                    reg_free(a);
                    arg_const(total_size(x) + phase);
                    call("PL1AALC");
                    ins(NULL, "LTR", "15,15");
                    ins(NULL, "BC", "8,@L%d", again);
                } else {
                    arg_const(total_size(x) + phase);
                    call("PL1ALOC");
                }
                r = reg_get();
                ins(NULL, "LR", "%d,15", r);  /* the place of p may need calls */
                areas_init(x, r, phase);
                p = gen_place(pe);
                if (phase)
                    ins(NULL, "LA", "%d,%d(,%d)", r, phase, r);
                ins(NULL, "ST", "%d,%s", r, p_rx(&p, 0));
                p_free(&p);
                if (rf) {
                    ins(NULL, rf->refer->t.p <= 15 ? "STH" : "ST", "%d,%d(,%d)", n,
                        root_off(rf->refer), r);
                    reg_free(n);
                }
                reg_free(r);
            } else if (it->in) {
                int a = area_addr(it->in), r = gen_ptr(pe);
                if (phase)
                    ins(NULL, "S", "%d,=F'%d'", r, phase);
                arg_reg(a);
                arg_reg(r);
                reg_free(a);
                reg_free(r);
                arg_const(total_size(x) + phase);
                call("PL1AFRE");
            } else {
                int r = gen_ptr(pe);
                if (phase)
                    ins(NULL, "S", "%d,=F'%d'", r, phase);
                arg_reg(r);
                reg_free(r);
                call("PL1FREE");
            }
        }
        break;
    case S_REVERT: {
        /* only what this block established can be reverted */
        int i = est_index(s);
        comment(s);
        if (i >= 0) {
            int r = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", r, ests[i].off);
            arg_reg(r);
            reg_free(r);
            call("PL1REVT");
        }
        break;
    }
    case S_SIGNAL: {
        int q, bit = s->cond == C_SIZE ? EN_SIZE : s->cond == C_SUBRG ? EN_SUBRG :
                     s->cond == C_STRG ? EN_STRG : s->cond == C_ZERODIVIDE ? EN_ZDIV :
                     s->cond == C_FIXEDOVERFLOW ? EN_FOFL : s->cond == C_CONVERSION ? EN_CONV :
                     s->cond == C_UNDERFLOW ? EN_UFL : s->cond == C_OVERFLOW ? EN_OFL : 0;
        comment(s);
        if (bit && !(cur_mask & bit))
            break;            /* disabled: a null statement (Q5SIG, JOB00804) */
        if (s->cond == C_ATTENTION)
            ins(NULL, "L", "15,=V(PL1ATTN)");   /* the run time's (rt/pl1abi.h) */
        q = qual_reg(s);
        arg_const(s->cond);
        arg_reg(q);
        reg_free(q);
        arg_const(signal_code(s->cond));
        call("PL1SIG");
        break;
    }
    case S_OPEN:
    case S_CLOSE:
        comment(s);
        for (int i = 0; i < s->nfiles; i++) {
            int r = file_desc(s->files[i].file);
            arg_reg(r);
            reg_free(r);
            if (s->k == S_OPEN) {
                arg_const(s->files[i].attrs);
                if (s->files[i].title) {
                    int t = reg_get();
                    int len = (int)strlen(s->files[i].title);
                    ins(NULL, "L", "%d,=A(@L%d)", t, str_label(s->files[i].title, len));
                    arg_reg(t);
                    reg_free(t);
                    arg_const(len);
                } else {
                    arg_const(0);
                    arg_const(0);
                }
                call("PL1OPEN");
                if (s->files[i].linesize || s->files[i].pagesize || s->files[i].ntabs) {
                    /* LINESIZE(e), PAGESIZE(e), ANSI's TAB(e, ...): set on
                     * the file now open (Q1LSZ, Tier 72) */
                    struct fopen *fo = &s->files[i];
                    int t = tmp(4 * 16, 4), ls, ps, d, ta;
                    for (int k = 0; k < fo->ntabs; k++) {
                        int v = gen_int(fo->tabs[k]);
                        ins(NULL, "ST", "%d,%d(,13)", v, t + 4 * k);
                        reg_free(v);
                    }
                    if (fo->linesize) {
                        ls = gen_int(fo->linesize);
                    } else {
                        ls = reg_get();
                        load_const(ls, 0);
                    }
                    if (fo->pagesize) {
                        ps = gen_int(fo->pagesize);
                    } else {
                        ps = reg_get();
                        load_const(ps, 0);
                    }
                    d = file_desc(fo->file);
                    ta = reg_get();
                    ins(NULL, "LA", "%d,%d(,13)", ta, t);
                    arg_reg(d);
                    arg_reg(ls);
                    arg_reg(ps);
                    arg_reg(ta);
                    arg_const(fo->ntabs);
                    reg_free(d);
                    reg_free(ls);
                    reg_free(ps);
                    reg_free(ta);
                    call("PL1OPTS");
                }
            } else {
                call("PL1CLOS");
            }
        }
        break;
    case S_CALL:
        comment(s);
        if (s->e->k == E_FCALL && s->e->callee && s->e->callee->asm_opt) {
            gen_asmcall(s->e);
            break;
        }
        if (s->e->k == E_CALL && s->e->fn == B_PLIRETC) {
            /* the return code, 999 at most (rt/pl1cond.c) */
            int r = gen_int(s->e->arg[0]);
            arg_reg(r);
            reg_free(r);
            call("PL1RETC");
            break;
        }
        if (s->e->k == E_CALL && s->e->fn == B_PLIDUMP) {
            /* PL1DUMP(A(options), length, A(identification), length):
             * omitted, a length of -1 (rt/pl1dump.c) */
            int a[2], l[2];
            for (int i = 0; i < 2; i++)
                if (i < s->e->nargs)
                    gen_string(s->e->arg[i], &a[i], &l[i]);
            for (int i = 0; i < 2; i++)
                if (i < s->e->nargs) {
                    arg_reg(a[i]);
                    arg_reg(l[i]);
                    reg_free(a[i]);
                    reg_free(l[i]);
                } else {
                    arg_const(0);
                    arg_const(-1);
                }
            call("PL1DUMP");
            break;
        }
        if (s->e->k == E_CALL && s->e->fn >= B_PLISRTA && s->e->fn <= B_PLISRTD) {
            gen_sort(s->e);
            break;
        }
        if (s->e->k == E_CALL && s->e->fn == B_PLICKPT) {
            /* PL1CKPT(A(block)): A(ddname), length, A(check-id), length,
             * A(org), length, A(code); omitted, a length of -1 and A 0
             * (rt/pl1ckpt.asm) */
            int a, l, r, blk = tmp(28, 4);
            for (int i = 0; i < 3; i++)
                if (i < s->e->nargs) {
                    gen_string(s->e->arg[i], &a, &l);
                    ins(NULL, "ST", "%d,%d(,13)", a, blk + 8 * i);
                    ins(NULL, "ST", "%d,%d(,13)", l, blk + 8 * i + 4);
                    reg_free(a);
                    reg_free(l);
                } else {
                    ins(NULL, "XC", "%d(4,13),%d(13)", blk + 8 * i, blk + 8 * i);
                    ins(NULL, "MVC", "%d(4,13),=F'-1'", blk + 8 * i + 4);
                }
            if (s->e->nargs == 4) {
                struct place p = gen_place(s->e->arg[3]);
                r = p_take(&p);
                ins(NULL, "LA", "%d,%s", r, p_rx(&p, 0));
                p_free_except(&p, r);
                ins(NULL, "ST", "%d,%d(,13)", r, blk + 24);
                reg_free(r);
            } else {
                ins(NULL, "XC", "%d(4,13),%d(13)", blk + 24, blk + 24);
            }
            r = reg_get();
            ins(NULL, "LA", "%d,%d(,13)", r, blk);
            arg_reg(r);
            reg_free(r);
            call("PL1CKPT");
            break;
        }
        if (s->e->k == E_CALL && (s->e->fn == B_PLIREST || s->e->fn == B_PLICANC)) {
            call(s->e->fn == B_PLIREST ? "PL1REST" : "PL1CANC");
            break;
        }
        gen_fcall(s->e);
        break;
    case S_RETURN:
        comment(s);
        if (s->e && CURP->ret_multi) {
            /* the attributes of the entry point entered: its number at
             * entoff (gen_proc) */
            for (int k = 0; k <= CURP->nentries; k++) {
                struct target t = { TG_RESULT, NULL, 0 };
                int next = new_label();
                if (!s->rets[k])
                    continue;
                ins(NULL, "CLC", "%d(4,13),=F'%d'", CURP->entoff, k);
                ins(NULL, "BC", "7,@L%d", next);
                tmp_top = tmp_base;
                gen_store(&t, k ? CURP->entries[k - 1].ret : CURP->ret, s->rets[k]);
                ins(NULL, "B", "@R%d", CURP->id);
                here(next);
            }
        } else if (s->e) {
            struct target t = { TG_RESULT, NULL, 0 };
            gen_store(&t, CURP->ret, s->e);
        }
        ins(NULL, "B", "@R%d", CURP->id);
        break;
    case S_ENTRY:
        comment(s);
        here(CURP->entries[s->goto_id - 1].label);
        break;
    case S_GOTO:
        comment(s);
        poll();
        if (s->e) {
            /* a label variable: the blocks above its frame end, then its
             * stub sets the bases of the label's block (gen_proc) */
            int v = gen_labelval(s->e), f = reg_get();
            ins(NULL, "L", "%d,%d(,13)", f, v + 4);
            arg_reg(f);
            reg_free(f);
            call("PL1GOTO");
            ins(NULL, "L", "15,%d(,13)", v);
            ins(NULL, "L", "13,%d(,13)", v + 4);
            ins(NULL, "BR", "15");
        } else if (s->goto_proc == CURP) {
            ins(NULL, "B", "@G%d", s->goto_id);
        } else {
            /* out of this block: the blocks above the label's end, and
             * the label's block runs on with its frame and base */
            int f = frame_reg(s->goto_proc);
            arg_reg(f);
            call("PL1GOTO");
            ins(NULL, "L", "15,=A(@G%d)", s->goto_id);
            ins(NULL, "L", "12,=A(@@FEN%d+2)", s->goto_proc->id);
            ins(NULL, "LA", "11,4095(,12)");
            ins(NULL, "LA", "11,1(,11)");
            ins(NULL, "LA", "10,4095(,11)");
            ins(NULL, "LA", "10,1(,10)");
            ins(NULL, "LR", "13,%d", f);
            ins(NULL, "BR", "15");
            reg_free(f);
        }
        break;
    }
}

static void gen_stmts(struct stmt *s)
{
    for (; s; s = s->next)
        gen_stmt(s);
}

/* ---- the program ---------------------------------------------------- */

static int total_size(struct sym *s)
{
    int n = s->size;
    if (s->tbits)
        return (s->tbits + 7) / 8;      /* packed bits (lower.c) */
    for (int i = 0; i < s->ndims; i++)
        n *= dim_extent(s, i);
    return n - s->tail;
}



/* Labels for the STATIC items of every procedure, before any code: a
 * procedure may use an enclosing one's. */
/* EXTERNAL variables and files (files are EXTERNAL by default): one
 * label for the name in every external procedure of the source.  As
 * PL/I (F) maps them (Q2EXT, JOB00781): an arithmetic scalar without
 * INITIAL is a common area, every other declaration a control section of
 * its own -- a string or aggregate without INITIAL one of zeros -- and
 * the first control section in the order of the source holds the
 * storage, so a CHAR without INITIAL in the main procedure wins over a
 * later INITIAL('...') */
#define MAXEXT 512
static struct { const char *name; int label, file; struct sym *owner; } exts[MAXEXT];
static int nexts;

static int ext_common(struct sym *s)
{
    return !s->ninits && !s->init && !s->child && !s->ndims && arith(s->t);
}

static int ext_label(struct sym *s, int file)
{
    for (int i = 0; i < nexts; i++)
        if (exts[i].file == file && strcmp(exts[i].name, s->name) == 0) {
            if (!file && ext_common(exts[i].owner) && !ext_common(s))
                exts[i].owner = s;        /* the first control section */
            return exts[i].label;
        }
    if (nexts == MAXEXT)
        fatal("%s: more than %d EXTERNAL names", src_name, MAXEXT);
    exts[nexts].name = s->name;
    exts[nexts].file = file;
    exts[nexts].owner = s;
    exts[nexts].label = file ? ++nfile : ++nstatic;
    return exts[nexts++].label;
}

static int ext_owner(struct sym *s, int file)
{
    for (int i = 0; i < nexts; i++)
        if (exts[i].file == file && strcmp(exts[i].name, s->name) == 0)
            return exts[i].owner == s;
    return 1;
}

/* not an ENTRY or FILE constant: data of its own */
static int const_ef(struct sym *s)
{
    return !((s->is_entry || s->is_file) && !s->var_ef);
}

static void label_statics(struct proc *p)
{
    for (struct sym *s = p->syms; s; s = s->next) {
        if (s->is_file && !s->var_ef)
            s->file_label = ext_label(s, 1);
        else if (s->root == s && s->external && s->st == ST_STATIC && const_ef(s))
            s->label = ext_label(s, 0);
        else if (s->root == s && (s->st == ST_STATIC || s->st == ST_CTL) && const_ef(s))
            s->label = ++nstatic;         /* CONTROLLED: its anchor */
    }
    for (struct proc *c = p->child; c; c = c->sibling)
        label_statics(c);
}

/* Where the variable s goes from off on: its offset, the end returned. */
static int place_var(struct sym *s, int off)
{
    if (s->child) {
        /* a structure begins at its phase from a doubleword (the frame
         * is doubleword aligned), as PL/I (F) maps it */
        while (off % 8 != s->phase)
            off++;
    } else {
        off = align(off, s->align);
    }
    s->off = off;
    if (s->adjust)
        return off + 8 + 8 * s->ndims;     /* its descriptor (S_ADJUST) */
    return off + total_size(s);
}

static int auto_var(struct sym *s)
{
    return !(s->root != s || !const_ef(s) || s->param || s->st == ST_STATIC ||
             s->st == ST_BASED || s->st == ST_CTL || s->defined);   /* DEFINED: its base's */
}

static void layout(struct proc *p)
{
    int off = VARS, high = 0;
    for (struct sym *s = p->syms; s; s = s->next)
        if (auto_var(s) && s->adjust)
            off = place_var(s, align(off, 4));  /* low: S_ADJUST reaches it */
    for (struct sym *s = p->syms; s; s = s->next) {
        if (!auto_var(s) || s->adjust)
            continue;
        if (s->size == 0) {           /* a loop variable made by lower.c */
            s->size = 4;
            s->align = 4;
        }
        off = place_var(s, off);
    }
    if (off > 3072) {
        /* More than a displacement reaches with the temporaries: the
         * variables that do not fit below 3072 go from 4096 on, above the
         * temporaries, and are reached through a register (gen_place) */
        off = VARS;
        high = 4096;
        for (struct sym *s = p->syms; s; s = s->next)
            if (auto_var(s) && s->adjust)
                off = place_var(s, align(off, 4));
        for (struct sym *s = p->syms; s; s = s->next) {
            int end;
            if (!auto_var(s) || s->adjust)
                continue;
            end = place_var(s, off);
            if (end <= 3072)
                off = end;
            else
                high = place_var(s, high);
        }
    }
    high_end = high;
    /* the establishment records of the ON statements */
    for (int i = 0; i < nests; i++) {
        off = align(off, 4);
        ests[i].off = off;
        off += EST_SIZE;
    }
    if (p->nentries) {
        /* secondary entries: the entry number, then the canonical
         * argument list (static link, result, every parameter) */
        off = align(off, 4);
        p->entoff = off;
        off += 4 + 4 * (2 + p->nparams);
    }
    vars_end = off;
    tmp_base = tmp_top = tmp_hwm = align(off, 8);
}

/* DC for one INITIAL value of an elementary item of type t. */
static void dc_value(struct type t, struct expr *v)
{
    if ((t.k == TY_PTR || t.k == TY_OFFSET) && v->k == E_CALL) {
        /* a STATIC locator's INITIAL (check.c takes these only) */
        if (v->fn == B_SYSNULL)
            out("         DC    F'0'");
        else if (v->fn == B_PTRVALUE) {
            struct expr *n = v->arg[0]->k == E_NEG ? v->arg[0]->l : v->arg[0];
            out("         DC    F'%s%s'", v->arg[0]->k == E_NEG ? "-" : "", n->num);
        } else
            out("         DC    X'FF000000'");
        return;
    }
    switch (t.k) {
    case TY_FIXBIN: {
        int n = 0;
        const_bin(v, t.q, &n);          /* a scaled binary at its scale */
        out("         DC    %s'%d'", t.p <= 15 ? "H" : "F", n);
        break;
    }
    case TY_FIXDEC: {
        struct expr *n = v->k == E_NEG ? v->l : v;
        char digits[40];
        int shift = t.q - n->t.q;
        snprintf(digits, sizeof digits, "%s", n->num);
        if (shift < 0)
            digits[strlen(digits) + (size_t)shift] = '\0';
        for (; shift > 0; shift--)
            strcat(digits, "0");
        out("         DC    PL%d'%s%s'", dec_bytes(t.p), v->k == E_NEG ? "-" : "",
            digits[0] ? digits : "0");
        break;
    }
    case TY_CHAR:
    case TY_VCHAR: {
        int n = v->len < t.p ? v->len : t.p;
        char text[600];
        int k = 0;
        if (t.k == TY_VCHAR)
            out("         DC    H'%d'", n);
        if (t.p > 256)
            fatal("%s: INITIAL strings over 256 characters are not supported yet", src_name);
        for (int i = 0; i < n; i++) {
            if (v->str[i] == '\'' || v->str[i] == '&')
                text[k++] = v->str[i];
            text[k++] = v->str[i];
        }
        text[k] = '\0';
        if (n == 0)
            out("         DC    CL%d' '", t.p);
        else
            out("         DC    CL%d'%s'", t.p, text);
        break;
    }
    case TY_FLTDEC:
    case TY_FLTBIN: {
        unsigned char b[8];
        char h[20];
        int n = flt_long(t) ? 8 : 4;
        flt_constant(v, flt_long(t), b);
        for (int i = 0; i < n; i++)
            snprintf(h + 2 * i, 3, "%02X", b[i]);
        out("         DC    XL%d'%s'", n, h);
        break;
    }
    case TY_BIT: {
        unsigned int b = 0;
        int nb = (t.p + 7) / 8;
        for (int i = 0; i < v->len && i < t.p; i++)
            if (v->str[i] == '1')
                b |= 0x80000000u >> i;
        out("         DC    XL%d'%0*X'", nb, 2 * nb, b >> (32 - 8 * nb));
        break;
    }
    default:
        break;
    }
}

/* Offset of the element idx[] of item s inside its root. */
static int elem_offset(struct sym *s, const int *idx)
{
    struct sym *chain[32];
    int n = 0, k = 0, off = 0;
    for (struct sym *x = s; x; x = x->parent)
        chain[n++] = x;
    for (int i = n - 1; i >= 0; i--) {
        struct sym *x = chain[i];
        int stride = x->size, strides[MAXDIMS];
        off += x->moff;
        for (int d = x->ndims - 1; d >= 0; d--) {
            strides[d] = stride;
            stride *= dim_extent(x, d);
        }
        for (int d = 0; d < x->ndims; d++, k++)
            off += (idx[k] - x->lb[d]) * strides[d];
    }
    return off;
}

/* The bit offset of the element idx[] of a packed bit string s inside
 * its root, as gen_place() computes it (moff and mbit, bstride). */
static int elem_bitoff(struct sym *s, const int *idx)
{
    struct sym *chain[32];
    int n = 0, k = 0, off = 0;
    for (struct sym *x = s; x; x = x->parent)
        chain[n++] = x;
    for (int i = n - 1; i >= 0; i--) {
        struct sym *x = chain[i];
        int stride = x->bstride, strides[MAXDIMS];
        if (x->parent)                  /* as gen_place() finds it */
            off += 8 * x->moff + x->mbit;
        for (int d = x->ndims - 1; d >= 0; d--) {
            strides[d] = stride;
            stride *= dim_extent(x, d);
        }
        for (int d = 0; d < x->ndims; d++, k++)
            off += (idx[k] - x->lb[d]) * strides[d];
    }
    return off;
}

/* The INITIAL bits of packed bit strings in the STATIC root being
 * emitted: they share bytes, so they are gathered here and written as
 * whole bytes once the root's members are done (static_members). */
static unsigned char *pk_img, *pk_used;
static int pk_len;

static void pk_bits(struct sym *s, const int *idx, struct expr *v)
{
    int b = elem_bitoff(s, idx);
    for (int i = 0; i < s->t.p; i++, b++) {
        if (b / 8 >= pk_len)
            break;
        pk_used[b / 8] = 1;
        if (i < v->len && v->str[i] == '1')
            pk_img[b / 8] |= (unsigned char)(0x80 >> (b % 8));
    }
}

/* INITIAL values of item s (elementary, in a STATIC root), placed at
 * their offsets with ORG. */
/* a STATIC INITIAL value; of a complex variable its two parts
 * (cplx_init, check.c) */
static void dc_init(struct type t, struct expr *v)
{
    struct expr *re, *im;
    if (is_cplx(t) && cplx_init(v, &re, &im)) {
        dc_value(part_type(t), re);
        dc_value(part_type(t), im);
        return;
    }
    dc_value(t, v);
}

static void static_inits(struct sym *s, const char *label)
{
    int lb[MAXDIMS * 4], hb[MAXDIMS * 4], idx[MAXDIMS * 4], n = 0, total = 1, item = 0, rep = 0;
    struct sym *chain[32];
    int nc = 0;

    if (!s->init && !s->ninits)
        return;
    for (struct sym *x = s; x; x = x->parent)
        chain[nc++] = x;
    for (int i = nc - 1; i >= 0; i--)
        for (int d = 0; d < chain[i]->ndims; d++) {
            lb[n] = chain[i]->lb[d];
            hb[n] = chain[i]->hb[d];
            n++;
        }
    for (int i = 0; i < n; i++) {
        total *= hb[i] - lb[i] + 1;
        idx[i] = lb[i];
    }
    if (n == 0) {
        struct expr *v = s->init ? s->init : s->inits[0].e;
        if (v && s->packed) {
            pk_bits(s, idx, v);
        } else if (v) {
            out("         ORG   %s+%d", label, elem_offset(s, idx));
            dc_init(s->t, v);
        }
        return;
    }
    for (int el = 0; el < total && item < s->ninits; el++) {
        if (s->inits[item].e && s->packed) {
            pk_bits(s, idx, s->inits[item].e);
        } else if (s->inits[item].e) {  /* '*': nothing there (Q1INIA) */
            out("         ORG   %s+%d", label, elem_offset(s, idx));
            dc_init(s->t, s->inits[item].e);
        }
        if (++rep == s->inits[item].rep) {
            rep = 0;
            item++;
        }
        for (int i = n - 1; i >= 0; i--) {
            if (++idx[i] <= hb[i])
                break;
            idx[i] = lb[i];
        }
    }
}

static void static_members(struct sym *s, const char *label)
{
    if (!s->child) {
        static_inits(s, label);
        return;
    }
    for (struct sym *c = s->child; c; c = c->sibling)
        static_members(c, label);
}

/* gen_static_data: 0 the private section's data, 1 the named sections of
 * EXTERNAL variables and files, written after everything else */
static int static_named;

/* The DCB a file's ENVIRONMENT gives (#215): RECFM as DCBRECFM's bits,
 * LRECL and BLKSIZE; all 0 without a record format.  Measured on PL/I
 * (F) (JOB01874): F(41) of a PRINT file is FA
 * 41/41, V(64,60) VB 60/64, F(800,80) FB 80/800, U(100) U 100/100. */
static void env_dcb(const struct sym *s, int *recfm, int *lrecl, int *blk)
{
    const char *f = s->env_recfm;
    *recfm = *lrecl = *blk = 0;
    if (!f[0])
        return;
    *blk = s->env_blk;
    if (f[0] == 'U') {
        *recfm = PL1_RECFM_U;
        *lrecl = s->env_blk;
    } else if (f[0] == 'F') {
        *lrecl = s->env_rec ? s->env_rec : s->env_blk;
        *recfm = PL1_RECFM_F | (*lrecl < *blk ? PL1_RECFM_B : 0);
    } else {
        /* V, VS, VBS; V with a record size is VB (V(64,60)) */
        *lrecl = s->env_rec ? s->env_rec : s->env_blk - 4;
        *recfm = PL1_RECFM_V;
        if (strcmp(f, "VBS") == 0)
            *recfm |= PL1_RECFM_B | PL1_RECFM_S;
        else if (strcmp(f, "VS") == 0)
            *recfm |= PL1_RECFM_S;
        else if (s->env_rec)
            *recfm |= PL1_RECFM_B;
    }
    if ((s->is_file & FL_PRINT) || (s->env_opts & EO_CTLASA))
        *recfm |= PL1_RECFM_A;
    else if (s->env_opts & EO_CTL360)
        *recfm |= PL1_RECFM_M;
}

static void gen_static_data(struct proc *p)
{
    for (struct proc *c = p->child; c; c = c->sibling)
        gen_static_data(c);
    for (struct sym *s = p->syms; s; s = s->next) {
        char name[9];
        if (!s->is_file || s->var_ef || !ext_owner(s, 1) || !static_named)
            continue;
        snprintf(name, sizeof name, "%.8s", s->name);
        /* a section of its own, the same in every external procedure */
        out("%-8s CSECT", ext_sect(s, 1));
        out("         DS    0F");
        /* the descriptor (rt/pl1iof.h): the DD name, the attributes, the
         * run time's state, and for a KEYED file its hook: so only a
         * program with REGIONAL or INDEXED files links rt/pl1iokey.c */
        int recfm, lrecl, blk;
        env_dcb(s, &recfm, &lrecl, &blk);
        if (s->is_file & (FL_REGIONAL | FL_INDEXED))
            out("         DC    CL8'%s',F'%d',A(0),V(PL1KHOK),F'%d,%d,%d'", name, s->is_file,
                recfm, lrecl, blk);
        else
            out("         DC    CL8'%s',F'%d',A(0),F'0,%d,%d,%d'", name, s->is_file, recfm,
                lrecl, blk);
    }
    for (struct sym *s = p->syms; s; s = s->next)
        if (s->root == s && s->st == ST_CTL && const_ef(s) && !static_named) {
            /* the anchor of a CONTROLLED variable: its latest generation */
            out("         DS    0F");
            out("@V%-6d DC    A(0)", s->label);
        }
    for (struct sym *s = p->syms; s; s = s->next) {
        char label[16];
        int size;
        if (s->root != s || s->st != ST_STATIC || !const_ef(s))
            continue;
        if (s->external && !ext_owner(s, 0))
            continue;                     /* another declaration holds it */
        if ((s->external != 0) != (static_named != 0))
            continue;                     /* the other pass's */
        size = total_size(s);
        if (s->external && ext_common(s)) {
            /* an arithmetic scalar without INITIAL: a common area (Q2EXT) */
            out("%-8s COM", ext_sect(s, 0));
            out("         DS    0D");
            out("         DS    XL%d", size);
            continue;
        }
        if (s->external) {
            /* a section of its own, the data at its phase */
            out("%-8s CSECT", ext_sect(s, 0));
            out("         DS    0D");
            if (s->child && s->phase)
                out("         DS    XL%d", s->phase);
            snprintf(label, sizeof label, "%s+%d", ext_sect(s, 0), s->child ? s->phase : 0);
            out("         DC    %dX'00'", size);
        } else {
        snprintf(label, sizeof label, "@V%d", s->label);
        out("         DS    0D");
        if (s->child && s->phase)
            out("         DS    XL%d", s->phase);
        out("%-8s DC    %dX'00'", label, size);
        }
        find_areas(s);
        for (int i = 0; i < nareas; i++) {
            out("         ORG   %s+%d", label, area_off[i]);
            out("         DC    F'%d'", area_size[i]);
        }
        pk_len = size;
        pk_img = calloc((size_t)size + 1, 1);
        pk_used = calloc((size_t)size + 1, 1);
        if (!pk_img || !pk_used)
            fatal("out of memory");
        static_members(s, label);
        for (int i = 0; i < size; i++)
            if (pk_used[i]) {
                out("         ORG   %s+%d", label, i);
                out("         DC    X'%02X'", pk_img[i]);
            }
        free(pk_img);
        free(pk_used);
        pk_img = pk_used = NULL;
        out("         ORG   %s+%d", label, size);
    }
}

/* One procedure: prologue, body, epilogue, its literal pool. */
static void gen_proc(struct proc *p, FILE *f)
{
    int frame;

    CURP = p;
    blen = 0;
    code_bytes = 120;                     /* the prologue and epilogue */
    lit_bytes = 0;
    for (int i = 0; i < nlits; i++)
        free(lits[i]);
    nlits = 0;
    nests = 0;
    need_zd = need_fofl = need_ufl = need_size = need_subrg = 0;
    cur_mask = p->mask & EN_SET ? p->mask : EN_DEFAULT;
    collect_ons(p->body);
    layout(p);
    for (int i = 0; i < p->nentries; i++)
        p->entries[i].label = new_label();
    for (int i = 0; i < nests; i++)
        out("         XC    %d(%d,13),%d(13)", ests[i].off, EST_SIZE, ests[i].off);
    /* AUTOMATIC structures start out zero: PL/I (F)'s padding bytes are 0
     * in the records it writes (JOB00123), ours would be what the stack
     * held; and so do events, incomplete with status 0 (#3) */
    for (struct sym *x = p->syms; x; x = x->next) {
        int o, n;
        if (x->root != x || !(x->child || x->t.k == TY_EVENT) || x->st != ST_AUTO ||
            x->param)
            continue;
        if (x->off + total_size(x) > 4095) {
            /* above a displacement: R1 addresses it */
            out("         L     1,=A(%d)", x->off);
            out("         AR    1,13");
            for (o = 0, n = total_size(x); n > 0; o += 256, n -= 256) {
                if (o == 3840) {
                    out("         LA    1,3840(,1)");
                    o = 0;
                }
                out("         XC    %d(%d,1),%d(1)", o, n > 256 ? 256 : n, o);
            }
            continue;
        }
        for (o = x->off, n = total_size(x); n > 0; o += 256, n -= 256)
            out("         XC    %d(%d,13),%d(13)", o, n > 256 ? 256 : n, o);
    }
    for (struct sym *x = p->syms; x; x = x->next)
        if (x->root == x && x->st == ST_AUTO && !x->param && !x->defined)
            areas_init(x, 13, x->off);
    gen_stmts(p->body);
    ins(NULL, "B", "@R%d", p->id);
    /* the raise stubs: entered by BAL 14, every register kept */
    for (int k = 0; k < 5; k++) {
        static const char tag[] = "ZOUSB";
        static const int cond[] = { C_ZERODIVIDE, C_FIXEDOVERFLOW, C_UNDERFLOW, C_SIZE, C_SUBRG };
        int need[5];
        need[0] = need_zd; need[1] = need_fofl; need[2] = need_ufl;
        need[3] = need_size; need[4] = need_subrg;
        if (!need[k])
            continue;
        out("@%c%-6d DS    0H", tag[k], p->id);
        ins(NULL, "ST", "14,%d(,13)", STUB14);
        arg_const(cond[k]);
        arg_const(0);
        arg_const(signal_code(cond[k]));
        /* SUBSCRIPTRANGE does not return normally: ERROR after the unit */
        call(k == 4 ? "PL1SUBR" : "PL1SIG");
        ins(NULL, "L", "14,%d(,13)", STUB14);
        ins(NULL, "BR", "14");
    }
    frame = align(tmp_hwm, 8);
    if (high_end > frame)
        frame = align(high_end, 8);
    if (code_bytes + lit_bytes > 12288 - 256)
        fatal("%s: procedure %s has about %d bytes of code and literals; PL1C/370 addresses "
              "12 KB per procedure yet -- split it into internal procedures", src_name,
              p->is_unit || p->is_block ? "(a block)" : p->name, code_bytes + lit_bytes);

    stk_frames += frame;
    if (p->recursive)
        stk_unknown = 1;
    fprintf(f, "* %s: PROCEDURE%s, frame %d bytes\n", p->name,
            p->is_main ? " OPTIONS(MAIN)" : p->depth == 0 ? " (external)" : "", frame);
    for (struct sym *s = p->syms; s; s = s->next) {
        if (s->root != s || s->is_entry)
            continue;
        if (s->param)
            fprintf(f, "*        %-31s parameter %d\n", s->name, s->param);
        else if (s->st == ST_AUTO)
            fprintf(f, "*        %-31s %4d(%d,13)\n", s->name, s->off, total_size(s));
        else if (s->st == ST_BASED)
            fprintf(f, "*        %-31s BASED, %d bytes\n", s->name, total_size(s));
        else
            fprintf(f, "*        %-31s @V%d STATIC\n", s->name, s->label);
    }
    if (p->depth == 0 && !p->is_main) {
        /* an external procedure: its name an entry point, for a call
         * from a procedure compiled apart (=V(name)) */
        if (strcmp(ext_name(p->name), "MAIN") == 0)
            fatal("%s: %s: an external procedure may not be named MAIN", src_name, p->name);
        if (!p->nentries) {
            fprintf(f, "         ENTRY %s\n", ext_name(p->name));
            fprintf(f, "%-8s DS    0H\n", ext_name(p->name));
        } else {
            /* with ENTRY statements: each name an entry point that sets R0,
             * the number of the entry, as a call of the source does */
            for (int k = 0; k <= p->nentries; k++) {
                const char *nm = ext_name(k ? p->entries[k - 1].name : p->name);
                fprintf(f, "         ENTRY %s\n", nm);
                fprintf(f, "%-8s DS    0H\n", nm);
                fprintf(f, "         USING %s,15\n", nm);
                fprintf(f, "         LA    0,%d\n", k);
                fprintf(f, "         L     15,@X%d_%d\n", p->id, k);
                fprintf(f, "         BR    15\n");
                fprintf(f, "@X%d_%d DC    A(@P%d)\n", p->id, k, p->id);
                fprintf(f, "         DROP  15\n");
            }
        }
    }
    if (p->is_main)
        fprintf(f, "MAIN     PDPPRLG CINDEX=0,FRAME=%d,BASER=12,ENTRY=YES\n", frame);
    else
        fprintf(f, "@P%-6d PDPPRLG CINDEX=%d,FRAME=%d,BASER=12,ENTRY=NO\n", p->id, p->id, frame);
    fprintf(f, "         B     @@FEN%d\n", p->id);
    fprintf(f, "         LTORG\n");
    fprintf(f, "@@FEN%d   EQU   *\n", p->id);
    fprintf(f, "         DROP  12\n");
    /* three base registers: 12 KB of code and literals (R10 and R11 are
     * not used by expression code) */
    fprintf(f, "         BALR  12,0\n");
    fprintf(f, "         USING *,12,11,10\n");
    fprintf(f, "%s", BASES);
    if (p->nentries) {
        /* the caller put the entry number in R0: its arguments go to
         * their places in the canonical list, R1 then points there */
        int cl = p->entoff + 4, n = 4 * (2 + p->nparams);
        fprintf(f, "         ST    0,%d(,13)\n", p->entoff);
        fprintf(f, "         XC    %d(%d,13),%d(13)\n", cl, n, cl);
        fprintf(f, "         MVC   %d(8,13),0(1)\n", cl);
        for (int k = 0; k <= p->nentries; k++) {
            fprintf(f, "         C     0,=F'%d'\n", k);
            fprintf(f, "         BNE   @@NE%d_%d\n", p->id, k);
            if (k == 0) {
                for (int j = 0; j < p->nprim; j++)
                    fprintf(f, "         MVC   %d(4,13),%d(1)\n", cl + 8 + 4 * j, 8 + 4 * j);
            } else {
                struct pentry *pe = &p->entries[k - 1];
                for (int j = 0; j < pe->nparams; j++)
                    fprintf(f, "         MVC   %d(4,13),%d(1)\n", cl + 8 + 4 * pe->canon[j],
                            8 + 4 * j);
            }
            fprintf(f, "         B     @@NX%d\n", p->id);
            fprintf(f, "@@NE%d_%d EQU   *\n", p->id, k);
        }
        fprintf(f, "@@NX%d   EQU   *\n", p->id);
        fprintf(f, "         LA    1,%d(,13)\n", cl);
    }
    fprintf(f, "         ST    1,%d(,13)\n", ALIST);
    if (p->is_main) {
        /* the program interruption exit (rt/pl1pie.asm), and the ESTAE
         * for an abnormal termination by the system (rt/pl1cond.c) */
        fprintf(f, "         L     15,=V(PL1PION)\n");
        fprintf(f, "         BALR  14,15\n");
        fprintf(f, "         L     15,=V(PL1STON)\n");
        fprintf(f, "         BALR  14,15\n");
        if (uses_complex) {
            /* a complex constant read or converted into a real target
             * gives its real part (rt/pl1cxsp.c) */
            fprintf(f, "         L     15,=V(PL1CXON)\n");
            fprintf(f, "         BALR  14,15\n");
        }
    }
    if (p->is_main && p->nparams &&
        (opt_plist || p->byvalue || (p->noexecops && !(p->nparams == 1 &&
                                         p->params[0]->t.k == TY_VCHAR)))) {
        /* R1 unchanged, an ordinary parameter list: PL/I (F) linked with
         * ENTRY IHESAPB or IHESAPD (-plist), or NOEXECOPS with other
         * parameters than one CHAR VARYING (rt/pl1parm.c, JOB01267) */
        /* BYVALUE: the words themselves, not as addresses (65536 + n) */
        fprintf(f, "         MVC   %d(4,13),=F'%d'\n", ARGS,
                p->nparams + (p->byvalue ? 65536 : 0));
        fprintf(f, "         LA    1,%d(,13)\n", ARGS);
        fprintf(f, "         L     15,=V(PL1MPLS)\n");
        fprintf(f, "         BALR  14,15\n");
        fprintf(f, "         ST    15,%d(,13)\n", ALIST);
    } else if (p->is_main && p->nparams == 1) {
        /* the PARM, as PL/I (F)'s IHESAPA passes it; in the default
         * language a VARYING parameter without NOEXECOPS gets the part
         * after the run-time options and '/' (rt/pl1parm.c) */
        fprintf(f, "         MVC   %d(4,13),=F'%d'\n", ARGS,
                p->params[0]->t.k != TY_VCHAR ? 0 :
                opt_lang_f || p->noexecops ? 1 : 2);
        fprintf(f, "         LA    1,%d(,13)\n", ARGS);
        fprintf(f, "         L     15,=V(PL1MPRM)\n");
        fprintf(f, "         BALR  14,15\n");
        fprintf(f, "         ST    15,%d(,13)\n", ALIST);
    }
    if (p->depth > 0) {
        fprintf(f, "         L     0,0(,1)\n");
        fprintf(f, "         ST    0,%d(,13)\n", SLINK);
    }
    if (opt_interrupt && p->depth == 0) {
        /* INTERRUPT: the attention exit, once for the program, when it
         * runs under a TSO terminal (rt/pl1cond.c) */
        fprintf(f, "         L     15,=V(PL1ATON)\n");
        fprintf(f, "         BALR  14,15\n");
    }
    /* the run time's current block: this one, until it ends */
    fprintf(f, "         L     15,=V(PL1CURF)\n");
    fprintf(f, "         MVC   %d(4,13),0(15)\n", CURF);
    fprintf(f, "         ST    13,0(,15)\n");
    fprintf(f, "         L     0,=A(@I%d)\n", p->id);
    fprintf(f, "         ST    0,%d(,13)\n", PINFO);
    /* the NOCONVERSION flag: the caller's kept, this block's set */
    fprintf(f, "         L     15,=V(PL1NOCV)\n");
    fprintf(f, "         MVC   %d(1,13),0(15)\n", NOCV);
    fprintf(f, "         MVI   %d(13),%d\n", NOCV + 1,
            rt_flags(p->mask & EN_SET ? p->mask : EN_DEFAULT));
    fprintf(f, "         MVC   0(1,15),%d(13)\n", NOCV + 1);
    for (int k = 1; k <= p->nentries; k++) {
        /* entered at an ENTRY statement: its descriptor */
        int l = new_label();
        fprintf(f, "         CLC   %d(4,13),=F'%d'\n", p->entoff, k);
        fprintf(f, "         BNE   @L%d\n", l);
        fprintf(f, "         L     0,=A(@I%dE%d)\n", p->id, k);
        fprintf(f, "         ST    0,%d(,13)\n", PINFO);
        fprintf(f, "@L%-6d DS    0H\n", l);
    }
    for (int k = 1; k <= p->nentries; k++) {
        /* entered at an ENTRY statement: on from there */
        fprintf(f, "         CLC   %d(4,13),=F'%d'\n", p->entoff, k);
        fprintf(f, "         BE    @L%d\n", p->entries[k - 1].label);
    }
    fwrite(buf, 1, blen, f);
    fprintf(f, "@R%-6d DS    0H\n", p->id);
    fprintf(f, "* END %s\n", p->name);
    if (p->is_main) {
        /* FINISH at the normal end of the program */
        fprintf(f, "         LA    1,%d(,13)\n", ARGS);
        fprintf(f, "         L     15,=V(PL1EMAIN)\n");
        fprintf(f, "         BALR  14,15\n");
    }
    if (nests) {
        /* what this block established ends with it */
        fprintf(f, "         ST    13,%d(,13)\n", ARGS);
        fprintf(f, "         LA    1,%d(,13)\n", ARGS);
        fprintf(f, "         L     15,=V(PL1BEXT)\n");
        fprintf(f, "         BALR  14,15\n");
    }
    fprintf(f, "         L     15,=V(PL1CURF)\n");
    fprintf(f, "         MVC   0(4,15),%d(13)\n", CURF);
    fprintf(f, "         L     15,=V(PL1NOCV)\n");
    fprintf(f, "         MVC   0(1,15),%d(13)\n", NOCV);
    if (p->is_main) {
        /* the return code PLIRETC set, 0 without one (rt/pl1cond.c) */
        fprintf(f, "         L     15,=V(PL1RTCD)\n");
        fprintf(f, "         L     15,0(,15)\n");
    } else
        fprintf(f, "         SR    15,15\n");
    fprintf(f, "         PDPEPIL\n");
    /* the stubs of the labels used as values: entered with R15 at the
     * stub and R13 the block's frame, they set the block's bases */
    for (int i = 0; i < p->nvlabels; i++) {
        fprintf(f, "         DS    0F\n");
        fprintf(f, "@J%-6d L     12,24(,15)\n", p->vlabels[i]);
        fprintf(f, "         LA    11,4095(,12)\n");
        fprintf(f, "         LA    11,1(,11)\n");
        fprintf(f, "         LA    10,4095(,11)\n");
        fprintf(f, "         LA    10,1(,10)\n");
        fprintf(f, "         B     @G%d\n", p->vlabels[i]);
        fprintf(f, "         DC    A(@@FEN%d+2)\n", p->id);
    }
    if (p->depth == 0)
        /* the ABI stamp: a run time of another level does not define it
         * (rt/pl1abi.h), and the link fails */
        fprintf(f, "         DC    V(%s)\n", PL1_ABI_STAMP);
    fprintf(f, "         LTORG\n");
    /* the next procedure's prologue has R12 alone: a USING of R11 and R10
     * left from here would take its branch to @@FEN, made 4K on, for this
     * procedure's second base (an on-unit after a main procedure of 4K:
     * S0C1, t199chk) */
    fprintf(f, "         DROP  12,11,10\n");
    for (struct proc *c = p->child; c; c = c->sibling)
        gen_proc(c, f);
}

/* Each procedure's descriptor: its entry and name, for the messages of
 * the run time.  An on-unit goes by the procedure it is written in. */
static void gen_pinfo(struct proc *p)
{
    struct proc *n = p;
    while (n->is_unit || n->is_block)
        n = n->parent;
    out("         DS    0F");
    /* the length byte: X'80' a BEGIN block or on-unit, X'40' the main
     * procedure (rt/pl1cond.c, SNAP's calling procedures) */
    {
        int fl = (p != n ? PL1_PI_BLOCK : 0) | (n->is_main ? PL1_PI_MAIN : 0);
        if (n->is_main)
            out("@I%-6d DC    A(MAIN),AL1(%d),C'%s'", p->id, (int)strlen(n->name) | fl, n->name);
        else
            out("@I%-6d DC    A(@P%d),AL1(%d),C'%s'", p->id, n->id, (int)strlen(n->name) | fl,
                n->name);
        /* each ENTRY statement's: ONLOC names the entry point (Q1MISC) */
        for (int k = 1; p == n && k <= p->nentries; k++)
            out("@I%dE%d DC    A(@P%d),AL1(%d),C'%s'", p->id, k, n->id,
                (int)strlen(p->entries[k - 1].name) | fl, p->entries[k - 1].name);
    }
    for (struct proc *c = p->child; c; c = c->sibling)
        gen_pinfo(c);
}

/* does block b, or a block in it, have COMPLEX data */
static int any_complex(const struct proc *b)
{
    for (const struct sym *x = b->syms; x; x = x->next)
        if (is_cplx(x->t))
            return 1;
    for (const struct proc *c = b->child; c; c = c->sibling)
        if (any_complex(c))
            return 1;
    return 0;
}

void gen_program(struct proc *p, FILE *f)
{
    for (struct proc *u = p; u; u = u->next_ext) {
        label_statics(u);
        if (any_complex(u))
            uses_complex = 1;
    }
    fprintf(f, "* %s -- generated by PL1C/370 from %s\n", p->name, src_name);
    fprintf(f, "         COPY  PDPTOP\n");
    fprintf(f, "         CSECT\n");
    if (p->is_main) {
        fprintf(f, "         ENTRY @@MAIN\n");
        fprintf(f, "@@MAIN   DS    0H\n");
        fprintf(f, "         BALR  15,0\n");
        fprintf(f, "         USING *,15\n");
        fprintf(f, "         L     15,=V(@@CRT0)\n");
        fprintf(f, "         BR    15\n");
        fprintf(f, "         DROP  15\n");
        fprintf(f, "         LTORG\n");
    }
    for (struct proc *u = p; u; u = u->next_ext)
        gen_proc(u, f);
    blen = 0;
    for (struct strlit *s = strs; s; s = s->next) {
        char label[16];
        snprintf(label, sizeof label, "@L%d", s->label);
        dc_chars(label, s->s, s->len);
    }
    for (struct proc *u = p; u; u = u->next_ext)
        gen_static_data(u);
    for (struct cname *c = cnames; c; c = c->next)
        out("@L%-6d DC    AL1(%d),C'%s'", c->label, (int)strlen(c->name), c->name);
    for (struct proc *u = p; u; u = u->next_ext)
        gen_pinfo(u);
    for (struct ftab *t = ftabs; t; t = t->next) {
        int m = 0, at;
        while (t->f[m].k != F_END)
            m++;
        at = 6 * (m + 1);               /* P pictures follow the END item */
        out("         DS    0H");
        out("@L%-6d DS    0H", t->label);
        for (int i = 0; ; i++) {
            if (t->f[i].k == F_P) {
                int n = (int)strlen(t->f[i].pic);
                out("         DC    H'%d',H'%d',H'%d'", F_P, n, at);
                at += n;
            } else {
                out("         DC    H'%d',H'%d',H'%d'", t->f[i].k, t->f[i].w, t->f[i].d);
            }
            if (t->f[i].k == F_END)
                break;
        }
        for (int i = 0; i < m; i++)
            if (t->f[i].k == F_P)
                out("         DC    C'%s'", t->f[i].pic);
    }
    static_named = 1;
    for (struct proc *u = p; u; u = u->next_ext)
        gen_static_data(u);
    static_named = 0;
    fwrite(buf, 1, blen, f);
    if (p->is_main) {
        /* the C stack: @@STKLEN, which @@CRT0 takes from 4096 on */
        int n = opt_stack >= 0 ? opt_stack :
                stk_unknown ? 0 : (stk_frames + STK_RESERVE + 4095) / 4096 * 4096;
        if (n > 0 && (opt_stack > 0 || n < STK_DEFAULT)) {
            fprintf(f, "* the C stack: %s\n", opt_stack > 0 ? "-stack" :
                    "the frames added up and a reserve");
            fprintf(f, "         CSECT\n");      /* back in the program's own section */
            fprintf(f, "         ENTRY @@STKLEN\n");
            fprintf(f, "@@STKLEN DC    F'%d'\n", n);
        }
    }
    fprintf(f, "         END   %s\n", p->is_main ? "@@MAIN" : "");
}
