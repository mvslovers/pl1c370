/* lower.c -- arrays and structures down to scalar statements.
 *
 * After this pass every reference names one elementary item with all its
 * subscripts.  Aggregates -- a whole array, a structure, an array of
 * structures -- are expanded:
 *
 *  - in an assignment, into DO loops over the dimensions the target leaves
 *    open, element by element (array expressions included), and member by
 *    member for a structure (matched by position);
 *  - in PUT LIST, into one item per element, in the order PL/I (F) writes
 *    them (JOB00078): rows first, and for an array of structures element
 *    by element, every member of R(1) before R(2);
 *  - INITIAL values become assignments at block entry (AUTOMATIC); for
 *    STATIC they stay in the declaration for the generator.
 *
 * DIM, LBOUND and HBOUND fold to constants: all bounds are constants.
 */
#include <stdlib.h>
#include <string.h>

#include "pl1c.h"

static struct proc *P;
static int nloop;

/* ---- layout ----------------------------------------------------------- */

static int elem_size(struct type t)
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
    case TY_AREA:   return 16 + t.p;
    case TY_OFFSET: return 4;
    case TY_CFIXBIN: case TY_CFIXDEC: case TY_CFLTDEC: case TY_CFLTBIN:
        return 2 * elem_size(part_type(t));     /* real part, imaginary part */
    case TY_EVENT:  return PL1_EV_SIZE;
    default:        return 0;
    }
}

static int elem_align(struct type t)
{
    switch (t.k) {
    case TY_FIXBIN: return t.p <= 15 ? 2 : 4;
    case TY_VCHAR:  return 2;
    case TY_VBIT:   return 2;
    case TY_FLTDEC:
    case TY_FLTBIN: return flt_long(t) ? 8 : 4;
    case TY_PTR:    return 4;
    case TY_LABEL:  return 4;
    case TY_AREA:   return 8;
    case TY_OFFSET: return 4;
    case TY_CFIXBIN: case TY_CFIXDEC: case TY_CFLTDEC: case TY_CFLTBIN:
        return elem_align(part_type(t));
    case TY_EVENT:  return 4;
    default:        return 1;
    }
}

static int extent(struct sym *s)
{
    int n = 1;
    for (int i = 0; i < s->ndims; i++)
        n *= s->hb[i] - s->lb[i] + 1;
    return n;
}


/* Size, alignment and doubleword phase of one element, members mapped as
 * PL/I (F) maps a structure (measured, JOB00115: CHAR(3), BIN(15),
 * CHAR(1), BIN(31), DEC(3) lie at 0, 3, 5, 7, 11, 13 bytes in all).  The
 * members are added one at a time; before each, the unit mapped so far
 * may be shifted by multiples of its largest alignment, within a
 * doubleword, to need the least padding -- the smallest shift on a tie.
 * The structure then begins at that phase from a doubleword boundary.
 *
 * The mapping is done in bits: an unaligned bit string has the alignment
 * of a bit, so PL/I (F) packs them (Q1BMAP, JOB00745: BIT(3), BIT(2)
 * before a CHAR lie at bits 3 and 6 of the first byte).  Everything else
 * is a whole number of bytes, and maps as it did in bytes. */
static int bit_unaligned(struct sym *s)
{
    return s->t.k == TY_BIT && !s->aligned;
}

static void layout_bits(struct sym *s)
{
    if (!s->child) {
        if (bit_unaligned(s)) {
            s->bsize = s->t.p > 0 ? s->t.p : 1;
            s->balign = 1;
        } else {
            s->bsize = 8 * elem_size(s->t);
            s->balign = 8 * elem_align(s->t);
        }
        s->bphase = 0;
        s->bstride = s->bsize;
        return;
    }
    {
        int start = -1, end = 0, al = 1;
        for (struct sym *c = s->child; c; c = c->sibling) {
            int csize, calign, cphase, best = -1, bestpad = 1 << 30, pos;
            layout_bits(c);
            csize = c->ndims ? c->bstride * (extent(c) - 1) + c->bsize : c->bsize;
            calign = c->balign;
            cphase = c->bphase % calign;
            if (start < 0) {
                start = cphase;           /* the first member at its phase */
                c->bmoff = start;
                end = start + csize;
                al = calign;
                continue;
            }
            /* shifts of the unit by multiples of al, its start in [0,64) */
            for (int st = start % al; st < 64; st += al) {
                int e = end + (st - start), pad;
                pos = e;
                while (((pos % calign) + calign) % calign != cphase)
                    pos++;
                pad = pos - e;
                if (pad < bestpad) {
                    bestpad = pad;
                    best = st;
                }
            }
            {
                int shift = best - start;
                for (struct sym *x = s->child; x != c; x = x->sibling)
                    x->bmoff += shift;
                start = best;
                end += shift;
            }
            pos = end;
            while (pos % calign != cphase)
                pos++;
            c->bmoff = pos;
            end = pos + csize;
            if (calign > al)
                al = calign;
        }
        for (struct sym *c = s->child; c; c = c->sibling)
            c->bmoff -= start;            /* offsets from the structure's start */
        s->balign = al;
        s->bphase = start;
        s->bsize = end - start;
        /* an element of an array of structures keeps the phase: its
         * stride is rounded up, but the last element has no padding
         * after it (JOB00123: two of CHAR(1), BIN(31) take 13 bytes) */
        s->bstride = (s->bsize + al - 1) / al * al;
    }
}

/* The byte fields from the bit mapping; packed: a BIT leaf at a bit
 * offset or with a stride that is not whole bytes somewhere in its chain */
static void mark_shared(struct sym *s, int bitin);
static void mark_packed(struct sym *s);

static void layout_bytes(struct sym *s, int bitin, int ispacked)
{
    int b = s->bmoff + bitin;
    s->moff = b / 8;
    s->mbit = b % 8;
    if (s->mbit || (s->ndims && s->bstride % 8) || (s->child && s->bphase % 8 && !s->parent))
        ispacked = 1;
    if (!s->child) {
        s->size = (s->bsize + 7) / 8;
        s->align = (s->balign + 7) / 8;
        s->phase = 0;
        s->tail = 0;
        s->packed = bit_unaligned(s) && ispacked;
        return;
    }
    for (struct sym *c = s->child; c; c = c->sibling)
        layout_bytes(c, 0, ispacked || s->bphase % 8 != 0);
    mark_shared(s, 0);
    s->align = (s->balign + 7) / 8;
    s->phase = s->bphase / 8;
    if (s->ndims) {
        s->size = (s->bstride + 7) / 8;
        s->tail = (s->bstride - s->bsize) / 8;
    } else {
        s->size = (s->bphase % 8 + s->bsize + 7) / 8;
        s->tail = 0;
    }
}

/* A bit string whose last byte holds another member's bits too is
 * reached bit by bit: storing its whole bytes would change the other. */
static void mark_shared(struct sym *s, int bitin)
{
    for (struct sym *c = s->child; c; c = c->sibling) {
        int end = c->bmoff + bitin + (c->ndims ? c->bstride * (extent(c) - 1) + c->bsize
                                               : c->bsize);
        if (c->sibling && c->sibling->bmoff + bitin < (end + 7) / 8 * 8)
            mark_packed(c);
    }
}

static void mark_packed(struct sym *s)
{
    if (!s->child) {
        if (bit_unaligned(s))
            s->packed = 1;
        return;
    }
    for (struct sym *c = s->child; c; c = c->sibling)
        mark_packed(c);
}

static int has_packed(struct sym *s)
{
    if (s->packed)
        return 1;
    for (struct sym *c = s->child; c; c = c->sibling)
        if (has_packed(c))
            return 1;
    return 0;
}

static void layout(struct sym *s)
{
    layout_bits(s);
    s->bmoff = 0;
    if (s->child) {
        /* the members' bits counted from the first byte of the storage */
        int lead = s->bphase % 8;
        s->moff = s->mbit = 0;
        for (struct sym *c = s->child; c; c = c->sibling)
            layout_bytes(c, lead, 0);
        mark_shared(s, lead);
        s->align = (s->balign + 7) / 8;
        s->phase = s->bphase / 8;
        if (s->ndims) {
            s->size = (s->bstride + 7) / 8;
            s->tail = (s->bstride - s->bsize) / 8;
        } else {
            s->size = (lead + s->bsize + 7) / 8;
            s->tail = 0;
        }
    } else {
        layout_bytes(s, 0, 0);
    }
    s->tbits = 0;
    if (has_packed(s)) {
        int lead = s->child ? s->bphase % 8 : 0;
        s->tbits = lead + (s->ndims ? s->bstride * (extent(s) - 1) + s->bsize : s->bsize);
    }
}

int cum_dims(struct sym *s)
{
    return s ? cum_dims(s->parent) + s->ndims : 0;
}


#define STARB (-2147483647)   /* a bound known at run time (A(*)) */

static struct expr *mk(enum ekind k, int line);
static struct expr *binop(int op, struct expr *l, struct expr *r);
static struct expr *int_const(int v, int line);
static struct expr *var_ref(struct sym *s, int line);

/* The bounds of the chain from the root down to s, outermost first. */
static int chain_bounds(struct sym *s, int *lb, int *hb)
{
    int n = 0;
    if (!s)
        return 0;
    n = chain_bounds(s->parent, lb, hb);
    for (int i = 0; i < s->ndims; i++) {
        /* an A(*) parameter's bounds are known at run time only, and
         * so is a bound given by REFER (bound_expr) */
        int rt = s->stardims || (s->refer && s->refer_dim == i);
        lb[n] = rt ? STARB : s->lb[i];
        hb[n] = rt ? STARB : s->hb[i];
        n++;
    }
    return n;
}

/* Bound k (from 0, outermost first) of the chain down to s: a constant,
 * or LBOUND/HBOUND of an A(*) parameter (gen.c loads it) */
static struct expr *bound_expr(struct sym *s, int k, int high, int line)
{
    struct sym *chain[32];
    int n = 0;
    for (struct sym *x = s; x; x = x->parent)
        chain[n++] = x;
    for (int i = n - 1; i >= 0; i--) {
        struct sym *x = chain[i];
        if (k < x->ndims) {
            struct expr *e;
            if (x->refer && x->refer_dim == k && high)
                return var_ref(x->refer, line);     /* REFER: from the member */
            if (!x->stardims)
                return int_const(high ? x->hb[k] : x->lb[k], line);
            e = mk(E_CALL, line);
            e->fn = high ? B_HBOUND : B_LBOUND;
            e->nargs = 2;
            e->arg[0] = var_ref(x, line);
            e->arg[1] = int_const(k + 1, line);
            return e;
        }
        k -= x->ndims;
    }
    fatal("%s(%d): internal: no bound %d", src_name, line, k);
    return NULL;
}

/* ---- constructors ----------------------------------------------------- */

static struct expr *mk(enum ekind k, int line)
{
    struct expr *e = xmalloc(sizeof *e);
    memset(e, 0, sizeof *e);
    e->k = k;
    e->line = line;
    return e;
}

/* An integer constant, typed FIXED DECIMAL as a written constant is. */
static struct expr *int_const(int v, int line)
{
    char buf[16];
    struct expr *n = mk(E_NUM, line);
    snprintf(buf, sizeof buf, "%d", v < 0 ? -v : v);
    n->num = xstrdup(buf);
    n->t.k = TY_FIXDEC;
    n->t.p = (int)strlen(buf);
    if (v < 0) {
        struct expr *m = mk(E_NEG, line);
        m->l = n;
        return m;
    }
    return n;
}

/* An integer constant typed FIXED BINARY(15): the value of DIM etc. */
static struct expr *bin15_const(int v, int line)
{
    struct expr *e = int_const(v, line);
    struct expr *n = e->k == E_NEG ? e->l : e;
    n->t.k = TY_FIXBIN;
    n->t.p = 15;
    n->t.q = 0;
    return e;
}

static struct expr *ref(struct sym *s, struct expr **subs, int nsubs, int line)
{
    struct expr *e = mk(E_VAR, line);
    e->sym = s;
    for (int i = 0; i < nsubs; i++)
        e->sub[i] = subs[i];
    e->nsubs = nsubs;
    return e;
}

static struct expr *var_ref(struct sym *s, int line)
{
    return ref(s, NULL, 0, line);
}

struct sym *new_loop_var(struct proc *p)
{
    char name[16];
    struct sym *s = xmalloc(sizeof *s);
    memset(s, 0, sizeof *s);
    snprintf(name, sizeof name, "#L%d", ++nloop);
    s->name = xstrdup(name);
    s->t.k = TY_FIXBIN;
    s->t.p = 31;
    s->level = 1;
    s->root = s;
    s->owner = p;
    s->synthetic = 1;
    s->next = p->syms;
    p->syms = s;
    return s;
}

static struct stmt *mk_stmt(enum skind k, struct stmt *like)
{
    struct stmt *s = xmalloc(sizeof *s);
    memset(s, 0, sizeof *s);
    s->k = k;
    s->skip = -1;
    if (like) {
        s->no = like->no;
        s->line = like->line;
        s->text = like->text;
        s->mask = like->mask;
    }
    return s;
}

/* DO v = lb TO hb; body; END */
static struct stmt *loop(struct sym *v, int lb, int hb, struct stmt *body, struct stmt *like)
{
    struct stmt *s = mk_stmt(S_ITER, like);
    s->target = v;
    s->e = int_const(lb, like ? like->line : 0);
    s->e2 = int_const(hb, like ? like->line : 0);
    s->body = body;
    return s;
}

/* ---- aggregates in expressions ---------------------------------------- */

static int is_struct(struct sym *s) { return s->child != NULL; }

/* The subscripts * of a cross section M(2,*). */
static int nstars(const struct expr *e)
{
    int n = 0;
    for (int i = 0; i < e->nsubs; i++)
        n += e->sub[i]->k == E_STAR;
    return n;
}

/* Open dimensions of a reference: those its subscripts leave out, or
 * give as * */
static int open_dims(struct expr *e)
{
    int n = nstars(e);
    return n ? n : cum_dims(e->sym) - e->nsubs;
}

static int is_aggregate(struct expr *e)
{
    return e->k == E_VAR && (is_struct(e->sym) || open_dims(e) > 0);
}

/* Fold DIM/LBOUND/HBOUND; returns the new node. */
static struct expr *fold_bounds(struct expr *e)
{
    int lb[MAXDIMS * 4], hb[MAXDIMS * 4], n, k, v = 0;
    struct expr *a = e->arg[0];

    if (a->k != E_VAR || a->nsubs != 0 || cum_dims(a->sym) == 0)
        fatal("%s(%d): DIM, LBOUND and HBOUND need an array", src_name, e->line);
    if (a->sym->stardims)
        return e;                 /* an A(*) parameter: at run time (gen.c) */
    if (a->sym->refer && a->sym->refer_dim >= 0 && e->fn == B_LBOUND &&
        e->arg[1]->k == E_NUM && atoi(e->arg[1]->num) == a->sym->refer_dim + 1)
        return bin15_const(a->sym->lb[a->sym->refer_dim], e->line);
    if (a->sym->refer && a->sym->refer_dim >= 0 &&
        e->arg[1]->k == E_NUM && atoi(e->arg[1]->num) == a->sym->refer_dim + 1) {
        /* REFER: HBOUND is the member, DIM the member - LBOUND + 1, as
         * FIXED BINARY(15) (Q1REF) */
        struct expr *y = ref(a->sym->refer, NULL, 0, e->line), *b = mk(E_CALL, e->line);
        y->qual = a->qual;
        if (e->fn == B_DIM)
            y = binop(T_PLUS, binop(T_MINUS, y, int_const(a->sym->lb[a->sym->refer_dim],
                                                           e->line)), int_const(1, e->line));
        b->fn = B_BINARY;
        b->nargs = 3;
        b->arg[0] = y;
        b->arg[1] = int_const(15, e->line);
        b->arg[2] = int_const(0, e->line);
        return b;
    }
    if (!(e->arg[1]->k == E_NUM && e->arg[1]->t.q == 0))
        fatal("%s(%d): the dimension must be an integer constant", src_name, e->line);
    k = atoi(e->arg[1]->num);
    n = chain_bounds(a->sym, lb, hb);
    if (k < 1 || k > n)
        fatal("%s(%d): %s has no dimension %d", src_name, e->line, a->sym->name, k);
    switch (e->fn) {
    case B_DIM:    v = hb[k - 1] - lb[k - 1] + 1; break;
    case B_LBOUND: v = lb[k - 1]; break;
    case B_HBOUND: v = hb[k - 1]; break;
    default: break;
    }
    return bin15_const(v, e->line);
}

static int is_reduce(struct expr *e)
{
    return e->k == E_CALL && (e->fn == B_SUM || e->fn == B_PROD || e->fn == B_POLY ||
                              e->fn == B_ALL || e->fn == B_ANY || e->fn == B_DOT ||
                              e->fn == B_EVERY || e->fn == B_SOME);
}

static struct expr *reduce(struct expr *e);
/* the statements to put before (array functions, CHECK of an entry) and
 * after (cross sections passed) the statement being lowered; none in a
 * WHILE or UNTIL condition (no_pre) */
static struct stmt *pre, *post;
static int no_pre;
static int in_put_items;      /* lowering a PUT or GET's repetitive items */
static int no_check;          /* lowering INITIAL values */
static const char *in_checks(const char *name);
static struct stmt *check_stmt(const char *name, struct sym *x, struct stmt *like);
static struct expr *section_arg(struct expr *a);
static struct stmt *append(struct stmt *list, struct stmt *s);

/* Copy e, appending the loop variables vars[0..nv) to every aggregate
 * array reference; its open bounds must be lb/hb.  Fold the bound
 * functions.  Structures cannot appear in an expression. */
static struct expr *subst(struct expr *e, struct sym **vars, int nv, const int *lb, const int *hb)
{
    struct expr *c;

    if (!e)
        return NULL;
    if (e->k == E_CALL && (e->fn == B_DIM || e->fn == B_LBOUND || e->fn == B_HBOUND))
        return fold_bounds(e);
    if (is_reduce(e))
        return reduce(e);
    if (e->k == E_FCALL && !no_pre && !in_put_items) {
        /* CHECK of the entry: before it is invoked (Q1CHK: SUB) */
        const char *n = e->entry ? e->entry->name : e->callee ? e->callee->name : NULL;
        if (e->entryno && e->callee)
            n = e->callee->entries[e->entryno - 1].name;
        if (n && in_checks(n)) {
            struct stmt *like = mk_stmt(S_NULL, NULL);
            like->line = e->line;
            pre = append(pre, check_stmt(in_checks(n), NULL, like));
        }
    }
    c = xmalloc(sizeof *c);
    *c = *e;
    for (int i = 0; i < e->nsubs; i++)
        c->sub[i] = subst(e->sub[i], NULL, 0, NULL, NULL);
    for (int i = 0; i < e->nargs; i++) {
        if ((e->k == E_FCALL || (e->k == E_CALL && (e->fn == B_ADDR || e->fn == B_STRING || e->fn == B_STG ||
                                 e->fn == B_CSTG))) &&
            is_aggregate(e->arg[i])) {
            /* an array or structure, passed whole; a cross section by a
             * copy, assigned back after the statement */
            c->arg[i] = nstars(e->arg[i]) ? section_arg(e->arg[i]) : e->arg[i];
        }
        else
            c->arg[i] = subst(e->arg[i], e->k == E_FCALL ? NULL : vars, e->k == E_FCALL ? 0 : nv,
                              lb, hb);
    }
    c->l = subst(e->l, vars, nv, lb, hb);
    c->r = subst(e->r, vars, nv, lb, hb);
    if (e->k == E_VAR) {
        if (is_struct(e->sym))
            fatal("%s(%d): structure %s cannot be an operand here", src_name, e->line, e->sym->name);
        if (nstars(e) > 0) {
            /* a cross section: its *s take the loop variables in order */
            int elb[MAXDIMS * 4], ehb[MAXDIMS * 4], k = 0;
            chain_bounds(e->sym, elb, ehb);
            if (nstars(e) != nv)
                fatal("%s(%d): array %s does not have the shape of the target", src_name,
                      e->line, e->sym->name);
            for (int i = 0; i < e->nsubs; i++) {
                if (e->sub[i]->k != E_STAR)
                    continue;
                if (elb[i] != STARB && lb[k] != STARB && (elb[i] != lb[k] || ehb[i] != hb[k]))
                    fatal("%s(%d): array %s does not have the bounds of the target", src_name,
                          e->line, e->sym->name);
                c->sub[i] = var_ref(vars[k++], e->line);
            }
        } else if (open_dims(e) > 0) {
            int elb[MAXDIMS * 4], ehb[MAXDIMS * 4], n = chain_bounds(e->sym, elb, ehb);
            int open = open_dims(e);
            if (e->nsubs != 0 && open != nv)
                fatal("%s(%d): %s: give all subscripts or none", src_name, e->line, e->sym->name);
            if (open != nv)
                fatal("%s(%d): array %s does not have the shape of the target", src_name,
                      e->line, e->sym->name);
            for (int i = 0; i < nv; i++)
                if (elb[n - nv + i] != STARB && lb[i] != STARB &&
                    (elb[n - nv + i] != lb[i] || ehb[n - nv + i] != hb[i]))
                    fatal("%s(%d): array %s does not have the bounds of the target", src_name,
                          e->line, e->sym->name);
            for (int i = 0; i < nv; i++) {
                if (c->nsubs == MAXDIMS)
                    fatal("%s(%d): too many dimensions", src_name, e->line);
                c->sub[c->nsubs++] = var_ref(vars[i], e->line);
            }
        }
    }
    return c;
}

/* The subscripts written on a reference: none, or all of them -- for a
 * structure, all of its own level (it then names one element).  PL/I (F)
 * refuses anything else (IEM0258I). */
static void check_subs(struct expr *e)
{
    if (!e)
        return;
    if (e->k == E_VAR && e->nsubs != 0 && e->nsubs != cum_dims(e->sym))
        fatal("%s(%d): the number of subscripts for %s conflicts with its dimensionality",
              src_name, e->line, e->sym->name);
    for (int i = 0; i < e->nsubs; i++)
        check_subs(e->sub[i]);
    for (int i = 0; i < e->nargs; i++)
        check_subs(e->arg[i]);
    check_subs(e->l);
    check_subs(e->r);
}

static void check_stmt_subs(struct stmt *s);

/* a data list item, a repetitive specification's items too */
static void check_item_subs(struct put_item *it)
{
    if (it->loop) {
        check_stmt_subs(it->loop);
        for (struct put_item *x = it->sub; x; x = x->next)
            check_item_subs(x);
    } else {
        check_subs(it->e);
    }
}

static void check_stmt_subs(struct stmt *s)
{
    for (; s; s = s->next) {
        check_subs(s->lhs);
        check_subs(s->e);
        check_subs(s->e2);
        check_subs(s->e3);
        for (struct dospec *d = s->k == S_ITER ? s->specs : NULL; d; d = d->next) {
            check_subs(d->e1);
            check_subs(d->e2);
            check_subs(d->e3);
            check_subs(d->w);
            check_subs(d->u);
            check_subs(d->rep);
        }
        for (struct put_item *it = s->items; it; it = it->next)
            check_item_subs(it);
        check_subs(s->file);
        check_stmt_subs(s->body);
        check_stmt_subs(s->els);
    }
}

static int has_aggregate(struct expr *e)
{
    if (!e)
        return 0;
    if (e->k == E_CALL && (e->fn == B_DIM || e->fn == B_LBOUND || e->fn == B_HBOUND ||
                           e->fn == B_ADDR || e->fn == B_STRING || e->fn == B_STG ||
                           e->fn == B_CSTG))
        return 0;                         /* ADDR of an array is one address */
    if (is_reduce(e))
        return 0;                         /* an element value */
    if (is_aggregate(e))
        return 1;
    for (int i = 0; i < e->nargs; i++)
        if (!(e->k == E_FCALL && is_aggregate(e->arg[i])) && has_aggregate(e->arg[i]))
            return 1;
    return has_aggregate(e->l) || has_aggregate(e->r);
}

/* ---- SUM, PROD, POLY, ALL, ANY ------------------------------------------ */

/* An array function becomes a temporary, computed by loops over the
 * elements before the statement that uses it (lower_list puts them
 * there).  Not in a WHILE or UNTIL condition, which is evaluated again
 * and again. */
static int nreduce;

static struct sym *new_temp(struct type t)
{
    char name[16];
    struct sym *s = xmalloc(sizeof *s);
    snprintf(name, sizeof name, "#R%d", ++nreduce);
    s->name = xstrdup(name);
    s->t = t;
    s->level = 1;
    s->root = s;
    s->owner = P;
    s->synthetic = 1;
    s->next = P->syms;
    P->syms = s;
    layout(s);                    /* the others were laid out before */
    return s;
}

/* The array reference that gives an argument its shape. */
static struct expr *shape_of(struct expr *e)
{
    struct expr *r = NULL;
    if (!e || is_reduce(e) || e->k == E_FCALL ||
        (e->k == E_CALL && (e->fn == B_DIM || e->fn == B_LBOUND || e->fn == B_HBOUND ||
                            e->fn == B_ADDR || e->fn == B_STRING || e->fn == B_STG ||
                            e->fn == B_CSTG)))
        return NULL;
    if (e->k == E_VAR)
        return open_dims(e) > 0 || is_struct(e->sym) ? e : NULL;
    for (int i = 0; i < e->nargs && !r; i++)
        r = shape_of(e->arg[i]);
    if (!r)
        r = shape_of(e->l);
    if (!r)
        r = shape_of(e->r);
    return r;
}

static struct stmt *assign(struct sym *t, struct expr *e, struct stmt *like)
{
    struct stmt *s = mk_stmt(S_ASSIGN, like);
    s->lhs = var_ref(t, e->line);
    s->e = e;
    return s;
}

static struct expr *binop(int op, struct expr *l, struct expr *r)
{
    struct expr *e = mk(E_BIN, l->line);
    e->op = op;
    e->l = l;
    e->r = r;
    return e;
}

/* Loops over the open dimensions of sh around body: vars, lb and hb are
 * filled first, by open_shape; the body is made from them. */
struct shape { struct sym *sym; struct sym *vars[MAXDIMS * 4]; int nv;
               int lb[MAXDIMS * 4], hb[MAXDIMS * 4], dim[MAXDIMS * 4];
               struct expr *qual; };   /* P-> of the reference: REFER's bound */

static void open_shape(struct expr *sh, struct shape *z)
{
    int lb[MAXDIMS * 4], hb[MAXDIMS * 4], n = chain_bounds(sh->sym, lb, hb);
    if (is_struct(sh->sym))
        fatal("%s(%d): structure %s cannot be an operand here", src_name, sh->line,
              sh->sym->name);
    if (sh->nsubs != 0 && !nstars(sh))
        fatal("%s(%d): %s: give all subscripts or none", src_name, sh->line, sh->sym->name);
    z->sym = sh->sym;
    z->qual = sh->qual;
    z->nv = open_dims(sh);
    for (int i = 0, k = 0; i < n && k < z->nv; i++) {
        if (nstars(sh) ? sh->sub[i]->k != E_STAR : i < n - z->nv)
            continue;
        z->vars[k] = new_loop_var(P);
        z->dim[k] = i;
        z->lb[k] = lb[i];
        z->hb[k] = hb[i];
        k++;
    }
}

/* The reference sh, a cross section, with its subscripts lowered and
 * each * replaced by the shape's loop variable. */
static struct expr *star_elem(struct expr *sh, struct shape *z)
{
    struct expr *c = xmalloc(sizeof *c);
    *c = *sh;
    for (int i = 0, k = 0; i < sh->nsubs; i++)
        c->sub[i] = sh->sub[i]->k == E_STAR ? var_ref(z->vars[k++], sh->line) :
                    subst(sh->sub[i], NULL, 0, NULL, NULL);
    if (!nstars(sh))
        for (int k = 0; k < z->nv; k++)
            c->sub[c->nsubs++] = var_ref(z->vars[k], sh->line);   /* a whole array */
    return c;
}

/* bound_expr of the shape's dimension: a REFER member under the
 * reference's pointer (P2->A: P2->N) */
static struct expr *shape_bound(struct shape *z, int i, int high, int line)
{
    struct expr *b = bound_expr(z->sym, z->dim[i], high, line);
    if (b->k == E_VAR && z->sym->refer && b->sym == z->sym->refer)
        b->qual = z->qual;
    return b;
}

static struct stmt *shape_loops(struct shape *z, struct stmt *body, struct stmt *like)
{
    for (int i = z->nv - 1; i >= 0; i--) {
        if (z->lb[i] == STARB) {
            body = loop(z->vars[i], 0, 0, body, like);
            body->e = shape_bound(z, i, 0, like->line);
            body->e2 = shape_bound(z, i, 1, like->line);
        } else {
            body = loop(z->vars[i], z->lb[i], z->hb[i], body, like);
        }
    }
    return body;
}

static struct expr *first_bound(struct shape *z, int high, int line)
{
    int v = high ? z->hb[0] : z->lb[0];
    return v == STARB ? shape_bound(z, 0, high, line) : int_const(v, line);
}

/* POLY(a, x): a(m) + a(m+1)*x(p) + a(m+2)*x(p)*x(p+1) + ..., the last
 * element of x standing for those beyond it; an element x for each of
 * them (Q3POLY, Q4ORD).  W is the product so far, K the subscript of x. */
static struct expr *poly(struct expr *e, struct stmt *like)
{
    int line = e->line;
    struct expr *a = e->arg[0], *x = e->arg[1], *xs = shape_of(x), *ae, *xe;
    struct shape za, zx;
    struct sym *t, *w, *k = NULL;
    struct stmt *body, *last;
    struct type ta, tx;

    if (a->k != E_VAR || shape_of(a) != a || cum_dims(a->sym) != 1 ||
        (xs && (x != xs || cum_dims(x->sym) != 1)))
        fatal("%s(%d): POLY needs one-dimensional arrays", src_name, line);
    open_shape(a, &za);
    ae = subst(a, za.vars, 1, za.lb, za.hb);
    ta = elem_type(subst(ae, NULL, 0, NULL, NULL));
    if (xs) {
        open_shape(x, &zx);
        k = zx.vars[0];
        xe = subst(x, zx.vars, 1, zx.lb, zx.hb);
    } else {
        xe = subst(x, NULL, 0, NULL, NULL);
    }
    tx = elem_type(subst(xe, NULL, 0, NULL, NULL));
    t = new_temp(reduce_type(ta, tx));
    w = new_temp(t->t);
    /* T = T + a(J) * W; IF J < n THEN DO; W = W * x(K);
     * IF K < q THEN K = K + 1; END */
    body = assign(t, binop(T_PLUS, var_ref(t, line), binop(T_STAR, ae, var_ref(w, line))), like);
    last = assign(w, binop(T_STAR, var_ref(w, line), xe), like);
    if (k) {
        struct stmt *inc = mk_stmt(S_IF, like);
        inc->e = binop(T_LT, var_ref(k, line), first_bound(&zx, 1, line));
        inc->body = assign(k, binop(T_PLUS, var_ref(k, line), int_const(1, line)), like);
        last->next = inc;
    }
    {
        struct stmt *more = mk_stmt(S_IF, like), *g = mk_stmt(S_GROUP, like);
        more->e = binop(T_LT, var_ref(za.vars[0], line), first_bound(&za, 1, line));
        g->body = last;
        more->body = g;
        body->next = more;
    }
    {
        struct stmt *l = assign(t, int_const(0, line), like);
        l->next = assign(w, int_const(1, line), like);
        if (k)
            l->next->next = assign(k, first_bound(&zx, 0, line), like);
        l = append(l, shape_loops(&za, body, like));
        pre = append(pre, l);
    }
    return var_ref(t, line);
}

/* A cross section as an argument: a temporary array of its shape, the
 * elements copied into it before the statement and back after it, so
 * that the procedure's changes reach the section (Q1XSEC). */
static struct expr *section_arg(struct expr *a)
{
    struct shape z;
    struct sym *t;
    struct stmt *like = mk_stmt(S_NULL, NULL), *in, *out;
    struct expr *tv;

    like->line = a->line;
    if (no_pre)
        fatal("%s(%d): a cross section in a WHILE or UNTIL condition is not supported yet",
              src_name, a->line);
    if (is_struct(a->sym))
        fatal("%s(%d): a cross section of a structure as an argument is not supported yet",
              src_name, a->line);
    open_shape(a, &z);
    t = xmalloc(sizeof *t);
    {
        char name[16];
        snprintf(name, sizeof name, "#X%d", ++nreduce);
        t->name = xstrdup(name);
    }
    t->t = a->sym->t;
    t->level = 1;
    t->root = t;
    t->owner = P;
    t->synthetic = 1;
    t->ndims = z.nv;
    for (int i = 0; i < z.nv; i++) {
        if (z.lb[i] == STARB)
            fatal("%s(%d): a cross section of an A(*) parameter as an argument is not "
                  "supported yet", src_name, a->line);
        t->lb[i] = z.lb[i];
        t->hb[i] = z.hb[i];
    }
    t->next = P->syms;
    P->syms = t;
    layout(t);
    {
        struct expr *subs[MAXDIMS * 4];
        for (int i = 0; i < z.nv; i++)
            subs[i] = var_ref(z.vars[i], a->line);
        tv = ref(t, subs, z.nv, a->line);
    }
    in = mk_stmt(S_ASSIGN, like);
    in->lhs = tv;
    in->e = star_elem(a, &z);
    out = mk_stmt(S_ASSIGN, like);
    out->lhs = star_elem(a, &z);
    out->e = tv;
    pre = append(pre, shape_loops(&z, in, like));
    post = append(post, shape_loops(&z, out, like));
    return var_ref(t, a->line);
}

/* A bit string constant of the bits written in b ('1', '0'). */
static struct expr *bit_const(const char *b, int line)
{
    struct expr *c = mk(E_BITC, line);
    c->str = xstrdup(b);
    c->len = (int)strlen(b);
    c->t.k = TY_BIT;
    c->t.p = c->len;
    return c;
}

/* EVERY(x), SOME(x), ANSI 9.4.4.33 and 9.4.4.73: one bit, '1'B if every
 * bit of every element of x as a bit string is 1 (a null string counts
 * as true), or if some bit is.  x may be an element.  T is the result:
 * EVERY: IF ^B ^= '0'B THEN T = '0'B; SOME: IF B ^= '0'B THEN T = '1'B. */
static struct expr *every_some(struct expr *e, struct stmt *like)
{
    int line = e->line, every = e->fn == B_EVERY;
    struct expr *a = e->arg[0], *sh = shape_of(a), *el;
    struct shape z;
    struct sym *t;
    struct stmt *ifs = mk_stmt(S_IF, like), *body;
    struct type rt, et;

    if (sh) {
        open_shape(sh, &z);
        el = subst(a, z.vars, z.nv, z.lb, z.hb);
    } else {
        el = subst(a, NULL, 0, NULL, NULL);
    }
    et = elem_type(subst(el, NULL, 0, NULL, NULL));
    if (!is_bit(et)) {
        struct expr *b = mk(E_CALL, line);
        b->fn = B_BIT;
        b->nargs = 1;
        b->arg[0] = el;
        el = b;
    }
    memset(&rt, 0, sizeof rt);
    rt.k = TY_BIT;
    rt.p = 1;
    t = new_temp(rt);
    if (every) {
        struct expr *n = mk(E_NOT, line);
        n->l = el;
        el = n;
    }
    ifs->e = binop(T_NE, el, bit_const("0", line));
    ifs->body = assign(t, bit_const(every ? "0" : "1", line), like);
    body = sh ? shape_loops(&z, ifs, like) : ifs;
    pre = append(pre, append(assign(t, bit_const(every ? "1" : "0", line), like), body));
    return var_ref(t, line);
}

/* DOT(x, y [, p [, q]]), ANSI 9.4.4.29: the sum of x(i) * y(i) over two
 * vectors.  Without p FLOAT of the common base and the greater
 * precision; with p (and q) of that precision, each product and the sum
 * kept at it: T = ADD(T, MULTIPLY(x(i), y(i), p, q), p, q). */
static struct expr *dot(struct expr *e, struct stmt *like)
{
    int line = e->line;
    struct expr *x = e->arg[0], *y = e->arg[1], *sx = shape_of(x), *ex, *ey, *sum;
    struct stmt *prod;
    struct shape z;
    struct sym *t;
    struct type tx, ty, rt;

    if (!sx || x != sx || cum_dims(x->sym) != 1 || shape_of(y) != y || cum_dims(y->sym) != 1)
        fatal("%s(%d): DOT needs two one-dimensional arrays", src_name, line);
    open_shape(x, &z);
    ex = subst(x, z.vars, 1, z.lb, z.hb);
    ey = subst(y, z.vars, 1, z.lb, z.hb);
    tx = elem_type(subst(ex, NULL, 0, NULL, NULL));
    ty = elem_type(subst(ey, NULL, 0, NULL, NULL));
    if (e->nargs == 2) {
        rt = reduce_type(tx, ty);
        t = new_temp(rt);
        sum = binop(T_PLUS, var_ref(t, line), binop(T_STAR, ex, ey));
    } else {
        struct expr *m = mk(E_CALL, line), *a = mk(E_CALL, line);
        memset(&rt, 0, sizeof rt);
        rt.k = (tx.k == TY_FIXBIN || tx.k == TY_FLTBIN || ty.k == TY_FIXBIN ||
                ty.k == TY_FLTBIN) ? TY_FIXBIN : TY_FIXDEC;
        if (is_float(tx) || is_float(ty))
            rt.k = rt.k == TY_FIXBIN ? TY_FLTBIN : TY_FLTDEC;
        if (e->arg[2]->k != E_NUM || (e->nargs == 4 && e->arg[3]->k != E_NUM))
            fatal("%s(%d): DOT's precision must be constants", src_name, line);
        rt.p = atoi(e->arg[2]->num);
        rt.q = e->nargs == 4 ? atoi(e->arg[3]->num) : 0;
        t = new_temp(rt);
        m->fn = B_MULTIPLY;
        a->fn = B_ADD;
        m->nargs = a->nargs = e->nargs;   /* x, y [, p [, q]] */
        m->arg[0] = ex;
        m->arg[1] = ey;
        a->arg[0] = var_ref(t, line);
        a->arg[1] = m;
        for (int i = 2; i < e->nargs; i++)
            m->arg[i] = a->arg[i] = e->arg[i];
        sum = a;
    }
    prod = assign(t, sum, like);
    pre = append(pre, append(assign(t, int_const(0, line), like), shape_loops(&z, prod, like)));
    return var_ref(t, line);
}

static struct expr *reduce(struct expr *e)
{
    int line = e->line;
    struct stmt *like = mk_stmt(S_NULL, NULL), *body, *init;
    struct expr *a = e->arg[0], *sh, *el;
    struct shape z;
    struct type et, rt;
    struct sym *t;

    like->line = line;
    if (no_pre)
        fatal("%s(%d): an array function in a WHILE or UNTIL condition is not supported yet",
              src_name, line);
    if (e->fn == B_POLY)
        return poly(e, like);
    if (e->fn == B_EVERY || e->fn == B_SOME)
        return every_some(e, like);
    if (e->fn == B_DOT)
        return dot(e, like);
    sh = shape_of(a);
    if (!sh)
        fatal("%s(%d): %s needs an array", src_name, line,
              e->fn == B_SUM ? "SUM" : e->fn == B_PROD ? "PROD" : e->fn == B_ALL ? "ALL" :
              "ANY");
    open_shape(sh, &z);
    el = subst(a, z.vars, z.nv, z.lb, z.hb);
    et = elem_type(subst(el, NULL, 0, NULL, NULL));
    if (e->fn == B_SUM || e->fn == B_PROD) {
        if (!arith(et) && !is_string(et))
            fatal("%s(%d): %s of a bit string array is not supported yet", src_name, line,
                  e->fn == B_SUM ? "SUM" : "PROD");
        memset(&rt, 0, sizeof rt);
        rt = reduce_type(et, rt);
        t = new_temp(rt);
        init = assign(t, int_const(e->fn == B_SUM ? 0 : 1, line), like);
        body = assign(t, binop(e->fn == B_SUM ? T_PLUS : T_STAR, var_ref(t, line), el), like);
    } else {
        /* ALL, ANY: & or | of the elements as bit strings, the shorter
         * extended with zeros (Q2BIT); F is set for the first */
        struct sym *f;
        struct stmt *ifs = mk_stmt(S_IF, like);
        if (!is_bit(et)) {
            struct expr *b = mk(E_CALL, line);
            b->fn = B_BIT;
            b->nargs = 1;
            b->arg[0] = el;
            el = b;
            et = elem_type(subst(el, NULL, 0, NULL, NULL));
        }
        memset(&rt, 0, sizeof rt);
        rt.k = et.k;
        rt.p = et.p;
        t = new_temp(rt);
        memset(&rt, 0, sizeof rt);
        rt.k = TY_FIXBIN;
        rt.p = 15;
        f = new_temp(rt);
        init = assign(f, int_const(1, line), like);
        ifs->e = binop(T_EQ, var_ref(f, line), int_const(1, line));
        ifs->body = assign(t, el, like);
        ifs->els = assign(t, binop(e->fn == B_ALL ? T_AND : T_OR, var_ref(t, line),
                                   subst(el, NULL, 0, NULL, NULL)), like);
        ifs->next = assign(f, int_const(0, line), like);
        body = ifs;
    }
    pre = append(pre, append(init, shape_loops(&z, body, like)));
    return var_ref(t, line);
}

/* ---- statements ------------------------------------------------------- */

static struct stmt *append(struct stmt *list, struct stmt *s)
{
    struct stmt **t = &list;
    while (*t)
        t = &(*t)->next;
    *t = s;
    return list;
}

/* Wrap body in loops over the open dimensions of target (sym with nsubs
 * subscripts given): new loop variables are appended to subs. */
static struct stmt *open_loops(struct sym *sym, struct expr **subs, int *nsubs,
                               struct sym **vars, int *nv, int *vlb, int *vhb, int first,
                               struct stmt *like, struct stmt *(*inner)(void *), void *ctx)
{
    int lb[MAXDIMS * 4], hb[MAXDIMS * 4], n = chain_bounds(sym, lb, hb);
    struct stmt *body;

    if (first >= n)
        return inner(ctx);
    {
        struct sym *v = new_loop_var(P);
        vars[*nv] = v;
        vlb[*nv] = lb[first];
        vhb[*nv] = hb[first];
        (*nv)++;
        subs[(*nsubs)++] = var_ref(v, like->line);
        body = open_loops(sym, subs, nsubs, vars, nv, vlb, vhb, first + 1, like, inner, ctx);
        if (lb[first] == STARB) {
            struct stmt *s = loop(v, 0, 0, body, like);
            s->e = bound_expr(sym, first, 0, like->line);
            s->e2 = bound_expr(sym, first, 1, like->line);
            return s;
        }
        return loop(v, lb[first], hb[first], body, like);
    }
}

/* Assignment of src to the reference (sym, subs), expanded. */
struct asg_ctx {
    struct sym  *sym;
    struct expr *subs[MAXDIMS * 4];
    int          nsubs;
    struct expr *src;          /* the source, before substitution */
    struct sym  *vars[MAXDIMS * 4];
    int          nv, vlb[MAXDIMS * 4], vhb[MAXDIMS * 4];
    struct stmt *like;
};

static struct stmt *expand_assign(struct sym *sym, struct expr **subs, int nsubs,
                                  struct expr *src, struct stmt *like,
                                  struct sym **vars, int nv, int *vlb, int *vhb);

static struct stmt *asg_inner(void *p)
{
    struct asg_ctx *c = p;
    return expand_assign(c->sym, c->subs, c->nsubs, c->src, c->like, c->vars, c->nv,
                         c->vlb, c->vhb);
}

/* The member of structure source src that corresponds to child number k
 * of the target; NULL if src is not a structure reference. */
static struct expr *member_of(struct expr *src, int k)
{
    struct sym *c;
    if (!src || src->k != E_VAR || !is_struct(src->sym))
        return NULL;
    c = src->sym->child;
    for (int i = 0; i < k && c; i++)
        c = c->sibling;
    if (!c)
        fatal("%s(%d): structure %s does not match the target", src_name, src->line,
              src->sym->name);
    return ref(c, src->sub, src->nsubs, src->line);
}

/* BY NAME: the expression with each structure operand replaced by its
 * member named name -- NULL if one of them has no such member, or it
 * is a structure where the target's is not, or the other way round
 * (Q4BYN, Q5BYN: only the names all of them have are assigned) */
static int byname;

static struct expr *named_member(struct expr *e, const char *name, int want_struct)
{
    struct expr *c;
    if (!e)
        return NULL;
    if (e->k == E_VAR && e->sym && is_struct(e->sym)) {
        for (struct sym *m = e->sym->child; m; m = m->sibling)
            if (strcmp(m->name, name) == 0)
                return (is_struct(m) != 0) == (want_struct != 0) ?
                       ref(m, e->sub, e->nsubs, e->line) : NULL;
        return NULL;
    }
    c = xmalloc(sizeof *c);
    *c = *e;
    if (e->l && !(c->l = named_member(e->l, name, want_struct)))
        return NULL;
    if (e->r && !(c->r = named_member(e->r, name, want_struct)))
        return NULL;
    if (e->k != E_FCALL)
        for (int i = 0; i < e->nargs; i++)
            if (e->arg[i] && !(c->arg[i] = named_member(e->arg[i], name, want_struct)))
                return NULL;
    return c;
}

static struct stmt *expand_assign(struct sym *sym, struct expr **subs, int nsubs,
                                  struct expr *src, struct stmt *like,
                                  struct sym **vars, int nv, int *vlb, int *vhb)
{
    int cum = cum_dims(sym);

    if (nsubs < cum) {
        /* open dimensions at this level: loops, then the same again */
        struct asg_ctx *c = xmalloc(sizeof *c);
        c->sym = sym;
        memcpy(c->subs, subs, (size_t)nsubs * sizeof *subs);
        c->nsubs = nsubs;
        c->src = src;
        memcpy(c->vars, vars, (size_t)nv * sizeof *vars);
        memcpy(c->vlb, vlb, (size_t)nv * sizeof *vlb);
        memcpy(c->vhb, vhb, (size_t)nv * sizeof *vhb);
        c->nv = nv;
        c->like = like;
        return open_loops(sym, c->subs, &c->nsubs, c->vars, &c->nv, c->vlb, c->vhb, nsubs,
                          like, asg_inner, c);
    }
    if (is_struct(sym) && byname) {
        struct stmt *list = NULL;
        for (struct sym *ch = sym->child; ch; ch = ch->sibling) {
            struct expr *m = named_member(src, ch->name, is_struct(ch));
            if (m)
                list = append(list, expand_assign(ch, subs, nsubs, m, like, vars, nv,
                                                  vlb, vhb));
        }
        if (!list) {
            /* nothing in common: a null statement */
            list = mk_stmt(S_GROUP, like);
        }
        return list;
    }
    if (is_struct(sym)) {
        struct stmt *list = NULL;
        int k = 0;
        for (struct sym *ch = sym->child; ch; ch = ch->sibling, k++) {
            struct expr *m = member_of(src, k);
            list = append(list, expand_assign(ch, subs, nsubs, m ? m : src, like, vars, nv,
                                              vlb, vhb));
        }
        return list;
    }
    {
        /* an array function in the source is evaluated again for each
         * element, after those assigned before (Q = Q / SUM(Q), t188sum) */
        struct stmt *s = mk_stmt(S_ASSIGN, like), *outer = pre;
        pre = NULL;
        s->lhs = ref(sym, subs, nsubs, like->line);
        s->e = subst(src, vars, nv, vlb, vhb);
        if (pre && nv > 0) {
            struct stmt *g = mk_stmt(S_GROUP, like);
            g->body = append(pre, s);
            pre = outer;
            return g;
        }
        pre = append(outer, pre);
        return s;
    }
}

/* PUT LIST of one item, expanded into PUT LIST statements. */
static struct stmt *expand_put(struct sym *sym, struct expr **subs, int nsubs,
                               struct expr *item, struct stmt *like);

struct put_ctx { struct sym *sym; struct expr *subs[MAXDIMS * 4]; int nsubs; struct stmt *like; };

static struct stmt *put_inner(void *p)
{
    struct put_ctx *c = p;
    return expand_put(c->sym, c->subs, c->nsubs, NULL, c->like);
}

static int expanding_get;       /* expand_put builds GET targets */
static struct stmt *cur_io;     /* ... of this PUT or GET */

/* One item of a PUT (a value) or of a GET (a target reference). */
static struct stmt *put_one(struct expr *e, struct stmt *like)
{
    struct stmt *s = mk_stmt(S_ITEM, like);
    s->owner_io = cur_io;
    if (expanding_get) {
        s->get = 1;
        s->lhs = e;
    } else {
        s->e = e;
    }
    return s;
}

static struct stmt *expand_put(struct sym *sym, struct expr **subs, int nsubs,
                               struct expr *item, struct stmt *like)
{
    int cum = cum_dims(sym);

    if (item && !(item->k == E_VAR && is_aggregate(item))) {
        if (has_aggregate(item))
            fatal("%s(%d): an array expression in PUT LIST is not supported yet", src_name,
                  like->line);
        return put_one(subst(item, NULL, 0, NULL, NULL), like);
    }
    if (nsubs < cum) {
        struct put_ctx *c = xmalloc(sizeof *c);
        struct sym *vars[MAXDIMS * 4];
        int nv = 0, vlb[MAXDIMS * 4], vhb[MAXDIMS * 4];
        c->sym = sym;
        memcpy(c->subs, subs, (size_t)nsubs * sizeof *subs);
        c->nsubs = nsubs;
        c->like = like;
        return open_loops(sym, c->subs, &c->nsubs, vars, &nv, vlb, vhb, nsubs, like,
                          put_inner, c);
    }
    if (is_struct(sym)) {
        struct stmt *list = NULL;
        for (struct sym *ch = sym->child; ch; ch = ch->sibling)
            list = append(list, expand_put(ch, subs, nsubs, NULL, like));
        return list;
    }
    {
        struct expr *s2[MAXDIMS * 4];
        for (int i = 0; i < nsubs; i++)
            s2[i] = subst(subs[i], NULL, 0, NULL, NULL);
        return put_one(ref(sym, s2, nsubs, like->line), like);
    }
}

static struct stmt *lower_list(struct stmt *s);
static struct stmt *ctl_inits(struct sym *s);

static void check_scalar(struct expr *e, int line)
{
    if (has_aggregate(e))
        fatal("%s(%d): an array or structure cannot be used here", src_name, line);
}

static struct stmt *lower_one(struct stmt *s);

/* ---- CHECK ------------------------------------------------------------- */

/* CHECK(name, ...) of the block and those around it (Q1CHK: SUB is
 * checked by the prefix of the procedure it is in) */
static const char *in_checks(const char *name)
{
    if (no_check)
        return NULL;
    for (struct proc *p = P; p; p = p->parent)
        for (int i = 0; i < p->nchecks; i++)
            if (strcmp(p->checks[i], name) == 0)
                return p->checks[i];
    return NULL;
}

/* The checked name of an assignment to x: x's or a containing
 * structure's (S.X under CHECK(S)) */
static const char *checked_var(struct sym *x)
{
    for (struct sym *y = x; y; y = y->parent) {
        const char *n = in_checks(y->name);
        if (n)
            return n;
    }
    return NULL;
}


/* SYSPRINT, as the block knows it: declared, or the implicit one of the
 * external procedure, made here if no statement used it */
static struct expr *sysprint_ref(int line)
{
    struct proc *top = P;
    struct sym *f;
    for (struct proc *p = P; p; p = p->parent) {
        top = p;
        for (struct sym *x = p->syms; x; x = x->next)
            if (x->is_file && strcmp(x->name, "SYSPRINT") == 0)
                return var_ref(x, line);
    }
    f = xmalloc(sizeof *f);
    f->name = xstrdup("SYSPRINT");
    f->line = line;
    f->level = 1;
    f->root = f;
    f->owner = top;
    f->implicit = 1;
    f->is_file = FL_STREAM | FL_OUTPUT | FL_PRINT;
    f->t.k = TY_FILE;
    f->t.p = 4;
    f->next = top->syms;
    top->syms = f;
    return var_ref(f, line);
}

/* CHECK(name) at this point: for a variable x, the system action writes
 * it whole, as PUT DATA does (ARR(2) = 7 writes ARR, S.X = 3 S.X) */
static struct stmt *check_stmt(const char *name, struct sym *x, struct stmt *like)
{
    struct stmt *c = mk_stmt(S_CHECK, like);
    c->cond = C_CHECK;
    c->cond_name = (char *)name;
    c->check_code = x ? 511 : 510;
    if (x) {
        struct stmt *put = mk_stmt(S_PUT, like);
        struct put_item *it = xmalloc(sizeof *it);
        it->e = var_ref(x, like->line);
        put->items = it;
        put->data = 1;
        put->skip = -1;
        put->file = sysprint_ref(like->line);
        c->body = lower_one(put);
    }
    return c;
}

/* The checks after a statement that assigns: its targets, in order. */
static struct stmt *checks_after(struct stmt *s)
{
    struct stmt *list = NULL;
    if (in_put_items)
        return NULL;
    if (s->k == S_ASSIGN || s->k == S_READ) {
        struct expr *t = s->lhs;
        if (t && t->k == E_CALL && t->fn == B_SUBSTR)
            t = t->arg[0];                  /* SUBSTR(C, ...) = ... */
        if (t && t->k == E_VAR && checked_var(t->sym))
            list = check_stmt(checked_var(t->sym), t->sym, s);
    } else if (s->k == S_GET) {
        for (struct put_item *it = s->items; it; it = it->next)
            if (!it->loop && it->e && it->e->k == E_VAR && checked_var(it->e->sym))
                list = append(list, check_stmt(checked_var(it->e->sym), it->e->sym, s));
    }
    return list;
}

/* The items of a PUT or GET: S_ITEMs, aggregates expanded; a repetitive
 * specification an S_ITER around its items' (Q1RFMT) */
static struct stmt *lower_items(struct put_item *items, struct stmt *s)
{
    struct stmt *list = NULL;
    for (struct put_item *it = items; it; it = it->next) {
        struct expr *e = it->e;
        if (it->loop) {
            struct stmt *lp = it->loop;
            lp->body = lower_items(it->sub, s);
            lp->next = NULL;
            in_put_items++;
            list = append(list, lower_one(lp));
            in_put_items--;
            continue;
        }
        if (e->k == E_VAR && is_aggregate(e) && (nstars(e) || (e->qual && !is_struct(e->sym)))) {
            /* a cross section: its elements, in row order (Q1XSEC) */
            struct shape z;
            if (is_struct(e->sym))
                fatal("%s(%d): a cross section of a structure cannot be transmitted yet",
                      src_name, s->line);
            open_shape(e, &z);
            list = append(list, shape_loops(&z, put_one(star_elem(e, &z), s), s));
        } else if (e->k == E_VAR && is_aggregate(e)) {
            struct expr *subs[MAXDIMS * 4];
            for (int i = 0; i < e->nsubs; i++)
                subs[i] = e->sub[i];
            if (e->qual)
                fatal("%s(%d): an array or structure with a pointer qualifier cannot be "
                      "transmitted as a whole yet", src_name, s->line);
            list = append(list, expand_put(e->sym, subs, e->nsubs, NULL, s));
        } else {
            if (s->k == S_GET && e->k != E_VAR)
                fatal("%s(%d): a GET target must be a variable", src_name, s->line);
            list = append(list, expand_put(NULL, NULL, 0, e, s));
        }
    }
    return list;
}

/* Whether e reads x's storage: a reference to it anywhere in e. */
static int reads_sym(struct expr *e, struct sym *x)
{
    if (!e)
        return 0;
    if (e->k == E_VAR && e->sym && e->sym->root == x->root)
        return 1;
    if (e->k == E_FCALL)
        return 1;                       /* a function may change anything */
    if (reads_sym(e->l, x) || reads_sym(e->r, x) || reads_sym(e->qual, x))
        return 1;
    for (int i = 0; i < e->nsubs; i++)
        if (reads_sym(e->sub[i], x))
            return 1;
    for (int i = 0; i < e->nargs; i++)
        if (reads_sym(e->arg[i], x))
            return 1;
    return 0;
}

static struct stmt *lower_one(struct stmt *s);

/* A, B, ... = e as single assignments, e evaluated once (Q1ADJ):
 * e itself into each target when no target's storage is read by e;
 * else e into the first and the first into the others, which have its
 * attributes. */
static struct stmt *multi_assign(struct stmt *s)
{
    struct stmt *list = NULL;
    int reads = 0, same = 1;
    struct expr *t[17];
    int n = 0;
    t[n++] = s->lhs;
    for (int i = 0; i < s->nmore; i++)
        t[n++] = s->more[i];
    for (int i = 0; i < n; i++) {
        if (t[i]->k != E_VAR || !t[i]->sym)
            fatal("%s(%d): a target of a multiple assignment must be a variable", src_name,
                  s->line);
        reads |= reads_sym(s->e, t[i]->sym);
        if (t[i]->sym->t.k != t[0]->sym->t.k || t[i]->sym->t.p != t[0]->sym->t.p ||
            t[i]->sym->t.q != t[0]->sym->t.q || t[i]->sym->t.pic != t[0]->sym->t.pic ||
            is_aggregate(t[i]) || t[i]->nsubs)
            same = 0;
    }
    if (reads && !same)
        fatal("%s(%d): a multiple assignment whose expression reads a target needs targets "
              "of one set of attributes, unsubscripted scalars (not supported yet)", src_name,
              s->line);
    s->nmore = 0;
    for (int i = 0; i < n; i++) {
        struct stmt *a = mk_stmt(S_ASSIGN, s);
        a->lhs = t[i];
        a->e = (reads && i > 0) ? t[0] : s->e;
        list = append(list, lower_one(a));
    }
    return list;
}

static struct stmt *lower_one(struct stmt *s)
{
    if (s->k == S_ASSIGN && s->nmore)
        return multi_assign(s);
    switch (s->k) {
    case S_ASSIGN: {
        struct expr *t = s->lhs;
        struct expr *subs[MAXDIMS * 4];
        if (t->k == E_CALL && (t->fn == B_COMPLETION || t->fn == B_STATUS || t->fn == B_REAL ||
                               t->fn == B_IMAG) && t->arg[0]->k == E_VAR &&
            t->arg[0]->nsubs == 0 && t->arg[0]->sym->ndims == 1 && !t->arg[0]->sym->parent &&
            !t->arg[0]->sym->stardims) {
            /* a pseudo-variable of a one-dimensional array: each element
             * takes the value (COMPLETION(EA) = '1'B, t228 Q1EVT) */
            struct sym *v = new_loop_var(P), *a = t->arg[0]->sym;
            struct stmt *b = mk_stmt(S_ASSIGN, s);
            struct expr *i = var_ref(v, s->line), *c = mk(E_CALL, s->line);
            check_scalar(s->e, s->line);
            *c = *t;
            c->arg[0] = ref(a, &i, 1, s->line);
            b->lhs = c;
            b->e = subst(s->e, NULL, 0, NULL, NULL);
            return loop(v, a->lb[0], a->hb[0], b, s);
        }
        struct sym *vars[MAXDIMS * 4];
        int vlb[MAXDIMS * 4], vhb[MAXDIMS * 4];
        if (is_aggregate(t) && t->qual && is_struct(t->sym))
            fatal("%s(%d): a structure with a pointer qualifier cannot be assigned "
                  "as a whole yet", src_name, s->line);
        if (is_aggregate(t) && (nstars(t) || (t->qual && !is_struct(t->sym)))) {
            /* a cross section: a loop for each *, the source by element
             * (Q1XSEC); array functions in it again for each */
            struct shape z;
            struct stmt *a = mk_stmt(S_ASSIGN, s), *outer = pre;
            if (is_struct(t->sym))
                fatal("%s(%d): a cross section of a structure cannot be assigned yet", src_name,
                      s->line);
            open_shape(t, &z);
            pre = NULL;
            a->lhs = star_elem(t, &z);
            a->e = subst(s->e, z.vars, z.nv, z.lb, z.hb);
            if (pre) {
                struct stmt *g = mk_stmt(S_GROUP, s);
                g->body = append(pre, a);
                a = g;
            }
            pre = outer;
            return shape_loops(&z, a, s);
        }
        if (!is_aggregate(t)) {
            check_scalar(s->e, s->line);
            s->e = subst(s->e, NULL, 0, NULL, NULL);
            s->lhs = subst(t, NULL, 0, NULL, NULL);
            return s;
        }
        for (int i = 0; i < t->nsubs; i++)
            subs[i] = subst(t->sub[i], NULL, 0, NULL, NULL);
        {
            struct stmt *r;
            byname = s->byname;
            r = expand_assign(t->sym, subs, t->nsubs, s->e, s, vars, 0, vlb, vhb);
            byname = 0;
            return r;
        }
    }
    case S_PUT:
    case S_GET: {
        /* The statement stays one statement; its items become a body of
         * S_ITEMs and loops, so an EDIT format list runs on across all of
         * them. */
        struct stmt *list = NULL;
        if (s->k == S_GET && s->data)
            return s;                     /* GET DATA: the run time's table */
        if (s->data == 2) {
            /* PUT DATA: every variable of the block, as declared (JOB00197) */
            struct put_item **tail = &s->items;
            struct sym *order[4096];
            int n = 0;
            if (P->depth != 0)
                fatal("%s(%d): PUT DATA without a list is supported in the main procedure "
                      "only", src_name, s->line);
            for (struct sym *x = P->syms; x; x = x->next)
                if (n < 4096 && x->level == 1 && !x->is_file && !x->is_entry && !x->synthetic &&
                    !x->implicit && x->st != ST_BASED && !x->param &&
                    (x->child || (x->t.k != TY_PTR && x->t.k != TY_OFFSET &&
                                  x->t.k != TY_AREA)))
                    order[n++] = x;
            for (int i = n - 1; i >= 0; i--) {
                struct put_item *it = xmalloc(sizeof *it);
                it->e = var_ref(order[i], s->line);
                it->in = NULL;
                it->next = NULL;
                *tail = it;
                tail = &it->next;
            }
        }
        expanding_get = s->k == S_GET;
        cur_io = s;
        list = lower_items(s->items, s);
        expanding_get = 0;
        s->items = NULL;
        s->body = list;
        return s;
    }
    case S_SIGNAL:
        if (s->cond == C_CHECK) {
            /* SIGNAL CHECK(x): as if x were assigned, or reached */
            for (struct proc *p = P; p; p = p->parent)
                for (struct sym *x = p->syms; x; x = x->next)
                    if (!x->synthetic && strcmp(x->name, s->cond_name) == 0 && !x->is_file &&
                        !x->is_entry && x->t.k != TY_LABEL && x->t.k != TY_ENTRY)
                        return check_stmt(s->cond_name, x, s);
            return check_stmt(s->cond_name, NULL, s);
        }
        return s;
    case S_ON:
    case S_REVERT:
    case S_BEGIN:
    case S_STOP:
    case S_ALLOC: {
        /* each ALLOCATE of a CONTROLLED variable applies its INITIAL
         * values again (JOB00320) */
        struct stmt *list = s;
        for (struct put_item *it = s->items; it; it = it->next)
            if (it->e->k == E_VAR && it->e->sym->st == ST_CTL) {
                no_check++;
                list = append(list, lower_list(ctl_inits(it->e->sym)));
                no_check--;
            }
        return list;
    }
    case S_FREE:
    case S_ENTRY:
        return s;
    case S_OPEN:
    case S_CLOSE:
    case S_ITEM:
    case S_READ:
    case S_WRITE:
    case S_REWRITE:
    case S_DELETE:
    case S_LOCATE:
    case S_FORMAT:
        return s;                         /* INTO/FROM take the whole item */
    case S_WAIT:
    case S_DELAY:
        return s;
    case S_DISPLAY:
        check_scalar(s->e, s->line);
        s->e = subst(s->e, NULL, 0, NULL, NULL);
        if (s->e2)
            s->e2 = subst(s->e2, NULL, 0, NULL, NULL);
        if (s->e3)
            s->e3 = subst(s->e3, NULL, 0, NULL, NULL);
        return s;
    case S_IF:
        check_scalar(s->e, s->line);
        s->e = subst(s->e, NULL, 0, NULL, NULL);
        if (s->body) s->body = lower_list(s->body);
        if (s->els) s->els = lower_list(s->els);
        return s;
    case S_WHILE:
        check_scalar(s->e, s->line);
        check_scalar(s->e2, s->line);
        no_pre = 1;
        if (s->e)
            s->e = subst(s->e, NULL, 0, NULL, NULL);
        if (s->e2)
            s->e2 = subst(s->e2, NULL, 0, NULL, NULL);    /* UNTIL */
        no_pre = 0;
        s->body = lower_list(s->body);
        return s;
    case S_SELECT:
        check_scalar(s->e, s->line);
        if (s->e)
            s->e = subst(s->e, NULL, 0, NULL, NULL);
        s->body = lower_list(s->body);
        if (s->els)
            s->els = lower_list(s->els);
        return s;
    case S_WHEN:
        for (struct put_item *it = s->items; it; it = it->next) {
            check_scalar(it->e, s->line);
            it->e = subst(it->e, NULL, 0, NULL, NULL);
        }
        s->body = lower_list(s->body);
        return s;
    case S_LEAVE:
    case S_CHECK:
    case S_FETCH:
    case S_RELEASE:
        return s;
    case S_ITER:
        for (struct dospec *d = s->specs; d; d = d->next) {
            struct expr **es[6] = { &d->e1, &d->e2, &d->e3, &d->w, &d->u, &d->rep };
            for (int i = 0; i < 6; i++)
                if (*es[i]) {
                    check_scalar(*es[i], s->line);
                    no_pre = i == 3 || i == 4;         /* WHILE, UNTIL */
                    *es[i] = subst(*es[i], NULL, 0, NULL, NULL);
                    no_pre = 0;
                }
        }
        check_scalar(s->e, s->line);
        check_scalar(s->e2, s->line);
        s->e = subst(s->e, NULL, 0, NULL, NULL);
        s->e2 = subst(s->e2, NULL, 0, NULL, NULL);
        s->body = lower_list(s->body);
        if (s->target && !in_put_items && checked_var(s->target)) {
            /* each value the control variable takes, the last one after
             * the loop (Q1CHK: A=1, A=2, A=3) */
            const char *n = checked_var(s->target);
            s->body = append(check_stmt(n, s->target, s), s->body);
            return append(s, check_stmt(n, s->target, s));
        }
        return s;
    case S_GROUP:
        s->body = lower_list(s->body);
        return s;
    case S_CALL:
    case S_RETURN:
        if (s->e) {
            if (s->k == S_RETURN)
                check_scalar(s->e, s->line);
            s->e = subst(s->e, NULL, 0, NULL, NULL);
        }
        return s;
    case S_NULL:
    case S_GOTO:
    case S_ADJUST:
        return s;
    }
    return s;
}

/* Lower every statement; one statement may become several, which are
 * spliced in.  An IF's THEN or ELSE unit that becomes several statements
 * is wrapped in a DO group. */
static struct stmt *lower_list(struct stmt *s)
{
    struct stmt *head = NULL;
    while (s) {
        struct stmt *next = s->next, *l, *outer = pre, *outpost = post, *lchk = NULL, *after;
        s->next = NULL;
        pre = post = NULL;
        after = checks_after(s);
        for (int i = 0; i < s->nlabels && !in_put_items; i++)
            if (in_checks(s->labels[i]))
                lchk = append(lchk, check_stmt(in_checks(s->labels[i]), NULL, s));
        l = lower_one(s);
        if (after)
            l = append(l, after);
        if (lchk) {
            /* CHECK of a label: before the statement, reached in any way;
             * the labels go to the check */
            struct stmt *h = l;
            for (int i = 0; i < s->nlabels; i++) {
                lchk->labels[i] = s->labels[i];
                lchk->label_id[i] = s->label_id[i];
            }
            lchk->nlabels = s->nlabels;
            if (h)
                h->nlabels = 0;
            s->nlabels = 0;
            l = append(lchk, l);
        }
        if (post) {
            /* cross sections passed: assigned back */
            l = append(l, post);
        }
        post = outpost;
        if (pre) {
            /* the array functions it uses, computed first */
            l = append(pre, l);
            pre = outer;
            if (s->nlabels) {
                for (int i = 0; i < s->nlabels; i++) {
                    l->labels[i] = s->labels[i];
                    l->label_id[i] = s->label_id[i];
                }
                l->nlabels = s->nlabels;
                s->nlabels = 0;
            }
            head = append(head, l);
            s = next;
            continue;
        }
        pre = outer;
        if (l && l != s && s->nlabels) {
            /* the labels go to the first statement of the expansion */
            for (int i = 0; i < s->nlabels; i++) {
                l->labels[i] = s->labels[i];
                l->label_id[i] = s->label_id[i];
            }
            l->nlabels = s->nlabels;
        }
        head = append(head, l);
        s = next;
    }
    return head;
}

static struct stmt *single(struct stmt *l, struct stmt *like)
{
    if (l && l->next) {
        struct stmt *g = mk_stmt(S_GROUP, like);
        g->body = l;
        return g;
    }
    return l;
}

/* IF units are single statements: wrap expansions. */
static void wrap_units(struct stmt *s)
{
    for (; s; s = s->next) {
        if (s->k == S_IF) {
            s->body = single(s->body, s);
            s->els = single(s->els, s);
            wrap_units(s->body);
            wrap_units(s->els);
        } else if (s->k == S_GROUP || s->k == S_WHILE || s->k == S_ITER || s->k == S_PUT ||
                   s->k == S_GET) {
            wrap_units(s->body);
        }
    }
}

/* ---- INITIAL ----------------------------------------------------------- */

/* Assignments for the INITIAL values of an AUTOMATIC item, in element
 * order (rows first), iteration factors expanded. */
static struct stmt *init_stmts(struct sym *s)
{
    int lb[MAXDIMS * 4], hb[MAXDIMS * 4], n = chain_bounds(s, lb, hb), idx[MAXDIMS * 4];
    int total = 1, item = 0, rep = 0;
    struct stmt *list = NULL, like;

    memset(&like, 0, sizeof like);
    like.line = s->line;
    like.text = "INITIAL";
    if (s->init && n == 0) {
        struct stmt *a = mk_stmt(S_ASSIGN, &like);
        a->lhs = var_ref(s, s->line);
        a->e = s->init;
        return a;
    }
    if (!s->ninits)
        return NULL;
    for (int i = 0; i < s->ninits; i++)
        if (s->inits[i].repx) {
            /* an iteration factor that is an expression (Q5INIF): the
             * elements counted at run time, K the next one's subscript:
             * DO J = 1 TO f; IF K <= hb THEN DO; x(K) = v; K = K + 1;
             * END; END; for each item */
            struct sym *k, *j;
            if (n != 1)
                fatal("%s(%d): %s: an iteration factor that is an expression is "
                      "supported for arrays of one dimension only", src_name, s->line,
                      s->name);
            k = new_loop_var(P);
            list = assign(k, int_const(lb[0], s->line), &like);
            for (int it = 0; it < s->ninits; it++) {
                struct stmt *body = NULL, *g = mk_stmt(S_GROUP, &like), *f = mk_stmt(S_IF, &like);
                struct expr *sub1[1];
                if (s->inits[it].e) {
                    struct stmt *a = mk_stmt(S_ASSIGN, &like);
                    sub1[0] = var_ref(k, s->line);
                    a->lhs = ref(s, sub1, 1, s->line);
                    a->e = s->inits[it].e;
                    body = a;
                }
                body = append(body, assign(k, binop(T_PLUS, var_ref(k, s->line),
                                                    int_const(1, s->line)), &like));
                g->body = body;
                f->e = binop(T_LE, var_ref(k, s->line), int_const(hb[0], s->line));
                f->body = g;
                if (s->inits[it].repx) {
                    struct stmt *d = mk_stmt(S_ITER, &like);
                    j = new_loop_var(P);
                    d->target = j;
                    d->e = int_const(1, s->line);
                    d->e2 = s->inits[it].repx;
                    d->body = f;
                    list = append(list, d);
                } else {
                    list = append(list, s->inits[it].rep == 1 ? f :
                                  loop(new_loop_var(P), 1, s->inits[it].rep, f, &like));
                }
            }
            return list;
        }
    for (int i = 0; i < n; i++) {
        total *= hb[i] - lb[i] + 1;
        idx[i] = lb[i];
    }
    for (int el = 0; el < total && item < s->ninits; el++) {
        struct stmt *a = mk_stmt(S_ASSIGN, &like);
        struct expr *subs[MAXDIMS * 4];
        for (int i = 0; i < n; i++)
            subs[i] = int_const(idx[i], s->line);
        a->lhs = ref(s, subs, n, s->line);
        a->e = s->inits[item].e;
        if (a->e)                       /* '*': the element is left as it is */
            list = append(list, a);
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
    return list;
}

/* ---- DEFINED ---------------------------------------------------------- */

/* The kind of a DEFINED item, once its base is known (JOB00274):
 *   - a CHARACTER or BIT scalar on a CHARACTER or BIT scalar is an
 *     overlay: it shares the base's bytes from POSITION(n) on (1 by
 *     default), and is addressed as a member of the base would be;
 *   - otherwise item and base have the same attributes, and an element
 *     of the item is an element of the base: the base reference with
 *     each iSUB replaced by the item's i-th subscript.  No subscripts on
 *     the base is correspondence defining, iSUB i for the i-th. */
static void define(struct sym *s)
{
    struct expr *b = s->defined;
    struct sym *base = b->sym;
    int bdims;

    if (!base || base->child || base->is_file || base->is_entry || base->defined)
        fatal("%s(%d): %s: DEFINED on a variable that is not a structure, nor DEFINED "
              "itself, is all that is supported", src_name, s->line, s->name);
    bdims = cum_dims(base);
    if (s->ndims == 0 && bdims == 0 && b->nsubs == 0 &&
        ((s->t.k == TY_CHAR && (base->t.k == TY_CHAR || base->t.k == TY_PIC)) ||
         (s->t.k == TY_BIT && base->t.k == TY_BIT))) {
        /* a numeric picture's characters are a string for this */
        int at = s->position ? s->position : 1;
        int blen = base->t.k == TY_PIC ? pic_length(base->t) : base->t.p;
        if (at < 1 || at - 1 + s->t.p > blen)
            fatal("%s(%d): %s does not fit in %s from position %d", src_name, s->line, s->name,
                  base->name, at);
        if (s->t.k == TY_BIT && (at - 1) % 8)
            fatal("%s(%d): %s: a BIT string defined from a position inside a byte is not "
                  "supported yet", src_name, s->line, s->name);
        s->overlay = 1;
        s->parent = base;
        s->root = base->root;
        s->st = base->root->st;
        s->moff = s->t.k == TY_CHAR ? at - 1 : (at - 1) / 8;
        return;
    }
    if (s->position)
        fatal("%s(%d): %s: POSITION is for a string defined on a string", src_name, s->line,
              s->name);
    if (s->t.k != base->t.k || s->t.p != base->t.p || s->t.q != base->t.q)
        fatal("%s(%d): %s: an item DEFINED on %s needs its attributes (only a string may "
              "overlay a string)", src_name, s->line, s->name, base->name);
    if (b->nsubs == 0) {
        if (s->ndims != bdims)
            fatal("%s(%d): %s and %s need the same number of dimensions", src_name, s->line,
                  s->name, base->name);
        for (int i = 0; i < s->ndims; i++) {
            struct expr *x = mk(E_ISUB, s->line);
            x->len = i + 1;
            b->sub[b->nsubs++] = x;
        }
    } else if (s->ndims == 0) {
        /* a "deferred feature" of PL/I (F) (IEM0609I, JOB00281) */
        fatal("%s(%d): %s: a scalar DEFINED on a subscripted base is not supported", src_name,
              s->line, s->name);
    } else if (b->nsubs != bdims) {
        fatal("%s(%d): %s: the base reference needs %d subscripts", src_name, s->line,
              s->name, bdims);
    }
}

/* A copy of base subscript e with iSUB i replaced by subs[i-1]. */
static struct expr *isub_copy(struct expr *e, struct expr **subs, int n)
{
    struct expr *c;
    if (!e)
        return NULL;
    if (e->k == E_ISUB) {
        if (e->len < 1 || e->len > n)
            fatal("%s(%d): %dSUB: the defined item has %d dimensions", src_name, e->line,
                  e->len, n);
        return isub_copy(subs[e->len - 1], NULL, 0);
    }
    c = xmalloc(sizeof *c);
    *c = *e;
    c->l = isub_copy(e->l, subs, n);
    c->r = isub_copy(e->r, subs, n);
    c->qual = isub_copy(e->qual, subs, n);
    for (int i = 0; i < e->nsubs; i++)
        c->sub[i] = isub_copy(e->sub[i], subs, n);
    for (int i = 0; i < e->nargs; i++)
        c->arg[i] = isub_copy(e->arg[i], subs, n);
    return c;
}

/* Every element of a DEFINED item (not an overlay) made one of its base. */
static void undef_expr(struct expr *e)
{
    struct sym *s;
    struct expr *subs[MAXCALLARGS];
    if (!e)
        return;
    undef_expr(e->l);
    undef_expr(e->r);
    undef_expr(e->qual);
    for (int i = 0; i < e->nsubs; i++)
        undef_expr(e->sub[i]);
    for (int i = 0; i < e->nargs; i++)
        undef_expr(e->arg[i]);
    if (e->k != E_VAR || !e->sym || !e->sym->defined || e->sym->overlay)
        return;
    s = e->sym;
    if (e->nsubs != s->ndims)
        fatal("%s(%d): %s, DEFINED on %s, only element by element here", src_name, e->line,
              s->name, s->defined->sym->name);
    for (int i = 0; i < e->nsubs; i++)
        subs[i] = e->sub[i];
    e->sym = s->defined->sym;
    e->nsubs = s->defined->nsubs;
    for (int i = 0; i < e->nsubs; i++)
        e->sub[i] = isub_copy(s->defined->sub[i], subs, s->ndims);
}

static void undef_stmts(struct stmt *s);

static void undef_item(struct put_item *it)
{
    if (it->loop) {
        undef_stmts(it->loop);
        for (struct put_item *x = it->sub; x; x = x->next)
            undef_item(x);
    } else {
        undef_expr(it->e);
    }
}

static void undef_stmts(struct stmt *s)
{
    for (; s; s = s->next) {
        if (s->k == S_ITEM && s->owner_io && s->owner_io->data && s->e && s->e->k == E_VAR &&
            s->e->sym && s->e->sym->defined)
            /* PL/I (F) refuses it too (IEM0692I, JOB00274) */
            fatal("%s(%d): %s is DEFINED: not in a data-directed list", src_name, s->line,
                  s->e->sym->name);
        undef_expr(s->lhs);
        undef_expr(s->e);
        undef_expr(s->e2);
        undef_expr(s->e3);
        undef_expr(s->string);
        undef_expr(s->iter_ref);
        for (struct dospec *d = s->k == S_ITER ? s->specs : NULL; d; d = d->next) {
            undef_expr(d->e1);
            undef_expr(d->e2);
            undef_expr(d->e3);
            undef_expr(d->w);
            undef_expr(d->u);
            undef_expr(d->rep);
        }
        for (struct put_item *it = s->items; it; it = it->next)
            undef_item(it);
        if (s->k == S_ITER && s->target && s->target->defined)
            fatal("%s(%d): %s is DEFINED: not a control variable", src_name, s->line,
                  s->target->name);
        undef_stmts(s->body);
        undef_stmts(s->els);
    }
}

/* ---- PICTURE -------------------------------------------------------- */

/* A numeric picture has two values (JOB00290): its characters where a
 * string is wanted -- ||, the string built-in functions, assignment to a
 * string, PUT -- and its arithmetic value everywhere else, comparison
 * with a string included.  The arithmetic value is B_PICVAL of the
 * characters. */
static void pic_expr(struct expr *e, int str)
{
    if (!e)
        return;
    switch (e->k) {
    case E_VAR:
        for (int i = 0; i < e->nsubs; i++)
            pic_expr(e->sub[i], 0);
        pic_expr(e->qual, 0);
        if (e->sym && e->sym->t.k == TY_PIC && !e->picstr) {
            if (str) {
                e->picstr = 1;
            } else {
                struct expr *v = xmalloc(sizeof *v);
                *v = *e;
                v->picstr = 1;
                memset(e, 0, sizeof *e);
                e->k = E_CALL;
                e->fn = B_PICVAL;
                e->nargs = 1;
                e->arg[0] = v;
                e->line = v->line;
            }
        }
        return;
    case E_BIN:
        pic_expr(e->l, e->op == T_CAT);
        pic_expr(e->r, e->op == T_CAT);
        return;
    case E_CALL:
        for (int i = 0; i < e->nargs; i++) {
            int s = 0;
            switch (e->fn) {
            case B_LENGTH: case B_INDEX: case B_VERIFY: case B_TRANSLATE: case B_UNSPEC:
            case B_ADDR: case B_AFTER: case B_BEFORE: case B_REVERSE: case B_VALID: case B_STRING:
            case B_STG: case B_CSTG:
                s = 1;
                break;
            case B_SUBSTR: case B_REPEAT: case B_COPY:
                s = i == 0;
                break;
            case B_DECAT:
                s = i < 2;
                break;
            default:
                break;
            }
            if (e->fn != B_PICVAL)
                pic_expr(e->arg[i], s);
        }
        return;
    default:
        pic_expr(e->l, 0);
        pic_expr(e->r, 0);
        for (int i = 0; i < e->nargs; i++)
            pic_expr(e->arg[i], 0);
        return;
    }
}

static int is_pic(struct expr *e)
{
    return e && e->k == E_VAR && e->sym && (e->sym->t.k == TY_PIC || e->sym->t.pic);
}

/* The target of an assignment or GET: its subscripts only. */
static void pic_target(struct expr *e)
{
    if (e && e->k == E_VAR) {
        for (int i = 0; i < e->nsubs; i++)
            pic_expr(e->sub[i], 0);
        pic_expr(e->qual, 0);
    }
}

static void pic_stmts(struct stmt *s);

static void pic_item(struct put_item *it)
{
    if (it->loop) {
        pic_stmts(it->loop);
        for (struct put_item *x = it->sub; x; x = x->next)
            pic_item(x);
    } else if (!is_pic(it->e)) {
        pic_expr(it->e, 0);
    }
}

static void pic_stmts(struct stmt *s)
{
    for (; s; s = s->next) {
        switch (s->k) {
        case S_ASSIGN:
            pic_target(s->lhs);
            pic_expr(s->e, s->lhs->k == E_VAR && s->lhs->sym && is_string(s->lhs->sym->t));
            break;
        case S_ITEM:
            if (s->get) {
                if (is_pic(s->e))
                    fatal("%s(%d): GET into a picture is not supported yet", src_name,
                          s->line);
                pic_target(s->e);
            } else if (is_pic(s->e) && s->e->sym->t.k == TY_PIC) {
                pic_target(s->e);        /* PUT: both values, gen.c */
            } else {
                pic_expr(s->e, 0);
            }
            break;
        case S_ITER:
            if (s->target && s->target->t.k == TY_PIC)
                fatal("%s(%d): a picture as control variable is not supported", src_name,
                      s->line);
            pic_expr(s->e, 0);
            pic_expr(s->e2, 0);
            pic_expr(s->e3, 0);
            for (struct dospec *d = s->specs; d; d = d->next) {
                pic_expr(d->e1, 0);
                pic_expr(d->e2, 0);
                pic_expr(d->e3, 0);
                pic_expr(d->w, 0);
                pic_expr(d->u, 0);
                pic_expr(d->rep, 0);
            }
            break;
        default:
            pic_target(s->lhs);
            pic_expr(s->e, 0);
            pic_expr(s->e2, 0);
            pic_expr(s->e3, 0);
            pic_expr(s->string, 1);
            break;
        }
        for (struct put_item *it = s->items; it; it = it->next)
            pic_item(it);
        pic_stmts(s->body);
        pic_stmts(s->els);
    }
}

/* The INITIAL assignments of a CONTROLLED variable and its members, in
 * the order they are declared. */
static struct stmt *ctl_inits(struct sym *s)
{
    struct stmt *list = NULL;
    if (s->initcall) {
        /* INITIAL CALL: a copy for each ALLOCATE */
        struct stmt *c = xmalloc(sizeof *c);
        *c = *s->initcall;
        c->next = NULL;
        list = c;
    }
    if (!s->child)
        return append(list, (s->init || s->ninits) ? init_stmts(s) : NULL);
    for (struct sym *c = s->child; c; c = c->sibling)
        list = append(list, ctl_inits(c));
    return list;
}

static void lower_proc(struct proc *p)
{
    struct stmt *inits = NULL;

    P = p;
    check_stmt_subs(p->body);
    for (struct sym *s = p->syms; s; s = s->next)
        if (s->root == s)
            layout(s);
    for (struct sym *s = p->syms; s; s = s->next)
        if (s->refer) {
            /* REFER: laid out with one unit -- an element of the leading
             * dimension, or a character; ALLOCATE adds the others (gen.c) */
            s->refer_from->t = s->refer_from->sym->t;
            if (s->refer_dim < 0) {
                s->refer_unit = 1;
                s->t.p = 256;              /* typed as CHAR(*) parameters are */
            } else {
                s->refer_unit = elem_size(s->t);
                for (int i = 1; i < s->ndims; i++)
                    s->refer_unit *= s->hb[i] - s->lb[i] + 1;
            }
        }
    for (struct sym *s = p->syms; s; s = s->next)
        if (s->defined)
            define(s);
    /* INITIAL of AUTOMATIC items, in declaration order */
    {
        struct sym *order[4096];
        int n = 0;
        for (struct sym *s = p->syms; s; s = s->next)
            if (n < 4096)
                order[n++] = s;
        for (int i = n - 1; i >= 0; i--) {
            struct sym *s = order[i];
            if (s->adjust && s->root == s && s->st == ST_AUTO) {
                /* bounds and storage first, at the block's entry (Tier 81) */
                struct stmt *a = mk_stmt(S_ADJUST, NULL);
                a->line = s->line;
                a->text = "DECLARE";
                a->target = s;
                inits = append(inits, a);
            }
            if (s->root->st == ST_AUTO && !s->child && (s->init || s->ninits))
                inits = append(inits, init_stmts(s));
            if (s->initcall && s->root->st == ST_AUTO)
                inits = append(inits, s->initcall);   /* INITIAL CALL */
            else if (s->initcall && s->root->st != ST_CTL)
                error_at(s->line, "%s: INITIAL CALL is for AUTOMATIC and CONTROLLED "
                         "variables", s->name);
            else if (s->root->st == ST_BASED && (s->init || s->ninits))
                error_at(s->line, "%s: INITIAL on a BASED variable is not supported yet",
                         s->name);
        }
    }
    {
        /* INITIAL is not an assignment CHECK sees (Q1CHK) */
        struct stmt *il;
        no_check++;
        il = lower_list(inits);
        no_check--;
        p->body = append(il, lower_list(p->body));
    }
    wrap_units(p->body);
    undef_stmts(p->body);
    pic_stmts(p->body);
}

/* The conditions enabled in a block and in each statement: the block's
 * prefix applies to all of it and to the blocks inside, a statement's to
 * the statement only. */
static int apply_prefix(int m, int on, int off)
{
    return ((m | on) & ~off) | EN_SET;
}

static void stmt_masks(struct stmt *s, int m)
{
    for (; s; s = s->next) {
        s->mask = apply_prefix(m, s->pon, s->poff);
        stmt_masks(s->body, m);
        stmt_masks(s->els, m);
    }
}

static void lower_unit(struct proc *p)
{
    p->mask = apply_prefix(p->parent ? p->parent->mask : EN_DEFAULT, p->pon, p->poff);
    stmt_masks(p->body, p->mask);
    lower_proc(p);
    for (struct proc *c = p->child; c; c = c->sibling)
        lower_unit(c);
}

void lower_program(struct proc *p)
{
    for (struct proc *u = p; u; u = u->next_ext)
        lower_unit(u);
}
