/* check.c -- types of expressions, the PL/I (F) rules.
 *
 * GC28-8201-4, arithmetic operations, as measured against PL/I (F) V5.5
 * (internals/research/pli/tier1-measurement.md):
 *  - FIXED BINARY with FIXED BINARY:
 *       + -  q = max(q1,q2), p = min(31, max(p1-q1,p2-q2)+q+1)
 *       *    p = min(31, p1+p2+1), q = q1+q2
 *       /    p = 31, q = 31-((p1-q1)+q2)          (a scaled binary)
 *  - FIXED DECIMAL with FIXED DECIMAL: the same with 15 for 31.
 *  - FIXED BINARY with FIXED DECIMAL: the decimal operand is converted to
 *    FIXED BINARY(1+CEIL(p*3.32), CEIL(q*3.32)) first.
 *  - Built-ins: MOD (p2-q2+max(q1,q2), the result in [0,|y|)), ABS, MAX/MIN
 *    (max(pi-qi)+max(qi)), INDEX and LENGTH FIXED BINARY(15).
 *  - A number becomes a string of p+3 characters (PUT LIST's field); a
 *    FIXED BINARY counts as its decimal conversion.
 */
#include <stdlib.h>
#include <string.h>

#include "pl1c.h"

static int min(int a, int b) { return a < b ? a : b; }
static int max(int a, int b) { return a > b ? a : b; }

int is_float(struct type t) { return t.k == TY_FLTDEC || t.k == TY_FLTBIN; }
int arith(struct type t) { return t.k == TY_FIXBIN || t.k == TY_FIXDEC || is_float(t); }
int is_string(struct type t) { return t.k == TY_CHAR || t.k == TY_VCHAR; }

/* COUNT is used: each item transmitted is counted (gen.c) */
int uses_count;
int uses_strz;

static int ceil_mul332(int x) { return (x * 332 + 99) / 100; }
static int ceil_div332(int x) { return (x * 100 + 331) / 332; }

/* FLOAT DECIMAL(p) is long above 6 digits, FLOAT BINARY(p) above 21
 * bits; PUT LIST writes ceil(p/3.32) digits of a binary one (JOB00156). */
int flt_long(struct type t) { return t.k == TY_FLTDEC ? t.p > 6 : t.p > 21; }
int flt_digits(struct type t) { return t.k == TY_FLTDEC ? t.p : ceil_div332(t.p); }

/* The FLOAT type an arithmetic value takes in a FLOAT operation: FIXED
 * keeps its base and precision. */
static struct type float_of(struct type t)
{
    if (!is_float(t)) {
        t.k = t.k == TY_FIXBIN ? TY_FLTBIN : TY_FLTDEC;
        t.q = 0;
    }
    return t;
}

/* The result of a FLOAT operation: bases that differ are binary, the
 * decimal precision p becoming ceil(p*3.32); the larger precision wins
 * (JOB00156: A + I with I FIXED BIN(31) is FLOAT BINARY(31)). */
struct type float_common(struct type a, struct type b)
{
    a = float_of(a);
    b = float_of(b);
    if (a.k != b.k) {
        struct type *d = a.k == TY_FLTDEC ? &a : &b;
        d->k = TY_FLTBIN;
        d->p = min(53, ceil_mul332(d->p));
    }
    a.p = max(a.p, b.p);
    return a;
}

int bin_prec(struct type t)
{
    if (t.k == TY_FIXBIN)
        return t.p;
    return min(31, 1 + ceil_mul332(t.p));
}

int bin_scale(struct type t)
{
    return t.k == TY_FIXBIN ? t.q : ceil_mul332(t.q);
}

int dec_prec(struct type t)
{
    return t.k == TY_FIXDEC ? t.p : 1 + ceil_div332(t.p);
}

int dec_scale(struct type t)
{
    return t.k == TY_FIXDEC ? t.q : ceil_div332(t.q);
}

/* Length of the string a value becomes (the PUT LIST field). */
static int string_len(struct type t)
{
    if (is_string(t))
        return t.p;
    if (is_float(t))
        return flt_digits(t) + 6;
    if (arith(t))
        return dec_prec(t) + 3;
    return t.p;
}

/* The common arithmetic type of two operands, before the operation. */
static void common(struct type *a, struct type *b)
{
    if (a->k == b->k)
        return;
    {
        struct type *d = a->k == TY_FIXDEC ? a : b;
        int p = bin_prec(*d), q = bin_scale(*d);
        d->k = TY_FIXBIN;
        d->p = p;
        d->q = q;
    }
}

static void type_of(struct expr *e);
static struct proc *CUR;
static int cx_expr(struct expr *e);
static int cx_compare(struct expr *e);
static void cx_stmt(struct stmt *s);
static struct expr *cx_put(struct expr *e);
static void type_cplxfn(struct expr *e);
static void type_cpxstr(struct expr *e);
static void cx_abs(struct expr *e);

/* A string where a number is needed: converted, as PL/I (F) does, to
 * FIXED DECIMAL(15,q), truncating (JOB00115: '1.5' * 2 is 2, and '1.236'
 * assigned to DEC(7,2) is 1.23). */
/* a long FLOAT, eight bytes; a short one has four */
static int float_long(struct type t)
{
    return t.p > (t.k == TY_FLTBIN ? 21 : 6);
}

static struct expr *conv(struct expr *e, int q)
{
    struct expr *c = xmalloc(sizeof *c);
    if (e->k == E_STR) {
        /* PL/I (F) converts a constant when it compiles, and a constant
         * that is no number is a severe error there (IEM1804I, JOB00137) */
        int bad = 0;
        for (int i = 0; i < e->len; i++)
            if (!strchr("0123456789+-. E", e->str[i]))
                bad = 1;
        if (bad)
            error_at(e->line, "the constant '%.*s' cannot be converted to a number", e->len,
                     e->str);
    }
    memset(c, 0, sizeof *c);
    c->k = E_CONV;
    c->l = e;
    c->line = e->line;
    c->t.k = TY_FIXDEC;
    c->t.p = 15;
    c->t.q = q;
    return c;
}

/* A string meeting an arithmetic operand: FIXED DECIMAL(15,0), or FLOAT
 * DECIMAL(15) next to a FLOAT (JOB00166: 3.25 + '1.5' is 4.75, printed
 * with 15 digits). */
static struct expr *conv_with(struct expr *e, struct type other)
{
    struct expr *c = conv(e, 0);
    if (is_float(other)) {
        c->t.k = TY_FLTDEC;
        c->t.p = 15;
        c->t.q = 0;
    }
    return c;
}

static int type_eq(struct type a, struct type b)
{
    return a.k == b.k && a.p == b.p && (arith(a) ? a.q == b.q : 1);
}

static const char *type_name(struct type t)
{
    static char b[4][48];
    static int k;
    char *r = b[k++ & 3];
    switch (t.k) {
    case TY_FIXBIN: snprintf(r, 48, "FIXED BINARY(%d,%d)", t.p, t.q); break;
    case TY_FIXDEC: snprintf(r, 48, "FIXED DECIMAL(%d,%d)", t.p, t.q); break;
    case TY_CHAR:   snprintf(r, 48, "CHARACTER(%d)", t.p); break;
    case TY_VCHAR:  snprintf(r, 48, "CHARACTER(%d) VARYING", t.p); break;
    case TY_BIT:    snprintf(r, 48, "BIT(%d)", t.p); break;
    case TY_FLTDEC: snprintf(r, 48, "FLOAT DECIMAL(%d)", t.p); break;
    case TY_FLTBIN: snprintf(r, 48, "FLOAT BINARY(%d)", t.p); break;
    case TY_LABEL:  snprintf(r, 48, "LABEL"); break;
    case TY_ENTRY:  snprintf(r, 48, "ENTRY"); break;
    case TY_FILE:   snprintf(r, 48, "FILE"); break;
    case TY_PTR:    snprintf(r, 48, "POINTER"); break;
    case TY_AREA:   snprintf(r, 48, "AREA(%d)", t.p); break;
    case TY_OFFSET: snprintf(r, 48, "OFFSET"); break;
    default:        snprintf(r, 48, "an aggregate"); break;
    }
    return r;
}

static int same_shape(struct sym *a, struct sym *b)
{
    if (a->child || b->child)
        return 0;                       /* structures as arguments: not yet */
    if (a->ndims != b->ndims || a->parent || b->parent || !type_eq(a->t, b->t))
        return 0;
    for (int i = 0; i < a->ndims; i++)
        if (a->lb[i] != b->lb[i] || a->hb[i] != b->hb[i])
            return 0;
    return 1;
}

/* A call of an internal procedure: its arguments and, as a function, the
 * type the invoking block sees.  PL/I (F) converts an argument only to an
 * ENTRY descriptor; without one, a variable is passed as it is and a
 * constant or expression as a dummy of its own type -- a mismatch with the
 * parameter then gives wrong values (JOB00088: INC(4) returned 19457), so
 * PL1C/370 refuses it. */
/* Whether argument a may take a GENERIC member's descriptor d with nd
 * dimensions (Q1GEN, Q2GEN, Q3GEN): arithmetic of the same base, scale and
 * precision; a string of the same kind, VARYING or not, of any length;
 * an array of as many dimensions. */
static int gen_match(struct expr *a, struct type d, int nd)
{
    struct type t;
    int dims = 0;
    if (a->k == E_VAR && a->sym && !a->sym->child && a->nsubs < cum_dims(a->sym)) {
        dims = cum_dims(a->sym) - a->nsubs;
        t = a->sym->t;
    } else {
        type_of(a);
        t = a->t;
    }
    if (dims != nd || d.k == TY_NONE)
        return dims == nd && d.k == TY_NONE ? 1 : 0;
    if (t.k == TY_PIC && d.k == TY_PIC)
        return 1;
    if (t.k != d.k)
        return 0;
    if (arith(t))
        return t.p == d.p && t.q == d.q;
    return 1;
}

/* ANSI's test of an argument against a generic description (4.4.5.12
 * to 4.4.5.15): every attribute written must be one the argument has. */
static int gdesc_match(struct expr *a, const struct gdesc *g)
{
    struct type t;
    int dims = 0;
    if (g->star)
        return 1;
    if (a->k == E_VAR && a->sym && !a->sym->child && a->nsubs < cum_dims(a->sym)) {
        dims = cum_dims(a->sym) - a->nsubs;
        t = a->sym->t;
    } else {
        type_of(a);
        t = a->t;
    }
    if (a->k == E_VAR && a->sym && a->sym->child && a->nsubs == 0)
        return 0;                         /* a structure: no STRUCTURE descriptions yet */
    if (g->dims && g->dims != dims)
        return 0;
    if (g->fixed && t.k != TY_FIXBIN && t.k != TY_FIXDEC)
        return 0;
    if (g->flt && !is_float(t))
        return 0;
    if (g->bin && t.k != TY_FIXBIN && t.k != TY_FLTBIN)
        return 0;
    if (g->dec && t.k != TY_FIXDEC && t.k != TY_FLTDEC)
        return 0;
    if (g->chr && t.k != TY_CHAR && t.k != TY_VCHAR)
        return 0;
    if (g->bit && t.k != TY_BIT && t.k != TY_VBIT)
        return 0;
    if (g->var && t.k != TY_VCHAR && t.k != TY_VBIT)
        return 0;
    if (g->nonvar && (t.k == TY_VCHAR || t.k == TY_VBIT))
        return 0;
    if ((g->ptr && t.k != TY_PTR) || (g->pic && t.k != TY_PIC) ||
        (g->label && t.k != TY_LABEL) || (g->entry && t.k != TY_ENTRY) ||
        (g->file && t.k != TY_FILE) || (g->area && t.k != TY_AREA) ||
        (g->offs && t.k != TY_OFFSET))
        return 0;
    if (g->prec) {
        if (!arith(t) || t.p < g->pmin || t.p > g->pmax)
            return 0;
        if (g->scale && (is_float(t) || t.q < g->qmin || t.q > g->qmax))
            return 0;
    }
    return 1;
}

/* A reference to a GENERIC name: the member whose descriptors the
 * arguments match, in number and attributes; none or more than one is
 * an error, as in PL/I (F) (IEM1065I, IEM1066I). */
static void gen_select(struct expr *e)
{
    struct sym *g = e->generic, *pick = NULL;
    int n = 0, ansi = 0;
    for (int i = 0; i < g->ngen; i++)
        ansi |= g->gel && g->gel[i].when;
    if (ansi) {
        /* ANSI: the first member that matches (4.4.5.11); one without
         * WHEN only for a reference without arguments */
        for (int i = 0; i < g->ngen && !pick; i++) {
            struct sym *m = g->gen[i];
            const struct gelem *ge = &g->gel[i];
            int ok;
            if (ge->when == 2)
                ok = !e->nargs && e->bare;
            else if (ge->when) {
                ok = ge->n == e->nargs;
                for (int k = 0; ok && k < e->nargs; k++)
                    ok = gdesc_match(e->arg[k], &ge->d[k]);
            } else {
                ok = m->ndesc == e->nargs;
                for (int k = 0; ok && k < e->nargs; k++)
                    ok = gen_match(e->arg[k], m->desc[k], m->desc_dims[k]);
            }
            if (ok)
                pick = m;
        }
        if (!pick) {
            error_at(e->line, "no GENERIC member of %s takes these arguments", g->name);
            pick = g->gen[0];
        }
        e->callee = pick->gen_callee;
        e->entry = pick;
        e->entryno = pick->gen_entryno;
        e->generic = NULL;
        return;
    }
    for (int i = 0; i < g->ngen; i++) {
        struct sym *m = g->gen[i];
        int ok = m->ndesc == e->nargs;
        for (int k = 0; ok && k < e->nargs; k++)
            ok = gen_match(e->arg[k], m->desc[k], m->desc_dims[k]);
        if (ok) {
            pick = m;
            n++;
        }
    }
    if (n != 1) {
        error_at(e->line, n ? "more than one GENERIC member of %s takes these arguments" :
                 "no GENERIC member of %s takes these arguments", g->name);
        pick = g->gen[0];
    }
    e->callee = pick->gen_callee;
    e->entry = pick;
    e->entryno = pick->gen_entryno;
    e->generic = NULL;
}

static void check_based(struct expr *e);

/* A structure argument to an entry compiled apart (no descriptors, or
 * a structure descriptor 1, 2 ...): passed as it is, by its address
 * (#273).  Returns 0 for anything else. */
static int whole_structure(struct expr *a, struct expr *e, int i)
{
    if (a->k != E_VAR || !a->sym || !a->sym->child)
        return 0;
    for (int k = 0; k < a->nsubs; k++)
        type_of(a->sub[k]);
    check_based(a);
    e->byref[i] = 1;
    memset(&e->argt[i], 0, sizeof e->argt[i]);
    e->argt[i].k = TY_NONE;
    return 1;
}

static void type_fcall(struct expr *e, int as_function)
{
    struct proc *p;
    struct sym *en;

    if (e->generic)
        gen_select(e);
    p = e->callee;
    en = e->entry;

    if (p && p->asm_opt) {
        /* OPTIONS(ASSEMBLER): CALL only; up to as many arguments as
         * descriptors; each passed as the address of its data, a dummy
         * where a descriptor's type differs (SC33-0006-7, chapter 11) */
        if (as_function)
            error_at(e->line, "%s: an OPTIONS(ASSEMBLER) entry is invoked by CALL only",
                     p->name);
        if (!p->nodesc && e->nargs > p->nparams)
            error_at(e->line, "%s has %d descriptors, called with %d arguments", p->name,
                     p->nparams, e->nargs);
        for (int i = 0; i < e->nargs; i++) {
            struct expr *a = e->arg[i];
            if (a->k == E_STAR) {
                /* an omitted argument: a word of zeros (OPTIONAL) */
                if (p->nodesc || i >= p->nparams || !p->params[i]->t.optional)
                    error_at(e->line, "argument %d of %s is omitted, but its descriptor is "
                             "not OPTIONAL", i + 1, p->name);
                if (p->byvalue)
                    error_at(e->line, "%s: OPTIONAL and BYVALUE together", p->name);
                e->byref[i] = 6;              /* omitted */
                continue;
            }
            type_of(a);
            if (p->byvalue) {
                /* the value itself in the list: a pointer or FIXED BIN(31) */
                struct type d = !p->nodesc && i < p->nparams ? p->params[i]->t : a->t;
                if (d.k != TY_PTR && !(d.k == TY_FIXBIN && d.p == 31 && d.q == 0))
                    error_at(e->line, "argument %d of %s: BYVALUE passes POINTER or FIXED "
                             "BINARY(31) only", i + 1, p->name);
                if (d.k == TY_PTR && a->t.k != TY_PTR)
                    error_at(e->line, "argument %d of %s must be a pointer", i + 1, p->name);
                e->byref[i] = 5;
                e->argt[i] = d;
                continue;
            }
            e->byref[i] = a->k == E_VAR ? 1 : 0;
            e->argt[i] = a->t;
            if (!p->nodesc && i < p->nparams) {
                struct sym *d = p->params[i];
                if (d->t.k != TY_NONE && !d->star && !d->ndims &&
                    (arith(d->t) || is_string(d->t) || d->t.k == TY_BIT) &&
                    !(a->k == E_VAR && !a->sym->ndims && !a->sym->child &&
                      type_eq(a->t, d->t))) {
                    e->byref[i] = 0;          /* a dummy of the descriptor's type */
                    e->argt[i] = d->t;
                }
            }
            if (e->byref[i] && a->sym->packed)
                error_at(e->line, "an unaligned bit string at a bit offset to an assembler "
                         "entry is not supported yet");
        }
        return;
    }

    if (!p) {
        /* through an ENTRY parameter or variable: the arguments as they
         * are, or as its descriptors say (a dummy where they differ) */
        if (en->has_desc && en->ndesc != e->nargs)
            error_at(e->line, "%s has %d descriptors, called with %d arguments", en->name,
                     en->ndesc, e->nargs);
        for (int i = 0; i < e->nargs; i++) {
            struct expr *a = e->arg[i];
            struct type d;
            if (whole_structure(a, e, i))
                continue;
            d = en->has_desc && i < en->ndesc ? en->desc[i] : a->t;
            type_of(a);
            /* packed bits: by reference through a copy (gen.c) */
            e->byref[i] = a->k == E_VAR ? (a->sym->packed ? 4 : 1) : 0;
            e->argt[i] = a->t;
            if (en->has_desc && i < en->ndesc && d.k != TY_NONE && !en->desc_dims[i] &&
                (arith(d) || is_string(d) || d.k == TY_BIT) && d.p > 0 &&
                !(a->k == E_VAR && !a->sym->ndims && !a->sym->child && type_eq(a->t, d))) {
                if (a->k == E_VAR && (a->sym->ndims || a->sym->child) && a->nsubs == 0)
                    error_at(e->line, "argument %d of %s: an aggregate for a scalar "
                             "descriptor", i + 1, en->name);
                e->byref[i] = 0;              /* a dummy of the descriptor's type */
                e->argt[i] = d;
            }
        }
        if (as_function) {
            if (!en->has_ret)
                error_at(e->line, "%s is used as a function but has no RETURNS", en->name);
            e->t = en->has_ret ? en->ret : e->t;
        }
        return;
    }

    if (p->nodesc) {
        /* compiled apart, declared ENTRY without descriptors: the
         * arguments as they are, as through an ENTRY parameter */
        for (int i = 0; i < e->nargs; i++) {
            struct expr *a = e->arg[i];
            if (whole_structure(a, e, i))
                continue;
            type_of(a);
            e->byref[i] = a->k == E_VAR ? (a->sym->packed ? 4 : 1) : 0;
            e->argt[i] = a->t;
        }
        if (as_function) {
            if (!p->has_ret)
                error_at(e->line, "%s is used as a function but has no RETURNS", p->name);
            e->t = p->ret;
        }
        return;
    }
    {
    /* the parameters of the entry point called */
    int np = p->nentries ? p->nprim : p->nparams, has_ret = p->has_ret;
    struct sym *pars[MAXCALLARGS];
    struct type ret = p->ret;
    if (e->entryno) {
        struct pentry *pe = &p->entries[e->entryno - 1];
        np = pe->nparams;
        for (int j = 0; j < np; j++)
            pars[j] = p->params[pe->canon[j]];
        if (pe->has_ret) {
            has_ret = 1;
            ret = pe->ret;
        }
    } else {
        for (int j = 0; j < np; j++)
            pars[j] = p->params[j];
    }
    if (e->nargs != np)
        error_at(e->line, "%s takes %d arguments, %d given", p->name, np, e->nargs);
    if (en && en->has_desc && en->ndesc != np)
        error_at(e->line, "the ENTRY declaration of %s has %d descriptors, the procedure %d "
                 "parameters", p->name, en->ndesc, np);
    for (int i = 0; i < e->nargs && i < np; i++) {
        struct expr *a = e->arg[i];
        struct sym *par = pars[i];
        int desc = en && en->has_desc && i < en->ndesc;

        if (a->k == E_FCALL && a->bare && a->callee &&
            (par->t.k == TY_ENTRY || (desc && en->desc[i].k == TY_ENTRY))) {
            /* a procedure's name for an ENTRY parameter: the procedure
             * itself, not its value (PL/I (F): IEM0786I, JOB00345) */
            if (a->entryno)
                error_at(e->line, "a secondary entry point as an argument is not supported yet");
            a->k = E_ENTRY;
            a->nargs = 0;
        }

        if (par->t.k == TY_NONE && !par->child && !par->ndims && whole_structure(a, e, i))
            continue;                 /* a structure descriptor: as it is (#273) */
        if (par->ndims && (par->stardims || par->star)) {
            /* A(*), or elements of CHAR(*): a descriptor with the
             * array's address, its element length and its bounds (gen.c) */
            struct sym *as = a->k == E_VAR ? a->sym : NULL;
            int ok = as && !as->child && !as->parent && a->nsubs == 0 &&
                     as->ndims == par->ndims;
            if (par->star && par->t.k == TY_BIT)
                error_at(e->line, "argument %d of %s: arrays of BIT(*) are not supported yet",
                         i + 1, p->name);
            if (ok && par->star)
                ok = as->t.k == par->t.k && as->t.p <= par->t.p;
            else if (ok)
                ok = type_eq(as->t, par->t) && !as->star;
            if (ok && !par->stardims) {
                for (int d = 0; d < par->ndims; d++)
                    if (!as->stardims && (as->lb[d] != par->lb[d] || as->hb[d] != par->hb[d]))
                        ok = 0;
                if (as->stardims)
                    ok = 0;
            }
            if (!ok)
                error_at(e->line, "argument %d of %s: an array of the parameter's type and "
                         "dimensions, as a whole", i + 1, p->name);
            e->byref[i] = 3;
            e->argt[i] = as ? as->t : par->t;
            continue;
        }
        if (a->k == E_VAR && (a->sym->child || a->nsubs < cum_dims(a->sym))) {
            /* an array passed whole: by reference, the same shape */
            if (!same_shape(a->sym, par) || a->nsubs != 0)
                error_at(e->line, "argument %d of %s: an array argument must match the "
                         "parameter's bounds and type", i + 1, p->name);
            e->byref[i] = 1;
            continue;
        }
        type_of(a);
        if (par->star && par->t.k != TY_AREA) {
            /* CHAR(*): the argument itself, its address and length in a
             * descriptor (gen.c); par->t.p is the bound star_pass found */
            if (par->t.k == TY_BIT ? !is_bit(a->t) :
                par->t.k == TY_VCHAR ? !(a->k == E_VAR && a->t.k == TY_VCHAR)
                                     : !(a->t.k == TY_CHAR || a->t.k == TY_VCHAR))
                error_at(e->line, "argument %d of %s: a %s%s value for %s(*)%s", i + 1,
                         p->name, par->t.k == TY_BIT ? "BIT" : "CHARACTER",
                         par->t.k == TY_VCHAR ? " VARYING variable" : "",
                         par->t.k == TY_BIT ? "BIT" : "CHAR",
                         par->t.k == TY_VCHAR ? " VARYING" : "");
            else if (a->t.p > par->t.p)
                error_at(e->line, "argument %d of %s: more than %d %s for %s(*) "
                         "(an expression's bound) is not supported yet", i + 1, p->name,
                         par->t.p, par->t.k == TY_BIT ? "bits" : "characters",
                         par->t.k == TY_BIT ? "BIT" : "CHAR");
            e->byref[i] = 2;
            e->argt[i] = a->t;
            continue;
        }
        if (par->t.k == TY_AREA && !par->ndims) {
            /* an area: by reference, whatever the parameter's size -- PL/I
             * (F) makes no dummy, the area keeps its own size (Q1ADUM) */
            if (a->k != E_VAR || a->t.k != TY_AREA)
                error_at(e->line, "argument %d of %s: an area variable for an area parameter",
                         i + 1, p->name);
            e->byref[i] = 1;
            e->argt[i] = a->t;
            continue;
        }
        if (par->ndims || par->child)
            error_at(e->line, "argument %d of %s: the parameter is an aggregate", i + 1, p->name);
        if (desc && en->desc[i].k == TY_NONE) {
            /* an empty descriptor: the argument as it is */
            type_of(a);
            e->byref[i] = a->k == E_VAR ? (a->sym->packed ? 4 : 1) : 0;
            e->argt[i] = a->t;
            continue;
        }
        if (desc) {
            if (!type_eq(en->desc[i], par->t))
                error_at(e->line, "descriptor %d of %s is %s, the parameter %s", i + 1, p->name,
                         type_name(en->desc[i]), type_name(par->t));
            e->byref[i] = a->k == E_VAR && type_eq(a->t, en->desc[i]) ? (a->sym->packed ? 4 : 1)
                                                                       : 0;
            e->argt[i] = en->desc[i];
            if (!e->byref[i] && arith(par->t) && is_string(a->t)) {
                e->arg[i] = conv(a, is_float(par->t) ? 0 : dec_scale(par->t));
                if (is_float(par->t))
                    e->arg[i]->t = par->t;       /* a string to FLOAT, as assigned */
            }
            a = e->arg[i];
            if (!e->byref[i] && !type_eq(a->t, par->t) && !((arith(a->t) && arith(par->t)) ||
                                  (is_string(par->t) && (is_string(a->t) || arith(a->t))) ||
                                  (is_bit(par->t) && is_bit(a->t))))
                error_at(e->line, "argument %d of %s cannot be converted to %s", i + 1,
                         p->name, type_name(par->t));
        } else {
            type_of(a);
            if (!type_eq(a->t, par->t) && is_float(a->t) && is_float(par->t) &&
                float_long(a->t) == float_long(par->t)) {
                /* passed as it is, as PL/I (F) does: FLOAT of the same
                 * length has the same form (Q3UND: 2.5E0 to FLOAT DEC(6)) */
                e->byref[i] = a->k == E_VAR;
                e->argt[i] = par->t;
                continue;
            }
            if (!type_eq(a->t, par->t) && p->depth > 0 && !opt_lang_f) {
                /* an internal procedure: a dummy of the parameter's
                 * attributes (SC26-3114-01, "Dummy Arguments": "for internal
                 * entry constants, the attributes are derived from the
                 * parameter declarations"); PL/I (F) passes it unconverted */
                e->byref[i] = 0;
                e->argt[i] = par->t;
                if (arith(par->t) && is_string(a->t)) {
                    e->arg[i] = conv(a, is_float(par->t) ? 0 : dec_scale(par->t));
                    if (is_float(par->t))
                        e->arg[i]->t = par->t;
                }
                a = e->arg[i];
                if (!type_eq(a->t, par->t) && !((arith(a->t) && arith(par->t)) ||
                                      (is_string(par->t) && (is_string(a->t) || arith(a->t))) ||
                                      (is_bit(par->t) && is_bit(a->t))))
                    error_at(e->line, "argument %d of %s cannot be converted to %s", i + 1,
                             p->name, type_name(par->t));
                continue;
            }
            if (!type_eq(a->t, par->t) && p->depth > 0)
                error_at(e->line, "argument %d of %s is %s, the parameter %s: PL/I (F) "
                         "passes it unconverted (the default language converts it)", i + 1,
                         p->name, type_name(a->t), type_name(par->t));
            else if (!type_eq(a->t, par->t))
                error_at(e->line, "argument %d of %s is %s, the parameter %s: declare %s "
                         "ENTRY(...) to have it converted", i + 1, p->name, type_name(a->t),
                         type_name(par->t), p->name);
            /* packed bits: by reference as PL/I (F) passes them -- here
             * through a copy, written back after the call (gen.c) */
            e->byref[i] = a->k == E_VAR ? (a->sym->packed ? 4 : 1) : 0;
            e->argt[i] = a->t;
        }
    }
    /* an element variable in parentheses, (X): a dummy, not X itself
     * (SC26-3114-01, "Dummy Arguments": "An expression with operators,
     * parentheses, or function references") */
    for (int i = 0; i < e->nargs; i++)
        if (e->arg[i]->paren && (e->byref[i] == 1 || e->byref[i] == 4) &&
            !(e->arg[i]->sym->ndims && e->arg[i]->nsubs == 0) && !e->arg[i]->sym->child)
            e->byref[i] = 0;
    if (!as_function)
        return;
    if (!has_ret) {
        error_at(e->line, "%s is used as a function but has no RETURNS", p->name);
        e->t.k = TY_FIXBIN;
        e->t.p = 31;
        return;
    }
    /* the type the invoking block sees */
    if (en && en->has_ret) {
        e->t = en->ret;
    } else if (p->depth > 0 && !opt_lang_f) {
        /* an internal entry constant: the RETURNS of its PROCEDURE
         * (SC26-3114-01, RETURNS attribute and "Procedures"); PL/I (F)
         * takes the invoking block's default (Q1BLT) */
        e->t = ret;
        return;
    } else if (p->name[0] >= 'I' && p->name[0] <= 'N') {
        e->t.k = TY_FIXBIN;
        e->t.p = 15;
        e->t.q = 0;
    } else if (ret.k == TY_FLTDEC && ret.p == 6) {
        /* the default the invoking block takes, FLOAT DECIMAL(6), is the
         * function's (Q1BLT: XSQ) */
        e->t = ret;
    } else {
        error_at(e->line, "%s: declare %s RETURNS(...) in the invoking block (its default is "
                 "FLOAT DECIMAL)", p->name, p->name);
        e->t = ret;
        return;
    }
    if (!type_eq(e->t, ret))
        error_at(e->line, "%s returns %s, the invoking block expects %s: declare %s "
                 "RETURNS(...)", p->name, type_name(ret), type_name(e->t), p->name);
    }
}

static void type_of(struct expr *e);

/* The pointer of a BASED reference: P of P -> X, else the one of BASED(P). */
static void check_based(struct expr *e)
{
    struct sym *root = e->sym->root;
    struct expr *p = e->qual ? e->qual : root->based;
    if (e->qual && root->st != ST_BASED) {
        error_at(e->line, "%s is not BASED: it takes no pointer qualifier", e->sym->name);
        return;
    }
    if (!p) {
        if (root->st == ST_BASED)
            error_at(e->line, "%s is BASED without a pointer: a reference needs P -> %s",
                     e->sym->name, e->sym->name);
        return;
    }
    type_of(p);
    if (p->t.k != TY_PTR)
        error_at(e->line, "the qualifier of %s must be a pointer", e->sym->name);
    else if (p->k == E_VAR && p->sym->root->st == ST_BASED && !p->qual &&
             p->sym->root->based == p)
        error_at(e->line, "%s is based on itself", e->sym->name);
}

/* An integer constant argument (a precision, a count). */
static int int_arg(struct expr *e, int *v)
{
    int neg = 0, n = 0;
    if (e->k == E_NEG) {
        neg = 1;
        e = e->l;
    }
    if (e->k != E_NUM || e->t.k != TY_FIXDEC || e->t.q != 0)
        return 0;
    for (const char *d = e->num; *d; d++)
        n = n * 10 + (*d - '0');
    *v = neg ? -n : n;
    return 1;
}

static struct expr *new_node(enum ekind k, int line)
{
    struct expr *c = xmalloc(sizeof *c);
    memset(c, 0, sizeof *c);
    c->k = k;
    c->line = line;
    return c;
}

/* e becomes l converted to t, as an assignment would (E_CAST). */
static void make_cast(struct expr *e, struct expr *l, struct type t)
{
    e->k = E_CAST;
    e->l = l;
    e->nargs = 0;
    e->t = t;
}

static struct expr *cast_of(struct expr *l, struct type t)
{
    struct expr *c = new_node(E_CAST, l->line);
    c->l = l;
    c->t = t;
    return c;
}

static int locator(struct type t)
{
    return t.k == TY_PTR || t.k == TY_OFFSET;
}

/* An offset as a pointer: its area's address added. */
/* A built-in call made for LANGLVL(SPROG)'s pointer operations */
static struct expr *sprog_call(enum bfn fn, struct expr *a, struct expr *b)
{
    struct expr *c = xmalloc(sizeof *c);
    c->k = E_CALL;
    c->fn = fn;
    c->line = a->line;
    c->arg[0] = a;
    c->nargs = 1;
    if (b) {
        c->arg[1] = b;
        c->nargs = 2;
    }
    return c;
}

static struct expr *offset_ptr(struct expr *e)
{
    struct type t;
    memset(&t, 0, sizeof t);
    t.k = TY_PTR;
    t.p = 4;
    if (!e->t.area)
        error_at(e->line, "NULLO is not converted to a pointer");
    return cast_of(e, t);
}

/* The target type of FIXED, FLOAT, BINARY, DECIMAL, PRECISION, ADD,
 * MULTIPLY and DIVIDE: base and scale from the function or the source,
 * the precision given or derived.  A FIXED result with a precision needs
 * the scale as well: PL/I (F) refuses (p) alone (IEM1061I, JOB00183). */
static int conv_type(struct expr *e, struct type src, int fn, int pi, struct type *t)
{
    int p = -1, q = 0, hasq = 0;
    int flt = is_float(src), bin = src.k == TY_FIXBIN || src.k == TY_FLTBIN;
    if (fn == B_FIXED) flt = 0;
    if (fn == B_FLOAT) flt = 1;
    if (fn == B_BINARY) bin = 1;
    if (fn == B_DECIMAL) bin = 0;
    if (pi < e->nargs && !int_arg(e->arg[pi], &p)) {
        error_at(e->line, "the precision must be an integer constant");
        return 0;
    }
    if (pi + 1 < e->nargs) {
        if (!int_arg(e->arg[pi + 1], &q)) {
            error_at(e->line, "the scale factor must be an integer constant");
            return 0;
        }
        hasq = 1;
    }
    memset(t, 0, sizeof *t);
    if (p < 0 && fn == B_FLOAT)
        p = bin ? 21 : 6;                 /* FLOAT(x): the default precision */
    if (p < 0 && fn == B_FIXED) {
        p = bin ? 15 : 5;                 /* FIXED(x): FIXED DEC(5,0) or BIN(15,0) */
        q = 0;
        hasq = 1;                         /* (JOB00186) */
    }
    if (flt) {
        if (hasq) {
            error_at(e->line, "a FLOAT result takes no scale factor");
            return 0;
        }
        if (p < 0) {
            /* the precision converted to the base: ceil(p*3.32) or ceil(p/3.32) */
            struct type f = src;
            if (!is_float(f)) {
                f.k = f.k == TY_FIXBIN ? TY_FLTBIN : TY_FLTDEC;
                f.q = 0;
            }
            if (bin && f.k == TY_FLTDEC)
                p = min(53, ceil_mul332(f.p));
            else if (!bin && f.k == TY_FLTBIN)
                p = ceil_div332(f.p);
            else
                p = f.p;
        }
        t->k = bin ? TY_FLTBIN : TY_FLTDEC;
        t->p = p;
        if (p < 1 || p > (bin ? 53 : 16)) {
            error_at(e->line, "FLOAT precision out of range");
            return 0;
        }
        return 1;
    }
    if (p >= 0 && !hasq && fn != B_FIXED) {
        /* FIXED(x,p) is FIXED(x,p,0); the others need q (JOB00183/184) */
        error_at(e->line, "give the scale factor too: a FIXED result needs (p,q)");
        return 0;
    }
    if (p < 0) {
        if (is_float(src)) {
            p = bin ? (src.k == TY_FLTBIN ? src.p : min(31, ceil_mul332(src.p)))
                    : (src.k == TY_FLTDEC ? src.p : ceil_div332(src.p));
            p = min(bin ? 31 : 15, p);
            q = 0;
        } else if (bin) {
            p = bin_prec(src);
            q = bin_scale(src);
        } else {
            p = dec_prec(src);
            q = dec_scale(src);
        }
    }
    t->k = bin ? TY_FIXBIN : TY_FIXDEC;
    t->p = p;
    t->q = q;
    if (p < 1 || p > (bin ? 31 : 15) || q < 0 || q > p) {
        error_at(e->line, "FIXED precision out of range");
        return 0;
    }
    return 1;
}

static struct expr *new_call1(int fn, struct expr *a);

/* A character string as BIT(x) would make it */
static struct expr *conv_bit(struct expr *a)
{
    struct expr *e = new_call1(B_BIT, a);
    type_of(e);
    return e;
}

static struct expr *new_call1(int fn, struct expr *a)
{
    struct expr *e = xmalloc(sizeof *e);
    memset(e, 0, sizeof *e);
    e->k = E_CALL;
    e->fn = fn;
    e->nargs = 1;
    e->arg[0] = a;
    e->line = a->line;
    return e;
}

/* ANSI's functions that PL/I (F)'s express (ANSI X3.53, 9.4.4), on the
 * typed arguments of e; 1 if e is typed now:
 *   ASIN(x)  as ATAN(y, SQRT(1 - y*y)), ACOS(x) as ATAN(SQRT(1 - y*y), y),
 *            y being x as FLOAT;
 *   SUBTRACT(x, y, p, q) as ADD(x, -y, p, q);
 *   OFFSET(p, a), POINTER(o, a) as the conversions by the area a. */
static int ansi_rewrite(struct expr *e)
{
    if (e->fn == B_ASIN || e->fn == B_ACOS) {
        struct expr *y = e->arg[0], *one = new_node(E_NUM, e->line), *sq, *d;
        if (!arith(y->t)) {
            error_at(e->line, "%s needs an arithmetic argument", e->fn == B_ASIN ? "ASIN" :
                     "ACOS");
            return 0;
        }
        if (!is_float(y->t)) {
            y = new_call1(B_FLOAT, y);
            type_of(y);
        }
        one->num = "1";
        one->t.k = TY_FIXDEC;
        one->t.p = 1;
        d = new_node(E_BIN, e->line);
        d->op = T_STAR;
        d->l = y;
        d->r = y;
        sq = new_node(E_BIN, e->line);
        sq->op = T_MINUS;
        sq->l = one;
        sq->r = d;
        sq = new_call1(B_SQRT, sq);
        type_of(sq);
        e->arg[0] = e->fn == B_ASIN ? y : sq;
        e->arg[1] = e->fn == B_ASIN ? sq : y;
        e->nargs = 2;
        e->fn = B_ATAN;
        return 0;
    }
    if (e->fn == B_SUBTRACT) {
        struct expr *n = new_node(E_NEG, e->line);
        n->l = e->arg[1];
        type_of(n);
        e->arg[1] = n;
        e->fn = B_ADD;
        return 0;
    }
    if (e->fn == B_OFFSETF || e->fn == B_POINTERF) {
        struct expr *x = e->arg[0], *a = e->arg[1];
        struct type t;
        if (a->k != E_VAR || a->t.k != TY_AREA) {
            error_at(e->line, "%s needs an area variable", e->fn == B_OFFSETF ? "OFFSET" :
                     "POINTER");
            return 1;
        }
        memset(&t, 0, sizeof t);
        t.k = TY_OFFSET;
        t.p = 4;
        t.area = a;
        if (e->fn == B_OFFSETF) {
            if (x->t.k != TY_PTR)
                error_at(e->line, "OFFSET needs a pointer");
            make_cast(e, x, t);                 /* the pointer by the area */
        } else {
            if (x->t.k != TY_OFFSET)
                error_at(e->line, "POINTER needs an offset");
            x = cast_of(x, t);                  /* the offset in the area a */
            t.k = TY_PTR;
            t.area = NULL;
            make_cast(e, x, t);
        }
        return 1;
    }
    return 0;
}

/* The leaves of x for STRING: kind 1 all CHARACTER (or numeric picture),
 * 2 all BIT, 3 mixed or other; *n the characters or bits, times the
 * elements of the arrays (mult). */
static void string_leaves(struct sym *x, int *kind, int *n, int mult)
{
    for (int i = 0; i < x->ndims; i++)
        mult *= x->hb[i] - x->lb[i] + 1;
    if (x->child) {
        for (struct sym *c = x->child; c; c = c->sibling)
            string_leaves(c, kind, n, mult);
        return;
    }
    {
        int k = x->t.k == TY_CHAR || x->t.k == TY_PIC ? 1 : x->t.k == TY_BIT ? 2 : 3;
        *kind = *kind == 0 || *kind == k ? k : 3;
        *n += mult * (x->t.k == TY_PIC ? pic_length(x->t) : x->t.p);
    }
}

/* CALL PLISRTA to PLISRTD (SC33-0006-7, chapter 12): the SORT and
 * RECORD statements, the storage, the return code variable, the exits
 * (E15 a function returning a character string, E35 a procedure of one
 * character parameter), then the prefix, the message level and the
 * technique, strings */
static void type_sort(struct expr *e)
{
    int nx = e->fn == B_PLISRTA ? 0 : e->fn == B_PLISRTD ? 2 : 1;
    struct expr *rc = e->arg[3];
    for (int i = 0; i < e->nargs; i++) {
        struct expr *a = e->arg[i];
        if (i >= 4 && i < 4 + nx) {
            int e35 = e->fn == B_PLISRTC || (e->fn == B_PLISRTD && i == 5);
            if (a->k == E_FCALL && a->bare && a->callee) {
                struct proc *p = a->callee;
                if (a->entryno || p->is_stub)
                    error_at(e->line, "the %s exit must be a procedure of this compilation, "
                             "by its first entry point, or an ENTRY variable",
                             e35 ? "E35" : "E15");
                else if (!e35 && (!p->has_ret || (p->ret.k != TY_CHAR && p->ret.k != TY_VCHAR)))
                    error_at(e->line, "the E15 exit %s must return a character string",
                             p->name);
                else if (e35 && (p->nparams != 1 || p->params[0]->ndims ||
                                 p->params[0]->t.k != TY_CHAR))
                    error_at(e->line, "the E35 exit %s must have one character parameter",
                             p->name);
                a->k = E_ENTRY;
                a->nargs = 0;
            } else {
                /* the run time must know E15's result and how E35 takes
                 * its parameter: from the procedure */
                error_at(e->line, "the sort's exits must be procedures of this compilation: "
                         "an ENTRY variable is not supported yet");
            }
            type_of(a);
            if (a->t.k != TY_ENTRY)
                error_at(e->line, "argument %d of the sort must be an entry", i + 1);
            continue;
        }
        type_of(a);
        if (i == 2 ? !arith(a->t) || is_float(a->t) : i != 3 && !is_string(a->t))
            error_at(e->line, "argument %d of the sort must be %s", i + 1,
                     i == 2 ? "FIXED BINARY" : "a character string");
    }
    if (rc->k != E_VAR || rc->t.k != TY_FIXBIN || rc->t.p != 31 || rc->t.q != 0)
        error_at(e->line, "the sort's return code must be a FIXED BINARY(31,0) variable");
    e->t.k = TY_FIXBIN;               /* no value: a subroutine */
    e->t.p = 31;
}

static void type_call(struct expr *e)
{
    if (e->fn == B_CXOP || e->fn == B_CXABS || e->fn == B_CXFN || e->fn == B_CXCVT)
        return;                           /* typed when made (cx_call, cx_abs, cx_fn) */
    if ((e->fn == B_LOG10 || e->fn == B_LOG2 || e->fn == B_ATAND || e->fn == B_SIND ||
         e->fn == B_COSD || e->fn == B_TAND || e->fn == B_ERF || e->fn == B_ERFC ||
         e->fn == B_ATAN) && e->nargs >= 1 &&
        (cx_expr(e->arg[0]) || (e->nargs > 1 && cx_expr(e->arg[1])))) {
        /* PL/I (F): IEM1056I, IEM1062I for ATAN(y,x) (JOB00148) */
        error_at(e->line, "this function does not take a complex argument");
        e->t.k = TY_FLTDEC;
        e->t.p = 6;
        return;
    }
    if (e->fn == B_ABS && e->nargs == 1 && cx_expr(e->arg[0])) {
        cx_abs(e);
        return;
    }
    if ((e->fn == B_ADD || e->fn == B_MULTIPLY || e->fn == B_DIVIDE) && e->nargs >= 2 &&
        (cx_expr(e->arg[0]) || cx_expr(e->arg[1]))) {
        error_at(e->line, "a complex value is not supported here yet");
        e->t.k = TY_FIXDEC;
        e->t.p = 1;
        return;
    }
    if (e->fn == B_REAL || e->fn == B_IMAG || e->fn == B_CONJG || e->fn == B_COMPLEX) {
        type_cplxfn(e);
        return;
    }
    if (e->fn == B_CPXSTR) {
        type_cpxstr(e);
        return;
    }
    if (e->fn >= B_PLISRTA && e->fn <= B_PLISRTD) {
        type_sort(e);
        return;
    }
    if ((e->fn == B_DIM || e->fn == B_LBOUND || e->fn == B_HBOUND) &&
        e->arg[0]->k == E_VAR && e->arg[0]->sym->stardims) {
        /* of an A(*) parameter: at run time; the array is not a value */
        struct expr *a = e->arg[0];
        if (a->nsubs || !(e->arg[1]->k == E_NUM && e->arg[1]->t.q == 0) ||
            atoi(e->arg[1]->num) < 1 || atoi(e->arg[1]->num) > a->sym->ndims)
            error_at(e->line, "%s: the array and a constant from 1 to %d",
                     e->fn == B_DIM ? "DIM" : e->fn == B_LBOUND ? "LBOUND" : "HBOUND",
                     a->sym->ndims);
        memset(&e->t, 0, sizeof e->t);
        e->t.k = TY_FIXBIN;
        e->t.p = 15;
        return;
    }
    if (e->fn == B_STG || e->fn == B_CSTG) {
        /* STORAGE(x), CURRENTSTORAGE(x): the bytes of a variable, an
         * element, array or structure (GC33-0009-4, SC26-3114-01) */
        struct expr *a = e->arg[0];
        memset(&e->t, 0, sizeof e->t);
        e->t.k = TY_FIXBIN;
        e->t.p = 31;
        if (a->k != E_VAR) {
            error_at(e->line, "%s needs a variable", e->fn == B_STG ? "STORAGE" :
                     "CURRENTSTORAGE");
            return;
        }
        for (int i = 0; i < a->nsubs; i++)
            type_of(a->sub[i]);
        check_based(a);
        if (a->sym->packed || a->sym->adjust || a->sym->refer || a->sym->star ||
            a->sym->stardims || (a->sym->root->child && a->sym->root->tbits) ||
            (a->nsubs && a->nsubs != cum_dims(a->sym)) ||
            (!a->nsubs && cum_dims(a->sym) != a->sym->ndims))
            error_at(e->line, "%s of %s is not supported yet", e->fn == B_STG ? "STORAGE" :
                     "CURRENTSTORAGE", a->sym->name);
        return;
    }
    if (e->fn == B_ADDR) {
        /* ADDR(x): any variable, an array or structure too */
        struct expr *a = e->arg[0];
        if (a->k != E_VAR) {
            error_at(e->line, "ADDR needs a variable");
        } else if (a->sym->packed) {
            error_at(e->line, "ADDR of an unaligned bit string at a bit offset");
        } else {
            for (int i = 0; i < a->nsubs; i++)
                type_of(a->sub[i]);
            check_based(a);
        }
        e->t.k = TY_PTR;
        e->t.p = 4;
        return;
    }
    if (e->fn == B_STRING) {
        /* STRING(x): the elements of x, a variable, in one string: the
         * characters of character data, or packed bits (Q1STR) */
        struct expr *a = e->arg[0];
        int n = 0, kind = 0;
        memset(&e->t, 0, sizeof e->t);
        if (a->k != E_VAR) {
            type_of(a);
            e->t = a->t;                    /* an expression: itself */
            if (!is_string(a->t) && !is_bit(a->t))
                error_at(e->line, "STRING needs a string or a variable of strings");
            return;
        }
        for (int i = 0; i < a->nsubs; i++)
            type_of(a->sub[i]);
        check_based(a);
        if (a->nsubs && a->nsubs == cum_dims(a->sym) && !a->sym->child) {
            type_of(a);                     /* an element */
            e->t = a->t;
            return;
        }
        if (a->nsubs || (a->sym->parent && cum_dims(a->sym->parent)))
            error_at(e->line, "STRING of a cross section or of a member of an array of "
                     "structures is not supported yet");
        string_leaves(a->sym, &kind, &n, 1);
        e->t.k = kind == 2 ? TY_BIT : TY_CHAR;
        e->t.p = n;
        if (kind == 3 || kind == 0)
            error_at(e->line, "STRING needs character strings or bit strings only");
        else if (kind == 2 && a->sym->tbits != n)
            error_at(e->line, "STRING of bit strings that are not packed together is not "
                     "supported yet");
        return;
    }
    if (e->fn == B_ENTRYADDR) {
        /* ENTRYADDR(x): the address of the entry x, an external entry
         * constant or an ENTRY variable (SC26-3114-01, chapter 16) */
        struct expr *a = e->arg[0];
        if (a->k == E_FCALL && a->bare && a->callee) {
            if (a->callee->parent || a->entryno)
                error_at(e->line, "ENTRYADDR takes an external procedure, by its first entry "
                         "point");
            a->k = E_ENTRY;
            a->nargs = 0;
        } else {
            type_of(a);
            if (a->t.k != TY_ENTRY || a->k != E_VAR)
                error_at(e->line, "ENTRYADDR takes an entry");
        }
        memset(&e->t, 0, sizeof e->t);
        e->t.k = TY_PTR;
        e->t.p = 4;
        return;
    }
    if (e->fn == B_ALLOCATION) {
        /* ALLOCATION(x): '1'B while x, CONTROLLED, has a generation */
        struct expr *a = e->arg[0];
        if (a->k != E_VAR || a->nsubs || a->sym->root != a->sym || a->sym->st != ST_CTL)
            error_at(e->line, "ALLOCATION needs a level-1 CONTROLLED variable");
        memset(&e->t, 0, sizeof e->t);
        e->t.k = TY_BIT;
        e->t.p = 1;
        return;
    }
    for (int i = 0; i < e->nargs; i++)
        type_of(e->arg[i]);
    if (ansi_rewrite(e))
        return;
    switch (e->fn) {
    case B_SUM: case B_PROD: case B_POLY: case B_ALL: case B_ANY:
    case B_DOT: case B_EVERY: case B_SOME:
        error_at(e->line, "an array function is not supported here yet");
        return;
    case B_ALLOCATION:
        return;
    case B_PICVAL: {
        /* the arithmetic value of a numeric picture: FIXED DECIMAL(p,q) */
        struct type pt = e->arg[0]->sym->t;
        memset(&e->t, 0, sizeof e->t);
        e->t.k = TY_FIXDEC;
        e->t.p = pt.p;
        e->t.q = pt.q;
        return;
    }
    case B_VERIFY:
        if (!is_string(e->arg[0]->t) || !is_string(e->arg[1]->t))
            error_at(e->line, "VERIFY needs strings");
        e->t.k = TY_FIXBIN;
        e->t.p = 15;
        return;
    case B_TRANSLATE:
        for (int i = 0; i < e->nargs; i++)
            if (!is_string(e->arg[i]->t))
                error_at(e->line, "TRANSLATE needs strings");
        e->t.k = TY_CHAR;
        e->t.p = string_len(e->arg[0]->t);
        return;
    case B_REPEAT: {
        int n;
        if (!is_string(e->arg[0]->t))
            error_at(e->line, "REPEAT needs a string");
        if (!int_arg(e->arg[1], &n) || n < 0) {
            error_at(e->line, "REPEAT needs a count that is a constant, 0 or more");
            n = 0;
        }
        e->t.k = TY_CHAR;
        e->t.p = string_len(e->arg[0]->t) * (n + 1);   /* REPEAT('AB',2) is ABABAB */
        if (e->t.p > 32767)
            error_at(e->line, "REPEAT result too long");
        return;
    }
    case B_HIGH:
    case B_LOW: {
        int n;
        if (!int_arg(e->arg[0], &n) || n < 1 || n > 32767) {
            error_at(e->line, "HIGH and LOW need a length that is a constant");
            n = 1;
        }
        e->t.k = TY_CHAR;
        e->t.p = n;
        return;
    }
    case B_UNSPEC: {
        struct type a = e->arg[0]->t;
        int n = a.k == TY_CHAR ? a.p : a.k == TY_FIXBIN ? (a.p <= 15 ? 2 : 4) :
                a.k == TY_FIXDEC ? dec_bytes(a.p) : is_float(a) ? (flt_long(a) ? 8 : 4) :
                a.k == TY_PTR || a.k == TY_OFFSET ? 4 : a.k == TY_BIT ? (a.p + 7) / 8 :
                a.k == TY_AREA ? a.p + 16 : is_cplx(a) ? 2 * (part_type(a).k == TY_FIXBIN ?
                (a.p <= 15 ? 2 : 4) : part_type(a).k == TY_FIXDEC ? dec_bytes(a.p) :
                flt_long(part_type(a)) ? 8 : 4) : 0;
        if (n < 1 || 8 * n > MAXBITS)
            error_at(e->line, "UNSPEC of more than %d bits is not supported", MAXBITS);
        /* an expression other than a variable or a string: its value
         * in a temporary of its own type (gen.c, unspec_tmp) */
        if (e->arg[0]->k == E_VAR && e->arg[0]->sym->child && n > 4 && a.k != TY_CHAR)
            error_at(e->line, "UNSPEC of more than 32 bits takes a variable");
        if (a.k == TY_AREA && e->arg[0]->k != E_VAR)
            error_at(e->line, "UNSPEC of this expression is not supported yet");
        e->t.k = TY_BIT;
        e->t.p = a.k == TY_BIT ? a.p : 8 * n;   /* of bits: those bits (t228 Q1EVT: '1'B) */
        return;
    }
    case B_SIGN:
        if (!arith(e->arg[0]->t))
            error_at(e->line, "SIGN needs an arithmetic argument");
        e->t.k = TY_FIXBIN;
        e->t.p = 15;
        return;
    case B_FLOOR:
    case B_CEIL:
    case B_TRUNC: {
        struct type a = e->arg[0]->t;
        if (!arith(a)) {
            error_at(e->line, "FLOOR, CEIL and TRUNC need an arithmetic argument");
            return;
        }
        e->t = a;
        if (!is_float(a)) {
            /* (min(N, max(p-q+1, 1)), 0) (JOB00184) */
            e->t.p = min(a.k == TY_FIXBIN ? 31 : 15, max(a.p - a.q + 1, 1));
            e->t.q = 0;
        }
        return;
    }
    case B_ROUND: {
        struct type a = e->arg[0]->t;
        int n;
        if (a.k != TY_FIXDEC) {
            error_at(e->line, "ROUND of other than FIXED DECIMAL is not supported yet");
            return;
        }
        if (!int_arg(e->arg[1], &n))
            error_at(e->line, "ROUND needs a constant number of places");
        /* (p+1, q), rounded half away from zero (JOB00184) */
        e->t = a;
        e->t.p = min(15, a.p + 1);
        return;
    }
    case B_FIXED:
    case B_FLOAT:
    case B_BINARY:
    case B_DECIMAL:
    case B_PRECISION: {
        struct type t;
        if (!arith(e->arg[0]->t)) {
            error_at(e->line, "this function needs an arithmetic argument");
            return;
        }
        if (conv_type(e, e->arg[0]->t, e->fn, 1, &t))
            make_cast(e, e->arg[0], t);
        else
            e->t = e->arg[0]->t;
        return;
    }
    case B_ADD:
    case B_MULTIPLY:
    case B_DIVIDE: {
        struct type a = e->arg[0]->t, b = e->arg[1]->t, c, t;
        struct expr *op;
        if (!arith(a) || !arith(b)) {
            error_at(e->line, "ADD, MULTIPLY and DIVIDE need arithmetic arguments");
            return;
        }
        c = is_float(a) || is_float(b) ? float_common(a, b) : a;
        if (!is_float(c) && a.k != b.k)
            c.k = TY_FIXBIN;               /* bases that differ: binary */
        if (!conv_type(e, c, B_PRECISION, 2, &t)) {
            e->t = a;
            return;
        }
        op = new_node(E_BIN, e->line);
        op->op = e->fn == B_ADD ? T_PLUS : e->fn == B_MULTIPLY ? T_STAR : T_SLASH;
        if (is_float(t)) {
            /* the operands are converted to the result first */
            op->l = cast_of(e->arg[0], t);
            op->r = cast_of(e->arg[1], t);
            op->t = t;
            make_cast(e, op, t);
        } else {
            /* the exact result, then converted (truncated) */
            op->l = e->arg[0];
            op->r = e->arg[1];
            type_of(op);
            make_cast(e, op, t);
        }
        return;
    }
    case B_SQRT: case B_EXP: case B_LOG: case B_LOG10: case B_LOG2: case B_SIN: case B_COS:
    case B_TAN: case B_SIND: case B_COSD: case B_TAND: case B_ATAN: case B_ATAND:
    case B_SINH: case B_COSH: case B_TANH: case B_ATANH: case B_ERF: case B_ERFC:
        /* the mathematical functions: FLOAT of the argument's base and
         * precision; ATAN(y,x) of the two's */
        for (int i = 0; i < e->nargs; i++)
            if (!arith(e->arg[i]->t)) {
                error_at(e->line, "this function needs arithmetic arguments");
                return;
            }
        e->t = e->nargs == 2 ? float_common(e->arg[0]->t, e->arg[1]->t)
                             : float_of(e->arg[0]->t);
        /* a short and a long argument make ATAN(y,x) FLOAT BINARY of
         * 31 to 33 bits, whatever the bases: ten digits printed, and
         * binary in what follows (JOB00235, JOB00236) */
        if (e->nargs == 2 && flt_long(float_of(e->arg[0]->t)) != flt_long(float_of(e->arg[1]->t))) {
            e->t.k = TY_FLTBIN;
            e->t.p = 33;
        }
        return;
    case B_NULL:
        e->t.k = TY_PTR;
        e->t.p = 4;
        return;
    case B_NULLO:
        e->t.k = TY_OFFSET;
        e->t.p = 4;
        return;
    case B_SYSNULL:                     /* the system null pointer: 0 */
        e->t.k = TY_PTR;
        e->t.p = 4;
        return;
    case B_PTRADD:
        /* POINTERADD(p, n): p + n, n BIT or REAL FIXED (p,0) as FIXED
         * BIN(31) (SC26-3114-01, chapter 16) */
        if (e->arg[0]->t.k != TY_PTR)
            error_at(e->line, "POINTERADD needs a pointer first");
        if (!(arith(e->arg[1]->t) || is_bit(e->arg[1]->t)) || is_float(e->arg[1]->t))
            error_at(e->line, "POINTERADD adds a FIXED or BIT value");
        e->t.k = TY_PTR;
        e->t.p = 4;
        return;
    case B_PTRVALUE:
        if (!(arith(e->arg[0]->t) || is_bit(e->arg[0]->t)) || is_float(e->arg[0]->t))
            error_at(e->line, "POINTERVALUE takes a FIXED or BIT value");
        e->t.k = TY_PTR;
        e->t.p = 4;
        return;
    case B_BINVALUE:
        if (e->arg[0]->t.k != TY_PTR)
            error_at(e->line, "BINARYVALUE takes a pointer");
        e->t.k = TY_FIXBIN;
        e->t.p = 31;
        return;
    case B_PLIRETC:
        if (!arith(e->arg[0]->t) || is_float(e->arg[0]->t))
            error_at(e->line, "PLIRETC takes a FIXED value");
        e->t.k = TY_FIXBIN;
        e->t.p = 31;
        return;
    case B_ENTRYADDR:                   /* typed above */
        return;
    case B_PLIDUMP:
        /* the options, a string; the identification, a string of up to
         * 90 characters or a decimal constant (SC33-0006-7, p. 289) */
        if (e->nargs > 0 && !is_string(e->arg[0]->t))
            error_at(e->line, "PLIDUMP's options must be a character string");
        if (e->nargs > 1 && e->arg[1]->k == E_NUM && e->arg[1]->t.k == TY_FIXDEC) {
            /* a decimal constant: printed as it is written, its digits */
            struct expr *x = e->arg[1];
            int nd = (int)strlen(x->num), q = x->t.q;
            char *s = xmalloc((size_t)(nd + q) + 3), *d = s;
            if (q >= nd) {
                *d++ = '.';
                for (int i = nd; i < q; i++)
                    *d++ = '0';
            }
            for (int i = 0; i < nd; i++) {
                if (q > 0 && q < nd && i == nd - q)
                    *d++ = '.';
                *d++ = x->num[i];
            }
            *d = 0;
            x->k = E_STR;
            x->str = s;
            x->len = (int)(d - s);
            x->t.k = TY_CHAR;
            x->t.p = x->len;
            x->t.q = 0;
        }
        if (e->nargs > 1 && !is_string(e->arg[1]->t))
            error_at(e->line, "PLIDUMP's identification must be a character string or a "
                     "decimal constant");
        e->t.k = TY_FIXBIN;             /* no value: a subroutine */
        e->t.p = 31;
        return;
    case B_PLISRTA: case B_PLISRTB: case B_PLISRTC: case B_PLISRTD:
        return;                         /* type_sort */
    case B_PLICKPT:
        /* ddname, check-id, organization: strings; code: a FIXED
         * BINARY(31,0) variable (SC33-0006-7, p. 339) */
        for (int i = 0; i < e->nargs && i < 3; i++)
            if (!is_string(e->arg[i]->t))
                error_at(e->line, "argument %d of PLICKPT must be a character string", i + 1);
        if (e->nargs == 4 && (e->arg[3]->k != E_VAR || e->arg[3]->t.k != TY_FIXBIN ||
                              e->arg[3]->t.p != 31 || e->arg[3]->t.q != 0))
            error_at(e->line, "PLICKPT's code must be a FIXED BINARY(31,0) variable");
        e->t.k = TY_FIXBIN;             /* no value: a subroutine */
        e->t.p = 31;
        return;
    case B_PLIREST: case B_PLICANC:
        e->t.k = TY_FIXBIN;
        e->t.p = 31;
        return;
    case B_REAL: case B_IMAG: case B_CONJG: case B_COMPLEX: case B_CPXSTR: case B_CXOP:
    case B_CXABS: case B_CXFN: case B_CXCVT:
        return;                         /* type_cplxfn, type_cpxstr */
    case B_COMPLETION: case B_STATUS:
        /* of an event: its completion, BIT(1); its status, FIXED
         * BINARY(31) (the PUT LIST field, t228 Q1EVT) */
        if (e->arg[0]->t.k != TY_EVENT || e->arg[0]->k != E_VAR)
            error_at(e->line, "%s takes an event variable", e->fn == B_STATUS ? "STATUS" :
                     "COMPLETION");
        else if (e->arg[0]->nsubs != cum_dims(e->arg[0]->sym))
            error_at(e->line, "%s of an array is not supported here", e->fn == B_STATUS ?
                     "STATUS" : "COMPLETION");
        memset(&e->t, 0, sizeof e->t);
        e->t.k = e->fn == B_STATUS ? TY_FIXBIN : TY_BIT;
        e->t.p = e->fn == B_STATUS ? 31 : 1;
        return;
    case B_PLIRETV:                     /* FIXED BINARY(31,0) */
        e->t.k = TY_FIXBIN;
        e->t.p = 31;
        return;
    case B_DATETIME:                    /* yyyymmddhhmmssttt */
        e->t.k = TY_CHAR;
        e->t.p = 17;
        return;
    case B_EMPTY:
        e->t.k = TY_AREA;
        e->t.p = 0;
        return;
    case B_ADDR:
    case B_STG:
    case B_CSTG:
        return;
    case B_ONCODE:
        e->t.k = TY_FIXBIN;
        e->t.p = 15;
        return;
    case B_CHAR: case B_BIT: {
        /* CHAR(x [, n]), BIT(x [, n]) (JOB00379): x as a string, then n
         * characters or bits, cut or padded */
        struct type a = e->arg[0]->t;
        int n = 0;
        memset(&e->t, 0, sizeof e->t);
        if (e->nargs == 2 && !int_arg(e->arg[1], &n))
            error_at(e->line, "the length of %s must be a constant", e->fn == B_CHAR ? "CHAR" :
                     "BIT");
        if (e->fn == B_CHAR) {
            e->t.k = TY_CHAR;
            e->t.p = n ? n : is_string(a) || is_bit(a) ? a.p : is_float(a) ? flt_digits(a) + 6 :
                     dec_prec(a) + 3;
        } else {
            if (is_float(a) || (arith(a) && a.q))
                error_at(e->line, "BIT of a FLOAT or scaled value is not supported yet");
            e->t.k = TY_BIT;
            e->t.p = n ? n : is_bit(a) || is_string(a) ? a.p : a.k == TY_FIXBIN ? a.p :
                     ceil_mul332(a.p);
            if (e->t.p > MAXBITS)
                error_at(e->line, "a bit string of more than %d bits", MAXBITS);
        }
        return;
    }
    case B_ONFILE:
        /* the name of the file of the last file condition */
        e->t.k = TY_CHAR;
        e->t.p = 31;
        return;
    case B_BOOL: {
        /* BOOL(x, y, z): bit by bit, z's four bits the results for the
         * pairs 00, 01, 10, 11; the shorter operand padded with zeros
         * (Q5BYN) */
        for (int i = 0; i < 3; i++) {
            if (!is_bit(e->arg[i]->t)) {
                if (!is_string(e->arg[i]->t))
                    error_at(e->line, "BOOL takes bit strings");
                e->arg[i] = conv_bit(e->arg[i]);
            }
        }
        memset(&e->t, 0, sizeof e->t);
        e->t.k = TY_BIT;
        e->t.p = e->arg[0]->t.p > e->arg[1]->t.p ? e->arg[0]->t.p : e->arg[1]->t.p;
        return;
    }
    case B_AFTER: case B_BEFORE: case B_DECAT: case B_REVERSE: case B_COPY: {
        /* ANSI's string functions (rt/pl1ansi.c): the result at most as
         * long as the first argument; COPY's n times that */
        int n = 0, sl, k = e->fn == B_REVERSE || e->fn == B_COPY ? 1 : 2, bits = 1;
        /* the common string type (ANSI 9.4.4): BIT if every string
         * argument is a bit string, else CHARACTER, a bit string made
         * the characters '0' and '1' (Tier 69) */
        for (int i = 0; i < k; i++) {
            if (!is_string(e->arg[i]->t) && !is_bit(e->arg[i]->t))
                error_at(e->line, "this function takes strings");
            if (!is_bit(e->arg[i]->t))
                bits = 0;
        }
        for (int i = 0; i < k && !bits; i++)
            if (is_bit(e->arg[i]->t)) {
                e->arg[i] = new_call1(B_CHAR, e->arg[i]);
                type_of(e->arg[i]);
            }
        sl = bits ? e->arg[0]->t.p : string_len(e->arg[0]->t);
        if (e->fn == B_DECAT) {
            /* the pattern as BIT(3): its first three bits, zeros after */
            struct expr *b = new_node(E_CALL, e->line), *n = new_node(E_NUM, e->line);
            n->num = "3";
            n->t.k = TY_FIXDEC;
            n->t.p = 1;
            b->fn = B_BIT;
            b->nargs = 2;
            b->arg[0] = e->arg[2];
            b->arg[1] = n;
            type_of(b);
            e->arg[2] = b;
        }
        if (e->fn == B_COPY && !arith(e->arg[1]->t))
            error_at(e->line, "COPY needs an arithmetic count");
        e->t.k = bits ? TY_BIT : TY_CHAR;
        e->t.p = sl;
        if (e->fn == B_COPY)
            e->t.p = int_arg(e->arg[1], &n) ? (n > 0 ? sl * n : 0) : 256;
        if (e->t.p > (bits ? MAXBITS : 32767))
            error_at(e->line, "COPY result too long");
        return;
    }
    case B_COLLATE:
        e->t.k = TY_CHAR;
        e->t.p = 256;
        return;
    case B_ASIN: case B_ACOS: case B_SUBTRACT: case B_OFFSETF: case B_POINTERF:
        return;                             /* ansi_rewrite */
    case B_STRING:
        return;                             /* typed above */
    case B_PAGENO:
        /* ANSI 9.4.4.62: the page number of a PRINT file */
        if (e->arg[0]->t.k != TY_FILE)
            error_at(e->line, "PAGENO needs a file");
        e->t.k = TY_FIXBIN;
        e->t.p = 15;
        return;
    case B_VALID:
        /* ANSI 9.4.4.86: '1'B if the picture variable holds a value its
         * picture can have (rt/pl1ansi.c) */
        if (e->arg[0]->k != E_VAR || !e->arg[0]->sym->t.pic)
            error_at(e->line, "VALID needs a PICTURE variable");
        e->t.k = TY_BIT;
        e->t.p = 1;
        return;
    case B_ONLOC:
        /* the entry point of the block that raised the condition (Q1MISC) */
        e->t.k = TY_CHAR;
        e->t.p = 31;
        return;
    case B_DATAFIELD:
        /* the data field of a NAME condition (Q1NAME); PL/I (F) keeps at
         * most 255 characters */
        e->t.k = TY_CHAR;
        e->t.p = 255;
        return;
    case B_DATE:
    case B_TIME:
        /* yymmdd, hhmmssttt */
        e->t.k = TY_CHAR;
        e->t.p = e->fn == B_DATE ? 6 : 9;
        return;
    case B_ONCOUNT:
    case B_COUNT:
    case B_LINENO:
        /* ONCOUNT is 0 without multitasking; COUNT the items of the last
         * GET or PUT on the file, LINENO the line of a PRINT file */
        if (e->fn != B_ONCOUNT && e->arg[0]->t.k != TY_FILE)
            error_at(e->line, "%s needs a file", e->fn == B_COUNT ? "COUNT" : "LINENO");
        if (e->fn == B_COUNT)
            uses_count = 1;
        e->t.k = TY_FIXBIN;
        e->t.p = 15;
        return;
    case B_ONKEY:
        /* the key of the record of a KEY or RECORD condition (Q1OK) */
        e->t.k = TY_CHAR;
        e->t.p = 255;
        return;
    case B_ONCHAR:
    case B_ONSOURCE:
        /* the character, or the field, of the conversion that failed */
        e->t.k = TY_CHAR;
        e->t.p = e->fn == B_ONCHAR ? 1 : ONSOURCE_MAX;
        return;
    case B_LENGTH:
        if (!is_string(e->arg[0]->t) && !arith(e->arg[0]->t) && !is_bit(e->arg[0]->t))
            error_at(e->line, "LENGTH needs a string argument");
        e->t.k = TY_FIXBIN;
        e->t.p = 15;
        return;
    case B_INDEX:
        /* both bit strings: a bit position (rt/pl1ansi.c); one of each:
         * the bit string as CHARACTER, as for AFTER and BEFORE */
        for (int i = 0; i < 2; i++)
            if (!is_string(e->arg[i]->t) && !is_bit(e->arg[i]->t))
                error_at(e->line, "INDEX needs string arguments");
        if (is_bit(e->arg[0]->t) != is_bit(e->arg[1]->t))
            for (int i = 0; i < 2; i++)
                if (is_bit(e->arg[i]->t)) {
                    e->arg[i] = new_call1(B_CHAR, e->arg[i]);
                    type_of(e->arg[i]);
                }
        e->t.k = TY_FIXBIN;
        e->t.p = 15;
        return;
    case B_SUBSTR:
        if (is_bit(e->arg[0]->t)) {
            /* of a bit string: a bit string of at most its length */
            for (int i = 1; i < e->nargs; i++)
                if (!arith(e->arg[i]->t))
                    error_at(e->line, "SUBSTR positions must be arithmetic");
            e->t.k = TY_BIT;
            e->t.p = e->arg[0]->t.p;
            if (e->nargs == 3 && e->arg[2]->k == E_NUM && e->arg[2]->t.q == 0) {
                int j = 0;
                for (const char *d = e->arg[2]->num; *d; d++)
                    j = j * 10 + (*d - '0');
                if (j <= e->t.p)
                    e->t.p = j;
            }
            return;
        }
        if (!is_string(e->arg[0]->t) && !arith(e->arg[0]->t))
            error_at(e->line, "SUBSTR needs a string");
        for (int i = 1; i < e->nargs; i++)
            if (!arith(e->arg[i]->t))
                error_at(e->line, "SUBSTR positions must be arithmetic");
        e->t.k = TY_CHAR;
        e->t.p = string_len(e->arg[0]->t);
        if (e->nargs == 3 && e->arg[2]->k == E_NUM && e->arg[2]->t.q == 0) {
            int j = 0;
            for (const char *d = e->arg[2]->num; *d; d++)
                j = j * 10 + (*d - '0');
            e->t.p = j;
        }
        return;
    case B_ABS:
        if (!arith(e->arg[0]->t))
            error_at(e->line, "ABS needs an arithmetic argument");
        e->t = e->arg[0]->t;
        return;
    case B_MOD: {
        struct type a = e->arg[0]->t, b = e->arg[1]->t;
        if (!arith(a) || !arith(b)) {
            error_at(e->line, "MOD needs arithmetic arguments");
            e->t.k = TY_FIXBIN; e->t.p = 31;
            return;
        }
        if (is_float(a) || is_float(b)) {
            error_at(e->line, "MOD of FLOAT values is not supported yet");
            e->t = float_common(a, b);
            return;
        }
        common(&a, &b);
        if (a.q != 0 || b.q != 0)
            error_at(e->line, "MOD with fractions is not supported yet");
        e->t.k = a.k;
        e->t.q = max(a.q, b.q);
        e->t.p = min(a.k == TY_FIXBIN ? 31 : 15, b.p - b.q + e->t.q);
        return;
    }
    case B_DIM:
    case B_LBOUND:
    case B_HBOUND: {
        /* folded by lower.c, but for an A(*) parameter: from its
         * descriptor at run time */
        struct expr *a = e->arg[0];
        if (a->k != E_VAR || !a->sym->stardims || a->nsubs)
            error_at(e->line, "internal: bound function not folded");
        else if (!(e->arg[1]->k == E_NUM && e->arg[1]->t.q == 0) ||
                 atoi(e->arg[1]->num) < 1 || atoi(e->arg[1]->num) > a->sym->ndims)
            error_at(e->line, "the dimension of %s must be a constant from 1 to %d",
                     a->sym->name, a->sym->ndims);
        e->t.k = TY_FIXBIN;
        e->t.p = 15;
        e->t.q = 0;
        return;
    }
    case B_MAX:
    case B_MIN: {
        int anybin = 0, anyflt = 0, ip = 0, q = 0;
        for (int i = 0; i < e->nargs; i++) {
            if (!arith(e->arg[i]->t)) {
                error_at(e->line, "MAX and MIN need arithmetic arguments");
                return;
            }
            if (e->arg[i]->t.k == TY_FIXBIN)
                anybin = 1;
            if (is_float(e->arg[i]->t))
                anyflt = 1;
        }
        if (anyflt) {
            e->t = e->arg[0]->t;
            for (int i = 1; i < e->nargs; i++)
                e->t = float_common(e->t, e->arg[i]->t);
            e->t = float_of(e->t);
            return;
        }
        for (int i = 0; i < e->nargs; i++) {
            struct type t = e->arg[i]->t;
            int p = anybin ? bin_prec(t) : t.p, s = anybin ? bin_scale(t) : t.q;
            ip = max(ip, p - s);
            q = max(q, s);
        }
        e->t.k = anybin ? TY_FIXBIN : TY_FIXDEC;
        e->t.q = q;
        e->t.p = min(anybin ? 31 : 15, ip + q);
        return;
    }
    }
}

/* x ** n with an integer n.  A FIXED x and an unsigned integer constant n
 * give FIXED of precision ((p+1)*n-1, q*n) while that fits -- computed as
 * n-1 multiplications, which have exactly that type --, else FLOAT of x's
 * base and precision (JOB00182: I**2 with I FIXED BIN(31) is FLOAT, 2**10
 * is 1E+03). */
static void type_power(struct expr *e)
{
    struct type a = e->l->t, b = e->r->t;
    int n;
    if (is_string(a)) {
        e->l = conv(e->l, 0);
        a = e->l->t;
    }
    if (!arith(a) || !arith(b)) {
        error_at(e->line, "** needs arithmetic operands");
        e->t = b;
        return;
    }
    if (is_float(b) || b.q != 0) {
        /* x ** y in general: FLOAT, EXP(y*LOG(x)) as the library computes
         * it (IHEXXS, IHEXXL) */
        e->t = float_common(a, b);
        return;
    }
    if (!is_float(a) && int_arg(e->r, &n) && n >= 1 && e->r->k == E_NUM &&
        (a.p + 1) * n - 1 <= (a.k == TY_FIXBIN ? 31 : 15)) {
        struct expr *x = e->l, *m;
        if (x->k != E_VAR && x->k != E_NUM && !(x->k == E_NEG && x->l->k == E_NUM))
            error_at(e->line, "** of this FIXED expression is not supported yet");
        if (x->k == E_VAR && x->nsubs)
            for (int i = 0; i < x->nsubs; i++)
                if (x->sub[i]->k != E_NUM)
                    error_at(e->line, "** of this FIXED expression is not supported yet");
        m = x;
        for (int i = 1; i < n; i++) {
            struct expr *t = new_node(E_BIN, e->line);
            t->op = T_STAR;
            t->l = m;
            t->r = x;
            type_of(t);
            m = t;
        }
        *e = *m;
        return;
    }
    e->t = float_of(a);
}

static void type_of(struct expr *e)
{
    switch (e->k) {
    case E_NUM:
    case E_STR:
    case E_BITC:
        return;
    case E_ISUB:
        error_at(e->line, "iSUB belongs in the base reference of a DEFINED item");
        return;
    case E_STAR:
        error_at(e->line, "a cross section is not supported here yet");
        return;
    case E_LABEL:
        memset(&e->t, 0, sizeof e->t);
        e->t.k = TY_LABEL;
        e->t.p = 8;
        return;
    case E_ENTRY:
        memset(&e->t, 0, sizeof e->t);
        e->t.k = TY_ENTRY;
        e->t.p = 8;
        return;
    case E_VAR:
        for (int i = 0; i < e->nsubs; i++) {
            type_of(e->sub[i]);
            if (!arith(e->sub[i]->t))
                error_at(e->line, "a subscript must be arithmetic");
            else if (is_float(e->sub[i]->t))
                error_at(e->line, "a FLOAT subscript is not supported yet");
        }
        check_based(e);
        if (e->sym->child)
            error_at(e->line, "structure %s cannot be used here", e->sym->name);
        else if (e->nsubs != cum_dims(e->sym))
            error_at(e->line, "%s needs %d subscripts", e->sym->name, cum_dims(e->sym));
        e->t = e->sym->t;              /* a part's own symbol for a complex part */
        if (e->t.k == TY_PIC && e->picstr) {
            /* a numeric picture's characters (lower.c pic_expr) */
            e->t.k = TY_CHAR;
            e->t.p = pic_length(e->sym->t);
            e->t.q = 0;
            e->t.pic = NULL;
        }
        return;
    case E_CALL:
        type_call(e);
        return;
    case E_FCALL:
        type_fcall(e, 1);
        return;
    case E_CONV:
        type_of(e->l);
        return;
    case E_CAST:
        return;                           /* typed when made */
    case E_NEG:
        if (cx_expr(e->l))
            error_at(e->line, "a complex value is not supported here yet");
        type_of(e->l);
        if (is_string(e->l->t))
            e->l = conv(e->l, 0);
        if (!arith(e->l->t))
            error_at(e->line, "prefix minus needs an arithmetic operand");
        e->t = e->l->t;
        return;
    case E_NOT:
        type_of(e->l);
        if (!is_bit(e->l->t))
            error_at(e->line, "NOT needs a bit string operand");
        e->t = e->l->t;
        e->t.k = TY_BIT;
        return;
    case E_BIN:
        break;
    }
    if (cx_expr(e->l) || cx_expr(e->r)) {
        /* complex operands: = and ¬= compare the parts (cx_compare); the
         * arithmetic was split where the value is used */
        if (cx_expr(e) || !cx_compare(e)) {
            if (cx_expr(e))
                error_at(e->line, "a complex value is not supported here yet");
            memset(&e->t, 0, sizeof e->t);
            e->t.k = TY_BIT;
            e->t.p = 1;
            return;
        }
    }
    type_of(e->l);
    type_of(e->r);
    {
        struct type a = e->l->t, b = e->r->t;

        if (e->op == T_AND || e->op == T_OR) {
            if (!is_bit(a) || !is_bit(b))
                error_at(e->line, "& and | need bit strings or comparisons");
            e->t.k = TY_BIT;
            e->t.p = max(a.p, b.p);
            return;
        }
        if (e->op == T_POWER) {
            type_power(e);
            return;
        }
        if (e->op == T_CAT) {
            if (is_bit(a) && is_bit(b)) {
                e->t.k = TY_BIT;
                e->t.p = a.p + b.p;
                if (e->t.p > MAXBITS)
                    error_at(e->line, "a bit string of more than %d bits", MAXBITS);
                return;
            }
            if (is_bit(a) || is_bit(b))
                error_at(e->line, "|| of a bit string and a character string is not supported "
                         "yet");
            e->t.k = TY_CHAR;
            e->t.p = string_len(a) + string_len(b);
            return;
        }
        if ((e->op == T_PLUS || e->op == T_MINUS) && (a.k == TY_PTR || b.k == TY_PTR)) {
            /* LANGLVL(SPROG): P + n, P - n, n + P as POINTERADD, P2 - P1 as
             * the difference of the BINARYVALUEs (SC26-3114-01, "Pointer
             * Operations") */
            struct expr *l = e->l, *r = e->r;
            beyond_f(e->line, L_MVS, "pointer arithmetic");
            if (!opt_sprog) {
                error_at(e->line, "pointer arithmetic needs LANGLVL(SPROG)");
            } else if (a.k == TY_PTR && b.k == TY_PTR && e->op == T_MINUS) {
                e->l = sprog_call(B_BINVALUE, l, NULL);
                e->r = sprog_call(B_BINVALUE, r, NULL);
                type_of(e);
            } else if ((a.k == TY_PTR) != (b.k == TY_PTR) &&
                       !(b.k == TY_PTR && e->op == T_MINUS)) {
                struct expr *p = a.k == TY_PTR ? l : r, *n = a.k == TY_PTR ? r : l;
                if (!((arith(n->t) && !is_float(n->t)) || is_bit(n->t)))
                    error_at(e->line, "a pointer takes a FIXED or BIT value added");
                if (e->op == T_MINUS) {
                    struct expr *m = xmalloc(sizeof *m);
                    m->k = E_NEG;
                    m->line = n->line;
                    m->l = n;
                    m->t = n->t;
                    n = m;
                }
                e->k = E_CALL;
                e->fn = B_PTRADD;
                e->nargs = 2;
                e->arg[0] = p;
                e->arg[1] = n;
                e->l = e->r = NULL;
                memset(&e->t, 0, sizeof e->t);
                e->t.k = TY_PTR;
                e->t.p = 4;
            } else {
                error_at(e->line, "this pointer arithmetic is not allowed");
            }
            return;
        }
        if (opt_sprog && a.k == TY_PTR && b.k == TY_PTR &&
            (e->op == T_LT || e->op == T_LE || e->op == T_GT || e->op == T_GE)) {
            /* LANGLVL(SPROG): pointers compared by their values */
            beyond_f(e->line, L_MVS, "pointers compared by < or >");
            e->l = sprog_call(B_BINVALUE, e->l, NULL);
            e->r = sprog_call(B_BINVALUE, e->r, NULL);
            type_of(e);
            return;
        }
        if (e->op == T_EQ || e->op == T_NE || e->op == T_LT || e->op == T_LE ||
            e->op == T_GT || e->op == T_GE) {
            if (a.k == TY_LABEL || b.k == TY_LABEL) {
                /* PL/I (F) refuses it (IEM1024I, JOB00304) */
                error_at(e->line, "labels cannot be compared");
            } else if (a.k == TY_AREA || b.k == TY_AREA) {
                error_at(e->line, "areas cannot be compared");
            } else if (locator(a) || locator(b)) {
                /* locators, for equality: an offset with a pointer as a
                 * pointer (GC28-8201-4, offset variables, rule 3) */
                if (!locator(a) || !locator(b) || (e->op != T_EQ && e->op != T_NE))
                    error_at(e->line, "a locator is compared to a locator, for equality only");
                else if (a.k == TY_OFFSET && b.k == TY_PTR)
                    e->l = offset_ptr(e->l);
                else if (a.k == TY_PTR && b.k == TY_OFFSET)
                    e->r = offset_ptr(e->r);
            } else if ((is_string(a) && is_string(b)) || (is_bit(a) && is_bit(b))) {
                /* compared as characters or as bits */
            } else if (arith(a) && is_string(b)) {
                e->r = conv_with(e->r, a);      /* compared as numbers */
            } else if (is_string(a) && arith(b)) {
                e->l = conv_with(e->l, b);
            } else if (!(arith(a) && arith(b))) {
                error_at(e->line, "comparison of these types is not supported yet");
            }
            e->t.k = TY_BIT;
            e->t.p = 1;
            return;
        }
        if (is_string(a) && arith(b)) {
            e->l = conv_with(e->l, b);
            a = e->l->t;
        } else if (arith(a) && is_string(b)) {
            e->r = conv_with(e->r, a);
            b = e->r->t;
        } else if (is_string(a) && is_string(b)) {
            e->l = conv(e->l, 0);
            e->r = conv(e->r, 0);
            a = e->l->t;
            b = e->r->t;
        }
        if (!arith(a) || !arith(b)) {
            error_at(e->line, "arithmetic needs arithmetic operands");
            e->t.k = TY_FIXBIN;
            e->t.p = 31;
            return;
        }
        if (is_float(a) || is_float(b)) {
            e->t = float_common(a, b);
            return;
        }
        common(&a, &b);
        {
            int lim = a.k == TY_FIXBIN ? 31 : 15;
            e->t.k = a.k;
            switch (e->op) {
            case T_STAR:
                e->t.p = min(lim, a.p + b.p + 1);
                e->t.q = a.q + b.q;
                break;
            case T_SLASH:
                e->t.p = lim;
                e->t.q = lim - ((a.p - a.q) + b.q);
                if (e->t.q < 0)
                    error_at(e->line, "the quotient would have a negative scale");
                break;
            default:
                e->t.q = max(a.q, b.q);
                e->t.p = min(lim, max(a.p - a.q, b.p - b.q) + e->t.q + 1);
                break;
            }
            if (e->t.q > e->t.p)
                error_at(e->line, "the result scale exceeds its precision");
        }
    }
}

static void check_assign(struct type tt, struct expr **pe, int line)
{
    struct expr *e = *pe;
    if (tt.k == TY_EVENT || (e->k == E_VAR && e->sym && !e->sym->child &&
                             e->sym->t.k == TY_EVENT)) {
        /* an event takes an event, and nothing else takes one (PL/I (F):
         * IEM1793I, t228 Q4E1-Q4E3) */
        type_of(e);
        if (tt.k != TY_EVENT || e->t.k != TY_EVENT || e->k != E_VAR)
            error_at(line, "an event can be assigned only another event");
        return;
    }
    if (tt.k == TY_ENTRY && e->k == E_FCALL && e->bare && e->callee) {
        /* an ENTRY variable takes the procedure, not its value */
        if (e->entryno || e->callee->is_stub)
            error_at(line, "an ENTRY variable takes a procedure of this compilation, by "
                     "its first entry point");
        e->k = E_ENTRY;
        e->nargs = 0;
    }
    if (tt.k == TY_PIC) {
        /* a numeric picture is assigned its value as FIXED DECIMAL(p,q) */
        tt.k = TY_FIXDEC;
        tt.pic = NULL;
    }
    type_of(e);
    if (is_float(tt) && is_string(e->t)) {
        /* a string to FLOAT: '-1.5E3' is -1.5E3 (JOB00156) */
        *pe = conv(e, 0);
        (*pe)->t = tt;
        return;
    }
    if (arith(tt) && is_string(e->t)) {
        *pe = conv(e, dec_scale(tt));   /* converted to the target's scale */
        return;
    }
    if (arith(tt) && arith(e->t))
        return;
    if (is_string(tt) && (is_string(e->t) || arith(e->t)))
        return;
    if (is_bit(tt) && is_bit(e->t))
        return;
    if (is_string(tt) && is_bit(e->t))
        return;                         /* '1' and '0' characters (JOB00336) */
    if (arith(tt) && is_bit(e->t)) {
        /* the bits' value, as FIXED BINARY(31) (JOB00336) */
        struct type b;
        memset(&b, 0, sizeof b);
        b.k = TY_FIXBIN;
        b.p = 31;
        *pe = cast_of(e, b);
        return;
    }
    if (tt.k == TY_PTR && e->t.k == TY_PTR)
        return;
    if (tt.k == TY_OFFSET && e->t.k == TY_OFFSET)
        return;                         /* the areas named are ignored */
    if ((tt.k == TY_OFFSET && e->t.k == TY_PTR) || (tt.k == TY_PTR && e->t.k == TY_OFFSET)) {
        /* converted by the offset's area; a null value is not
         * (PL/I (F): IEM1797I) */
        if (e->k == E_CALL && (e->fn == B_NULL || e->fn == B_NULLO))
            error_at(line, "NULL and NULLO are not converted: a pointer takes NULL, an offset "
                     "NULLO");
        else if (tt.k == TY_PTR)
            *pe = offset_ptr(e);
        else if (!tt.area)
            error_at(line, "a pointer is converted to an offset only by its area");
        else
            *pe = cast_of(e, tt);
        return;
    }
    if (tt.k == TY_AREA && e->t.k == TY_AREA)
        return;
    if (tt.k == TY_LABEL && e->t.k == TY_LABEL)
        return;
    if ((tt.k == TY_ENTRY && e->t.k == TY_ENTRY) || (tt.k == TY_FILE && e->t.k == TY_FILE))
        return;
    error_at(line, "this assignment between types is not supported yet");
}

/* A name of GET DATA: a scalar, an array of one dimension, or a
 * structure of such members. */
static void check_data_sym(struct sym *x, int line)
{
    if (x->child) {
        if (x->ndims)
            error_at(line, "GET DATA of an array of structures is not supported yet");
        for (struct sym *c = x->child; c; c = c->sibling)
            check_data_sym(c, line);
        return;
    }
    if (x->ndims > 1 || cum_dims(x) > x->ndims)
        error_at(line, "GET DATA of %s: one dimension at most here", x->name);
    if (!arith(x->t) && !is_cplx(x->t) && !is_string(x->t) && x->t.k != TY_BIT)
        error_at(line, "GET DATA of %s is not supported", x->name);
}

static void check_get_data(struct expr *e, int line)
{
    if (e->k != E_VAR || e->nsubs || e->qual) {
        error_at(line, "GET DATA takes names");
        return;
    }
    if (e->sym->root->st == ST_BASED)
        error_at(line, "GET DATA of a BASED variable is not supported yet");
    check_data_sym(e->sym, line);
}

static void check_cond(struct expr *e, int line)
{
    type_of(e);
    if (!is_bit(e->t))
        error_at(line, "a condition must be a bit string or a comparison");
}

static int has_varying(struct sym *s)
{
    if (!s->child)
        return s->t.k == TY_VCHAR;
    for (struct sym *c = s->child; c; c = c->sibling)
        if (has_varying(c))
            return 1;
    return 0;
}

/* A file reference: a file constant, usable in that direction. */
static void check_file(struct expr *f, int line, int dir)
{
    if (!f || f->k != E_VAR || !f->sym || !f->sym->is_file) {
        error_at(line, "%s is not a file", f && f->sym ? f->sym->name : "this");
        return;
    }
    if (dir == FL_OUTPUT && (f->sym->is_file & FL_INPUT) && !(f->sym->is_file & FL_OUTPUT))
        error_at(line, "%s is an INPUT file", f->sym->name);
    if (dir == FL_INPUT && (f->sym->is_file & FL_OUTPUT) && !(f->sym->is_file & FL_INPUT))
        error_at(line, "%s is an OUTPUT file", f->sym->name);
}

/* KEY, KEYFROM, KEYTO of record I/O: a source key that converts to a
 * character string, a KEYTO variable of CHARACTER; a REGIONAL(1) file
 * writes with KEYFROM, a DIRECT one reads and rewrites with KEY. */
static void check_keys(struct stmt *s)
{
    int fa = s->file && s->file->sym && s->file->sym->is_file ? s->file->sym->is_file : 0;
    if (s->e) {
        type_of(s->e);
        if (!arith(s->e->t) && !is_string(s->e->t) && !is_bit(s->e->t))
            error_at(s->line, "a key is a character string");
    }
    if (s->e2) {
        type_of(s->e2);
        if (s->e2->k != E_VAR || s->e2->t.k != TY_CHAR || s->e2->sym->child)
            error_at(s->line, "KEYTO takes a CHARACTER variable");
        if (s->e)
            error_at(s->line, "KEY and KEYTO in one READ");
    }
    if ((s->e || s->e2) && fa && !(fa & FL_KEYED))
        error_at(s->line, "the file needs the KEYED attribute for KEY, KEYFROM or KEYTO");
    if (s->k == S_WRITE && (fa & (FL_REGIONAL | FL_INDEXED)) && !s->e)
        error_at(s->line, "WRITE on a REGIONAL or INDEXED file needs KEYFROM");
    if (s->k == S_DELETE && !s->e && (fa & FL_DIRECT))
        error_at(s->line, "DELETE on a DIRECT file needs KEY");
    if ((s->k == S_READ || s->k == S_REWRITE) && (fa & FL_DIRECT) && !s->e)
        error_at(s->line, "%s on a DIRECT file needs KEY", s->k == S_READ ? "READ" : "REWRITE");
}

/* A compiler temporary of type t in the current block (TO, BY). */
static struct sym *iter_temp(struct type t)
{
    struct sym *v;
    if (is_cplx(t)) {
        /* the two parts, the real one first (cx_call) */
        v = iter_temp(part_type(t));
        v->t = t;
        v->size *= 2;
        return v;
    }
    v = new_loop_var(CUR);
    if (t.k == TY_PIC) {
        t.k = TY_FIXDEC;
        t.pic = NULL;
    }
    v->t = t;
    switch (t.k) {
    case TY_FIXBIN: v->size = v->align = t.p <= 15 ? 2 : 4; break;
    case TY_FIXDEC: v->size = dec_bytes(t.p); v->align = 1; break;
    case TY_CHAR:   v->size = t.p ? t.p : 1; v->align = 1; break;
    case TY_VCHAR:  v->size = 2 + t.p; v->align = 2; break;
    case TY_BIT:    v->size = (t.p + 7) / 8 ? (t.p + 7) / 8 : 1; v->align = 1; break;
    case TY_VBIT:   v->size = 2 + (t.p + 7) / 8; v->align = 2; break;
    case TY_PTR:
    case TY_OFFSET: v->size = v->align = 4; break;
    default:        v->size = v->align = flt_long(t) ? 8 : 4; break;
    }
    return v;
}

static struct expr *iter_var(struct sym *v, int line)
{
    struct expr *e = new_node(E_VAR, line);
    e->sym = v;
    return e;
}

static struct expr *iter_num(const char *digits, int line)
{
    struct expr *e = new_node(E_NUM, line);
    e->num = (char *)digits;
    e->t.k = TY_FIXDEC;
    e->t.p = 1;
    return e;
}

static struct expr *iter_bin(int op, struct expr *l, struct expr *r, int line)
{
    struct expr *e = new_node(E_BIN, line);
    e->op = op;
    e->l = l;
    e->r = r;
    type_of(e);
    return e;
}

/* ---- COMPLEX (#2, tier C1) ------------------------------------------ */

/* A complex value is split into its two parts before it is typed: an
 * assignment to a complex variable becomes one to its real part and
 * one to its imaginary part, each of real expressions over the parts
 * (internals/design/complex.md section 3.4).  The parts then go
 * through the real type rules, which give PL/I (F)'s precisions: a
 * real operand meets a zero imaginary part of its own attributes
 * (GC28-8201-4, "Mode"), and COMPLEX(a,b) is the parts converted to
 * their common type (t227 Q4CBIF).  Comparisons = and ¬= become the
 * AND or OR of the parts' comparisons; REAL and IMAG choose a part.
 * What else meets a complex value -- * / **, arguments, RETURNS, PUT
 * EDIT, GET -- is refused here for now (tiers C2 to C5). */

static struct sym *iter_temp(struct type t);

/* a numeric picture's value: FIXED DECIMAL(p,q) */
static struct type cx_real(struct type t)
{
    if (t.k == TY_PIC) {
        t.k = TY_FIXDEC;
        t.pic = NULL;
    }
    return t;
}

/* a mathematical function that takes a complex argument (GC28-8201-4,
 * Figure G-1) */
static int cx_math(enum bfn f)
{
    return f == B_SQRT || f == B_EXP || f == B_LOG || f == B_SIN || f == B_COS ||
           f == B_SINH || f == B_COSH || f == B_TAN || f == B_TANH || f == B_ATAN ||
           f == B_ATANH;
}

static int cx_expr(struct expr *e)
{
    if (!e)
        return 0;
    switch (e->k) {
    case E_NUM:
        return e->imag;
    case E_VAR:
        return e->sym && !e->part && !e->sym->child && is_cplx(e->sym->t);
    case E_BIN:
        return (e->op == T_PLUS || e->op == T_MINUS || e->op == T_STAR || e->op == T_SLASH ||
                e->op == T_POWER) && (cx_expr(e->l) || cx_expr(e->r));
    case E_NEG:
        return cx_expr(e->l);
    case E_CALL:
        if (e->fn == B_ADD || e->fn == B_MULTIPLY || e->fn == B_DIVIDE)
            return e->nargs >= 2 && (cx_expr(e->arg[0]) || cx_expr(e->arg[1]));
        if (cx_math(e->fn) && e->nargs == 1)
            return cx_expr(e->arg[0]);
        if (e->fn == B_CXCVT)
            return 1;
        return e->fn == B_COMPLEX || (e->fn == B_CONJG && cx_expr(e->arg[0]));
    case E_FCALL:
        return e->callee && e->callee->has_ret && is_cplx(e->callee->ret);
    default:
        return 0;
    }
}

/* A part of a complex variable as a symbol: a copy with the part's
 * type whose root is the variable's, so the same storage (gen_place
 * adds the imaginary part's offset for an E_VAR with part 2), and the
 * code generator sees a real variable.  Not in any block's list: it
 * allocates nothing. */
static struct sym *cx_sym(struct sym *v, int part)
{
    if (!v->cpart[part - 1]) {
        struct sym *c = xmalloc(sizeof *c);
        *c = *v;
        c->t = part_type(v->t);
        c->next = NULL;
        c->init = NULL;
        c->ninits = 0;
        c->cpart[0] = c->cpart[1] = NULL;
        /* root stays the variable's: gen.c lays out the frame and finds
         * the storage through it */
        v->cpart[part - 1] = c;
    }
    return v->cpart[part - 1];
}

/* a copy of an expression not yet typed, its subtrees copied too, so
 * that each part types its own */
static struct expr *cx_copy(const struct expr *e)
{
    struct expr *c;
    if (!e)
        return NULL;
    c = xmalloc(sizeof *c);
    *c = *e;
    c->l = cx_copy(e->l);
    c->r = cx_copy(e->r);
    c->qual = cx_copy(e->qual);
    for (int i = 0; i < e->nargs && i < MAXCALLARGS; i++)
        c->arg[i] = cx_copy(e->arg[i]);
    for (int i = 0; i < e->nsubs && i < MAXCALLARGS; i++)
        c->sub[i] = cx_copy(e->sub[i]);
    return c;
}

/* 0 with the attributes of t */
static struct expr *cx_zero(struct type t, int line)
{
    t.pic = NULL;
    return cast_of(iter_num("0", line), t);
}

static struct expr *cx_part(struct expr *e, int part);
static struct expr *cx_fn(struct expr *e, int part);

/* a real operand that can be taken twice, once for each part: a
 * constant, or a variable with constant or plain subscripts */
static int cx_simple(const struct expr *e)
{
    if (e->k == E_NEG)
        e = e->l;
    if (e->k == E_NUM)
        return 1;
    if (e->k != E_VAR || e->qual || (e->sym && e->sym->child))
        return 0;
    for (int i = 0; i < e->nsubs && i < MAXCALLARGS; i++) {
        const struct expr *x = e->sub[i];
        if (x->k == E_NEG)
            x = x->l;
        if (x->k != E_NUM && (x->k != E_VAR || x->nsubs || x->qual))
            return 0;
    }
    return 1;
}

/* is e a complex * / ** that a run-time call computes (cx_call) */
static int cx_is_call(const struct expr *e)
{
    if (e->k != E_BIN || (e->op != T_STAR && e->op != T_SLASH && e->op != T_POWER) ||
        !cx_expr((struct expr *)e))
        return 0;
    if (e->op == T_POWER)
        return 1;
    return !((!cx_expr(e->r) && cx_simple(e->r)) ||
             (e->op == T_STAR && !cx_expr(e->l) && cx_simple(e->l)));
}

/* x in the type t, a cast only if it changes something */
static struct expr *cx_to(struct expr *x, struct type t)
{
    if (x->t.k == t.k && x->t.p == t.p && x->t.q == t.q)
        return x;
    return cast_of(x, t);
}

/* an error of the complex operations, once a line: each part of a
 * value, and each copy, would report it again */
static void cx_error(int line, const char *fmt, const char *arg)
{
    static int last;
    if (line == last)
        return;
    last = line;
    error_at(line, fmt, arg);
}

/* the part of the complex temporary v, as a variable */
static struct expr *cx_read(struct sym *v, int part, int line)
{
    struct expr *c = new_node(E_VAR, line);
    c->sym = cx_sym(v, part);
    c->part = part;
    c->t = c->sym->t;
    return c;
}

/* A complex * / or ** that the run time computes (rt/pl1cx.h,
 * rt/pl1cxf.c): one B_CXOP call with the operands' parts as arguments,
 * whose value goes into a complex temporary.  cx_part asks for each
 * part of e once, the first time the call itself (the part read from
 * the temporary after it, e->l), the second time only a read: so the
 * operands are evaluated and the conditions raised once, however the
 * value is split.  gen.c emits the call where it meets it, and the
 * part asked for first is generated first, on every path to the other
 * (cx_stmt's assignments follow each other, cx_compare's & and | and
 * cx_put's arguments are generated left to right).  ** takes an integer exponent, a constant or a FIXED (p,0)
 * value, which keeps its REAL mode (GC28-8201-4, "Mode"). */
static struct expr *cx_call(struct expr *e, int part)
{
    struct expr *x[4], *n;
    struct type ta, tb, t;
    int nx = 4;
    if (e->cxt)
        return cx_read(e->cxt, part, e->line);
    x[0] = cx_part(e->l, 1);
    x[1] = cx_part(e->l, 2);
    type_of(x[0]);
    type_of(x[1]);
    ta = cx_real(x[0]->t);
    if (!arith(ta)) {
        cx_error(e->line, "a complex value takes arithmetic operands%s", "");
        return iter_num("0", e->line);
    }
    if (e->op == T_POWER) {
        struct expr *y = e->r;
        int con = y->k == E_NUM || (y->k == E_NEG && y->l->k == E_NUM);
        if (!cx_expr(y)) {
            y = cx_copy(y);
            type_of(y);
            tb = cx_real(y->t);
            if (!arith(tb)) {
                cx_error(e->line, "a complex value takes arithmetic operands%s", "");
                return iter_num("0", e->line);
            }
        }
        if (cx_expr(e->r) || is_float(tb) || tb.q != 0) {
            /* the general case, z1**z2 = EXP(z2*LOG(z1)) (rt/pl1cxx.c):
             * a complex exponent, or a real one not an integer */
            x[2] = cx_part(e->r, 1);
            x[3] = cx_part(e->r, 2);
            type_of(x[2]);
            type_of(x[3]);
            tb = cx_real(x[2]->t);
            if (!arith(tb)) {
                cx_error(e->line, "a complex value takes arithmetic operands%s", "");
                return iter_num("0", e->line);
            }
            t = float_common(float_of(ta), float_of(tb));
            x[2] = cx_to(x[2], t);
            x[3] = cx_to(x[3], t);
            goto made;
        }
        if (!is_float(ta) && con) {
            /* FIXED to a positive integer constant stays FIXED when the
             * precision allows (GC28-8201-4, "Exponentiation") */
            int n = 0, lim = ta.k == TY_FIXBIN ? 31 : 15;
            const struct expr *c = y->k == E_NEG ? y->l : y;
            for (const char *d = c->num; *d && n < 100; d++)
                n = n * 10 + (*d - '0');
            if (y->k != E_NEG && n > 0 && (ta.p + 1) * n - 1 <= lim) {
                /* z*z*...*z, the precision (p+1)*n-1 by the rules of
                 * the products; exact, so the order does not matter.
                 * The base is shared: a run-time value in it is
                 * computed once (cx_call), a variable read n times. */
                struct expr *b = e->l, *m = b;
                if (n == 1) {
                    *e = *b;
                    return cx_part(e, part);
                }
                for (int i = 2; i < n; i++) {
                    struct expr *k = new_node(E_BIN, e->line);
                    k->op = T_STAR;
                    k->l = m;
                    k->r = b;
                    m = k;
                }
                e->op = T_STAR;
                e->l = m;
                e->r = b;
                return cx_part(e, part);
            }
        }
        /* a FIXED base otherwise as FLOAT of its base and precision
         * (P**7, P**N, P**0 measured, JOB00075, JOB00079) */
        t = float_of(ta);
        memset(&tb, 0, sizeof tb);
        tb.k = TY_FIXBIN;
        tb.p = 31;
        x[2] = cx_to(y, tb);
        nx = 3;
    } else {
        x[2] = cx_part(e->r, 1);
        x[3] = cx_part(e->r, 2);
        type_of(x[2]);
        type_of(x[3]);
        tb = cx_real(x[2]->t);
        if (!arith(tb)) {
            cx_error(e->line, "a complex value takes arithmetic operands%s", "");
            return iter_num("0", e->line);
        }
        if (!is_float(ta) && !is_float(tb)) {
            /* FIXED: the operands in their common base, each with its own
             * precision and scale; the result's precision by the real
             * rules on the parts (rt/pl1cxf.c, JOB00077) */
            int lim;
            common(&ta, &tb);
            lim = ta.k == TY_FIXBIN ? 31 : 15;
            t = ta;
            if (e->op == T_STAR) {
                t.p = min(lim, ta.p + tb.p + 1);
                t.q = ta.q + tb.q;
            } else {
                t.p = lim;
                t.q = lim - ((ta.p - ta.q) + tb.q);
                if (t.q < 0)
                    cx_error(e->line, "the quotient would have a negative scale%s", "");
            }
            x[0] = cx_to(x[0], ta);
            x[1] = cx_to(x[1], ta);
            x[2] = cx_to(x[2], tb);
            x[3] = cx_to(x[3], tb);
        } else {
            t = float_common(ta, tb);
            x[2] = cx_to(x[2], t);
            x[3] = cx_to(x[3], t);
        }
    }
made:
    if (is_float(t)) {
        x[0] = cx_to(x[0], t);
        x[1] = cx_to(x[1], t);
    }
    n = new_node(E_CALL, e->line);
    n->fn = B_CXOP;
    n->op = e->op;
    n->nargs = nx;
    for (int i = 0; i < nx; i++)
        n->arg[i] = x[i];
    n->t = t;
    e->cxt = n->cxt = iter_temp(cplx_type(t));
    n->l = cx_read(n->cxt, part, e->line);
    return n;
}

/* SQRT, EXP, LOG, SIN, COS, SINH, COSH, TAN, TANH, ATAN, ATANH of a
 * complex value: one B_CXFN run-time call (rt/pl1cx.h, C3), as cx_call
 * does for the operators; FLOAT of the argument's base and precision
 * (GC28-8201-4, "Mathematical Built-in Functions"). */
static struct expr *cx_fn(struct expr *e, int part)
{
    struct expr *a, *b, *n;
    struct type t;
    if (e->cxt)
        return cx_read(e->cxt, part, e->line);
    if (e->fn == B_CXCVT) {
        /* a string: split and converted by the run time into a complex
         * temporary of the type e->t (rt/pl1cxsp.c) */
        n = new_node(E_CALL, e->line);
        n->fn = B_CXCVT;
        n->nargs = 1;
        n->arg[0] = e->arg[0];
        n->t = e->t;
        e->cxt = n->cxt = iter_temp(cplx_type(e->t));
        n->l = cx_read(n->cxt, part, e->line);
        return n;
    }
    a = cx_part(e->arg[0], 1);
    b = cx_part(e->arg[0], 2);
    type_of(a);
    type_of(b);
    t = cx_real(a->t);
    if (!arith(t)) {
        cx_error(e->line, "this function needs an arithmetic argument%s", "");
        return iter_num("0", e->line);
    }
    t = float_of(t);
    n = new_node(E_CALL, e->line);
    n->fn = B_CXFN;
    n->op = (int)e->fn;
    n->nargs = 2;
    n->arg[0] = cx_to(a, t);
    n->arg[1] = cx_to(b, t);
    n->t = t;
    e->cxt = n->cxt = iter_temp(cplx_type(t));
    n->l = cx_read(n->cxt, part, e->line);
    return n;
}

/* ABS of a complex value: B_CXABS, a run-time call with its two parts
 * (rt/pl1cxa.c, rt/pl1cxf.c), real. */
static void cx_abs(struct expr *e)
{
    struct expr *a = cx_part(e->arg[0], 1), *b = cx_part(e->arg[0], 2);
    struct type t;
    type_of(a);
    type_of(b);
    t = cx_real(a->t);
    if (!arith(t)) {
        error_at(e->line, "ABS needs an arithmetic argument");
        e->t = t;
        return;
    }
    e->fn = B_CXABS;
    e->nargs = 2;
    e->arg[0] = cx_to(a, t);
    e->arg[1] = cx_to(b, t);
    if (!is_float(t))
        /* (MIN(N,p+1),q) (GC28-8201-4, ABS); truncated (rt/pl1cxf.c) */
        t.p = min(t.k == TY_FIXBIN ? 31 : 15, t.p + 1);
    e->t = t;
}

/* the part (1 real, 2 imaginary) of e, a real expression not yet typed */
static struct expr *cx_part(struct expr *e, int part)
{
    struct expr *c;
    if (!cx_expr(e)) {
        /* a real operand: itself, and a zero of its attributes */
        if (part == 1)
            return cx_copy(e);
        c = cx_copy(e);
        type_of(c);
        if (!arith(c->t) && c->t.k != TY_PIC) {
            error_at(e->line, "a complex value takes arithmetic operands");
            return iter_num("0", e->line);
        }
        return cx_zero(cx_real(c->t), e->line);
    }
    switch (e->k) {
    case E_NUM:
        c = cx_copy(e);
        c->imag = 0;
        c->t = part_type(e->t);
        return part == 2 ? c : cx_zero(c->t, e->line);
    case E_VAR:
        c = cx_copy(e);
        c->part = part;
        c->sym = cx_sym(e->sym, part);
        return c;
    case E_NEG:
        c = new_node(E_NEG, e->line);
        c->l = cx_part(e->l, part);
        return c;
    case E_BIN:
        if (e->op == T_PLUS || e->op == T_MINUS)
            return iter_bin(e->op, cx_part(e->l, part), cx_part(e->r, part), e->line);
        if (e->op != T_POWER && !cx_expr(e->r) && cx_simple(e->r))
            /* by a real value: each part by it, as PL/I (F) does (two
             * ZERODIVIDEs for a real 0 divisor, JOB00079) */
            return iter_bin(e->op, cx_part(e->l, part), cx_copy(e->r), e->line);
        if (e->op == T_STAR && !cx_expr(e->l) && cx_simple(e->l))
            return iter_bin(e->op, cx_copy(e->l), cx_part(e->r, part), e->line);
        return cx_call(e, part);
    case E_CALL:
        if (e->fn == B_ADD || e->fn == B_MULTIPLY || e->fn == B_DIVIDE) {
            /* the operation (made once, in e->l), then each part
             * converted to the precision given, as for real values */
            struct expr *o = e->l, *r;
            struct type t;
            if (!o) {
                o = new_node(E_BIN, e->line);
                o->op = e->fn == B_ADD ? T_PLUS : e->fn == B_MULTIPLY ? T_STAR : T_SLASH;
                o->l = e->arg[0];
                o->r = e->arg[1];
                e->l = o;
            }
            r = cx_part(o, part);
            type_of(r);
            if (!arith(r->t) || !conv_type(e, r->t, B_PRECISION, 2, &t))
                return r;
            if (is_float(t) && flt_long(t) != flt_long(r->t))
                cx_error(e->line, "%s of complex values to another length is not supported "
                         "yet", e->fn == B_ADD ? "ADD" : e->fn == B_MULTIPLY ? "MULTIPLY" :
                         "DIVIDE");
            return cx_to(r, t);
        }
        if (cx_math(e->fn) || e->fn == B_CXCVT)
            return cx_fn(e, part);
        if (e->fn == B_CONJG) {
            if (part == 1)
                return cx_part(e->arg[0], 1);
            c = new_node(E_NEG, e->line);
            c->l = cx_part(e->arg[0], 2);
            return c;
        } else {
            /* COMPLEX(a,b): a and b in their common type */
            struct expr *a = cx_copy(e->arg[0]), *b = cx_copy(e->arg[1]);
            struct type ta, tb, t;
            type_of(a);
            type_of(b);
            ta = cx_real(a->t);
            tb = cx_real(b->t);
            if (cx_expr(e->arg[0]) || cx_expr(e->arg[1]) || !arith(ta) || !arith(tb)) {
                error_at(e->line, "COMPLEX takes two real arithmetic values");
                return iter_num("0", e->line);
            }
            if (is_float(ta) || is_float(tb)) {
                t = float_common(ta, tb);
            } else {
                int il, q;
                common(&ta, &tb);
                q = ta.q > tb.q ? ta.q : tb.q;
                il = ta.p - ta.q > tb.p - tb.q ? ta.p - ta.q : tb.p - tb.q;
                t = ta;
                t.q = q;
                t.p = il + q > (t.k == TY_FIXBIN ? 31 : 15) ? (t.k == TY_FIXBIN ? 31 : 15) :
                      il + q;
            }
            return cast_of(part == 1 ? a : b, t);
        }
    default:
        error_at(e->line, "a complex %s is not supported yet", e->k == E_FCALL ?
                 "function value" : "value here");
        return iter_num("0", e->line);
    }
}

/* = and ¬= of complex operands: the parts compared; another comparison
 * is an error, as in PL/I (F) (IEM1608I, t227 Q5CERR). 0 when e is
 * done with. */
static int cx_compare(struct expr *e)
{
    struct expr *l = e->l, *r = e->r;
    if (e->op != T_EQ && e->op != T_NE) {
        if (e->op == T_LT || e->op == T_LE || e->op == T_GT || e->op == T_GE ||
            e->op == T_NLT || e->op == T_NGT)
            error_at(e->line, "only = and ¬= compare complex values");
        else
            error_at(e->line, "a complex value is not supported here yet");
        e->t.k = TY_BIT;
        e->t.p = 1;
        return 0;
    }
    e->l = iter_bin(e->op, cx_part(l, 1), cx_part(r, 1), e->line);
    e->r = iter_bin(e->op, cx_part(l, 2), cx_part(r, 2), e->line);
    e->op = e->op == T_EQ ? T_AND : T_OR;
    return 1;
}

/* does e refer to the variable v */
static int cx_uses(const struct expr *e, const struct sym *v)
{
    if (!e)
        return 0;
    if (e->k == E_VAR && e->sym && e->sym->root == v->root)
        return 1;
    if (cx_uses(e->l, v) || cx_uses(e->r, v) || cx_uses(e->qual, v))
        return 1;
    for (int i = 0; i < e->nargs && i < MAXCALLARGS; i++)
        if (cx_uses(e->arg[i], v))
            return 1;
    for (int i = 0; i < e->nsubs && i < MAXCALLARGS; i++)
        if (cx_uses(e->sub[i], v))
            return 1;
    return 0;
}

/* a statement after s, the assignment of e to lhs */
static struct stmt *cx_assign_after(struct stmt *s, struct expr *lhs, struct expr *e)
{
    struct stmt *n = xmalloc(sizeof *n);
    memset(n, 0, sizeof *n);
    n->k = S_ASSIGN;
    n->line = s->line;
    n->lhs = lhs;
    n->e = e;
    n->next = s->next;
    s->next = n;
    return n;
}

/* An assignment s with a complex target or source, or a complex
 * pseudo-variable: made into assignments of real values, s the first
 * of them (the others follow it and are checked after it). */
static void cx_stmt(struct stmt *s)
{
    struct expr *lhs = s->lhs, *src = s->e, *a = NULL, *b = NULL;
    if (s->nmore)
        error_at(s->line, "a complex value in an assignment of several targets is not "
                 "supported yet");
    if (lhs->k == E_CALL && (lhs->fn == B_REAL || lhs->fn == B_IMAG)) {
        /* REAL(z) = e, IMAG(z) = e: that part of z takes e's real part */
        struct expr *z = lhs->arg[0];
        if (z->k != E_VAR || !is_cplx(z->sym->t) || z->sym->child) {
            if (z->k == E_VAR && lhs->fn == B_REAL && !z->sym->child && !is_cplx(z->sym->t))
                /* PL/I (F): IEM1625I, a warning, the target taken as it is */
                s->lhs = z;
            else
                error_at(s->line, "%s as a pseudo-variable takes a complex variable",
                         lhs->fn == B_REAL ? "REAL" : "IMAG");
        } else {
            s->lhs = cx_copy(z);
            s->lhs->part = lhs->fn == B_REAL ? 1 : 2;
            s->lhs->sym = cx_sym(z->sym, s->lhs->part);
        }
        if (cx_expr(src))
            s->e = cx_part(src, 1);
        return;
    }
    if (!cx_expr(src) && lhs->k == E_VAR && cx_expr(lhs)) {
        struct expr *c = cx_copy(src);
        type_of(c);
        if (is_string(c->t) && !is_bit(c->t)) {
            /* a string to a complex target: the run time splits it and
             * converts the parts, to the target's FLOAT or to FIXED
             * DECIMAL(15,q) as for a real target (Q12CCV2, JOB00248) */
            struct type pt = cx_real(part_type(lhs->sym->t)), ct;
            struct expr *v = new_node(E_CALL, s->line);
            memset(&ct, 0, sizeof ct);
            if (is_float(pt)) {
                ct = pt;
            } else {
                ct.k = TY_FIXDEC;
                ct.p = 15;
                ct.q = dec_scale(pt);
            }
            type_of(src);
            v->fn = B_CXCVT;
            v->nargs = 1;
            v->arg[0] = src;
            v->t = ct;
            s->e = src = v;
        }
    }
    if (lhs->k == E_CALL && lhs->fn == B_COMPLEX) {
        a = lhs->arg[0];
        b = lhs->arg[1];
        if (a->k != E_VAR || b->k != E_VAR || is_cplx(a->sym->t) || is_cplx(b->sym->t))
            error_at(s->line, "COMPLEX as a pseudo-variable takes two real variables");
    } else if (lhs->k == E_VAR && cx_expr(lhs)) {
        a = cx_copy(lhs);
        a->part = 1;
        a->sym = cx_sym(lhs->sym, 1);
        b = cx_copy(lhs);
        b->part = 2;
        b->sym = cx_sym(lhs->sym, 2);
    } else {
        /* a real target: the real part; a character string: the value as
         * PUT LIST writes it (t227 Q3CMODE) */
        struct expr *t = cx_copy(lhs);
        type_of(t);
        if (is_string(t->t) && !is_bit(t->t)) {
            struct expr *c = new_node(E_CALL, s->line);
            c->fn = B_CPXSTR;
            c->nargs = 2;
            c->arg[0] = cx_part(src, 1);
            c->arg[1] = cx_part(src, 2);
            s->e = c;
        } else if (arith(t->t) || t->t.k == TY_PIC) {
            s->e = cx_part(src, 1);
        } else {
            error_at(s->line, "a complex value cannot be assigned to this target yet");
        }
        return;
    }
    if ((cx_uses(src, a->sym) || cx_uses(src, b->sym)) && !cx_is_call(src)) {
        /* the target in its own source, Z = COMPLEX(IMAG(Z), REAL(Z)): the
         * imaginary part into a temporary first */
        struct expr *i = cx_part(src, 2);
        struct sym *v;
        type_of(i);
        v = iter_temp(cx_real(i->t));
        s->lhs = iter_var(v, s->line);
        s->e = i;
        cx_assign_after(cx_assign_after(s, a, cx_part(src, 1)), b, iter_var(v, s->line));
        return;
    }
    s->lhs = a;
    s->e = cx_part(src, 1);
    cx_assign_after(s, b, cx_part(src, 2));
}

/* a complex PUT item: the value as one character string, written as a
 * number is (B_CPXSTR, rt/pl1cx.c) */
static struct expr *cx_put(struct expr *e)
{
    struct expr *c = new_node(E_CALL, e->line);
    c->fn = B_CPXSTR;
    c->nargs = 2;
    c->arg[0] = cx_part(e, 1);
    c->arg[1] = cx_part(e, 2);
    c->arg[2] = e;                      /* for PUT DATA's name (gen.c data_name) */
    return c;
}

/* REAL, IMAG, CONJG, COMPLEX met in an expression: REAL and IMAG a
 * part of a complex value; of a real one, REAL the value and IMAG a
 * zero (PL/I (F) takes both as functions without a message, t227
 * Q5CERR); CONJG of a real value is the value */
static void type_cplxfn(struct expr *e)
{
    struct expr *x = e->arg[0], *r;
    if (e->fn == B_REAL || e->fn == B_IMAG) {
        if (cx_expr(x)) {
            r = cx_part(x, e->fn == B_REAL ? 1 : 2);
        } else if (e->fn == B_REAL) {
            r = x;
        } else {
            struct expr *c = cx_copy(x);
            type_of(c);
            r = cx_zero(cx_real(c->t), e->line);
        }
    } else if (e->fn == B_CONJG && !cx_expr(x)) {
        r = x;
    } else {
        error_at(e->line, "a complex value is not supported here yet");
        memset(&e->t, 0, sizeof e->t);
        e->t.k = TY_FIXDEC;
        e->t.p = 1;
        return;
    }
    *e = *r;
    type_of(e);
}

/* A complex INITIAL value: a real constant, an imaginary one, or a real
 * one and an imaginary one with + or - between them, each with a sign
 * or none (GC28-8201-4, "INITIAL").  Its parts, each a constant or a
 * negative one, a zero for one not written (gen.c static_inits; the
 * AUTOMATIC ones are assignments, lower.c). */
int cplx_init(struct expr *v, struct expr **re, struct expr **im)
{
    struct expr *r = NULL, *i = NULL;
    const struct expr *n;
    if (v->k == E_BIN && (v->op == T_PLUS || v->op == T_MINUS)) {
        r = v->l;
        i = v->r;
        n = r->k == E_NEG ? r->l : r;
        if (n->k != E_NUM || n->imag || i->k != E_NUM || !i->imag)
            return 0;
        i = cx_copy(i);
        i->imag = 0;
        i->t = part_type(i->t);
        if (v->op == T_MINUS) {
            struct expr *m = new_node(E_NEG, v->line);
            m->l = i;
            i = m;
        }
    } else {
        n = v->k == E_NEG ? v->l : v;
        if (n->k != E_NUM)
            return 0;
        if (n->imag) {
            struct expr *c = cx_copy(n);
            c->imag = 0;
            c->t = part_type(c->t);
            if (v->k == E_NEG) {
                i = new_node(E_NEG, v->line);
                i->l = c;
            } else {
                i = c;
            }
        } else {
            r = v;
        }
    }
    *re = r ? r : iter_num("0", v->line);
    *im = i ? i : iter_num("0", v->line);
    return 1;
}

/* B_CPXSTR: each part as a string (gen_string, the PUT LIST field),
 * joined: real||signed imaginary||'I' */
static void type_cpxstr(struct expr *e)
{
    int n = 2;                          /* a sign the imaginary part lacks, and I */
    for (int i = 0; i < 2; i++) {
        type_of(e->arg[i]);
        n += string_len(cx_real(e->arg[i]->t));
    }
    memset(&e->t, 0, sizeof e->t);
    e->t.k = TY_VCHAR;
    e->t.p = n;
}

/* An iterative DO: the simple form -- one specification with TO, a
 * FIXED BINARY integer control variable, BY an integer constant or none,
 * no WHILE -- goes the old way (s->e, s->e2, s->e3); every other one is
 * the general form, gen.c following the expansion of GC28-8201-4,
 * section J (Q1DO, JOB00445). */
static void prepare_iter(struct stmt *s)
{
    struct dospec *d = s->specs;
    struct type ct = s->target->t;
    struct expr *b = d->e3 && d->e3->k == E_NEG ? d->e3->l : d->e3;
    int line = s->line;
    if (!d->next && !d->w && !d->u && !d->rep && d->e2 && ct.k == TY_FIXBIN && ct.q == 0 &&
        (!b || (b->k == E_NUM && b->t.k == TY_FIXDEC && b->t.q == 0))) {
        s->e = d->e1;
        s->e2 = d->e2;
        s->e3 = d->e3;
        return;
    }
    s->general = 1;
    if (!arith(ct) && ct.k != TY_PIC) {
        /* a locator steps by REPEAT, or takes single values; TO and BY
         * need arithmetic (GC33-0009-4, DO statement, example 4) */
        int ok = ct.k == TY_PTR || ct.k == TY_OFFSET;
        for (struct dospec *x = d; x; x = x->next)
            if (x->e2 || x->e3)
                ok = 0;
        if (!ok) {
            error_at(line, "the control variable must be arithmetic");
            return;
        }
    }
    if (ct.k == TY_PIC)
        error_at(line, "a picture as control variable is not supported");
    for (; d; d = d->next) {
        check_assign(ct, &d->e1, line);
        if (d->e2) {
            type_of(d->e2);
            if (!arith(d->e2->t) && d->e2->t.k != TY_PIC)
                error_at(line, "TO needs an arithmetic value");
            d->v2 = iter_temp(d->e2->t);
            check_assign(d->v2->t, &d->e2, line);
            d->gt = iter_bin(T_GT, iter_var(s->target, line), iter_var(d->v2, line), line);
            d->lt = iter_bin(T_LT, iter_var(s->target, line), iter_var(d->v2, line), line);
        }
        if (d->e3) {
            type_of(d->e3);
            if (!arith(d->e3->t) && d->e3->t.k != TY_PIC)
                error_at(line, "BY needs an arithmetic value");
            d->v3 = iter_temp(d->e3->t);
            check_assign(d->v3->t, &d->e3, line);
            d->neg = iter_bin(T_LT, iter_var(d->v3, line), iter_num("0", line), line);
        }
        if (d->e2 || d->e3) {
            d->incr = iter_bin(T_PLUS, iter_var(s->target, line),
                               d->e3 ? iter_var(d->v3, line) : iter_num("1", line), line);
            check_assign(ct, &d->incr, line);
        }
        if (d->w)
            check_cond(d->w, line);
        if (d->u)
            check_cond(d->u, line);
        if (d->rep)
            check_assign(ct, &d->rep, line);
    }
}

static void check_stmts(struct stmt *s)
{
    for (; s; s = s->next) {
        switch (s->k) {
        case S_NULL:
            break;
        case S_ADJUST:
            for (int d = 0; d < s->target->ndims; d++)
                for (int h = 0; h < 2; h++) {
                    struct expr **x = h ? &s->target->hbx[d] : &s->target->lbx[d];
                    type_of(*x);
                    if (!arith((*x)->t))
                        error_at(s->line, "a bound of %s must be arithmetic", s->target->name);
                }
            break;
        case S_CHECK:
            check_stmts(s->body);
            break;
        case S_ASSIGN:
            if (cx_expr(s->lhs) || cx_expr(s->e) ||
                (s->lhs->k == E_CALL && (s->lhs->fn == B_REAL || s->lhs->fn == B_IMAG ||
                                         s->lhs->fn == B_COMPLEX)))
                cx_stmt(s);           /* complex: assignments of the parts */
            type_of(s->lhs);
            if (s->lhs->k == E_CALL && s->lhs->fn == B_SUBSTR) {
                /* SUBSTR as a pseudovariable: that part of the string */
                struct type tt = s->lhs->arg[0]->t;
                if (tt.k == TY_PIC || tt.pic)
                    error_at(s->line, "SUBSTR of a picture as a target is not supported");
                if (s->lhs->arg[0]->k == E_VAR && s->lhs->arg[0]->sym->packed)
                    error_at(s->line, "SUBSTR of an unaligned bit string at a bit offset as a "
                             "target is not supported yet");
                if (is_bit(tt)) {
                    type_of(s->e);
                    if (!is_bit(s->e->t))
                        error_at(s->line, "SUBSTR of a bit string takes a bit string");
                } else {
                    check_assign(s->lhs->t, &s->e, s->line);
                }
                break;
            }
            if (s->lhs->k == E_CALL && (s->lhs->fn == B_STRING || s->lhs->fn == B_UNSPEC)) {
                /* STRING(x) = ..., UNSPEC(x) = ...: x's storage takes the
                 * string, padded or cut (Q1STR, Q2UNS) */
                struct expr *x = s->lhs->arg[0];
                type_of(s->e);
                if (s->lhs->fn == B_UNSPEC) {
                    if (x->k != E_VAR || x->sym->child || x->nsubs != cum_dims(x->sym) ||
                        x->sym->packed || x->t.k == TY_VCHAR || x->t.k == TY_VBIT)
                        error_at(s->line, "UNSPEC as a target takes a scalar variable, not "
                                 "VARYING (an aggregate: not supported yet)");
                    if (!is_bit(s->e->t))
                        s->e = conv_bit(s->e);
                } else if (is_bit(s->lhs->t)) {
                    if (!is_bit(s->e->t))
                        s->e = conv_bit(s->e);
                } else {
                    check_assign(s->lhs->t, &s->e, s->line);
                }
                break;
            }
            if (s->lhs->k == E_CALL && s->lhs->fn == B_ENTRYADDR) {
                struct type it;
                if (s->lhs->arg[0]->k != E_VAR)
                    error_at(s->line, "ENTRYADDR as a pseudovariable takes an ENTRY variable");
                memset(&it, 0, sizeof it);
                it.k = TY_PTR;
                it.p = 4;
                check_assign(it, &s->e, s->line);
                break;
            }
            if (s->lhs->k == E_CALL && (s->lhs->fn == B_COMPLETION || s->lhs->fn == B_STATUS)) {
                /* COMPLETION(e) = bit, STATUS(e) = number (#3) */
                struct type it;
                memset(&it, 0, sizeof it);
                it.k = s->lhs->fn == B_STATUS ? TY_FIXBIN : TY_BIT;
                it.p = s->lhs->fn == B_STATUS ? 31 : 1;
                check_assign(it, &s->e, s->line);
                break;
            }
            if (s->lhs->k == E_CALL && s->lhs->fn == B_PAGENO) {
                struct type it;
                memset(&it, 0, sizeof it);
                it.k = TY_FIXBIN;
                it.p = 31;
                check_assign(it, &s->e, s->line);
                break;
            }
            if (s->lhs->k == E_CALL) {
                /* ONCHAR or ONSOURCE as a pseudovariable */
                type_of(s->e);
                if (!is_string(s->e->t))
                    error_at(s->line, "%s takes a character string", s->lhs->fn == B_ONCHAR ?
                             "ONCHAR" : "ONSOURCE");
                break;
            }
            check_assign(s->lhs->t, &s->e, s->line);
            break;
        case S_IF:
            check_cond(s->e, s->line);
            if (s->body) check_stmts(s->body);
            if (s->els) check_stmts(s->els);
            break;
        case S_GROUP:
            check_stmts(s->body);
            break;
        case S_WHILE:
            if (s->e)
                check_cond(s->e, s->line);
            if (s->e2)
                check_cond(s->e2, s->line);       /* UNTIL */
            check_stmts(s->body);
            break;
        case S_SELECT:
            /* SELECT(e): e once into a temporary, each WHEN value compared
             * with it as = compares; SELECT: each WHEN value a condition */
            if (s->e) {
                type_of(s->e);
                s->target = iter_temp(s->e->t);
                check_assign(s->target->t, &s->e, s->line);
            }
            for (struct stmt *w = s->body; w; w = w->next) {
                for (struct put_item *it = w->items; it; it = it->next)
                    if (s->e)
                        it->in = iter_bin(T_EQ, iter_var(s->target, s->line), it->e, s->line);
                    else
                        check_cond(it->e, s->line);
                check_stmts(w->body);
            }
            if (s->els)
                check_stmts(s->els);
            break;
        case S_WHEN:
        case S_LEAVE:
        case S_FETCH:
        case S_RELEASE:
            break;
        case S_ITER:
            if (s->specs)
                prepare_iter(s);
            if (s->general) {
                check_stmts(s->body);
                break;
            }
            if (s->target->t.k != TY_FIXBIN || s->target->t.q != 0)
                error_at(s->line, "the control variable must be FIXED BINARY");
            check_assign(s->target->t, &s->e, s->line);
            type_of(s->e2);
            if (!arith(s->e2->t))
                error_at(s->line, "TO needs an arithmetic value");
            if (s->e3) {
                struct expr *b = s->e3->k == E_NEG ? s->e3->l : s->e3;
                if (b->k != E_NUM || b->t.q != 0)
                    error_at(s->line, "BY takes an integer constant");
                type_of(s->e3);
            }
            check_stmts(s->body);
            break;
        case S_PUT:
        case S_GET:
            if (s->string) {
                type_of(s->string);
                if (!is_string(s->string->t))
                    error_at(s->line, "STRING needs a character string");
                else if (s->k == S_PUT && (s->string->k != E_VAR || s->string->sym->child))
                    error_at(s->line, "PUT STRING needs a character variable");
                if (s->skip >= 0 || s->page)
                    error_at(s->line, "STRING takes no SKIP, PAGE or LINE");
                if (s->edit)
                    for (int i = 0; i < s->nfmts; i++)
                        if (s->fmts[i].k == F_SKIP || s->fmts[i].k == F_PAGE ||
                            s->fmts[i].k == F_LINE || s->fmts[i].k == F_COL)
                            error_at(s->line, "SKIP, PAGE, LINE and COLUMN are not supported "
                                     "with STRING");
            } else {
                check_file(s->file, s->line, s->k == S_PUT ? FL_OUTPUT : FL_INPUT);
            }
            if (s->copyfile)
                check_file(s->copyfile, s->line, FL_OUTPUT);
            if (s->k == S_GET && s->data)
                for (struct put_item *it = s->items; it; it = it->next)
                    check_get_data(it->e, s->line);
            if (s->edit) {
                int data = 0;
                for (int i = 0; i < s->nfmts; i++)
                    if (s->fmts[i].k == F_A || s->fmts[i].k == F_AW || s->fmts[i].k == F_F ||
                        s->fmts[i].k == F_E || s->fmts[i].k == F_P || s->fmts[i].k == F_B ||
                        s->fmts[i].k == F_BW)
                        data = 1;
                if (!data && s->body)
                    error_at(s->line, "the format list has no data format item");
                if (s->k == S_GET)
                    for (int i = 0; i < s->nfmts; i++)
                        if (s->fmts[i].k == F_A || s->fmts[i].k == F_B)
                            error_at(s->line, "GET EDIT needs A(w) and B(w), with a width");

            }
            check_stmts(s->body);
            break;
        case S_ITEM:
            if (s->get && s->lhs->k == E_VAR && cx_expr(s->lhs)) {
                /* the real part, then the imaginary part as a target after
                 * it, which the run time reads from one complex constant
                 * or a C item's two fields (rt/pl1ioget.c pl1_get_cpx) */
                struct stmt *n = xmalloc(sizeof *n);
                struct expr *x = s->lhs;
                *n = *s;
                n->nlabels = 0;
                n->cxpair = 2;
                s->cxpair = 1;
                s->lhs = cx_part(x, 1);
                n->lhs = cx_part(x, 2);
                s->next = n;
            }
            if (!s->get && cx_expr(s->e)) {
                if (s->owner_io && s->owner_io->edit) {
                    /* PUT EDIT: the real part, then the imaginary part as an
                     * item after it, which the run time edits with a C item's
                     * two items (rt/pl1ioed.c pl1_edit_cpx) */
                    struct stmt *n = xmalloc(sizeof *n);
                    struct expr *x = s->e;
                    *n = *s;
                    n->nlabels = 0;
                    n->cxpair = 2;
                    s->cxpair = 1;
                    s->e = cx_part(x, 1);
                    n->e = cx_part(x, 2);
                    s->next = n;
                } else {
                    s->e = cx_put(s->e);
                }
            }
            if (s->get) {
                type_of(s->lhs);
                if (!arith(s->lhs->t) && !is_string(s->lhs->t) && !is_bit(s->lhs->t) &&
                    s->lhs->t.k != TY_PIC)
                    error_at(s->line, "GET into this variable is not supported");
                if (s->lhs->t.k == TY_BIT && s->lhs->t.p > 32)
                    error_at(s->line, "GET into a bit string of more than 32 bits is not "
                             "supported yet");
                if (s->lhs->t.k == TY_PTR)
                    error_at(s->line, "a pointer cannot be read");
                if (s->lhs->t.k == TY_OFFSET || s->lhs->t.k == TY_AREA)
                    error_at(s->line, "an offset or an area cannot be read by GET");
            } else {
                type_of(s->e);
                if (s->e->t.k == TY_PTR)
                    error_at(s->line, "a pointer cannot be written");
                if (s->e->t.k == TY_OFFSET || s->e->t.k == TY_AREA)
                    error_at(s->line, "an offset or an area cannot be written by PUT");
                if (s->e->t.k == TY_EVENT)
                    error_at(s->line, "an event cannot be written by PUT");
            }
            break;
        case S_OPEN:
        case S_CLOSE:
            for (int i = 0; i < s->nfiles; i++) {
                check_file(s->files[i].file, s->line, 0);
                for (int k = -2; k < s->files[i].ntabs; k++) {
                    struct expr *x = k == -2 ? s->files[i].linesize : k == -1 ?
                                     s->files[i].pagesize : s->files[i].tabs[k];
                    if (!x)
                        continue;
                    type_of(x);
                    if (!arith(x->t))
                        error_at(s->line, "LINESIZE, PAGESIZE and TAB take numbers");
                }
            }
            break;
        case S_BEGIN:
        case S_STOP:
            break;
        case S_WAIT:
            /* the events, elements or whole arrays; the count a number */
            for (int i = 0; i < s->nmore; i++) {
                struct expr *x = s->more[i];
                if (x->k != E_VAR || x->sym->child || x->sym->t.k != TY_EVENT) {
                    error_at(s->line, "WAIT takes event variables");
                    continue;
                }
                for (int k = 0; k < x->nsubs; k++)
                    type_of(x->sub[k]);
                if (x->nsubs != 0 && x->nsubs != cum_dims(x->sym))
                    error_at(s->line, "WAIT takes an event or an array of them");
                else if (x->nsubs == 0 && cum_dims(x->sym) != x->sym->ndims)
                    error_at(s->line, "WAIT on an array of events in a structure array is not "
                             "supported yet");
                x->t = x->sym->t;
            }
            if (s->e) {
                struct type it;
                memset(&it, 0, sizeof it);
                it.k = TY_FIXBIN;
                it.p = 31;
                check_assign(it, &s->e, s->line);
            }
            break;
        case S_DELAY: {
            struct type it;
            memset(&it, 0, sizeof it);
            it.k = TY_FIXBIN;
            it.p = 31;
            check_assign(it, &s->e, s->line);
            break;
        }
        case S_DISPLAY:
            /* the message an element, converted to a character string;
             * the reply a character variable (IEM1826I, GC28-8201-4) */
            type_of(s->e);
            if ((s->e->k == E_VAR && (s->e->sym->child ||
                                      cum_dims(s->e->sym) > s->e->nsubs)) ||
                !(is_string(s->e->t) || is_bit(s->e->t) || arith(s->e->t)))
                error_at(s->line, "DISPLAY needs an element string or arithmetic value");
            if (s->e2) {
                type_of(s->e2);
                if (s->e2->k != E_VAR || s->e2->sym->child ||
                    cum_dims(s->e2->sym) > s->e2->nsubs ||
                    (s->e2->t.k != TY_CHAR && s->e2->t.k != TY_VCHAR))
                    error_at(s->line, "REPLY needs an element character variable");
            }
            if (s->e3) {
                /* EVENT: the WTOR is answered while the program goes on;
                 * a WAIT on the event waits for the reply (#3) */
                type_of(s->e3);
                if (s->e3->k != E_VAR || s->e3->t.k != TY_EVENT ||
                    cum_dims(s->e3->sym) > s->e3->nsubs)
                    error_at(s->line, "EVENT needs an element event variable");
            }
            break;
        case S_ALLOC:
        case S_FREE:
            for (struct put_item *it = s->items; it; it = it->next) {
                struct expr *x = it->e;
                if (it->in) {
                    type_of(it->in);
                    if (it->in->k != E_VAR || it->in->t.k != TY_AREA)
                        error_at(s->line, "IN names an area variable");
                    else if (it->in->sym->root->st == ST_BASED)
                        check_based(it->in);
                    if (x->k == E_VAR && x->sym->st == ST_CTL)
                        error_at(s->line, "a CONTROLLED variable is not allocated IN an area");
                }
                if (x->k == E_VAR && x->sym->root == x->sym && x->sym->st == ST_CTL &&
                    !x->nsubs && !x->qual)
                    continue;               /* CONTROLLED: a generation */
                if (x->k != E_VAR || x->sym->root != x->sym || x->sym->st != ST_BASED ||
                    x->nsubs) {
                    error_at(s->line, "%s takes a level-1 BASED or CONTROLLED variable",
                             s->k == S_ALLOC ? "ALLOCATE" : "FREE");
                    continue;
                }
                check_based(x);
                if (s->k == S_ALLOC && !x->qual && (!x->sym->based || x->sym->based->k != E_VAR))
                    error_at(s->line, "ALLOCATE %s needs SET(p)", x->sym->name);
            }
            break;
        case S_ON:
            if (s->cond == C_STRINGSIZE)
                uses_strz = 1;
            /* fall through */
        case S_SIGNAL:
        case S_REVERT:
            if (FILE_COND(s->cond))
                check_file(s->file, s->line, 0);
            break;
        case S_FORMAT:
            break;                        /* expanded where R names it */
        case S_LOCATE:
            check_file(s->file, s->line, FL_OUTPUT);
            if (s->lhs->k != E_VAR || s->lhs->nsubs || s->lhs->sym->root != s->lhs->sym ||
                s->lhs->sym->st != ST_BASED)
                error_at(s->line, "LOCATE takes a level-1 BASED variable");
            else if (!s->e3 && (!s->lhs->sym->based || s->lhs->sym->based->k != E_VAR))
                error_at(s->line, "LOCATE %s needs SET(p)", s->lhs->sym->name);
            if (s->e3) {
                type_of(s->e3);
                if (s->e3->k != E_VAR || s->e3->t.k != TY_PTR)
                    error_at(s->line, "SET takes a pointer variable");
            }
            if (s->e) {
                int fa = s->file && s->file->sym ? s->file->sym->is_file : 0;
                if (fa & FL_REGIONAL)
                    error_at(s->line, "LOCATE KEYFROM is for INDEXED files");
                check_keys(s);
            }
            break;
        case S_READ:
        case S_WRITE:
        case S_REWRITE:
            if (s->nmore) {
                /* EVENT(e) (#3) */
                struct expr *x = s->more[0];
                type_of(x);
                if (x->k != E_VAR || x->t.k != TY_EVENT || cum_dims(x->sym) > x->nsubs)
                    error_at(s->line, "EVENT needs an element event variable");
            }
            if (!s->lhs) {
                /* READ ... SET(p); REWRITE FILE(f) after it */
                check_file(s->file, s->line, s->k == S_READ ? FL_INPUT : 0);
                if (s->e3) {
                    type_of(s->e3);
                    if (s->e3->k != E_VAR || s->e3->t.k != TY_PTR)
                        error_at(s->line, "SET takes a pointer variable");
                }
                if (s->ignore) {
                    type_of(s->ignore);
                    if (!arith(s->ignore->t))
                        error_at(s->line, "IGNORE takes a number of records");
                }
                if (s->e || s->e2)
                    check_keys(s);
                break;
            }
            check_file(s->file, s->line, s->k == S_READ ? FL_INPUT :
                       s->k == S_WRITE ? FL_OUTPUT : 0);
            if (s->k == S_REWRITE && s->file && s->file->sym && s->file->sym->is_file &&
                (s->file->sym->is_file & (FL_INPUT | FL_OUTPUT)))
                error_at(s->line, "REWRITE needs an UPDATE file");
            if (s->file && s->file->sym && s->file->sym->is_file &&
                !(s->file->sym->is_file & FL_RECORD))
                error_at(s->line, "%s needs a RECORD file", s->k == S_READ ? "READ" : "WRITE");
            if (s->lhs->k != E_VAR)
                error_at(s->line, "INTO and FROM take a variable");
            else if (s->lhs->sym->level > 1)
                /* PL/I (F): IEM0699I, JOB00174 */
                error_at(s->line, "INTO and FROM take a level-1 variable, not %s",
                         s->lhs->sym->name);
            else if (s->lhs->sym->child && has_varying(s->lhs->sym))
                /* PL/I (F) refuses it too: IEM1090I (JOB00121) */
                error_at(s->line, "structure %s contains VARYING strings and cannot be "
                         "used in record I/O", s->lhs->sym->name);
            else if (!s->lhs->sym->child && s->lhs->nsubs == cum_dims(s->lhs->sym))
                type_of(s->lhs);
            else if (s->lhs->nsubs != 0 && s->lhs->nsubs != cum_dims(s->lhs->sym))
                error_at(s->line, "INTO and FROM take all subscripts or none");
            else if (s->lhs->nsubs == 0 && cum_dims(s->lhs->sym) != s->lhs->sym->ndims)
                error_at(s->line, "INTO and FROM of an array in a structure array are not "
                         "supported yet");
            check_keys(s);
            break;
        case S_DELETE:
            check_file(s->file, s->line, 0);
            check_keys(s);
            break;
        case S_CALL:
            if (s->e->k == E_CALL)
                type_of(s->e);          /* a built-in subroutine: PLIRETC */
            else
                type_fcall(s->e, 0);
            break;
        case S_RETURN:
            if (s->e && !CUR->has_ret)
                error_at(s->line, "RETURN with a value in %s, which has no RETURNS", CUR->name);
            else if (!s->e && CUR->ret_all)
                error_at(s->line, "RETURN in %s needs a value", CUR->name);
            else if (s->e && CUR->ret_multi) {
                /* the value for each entry's attributes (gen.c); one of
                 * another kind, a bit string for an arithmetic entry,
                 * say, is not converted: that entry's value is left
                 * unset, as the RETURN is not meant for it */
                type_of(s->e);
                s->rets = xmalloc((size_t)(CUR->nentries + 1) * sizeof *s->rets);
                for (int k = 0; k <= CUR->nentries; k++) {
                    struct type rt = k ? CUR->entries[k - 1].ret : CUR->ret;
                    s->rets[k] = NULL;
                    if (k ? !CUR->entries[k - 1].has_ret : !CUR->ret_main)
                        continue;
                    if (arith(rt) != arith(s->e->t) || is_bit(rt) != is_bit(s->e->t))
                        continue;
                    s->rets[k] = s->e;    /* check_assign wraps, never changes it */
                    check_assign(rt, &s->rets[k], s->line);
                }
            } else if (s->e)
                check_assign(CUR->ret, &s->e, s->line);
            break;
        case S_ENTRY:
            break;
        case S_GOTO:
            if (s->e) {
                type_of(s->e);
                if (s->e->t.k != TY_LABEL)
                    error_at(s->line, "GO TO needs a label or a LABEL variable");
            }
            break;
        }
    }
}

static void check_proc(struct proc *p)
{
    CUR = p;
    p->ret_main = p->has_ret;
    p->ret_all = p->has_ret;
    for (int i = 0; i < p->nentries; i++) {
        struct pentry *pe = &p->entries[i];
        if (!pe->has_ret) {
            p->ret_all = 0;
            continue;
        }
        if (!p->has_ret) {
            /* a procedure without RETURNS whose entries return values
             * (SETSUC: FAIL and SUC, Q1SUC) */
            p->has_ret = 1;
            p->ret = pe->ret;
        } else if (!type_eq(pe->ret, p->ret)) {
            /* RETURN converts to the attributes of the entry point
             * entered (Q1SUC: COUNT) */
            p->ret_multi = 1;
        }
    }
    for (struct sym *s = p->syms; s; s = s->next) {
        /* ANSI's PARAMETER, STRUCTURE and MEMBER must say what is so
         * (4.3.3.1, Step 3) */
        if (s->ansi_parm && !s->param)
            error_at(s->line, "%s: PARAMETER, but not a parameter", s->name);
        if (s->ansi_struct && !s->child)
            error_at(s->line, "%s: STRUCTURE, but no members", s->name);
        if (s->ansi_member && !s->parent)
            error_at(s->line, "%s: MEMBER, but not in a structure", s->name);
        for (int i = 0; i < s->ninits || (i == 0 && s->init && !s->ninits); i++) {
            struct expr *v = s->ninits ? s->inits[i].e : s->init;
            int stat = s->root->st == ST_STATIC;
            if (s->ninits && s->inits[i].repx) {
                type_of(s->inits[i].repx);
                if (!arith(s->inits[i].repx->t))
                    error_at(s->line, "an iteration factor of %s must be arithmetic", s->name);
                if (stat)
                    error_at(s->line, "%s: STATIC INITIAL takes constant iteration factors",
                             s->name);
            }
            if (!v)
                continue;                   /* '*' */
            if (is_cplx(s->t) || cx_expr(v)) {
                struct expr *re, *im;
                if (!is_cplx(s->t))
                    error_at(s->line, "INITIAL value of %s does not match its type", s->name);
                else if (!cplx_init(v, &re, &im))
                    error_at(s->line, "%s: a complex INITIAL takes constants such as 1.5-2I",
                             s->name);
                continue;
            }
            if (stat && v->k == E_CALL && (s->t.k == TY_PTR || s->t.k == TY_OFFSET) &&
                (v->fn == B_NULL || v->fn == B_NULLO || v->fn == B_SYSNULL ||
                 (v->fn == B_PTRVALUE && v->nargs == 1 &&
                  (v->arg[0]->k == E_NUM ||
                   (v->arg[0]->k == E_NEG && v->arg[0]->l->k == E_NUM))))) {
                /* a STATIC locator: NULL, NULLO (the Optimizer's, GC33-0009-4
                 * INITIAL rule 7; PL/I (F) refuses them, IEM0512I, Q1SNUL),
                 * SYSNULL, POINTERVALUE(constant) (SC26-3114-01); an
                 * offset's INITIAL is refused below all the same */
                type_of(v);
                continue;
            }
            if (stat && v->k != E_NUM && v->k != E_STR && v->k != E_BITC &&
                !(v->k == E_NEG && v->l->k == E_NUM)) {
                /* ANSI allows constant expressions there; PL/I (F) only
                 * constants (IEM0139I, Q3INIS) */
                error_at(s->line, "%s: STATIC INITIAL takes constants", s->name);
                continue;
            }
            type_of(v);
            /* a numeric picture takes an arithmetic value */
            if (((arith(s->t) || s->t.k == TY_PIC) != arith(v->t)) ||
                (is_bit(s->t) != (v->t.k == TY_BIT)))
                error_at(s->line, "INITIAL value of %s does not match its type", s->name);
        }
        if (!s->child && cum_dims(s) == 0 && s->ninits > 0 &&
            (s->ninits > 1 || s->inits[0].rep > 1))
            /* PL/I (F): IEM0578I, an error, the INITIAL ignored (Q4FIN) */
            error_at(s->line, "%s: INITIAL of more than one value for a variable that is "
                     "not an array", s->name);
        if ((s->init || s->ninits) && s->child)
            error_at(s->line, "%s: a structure takes INITIAL on its members", s->name);
        if (s->t.k == TY_OFFSET) {
            /* OFFSET(a): a level-1 BASED area (PL/I (F): IEM0606I) */
            struct expr *a = s->t.area;
            if (a->k != E_VAR || a->nsubs || a->qual || a->sym->root != a->sym ||
                a->sym->st != ST_BASED || a->sym->t.k != TY_AREA)
                error_at(s->line, "OFFSET of %s names an unsubscripted level-1 BASED area",
                         s->name);
            else
                type_of(a);
        }
        if (s->t.k == TY_AREA && s->star && !s->param)
            error_at(s->line, "%s: AREA(*) is for parameters", s->name);
        if ((s->packed || (s->root->child && s->root->tbits)) && (s->defined || s->root->defined))
            error_at(s->line, "%s: DEFINED with packed bit strings is not supported yet",
                     s->name);
        if (s->adjust && (s->root != s || s->st != ST_AUTO || s->param || s->child ||
                          s->init || s->ninits || s->defined || s->packed || s->star))
            error_at(s->line, "%s: bounds from expressions are supported for AUTOMATIC "
                     "level-1 arrays of scalars without INITIAL only", s->name);
        else if ((s->stardims || (s->star && s->t.k == TY_BIT)) && !s->param && !s->adjust)
            error_at(s->line, "%s: * as a bound or a length is for parameters", s->name);
    }
    check_stmts(p->body);
}

/* ---- CHAR(*) parameters: the bound of their length -------------------- */

/* The longest argument a CHAR(*) parameter gets anywhere, from the
 * arguments as written (a variable's declared length, a constant's);
 * an expression counts 256.  It sizes the temporaries of expressions
 * with the parameter, so it is found before any body is checked. */
#define STAR_EXPR 256

static void star_expr(struct expr *e);

static void star_call(struct expr *e)
{
    struct proc *p;
    if (e->generic)
        gen_select(e);                  /* GENERIC: the member, now */
    p = e->callee;
    if (!p)
        return;
    if (e->bare && e->nargs == 0 && p->nparams > 0) {
        /* the procedure as a value, for an ENTRY variable or argument:
         * called through it, by no call written here (#284); a call
         * through an entry passes strings by descriptor (gen.c) */
        p->as_value = 1;
        for (int i = 0; i < p->nparams; i++) {
            struct sym *par = p->params[i];
            if (par && par->star && par->t.k != TY_AREA && par->t.p < STAR_EXPR)
                par->t.p = STAR_EXPR;
        }
    }
    for (int i = 0; i < e->nargs; i++) {
        struct sym *par = NULL;
        struct expr *a = e->arg[i];
        int n;
        if (e->entryno) {
            struct pentry *pe = &p->entries[e->entryno - 1];
            if (i < pe->nparams)
                par = p->params[pe->canon[i]];
        } else if (i < (p->nentries ? p->nprim : p->nparams)) {
            par = p->params[i];
        }
        if (!par || !par->star || par->t.k == TY_AREA)
            continue;
        n = a->k == E_STR || a->k == E_BITC ? a->len :
            a->k == E_VAR && a->sym && (a->sym->t.k == TY_CHAR || a->sym->t.k == TY_VCHAR ||
                                        a->sym->t.k == TY_BIT || a->sym->t.k == TY_VBIT) &&
            !a->sym->child ? a->sym->t.p : STAR_EXPR;
        if (n > par->t.p)
            par->t.p = n;
    }
}

/* E35 of a sort is called by the sort alone: a CHAR(*) parameter gets
 * the record length l1 of the RECORD statement, when it is a constant,
 * else STAR_EXPR; the run time cuts a longer record to that bound */
static void star_sort(struct expr *e)
{
    struct expr *x = e->fn == B_PLISRTC ? e->arg[4] : e->nargs > 5 ? e->arg[5] : NULL;
    struct expr *r = e->arg[1];
    struct sym *par;
    int n = STAR_EXPR;
    if (!x || x->k != E_FCALL || !x->callee || x->callee->nparams != 1)
        return;
    par = x->callee->params[0];
    if (!par->star || par->t.k != TY_CHAR)
        return;
    if (r->k == E_STR) {
        const char *s = r->str, *l = NULL;
        for (int i = 0; i + 7 <= r->len; i++)
            if (memcmp(s + i, "LENGTH=", 7) == 0) {
                l = s + i + 7;
                break;
            }
        if (l) {
            int v = 0;
            if (*l == '(')
                l++;
            while (l < s + r->len && *l >= '0' && *l <= '9')
                v = v * 10 + (*l++ - '0');
            if (v > 0)
                n = v;
        }
    }
    if (n > par->t.p)
        par->t.p = n;
}

static void star_expr(struct expr *e)
{
    if (!e)
        return;
    if (e->k == E_FCALL)
        star_call(e);
    if (e->k == E_CALL && (e->fn == B_PLISRTC || e->fn == B_PLISRTD))
        star_sort(e);
    star_expr(e->l);
    star_expr(e->r);
    star_expr(e->qual);
    for (int i = 0; i < e->nargs && i < MAXCALLARGS; i++)
        star_expr(e->arg[i]);
    for (int i = 0; i < e->nsubs && i < MAXCALLARGS; i++)
        star_expr(e->sub[i]);
}

static void star_stmts(struct stmt *s)
{
    for (; s; s = s->next) {
        star_expr(s->lhs);
        star_expr(s->e);
        star_expr(s->e2);
        star_expr(s->e3);
        for (struct put_item *it = s->items; it; it = it->next)
            star_expr(it->e);
        for (struct dospec *d = s->k == S_ITER ? s->specs : NULL; d; d = d->next) {
            star_expr(d->e1);
            star_expr(d->e2);
            star_expr(d->e3);
            star_expr(d->w);
            star_expr(d->u);
            star_expr(d->rep);
        }
        star_stmts(s->body);
        star_stmts(s->els);
    }
}

static void star_pass(struct proc *p)
{
    star_stmts(p->body);
    for (struct proc *c = p->child; c; c = c->sibling)
        star_pass(c);
}

static void check_unit(struct proc *p)
{
    check_proc(p);
    for (struct proc *c = p->child; c; c = c->sibling)
        check_unit(c);
}

/* Every external procedure of the source: the bounds of CHAR(*) first,
 * from the calls in all of them */
void check_program(struct proc *p)
{
    /* the CHAR(*) parameters of an external procedure: called from a
     * procedure compiled apart, too, with arguments up to the bound of an
     * expression */
    for (struct proc *u = p; u; u = u->next_ext)
        for (int i = 0; !u->is_main && i < u->nparams; i++) {
            struct sym *par = u->params[i];
            if (par && par->star && par->t.k != TY_AREA && par->t.p < STAR_EXPR)
                par->t.p = STAR_EXPR;
        }
    for (struct proc *u = p; u; u = u->next_ext)
        star_pass(u);
    for (struct proc *u = p; u; u = u->next_ext)
        for (int i = 0; u->byvalue && i < u->nparams; i++) {
            /* BYVALUE: "only ... scalar parameters ... either POINTER or
             * REAL FIXED BINARY(31,0)" (SC26-3114-01) */
            struct sym *par = u->params[i];
            if (par->ndims || par->child ||
                !(par->t.k == TY_PTR || (par->t.k == TY_FIXBIN && par->t.p == 31 &&
                                         par->t.q == 0)))
                error_at(u->line, "%s: a BYVALUE parameter is a POINTER or FIXED BINARY(31) "
                         "scalar", par->name);
        }
    for (struct proc *u = p; u; u = u->next_ext)
        check_unit(u);
}

int dec_bytes(int p)
{
    return p / 2 + 1;
}

/* The type of an element of an array function's argument (lower.c),
 * typed here on a copy: e is changed. */
struct type elem_type(struct expr *e)
{
    type_of(e);
    return e->t;
}

/* SUM and PROD of elements of type t: FLOAT, of the base and precision
 * the elements take as FLOAT; a character string is FIXED DECIMAL(15)
 * first (Q1ARR). POLY: the FLOAT type of both arguments (Q3POLY). */
struct type reduce_type(struct type t, struct type x)
{
    if (is_string(t)) {
        memset(&t, 0, sizeof t);
        t.k = TY_FLTDEC;
        t.p = 15;
    }
    if (x.k == TY_NONE)
        return float_of(t);
    return float_common(t, x);
}
