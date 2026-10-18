/* resolve.c -- names to declarations, across the block structure.
 *
 * A name is looked up from the block it is written in outward; the first
 * block that knows it decides.  A block knows its declarations and the
 * names of the procedures it contains.  In the block that knows it, a name
 * is
 *   - a variable (or a member of a structure), or
 *   - an entry: an internal procedure, or a DECLARE name ENTRY/RETURNS --
 *     the caller's view of a procedure, which PL/I (F) uses for argument
 *     conversion and the type of the returned value (JOB00052/53).
 * A name no block knows is a built-in function if it is one, otherwise it
 * is declared implicitly in the outermost block (FIXED BINARY(15) for I-N).
 */
#include <stdlib.h>
#include <string.h>

#include "pl1c.h"

static struct proc *MAINP;
static struct proc *RCUR;      /* the procedure being resolved */
static int nlabel_ids;

struct label { char *name; int id; struct label *next; };

/* ---- lookup inside one block -------------------------------------------- */

/* The item named q[n-1] in block b whose ancestors contain q[0..n-2] in
 * order; several: the fully qualified one, else ambiguous. */
static struct sym *find_in(struct proc *b, char **q, int n, int line, int *count)
{
    struct sym *found = NULL, *exact = NULL;
    *count = 0;
    for (struct sym *s = b->syms; s; s = s->next) {
        struct sym *a = s->parent;
        int k = n - 2, depth = 0;
        if (s->synthetic || strcmp(s->name, q[n - 1]) != 0)
            continue;
        for (; a && k >= 0; a = a->parent)
            if (strcmp(a->name, q[k]) == 0)
                k--;
        if (k >= 0)
            continue;
        for (a = s->parent; a; a = a->parent)
            depth++;
        (*count)++;
        found = s;
        if (depth == n - 1)
            exact = s;
    }
    if (*count > 1) {
        if (exact)
            return exact;
        error_at(line, "%s is ambiguous: qualify it", q[n - 1]);
    }
    return found;
}

static struct proc *child_named(struct proc *b, const char *name)
{
    for (struct proc *c = b->child; c; c = c->sibling)
        if (strcmp(c->name, name) == 0)
            return c;
    return NULL;
}

/* A secondary entry point of that name among b's procedures: the
 * procedure, *k the entry's number (1 for its first ENTRY statement). */
static struct proc *child_entry(struct proc *b, const char *name, int *k)
{
    for (struct proc *c = b->child; c; c = c->sibling)
        for (int i = 0; i < c->nentries; i++)
            if (strcmp(c->entries[i].name, name) == 0) {
                *k = i + 1;
                return c;
            }
    return NULL;
}

/* The internal procedure of that name seen from block b outward, or the
 * procedure with an entry point of that name (*k, else 0). */
static struct proc *EXTS;        /* the external procedures of the source */
static struct expr *call_target; /* the reference a CALL statement names */

/* An external procedure of the source by its name (Q1EXT) */
static struct proc *ext_named(const char *name)
{
    for (struct proc *p = EXTS; p; p = p->next_ext)
        if (strcmp(p->name, name) == 0)
            return p;
    return NULL;
}

/* ... or one with a secondary entry point of that name (*k) */
static struct proc *ext_entry(const char *name, int *k)
{
    for (struct proc *p = EXTS; p; p = p->next_ext)
        for (int i = 0; i < p->nentries; i++)
            if (strcmp(p->entries[i].name, name) == 0) {
                *k = i + 1;
                return p;
            }
    return NULL;
}

/* An external procedure compiled apart, from its ENTRY declaration: its
 * parameters are the descriptors' types (CHAR(*) a star parameter, an
 * empty descriptor TY_NONE: the argument as it is) */
static struct proc *STUBS;

static struct proc *stub_of(struct sym *en)
{
    struct proc *p;
    for (p = STUBS; p; p = p->next_ext)
        if (strcmp(p->name, en->name) == 0)
            return p;
    p = xmalloc(sizeof *p);
    memset(p, 0, sizeof *p);
    p->name = en->name;
    p->line = en->line;
    p->is_stub = 1;
    p->asm_opt = en->asm_opt;
    p->byvalue = en->byvalue;
    p->retcode = en->retcode;
    p->extname = en->extname;
    p->nodesc = !en->has_desc;
    p->has_ret = en->has_ret;
    p->ret = en->ret;
    for (int i = 0; en->has_desc && i < en->ndesc && i < MAXPARAMS; i++) {
        struct sym *s = xmalloc(sizeof *s);
        memset(s, 0, sizeof *s);
        s->name = "(parameter)";
        s->t = en->desc[i];
        s->level = 1;
        s->root = s;
        s->param = i + 1;
        s->owner = p;
        if ((s->t.k == TY_CHAR || s->t.k == TY_VCHAR || s->t.k == TY_BIT) && s->t.p == 0) {
            s->star = 1;
            s->t.p = 1;
        }
        p->params[p->nparams++] = s;
    }
    p->next_ext = STUBS;
    STUBS = p;
    return p;
}

static int ihek;                  /* proc_named()'s entry number, not used */

static struct proc *proc_named(struct proc *b, const char *name, int *k)
{
    *k = 0;
    for (; b; b = b->parent) {
        struct proc *c = child_named(b, name);
        if (!c)
            c = child_entry(b, name, k);
        if (c)
            return c;
    }
    {
        struct proc *x = ext_named(name);
        return x ? x : ext_entry(name, k);
    }
}

static struct sym *implicit(const char *name, int line)
{
    struct sym *s = xmalloc(sizeof *s);
    memset(s, 0, sizeof *s);
    s->name = xstrdup(name);
    s->line = line;
    s->level = 1;
    s->root = s;
    s->owner = MAINP;
    s->implicit = 1;
    if (strcmp(name, "SYSPRINT") == 0 || strcmp(name, "SYSIN") == 0) {
        /* the standard files, declared implicitly (as PL/I does) */
        s->is_file = FL_STREAM | (name[3] == 'P' ? FL_OUTPUT | FL_PRINT : FL_INPUT);
        s->t.k = TY_FILE;
        s->t.p = 4;
    } else if (name[0] >= 'I' && name[0] <= 'N') {
        s->t.k = TY_FIXBIN;
        s->t.p = 15;
    } else {
        s->t.k = TY_FLTDEC;      /* the implicit attributes of A-H, O-Z */
        s->t.p = 6;
    }
    if (!s->is_file)
        default_sym(s, MAINP);   /* the main procedure's DEFAULT statements */
    s->next = MAINP->syms;
    MAINP->syms = s;
    return s;
}

/* ---- built-in functions ------------------------------------------------- */

/* The built-in functions, each with the language level it comes from
 * (enum level, docs/language-levels.md). */

static const struct { const char *name; enum bfn fn; int min, max; enum level level; }
builtins[] = {
    {"LENGTH", B_LENGTH, 1, 1, L_F}, {"SUBSTR", B_SUBSTR, 2, 3, L_F}, {"INDEX", B_INDEX, 2, 2, L_F},
    {"MOD", B_MOD, 2, 2, L_F}, {"ABS", B_ABS, 1, 1, L_F}, {"MAX", B_MAX, 2, MAXCALLARGS, L_F},
    {"MIN", B_MIN, 2, MAXCALLARGS, L_F}, {"DIM", B_DIM, 2, 2, L_F},
    {"LBOUND", B_LBOUND, 2, 2, L_F}, {"HBOUND", B_HBOUND, 2, 2, L_F},
    {"ONCODE", B_ONCODE, 0, 0, L_F}, {"ONCHAR", B_ONCHAR, 0, 0, L_F}, {"ONSOURCE", B_ONSOURCE, 0, 0, L_F},
    {"ONFILE", B_ONFILE, 0, 0, L_F}, {"ONKEY", B_ONKEY, 0, 0, L_F}, {"BOOL", B_BOOL, 3, 3, L_F}, {"CHAR", B_CHAR, 1, 2, L_F}, {"BIT", B_BIT, 1, 2, L_F},
    {"NULL", B_NULL, 0, 0, L_F}, {"ADDR", B_ADDR, 1, 1, L_F},
    {"STORAGE", B_STG, 1, 1, L_OPT}, {"STG", B_STG, 1, 1, L_OPT},
    {"CURRENTSTORAGE", B_CSTG, 1, 1, L_OPT}, {"CSTG", B_CSTG, 1, 1, L_OPT},
    {"VERIFY", B_VERIFY, 2, 2, L_F}, {"TRANSLATE", B_TRANSLATE, 2, 3, L_F}, {"REPEAT", B_REPEAT, 2, 2, L_F},
    {"HIGH", B_HIGH, 1, 1, L_F}, {"LOW", B_LOW, 1, 1, L_F}, {"UNSPEC", B_UNSPEC, 1, 1, L_F},
    {"SIGN", B_SIGN, 1, 1, L_F}, {"FLOOR", B_FLOOR, 1, 1, L_F}, {"CEIL", B_CEIL, 1, 1, L_F},
    {"TRUNC", B_TRUNC, 1, 1, L_F}, {"ROUND", B_ROUND, 2, 2, L_F}, {"FIXED", B_FIXED, 1, 3, L_F},
    {"FLOAT", B_FLOAT, 1, 2, L_F}, {"BINARY", B_BINARY, 1, 3, L_F}, {"DECIMAL", B_DECIMAL, 1, 3, L_F},
    {"PRECISION", B_PRECISION, 2, 3, L_F}, {"ADD", B_ADD, 3, 4, L_F}, {"MULTIPLY", B_MULTIPLY, 3, 4, L_F},
    {"DIVIDE", B_DIVIDE, 3, 4, L_F}, {"SQRT", B_SQRT, 1, 1, L_F},
    {"EXP", B_EXP, 1, 1, L_F}, {"LOG", B_LOG, 1, 1, L_F}, {"LOG10", B_LOG10, 1, 1, L_F}, {"LOG2", B_LOG2, 1, 1, L_F},
    {"SIN", B_SIN, 1, 1, L_F}, {"COS", B_COS, 1, 1, L_F}, {"TAN", B_TAN, 1, 1, L_F}, {"SIND", B_SIND, 1, 1, L_F},
    {"COSD", B_COSD, 1, 1, L_F}, {"TAND", B_TAND, 1, 1, L_F}, {"ATAN", B_ATAN, 1, 2, L_F},
    {"ATAND", B_ATAND, 1, 2, L_F}, {"SINH", B_SINH, 1, 1, L_F}, {"COSH", B_COSH, 1, 1, L_F},
    {"TANH", B_TANH, 1, 1, L_F}, {"ATANH", B_ATANH, 1, 1, L_F}, {"ERF", B_ERF, 1, 1, L_F},
    {"ERFC", B_ERFC, 1, 1, L_F}, {"ALLOCATION", B_ALLOCATION, 1, 1, L_F},
    {"NULLO", B_NULLO, 0, 0, L_F}, {"EMPTY", B_EMPTY, 0, 0, L_F}, {"PREC", B_PRECISION, 2, 3, L_F},
    {"SUM", B_SUM, 1, 1, L_F}, {"PROD", B_PROD, 1, 1, L_F}, {"POLY", B_POLY, 2, 2, L_F},
    {"ALL", B_ALL, 1, 1, L_F}, {"ANY", B_ANY, 1, 1, L_F},
    {"ONLOC", B_ONLOC, 0, 0, L_F}, {"ONCOUNT", B_ONCOUNT, 0, 0, L_F}, {"DATE", B_DATE, 0, 0, L_F},
    {"TIME", B_TIME, 0, 0, L_F}, {"COUNT", B_COUNT, 1, 1, L_F}, {"LINENO", B_LINENO, 1, 1, L_F},
    {"DATAFIELD", B_DATAFIELD, 0, 0, L_F}, {"STRING", B_STRING, 1, 1, L_F},
    /* ANSI X3.53-1976 (Tier 65 on); some of them the Optimizer has too */
    {"AFTER", B_AFTER, 2, 2, L_ISO}, {"BEFORE", B_BEFORE, 2, 2, L_ISO}, {"DECAT", B_DECAT, 3, 3, L_ISO},
    {"REVERSE", B_REVERSE, 1, 1, L_ISO}, {"COPY", B_COPY, 2, 2, L_ISO}, {"COLLATE", B_COLLATE, 0, 0, L_ISO},
    {"ASIN", B_ASIN, 1, 1, L_OPT}, {"ACOS", B_ACOS, 1, 1, L_OPT}, {"SUBTRACT", B_SUBTRACT, 3, 4, L_ISO},
    {"DOT", B_DOT, 2, 4, L_ISO}, {"EVERY", B_EVERY, 1, 1, L_ISO}, {"SOME", B_SOME, 1, 1, L_ISO},
    {"OFFSET", B_OFFSETF, 2, 2, L_OPT}, {"POINTER", B_POINTERF, 2, 2, L_OPT}, {"PTR", B_POINTERF, 2, 2, L_OPT},
    {"ONFIELD", B_DATAFIELD, 0, 0, L_ISO}, {"PAGENO", B_PAGENO, 1, 1, L_ISO}, {"VALID", B_VALID, 1, 1, L_ISO},
    /* PL/I for MVS & VM (SC26-3114-01, chapter 16; Tier 86) */
    {"SYSNULL", B_SYSNULL, 0, 0, L_MVS}, {"DATETIME", B_DATETIME, 0, 0, L_MVS},
    {"POINTERADD", B_PTRADD, 2, 2, L_MVS}, {"PTRADD", B_PTRADD, 2, 2, L_MVS},
    {"POINTERVALUE", B_PTRVALUE, 1, 1, L_MVS}, {"PTRVALUE", B_PTRVALUE, 1, 1, L_MVS},
    {"BINARYVALUE", B_BINVALUE, 1, 1, L_MVS}, {"BINVALUE", B_BINVALUE, 1, 1, L_MVS},
    /* the Optimizing Compiler's return code (SC33-0006-7; Tier 87) */
    {"PLIRETC", B_PLIRETC, 1, 1, L_OPT}, {"PLIRETV", B_PLIRETV, 0, 0, L_OPT},
    {"ENTRYADDR", B_ENTRYADDR, 1, 1, L_MVS},
    /* the Optimizer's formatted dump (SC33-0006-7; Tier 105) */
    {"PLIDUMP", B_PLIDUMP, 0, 2, L_OPT},
    /* the Optimizer's sort interface (SC33-0006-7, chapter 12; Tier
     * 107): sort, record, storage, retcode, the exits, then prefix,
     * message level, technique */
    {"PLISRTA", B_PLISRTA, 4, 7, L_OPT}, {"PLISRTB", B_PLISRTB, 5, 8, L_OPT},
    {"PLISRTC", B_PLISRTC, 5, 8, L_OPT}, {"PLISRTD", B_PLISRTD, 6, 9, L_OPT},
    /* checkpoint and restart (SC33-0006-7, chapter 13; Tier 108):
     * ddname, check-id, organization, code */
    {"PLICKPT", B_PLICKPT, 0, 4, L_OPT}, {"PLIREST", B_PLIREST, 0, 0, L_OPT},
    {"PLICANC", B_PLICANC, 0, 0, L_OPT},
    /* COMPLEX (GC28-8201-4; #2) */
    {"REAL", B_REAL, 1, 1, L_F}, {"IMAG", B_IMAG, 1, 1, L_F}, {"CONJG", B_CONJG, 1, 1, L_F},
    {"COMPLEX", B_COMPLEX, 2, 2, L_F}, {"CPLX", B_COMPLEX, 2, 2, L_F},
    /* events (GC28-8201-4; #3) */
    {"COMPLETION", B_COMPLETION, 1, 1, L_F}, {"STATUS", B_STATUS, 1, 1, L_F},
};

/* a built-in subroutine: CALL only */
static int subroutine(int fn)
{
    return fn == B_PLIRETC || fn == B_PLIDUMP || (fn >= B_PLISRTA && fn <= B_PLICANC);
}

static int builtin(const char *name)
{
    for (size_t b = 0; b < sizeof builtins / sizeof builtins[0]; b++)
        if (strcmp(builtins[b].name, name) == 0)
            return (int)b;
    return -1;
}

/* ---- expressions ---------------------------------------------------------- */

static void resolve_expr(struct expr *e);

/* Turn a written reference into a function call of procedure p. */
static void make_call(struct expr *e, struct proc *p, struct sym *entry)
{
    e->bare = e->nsubs == 0;
    e->k = E_FCALL;
    e->callee = p;
    e->entry = entry;
    e->nargs = e->nsubs;
    for (int i = 0; i < e->nsubs; i++)
        e->arg[i] = e->sub[i];
    e->nsubs = 0;
}

static void resolve_ref(struct expr *e)
{
    int n = e->nq, count;
    char **q = e->qn;

    for (struct proc *b = e->scope; b; b = b->parent) {
        struct sym *s = find_in(b, q, n, e->line, &count);
        struct proc *c = n == 1 ? child_named(b, q[0]) : NULL;
        int ek = 0;
        if (!c && n == 1)
            c = child_entry(b, q[0], &ek);
        if (s && s->is_builtin)
            break;                        /* DCL name BUILTIN: below (Q1BLT) */
        if (s && s->is_cond)
            fatal("%s(%d): %s is a condition name", src_name, e->line, s->name);
        if (s && s->ngen) {
            /* a GENERIC name: check.c chooses the member by the
             * arguments (Q1GEN) */
            make_call(e, NULL, NULL);
            e->generic = s;
            return;
        }
        if (s && s->is_entry && (s->param || s->var_ef)) {
            /* an ENTRY parameter or variable: invoked with arguments or
             * by CALL, else its value */
            if (e->nsubs > 0 || e == call_target || e->nullargs)
                make_call(e, NULL, s);
            else
                e->sym = s;
            return;
        }
        if (s && s->is_entry && e == call_target && e->nsubs == 1 &&
            (strcmp(q[0], "IHESARC") == 0 || strcmp(q[0], "IHETSAC") == 0) &&
            !proc_named(e->scope, q[0], &ihek)) {
            /* PL/I (F)'s return code: CALL IHESARC(n), IHETSAC for a
             * task's -- PLIRETC's (GC28-6594-7, "Return Codes") */
            e->k = E_CALL;
            e->fn = B_PLIRETC;
            e->nargs = 1;
            e->arg[0] = e->sub[0];
            e->nsubs = 0;
            return;
        }
        if (s && s->is_entry && e == call_target && strncmp(q[0], "IHESRT", 6) == 0 &&
            q[0][6] >= 'A' && q[0][6] <= 'D' && !q[0][7] && !proc_named(e->scope, q[0], &ihek)) {
            /* PL/I (F)'s sort interface, CALL IHESRTA to IHESRTD with the
             * same arguments (GC28-6594-7, "Sort Interface"): PLISRTx */
            static const int srt[] = { B_PLISRTA, B_PLISRTB, B_PLISRTC, B_PLISRTD };
            int x = q[0][6] - 'A';
            int lo = x == 0 ? 4 : x == 3 ? 6 : 5;
            if (e->nsubs < lo || e->nsubs > lo + 1)
                fatal("%s(%d): wrong number of arguments for %s", src_name, e->line, q[0]);
            e->k = E_CALL;
            e->fn = srt[x];
            e->nargs = e->nsubs;
            for (int i = 0; i < e->nsubs; i++)
                e->arg[i] = e->sub[i];
            e->nsubs = 0;
            return;
        }
        if (s && s->is_entry && e == call_target && !proc_named(e->scope, q[0], &ihek) &&
            ((strcmp(q[0], "IHECKPS") == 0 && e->nsubs <= 4) ||
             ((strcmp(q[0], "IHECKPT") == 0 || strcmp(q[0], "IHEREST") == 0 ||
               strcmp(q[0], "IHERESN") == 0) && e->nsubs == 0))) {
            /* PL/I (F)'s checkpoint and restart (GC28-6594-7,
             * "Checkpoint/Restart"): PLICKPT, PLIREST, PLICANC */
            e->k = E_CALL;
            e->fn = q[0][3] == 'C' ? B_PLICKPT : q[0][6] == 'T' ? B_PLIREST : B_PLICANC;
            e->nargs = e->nsubs;
            for (int i = 0; i < e->nsubs; i++)
                e->arg[i] = e->sub[i];
            e->nsubs = 0;
            return;
        }
        if (s && s->is_entry) {
            int k;
            struct proc *p = proc_named(e->scope, q[0], &k);
            if (!p)
                p = stub_of(s);           /* compiled apart: called through V() */
            make_call(e, p, s);
            e->entryno = k;
            return;
        }
        if (s) {
            e->sym = s;
            return;
        }
        if (n == 1 && e->nsubs == 0)
            for (struct label *l = b->labels; l; l = l->next)
                if (strcmp(l->name, q[0]) == 0) {
                    /* a label constant as a value: its block makes a stub */
                    int k;
                    e->k = E_LABEL;
                    e->len = l->id;
                    e->callee = b;
                    for (k = 0; k < b->nvlabels && b->vlabels[k] != l->id; k++)
                        ;
                    if (k == b->nvlabels) {
                        b->vlabels = realloc(b->vlabels, sizeof(int) * (size_t)(k + 1));
                        if (!b->vlabels)
                            fatal("out of memory");
                        b->vlabels[b->nvlabels++] = l->id;
                    }
                    return;
                }
        if (c) {
            make_call(e, c, NULL);
            e->entryno = ek;
            return;
        }
    }
    if (n == 1) {
        int b = builtin(q[0]);
        if (b >= 0 && (e->nsubs > 0 || builtins[b].max == 0 ||
                       (e == call_target && (builtins[b].fn == B_PLIDUMP ||
                                             builtins[b].fn == B_PLICKPT)))) {
            e->k = E_CALL;
            e->fn = builtins[b].fn;
            e->nargs = e->nsubs;
            for (int i = 0; i < e->nsubs; i++)
                e->arg[i] = e->sub[i];
            e->nsubs = 0;
            if (e->nargs < builtins[b].min || e->nargs > builtins[b].max)
                fatal("%s(%d): wrong number of arguments for %s", src_name, e->line, q[0]);
            if ((builtins[b].fn == B_PTRADD || builtins[b].fn == B_PTRVALUE ||
                 builtins[b].fn == B_BINVALUE) && !opt_sprog)
                /* SC26-3113-01, LANGLVL: SPROG allows "the use of the
                 * POINTERADD, BINARYVALUE, and POINTERVALUE built-in
                 * functions" */
                error_at(e->line, "%s needs LANGLVL(SPROG)", q[0]);
            if (builtins[b].fn == B_PLIRETC && e != call_target)
                error_at(e->line, "PLIRETC is a subroutine: CALL PLIRETC(n)");
            if (builtins[b].fn == B_PLIDUMP && e != call_target)
                error_at(e->line, "PLIDUMP is a subroutine: CALL PLIDUMP");
            if (builtins[b].fn >= B_PLISRTA && builtins[b].fn <= B_PLICANC && e != call_target)
                error_at(e->line, "%s is a subroutine: CALL %s", q[0], q[0]);
            if (builtins[b].level != L_F) {
                char what[48];
                snprintf(what, sizeof what, "the built-in function %s", q[0]);
                beyond_f(e->line, builtins[b].level, what);
            }
            return;
        }
        {
            /* another external procedure of the source, not declared:
             * an entry name, as PL/I (F) takes it */
            int k = 0;
            struct proc *x = ext_named(q[0]);
            if (!x)
                x = ext_entry(q[0], &k);
            if (x) {
                make_call(e, x, NULL);
                e->entryno = k;
                return;
            }
        }
        if (e == call_target) {
            /* CALL of a name declared nowhere: an external entry, by its
             * context (PL1SAMP: CALL INGSP(IGSP, NULL)) -- the arguments as
             * they are */
            struct sym *en = xmalloc(sizeof *en);
            memset(en, 0, sizeof *en);
            en->name = q[0];
            en->line = e->line;
            en->is_entry = 1;
            make_call(e, stub_of(en), en);
            return;
        }
        if (e->nsubs > 0)
            fatal("%s(%d): %s(...) is neither an array nor a known function", src_name,
                  e->line, q[0]);
        e->sym = implicit(q[0], e->line);
        return;
    }
    fatal("%s(%d): %s is not declared in that structure", src_name, e->line, q[n - 1]);
}

static void resolve_expr(struct expr *e)
{
    if (!e)
        return;
    if (e->k == E_VAR && e->qn && !e->sym)
        resolve_ref(e);
    resolve_expr(e->qual);
    /* GETP -> X, a function reference without arguments: PL/I (F)
     * takes a pointer variable only (IEM0646I, JOB01090); with
     * arguments parse_ref has seen it already */
    if (e->qual && e->qual->k == E_FCALL && e->qual->bare)
        beyond_f(e->line, L_OPT, "a locator qualifier other than a pointer variable");
    for (int i = 0; i < e->nsubs; i++)
        resolve_expr(e->sub[i]);
    for (int i = 0; i < e->nargs; i++)
        resolve_expr(e->arg[i]);
    resolve_expr(e->l);
    resolve_expr(e->r);
}

/* ---- labels --------------------------------------------------------------- */

static void collect_labels(struct stmt *s, struct label **list)
{
    for (; s; s = s->next) {
        for (int i = 0; i < s->nlabels; i++) {
            struct label *l;
            for (l = *list; l; l = l->next)
                if (strcmp(l->name, s->labels[i]) == 0)
                    error_at(s->line, "label %s is used twice", s->labels[i]);
            l = xmalloc(sizeof *l);
            l->name = s->labels[i];
            l->id = s->label_id[i] = ++nlabel_ids;
            l->next = *list;
            *list = l;
        }
        collect_labels(s->body, list);
        collect_labels(s->els, list);
    }
}

/* A reference to a standard file, as if written in the current block. */
static struct expr *std_file(const char *name, int line)
{
    struct expr *e = xmalloc(sizeof *e);
    memset(e, 0, sizeof *e);
    e->k = E_VAR;
    e->line = line;
    e->qn = xmalloc(sizeof *e->qn);
    e->qn[0] = xstrdup(name);
    e->nq = 1;
    e->scope = RCUR;
    return e;
}

static void resolve_stmts(struct stmt *s, struct label *labels);

/* data list items, a repetitive specification's loop and items too */
static void resolve_items(struct put_item *it, struct label *labels)
{
    for (; it; it = it->next) {
        if (it->loop) {
            resolve_stmts(it->loop, labels);
            resolve_items(it->sub, labels);
            continue;
        }
        resolve_expr(it->e);
        resolve_expr(it->in);
    }
}

static void resolve_stmts(struct stmt *s, struct label *labels)
{
    for (; s; s = s->next) {
        if ((s->k == S_PUT || s->k == S_GET) && !s->file && !s->string)
            s->file = std_file(s->k == S_PUT ? "SYSPRINT" : "SYSIN", s->line);
        resolve_expr(s->string);
        resolve_expr(s->file);
        resolve_expr(s->copyfile);
        for (int i = 0; i < s->nfiles; i++) {
            resolve_expr(s->files[i].file);
            resolve_expr(s->files[i].linesize);
            resolve_expr(s->files[i].pagesize);
            for (int k = 0; k < s->files[i].ntabs; k++)
                resolve_expr(s->files[i].tabs[k]);
        }
        resolve_expr(s->lhs);
        for (int i = 0; i < s->nmore; i++) {
            if (s->k == S_FETCH || s->k == S_RELEASE)
                call_target = s->more[i];   /* a name, not a call of it */
            resolve_expr(s->more[i]);
            call_target = NULL;
            if (s->k == S_FETCH || s->k == S_RELEASE) {
                struct expr *x = s->more[i];
                /* an assembler routine; a PL/I procedure fetched would need
                 * a run time shared with it (internals/research/
                 * le-light-study.md, stage 2) */
                if (x->k != E_FCALL || !x->bare || !x->callee || !x->callee->is_stub ||
                    !x->callee->asm_opt)
                    error_at(s->line, "%s names an OPTIONS(ASSEMBLER) entry; a PL/I procedure "
                             "is not fetched yet", s->k == S_FETCH ? "FETCH" : "RELEASE");
                else
                    x->callee->fetched = 1;
            }
        }
        call_target = s->k == S_CALL ? s->e : NULL;
        if (s->k != S_GOTO)
            resolve_expr(s->e);             /* GO TO: below */
        call_target = NULL;
        resolve_expr(s->e2);
        resolve_expr(s->e3);
        resolve_expr(s->ignore);
        resolve_items(s->items, labels);
        if (s->k == S_ASSIGN && s->lhs->k != E_VAR &&
            !(s->lhs->k == E_CALL && (s->lhs->fn == B_ONCHAR || s->lhs->fn == B_ONSOURCE ||
                                        s->lhs->fn == B_PAGENO || s->lhs->fn == B_ENTRYADDR)) &&
            !(s->lhs->k == E_CALL && (s->lhs->fn == B_STRING || s->lhs->fn == B_UNSPEC) &&
              s->lhs->arg[0]->k == E_VAR) &&
            !(s->lhs->k == E_CALL && s->lhs->fn == B_SUBSTR && s->lhs->arg[0]->k == E_VAR) &&
            !(s->lhs->k == E_CALL && (s->lhs->fn == B_REAL || s->lhs->fn == B_IMAG ||
                                        s->lhs->fn == B_COMPLETION || s->lhs->fn == B_STATUS) &&
              s->lhs->arg[0]->k == E_VAR) &&
            !(s->lhs->k == E_CALL && s->lhs->fn == B_COMPLEX && s->lhs->arg[0]->k == E_VAR &&
              s->lhs->arg[1]->k == E_VAR))
            error_at(s->line, "the target of an assignment must be a variable");
        for (struct dospec *d = s->k == S_ITER ? s->specs : NULL; d; d = d->next) {
            resolve_expr(d->e1);
            resolve_expr(d->e2);
            resolve_expr(d->e3);
            resolve_expr(d->w);
            resolve_expr(d->u);
            resolve_expr(d->rep);
        }
        if (s->k == S_ITER) {
            resolve_expr(s->iter_ref);
            if (s->iter_ref->k != E_VAR || s->iter_ref->nsubs || s->iter_ref->sym->child)
                error_at(s->line, "the control variable must be a simple variable");
            else
                s->target = s->iter_ref->sym;
        }
        if (s->k == S_CALL && s->e->k == E_VAR && s->e->sym && s->e->sym->is_entry &&
            s->e->sym->param)
            make_call(s->e, NULL, s->e->sym);   /* CALL of an ENTRY parameter */
        if (s->k == S_CALL && s->e->k != E_FCALL &&
            !(s->e->k == E_CALL && subroutine(s->e->fn)))
            error_at(s->line, "CALL needs a procedure name");
        if (s->k == S_GOTO) {
            /* a label of this block or of one containing it, else a label
             * variable */
            struct label *l = NULL;
            struct proc *b = NULL;
            if (s->goto_name)
                for (b = RCUR; b && !l; b = l ? b : b->parent)
                    for (l = b == RCUR ? labels : b->labels; l; l = l->next)
                        if (strcmp(l->name, s->goto_name) == 0)
                            break;
            if (l) {
                s->goto_id = l->id;
                s->goto_proc = b;
                s->e = NULL;
            } else {
                resolve_expr(s->e);
                if (s->e->k != E_VAR || !s->e->sym || s->e->sym->implicit)
                    error_at(s->line, "label %s is not known here",
                             s->goto_name ? s->goto_name : "");
            }
        }
        if (s->k == S_RETURN && RCUR->is_unit)
            error_at(s->line, "RETURN cannot end an on-unit");
        if (s->k == S_RETURN && RCUR->is_block)
            error_at(s->line, "RETURN out of a BEGIN block is not supported yet");
        resolve_stmts(s->body, labels);
        resolve_stmts(s->els, labels);
    }
}

/* ---- procedures ----------------------------------------------------------- */

static void bind_params(struct proc *p)
{
    for (int i = 0; i < p->nparams; i++) {
        char *q[1] = { p->pnames[i] };
        int count;
        struct sym *s = find_in(p, q, 1, p->line, &count);
        if (!s) {
            /* an undeclared parameter: implicit attributes, in its own block */
            s = xmalloc(sizeof *s);
            memset(s, 0, sizeof *s);
            s->name = p->pnames[i];
            s->line = p->line;
            s->level = 1;
            s->root = s;
            s->owner = p;
            s->implicit = 1;
            if (s->name[0] >= 'I' && s->name[0] <= 'N') {
                s->t.k = TY_FIXBIN;
                s->t.p = 15;
            } else {
                s->t.k = TY_FLTDEC;
                s->t.p = 6;
            }
            default_sym(s, MAINP);    /* as any implicit declaration */
            s->next = p->syms;
            p->syms = s;
        }
        if (s->level != 1)
            error_at(p->line, "parameter %s must be a level-1 variable", s->name);
        if (s->is_entry) {
            s->t.k = TY_ENTRY;            /* a procedure passed in */
            s->t.p = 8;
        }
        if (s->st == ST_STATIC)
            error_at(p->line, "parameter %s cannot be STATIC", s->name);
        if (s->init || s->ninits)
            error_at(p->line, "parameter %s cannot have INITIAL", s->name);
        s->param = i + 1;
        p->params[i] = s;
    }
}

/* ---- LIKE ----------------------------------------------------------------- */

static int has_like(struct sym *s)
{
    if (s->like)
        return 1;
    for (struct sym *c = s->child; c; c = c->sibling)
        if (has_like(c))
            return 1;
    return 0;
}

/* The members of t, copied under d: levels renumbered from d's, the
 * attributes and dimensions of each member its own. */
static void copy_members(struct sym *d, struct sym *t)
{
    struct sym **tail = &d->child;
    for (struct sym *c = t->child; c; c = c->sibling) {
        struct sym *n = xmalloc(sizeof *n);
        if (c->init || c->ninits)
            fatal("%s(%d): %s: LIKE of a member with INITIAL is not supported yet", src_name,
                  d->line, c->name);
        memset(n, 0, sizeof *n);
        n->name = c->name;
        n->t = c->t;
        n->line = d->line;
        n->level = d->level + 1;
        n->ndims = c->ndims;
        memcpy(n->lb, c->lb, sizeof n->lb);
        memcpy(n->hb, c->hb, sizeof n->hb);
        n->parent = d;
        n->root = d->root;
        n->st = d->root->st;
        n->owner = d->owner;
        n->next = d->owner->syms;
        d->owner->syms = n;
        *tail = n;
        tail = &n->sibling;
        copy_members(n, c);
    }
}

/* DCL 1 S LIKE T: T, seen from S's block, must be a structure that is
 * not declared LIKE itself and contains no LIKE (IEM0596I, JOB00274). */
static void expand_like(struct proc *p)
{
    for (struct sym *s = p->syms; s; s = s->next) {
        struct sym *t = NULL;
        int count = 0;
        if (!s->like)
            continue;
        for (struct proc *b = p; b && !t; b = b->parent)
            t = find_in(b, s->like->qn, s->like->nq, s->line, &count);
        if (!t || !t->child)
            fatal("%s(%d): %s: LIKE needs a structure", src_name, s->line, s->name);
        for (struct sym *c = t->child; c; c = c->sibling)
            if (has_like(c))
                fatal("%s(%d): %s: LIKE of a structure that contains LIKE", src_name,
                      s->line, s->name);
        for (struct sym *a = t; a; a = a->parent)
            if (a->like)
                fatal("%s(%d): %s: LIKE of a structure declared LIKE", src_name, s->line,
                      s->name);
        copy_members(s, t);
    }
}

/* x REFER(y) (GC28-8201, "The REFER Option"): x an unsubscripted FIXED
 * BINARY variable outside the structure, y a FIXED BINARY member of it;
 * the element is the last of a based structure */
static void resolve_refer(struct sym *s)
{
    struct expr *x = s->refer_from, *y = s->refer_to;
    resolve_expr(x);
    resolve_expr(y);
    if (x->k != E_VAR || x->nsubs || x->sym->child || x->sym->t.k != TY_FIXBIN ||
        x->sym->root == s->root)
        error_at(s->line, "%s: REFER needs a FIXED BINARY variable outside the structure",
                 s->name);
    if (y->k != E_VAR || y->nsubs || y->qual || y->sym->child || y->sym->t.k != TY_FIXBIN ||
        y->sym->root != s->root)
        error_at(s->line, "%s: REFER(y): y must be a FIXED BINARY member of the structure",
                 s->name);
    if (s->root->st != ST_BASED || s->root == s)
        error_at(s->line, "%s: REFER belongs to a member of a based structure", s->name);
    for (struct sym *c = s; c && c != s->root; c = c->parent)
        if (c->sibling)
            error_at(s->line, "%s: REFER belongs to the last element of the structure",
                     s->name);
    if (s->refer_dim > 0 || (s->refer_dim < 0 && s->t.k != TY_CHAR) || s->child)
        error_at(s->line, "%s: REFER gives the leading upper bound or a CHARACTER length",
                 s->name);
    if (y->k == E_VAR)
        s->refer = y->sym;
}

static void resolve_proc(struct proc *p)
{
    struct label *labels = NULL;
    RCUR = p;
    expand_like(p);
    bind_params(p);
    for (struct sym *s = p->syms; s; s = s->next) {
        for (int i = 0; i < s->ninits; i++) {
            resolve_expr(s->inits[i].e);
            resolve_expr(s->inits[i].repx);
        }
        resolve_expr(s->init);
        if (s->initcall)
            resolve_stmts(s->initcall, labels);
        resolve_expr(s->based);
        resolve_expr(s->defined);
        for (int d = 0; s->adjust && d < s->ndims; d++) {
            resolve_expr(s->lbx[d]);
            resolve_expr(s->hbx[d]);
        }
        resolve_expr(s->t.area);
        if (s->refer_from)
            resolve_refer(s);
        for (int i = 0; i < s->ngen; i++) {
            /* each GENERIC member's procedure, as a call through its
             * entry declaration finds it */
            struct sym *m = s->gen[i];
            int k = 0;
            struct proc *pr = proc_named(p, m->name, &k);
            m->gen_callee = pr ? pr : stub_of(m);
            m->gen_entryno = k;
        }
    }
    collect_labels(p->body, &labels);
    p->labels = labels;
    resolve_stmts(p->body, labels);
    for (struct proc *c = p->child; c; c = c->sibling)
        resolve_proc(c);
    RCUR = p;
}

void resolve_program(struct proc *p)
{
    EXTS = p;
    for (struct proc *u = p; u; u = u->next_ext) {
        MAINP = u;                    /* implicit declarations: in their own one */
        resolve_proc(u);
    }
}
