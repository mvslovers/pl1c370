/* parse.c -- PL/I statements into a tree, for the Tier 0 language.
 *
 * Keywords are not reserved: a statement whose first word is followed by
 * '=' is an assignment, whatever the word.  Everything else is decided by
 * the first word.
 */
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "pl1c.h"
#include "../rt/pl1pic.h"

static struct lexer *L;
static int pos;
static int stmt_no;
static struct proc *cur;
static int nproc;            /* internal procedures and on-units numbered */

static struct token *tk(int k) { return &L->toks[pos + k < L->ntok ? pos + k : L->ntok - 1]; }
static int is(enum tok k) { return tk(0)->kind == k; }
static int isword(const char *w) { return is(T_IDENT) && strcmp(tk(0)->text, w) == 0; }
/* CHECK(name, ...) of the prefix prefix() read last */
static char **pfx_checks;
static int pfx_nchecks;

static int isword2(const char *a, const char *b) { return isword(a) || isword(b); }

static void expect(enum tok k, const char *what)
{
    if (!is(k))
        fatal("%s(%d): %s expected", src_name, tk(0)->line, what);
    pos++;
}

static char *stmt_text(int from, int to)
{
    int a = L->toks[from].pos, b = L->toks[to].pos;
    char *s = xmalloc((size_t)(b - a + 2));
    int n = 0;

    for (int i = a; i <= b && i < L->n; i++) {
        char c = L->s[i];
        if (c == ' ' && n > 0 && s[n - 1] == ' ')
            continue;
        s[n++] = c;
    }
    while (n > 0 && s[n - 1] == ' ')
        n--;
    s[n] = '\0';
    return s;
}

/* ---- symbols -------------------------------------------------------- */

static struct sym *lookup(const char *name)
{
    for (struct sym *s = cur->syms; s; s = s->next)
        if (strcmp(s->name, name) == 0 && !s->synthetic)
            return s;
    return NULL;
}

static struct sym *add_sym(const char *name, int line)
{
    struct sym *s = xmalloc(sizeof *s);
    memset(s, 0, sizeof *s);
    s->name = xstrdup(name);
    s->line = line;
    s->level = 1;
    s->root = s;
    s->owner = cur;
    s->next = cur->syms;
    cur->syms = s;
    return s;
}

static struct expr *expr(void);
static struct expr *new_expr(enum ekind k, int line);

/* A reference: name [ (subscripts) ] { . name [ (subscripts) ] }.
 * Subscripts written at any level are collected in order. */
static struct expr *parse_ref1(void);

/* A reference, with a pointer qualifier: P -> X */
static struct expr *parse_ref(void)
{
    struct expr *e = parse_ref1();
    /* ANSI: any pointer-valued reference qualifies, P -> Q -> X,
     * ADDR(A) -> X, F(I) -> X (Tier 76); PL/I (F) takes a pointer
     * variable only (IEM0646I) */
    while (is(T_ARROW)) {
        struct expr *x;
        if (e->nsubs || e->qual)
            beyond_f(e->line, L_OPT, "a locator qualifier other than a pointer variable");
        pos++;
        x = parse_ref1();
        x->qual = e;
        e = x;
    }
    return e;
}

static struct expr *parse_ref1(void)
{
    char *q[16];
    int n = 0, line = tk(0)->line;
    struct expr *e = new_expr(E_VAR, line);

    for (;;) {
        if (!is(T_IDENT))
            fatal("%s(%d): name expected", src_name, tk(0)->line);
        if (n == 16)
            fatal("%s(%d): qualification too deep", src_name, line);
        q[n++] = tk(0)->text;
        pos++;
        if (is(T_LP) && tk(1)->kind == T_RP && n == 1 &&
            (strcmp(q[0], "SYSNULL") == 0 || strcmp(q[0], "DATETIME") == 0 ||
             strcmp(q[0], "PLIRETV") == 0)) {
            pos += 2;               /* SYSNULL(), DATETIME(), PLIRETV() (SC26-3114-01) */
        } else if (is(T_LP) && tk(1)->kind == T_RP) {
            /* a null argument list: a function reference with no arguments
             * (GC33-0009-4, "Entry Expression Arguments"); PL/I (F) refuses
             * it (IEM0031I, Q1EMP) */
            beyond_f(line, L_OPT, "a null argument list ()");
            e->nullargs = 1;
            pos += 2;
        } else if (is(T_LP)) {
            pos++;
            for (;;) {
                if (e->nsubs == MAXCALLARGS)
                    fatal("%s(%d): too many subscripts or arguments", src_name, line);
                if (is(T_STAR) && (tk(1)->kind == T_COMMA || tk(1)->kind == T_RP)) {
                    /* M(2,*): a cross section (Q1XSEC) */
                    e->sub[e->nsubs++] = new_expr(E_STAR, tk(0)->line);
                    pos++;
                } else {
                    e->sub[e->nsubs++] = expr();
                }
                if (is(T_COMMA)) { pos++; continue; }
                break;
            }
            expect(T_RP, "')'");
        }
        if (is(T_DOT) && tk(1)->kind == T_IDENT) {
            pos++;
            continue;
        }
        break;
    }
    for (int i = 1; i + 1 < e->nsubs; i++)
        if (e->sub[i]->k != E_STAR) {
            int before = 0, after = 0;
            for (int j = 0; j < i; j++)
                before |= e->sub[j]->k == E_STAR;
            for (int j = i + 1; j < e->nsubs; j++)
                after |= e->sub[j]->k == E_STAR;
            if (before && after) {
                /* K(*,3,*): IEM0881I (Tier 61, JOB00978) */
                beyond_f(line, L_OPT, "a subscript between two asterisks");
                break;
            }
        }
    e->qn = xmalloc((size_t)n * sizeof *e->qn);
    memcpy(e->qn, q, (size_t)n * sizeof *q);
    e->nq = n;
    e->scope = cur;
    return e;
}

/* ---- expressions ---------------------------------------------------- */

static struct expr *new_expr(enum ekind k, int line)
{
    struct expr *e = xmalloc(sizeof *e);
    memset(e, 0, sizeof *e);
    e->k = k;
    e->line = line;
    return e;
}

static struct expr *expr(void);

static struct expr *primary(void)
{
    struct token *t = tk(0);
    struct expr *e;

    if (is(T_NUM) && tk(1)->kind == T_IDENT && strcmp(tk(1)->text, "SUB") == 0) {
        /* iSUB, in the base reference of a DEFINED item */
        e = new_expr(E_ISUB, t->line);
        e->len = atoi(t->text);
        pos += 2;
        return e;
    }
    if (is(T_NUM) && strchr(t->text, 'B')) {
        /* a binary constant: FIXED BINARY(p,q), p the digits and q those
         * after the point, or FLOAT BINARY(p) with the decimal exponent
         * of 2 (Q1BINC, JOB00349) */
        const char *s = t->text;
        int p = 0, q = 0, point = 0;
        unsigned long long m = 0;
        char *b;
        e = new_expr(E_NUM, t->line);
        e->bin = 1;
        e->imag = s[strlen(s) - 1] == 'I';
        for (; *s && *s != 'E' && *s != 'B'; s++) {
            if (*s == '.') { point = 1; continue; }
            if (p < 64)
                m = 2 * m + (unsigned)(*s - '0');
            p++;
            if (point) q++;
        }
        b = xmalloc(24);
        snprintf(b, 24, "%llu", m);
        e->num = b;
        if (*s == 'E') {
            e->e10 = atoi(s + 1) - q;
            e->t.k = e->imag ? TY_CFLTBIN : TY_FLTBIN;
            e->t.p = p;
            if (p > 53)
                error_at(t->line, "constant %s has more than 53 digits", t->text);
        } else {
            e->t.k = e->imag ? TY_CFIXBIN : TY_FIXBIN;
            e->t.p = p;
            e->t.q = q;
            if (p > 31)
                error_at(t->line, "constant %s has more than 31 digits", t->text);
        }
        pos++;
        return e;
    }
    if (is(T_NUM)) {
        const char *s = t->text;
        int p = 0, q = 0, point = 0;
        e = new_expr(E_NUM, t->line);
        e->num = xmalloc(strlen(s) + 1);
        e->imag = s[strlen(s) - 1] == 'I';  /* 2I: the attributes of 2 (GC28-8201-4) */
        for (; *s && *s != 'E' && *s != 'I'; s++) {
            if (*s == '.') { point = 1; continue; }
            e->num[p++] = *s;
            if (point) q++;
        }
        e->num[p] = '\0';
        if (*s == 'E') {
            /* a FLOAT constant: its precision is the number of digits */
            e->e10 = atoi(s + 1) - q;
            e->t.k = e->imag ? TY_CFLTDEC : TY_FLTDEC;
            e->t.p = p;
            if (p > 16)
                error_at(t->line, "constant %s has more than 16 digits", t->text);
            pos++;
            return e;
        }
        if (p > 15)
            error_at(t->line, "constant %s has more than 15 digits", t->text);
        e->t.k = e->imag ? TY_CFIXDEC : TY_FIXDEC;
        e->t.p = p;
        e->t.q = q;
        pos++;
        return e;
    }
    if (is(T_STR)) {
        e = new_expr(E_STR, t->line);
        e->str = t->str;
        e->len = t->len;
        e->t.k = TY_CHAR;
        e->t.p = t->len;
        pos++;
        return e;
    }
    if (is(T_BITC)) {
        e = new_expr(E_BITC, t->line);
        e->str = t->str;
        e->len = t->len;
        e->t.k = TY_BIT;
        e->t.p = t->len;
        pos++;
        return e;
    }
    if (is(T_LP) && tk(1)->kind == T_NUM && tk(2)->kind == T_RP &&
        (tk(3)->kind == T_STR || tk(3)->kind == T_BITC)) {
        /* (n)'string': the string n times */
        int n = atoi(tk(1)->text), len = tk(3)->len;
        char *v = xmalloc((size_t)(n * len + 1));
        for (int i = 0; i < n; i++)
            memcpy(v + i * len, tk(3)->str, (size_t)len);
        v[n * len] = '\0';
        e = new_expr(tk(3)->kind == T_STR ? E_STR : E_BITC, t->line);
        e->str = v;
        e->len = n * len;
        e->t.k = tk(3)->kind == T_STR ? TY_CHAR : TY_BIT;
        e->t.p = n * len;
        pos += 4;
        return e;
    }
    if (is(T_LP)) {
        pos++;
        e = expr();
        expect(T_RP, "')'");
        if (e->k == E_VAR)
            e->paren = 1;         /* an argument (X) is a dummy (check.c) */
        return e;
    }
    if (is(T_IDENT)) {
        return parse_ref();
    }
    fatal("%s(%d): operand expected", src_name, t->line);
    return NULL;
}

static struct expr *unary(void)
{
    if (is(T_MINUS)) {
        int line = tk(0)->line;
        struct expr *e;
        pos++;
        e = new_expr(E_NEG, line);
        e->l = unary();
        return e;
    }
    if (is(T_PLUS)) {
        pos++;
        return unary();
    }
    if (is(T_NOT)) {
        int line = tk(0)->line;
        struct expr *e;
        pos++;
        e = new_expr(E_NOT, line);
        e->l = unary();
        return e;
    }
    if (is(T_POWER))
        fatal("%s(%d): operand expected before **", src_name, tk(0)->line);
    {
        /* ** binds tighter than the prefix operators and from the right:
         * -2**2 is -4, 2**3**2 is 512 (JOB00182) */
        struct expr *e = primary();
        if (is(T_POWER)) {
            struct expr *p = new_expr(E_BIN, e->line);
            pos++;
            p->op = T_POWER;
            p->l = e;
            p->r = unary();
            return p;
        }
        return e;
    }
}

static struct expr *binary(struct expr *l, int op, struct expr *r)
{
    struct expr *e = new_expr(E_BIN, l->line);
    e->op = op;
    e->l = l;
    e->r = r;
    return e;
}

static struct expr *mult(void)
{
    struct expr *e = unary();
    while (is(T_STAR) || is(T_SLASH)) {
        int op = tk(0)->kind;
        pos++;
        e = binary(e, op, unary());
    }
    return e;
}

static struct expr *add(void)
{
    struct expr *e = mult();
    while (is(T_PLUS) || is(T_MINUS)) {
        int op = tk(0)->kind;
        pos++;
        e = binary(e, op, mult());
    }
    return e;
}

static struct expr *cat(void)
{
    struct expr *e = add();
    while (is(T_CAT)) {
        pos++;
        e = binary(e, T_CAT, add());
    }
    return e;
}

static struct expr *cmp(void)
{
    struct expr *e = cat();
    while (is(T_EQ) || is(T_NE) || is(T_LT) || is(T_LE) || is(T_GT) || is(T_GE) ||
           is(T_NLT) || is(T_NGT)) {
        int op = tk(0)->kind;
        if (op == T_NLT) op = T_GE;
        if (op == T_NGT) op = T_LE;
        pos++;
        e = binary(e, op, cat());
    }
    return e;
}

static struct expr *and_(void)
{
    struct expr *e = cmp();
    while (is(T_AND)) {
        pos++;
        e = binary(e, T_AND, cmp());
    }
    return e;
}

static struct expr *expr(void)
{
    struct expr *e = and_();
    while (is(T_OR)) {
        pos++;
        e = binary(e, T_OR, and_());
    }
    return e;
}

/* ---- declarations --------------------------------------------------- */

static int number_in_parens(int *second)
{
    int a;
    expect(T_LP, "'('");
    if (!is(T_NUM))
        fatal("%s(%d): number expected", src_name, tk(0)->line);
    a = atoi(tk(0)->text);
    pos++;
    if (second) {
        *second = 0;
        if (is(T_COMMA)) {
            pos++;
            if (!is(T_NUM))
                fatal("%s(%d): number expected", src_name, tk(0)->line);
            *second = atoi(tk(0)->text);
            pos++;
        }
    }
    expect(T_RP, "')'");
    return a;
}

static int signed_number(void)
{
    int neg = 0, v;
    if (is(T_MINUS)) { neg = 1; pos++; }
    else if (is(T_PLUS)) pos++;
    if (!is(T_NUM))
        fatal("%s(%d): number expected", src_name, tk(0)->line);
    v = atoi(tk(0)->text);
    pos++;
    return neg ? -v : v;
}

/* (bound, bound, ...) with bound = [lb:]hb, lb 1 by default. */
/* x REFER(y), for bound k of s (-1: its length) */
static void refer(struct sym *s, int k)
{
    if (s->refer_from)
        fatal("%s(%d): REFER more than once in a declaration", src_name, tk(0)->line);
    s->refer_from = parse_ref();
    if (!isword("REFER"))
        fatal("%s(%d): REFER expected", src_name, tk(0)->line);
    pos++;
    expect(T_LP, "'('");
    s->refer_to = parse_ref();
    expect(T_RP, "')'");
    s->refer_dim = k;
}

static struct expr *int_const_p(int v, int line)
{
    char buf[16];
    struct expr *n = new_expr(E_NUM, line);
    snprintf(buf, sizeof buf, "%d", v < 0 ? -v : v);
    n->num = xstrdup(buf);
    n->t.k = TY_FIXDEC;                 /* as a written constant */
    n->t.p = (int)strlen(buf);
    if (v < 0) {
        struct expr *m = new_expr(E_NEG, line);
        m->l = n;
        return m;
    }
    return n;
}

/* One bound: a signed number into *v (NULL returned), M REFER(N) (the
 * bound 1 here, refer() records it), or an expression (returned). */
static struct expr *bound_value(struct sym *s, int *v)
{
    int save = pos;
    struct expr *x;
    if (is(T_NUM) || ((is(T_MINUS) || is(T_PLUS)) && tk(1)->kind == T_NUM &&
                      (tk(2)->kind == T_COMMA || tk(2)->kind == T_RP || tk(2)->kind == T_COLON))) {
        *v = signed_number();
        if (is(T_COMMA) || is(T_RP) || is(T_COLON))
            return NULL;
        pos = save;                    /* 10*N: an expression after all */
    }
    x = expr();
    if (isword("REFER")) {
        pos = save;
        *v = 1;
        refer(s, s->ndims);
        return NULL;
    }
    *v = 1;
    return x;
}

static void dims(struct sym *s)
{
    expect(T_LP, "'('");
    for (;;) {
        int a, b, lb = 1;
        if (is(T_STAR)) {
            /* A(*): a parameter's bounds, those of its argument */
            pos++;
            if (s->ndims && !s->stardims)
                error_at(s->line, "%s: either every bound is * or none", s->name);
            s->stardims = 1;
            s->lb[s->ndims] = s->hb[s->ndims] = 1;
            if (s->ndims == MAXDIMS)
                fatal("%s(%d): too many dimensions", src_name, s->line);
            s->ndims++;
            if (is(T_COMMA)) { pos++; continue; }
            break;
        }
        if (s->stardims && !s->adjust)
            error_at(s->line, "%s: either every bound is * or none", s->name);
        if (s->ndims == MAXDIMS)
            fatal("%s(%d): too many dimensions", src_name, s->line);
        {
            /* a bound: a number, M REFER(N) (Q1REF), or an expression of
             * an AUTOMATIC array, evaluated at the block's entry (Tier 81,
             * RXPI's A(LEN)) */
            struct expr *xl = NULL, *xh;
            xh = bound_value(s, &a);
            b = a;
            if (is(T_COLON)) {
                pos++;
                xl = xh;
                lb = a;
                xh = bound_value(s, &b);
                if (!xl && !xh && s->refer_from && s->refer_dim == s->ndims && b == 1)
                    b = lb;                     /* lb : M REFER(N) */
            }
            if (xl || xh) {
                if (s->ndims && !s->adjust)
                    error_at(s->line, "%s: bounds from expressions in every dimension or "
                             "in none", s->name);
                s->adjust = 1;
                s->stardims = 1;
                s->lbx[s->ndims] = xl ? xl : int_const_p(lb, s->line);
                s->hbx[s->ndims] = xh ? xh : int_const_p(b, s->line);
                lb = b = 1;
            } else if (s->adjust) {
                s->lbx[s->ndims] = int_const_p(lb, s->line);
                s->hbx[s->ndims] = int_const_p(b, s->line);
                lb = b = 1;
            }
        }
        if (b < lb)
            error_at(s->line, "%s: upper bound below lower bound", s->name);
        s->lb[s->ndims] = lb;
        s->hb[s->ndims] = b;
        s->ndims++;
        if (is(T_COMMA)) { pos++; continue; }
        break;
    }
    expect(T_RP, "')'");
}

static struct expr *init_value1(void);

/* INITIAL value: a constant, a negative constant, a string, a bit string;
 * '*' (NULL); ANSI's (expression) and references (CM43 to CM45); a
 * complex constant, a real and an imaginary one with a sign between
 * them (#2) */
static struct expr *init_value(void)
{
    struct expr *n = init_value1();
    if (n && (n->k == E_NUM || (n->k == E_NEG && n->l->k == E_NUM)) &&
        !(n->k == E_NUM ? n : n->l)->imag && (is(T_PLUS) || is(T_MINUS)) &&
        tk(1)->kind == T_NUM && tk(1)->text[strlen(tk(1)->text) - 1] == 'I') {
        struct expr *e = new_expr(E_BIN, tk(0)->line);
        e->op = tk(0)->kind;
        pos++;
        e->l = n;
        e->r = primary();
        return e;
    }
    return n;
}

static struct expr *init_value1(void)
{
    struct expr *n;
    if (is(T_STAR)) {
        pos++;
        return NULL;
    }
    if (is(T_LP) && !(tk(1)->kind == T_NUM && tk(2)->kind == T_RP &&
                      (tk(3)->kind == T_STR || tk(3)->kind == T_BITC))) {
        /* ANSI: a parenthesized expression (Tier 78); PL/I (F)
         * refuses it (IEM0139I, Q2INIE) */
        beyond_f(tk(0)->line, L_OPT, "an expression in INITIAL");
        pos++;
        n = expr();
        expect(T_RP, "')'");
        return n;
    }
    if (is(T_PLUS))
        pos++;
    if (is(T_IDENT)) {
        beyond_f(tk(0)->line, L_OPT, "a reference in INITIAL");
        return parse_ref();        /* ANSI: a reference (Tier 78) */
    }
    if (is(T_MINUS)) {
        int line = tk(0)->line;
        pos++;
        n = primary();
        if (n->k != E_NUM)
            fatal("%s(%d): a number expected after '-'", src_name, line);
        {
            struct expr *e = new_expr(E_NEG, line);
            e->l = n;
            return e;
        }
    }
    n = primary();
    if (n->k != E_NUM && n->k != E_STR && n->k != E_BITC)
        fatal("%s(%d): INITIAL takes constants", src_name, n->line);
    return n;
}

/* The items of an INITIAL list up to its ')', appended to s->inits; a
 * group (n)(item, ...) is expanded n times here (nested to any depth
 * the source has). */
static void init_list(struct sym *s)
{
    for (;;) {
        int rep = 1;
        struct expr *repx = NULL;
        if (is(T_LP) && tk(1)->kind == T_NUM && tk(2)->kind == T_RP &&
            tk(3)->kind != T_STR && tk(3)->kind != T_BITC) {
            /* an iteration factor; before a string it is the string's
             * repetition factor: CHAR(8) INIT((3)'AB') is 'ABABAB', and
             * (2)'Q' one element (Q3INA, Q6IND) */
            rep = atoi(tk(1)->text);
            pos += 3;
            if (is(T_LP) && !(tk(1)->kind == T_NUM && tk(2)->kind == T_RP &&
                              (tk(3)->kind == T_STR || tk(3)->kind == T_BITC))) {
                /* (n)(item, ...): the group n times; (n)(m)'..' is m
                 * copies of the string n times (T188SUM) */
                int first = s->ninits, n;
                pos++;
                init_list(s);
                expect(T_RP, "')'");
                n = s->ninits - first;
                if (rep == 0)
                    s->ninits = first;
                for (int k = 1; k < rep; k++) {
                    if (s->ninits + n > MAXINIT)
                        fatal("%s(%d): too many INITIAL items", src_name, s->line);
                    memcpy(&s->inits[s->ninits], &s->inits[first], (size_t)n * sizeof *s->inits);
                    s->ninits += n;
                }
                if (is(T_COMMA)) { pos++; continue; }
                break;
            }
        } else if (is(T_LP) && !(tk(1)->kind == T_NUM && tk(2)->kind == T_RP)) {
            /* (expression) item: an iteration factor (Q5INIF); without an
             * item after it, ANSI's parenthesized value */
            int save = pos;
            struct expr *x;
            pos++;
            x = expr();
            expect(T_RP, "')'");
            if (is(T_COMMA) || is(T_RP)) {
                pos = save;
            } else {
                if (is(T_LP))
                    fatal("%s(%d): a group after an iteration factor that is an expression "
                          "is not supported yet", src_name, s->line);
                repx = x;
                rep = 0;
            }
        }
        if (s->ninits == MAXINIT)
            fatal("%s(%d): too many INITIAL items", src_name, s->line);
        s->inits[s->ninits].rep = rep;
        s->inits[s->ninits].repx = repx;
        s->inits[s->ninits].e = init_value();
        s->ninits++;
        if (is(T_COMMA)) { pos++; continue; }
        break;
    }
}

/* INITIAL(item, ...), item = [(n)] value | (n)(item, ...). */
static void initial(struct sym *s)
{
    if (isword("CALL")) {
        /* INITIAL CALL p(args): p called when the variable is allocated
         * (GC33-0009-4, INITIAL attribute; SC26-3114-01) */
        struct stmt *c = xmalloc(sizeof *c);
        memset(c, 0, sizeof *c);
        beyond_f(tk(0)->line, L_OPT, "INITIAL CALL");
        c->k = S_CALL;
        c->no = stmt_no;
        c->line = tk(0)->line;
        c->text = "INITIAL CALL";
        pos++;
        c->e = parse_ref();
        s->initcall = c;
        return;
    }
    expect(T_LP, "'('");
    s->inits = xmalloc(MAXINIT * sizeof *s->inits);
    init_list(s);
    expect(T_RP, "')'");
    if (s->ninits == 1 && s->inits[0].rep == 1 && s->inits[0].e)
        s->init = s->inits[0].e;
}

/* Data attributes of a parameter descriptor or RETURNS: FIXED BINARY(p),
 * FIXED DECIMAL(p,q), CHARACTER(n) [VARYING], BIT(n). */
/* The arithmetic type of FIXED/FLOAT, BINARY/DECIMAL and a precision:
 * FIXED and DECIMAL by default, FIXED DEC(5), FIXED BIN(15), FLOAT
 * DEC(6), FLOAT BIN(21). */
static void arith_type(struct type *t, int flt, int bin, int prec, int scale, const char *name,
                       int line)
{
    memset(t, 0, sizeof *t);
    if (flt) {
        t->k = bin ? TY_FLTBIN : TY_FLTDEC;
        t->p = prec < 0 ? (bin ? 21 : 6) : prec;
        if (scale)
            error_at(line, "%s: FLOAT takes no scale factor", name);
        if (t->p < 1 || t->p > (bin ? 53 : 16))
            error_at(line, "%s: FLOAT %s precision must be 1 to %d", name, bin ? "BINARY" :
                     "DECIMAL", bin ? 53 : 16);
    } else if (bin) {
        t->k = TY_FIXBIN;
        t->p = prec < 0 ? 15 : prec;
        t->q = scale;
    } else {
        t->k = TY_FIXDEC;
        t->p = prec < 0 ? 5 : prec;
        t->q = scale;
    }
}

/* (lo[:hi]) or (lo[:hi], qlo[:qhi]) of a generic description */
static void gen_prec(struct gdesc *g, int line)
{
    expect(T_LP, "'('");
    for (int k = 0; k < 2; k++) {
        int neg = 0, lo, hi;
        if (is(T_MINUS)) { neg = 1; pos++; }
        if (!is(T_NUM))
            fatal("%s(%d): a number expected in a generic precision", src_name, line);
        lo = atoi(tk(0)->text) * (neg ? -1 : 1);
        pos++;
        hi = lo;
        if (is(T_COLON)) {
            /* the Optimizer takes one number only (GC33-0009-4, GENERIC) */
            beyond_f(line, L_ISO, "a precision range in a generic description");
            pos++;
            neg = 0;
            if (is(T_MINUS)) { neg = 1; pos++; }
            if (!is(T_NUM))
                fatal("%s(%d): a number expected in a generic precision", src_name, line);
            hi = atoi(tk(0)->text) * (neg ? -1 : 1);
            pos++;
        }
        if (k == 0) { g->prec = 1; g->pmin = (short)lo; g->pmax = (short)hi; }
        else { g->scale = 1; g->qmin = (short)lo; g->qmax = (short)hi; }
        if (!is(T_COMMA))
            break;
        pos++;
    }
    expect(T_RP, "')'");
}

/* WHEN ([generic-description, ...]): ANSI's generic element (CM37) */
static void gen_when(struct gelem *ge, int line)
{
    pos++;                                   /* WHEN */
    expect(T_LP, "'('");
    ge->when = 1;
    ge->n = 0;
    while (!is(T_RP)) {
        struct gdesc *g;
        if (ge->n == 16)
            fatal("%s(%d): too many generic descriptions", src_name, line);
        g = &ge->d[ge->n++];
        memset(g, 0, sizeof *g);
        if (is(T_STAR)) {
            pos++;
            g->star = 1;
        } else {
            if (is(T_NUM))
                fatal("%s(%d): a level in a generic description (a structure) is not "
                      "supported yet", src_name, line);
            if (is(T_LP)) {
                /* (*, ...): the asterisk bounds of an array */
                pos++;
                for (;;) {
                    if (!is(T_STAR))
                        fatal("%s(%d): a generic description's bounds are asterisks",
                              src_name, line);
                    pos++;
                    g->dims++;
                    if (is(T_COMMA)) { pos++; continue; }
                    break;
                }
                expect(T_RP, "')'");
            }
            while (is(T_IDENT)) {
                if (isword("FIXED")) { g->fixed = 1; pos++; if (is(T_LP)) gen_prec(g, line); }
                else if (isword("FLOAT")) { g->flt = 1; pos++; if (is(T_LP)) gen_prec(g, line); }
                else if (isword2("BIN", "BINARY")) { g->bin = 1; pos++; if (is(T_LP)) gen_prec(g, line); }
                else if (isword2("DEC", "DECIMAL")) { g->dec = 1; pos++; if (is(T_LP)) gen_prec(g, line); }
                else if (isword2("PREC", "PRECISION")) { pos++; gen_prec(g, line); }
                else if (isword2("CHAR", "CHARACTER")) { g->chr = 1; pos++; }
                else if (isword("BIT")) { g->bit = 1; pos++; }
                else if (isword2("VAR", "VARYING")) { g->var = 1; pos++; }
                else if (isword2("NONVAR", "NONVARYING")) {
                    beyond_f(line, L_ISO, "NONVARYING");
                    g->nonvar = 1;
                    pos++;
                }
                else if (isword2("POINTER", "PTR")) { g->ptr = 1; pos++; }
                else if (isword2("PIC", "PICTURE")) { g->pic = 1; pos++; if (is(T_STR)) pos++; }
                else if (isword("LABEL")) { g->label = 1; pos++; }
                else if (isword("ENTRY")) { g->entry = 1; pos++; }
                else if (isword("FILE")) { g->file = 1; pos++; }
                else if (isword("AREA")) { g->area = 1; pos++; }
                else if (isword("OFFSET")) { g->offs = 1; pos++; }
                else if (isword("REAL") || isword("ALIGNED") || isword2("UNAL", "UNALIGNED"))
                    pos++;
                else if (isword2("DIM", "DIMENSION")) {
                    beyond_f(line, L_ISO, "DIMENSION");
                    pos++;
                    expect(T_LP, "'('");
                    for (;;) {
                        if (!is(T_STAR))
                            fatal("%s(%d): DIMENSION in a generic description takes "
                                  "asterisks", src_name, line);
                        pos++;
                        g->dims++;
                        if (is(T_COMMA)) { pos++; continue; }
                        break;
                    }
                    expect(T_RP, "')'");
                } else
                    fatal("%s(%d): %s in a generic description is not supported yet",
                          src_name, line, tk(0)->text);
            }
        }
        if (is(T_COMMA)) { pos++; continue; }
        break;
    }
    expect(T_RP, "')'");
}

static void attrs(struct type *t, int line)
{
    int fixed = 0, flt = 0, bin = 0, dec = 0, chr = 0, var = 0, bit = 0, prec = -1, scale = 0,
        len = 1, optional = 0, cplx = 0;
    if (is(T_COMMA) || is(T_RP)) {
        /* an empty descriptor: the argument as it is (PL1SAMP's
         * ENTRY(,FLOAT BIN,FLOAT BIN)) */
        memset(t, 0, sizeof *t);
        t->k = TY_NONE;
        return;
    }
    while (is(T_IDENT) || (is(T_LP) && (fixed || flt || bin || dec))) {
        if (isword("OPTIONAL")) {
            /* an argument that may be omitted, for an OPTIONS(ASSEMBLER)
             * entry (SC26-3114-01, "OPTIONAL Attribute") */
            beyond_f(line, L_MVS, "OPTIONAL");
            optional = 1;
            pos++;
            if (!is(T_IDENT) && !fixed && !flt && !bin && !dec && !chr && !bit)
                fatal("%s(%d): OPTIONAL with the descriptor's attributes", src_name, line);
            continue;
        }
        if (isword("FIXED")) { fixed = 1; pos++; }
        else if (isword("FLOAT")) { flt = 1; pos++; }
        else if (isword2("BIN", "BINARY")) { bin = 1; pos++; }
        else if (isword2("DEC", "DECIMAL")) { dec = 1; pos++; }
        else if (isword2("CHAR", "CHARACTER")) {
            chr = 1;
            pos++;
            if (is(T_LP) && tk(1)->kind == T_STAR && tk(2)->kind == T_RP) {
                pos += 3;          /* CHAR(*): the argument's length */
                len = 0;
            } else if (is(T_LP)) {
                len = number_in_parens(NULL);
            }
        }
        else if (isword("BIT")) {
            bit = 1;
            pos++;
            if (is(T_LP) && tk(1)->kind == T_STAR && tk(2)->kind == T_RP) {
                pos += 3;          /* BIT(*): the argument's length */
                len = 0;
            } else if (is(T_LP)) {
                len = number_in_parens(NULL);
            }
        }
        else if (isword2("VAR", "VARYING")) { var = 1; pos++; }
        else if (isword2("NONVAR", "NONVARYING") || isword2("PARM", "PARAMETER")) {
            beyond_f(line, L_ISO, tk(0)->text);
            pos++;                         /* ANSI: what F implies (Tier 74) */
        }
        else if (isword2("POINTER", "PTR")) { bit = -1; pos++; }
        else if (isword("AREA")) {
            /* AREA, AREA(n) or AREA(*) */
            bit = -5;
            pos++;
            len = 1000;
            if (is(T_LP) && tk(1)->kind == T_STAR && tk(2)->kind == T_RP) {
                pos += 3;
                len = 0;
            } else if (is(T_LP)) {
                len = number_in_parens(NULL);
            }
        }
        else if (isword("LABEL")) { bit = -2; pos++; }
        else if (isword2("CONN", "CONNECTED")) { beyond_f(line, L_OPT, "CONNECTED"); pos++; }
        else if (isword("ENTRY")) { bit = -3; pos++; }
        else if (isword("FILE")) { bit = -4; pos++; }
        else if (isword2("COMPLEX", "CPLX")) { cplx = 1; pos++; }   /* #2 C5 */
        else if (isword("REAL")) pos++;
        else if (is(T_LP)) prec = number_in_parens(&scale);
        else
            fatal("%s(%d): attribute %s is not supported in a descriptor yet", src_name, line,
                  tk(0)->text);
    }
    memset(t, 0, sizeof *t);
    if (bit == -5) { t->k = TY_AREA; t->p = len; }
    else if (bit == -2) { t->k = TY_LABEL; t->p = 8; }
    else if (bit == -3) { t->k = TY_ENTRY; t->p = 8; }
    else if (bit == -4) { t->k = TY_FILE; t->p = 4; }
    else if (bit < 0) { t->k = TY_PTR; t->p = 4; }
    else if (bit) { t->k = var ? TY_VBIT : TY_BIT; t->p = len; }
    else if (chr) { t->k = var ? TY_VCHAR : TY_CHAR; t->p = len; }
    else if (fixed || flt || bin || dec || cplx) arith_type(t, !fixed, bin, prec, scale, "descriptor", line);
    else
        fatal("%s(%d): give the attributes here", src_name, line);
    if (cplx) {
        if (bit || chr)
            fatal("%s(%d): COMPLEX takes FIXED or FLOAT data", src_name, line);
        *t = cplx_type(*t);
    }
    t->optional = optional;
}

/* PICTURE 'spec', repetition factors (n) expanded.  A picture with X or
 * A is a character string (X, A, 9 only); any other one is numeric
 * character data of fixed-point decimal value: 9 V Z * Y , . / B $ S + -
 * T I R CR DB (Language Reference, section D; the editing is
 * rt/pl1pic.c's, compiled in here for the attributes). */
static void picture(const char *s, int len, int line, struct type *t)
{
    char b[300];
    int n = 0, nv = 0, nz = 0, ns = 0;
    for (int i = 0; i < len; i++) {
        int rep = 1;
        if (s[i] == '(') {
            rep = 0;
            for (i++; i < len && isdigit((unsigned char)s[i]); i++)
                rep = rep * 10 + (s[i] - '0');
            if (i >= len - 1 || s[i] != ')' || rep < 1)
                fatal("%s(%d): a repetition factor in a picture is (n) followed by a "
                      "character", src_name, line);
            i++;
        }
        for (int r = 0; r < rep; r++) {
            if (n >= 255)
                fatal("%s(%d): a picture of more than 255 characters", src_name, line);
            b[n++] = (char)toupper((unsigned char)s[i]);
        }
    }
    b[n] = '\0';
    if (n == 0)
        fatal("%s(%d): an empty picture", src_name, line);
    memset(t, 0, sizeof *t);
    if (b[0] == 'G') {
        /* a sterling picture: G [edit] M pounds M [sep] shillings M [sep]
         * pence [edit], its value pence (GC28-8201-4, section D; the
         * editing rt/pl1pic.c's) */
        int nm = 0;
        for (int i = 1; i < n; i++) {
            if (b[i] == 'M')
                nm++;
            else if ((b[i] == 'C' && b[i + 1] == 'R') || (b[i] == 'D' && b[i + 1] == 'B'))
                i++;
            else if (!strchr("9ZY*TIR,./B$S+-V876HP", b[i]))
                fatal("%s(%d): picture character %c is not one of a sterling picture",
                      src_name, line, b[i]);
        }
        if (nm != 3)
            fatal("%s(%d): a sterling picture has three fields, each after an M", src_name,
                  line);
        t->k = TY_PIC;
        t->p = pl1_pic_digits(b, n);
        t->q = pl1_pic_scale(b, n);
        t->pic = xstrdup(b);
        if (t->p > 15)
            fatal("%s(%d): a sterling picture of more than 15 digits", src_name, line);
        return;
    }
    if (strpbrk(b, "XA")) {
        for (int i = 0; i < n; i++)
            if (!strchr("XA9", b[i]))
                fatal("%s(%d): a character-string picture takes X, A and 9 only", src_name,
                      line);
        t->k = TY_CHAR;
        t->p = n;
        t->pic = xstrdup(b);
        return;
    }
    for (int i = 0; i < n; i++) {
        char c = b[i];
        if ((c == 'C' && b[i + 1] == 'R') || (c == 'D' && b[i + 1] == 'B')) {
            i++;
            continue;
        }
        if (c == 'V') nv++;
        else if (c == 'Z') nz++;
        else if (c == '*') ns++;
        else if (!strchr("9Y,./B$S+-TIR", c))
            fatal("%s(%d): picture character %c is not supported (floating-point, scaled "
                  "pictures are not)", src_name, line, c);
    }
    if (nv > 1)
        fatal("%s(%d): a picture has one V at most", src_name, line);
    if (nz && ns)
        fatal("%s(%d): Z and * in one picture are not supported", src_name, line);
    {
        const char *v = strchr(b, 'V');
        if (v)
            for (const char *d = "$S+-"; *d; d++) {
                int before = 0, after = 0;
                for (const char *c = b; *c; c++)
                    if (*c == *d)
                        *(c < v ? &before : &after) += 1;
                if (before + after > 1 && after)
                    fatal("%s(%d): a drifting string past the V is not supported", src_name,
                          line);
            }
    }
    t->k = TY_PIC;
    t->p = pl1_pic_digits(b, n);
    t->q = pl1_pic_scale(b, n);
    t->pic = xstrdup(b);
    if (t->p < 1)
        fatal("%s(%d): a numeric picture needs a digit position", src_name, line);
    if (t->p > 15)
        fatal("%s(%d): a numeric picture of more than 15 digits", src_name, line);
}

int pic_length(struct type t)
{
    return pl1_pic_length(t.pic, (int)strlen(t.pic));
}

/* One DECLARE statement.  Items are [level] name [(dims)] [attributes]
 * or (name, name, ...) attributes; a level-n item belongs to the nearest
 * item before it with a lower level. */
/* ---- factored declarations ----------------------------------------- */

/* A token list (expanding a DECLARE). */
struct tlist { struct token *t; int n, cap; };

static void tl_add(struct tlist *l, const struct token *t)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? 2 * l->cap : 64;
        l->t = realloc(l->t, (size_t)l->cap * sizeof *l->t);
        if (!l->t)
            fatal("out of memory");
    }
    l->t[l->n++] = *t;
}

/* the end of the list element starting at i: a comma or the end e at
 * depth 0 */
static int elem_end(int i, int e)
{
    int d = 0;
    for (; i < e; i++) {
        enum tok k = L->toks[i].kind;
        if (k == T_LP) d++;
        else if (k == T_RP) d--;
        else if (k == T_COMMA && d == 0) break;
    }
    return i;
}

/* the matching ')' of the '(' at i */
static int paren_end(int i, int e)
{
    int d = 0;
    for (; i < e; i++) {
        if (L->toks[i].kind == T_LP) d++;
        else if (L->toks[i].kind == T_RP && --d == 0) return i;
    }
    fatal("%s(%d): ')' missing in a declaration", src_name, L->toks[i - 1].line);
    return e;
}

/* Expand the items in [b, e): each one written out with the level and
 * the attributes factored around it, as GC28-8201-4 (DECLARE) allows:
 * DCL ((A, B) FIXED, C FLOAT) STATIC is DCL A FIXED STATIC, B FIXED
 * STATIC, C FLOAT STATIC.  pre and post are the tokens to put before
 * (a level number) and after (attributes) every item. */
static void expand_items(struct tlist *out, int b, int e, const struct tlist *pre,
                         const struct tlist *post)
{
    int i = b;
    while (i < e) {
        int j = elem_end(i, e), k = i;
        struct tlist mypre = { 0 }, mypost = { 0 };
        int first = out->n == 0;
        if (!first) {
            struct token c = L->toks[i - 1];
            c.kind = T_COMMA;
            c.text = ",";
            tl_add(out, &c);
        }
        /* a level number of its own, else the one factored around it */
        if (L->toks[k].kind == T_NUM) {
            tl_add(&mypre, &L->toks[k]);
            k++;
        } else {
            for (int x = 0; x < pre->n; x++)
                tl_add(&mypre, &pre->t[x]);
        }
        if (k < j && L->toks[k].kind == T_LP && k + 1 < j &&
            (L->toks[k + 1].kind == T_IDENT || L->toks[k + 1].kind == T_LP ||
             L->toks[k + 1].kind == T_NUM)) {
            /* a factored list: its own attributes, then the outer ones */
            int r = paren_end(k, j);
            for (int x = r + 1; x < j; x++)
                tl_add(&mypost, &L->toks[x]);
            for (int x = 0; x < post->n; x++)
                tl_add(&mypost, &post->t[x]);
            {
                struct tlist sub = { 0 };
                expand_items(&sub, k + 1, r, &mypre, &mypost);
                for (int x = 0; x < sub.n; x++)
                    tl_add(out, &sub.t[x]);
                free(sub.t);
            }
        } else {
            for (int x = 0; x < mypre.n; x++)
                tl_add(out, &mypre.t[x]);
            for (int x = k; x < j; x++)
                tl_add(out, &L->toks[x]);
            for (int x = 0; x < post->n; x++)
                tl_add(out, &post->t[x]);
        }
        free(mypre.t);
        free(mypost.t);
        i = j < e ? j + 1 : j;
    }
}

/* The DECLARE at pos with its factored lists written out, in place. */
static void unfactor_declare(void)
{
    int b = pos + 1, e = b, d = 0, factored = 0;
    struct tlist out = { 0 }, none = { 0 };
    for (; e < L->ntok && !(L->toks[e].kind == T_SEMI && d == 0); e++) {
        if (L->toks[e].kind == T_LP) d++;
        else if (L->toks[e].kind == T_RP) d--;
    }
    /* only if some item is factored with something more than names in
     * it, or nested: declare() reads (A, B) attributes itself */
    for (int i = b; i < e; i = elem_end(i, e) + 1) {
        int k = i;
        if (L->toks[k].kind == T_NUM)
            k++;
        if (L->toks[k].kind == T_LP) {
            int r = paren_end(k, e);
            for (int x = k + 1; x < r; x++)
                if (L->toks[x].kind != T_IDENT && L->toks[x].kind != T_COMMA)
                    factored = 1;
        }
    }
    if (!factored)
        return;
    expand_items(&out, b, e, &none, &none);
    {
        /* splice: tokens [b, e) become out */
        int delta = out.n - (e - b);
        if (L->ntok + delta > L->cap) {
            L->cap = L->ntok + delta + 64;
            L->toks = realloc(L->toks, (size_t)L->cap * sizeof *L->toks);
            if (!L->toks)
                fatal("out of memory");
        }
        memmove(&L->toks[e + delta], &L->toks[e], (size_t)(L->ntok - e) * sizeof *L->toks);
        memcpy(&L->toks[b], out.t, (size_t)out.n * sizeof *out.t);
        L->ntok += delta;
    }
    free(out.t);
}

static void declare(void)
{
    int line = tk(0)->line;
    struct sym *stack[16];
    int depth = 0, fact_level = 0;

    unfactor_declare();
    pos++;                                   /* DCL */
    for (;;) {
        char *names[64];
        int ptr = 0;
        int nn = 0, level = 1, fixed = 0, flt = 0, bin = 0, dec = 0, chr = 0, var = 0, prec = -1,
            scale = 0, clen = 1, stat = 0, bit = 0, data, entry = 0, hasret = 0, file = 0,
            fattr = 0, cplx = 0, evt = 0;
        struct sym tmpl;
        struct type pict;
        int aligned = 0, unal = 0, ext = 0, bltin = 0, o_asm = 0, o_retc = 0, o_byval = 0;
        int excl = 0;
        struct { char recfm[4]; int blk, rec, bufs, ncp, iarea, opts; } env = { "", 0, 0, 0, 0, 0, 0 };
        const char *extname = NULL;
        /* ANSI's attributes that spell out what PL/I (F) implies (Tier 74) */
        int a_const = 0, a_var = 0, a_parm = 0, a_local = 0, a_struct = 0, a_member = 0,
            a_nonvar = 0, a_cond = 0, based_np = 0;
        int haspic = 0, islabel = 0, ctl = 0, area = 0, offs = 0, clen_given = 0, star = 0,
            area_dflt = 0;
        struct expr *offarea = NULL;

        memset(&tmpl, 0, sizeof tmpl);
        memset(&pict, 0, sizeof pict);
        if (is(T_NUM)) {
            level = atoi(tk(0)->text);
            pos++;
        }
        if (is(T_LP)) {
            pos++;
            for (;;) {
                if (!is(T_IDENT))
                    fatal("%s(%d): name expected", src_name, tk(0)->line);
                if (nn == 64)
                    fatal("%s(%d): too many names in one factored declaration", src_name, line);
                names[nn++] = tk(0)->text;
                pos++;
                if (is(T_COMMA)) { pos++; continue; }
                expect(T_RP, "')'");
                break;
            }
        } else {
            if (!is(T_IDENT))
                fatal("%s(%d): name expected", src_name, tk(0)->line);
            names[nn++] = tk(0)->text;
            pos++;
        }
        tmpl.line = line;
        tmpl.name = names[0];
        if (is(T_LP))
            dims(&tmpl);
        while (!is(T_COMMA) && !is(T_SEMI)) {
            if (isword("FIXED")) { fixed = 1; pos++; }
            else if (isword("FLOAT")) { flt = 1; pos++; }
            else if (isword2("BIN", "BINARY")) { bin = 1; pos++; }
            else if (isword2("DEC", "DECIMAL")) { dec = 1; pos++; }
            else if (isword2("COMPLEX", "CPLX")) { cplx = 1; pos++; }   /* #2 */
            else if (isword("EVENT")) { evt = 1; pos++; }               /* #3 */
            else if (isword("REAL")) pos++;
            else if (isword2("CHAR", "CHARACTER")) {
                chr = 1; pos++;
                if (is(T_LP) && tk(1)->kind == T_STAR && tk(2)->kind == T_RP) {
                    pos += 3;
                    star = 1;              /* CHAR(*): a parameter's */
                    clen = 1;
                    clen_given = 1;
                } else if (is(T_LP) && tk(1)->kind == T_IDENT) {
                    /* CHAR(K REFER(L)) (Q1REF) */
                    pos++;
                    refer(&tmpl, -1);
                    expect(T_RP, "')'");
                    clen = 1;
                    clen_given = 1;
                } else if (is(T_LP)) { clen = number_in_parens(NULL); clen_given = 1; }
            }
            else if (isword("BIT")) {
                bit = 1; pos++;
                if (is(T_LP) && tk(1)->kind == T_STAR && tk(2)->kind == T_RP) {
                    pos += 3;
                    star = 1;              /* BIT(*): a parameter's */
                    clen = 1;
                    clen_given = 1;
                } else if (is(T_LP)) { clen = number_in_parens(NULL); clen_given = 1; }
            }
            else if (isword2("VAR", "VARYING")) { var = 1; pos++; }
            else if (isword2("NONVAR", "NONVARYING")) { a_nonvar = 1; pos++; }
            else if (isword("CONSTANT")) { a_const = 1; pos++; }
            else if (isword("VARIABLE")) { a_var = 1; pos++; }
            else if (isword2("PARM", "PARAMETER")) { a_parm = 1; pos++; }
            else if (isword("LOCAL")) { a_local = 1; pos++; }
            else if (isword("STRUCTURE")) { a_struct = 1; pos++; }
            else if (isword("MEMBER")) { a_member = 1; pos++; }
            else if (isword2("COND", "CONDITION")) { a_cond = 1; pos++; }
            else if (isword2("DIM", "DIMENSION")) {
                beyond_f(line, L_ISO, "DIMENSION");
                pos++;
                if (tmpl.ndims)
                    error_at(line, "%s: two dimension lists", names[0]);
                dims(&tmpl);
            }
            else if (isword("ALIGNED")) { aligned = 1; unal = 0; pos++; }
            else if (isword2("UNAL", "UNALIGNED")) { aligned = 0; unal = 1; pos++; }
            else if (isword2("AUTO", "AUTOMATIC")) { pos++; }
            /* REDUCIBLE, IRREDUCIBLE: optimization hints, no effect here */
            else if (isword2("RED", "REDUCIBLE") || isword2("IRRED", "IRREDUCIBLE")) { pos++; }
            else if (isword2("POINTER", "PTR")) { ptr = 1; pos++; }
            else if (isword("AREA")) {
                /* AREA(n), 1000 bytes if n is not given (GC28-8201-4);
                 * AREA(*) a parameter's, its size in its own header */
                area = 1000;
                area_dflt = 1;
                pos++;
                if (is(T_LP) && tk(1)->kind == T_STAR && tk(2)->kind == T_RP) {
                    pos += 3;
                    star = 1;
                    area_dflt = 0;
                } else if (is(T_LP)) {
                    area = number_in_parens(NULL);
                    area_dflt = 0;
                }
                if (area < 1 || area > 32767)
                    error_at(line, "the size of an area is 1 to 32767 bytes");
            }
            else if (isword("OFFSET")) {
                pos++;
                offs = 1;
                if (!is(T_LP))
                    fatal("%s(%d): OFFSET needs its area: OFFSET(a)", src_name, line);
                pos++;
                offarea = parse_ref1();
                expect(T_RP, "')'");
            }
            else if (isword("BASED")) {
                pos++;
                if (!is(T_LP)) {
                    /* ANSI: BASED without a pointer, every reference
                     * qualified (Tier 76); PL/I (F) needs the pointer
                     * (IEM0154I, JOB00171) */
                    based_np = 1;
                } else {
                    pos++;
                    tmpl.based = parse_ref();
                    expect(T_RP, "')'");
                }
            }
            else if (isword("STATIC")) { stat = 1; pos++; }
            else if (isword2("EXT", "EXTERNAL")) {
                ext = 1; stat = 1; pos++;
                if (is(T_LP) && tk(1)->kind == T_STR && tk(2)->kind == T_RP) {
                    /* EXTERNAL('name'): the name outside, for an
                     * OPTIONS(ASSEMBLER) entry (SC26-3114-01) */
                    beyond_f(line, L_MVS, "EXTERNAL('name')");
                    if (tk(1)->len < 1 || tk(1)->len > 8)
                        error_at(line, "EXTERNAL('name'): 1 to 8 characters");
                    extname = tk(1)->str;
                    pos += 3;
                }
            }
            else if (isword("OPTIONS") && tk(1)->kind == T_LP) {
                /* an ENTRY's OPTIONS(ASSEMBLER [RETCODE] [INTER] [BYADDR])
                 * (the Optimizer's, SC33-0006-7 chapter 11) */
                beyond_f(line, L_OPT, "OPTIONS of an ENTRY");
                pos += 2;
                while (!is(T_RP)) {
                    if (isword2("ASM", "ASSEMBLER"))
                        o_asm = 1;
                    else if (isword("RETCODE"))
                        o_retc = 1;
                    else if (isword("BYVALUE")) {
                        beyond_f(line, L_MVS, "BYVALUE");
                        o_byval = 1;
                    }
                    else if (!isword("INTER") && !isword("BYADDR"))
                        fatal("%s(%d): entry option %s is not supported yet", src_name, line,
                              is(T_IDENT) ? tk(0)->text : "?");
                    pos++;
                    if (is(T_COMMA))
                        pos++;
                }
                pos++;
            }
            else if (isword("BUILTIN")) { bltin = 1; pos++; }
            else if (isword2("INT", "INTERNAL")) { pos++; }
            else if (isword2("CONN", "CONNECTED")) {
                /* a parameter in connected storage (GC33-0009-4): PL1C/370
                 * passes a cross section as a copy anyway, no effect */
                beyond_f(line, L_OPT, "CONNECTED");
                pos++;
            }
            else if (isword2("CTL", "CONTROLLED")) { ctl = 1; pos++; }
            else if (isword("LABEL")) { islabel = 1; pos++; }
            else if (isword2("PIC", "PICTURE")) {
                pos++;
                if (!is(T_STR))
                    fatal("%s(%d): PICTURE needs the picture as a string", src_name, line);
                picture(tk(0)->str, tk(0)->len, line, &pict);
                haspic = 1;
                pos++;
            }
            else if (isword2("DEF", "DEFINED")) { pos++; tmpl.defined = parse_ref1(); }
            else if (isword2("POS", "POSITION")) { pos++; tmpl.position = number_in_parens(NULL); }
            else if (isword("LIKE")) {
                pos++;
                tmpl.like = parse_ref1();
                if (tmpl.like->nsubs)
                    fatal("%s(%d): LIKE names a structure, without subscripts", src_name, line);
            }
            else if (isword2("INIT", "INITIAL")) { pos++; initial(&tmpl); }
            else if (isword("FILE")) { file = 1; pos++; }
            else if (isword("STREAM")) { fattr |= FL_STREAM; pos++; }
            else if (isword("RECORD")) { fattr |= FL_RECORD; pos++; }
            else if (isword("INPUT")) { fattr |= FL_INPUT; pos++; }
            else if (isword("OUTPUT")) { fattr |= FL_OUTPUT; pos++; }
            else if (isword("UPDATE")) { fattr |= FL_UPDATE | FL_RECORD; pos++; }
            else if (isword("SEQUENTIAL") || isword("SEQL")) { fattr |= FL_RECORD; pos++; }
            else if (isword("PRINT")) { fattr |= FL_PRINT | FL_OUTPUT; pos++; }
            else if (isword("KEYED")) { fattr |= FL_KEYED | FL_RECORD; pos++; }
            else if (isword("DIRECT")) { fattr |= FL_DIRECT | FL_KEYED | FL_RECORD; pos++; }
            else if (isword("BUFFERED") || isword("BUF") || isword("UNBUFFERED") ||
                     isword("UNBUF")) { pos++; }   /* UNBUFFERED: as BUFFERED */
            else if (isword2("EXCL", "EXCLUSIVE")) {
                /* record locking between tasks; with one task it has no
                 * effect (Q1EXC, #217).  It implies RECORD and KEYED. */
                excl = 1;
                fattr |= FL_KEYED | FL_RECORD;
                pos++;
            }
            else if (isword2("ENV", "ENVIRONMENT")) {
                /* ENVIRONMENT(option ...), the options separated by blanks
                 * (GC28-8201-4, the ENVIRONMENT attribute): F(b[,r]),
                 * V(b[,r]), VS(b[,r]), VBS(b[,r]), U(b), BUFFERS(n),
                 * CONSECUTIVE, INDEXED, REGIONAL(n), LEAVE, REWIND,
                 * CTLASA, CTL360, COBOL, INDEXAREA[(n)], NOWRITE, GENKEY,
                 * NCP(n), TRKOFL */
                pos++;
                expect(T_LP, "'('");
                while (!is(T_RP)) {
                    if (!is(T_IDENT))
                        fatal("%s(%d): an ENVIRONMENT option expected", src_name, line);
                    if (isword("INDEXED")) {
                        pos++;
                        fattr |= FL_INDEXED | FL_RECORD;
                    } else if (isword("REGIONAL")) {
                        pos++;
                        int rn = number_in_parens(NULL);
                        if (rn < 1 || rn > 3)
                            fatal("%s(%d): REGIONAL(%d): 1, 2 or 3", src_name, line, rn);
                        fattr |= FL_REGIONAL | FL_RECORD |
                                 (rn == 2 ? FL_REG2 : rn == 3 ? FL_REG3 : 0);
                    } else if (isword("F") || isword("V") || isword("VS") || isword("VBS") ||
                               isword("U")) {
                        /* the record format, with the block size and (not
                         * for U) the record size */
                        if (env.recfm[0])
                            error_at(line, "%s: two record formats", names[0]);
                        snprintf(env.recfm, sizeof env.recfm, "%s", tk(0)->text);
                        pos++;
                        expect(T_LP, "'('");
                        if (!is(T_NUM))
                            fatal("%s(%d): a block size expected", src_name, line);
                        env.blk = atoi(tk(0)->text);
                        pos++;
                        if (is(T_COMMA)) {
                            pos++;
                            if (!is(T_NUM) || env.recfm[0] == 'U')
                                fatal("%s(%d): a record size expected (and none for U)",
                                      src_name, line);
                            env.rec = atoi(tk(0)->text);
                            pos++;
                        }
                        expect(T_RP, "')'");
                        if (env.recfm[0] == 'F' && env.rec && env.blk % env.rec)
                            error_at(line, "%s: F(%d,%d): the block size is a multiple of "
                                     "the record size", names[0], env.blk, env.rec);
                        if (env.recfm[0] == 'V' && env.rec && env.rec > env.blk - 4 &&
                            strcmp(env.recfm, "V") == 0)
                            error_at(line, "%s: V(%d,%d): the record size is at least four "
                                     "less than the block size", names[0], env.blk, env.rec);
                    } else if (isword("BUFFERS")) {
                        pos++;
                        env.bufs = number_in_parens(NULL);
                    } else if (isword("NCP")) {
                        pos++;
                        env.ncp = number_in_parens(NULL);
                    } else if (isword("INDEXAREA")) {
                        pos++;
                        env.opts |= EO_INDEXAREA;
                        if (is(T_LP))
                            env.iarea = number_in_parens(NULL);
                    } else {
                        static const struct { const char *w; int bit; } eo[] = {
                            {"CONSECUTIVE", EO_CONSECUTIVE}, {"LEAVE", EO_LEAVE},
                            {"REWIND", EO_REWIND}, {"CTLASA", EO_CTLASA},
                            {"CTL360", EO_CTL360}, {"COBOL", EO_COBOL},
                            {"NOWRITE", EO_NOWRITE}, {"GENKEY", EO_GENKEY},
                            {"TRKOFL", EO_TRKOFL},
                        };
                        size_t k = 0;
                        while (k < sizeof eo / sizeof eo[0] && !isword(eo[k].w))
                            k++;
                        if (k == sizeof eo / sizeof eo[0])
                            fatal("%s(%d): ENVIRONMENT option %s is not supported yet",
                                  src_name, line, tk(0)->text);
                        env.opts |= eo[k].bit;
                        pos++;
                    }
                    if (is(T_COMMA))
                        pos++;              /* PL/I (F) separates them by blanks */
                }
                pos++;
                if ((env.opts & EO_CTLASA) && (env.opts & EO_CTL360))
                    error_at(line, "%s: CTLASA and CTL360", names[0]);
            }
            else if (isword("ENTRY")) {
                entry = 1;
                pos++;
                if (is(T_LP)) {
                    pos++;
                    tmpl.has_desc = 1;
                    if (!is(T_RP))
                        for (;;) {
                            if (tmpl.ndesc == MAXCALLARGS)
                                fatal("%s(%d): too many parameter descriptors", src_name, line);
                            if (is(T_NUM) && (tk(1)->kind == T_COMMA || tk(1)->kind == T_RP)) {
                                /* 1, 2 attributes, ...: a structure parameter
                                 * (GC28-8201-4, ENTRY attribute) -- passed as
                                 * it is, its members' descriptors skipped */
                                int lv = atoi(tk(0)->text);
                                memset(&tmpl.desc[tmpl.ndesc], 0, sizeof tmpl.desc[0]);
                                tmpl.desc[tmpl.ndesc++].k = TY_NONE;
                                pos++;
                                while (is(T_COMMA) && tk(1)->kind == T_NUM &&
                                       atoi(tk(1)->text) > lv) {
                                    struct type skip;
                                    pos += 2;
                                    if (!is(T_COMMA) && !is(T_RP))
                                        attrs(&skip, line);
                                }
                                if (is(T_COMMA)) { pos++; continue; }
                                break;
                            }
                            if (is(T_LP) && (tk(1)->kind == T_STAR || tk(1)->kind == T_NUM) &&
                                (tk(2)->kind == T_RP || tk(2)->kind == T_COMMA)) {
                                /* (*) or (n, ...): an array parameter, as in
                                 * GENERIC below */
                                int nd = 0;
                                pos++;
                                for (;;) {
                                    nd++;
                                    pos++;
                                    if (is(T_COMMA)) { pos++; continue; }
                                    break;
                                }
                                expect(T_RP, "')'");
                                tmpl.desc_dims[tmpl.ndesc] = (unsigned char)nd;
                            }
                            attrs(&tmpl.desc[tmpl.ndesc++], line);
                            if (is(T_COMMA)) { pos++; continue; }
                            break;
                        }
                    expect(T_RP, "')'");
                }
            }
            else if (isword("RETURNS")) {
                pos++;
                expect(T_LP, "'('");
                attrs(&tmpl.ret, line);
                expect(T_RP, "')'");
                hasret = 1;
            }
            else if (isword("GENERIC")) {
                /* GENERIC(member ENTRY(...) [RETURNS(...)], ...): each member
                 * an entry declaration of the block (Q1GEN) */
                pos++;
                expect(T_LP, "'('");
                for (;;) {
                    struct sym *m;
                    if (!is(T_IDENT))
                        fatal("%s(%d): a GENERIC member name expected", src_name, line);
                    m = lookup(tk(0)->text);
                    if (!m || m->level != 1)
                        m = add_sym(tk(0)->text, line);
                    m->implicit = 0;
                    m->level = 1;
                    m->root = m;
                    m->is_entry = 1;
                    pos++;
                    tmpl.gel = realloc(tmpl.gel, (size_t)(tmpl.ngen + 1) * sizeof *tmpl.gel);
                    if (!tmpl.gel)
                        fatal("out of memory");
                    memset(&tmpl.gel[tmpl.ngen], 0, sizeof *tmpl.gel);
                    if (isword("WHEN")) {
                        /* ANSI: name WHEN (descriptions), the first that
                         * matches is taken (4.4.5.11) */
                        beyond_f(line, L_OPT, "GENERIC with WHEN");
                        gen_when(&tmpl.gel[tmpl.ngen], line);
                    } else if (is(T_COMMA) || is(T_RP)) {
                        /* ANSI: a member without WHEN, for a reference
                         * without arguments (4.4.5.11, Case 1.1) */
                        beyond_f(line, L_ISO, "a GENERIC member without WHEN");
                        tmpl.gel[tmpl.ngen].when = 2;
                    } else if (!isword("ENTRY")) {
                        fatal("%s(%d): a GENERIC member needs ENTRY or WHEN", src_name, line);
                    } else {
                    pos++;
                    m->has_desc = 1;
                    m->ndesc = 0;
                    if (is(T_LP)) {
                        pos++;
                        while (!is(T_RP)) {
                            int nd = 0;
                            if (m->ndesc == MAXCALLARGS)
                                fatal("%s(%d): too many parameter descriptors", src_name, line);
                            if (is(T_LP) && (tk(1)->kind == T_STAR || tk(1)->kind == T_NUM)) {
                                /* (*) or (n, ...): an array parameter */
                                pos++;
                                for (;;) {
                                    nd++;
                                    pos++;
                                    if (is(T_COMMA)) { pos++; continue; }
                                    break;
                                }
                                expect(T_RP, "')'");
                            }
                            m->desc_dims[m->ndesc] = (unsigned char)nd;
                            attrs(&m->desc[m->ndesc++], line);
                            if (is(T_COMMA)) { pos++; continue; }
                            break;
                        }
                        expect(T_RP, "')'");
                    }
                    if (isword("RETURNS")) {
                        pos++;
                        expect(T_LP, "'('");
                        attrs(&m->ret, line);
                        expect(T_RP, "')'");
                        m->has_ret = 1;
                    }
                    }
                    tmpl.gen = realloc(tmpl.gen, (size_t)(tmpl.ngen + 1) * sizeof *tmpl.gen);
                    if (!tmpl.gen)
                        fatal("out of memory");
                    tmpl.gen[tmpl.ngen++] = m;
                    if (is(T_COMMA)) { pos++; continue; }
                    break;
                }
                expect(T_RP, "')'");
            }
            else if (is(T_LP) && (fixed || flt || bin || dec)) prec = number_in_parens(&scale);
            else if (is(T_IDENT))
                fatal("%s(%d): attribute %s is not supported yet", src_name, tk(0)->line, tk(0)->text);
            else
                fatal("%s(%d): attribute expected", src_name, tk(0)->line);
        }
        data = fixed || flt || bin || dec || chr || bit || var || ptr || haspic || islabel ||
               area || offs || cplx || evt;
        if (fixed && flt)
            error_at(line, "%s: FIXED and FLOAT", names[0]);
        if (a_nonvar && var)
            error_at(line, "%s: VARYING and NONVARYING", names[0]);
        if (a_nonvar && !chr && !bit)
            error_at(line, "%s: NONVARYING needs CHARACTER or BIT", names[0]);
        if (a_const && a_var)
            error_at(line, "%s: CONSTANT and VARIABLE", names[0]);
        if (a_const && !file && !fattr && !entry && !hasret)
            error_at(line, "%s: CONSTANT is for a file or an entry", names[0]);
        if (a_var && (file || fattr || entry || hasret) && (level > 1 || tmpl.ndims || ctl ||
                                                           stat || tmpl.based || tmpl.defined))
            fatal("%s(%d): %s: an ENTRY or FILE variable in a structure, an array or "
                  "not AUTOMATIC is not supported yet",
                  src_name, line, names[0]);
        if (a_local && !islabel)
            error_at(line, "%s: LOCAL is for a label", names[0]);
        if (a_cond && (data || file || fattr || entry || hasret || level > 1 || tmpl.ndims))
            error_at(line, "%s: a CONDITION takes no other attributes", names[0]);
        if (a_nonvar)
            beyond_f(line, L_ISO, "NONVARYING");
        if (a_const)
            beyond_f(line, L_ISO, "CONSTANT");
        if (a_var)                         /* GC33-0009-4: ENTRY and FILE only */
            beyond_f(line, file || fattr || entry || hasret ? L_OPT : L_ISO, "VARIABLE");
        if (a_parm)
            beyond_f(line, L_ISO, "PARAMETER");
        if (a_local)
            beyond_f(line, L_ISO, "LOCAL");
        if (a_struct || a_member)
            beyond_f(line, L_ISO, a_struct ? "STRUCTURE" : "MEMBER");
        if (a_cond)
            beyond_f(line, L_OPT, "the CONDITION attribute");
        if (based_np)
            beyond_f(line, L_OPT, "BASED without a pointer");
        if (tmpl.based && (tmpl.based->nsubs || tmpl.based->qual))
            beyond_f(line, L_OPT, "BASED with a pointer other than a pointer variable");
        /* 2 (A, B) FIXED: members side by side, each with the attributes
         * (GC28-8201-4, DECLARE); minor structures factored so, with
         * members of their own below, are not supported */
        if (fact_level && level > fact_level)
            error_at(line, "%s: members below factored names are not supported", names[0]);
        fact_level = level > 1 && nn > 1 ? level : 0;
        while (depth > 0 && stack[depth - 1]->level >= level)
            depth--;
        if (level > 1 && depth == 0)
            error_at(line, "%s: level %d with no structure above it", names[0], level);
        for (int i = 0; i < nn; i++) {
            struct sym *s = NULL;
            if (level == 1) {
                s = lookup(names[i]);
                if (s && s->level == 1 && !s->implicit)
                    error_at(line, "%s is declared twice", names[i]);
                if (s && !(s->implicit && s->level == 1))
                    s = NULL;
            }
            if (!s)
                s = add_sym(names[i], line);
            s->implicit = 0;
            s->level = level;
            s->star = star;
            s->aligned = aligned;
            s->unal = unal;
            s->is_builtin = bltin;
            s->ansi_parm = a_parm;
            s->ansi_struct = a_struct;
            s->ansi_member = a_member;
            s->is_cond = a_cond;
            s->external = ext && level == 1;
            s->st = stat ? ST_STATIC : tmpl.based || based_np ? ST_BASED : ctl ? ST_CTL : ST_AUTO;
            if (based_np && (stat || ctl))
                error_at(line, "%s: STATIC or CONTROLLED and BASED", names[i]);
            if (based_np && level > 1)
                error_at(line, "%s: BASED belongs on the level-1 name", names[i]);
            if (ctl && (stat || tmpl.based || tmpl.defined))
                error_at(line, "%s: CONTROLLED and another storage class", names[i]);
            s->based = tmpl.based;
            s->like = tmpl.like;
            s->defined = tmpl.defined;
            s->position = tmpl.position;
            if (tmpl.position && !tmpl.defined)
                error_at(line, "%s: POSITION needs DEFINED", names[i]);
            if (tmpl.defined && (level > 1 || stat || tmpl.based || tmpl.init || tmpl.ninits))
                error_at(line, "%s: a DEFINED item is a level-1 name with no storage of its own",
                         names[i]);
            if (tmpl.like && data)
                error_at(line, "%s: LIKE takes no data attributes", names[i]);
            if (stat && tmpl.based)
                error_at(line, "%s: STATIC and BASED", names[i]);
            s->ndims = tmpl.ndims;
            s->stardims = tmpl.stardims;
            s->adjust = tmpl.adjust;
            memcpy(s->lbx, tmpl.lbx, sizeof s->lbx);
            memcpy(s->hbx, tmpl.hbx, sizeof s->hbx);
            s->gen = tmpl.gen;
            s->gel = tmpl.gel;
            s->ngen = tmpl.ngen;
            s->refer_from = tmpl.refer_from;
            s->refer_to = tmpl.refer_to;
            s->refer_dim = tmpl.refer_dim;
            memcpy(s->lb, tmpl.lb, sizeof s->lb);
            memcpy(s->hb, tmpl.hb, sizeof s->hb);
            s->init = tmpl.init;
            s->inits = tmpl.inits;
            s->ninits = tmpl.ninits;
            if (tmpl.initcall && nn > 1)
                error_at(line, "INITIAL CALL for factored names is not supported");
            s->initcall = tmpl.initcall;
            if (level > 1 && depth > 0) {
                struct sym *p = stack[depth - 1], **c = &p->child;
                while (*c)
                    c = &(*c)->sibling;
                *c = s;
                s->parent = p;
                s->root = p->root;
                s->st = p->root->st;
                if (tmpl.based)
                    error_at(line, "%s: BASED belongs on the level-1 name", names[i]);
                if (stat)
                    error_at(line, "%s: STATIC belongs on the level-1 name", names[i]);
                if (ctl)
                    error_at(line, "%s: CONTROLLED belongs on the level-1 name", names[i]);
            }
            s->t.k = TY_NONE;
            if (file || fattr) {
                if (data || entry || hasret || level > 1 || tmpl.ndims)
                    error_at(line, "%s: a file takes no other attributes", names[i]);
                if ((fattr & FL_RECORD) && (fattr & FL_PRINT))
                    error_at(line, "%s: a RECORD file cannot be PRINT", names[i]);
                if (excl && (fattr & FL_STREAM))
                    error_at(line, "%s: EXCLUSIVE and STREAM conflict", names[i]);
                s->is_file = fattr & FL_RECORD ? fattr : fattr | FL_STREAM;
                s->t.k = TY_FILE;
                s->t.p = 4;
                s->var_ef = a_var;
                memcpy(s->env_recfm, env.recfm, sizeof s->env_recfm);
                s->env_blk = env.blk;
                s->env_rec = env.rec;
                s->env_bufs = env.bufs;
                s->env_ncp = env.ncp;
                s->env_iarea = env.iarea;
                s->env_opts = env.opts;
            } else if (entry || hasret) {
                if (extname && !o_asm)
                    error_at(line, "%s: EXTERNAL('name') is for an OPTIONS(ASSEMBLER) entry",
                             names[i]);
                if (o_byval && !o_asm)
                    error_at(line, "%s: OPTIONS(BYVALUE) of an entry is supported with "
                             "ASSEMBLER only", names[i]);
                s->asm_opt = o_asm;
                s->byvalue = o_byval;
                s->retcode = o_retc;
                s->extname = extname;
                if (data || level > 1 || tmpl.ndims)
                    error_at(line, "%s: an ENTRY declaration takes no data attributes", names[i]);
                s->is_entry = 1;
                s->has_desc = tmpl.has_desc;
                s->ndesc = tmpl.ndesc;
                memcpy(s->desc, tmpl.desc, sizeof s->desc);
                s->has_ret = hasret;
                s->ret = tmpl.ret;
                if (a_var) {
                    /* A(the procedure), A(the frame of its parent), as an
                     * ENTRY parameter's value */
                    s->var_ef = 1;
                    s->t.k = TY_ENTRY;
                    s->t.p = 8;
                }
            } else if (!data) {
                /* a structure, if members follow; checked below */
            } else if (islabel) {
                if (fixed || flt || bin || dec || chr || bit || var || ptr || haspic)
                    error_at(line, "%s: LABEL and other data attributes", names[i]);
                if (tmpl.init || tmpl.ninits)
                    error_at(line, "%s: INITIAL of a label is not supported yet", names[i]);
                s->t.k = TY_LABEL;
                s->t.p = 8;
            } else if (haspic) {
                if (fixed || flt || bin || dec || chr || bit || var || ptr)
                    error_at(line, "%s: PICTURE and other data attributes", names[i]);
                if (stat && (tmpl.init || tmpl.ninits) && pict.k == TY_PIC)
                    error_at(line, "%s: STATIC INITIAL of a numeric picture is not supported "
                             "yet", names[i]);
                s->t = pict;
            } else if (area || offs) {
                if (fixed || flt || bin || dec || chr || bit || var || ptr || (area && offs))
                    error_at(line, "%s: conflicting attributes", names[i]);
                if (tmpl.init || tmpl.ninits)
                    error_at(line, "%s: INITIAL of an area or offset is not supported",
                             names[i]);
                if (area) {
                    s->area_dflt = area_dflt;
                    s->t.k = TY_AREA;
                    s->t.p = area;
                } else {
                    s->t.k = TY_OFFSET;
                    s->t.p = 4;
                    s->t.area = offarea;
                }
            } else if (evt) {
                if (fixed || flt || bin || dec || chr || bit || var || ptr || cplx)
                    error_at(line, "%s: conflicting attributes", names[i]);
                s->t.k = TY_EVENT;
                s->t.p = PL1_EV_SIZE;
                if (tmpl.init || tmpl.ninits)
                    error_at(line, "%s: an event takes no INITIAL", names[i]);
            } else if (ptr) {
                if (fixed || flt || bin || dec || chr || bit || var)
                    error_at(line, "%s: conflicting attributes", names[i]);
                s->t.k = TY_PTR;
                s->t.p = 4;
                for (struct sym *q = s; q && !q->aligned; q = q->parent)
                    if (q->unal) {
                        /* IEM3181I (Tier 82, t226fct) */
                        beyond_f(line, L_OPT, "an UNALIGNED pointer");
                        break;
                    }
            } else if (bit) {
                if (fixed || bin || dec || chr)
                    error_at(line, "%s: conflicting attributes", names[i]);
                if (flt)
                    error_at(line, "%s: conflicting attributes", names[i]);
                s->t.k = var ? TY_VBIT : TY_BIT;
                s->t.p = clen;
                if (clen < 1 || clen > MAXBITS)
                    error_at(line, "%s: BIT(n) with n from 1 to %d", names[i], MAXBITS);
            } else if (chr) {
                if (fixed || flt || bin || dec)
                    error_at(line, "%s: conflicting attributes", names[i]);
                s->t.k = var ? TY_VCHAR : TY_CHAR;
                s->t.p = clen;
            } else if (var) {
                error_at(line, "%s: VARYING needs CHARACTER or BIT", names[i]);
            } else if (fixed && bin) {
                s->t.k = TY_FIXBIN;
                s->t.p = prec < 0 ? 15 : prec;
                s->t.q = scale;               /* a scaled binary (Q1SBIN) */
                if (scale < 0 || scale > 31)
                    error_at(line, "%s: a FIXED BINARY scale from 0 to 31 is supported",
                             names[i]);
                if (s->t.p < 1 || s->t.p > 31)
                    error_at(line, "%s: FIXED BINARY precision must be 1 to 31", names[i]);
            } else if (fixed) {
                s->t.k = TY_FIXDEC;
                s->t.p = prec < 0 ? 5 : prec;
                s->t.q = scale;
                if (s->t.p < 1 || s->t.p > 15 || scale < 0 || scale > s->t.p)
                    error_at(line, "%s: FIXED DECIMAL(p,q) needs 1 <= p <= 15, 0 <= q <= p", names[i]);
            } else if (fixed || flt || bin || dec) {
                /* FLOAT, or BINARY or DECIMAL without FIXED: FLOAT */
                arith_type(&s->t, 1, bin, prec, scale, names[i], line);
            } else if (cplx) {
                /* COMPLEX alone: the default by the first letter */
                int letter = names[i][0] >= 'I' && names[i][0] <= 'N';
                arith_type(&s->t, !letter, letter, -1, 0, names[i], line);
            }
            if (cplx) {
                if (s->t.k != TY_FIXBIN && s->t.k != TY_FIXDEC && s->t.k != TY_FLTDEC &&
                    s->t.k != TY_FLTBIN)
                    error_at(line, "%s: COMPLEX takes FIXED or FLOAT data%s", names[i],
                             haspic ? " (COMPLEX PICTURE is not supported yet)" : "");
                else
                    s->t = cplx_type(s->t);
            }
            if (!(file || entry || hasret || islabel || area || offs || tmpl.like ||
                  tmpl.defined)) {
                /* what DEFAULT may complete (default_pass) */
                struct rawattr *a = &s->raw;
                memset(a, 0, sizeof *a);
                a->fixed = fixed; a->flt = flt; a->bin = bin; a->dec = dec;
                a->chr = chr; a->bit = bit; a->var = var; a->ptr = ptr;
                a->haspic = haspic; a->pict = pict;
                a->prec = prec; a->scale = scale; a->clen = clen; a->hasclen = clen_given;
                s->hasraw = 1;
            }
            if (nn == 1) {
                if (depth == 16)
                    fatal("%s(%d): structure too deep", src_name, line);
                stack[depth++] = s;
            }
        }
        if (is(T_COMMA)) { pos++; continue; }
        break;
    }
    expect(T_SEMI, "';'");
    /* an item without data attributes must be a structure */
    for (struct sym *s = cur->syms; s; s = s->next)
        if (s->t.k == TY_NONE && !s->child && !s->implicit && !s->synthetic && !s->is_entry &&
            !s->is_file && !s->like)
            /* no attributes: FIXED BINARY(15) for I to N, else FLOAT DECIMAL(6) */
            arith_type(&s->t, !(s->name[0] >= 'I' && s->name[0] <= 'N'),
                       s->name[0] >= 'I' && s->name[0] <= 'N', -1, 0, s->name, s->line);
        else if (s->t.k != TY_NONE && s->child)
            error_at(s->line, "%s: a structure has no data attributes", s->name);
}

/* ---- DEFAULT (the Optimizing Compiler, GC33-0009-4) ------------------- */

/* attributes of a specification: data attributes and VALUE(...) */
static void default_attrs(struct dflt *d)
{
    struct rawattr *a = &d->a;
    while (is(T_IDENT)) {
        if (isword("FIXED")) a->fixed = 1;
        else if (isword("FLOAT")) a->flt = 1;
        else if (isword2("BIN", "BINARY")) a->bin = 1;
        else if (isword2("DEC", "DECIMAL")) a->dec = 1;
        else if (isword2("CHAR", "CHARACTER")) a->chr = 1;
        else if (isword("BIT")) a->bit = 1;
        else if (isword2("VAR", "VARYING")) a->var = 1;
        else if (isword2("POINTER", "PTR")) a->ptr = 1;
        else if (isword("REAL") || isword("ALIGNED") || isword2("UNAL", "UNALIGNED") ||
                 isword2("AUTO", "AUTOMATIC") || isword2("NONVAR", "NONVARYING")) ;
        else if (isword2("PIC", "PICTURE")) {
            pos++;
            if (!is(T_STR))
                fatal("%s(%d): PICTURE needs the picture as a string", src_name, tk(0)->line);
            picture(tk(0)->str, tk(0)->len, tk(0)->line, &a->pict);
            a->haspic = 1;
        }
        else if (isword("VALUE")) {
            /* VALUE(FIXED BINARY(p), FLOAT DECIMAL(p), CHARACTER(n), ...) */
            pos++;
            expect(T_LP, "'('");
            for (;;) {
                int fx = 0, fl = 0, bi = 0, de = 0, ch = 0, bt = 0, ar = 0, n, q = 0,
                    line = tk(0)->line;
                struct type t;
                while (is(T_IDENT) && !is(T_LP)) {
                    if (isword("FIXED")) fx = 1;
                    else if (isword("FLOAT")) fl = 1;
                    else if (isword2("BIN", "BINARY")) bi = 1;
                    else if (isword2("DEC", "DECIMAL")) de = 1;
                    else if (isword2("CHAR", "CHARACTER")) ch = 1;
                    else if (isword("BIT")) bt = 1;
                    else if (isword("AREA")) ar = 1;
                    else
                        fatal("%s(%d): VALUE takes base, scale, CHARACTER and BIT", src_name,
                              line);
                    pos++;
                }
                n = number_in_parens(&q);
                memset(&t, 0, sizeof t);
                if (ar) {
                    t.k = TY_AREA;
                    t.p = n;
                } else if (ch || bt) {
                    t.k = ch ? TY_CHAR : TY_BIT;
                    t.p = n;
                } else if (fx || fl || bi || de) {
                    /* base and scale both, as GC33-0009-4 requires */
                    if (!(fx || fl) || !(bi || de))
                        fatal("%s(%d): VALUE needs a base and a scale with a precision",
                              src_name, line);
                    t.k = fx ? (bi ? TY_FIXBIN : TY_FIXDEC) : (bi ? TY_FLTBIN : TY_FLTDEC);
                    t.p = n;
                    t.q = q;
                } else {
                    fatal("%s(%d): VALUE takes base, scale, CHARACTER and BIT", src_name, line);
                }
                if (d->nv == 8)
                    fatal("%s(%d): too many VALUE items", src_name, line);
                d->vt[d->nv++] = t;
                if (is(T_COMMA)) { pos++; continue; }
                break;
            }
            expect(T_RP, "')'");
            continue;
        }
        else
            fatal("%s(%d): attribute %s is not supported in DEFAULT yet", src_name,
                  tk(0)->line, tk(0)->text);
        pos++;
    }
}

/* a specification or a parenthesized (factored) list of them; the
 * attributes after a factored list go to each one in it */
static void default_spec(struct dflt **list)
{
    struct dflt *first = *list;
    if (is(T_LP)) {
        pos++;
        for (;;) {
            default_spec(list);
            if (is(T_COMMA)) { pos++; continue; }
            break;
        }
        expect(T_RP, "')'");
        {
            struct dflt tmp;
            memset(&tmp, 0, sizeof tmp);
            default_attrs(&tmp);
            for (struct dflt *d = *list; d != first; d = d->next) {
                struct rawattr *a = &d->a, *b = &tmp.a;
                a->fixed |= b->fixed; a->flt |= b->flt; a->bin |= b->bin; a->dec |= b->dec;
                a->chr |= b->chr; a->bit |= b->bit; a->var |= b->var; a->ptr |= b->ptr;
                if (b->haspic) { a->haspic = 1; a->pict = b->pict; }
                for (int i = 0; i < tmp.nv && d->nv < 8; i++)
                    d->vt[d->nv++] = tmp.vt[i];
            }
        }
        return;
    }
    if (isword("DESCRIPTORS"))
        fatal("%s(%d): DEFAULT DESCRIPTORS is not supported yet", src_name, tk(0)->line);
    if (!isword("RANGE"))
        fatal("%s(%d): DEFAULT expects RANGE(...)", src_name, tk(0)->line);
    {
        struct dflt *d = xmalloc(sizeof *d);
        pos++;
        expect(T_LP, "'('");
        for (;;) {
            if (is(T_STAR)) {
                d->all = 1;
                pos++;
            } else if (is(T_IDENT) && tk(1)->kind == T_COLON && tk(2)->kind == T_IDENT) {
                if (d->nr == 16)
                    fatal("%s(%d): too many ranges", src_name, tk(0)->line);
                d->r[d->nr].lo = tk(0)->text[0];
                d->r[d->nr].hi = tk(2)->text[0];
                d->nr++;
                pos += 3;
            } else if (is(T_IDENT)) {
                if (d->nr == 16)
                    fatal("%s(%d): too many ranges", src_name, tk(0)->line);
                d->r[d->nr].id = tk(0)->text;
                d->nr++;
                pos++;
            } else {
                fatal("%s(%d): RANGE takes identifiers, letter:letter or *", src_name,
                      tk(0)->line);
            }
            if (is(T_COMMA)) { pos++; continue; }
            break;
        }
        expect(T_RP, "')'");
        default_attrs(d);
        d->next = *list;
        *list = d;
    }
}

/* DEFAULT spec [, spec]...;  the specifications of the block, the last
 * written first (default_pass searches them in order) */
static void default_stmt(void)
{
    struct dflt *list = NULL, **tail;
    pos++;
    for (;;) {
        default_spec(&list);
        if (is(T_COMMA)) { pos++; continue; }
        break;
    }
    expect(T_SEMI, "';'");
    /* keep the written order: reverse, then append to the block's */
    {
        struct dflt *r = NULL;
        while (list) {
            struct dflt *n = list->next;
            list->next = r;
            r = list;
            list = n;
        }
        for (tail = &cur->defaults; *tail; tail = &(*tail)->next)
            ;
        *tail = r;
    }
}

static int in_range(const struct dflt *d, const char *name)
{
    if (d->all)
        return 1;
    for (int i = 0; i < d->nr; i++) {
        if (d->r[i].id) {
            size_t n = strlen(d->r[i].id);
            if (strncmp(name, d->r[i].id, n) == 0)
                return 1;
        } else if (name[0] >= d->r[i].lo && name[0] <= d->r[i].hi) {
            return 1;
        }
    }
    return 0;
}

/* the type of attributes a: the standard rules after DEFAULT's */
static void raw_type(struct sym *s, const struct rawattr *a)
{
    int letter = s->name[0] >= 'I' && s->name[0] <= 'N';
    memset(&s->t, 0, sizeof s->t);
    if (a->haspic) {
        s->t = a->pict;
    } else if (a->ptr) {
        s->t.k = TY_PTR;
        s->t.p = 4;
    } else if (a->bit) {
        s->t.k = a->var ? TY_VBIT : TY_BIT;
        s->t.p = a->clen;
    } else if (a->chr) {
        s->t.k = a->var ? TY_VCHAR : TY_CHAR;
        s->t.p = a->clen;
    } else if (!(a->fixed || a->flt || a->bin || a->dec)) {
        /* no base and no scale: by the first letter */
        arith_type(&s->t, !letter, letter, a->prec, a->scale, s->name, s->line);
    } else if (a->fixed && a->bin) {
        s->t.k = TY_FIXBIN;
        s->t.p = a->prec < 0 ? 15 : a->prec;
    } else if (a->fixed) {
        s->t.k = TY_FIXDEC;
        s->t.p = a->prec < 0 ? 5 : a->prec;
        s->t.q = a->scale;
    } else {
        arith_type(&s->t, 1, a->bin, a->prec, a->scale, s->name, s->line);
    }
}

/* a completed by d: only the classes a lacks (GC33-0009-4, DEFAULT) */
static void merge_default(struct rawattr *a, const struct dflt *d, const char *name)
{
    const struct rawattr *b = &d->a;
    int letter = name[0] >= 'I' && name[0] <= 'N';
    if (!(a->fixed || a->flt || a->bin || a->dec || a->chr || a->bit || a->ptr || a->haspic)) {
        /* no type yet: DEFAULT's string, locator or picture attribute */
        if (b->chr || b->bit) {
            a->chr = b->chr;
            a->bit = !b->chr && b->bit;
        } else if (b->ptr) {
            a->ptr = 1;
        } else if (b->haspic) {
            a->haspic = 1;
            a->pict = b->pict;
        }
    }
    if (a->chr || a->bit) {
        if (!a->var && b->var)
            a->var = 1;
        if (!a->hasclen)
            for (int i = 0; i < d->nv; i++)
                if (d->vt[i].k == (a->chr ? TY_CHAR : TY_BIT)) {
                    a->clen = d->vt[i].p;
                    a->hasclen = 1;
                }
        return;
    }
    if (a->ptr || a->haspic)
        return;
    if (!a->bin && !a->dec) {
        a->bin = b->bin;
        a->dec = b->dec;
    }
    if (!a->fixed && !a->flt) {
        a->fixed = b->fixed;
        a->flt = b->flt;
    }
    if (a->prec < 0) {
        /* VALUE: the precision of the base and scale it now has */
        int fx, bi;
        enum tkind k;
        if (!(a->fixed || a->flt || a->bin || a->dec)) {
            fx = letter;
            bi = letter;
        } else {
            fx = a->fixed;
            bi = a->bin;
        }
        k = fx ? (bi ? TY_FIXBIN : TY_FIXDEC) : (bi ? TY_FLTBIN : TY_FLTDEC);
        for (int i = 0; i < d->nv; i++)
            if (d->vt[i].k == k) {
                a->prec = d->vt[i].p;
                a->scale = d->vt[i].q;
                if (!(a->fixed || a->flt || a->bin || a->dec)) {
                    a->fixed = fx;
                    a->flt = !fx;
                    a->bin = bi;
                    a->dec = !bi;
                }
            }
    }
}

static const struct dflt *find_default(struct proc *scope, const char *name, int only)
{
    for (struct proc *b = scope; b; b = only ? NULL : b->parent)
        for (const struct dflt *d = b->defaults; d; d = d->next)
            if (in_range(d, name))
                return d;
    return NULL;
}

/* The attributes of s after DEFAULT: an explicit declaration takes the
 * DEFAULT of its own block or the nearest containing one whose range
 * holds the name; an implicit one only the main procedure's. */
void default_sym(struct sym *s, struct proc *scope)
{
    const struct dflt *d = find_default(scope, s->name, s->implicit);
    struct rawattr a;
    if (!d)
        return;
    if (s->implicit) {
        memset(&a, 0, sizeof a);
        a.prec = -1;
        a.clen = 1;
    } else {
        a = s->raw;
    }
    merge_default(&a, d, s->name);
    if (!a.hasclen)
        a.clen = 1;
    raw_type(s, &a);
}

static void default_pass(struct proc *p)
{
    for (struct sym *s = p->syms; s; s = s->next) {
        if (s->hasraw && !s->child && !s->is_entry && !s->is_file && !s->implicit)
            default_sym(s, p);
        if (s->t.k == TY_AREA && s->area_dflt) {
            /* AREA without a size: VALUE(AREA(n)) of its DEFAULT */
            const struct dflt *d = find_default(p, s->name, 0);
            for (int i = 0; d && i < d->nv; i++)
                if (d->vt[i].k == TY_AREA)
                    s->t.p = d->vt[i].p;
        }
    }
    for (struct proc *c = p->child; c; c = c->sibling)
        default_pass(c);
}

/* ---- statements ----------------------------------------------------- */

static struct stmt *statement(void);

static struct stmt *new_stmt(enum skind k, int from)
{
    struct stmt *s = xmalloc(sizeof *s);
    memset(s, 0, sizeof *s);
    s->k = k;
    s->no = ++stmt_no;
    s->line = L->toks[from].line;
    return s;
}

static void finish(struct stmt *s, int from)
{
    s->text = stmt_text(from, pos - 1);
}

static struct proc *parse_proc(int from, struct proc *parent);
static int prefix(int *on, int *off);

/* Statements up to the END that closes a group; the END is consumed.
 * A PROCEDURE statement inside opens an internal procedure, which is a
 * block of its own, not a statement of this one. */
/* The groups and blocks open while the parser reads them, with the
 * labels of their DO, BEGIN or PROCEDURE statement: END X closes the
 * nearest one labelled X and every one inside it (GC28-8201-4, END). */
static struct { char *labels[MAXLABELS]; int n; } gstack[64];
static int ngstack;
static char **glabels;               /* the labels of the statement now read */
static int nglabels;

static int open_label(const char *x, int below)
{
    for (int g = below - 1; g >= 0; g--)
        for (int i = 0; i < gstack[g].n; i++)
            if (strcmp(gstack[g].labels[i], x) == 0)
                return g;
    return -1;
}

static struct stmt *group_body(void)
{
    struct stmt *head = NULL, **tail = &head;
    int me = ngstack;
    if (ngstack == 64)
        fatal("%s: groups nested more than 64 deep", src_name);
    gstack[me].n = 0;
    for (int i = 0; i < nglabels && i < MAXLABELS; i++)
        gstack[me].labels[gstack[me].n++] = glabels[i];
    nglabels = 0;
    ngstack++;
    for (;;) {
        int from = pos;
        char *endlabel = NULL;
        if (is(T_EOF))
            fatal("%s: END missing at end of program", src_name);
        if (is(T_IDENT) && tk(1)->kind == T_COLON && tk(2)->kind == T_IDENT &&
            strcmp(tk(2)->text, "END") == 0 && tk(3)->kind != T_EQ) {
            endlabel = tk(0)->text;          /* label on END */
            pos += 2;
        }
        if (isword("END") && tk(1)->kind != T_EQ) {
            if (tk(1)->kind == T_IDENT) {
                int g = open_label(tk(1)->text, ngstack);
                if (g >= 0 && g < me) {
                    /* END X of a group around this one: it ends here too,
                     * and the END is read again by the group it names */
                    pos = from;
                    ngstack = me;
                    return head;
                }
            }
            ngstack = me;
            pos++;
            if (is(T_IDENT))
                pos++;                       /* END label */
            expect(T_SEMI, "';'");
            if (endlabel) {
                /* a GO TO to it goes to the end of the group: a null
                 * statement there carries the label */
                struct stmt *n = new_stmt(S_NULL, from);
                n->labels[0] = endlabel;
                n->nlabels = 1;
                finish(n, from);
                *tail = n;
                tail = &n->next;
                return head;
            }
            stmt_no++;
            return head;
        }
        pos = from;
        {
            int pon = 0, poff = 0;
            if (prefix(&pon, &poff) && is(T_IDENT) && tk(1)->kind == T_COLON &&
                tk(2)->kind == T_IDENT && (strcmp(tk(2)->text, "PROC") == 0 ||
                strcmp(tk(2)->text, "PROCEDURE") == 0)) {
                char **ck = pfx_checks;
                int nck = pfx_nchecks;
                struct proc *q = parse_proc(pos, cur);
                q->pon = pon;
                q->poff = poff;
                q->checks = ck;
                q->nchecks = nck;
                continue;
            }
            pos = from;
        }
        if (is(T_IDENT) && tk(1)->kind == T_COLON && tk(2)->kind == T_IDENT &&
            (strcmp(tk(2)->text, "PROC") == 0 || strcmp(tk(2)->text, "PROCEDURE") == 0) &&
            tk(3)->kind != T_EQ) {
            parse_proc(from, cur);
            continue;
        }
        {
            struct stmt *s = statement();
            if (s) {
                *tail = s;
                tail = &s->next;
            }
        }
    }
}

static void parse_specs(struct dospec **tail);

/* A reference -- names, subscripts in parentheses, '.', '->' -- followed
 * by '=': the control variable of an iterative DO. */
static int ref_then_eq(void)
{
    int j = 0, depth = 0;
    for (;;) {
        enum tok k = tk(j)->kind;
        if (k == T_SEMI || k == T_EOF)
            return 0;
        if (depth == 0 && k == T_EQ)
            return j > 0;
        if (k == T_LP)
            depth++;
        else if (k == T_RP && --depth < 0)
            return 0;
        else if (depth == 0 && k != T_IDENT && k != T_DOT && k != T_ARROW && k != T_RP)
            return 0;
        j++;
    }
}

static struct stmt *do_stmt(int from)
{
    struct stmt *s;
    pos++;                                   /* DO */
    if (is(T_SEMI)) {
        pos++;
        s = new_stmt(S_GROUP, from);
        finish(s, from);
        s->body = group_body();
        return s;
    }
    if ((isword("WHILE") || isword("UNTIL")) && tk(1)->kind == T_LP) {
        /* DO WHILE(e) [UNTIL(u)];  DO UNTIL(u) [WHILE(e)]; (UNTIL: the
         * Optimizing Compiler, GC33-0009-4) */
        s = new_stmt(S_WHILE, from);
        for (;;) {
            if (isword("WHILE") && !s->e && tk(1)->kind == T_LP) {
                pos += 2;
                s->e = expr();
                expect(T_RP, "')'");
            } else if (isword("UNTIL") && !s->e2 && tk(1)->kind == T_LP) {
                beyond_f(tk(0)->line, L_OPT, "UNTIL");
                pos += 2;
                s->e2 = expr();
                expect(T_RP, "')'");
            } else {
                break;
            }
        }
        expect(T_SEMI, "';'");
        finish(s, from);
        s->body = group_body();
        return s;
    }
    if (is(T_IDENT) && ref_then_eq()) {
        s = new_stmt(S_ITER, from);
        struct dospec **tail;
        s->iter_ref = parse_ref();
        pos++;                               /* = */
        tail = &s->specs;
        parse_specs(tail);
        if (!is(T_SEMI))
            fatal("%s(%d): TO, BY, REPEAT, WHILE, UNTIL or ',' expected", src_name,
                  tk(0)->line);
        pos++;
        finish(s, from);
        s->body = group_body();
        if (s->iter_ref->nsubs || s->iter_ref->qual) {
            /* DO A(I) = e [WHILE(w)]; an element as the control variable
             * of one specification without TO, BY or REPEAT: executed
             * once, as A(I) = e; [IF w THEN] DO; ...; END; (RXCOMM's
             * DO ARG(AI) = ...; foo, PL/I (F) takes it) */
            struct dospec *d = s->specs;
            struct stmt *a, *g;
            if (!d || d->next || d->e2 || d->e3 || d->rep || d->u)
                fatal("%s(%d): a subscripted control variable with TO, BY, REPEAT, UNTIL or "
                      "more than one specification is not supported yet", src_name, s->line);
            a = new_stmt(S_ASSIGN, from);
            a->lhs = s->iter_ref;
            a->e = d->e1;
            a->line = s->line;
            g = new_stmt(S_GROUP, from);
            g->line = s->line;
            g->body = s->body;
            if (d->w) {
                struct stmt *f = new_stmt(S_IF, from);
                f->line = s->line;
                f->e = d->w;
                f->body = g;
                g = f;
            }
            a->next = g;
            s->k = S_GROUP;
            s->iter_ref = NULL;
            s->specs = NULL;
            s->body = a;
        }
        return s;
    }
    fatal("%s(%d): this form of DO is not supported yet", src_name, tk(0)->line);
    return NULL;
}

/* specifications: e1 [TO e2] [BY e3] [WHILE(w)], separated by commas */
static void parse_specs(struct dospec **tail)
{
    {
        for (;;) {
            struct dospec *d = xmalloc(sizeof *d);
            d->e1 = expr();
            for (;;) {
                if (isword("TO") && !d->e2 && !d->rep) { pos++; d->e2 = expr(); }
                else if (isword("BY") && !d->e3 && !d->rep) { pos++; d->e3 = expr(); }
                else if (isword("REPEAT") && !d->rep && !d->e2 && !d->e3) {
                    beyond_f(tk(0)->line, L_OPT, "REPEAT");
                    pos++;
                    d->rep = expr();
                }
                else if (isword("UNTIL") && !d->u && tk(1)->kind == T_LP) {
                    beyond_f(tk(0)->line, L_OPT, "UNTIL");
                    pos += 2;
                    d->u = expr();
                    expect(T_RP, "')'");
                }
                else if (isword("WHILE") && !d->w && tk(1)->kind == T_LP) {
                    pos += 2;
                    d->w = expr();
                    expect(T_RP, "')'");
                }
                else break;
            }
            *tail = d;
            tail = &d->next;
            if (is(T_COMMA)) { pos++; continue; }
            break;
        }
    }
}

/* FILE(name) */
static struct expr *file_option(void)
{
    struct expr *f;
    pos++;                                   /* FILE */
    expect(T_LP, "'('");
    f = parse_ref();
    expect(T_RP, "')'");
    return f;
}

/* A format item's width or d given by an expression (Q1FEXP): it is
 * assigned to a variable of its own just before the statement, which
 * then runs in a DO group with the assignments (fmt_pre). */
static struct stmt *fmt_pre, **fmt_pre_tail = &fmt_pre;
static int nfmtvar;

static struct sym *format_expr(void)
{
    char name[16];
    struct sym *v;
    struct stmt *a = new_stmt(S_ASSIGN, pos);
    struct expr *r;
    snprintf(name, sizeof name, "#F%d", ++nfmtvar);
    v = add_sym(name, tk(0)->line);
    v->t.k = TY_FIXBIN;
    v->t.p = 31;
    v->st = ST_AUTO;
    v->implicit = 0;
    r = new_expr(E_VAR, tk(0)->line);
    r->qn = xmalloc(sizeof *r->qn);
    r->qn[0] = v->name;
    r->nq = 1;
    r->scope = cur;
    a->lhs = r;
    a->e = expr();
    a->text = "(a format item's width)";
    *fmt_pre_tail = a;
    fmt_pre_tail = &a->next;
    return v;
}

/* A number in a format item, or an expression (*v set, 0 returned). */
static int format_value(struct sym **v)
{
    int n;
    *v = NULL;
    if (is(T_NUM) && (tk(1)->kind == T_RP || tk(1)->kind == T_COMMA)) {
        n = atoi(tk(0)->text);
        pos++;
        return n;
    }
    *v = format_expr();
    return 0;
}

static int format_number_v(struct sym **v)
{
    expect(T_LP, "'('");
    return format_value(v);
}

/* The statement with the assignments of its format widths before it,
 * in a DO group, if it has any */
static struct stmt *with_fmt_pre(struct stmt *st, int from)
{
    struct stmt *g;
    if (!fmt_pre)
        return st;
    g = new_stmt(S_GROUP, from);
    g->text = st->text;
    *fmt_pre_tail = st;
    g->body = fmt_pre;
    fmt_pre = NULL;
    fmt_pre_tail = &fmt_pre;
    return g;
}

static int format_number(void)
{
    int v;
    expect(T_LP, "'('");
    if (!is(T_NUM))
        fatal("%s(%d): a constant expected in the format item", src_name, tk(0)->line);
    v = atoi(tk(0)->text);
    pos++;
    return v;
}

/* whether the parenthesis at pos encloses an iteration factor given by
 * an expression: a format item or list follows its closing parenthesis
 * ((N) F(4), (N - 1) (A, X(1))); a group of items is followed by a comma
 * or a parenthesis that closes */
static int rep_expr_ahead(void)
{
    int depth = 0, i = 0;
    for (;; i++) {
        struct token *t = tk(i);
        if (t->kind == T_EOF || t->kind == T_SEMI)
            return 0;
        if (t->kind == T_LP)
            depth++;
        else if (t->kind == T_RP && --depth == 0)
            break;
    }
    return tk(i + 1)->kind == T_IDENT || tk(i + 1)->kind == T_LP;
}

static int rep_depth;               /* (expression) factors open */

/* (item, ...): items with iteration factors (n) and groups, expanded;
 * an iteration factor given by an expression a F_REP item before the
 * item or group it repeats (rt/pl1ioed.c next_data) */
static void format_list(struct fmt *out, int *n)
{
    expect(T_LP, "'('");
    for (;;) {
        int rep = 1, start = *n, erep = -1;
        if (is(T_LP) && !(tk(1)->kind == T_NUM && tk(2)->kind == T_RP) && rep_expr_ahead()) {
            struct fmt f;
            memset(&f, 0, sizeof f);
            f.k = F_REP;
            pos++;
            f.ws = format_expr();
            expect(T_RP, "')'");
            if (*n >= MAXFMT - 1)
                fatal("%s(%d): format list too long", src_name, tk(0)->line);
            if (++rep_depth > 8)
                fatal("%s(%d): iteration factors given by expressions are nested more than "
                      "8 deep", src_name, tk(0)->line);
            erep = (*n)++;
            out[erep] = f;
            start = *n;
        } else if (is(T_LP) && tk(1)->kind == T_NUM && tk(2)->kind == T_RP &&
            tk(3)->kind != T_STR && tk(3)->kind != T_BITC) {
            /* an iteration factor; before a string it is the string's
             * repetition factor: CHAR(8) INIT((3)'AB') is 'ABABAB', and
             * (2)'Q' one element (Q3INA, Q6IND) */
            rep = atoi(tk(1)->text);
            pos += 3;
        } else if (is(T_NUM) && (tk(1)->kind == T_IDENT || tk(1)->kind == T_LP)) {
            /* an iteration factor without parentheses: 3 F(8,2), 8F(10,2)
             * (PL/I (F) takes it, IEM0009I: Q1RFMT) */
            rep = atoi(tk(0)->text);
            pos++;
        }
        if (is(T_LP)) {
            format_list(out, n);
        } else {
            struct fmt f;
            memset(&f, 0, sizeof f);
            if (!is(T_IDENT))
                fatal("%s(%d): format item expected", src_name, tk(0)->line);
            if (isword("A")) {
                pos++;
                f.k = F_A;
                if (is(T_LP)) { f.k = F_AW; f.w = format_number_v(&f.ws); expect(T_RP, "')'"); }
            } else if (isword("R") && tk(1)->kind == T_LP) {
                /* R(label): the format list of that FORMAT statement */
                pos += 2;
                if (!is(T_IDENT) || tk(1)->kind != T_RP)
                    fatal("%s(%d): R takes the label of a FORMAT statement", src_name,
                          tk(0)->line);
                f.k = F_R;
                f.pic = tk(0)->text;
                pos += 2;
            } else if (isword("B") || isword("B1") || isword("B2") || isword("B3") ||
                       isword("B4")) {
                /* B, B(w): a bit string as its characters 1 and 0 (Q1BFMT);
                 * ANSI's B2, B3, B4 its digits of 2, 3, 4 bits (CM145), the
                 * radix in d */
                if (tk(0)->text[1])
                    beyond_f(tk(0)->line, L_ISO, "the format item B1, B2, B3 or B4");
                f.d = tk(0)->text[1] ? tk(0)->text[1] - '0' : 1;
                pos++;
                f.k = F_B;
                if (is(T_LP)) { f.k = F_BW; f.w = format_number_v(&f.ws); expect(T_RP, "')'"); }
            } else if (isword("F")) {
                pos++;
                f.k = F_F;
                f.w = format_number_v(&f.ws);
                if (is(T_COMMA)) {
                    pos++;
                    f.d = format_value(&f.ds);
                }
                expect(T_RP, "')'");
            } else if (isword("E")) {
                /* E(w,d[,s]): s significant digits, d of them after the point */
                int d, sd;
                pos++;
                f.k = F_E;
                f.w = format_number_v(&f.ws);
                expect(T_COMMA, "','");
                d = format_value(&f.ds);
                sd = d + 1;
                f.sd = -1;
                if (is(T_COMMA)) {
                    pos++;
                    if (!is(T_NUM))
                        fatal("%s(%d): a constant expected in E(w,d,s)", src_name, tk(0)->line);
                    sd = atoi(tk(0)->text);
                    f.sd = sd;
                    pos++;
                }
                expect(T_RP, "')'");
                if (f.ws || f.ds)
                    ;                     /* checked at run time, not here */
                else if (sd < d || sd > 16 || sd < 1)
                    error_at(tk(0)->line, "E(w,d,s) needs d <= s <= 16");
                else if (f.w < sd + (d ? 1 : 0) + 4)
                    /* no room for the digits, the point and E+xx: PL/I (F)
                     * deletes such an item (IEM1871I, JOB00157); with no room
                     * for a minus sign it only warns (IEM1872I) */
                    error_at(tk(0)->line, "E(%d,%d,%d) is too narrow", f.w, d, sd);
                f.d = d + 256 * sd;
            } else if (isword("P") && tk(1)->kind == T_STR) {
                /* P'picture': a numeric picture (rt/pl1io.c K_P) */
                struct type pt;
                pos++;
                picture(tk(0)->str, tk(0)->len, tk(0)->line, &pt);
                if (pt.k != TY_PIC)
                    fatal("%s(%d): a P format item with a character-string picture is not "
                          "supported yet", src_name, tk(0)->line);
                f.k = F_P;
                f.pic = pt.pic;
                pos++;
            } else if (isword("C") && tk(1)->kind == T_LP) {
                /* C(item[,item]): a complex value's parts, each an F, E or P
                 * item, the second the first again if not given (GC28-8201-4,
                 * "The C Format Item"); F_C, then its two items */
                int c0;
                pos++;
                if (*n >= MAXFMT - 3)
                    fatal("%s(%d): format list too long", src_name, tk(0)->line);
                f.k = F_C;
                out[(*n)++] = f;
                c0 = *n;
                format_list(out, n);
                if (*n - c0 < 1 || *n - c0 > 2)
                    fatal("%s(%d): C takes one or two format items", src_name, tk(0)->line);
                for (int i = c0; i < *n; i++)
                    if (out[i].k != F_F && out[i].k != F_E && out[i].k != F_P)
                        fatal("%s(%d): C takes F, E and P format items", src_name,
                              tk(0)->line);
                if (*n - c0 == 1)
                    out[(*n)++] = out[c0];
                goto c_done;
            } else if (isword("X")) {
                pos++; f.k = F_X; f.w = format_number_v(&f.ws); expect(T_RP, "')'");
            } else if (isword("COLUMN") || isword("COL")) {
                pos++; f.k = F_COL; f.w = format_number_v(&f.ws); expect(T_RP, "')'");
            } else if (isword("SKIP")) {
                pos++; f.k = F_SKIP; f.w = 1;
                if (is(T_LP)) { f.w = format_number_v(&f.ws); expect(T_RP, "')'"); }
            } else if (isword("TAB")) {
                /* ANSI's TAB [(w)]: w tab positions on, 1 if none (CM147) */
                beyond_f(tk(0)->line, L_ISO, "the format item TAB");
                pos++; f.k = F_TAB; f.w = 1;
                if (is(T_LP)) { f.w = format_number_v(&f.ws); expect(T_RP, "')'"); }
            } else if (isword("PAGE")) {
                pos++; f.k = F_PAGE;
            } else if (isword("LINE")) {
                pos++; f.k = F_LINE; f.w = format_number_v(&f.ws); expect(T_RP, "')'");
            } else {
                fatal("%s(%d): format item %s is not supported yet", src_name, tk(0)->line,
                      tk(0)->text);
            }
            if (*n >= MAXFMT - 1)
                fatal("%s(%d): format list too long", src_name, tk(0)->line);
            out[(*n)++] = f;
        }
    c_done:
        {
            int len = *n - start;
            for (int r = 1; r < rep; r++)
                for (int i = 0; i < len; i++) {
                    if (*n >= MAXFMT - 1)
                        fatal("%s(%d): format list too long", src_name, tk(0)->line);
                    out[(*n)++] = out[start + i];
                }
        }
        if (erep >= 0) {
            out[erep].d = *n - start;       /* the items it repeats */
            rep_depth--;
        }
        if (is(T_COMMA)) { pos++; continue; }
        break;
    }
    expect(T_RP, "')'");
}

/* (item, ...) of a data list */
/* whether the parenthesis at pos opens a repetitive specification:
 * (items DO v = ...) -- a DO at its own level */
static int repetitive_ahead(void)
{
    int depth = 0;
    for (int i = pos; tk(i - pos)->kind != T_EOF && tk(i - pos)->kind != T_SEMI; i++) {
        struct token *t = tk(i - pos);
        if (t->kind == T_LP)
            depth++;
        else if (t->kind == T_RP && --depth == 0)
            return 0;
        else if (depth == 1 && t->kind == T_IDENT && strcmp(t->text, "DO") == 0)
            return 1;
    }
    return 0;
}

static struct put_item *data_item(int targets)
{
    struct put_item *it = xmalloc(sizeof *it);
    memset(it, 0, sizeof *it);
    if (is(T_LP) && repetitive_ahead()) {
        /* (items DO v = e1 TO e2 ...): the items for each value of v,
         * nested ones too (Q1RFMT) */
        struct put_item **t = &it->sub;
        int line = tk(0)->line;
        pos++;
        for (;;) {
            *t = data_item(targets);
            t = &(*t)->next;
            if (is(T_COMMA)) { pos++; continue; }
            break;
        }
        if (!isword("DO"))
            fatal("%s(%d): DO expected in the repetitive specification", src_name, tk(0)->line);
        pos++;
        it->loop = new_stmt(S_ITER, pos);
        it->loop->line = line;
        it->loop->iter_ref = parse_ref();
        expect(T_EQ, "'='");
        parse_specs(&it->loop->specs);
        expect(T_RP, "')'");
        return it;
    }
    it->e = targets ? parse_ref() : expr();
    return it;
}

static void data_list(struct put_item ***tail, int targets)
{
    expect(T_LP, "'('");
    for (;;) {
        struct put_item *it = data_item(targets);
        **tail = it;
        *tail = &it->next;
        if (is(T_COMMA)) { pos++; continue; }
        break;
    }
    expect(T_RP, "')'");
}

static void edit_formats(struct stmt *s)
{
    s->fmts = xmalloc(MAXFMT * sizeof *s->fmts);
    s->nfmts = 0;
    format_list(s->fmts, &s->nfmts);
    s->fmts[s->nfmts].k = F_END;
}

/* GET [FILE(f)] [SKIP[(n)]] LIST(targets) | EDIT(targets)(formats) */
static struct stmt *get_stmt(int from)
{
    struct stmt *s = new_stmt(S_GET, from);
    struct put_item **tail = &s->items;

    s->skip = -1;
    pos++;                                   /* GET */
    while (!is(T_SEMI)) {
        if (isword("FILE")) {
            s->file = file_option();
        } else if (isword("SKIP")) {
            pos++;
            s->skip = 1;
            if (is(T_LP)) { s->skip = format_number(); expect(T_RP, "')'"); }
        } else if (isword("COPY")) {
            pos++;                         /* each field read, to SYSPRINT */
            s->copy = 1;
            if (is(T_LP)) {                /* ANSI's COPY(file) (CM112) */
                beyond_f(tk(0)->line, L_OPT, "COPY(file)");
                pos++;
                s->copyfile = parse_ref();
                expect(T_RP, "')'");
            }
        } else if (isword("LIST")) {
            pos++;
            data_list(&tail, 1);
        } else if (isword("EDIT")) {
            pos++;
            s->edit = 1;
            data_list(&tail, 1);
            edit_formats(s);
        } else if (isword("DATA")) {
            pos++;
            s->data = 1;
            if (!is(T_LP))
                fatal("%s(%d): GET DATA needs its list here", src_name, tk(0)->line);
            data_list(&tail, 1);
        } else if (isword("STRING")) {
            pos++;
            expect(T_LP, "'('");
            s->string = expr();
            expect(T_RP, "')'");
        } else {
            fatal("%s(%d): GET option %s is not supported yet", src_name, tk(0)->line,
                  is(T_IDENT) ? tk(0)->text : "?");
        }
    }
    pos++;
    finish(s, from);
    return s;
}

/* OPEN|CLOSE FILE(f) [attributes] [TITLE('dd')] [, FILE(g) ...] */
static struct stmt *open_stmt(int from, enum skind k)
{
    struct stmt *s = new_stmt(k, from);
    pos++;
    for (;;) {
        struct fopen *f;
        if (s->nfiles == MAXFILES)
            fatal("%s(%d): too many files in one statement", src_name, s->line);
        f = &s->files[s->nfiles++];
        memset(f, 0, sizeof *f);
        if (!isword("FILE"))
            fatal("%s(%d): FILE(...) expected", src_name, tk(0)->line);
        f->file = file_option();
        while (is(T_IDENT) && !isword("FILE")) {
            if (isword("INPUT")) f->attrs |= FL_INPUT;
            else if (isword("OUTPUT")) f->attrs |= FL_OUTPUT;
            else if (isword("UPDATE")) f->attrs |= FL_UPDATE | FL_RECORD;
            else if (isword("SEQUENTIAL") || isword("SEQL")) f->attrs |= FL_RECORD;
            else if (isword("DIRECT")) f->attrs |= FL_DIRECT | FL_KEYED | FL_RECORD;
            else if (isword("KEYED")) f->attrs |= FL_KEYED | FL_RECORD;
            else if (isword("BUFFERED") || isword("BUF") || isword("UNBUFFERED") ||
                     isword("UNBUF")) ;
            else if (isword("STREAM")) f->attrs |= FL_STREAM;
            else if (isword("RECORD")) f->attrs |= FL_RECORD;
            else if (isword("SEQUENTIAL") || isword("SEQL")) ;
            else if (isword("PRINT")) f->attrs |= FL_PRINT | FL_OUTPUT;
            else if (isword("PAGESIZE") || isword("LINESIZE")) {
                /* PAGESIZE(e), LINESIZE(e) (Q1LSZ): set after the opening */
                int ps = isword("PAGESIZE");
                pos++;
                expect(T_LP, "'('");
                *(ps ? &f->pagesize : &f->linesize) = expr();
                expect(T_RP, "')'");
                continue;
            }
            else if (isword("TAB")) {
                /* ANSI's TAB(e, ...): the tab positions of a PRINT file */
                beyond_f(tk(0)->line, L_ISO, "the TAB option");
                pos++;
                expect(T_LP, "'('");
                for (;;) {
                    if (f->ntabs == 16)
                        fatal("%s(%d): more than 16 tab positions", src_name, s->line);
                    f->tabs[f->ntabs++] = expr();
                    if (is(T_COMMA)) { pos++; continue; }
                    break;
                }
                expect(T_RP, "')'");
                continue;
            }
            else if (isword("TITLE")) {
                pos++;
                expect(T_LP, "'('");
                if (!is(T_STR) || tk(0)->len < 1 || tk(0)->len > 8)
                    fatal("%s(%d): TITLE takes a DD name in quotes", src_name, tk(0)->line);
                f->title = xmalloc((size_t)tk(0)->len + 1);
                memcpy(f->title, tk(0)->str, (size_t)tk(0)->len);
                f->title[tk(0)->len] = '\0';
                pos++;
                expect(T_RP, "')'");
                continue;
            } else
                fatal("%s(%d): option %s is not supported here yet", src_name, tk(0)->line,
                      tk(0)->text);
            if (k == S_CLOSE)
                error_at(s->line, "CLOSE takes no attributes");
            pos++;
        }
        if (is(T_COMMA)) { pos++; continue; }
        break;
    }
    expect(T_SEMI, "';'");
    finish(s, from);
    return s;
}

static struct stmt *put_stmt(int from)
{
    struct stmt *s = new_stmt(S_PUT, from);
    struct put_item **tail = &s->items;

    s->skip = -1;
    pos++;                                   /* PUT */
    while (!is(T_SEMI)) {
        if (isword("SKIP")) {
            pos++;
            s->skip = 1;
            if (is(T_LP)) {
                pos++;
                if (!is(T_NUM))
                    fatal("%s(%d): SKIP takes a constant here", src_name, tk(0)->line);
                s->skip = atoi(tk(0)->text);
                pos++;
                expect(T_RP, "')'");
            }
        } else if (isword("PAGE")) {
            pos++;
            s->page = 1;
        } else if (isword("LIST")) {
            pos++;
            data_list(&tail, 0);
        } else if (isword("FILE")) {
            s->file = file_option();
        } else if (isword("EDIT")) {
            pos++;
            s->edit = 1;
            data_list(&tail, 0);
            edit_formats(s);
        } else if (isword("DATA")) {
            pos++;
            s->data = 2;
            if (is(T_LP)) {
                s->data = 1;
                data_list(&tail, 1);
            }
        } else if (isword("STRING")) {
            pos++;
            expect(T_LP, "'('");
            s->string = parse_ref();
            expect(T_RP, "')'");
        } else if (isword("LINE")) {
            fatal("%s(%d): PUT LINE is not supported yet", src_name, tk(0)->line);
        } else {
            fatal("%s(%d): PUT option expected", src_name, tk(0)->line);
        }
    }
    pos++;
    finish(s, from);
    return s;
}

static const struct { const char *name, *abbr; enum cond c; } conds[] = {
    {"ENDFILE", NULL, C_ENDFILE}, {"ENDPAGE", NULL, C_ENDPAGE},
    {"UNDEFINEDFILE", "UNDF", C_UNDF}, {"RECORD", NULL, C_RECORD}, {"AREA", NULL, C_AREA},
    {"KEY", NULL, C_KEY}, {"NAME", NULL, C_NAME}, {"CHECK", NULL, C_CHECK},
    {"TRANSMIT", NULL, C_TRANSMIT}, {"STRINGSIZE", "STRZ", C_STRINGSIZE},
    {"STORAGE", NULL, C_STORAGE},
    {"ZERODIVIDE", "ZDIV", C_ZERODIVIDE},
    {"FIXEDOVERFLOW", "FOFL", C_FIXEDOVERFLOW}, {"CONVERSION", "CONV", C_CONVERSION},
    {"ERROR", NULL, C_ERROR}, {"FINISH", NULL, C_FINISH}, {"CONDITION", "COND", C_CONDITION},
    {"UNDERFLOW", "UFL", C_UNDERFLOW}, {"SIZE", NULL, C_SIZE},
    {"SUBSCRIPTRANGE", "SUBRG", C_SUBRG}, {"STRINGRANGE", "STRG", C_STRG},
    {"OVERFLOW", "OFL", C_OVERFLOW}, {"ATTENTION", "ATTN", C_ATTENTION},
};

/* A condition prefix: (name, ...): with names of conditions and their NO
 * forms.  Returns 0 (and parses nothing) if what follows is not one. */
static int prefix(int *on, int *off)
{
    static const struct { const char *name, *abbr; int bit; } pn[] = {
        {"SIZE", NULL, EN_SIZE}, {"SUBSCRIPTRANGE", "SUBRG", EN_SUBRG},
        {"STRINGRANGE", "STRG", EN_STRG}, {"ZERODIVIDE", "ZDIV", EN_ZDIV},
        {"FIXEDOVERFLOW", "FOFL", EN_FOFL}, {"CONVERSION", "CONV", EN_CONV},
        {"UNDERFLOW", "UFL", EN_UFL}, {"OVERFLOW", "OFL", EN_OFL},
        {"STRINGSIZE", "STRZ", EN_STRZ},
    };
    int k = 1;
    pfx_checks = NULL;
    pfx_nchecks = 0;
    if (!is(T_LP))
        return 0;
    for (;;) {
        /* name, or CHECK(name, ...), then , or ): */
        if (tk(k)->kind != T_IDENT)
            return 0;
        if (strcmp(tk(k)->text, "CHECK") == 0 && tk(k + 1)->kind == T_LP) {
            k += 2;
            while (tk(k)->kind == T_IDENT && tk(k + 1)->kind == T_COMMA)
                k += 2;
            if (tk(k)->kind != T_IDENT || tk(k + 1)->kind != T_RP)
                return 0;
            k++;
        }
        if (tk(k + 1)->kind == T_COMMA) {
            k += 2;
            continue;
        }
        if (tk(k + 1)->kind == T_RP && tk(k + 2)->kind == T_COLON)
            break;
        return 0;
    }
    pos++;
    for (;;) {
        const char *w = tk(0)->text;
        int no = strncmp(w, "NO", 2) == 0 && strcmp(w, "NOT") != 0, found = 0;
        if (strcmp(w, "CHECK") == 0 && tk(1)->kind == T_LP) {
            /* CHECK(name, ...): for a PROCEDURE or BEGIN block (Q1CHK) */
            pos += 2;
            for (;;) {
                pfx_checks = realloc(pfx_checks, (size_t)(pfx_nchecks + 1) * sizeof *pfx_checks);
                if (!pfx_checks)
                    fatal("out of memory");
                pfx_checks[pfx_nchecks++] = tk(0)->text;
                pos++;
                if (is(T_COMMA)) { pos++; continue; }
                break;
            }
            pos++;                           /* ) */
            if (is(T_COMMA)) { pos++; continue; }
            break;
        }
        for (size_t i = 0; i < sizeof pn / sizeof pn[0]; i++) {
            const char *x = no ? w + 2 : w;
            if (strcmp(x, pn[i].name) == 0 || (pn[i].abbr && strcmp(x, pn[i].abbr) == 0)) {
                if (pn[i].bit == EN_STRZ) {
                    beyond_f(tk(0)->line, L_OPT, "the STRINGSIZE condition");
                    if (!no)
                        uses_strz = 1;
                }
                if (no) { *off |= pn[i].bit; *on &= ~pn[i].bit; }
                else { *on |= pn[i].bit; *off &= ~pn[i].bit; }
                found = 1;
            }
        }
        if (!found && (strcmp(no ? w + 2 : w, "ATTENTION") == 0 ||
                       strcmp(no ? w + 2 : w, "ATTN") == 0))
            /* "ATTENTION is always enabled; it cannot be disabled"
             * (GC33-0009-4, p. 393) */
            fatal("%s(%d): ATTENTION cannot be named in a condition prefix: it is always "
                  "enabled", src_name, tk(0)->line);
        if (!found)
            fatal("%s(%d): condition prefix %s is not supported yet", src_name, tk(0)->line, w);
        pos++;
        if (is(T_COMMA)) { pos++; continue; }
        break;
    }
    pos += 2;                                /* ): */
    return 1;
}

/* The condition of an ON, SIGNAL or REVERT statement. */
static void condition(struct stmt *st)
{
    if (!is(T_IDENT))
        fatal("%s(%d): condition name expected", src_name, tk(0)->line);
    for (size_t i = 0; i < sizeof conds / sizeof conds[0]; i++)
        if (strcmp(tk(0)->text, conds[i].name) == 0 ||
            (conds[i].abbr && strcmp(tk(0)->text, conds[i].abbr) == 0)) {
            st->cond = conds[i].c;
            if (st->cond == C_STRINGSIZE)
                beyond_f(tk(0)->line, L_OPT, "the STRINGSIZE condition");
            else if (st->cond == C_STORAGE)
                beyond_f(tk(0)->line, L_ISO, "the STORAGE condition");
            else if (st->cond == C_ATTENTION)
                beyond_f(tk(0)->line, L_OPT, "the ATTENTION condition");
        }
    if (st->cond == C_NONE)
        fatal("%s(%d): condition %s is not supported yet", src_name, tk(0)->line, tk(0)->text);
    pos++;
    if (FILE_COND(st->cond)) {
        expect(T_LP, "'('");
        st->file = parse_ref();
        expect(T_RP, "')'");
    } else if (st->cond == C_CONDITION || st->cond == C_CHECK) {
        expect(T_LP, "'('");
        if (!is(T_IDENT))
            fatal("%s(%d): a condition name is expected", src_name, tk(0)->line);
        st->cond_name = tk(0)->text;
        pos++;
        expect(T_RP, "')'");
    }
}

static struct stmt *statement(void);

/* The on-unit of an ON statement, as a block inside the current one. */
static struct proc *block(struct stmt *on, int is_unit);

static struct proc *on_unit(struct stmt *on)
{
    return block(on, 1);
}

/* An on-unit or a BEGIN block: a block of its own inside the current one. */
static struct proc *block(struct stmt *on, int is_unit)
{
    struct proc *p = xmalloc(sizeof *p), *saved = cur;
    char name[16];

    memset(p, 0, sizeof *p);
    snprintf(name, sizeof name, is_unit ? "@ON%d" : "@BEGIN%d", nproc + 1);
    p->name = xstrdup(name);
    p->line = on->line;
    p->parent = cur;
    {
        struct proc **c = &cur->child;
        while (*c)
            c = &(*c)->sibling;
        *c = p;
    }
    p->depth = cur->depth + 1;
    p->id = ++nproc;
    p->is_unit = is_unit;
    p->is_block = !is_unit;
    cur = p;
    if (isword("BEGIN") && tk(1)->kind == T_SEMI) {
        pos += 2;
        stmt_no++;
        p->body = group_body();
    } else {
        p->body = statement();
        if (!p->body)
            fatal("%s(%d): the on-unit is missing", src_name, on->line);
    }
    cur = saved;
    return p;
}

static struct stmt *statement(void)
{
    int from = pos;
    char *labels[MAXLABELS];
    int nl = 0;
    struct stmt *st = NULL;

    int pon = 0, poff = 0, nck;
    char **ck;
    prefix(&pon, &poff);
    ck = pfx_checks;
    nck = pfx_nchecks;
    while (is(T_IDENT) && tk(1)->kind == T_COLON) {
        if (nl < MAXLABELS)
            labels[nl++] = tk(0)->text;
        pos += 2;                            /* statement labels */
    }
    glabels = labels;                        /* for the group it may open */
    nglabels = nl;
    if (is(T_SEMI)) {
        st = new_stmt(S_NULL, from);
        pos++;
        finish(st, from);
        goto done;
    }
    if (!is(T_IDENT))
        fatal("%s(%d): statement expected", src_name, tk(0)->line);
    /* An assignment unless the first word is a statement keyword not
     * followed by '=' -- keywords are not reserved. */
    {
        static const char *kw[] = { "DCL", "DECLARE", "IF", "DO", "PUT", "PROC", "PROCEDURE",
                                    "END", "ELSE", "GET", "CALL", "RETURN", "GO", "GOTO", "ON",
                                    "OPEN", "CLOSE", "BEGIN", "READ", "WRITE", "SIGNAL",
                                    "REVERT", "STOP", "EXIT", "ALLOCATE", "ALLOC", "FREE",
                                    "ENTRY", "REWRITE", "DELETE", "SELECT", "WHEN",
                                    "OTHERWISE", "OTHER", "LEAVE", "DEFAULT", "DFT",
                                    "LOCATE", "FORMAT", "UNLOCK", "DISPLAY", "WAIT", "DELAY" };
        int keyword = 0;
        for (size_t k = 0; k < sizeof kw / sizeof kw[0]; k++)
            if (strcmp(tk(0)->text, kw[k]) == 0)
                keyword = 1;
        if (tk(1)->kind == T_EQ || (!keyword && (tk(1)->kind == T_LP || tk(1)->kind == T_DOT ||
                                                 tk(1)->kind == T_ARROW || tk(1)->kind == T_COMMA))) {
            st = new_stmt(S_ASSIGN, from);
            st->lhs = parse_ref();
            while (is(T_COMMA)) {
                /* A, B = e: PL/I (F) evaluates e once (Q1ADJ, JOB01248) */
                pos++;
                if (st->nmore == 16)
                    fatal("%s(%d): too many targets", src_name, tk(0)->line);
                st->more[st->nmore++] = parse_ref();
            }
            expect(T_EQ, "'='");
            st->e = expr();
            if (is(T_COMMA) && tk(1)->kind == T_IDENT && strcmp(tk(1)->text, "BY") == 0 &&
                tk(2)->kind == T_IDENT && strcmp(tk(2)->text, "NAME") == 0) {
                st->byname = 1;              /* structures matched by name */
                pos += 3;
            }
            expect(T_SEMI, "';'");
            finish(st, from);
            goto done;
        }
    }
    if (isword("ENTRY")) {
        /* name: ENTRY [(params)] [RETURNS(attributes)]; a secondary entry
         * point of the procedure; flowing into it does nothing */
        struct pentry *pe;
        if (!nl)
            fatal("%s(%d): an ENTRY statement needs a label, its name", src_name, tk(0)->line);
        if (cur->is_main)
            fatal("%s(%d): an ENTRY statement in the main procedure is not supported",
                  src_name, tk(0)->line);
        if (cur->is_block || cur->is_unit)
            fatal("%s(%d): an ENTRY statement belongs in a procedure", src_name, tk(0)->line);
        st = new_stmt(S_ENTRY, from);
        pos++;
        cur->entries = realloc(cur->entries, sizeof *cur->entries * (size_t)(cur->nentries + 1));
        if (!cur->entries)
            fatal("out of memory");
        pe = &cur->entries[cur->nentries++];
        memset(pe, 0, sizeof *pe);
        pe->name = labels[0];
        if (is(T_LP)) {
            pos++;
            for (;;) {
                int k;
                if (!is(T_IDENT))
                    fatal("%s(%d): parameter name expected", src_name, tk(0)->line);
                for (k = 0; k < cur->nparams && strcmp(cur->pnames[k], tk(0)->text) != 0; k++)
                    ;
                if (k == cur->nparams) {
                    if (cur->nparams == MAXPARAMS)
                        fatal("%s(%d): too many parameters", src_name, st->line);
                    cur->pnames[cur->nparams++] = tk(0)->text;
                }
                if (pe->nparams == MAXCALLARGS)
                    fatal("%s(%d): too many parameters", src_name, st->line);
                pe->canon[pe->nparams++] = k;
                pos++;
                if (is(T_COMMA)) { pos++; continue; }
                break;
            }
            expect(T_RP, "')'");
        }
        while (!is(T_SEMI)) {
            if (isword("RETURNS")) {
                pos++;
                expect(T_LP, "'('");
                attrs(&pe->ret, st->line);
                expect(T_RP, "')'");
                pe->has_ret = 1;
            } else {
                fatal("%s(%d): ENTRY option %s is not supported", src_name, tk(0)->line,
                      is(T_IDENT) ? tk(0)->text : "?");
            }
        }
        pos++;
        st->goto_id = cur->nentries;
        finish(st, from);
        goto done;
    }
    if (isword2("DCL", "DECLARE")) {
        if (nl)
            error_at(tk(0)->line, "a DECLARE statement takes no label");
        declare();
        stmt_no++;
        return NULL;
    }
    if (isword2("DEFAULT", "DFT") && tk(1)->kind != T_EQ) {
        /* applied after the whole program is read (default_pass): its
         * scope is the block, wherever it stands; a branch to its label
         * is a branch to a null statement */
        beyond_f(tk(0)->line, L_OPT, "the DEFAULT statement");
        default_stmt();
        st = new_stmt(S_NULL, from);
        finish(st, from);
        goto done;
    }
    if (isword("IF")) {
        st = new_stmt(S_IF, from);
        pos++;
        st->e = expr();
        if (!isword("THEN"))
            fatal("%s(%d): THEN expected", src_name, tk(0)->line);
        pos++;
        finish(st, from);
        st->body = statement();
        if (isword("ELSE") && tk(1)->kind != T_EQ) {
            pos++;
            stmt_no++;
            st->els = statement();
        }
        goto done;
    }
    if (isword("DO")) {
        st = do_stmt(from);
        goto done;
    }
    if (isword("PUT")) {
        st = put_stmt(from);
        st = with_fmt_pre(st, from);
        goto done;
    }
    if (isword("GET")) {
        st = get_stmt(from);
        st = with_fmt_pre(st, from);
        goto done;
    }
    if (isword("OPEN")) {
        st = open_stmt(from, S_OPEN);
        goto done;
    }
    if (isword("READ") || isword("WRITE") || isword("REWRITE")) {
        /* READ FILE(f) INTO(v);  WRITE FILE(f) FROM(v);  REWRITE likewise */
        int rd = isword("READ");
        st = new_stmt(rd ? S_READ : isword("REWRITE") ? S_REWRITE : S_WRITE, from);
        pos++;
        while (!is(T_SEMI)) {
            if (isword("FILE")) {
                st->file = file_option();
            } else if (isword(rd ? "INTO" : "FROM")) {
                pos++;
                expect(T_LP, "'('");
                st->lhs = parse_ref();
                expect(T_RP, "')'");
            } else if (isword(st->k == S_WRITE ? "KEYFROM" : "KEY") && !st->e) {
                /* the source key, a character string (KEYFROM for WRITE) */
                pos++;
                expect(T_LP, "'('");
                st->e = expr();
                expect(T_RP, "')'");
            } else if (rd && isword("SET") && !st->e3 && !st->lhs) {
                /* READ ... SET(p): p points to the record */
                pos++;
                expect(T_LP, "'('");
                st->e3 = parse_ref();
                expect(T_RP, "')'");
            } else if (rd && isword("IGNORE") && !st->ignore) {
                /* READ ... IGNORE(n): n records skipped */
                pos++;
                expect(T_LP, "'('");
                st->ignore = expr();
                expect(T_RP, "')'");
            } else if (isword("EVENT") && !st->nmore) {
                /* EVENT(e): the transmission's conditions at a WAIT (#3) */
                pos++;
                expect(T_LP, "'('");
                st->more[st->nmore++] = parse_ref();
                expect(T_RP, "')'");
            } else if (rd && isword("NOLOCK")) {
                pos++;                    /* no lock to leave out in one task (#217) */
            } else if (rd && isword("KEYTO") && !st->e2) {
                pos++;
                expect(T_LP, "'('");
                st->e2 = parse_ref();
                expect(T_RP, "')'");
            } else {
                fatal("%s(%d): %s option %s is not supported yet", src_name, tk(0)->line,
                      rd ? "READ" : "WRITE", is(T_IDENT) ? tk(0)->text : "?");
            }
        }
        pos++;
        if (rd && st->file && !st->lhs && !st->e3 && !st->ignore && !st->e) {
            /* READ FILE(f); is READ FILE(f) IGNORE(1); (GC28-8201-2) */
            st->ignore = new_expr(E_NUM, st->line);
            st->ignore->num = "1";
            st->ignore->t.k = TY_FIXDEC;
            st->ignore->t.p = 1;
        }
        if (!st->file || (!st->lhs && !(rd && (st->e3 || st->ignore)) && st->k != S_REWRITE))
            fatal("%s(%d): %s needs FILE(...) and %s(...)", src_name, st->line,
                  rd ? "READ" : "WRITE", rd ? "INTO or SET" : "FROM");
        finish(st, from);
        goto done;
    }
    if (isword("CLOSE")) {
        st = open_stmt(from, S_CLOSE);
        goto done;
    }
    if (isword("SELECT") && (tk(1)->kind == T_SEMI || tk(1)->kind == T_LP)) {
        /* SELECT [(e)]; WHEN (e1, ...) action; ... [OTHERWISE action;] END;
         * (the Optimizing Compiler, GC33-0009-4) */
        struct stmt **tail;
        st = new_stmt(S_SELECT, from);
        beyond_f(tk(0)->line, L_OPT, "the SELECT statement");
        pos++;
        if (is(T_LP)) {
            pos++;
            st->e = expr();
            expect(T_RP, "')'");
        }
        expect(T_SEMI, "';'");
        finish(st, from);
        tail = &st->body;
        for (;;) {
            int wf = pos;
            if (isword("WHEN") && tk(1)->kind == T_LP) {
                struct stmt *w = new_stmt(S_WHEN, wf);
                struct put_item **it = &w->items;
                pos += 2;
                for (;;) {
                    struct put_item *x = xmalloc(sizeof *x);
                    x->e = expr();
                    *it = x;
                    it = &x->next;
                    if (is(T_COMMA)) { pos++; continue; }
                    break;
                }
                expect(T_RP, "')'");
                finish(w, wf);
                w->body = statement();
                *tail = w;
                tail = &w->next;
            } else if ((isword("OTHERWISE") || isword("OTHER")) && !st->els) {
                pos++;
                st->els = statement();
            } else if (isword("END")) {
                pos++;
                if (is(T_IDENT))
                    pos++;
                expect(T_SEMI, "';'");
                break;
            } else {
                fatal("%s(%d): WHEN, OTHERWISE or END expected in a select-group", src_name,
                      tk(0)->line);
            }
        }
        goto done;
    }
    if (isword("FORMAT") && tk(1)->kind == T_LP) {
        /* label: FORMAT (list); -- not executed, the list of R(label) */
        if (!nl)
            fatal("%s(%d): a FORMAT statement needs a label", src_name, tk(0)->line);
        st = new_stmt(S_FORMAT, from);
        pos++;
        edit_formats(st);
        expect(T_SEMI, "';'");
        finish(st, from);
        goto done;
    }
    if (isword("LOCATE") && tk(1)->kind == T_IDENT) {
        /* LOCATE x FILE(f) [SET(p)] [KEYFROM(k)]; */
        st = new_stmt(S_LOCATE, from);
        pos++;
        st->lhs = parse_ref();
        while (!is(T_SEMI)) {
            if (isword("FILE")) {
                st->file = file_option();
            } else if (isword("SET") && !st->e3) {
                pos++;
                expect(T_LP, "'('");
                st->e3 = parse_ref();
                expect(T_RP, "')'");
            } else if (isword("KEYFROM") && !st->e) {
                pos++;
                expect(T_LP, "'('");
                st->e = expr();
                expect(T_RP, "')'");
            } else {
                fatal("%s(%d): LOCATE option %s is not supported yet", src_name, tk(0)->line,
                      is(T_IDENT) ? tk(0)->text : "?");
            }
        }
        pos++;
        if (!st->file)
            fatal("%s(%d): LOCATE needs FILE(...)", src_name, st->line);
        finish(st, from);
        goto done;
    }
    if ((isword("FETCH") || isword("RELEASE")) && tk(1)->kind == T_IDENT) {
        /* FETCH entry, ...; RELEASE entry, ...; (the Optimizer's,
         * GC33-0009-4): load and delete an assembler routine here */
        st = new_stmt(isword("FETCH") ? S_FETCH : S_RELEASE, from);
        beyond_f(tk(0)->line, L_OPT, st->k == S_FETCH ? "the FETCH statement" :
                 "the RELEASE statement");
        pos++;
        for (;;) {
            if (st->nmore == 16)
                fatal("%s(%d): too many entries in one statement", src_name, st->line);
            st->more[st->nmore++] = parse_ref();
            if (is(T_COMMA)) { pos++; continue; }
            break;
        }
        expect(T_SEMI, "';'");
        finish(st, from);
        goto done;
    }
    if (isword("LEAVE") && (tk(1)->kind == T_SEMI || tk(1)->kind == T_IDENT)) {
        st = new_stmt(S_LEAVE, from);
        beyond_f(tk(0)->line, L_OPT, "the LEAVE statement");
        pos++;
        if (is(T_IDENT)) {
            st->goto_name = tk(0)->text;
            pos++;
        }
        expect(T_SEMI, "';'");
        finish(st, from);
        goto done;
    }
    if (isword("DELETE") && tk(1)->kind == T_IDENT) {
        /* DELETE FILE(f) [KEY(k)]; */
        st = new_stmt(S_DELETE, from);
        pos++;
        while (!is(T_SEMI)) {
            if (isword("FILE")) {
                st->file = file_option();
            } else if (isword("KEY") && !st->e) {
                pos++;
                expect(T_LP, "'('");
                st->e = expr();
                expect(T_RP, "')'");
            } else {
                fatal("%s(%d): DELETE option %s is not supported yet", src_name, tk(0)->line,
                      is(T_IDENT) ? tk(0)->text : "?");
            }
        }
        pos++;
        if (!st->file)
            fatal("%s(%d): DELETE needs FILE(...)", src_name, st->line);
        finish(st, from);
        goto done;
    }
    if (isword("UNLOCK") && tk(1)->kind == T_IDENT) {
        /* UNLOCK FILE(f) KEY(k); releases a record an EXCLUSIVE file's
         * READ locked against other tasks.  With one task there is no
         * lock: a null statement, the key not evaluated (Q1EXC, #217) */
        int file = 0, key = 0;
        st = new_stmt(S_NULL, from);
        pos++;
        while (!is(T_SEMI)) {
            if (isword("FILE") && !file) {
                file_option();
                file = 1;
            } else if (isword("KEY") && !key) {
                pos++;
                expect(T_LP, "'('");
                expr();
                expect(T_RP, "')'");
                key = 1;
            } else {
                fatal("%s(%d): UNLOCK option %s is not supported", src_name, tk(0)->line,
                      is(T_IDENT) ? tk(0)->text : "?");
            }
        }
        pos++;
        if (!file || !key)
            error_at(st->line, "UNLOCK needs FILE(...) and KEY(...)");   /* IEM0237I */
        finish(st, from);
        goto done;
    }
    if (isword("DISPLAY") && tk(1)->kind == T_LP) {
        /* DISPLAY (message) [REPLY (c)] [EVENT (e)]; a WTO, or with
         * REPLY a WTOR the program waits for (GC28-8201-4, Section J;
         * Q1DSP, #218).  REPLY and EVENT in either order (Q1DSP) */
        int event = 0;
        st = new_stmt(S_DISPLAY, from);
        pos++;
        expect(T_LP, "'('");
        st->e = expr();
        expect(T_RP, "')'");
        while (!is(T_SEMI)) {
            if (isword("REPLY") && !st->e2) {
                pos++;
                expect(T_LP, "'('");
                st->e2 = expr();
                expect(T_RP, "')'");
            } else if (isword("EVENT") && !event) {
                pos++;
                expect(T_LP, "'('");
                st->e3 = parse_ref();
                expect(T_RP, "')'");
                event = 1;
            } else {
                fatal("%s(%d): DISPLAY option %s is not supported or given twice", src_name, tk(0)->line,
                      is(T_IDENT) ? tk(0)->text : "?");
            }
        }
        pos++;
        if (event && !st->e2)
            error_at(st->line, "EVENT on a DISPLAY needs REPLY");
        finish(st, from);
        goto done;
    }
    if (isword("ON")) {
        /* ON condition SYSTEM;  or  ON condition on-unit, the unit a
         * statement or BEGIN; ... END; -- a block of its own */
        st = new_stmt(S_ON, from);
        pos++;
        condition(st);
        if (isword("SNAP")) {
            /* SNAP: the place and the calling procedures written first
             * (Q1SNAP to Q4SNAP) */
            st->snap = 1;
            pos++;
        }
        if (isword("SYSTEM") && tk(1)->kind == T_SEMI) {
            pos += 2;
            finish(st, from);
            goto done;
        }
        finish(st, from);
        st->unit = on_unit(st);
        goto done;
    }
    if (isword("BEGIN") && tk(1)->kind == T_SEMI) {
        /* a BEGIN block: a block of its own, entered here */
        st = new_stmt(S_BEGIN, from);
        pos += 2;
        finish(st, from);
        pos -= 2;                         /* block() takes BEGIN; again */
        st->unit = block(st, 0);
        goto done;
    }
    if (isword("WAIT") && tk(1)->kind == T_LP) {
        /* WAIT (event, ...) [(count)]; (GC28-8201-4, #3) */
        st = new_stmt(S_WAIT, from);
        pos += 2;
        for (;;) {
            if (st->nmore == 16)
                fatal("%s(%d): WAIT on more than 16 events is not supported", src_name,
                      st->line);
            st->more[st->nmore++] = parse_ref();
            if (is(T_COMMA)) {
                pos++;
                continue;
            }
            break;
        }
        expect(T_RP, "')'");
        if (is(T_LP)) {
            pos++;
            st->e = expr();
            expect(T_RP, "')'");
        }
        expect(T_SEMI, "';'");
        finish(st, from);
        goto done;
    }
    if (isword("DELAY") && tk(1)->kind == T_LP) {
        /* DELAY (milliseconds); (#3) */
        st = new_stmt(S_DELAY, from);
        pos += 2;
        st->e = expr();
        expect(T_RP, "')'");
        expect(T_SEMI, "';'");
        finish(st, from);
        goto done;
    }
    if ((isword("STOP") || isword("EXIT")) && tk(1)->kind == T_SEMI) {
        st = new_stmt(S_STOP, from);
        pos += 2;
        finish(st, from);
        goto done;
    }
    if (isword2("ALLOCATE", "ALLOC") || isword("FREE")) {
        /* ALLOCATE x [IN(a)] [SET(p)], ...;  FREE [p ->] x [IN(a)], ...; */
        struct put_item **tail;
        int alloc = !isword("FREE");
        st = new_stmt(alloc ? S_ALLOC : S_FREE, from);
        pos++;
        tail = &st->items;
        for (;;) {
            struct put_item *it = xmalloc(sizeof *it);
            memset(it, 0, sizeof *it);
            it->e = parse_ref();
            if (alloc && it->e->qual)
                fatal("%s(%d): write ALLOCATE x SET(p), not p -> x", src_name, st->line);
            for (;;) {
                if (alloc && isword("SET") && !it->e->qual) {
                    pos++;
                    expect(T_LP, "'('");
                    it->e->qual = parse_ref();
                    expect(T_RP, "')'");
                } else if (isword("IN") && !it->in) {
                    pos++;
                    expect(T_LP, "'('");
                    it->in = parse_ref();
                    expect(T_RP, "')'");
                } else {
                    break;
                }
            }
            *tail = it;
            tail = &it->next;
            if (is(T_COMMA)) { pos++; continue; }
            break;
        }
        expect(T_SEMI, "';'");
        finish(st, from);
        goto done;
    }
    if (isword("SIGNAL") || isword("REVERT")) {
        st = new_stmt(isword("SIGNAL") ? S_SIGNAL : S_REVERT, from);
        pos++;
        condition(st);
        expect(T_SEMI, "';'");
        finish(st, from);
        goto done;
    }
    if (isword("CALL")) {
        st = new_stmt(S_CALL, from);
        pos++;
        st->e = parse_ref();
        expect(T_SEMI, "';'");
        finish(st, from);
        goto done;
    }
    if (isword("RETURN")) {
        st = new_stmt(S_RETURN, from);
        pos++;
        if (is(T_LP)) {
            struct proc *pp = cur;
            pos++;
            st->e = expr();
            expect(T_RP, "')'");
            while (pp && (pp->is_block || pp->is_unit))
                pp = pp->parent;
            if (pp && !pp->has_ret && !pp->nentries) {
                /* a function without RETURNS: the attributes its name's first
                 * letter gives, I to N FIXED BINARY(15), else FLOAT DECIMAL(6)
                 * (Q1BLT: KOUNT 21, XSQ 2.25E+00) */
                memset(&pp->ret, 0, sizeof pp->ret);
                if (pp->name[0] >= 'I' && pp->name[0] <= 'N') {
                    pp->ret.k = TY_FIXBIN;
                    pp->ret.p = 15;
                } else {
                    pp->ret.k = TY_FLTDEC;
                    pp->ret.p = 6;
                }
                pp->has_ret = 1;
            }
        }
        expect(T_SEMI, "';'");
        finish(st, from);
        goto done;
    }
    if (isword("GOTO") || (isword("GO") && tk(1)->kind == T_IDENT && strcmp(tk(1)->text, "TO") == 0)) {
        st = new_stmt(S_GOTO, from);
        pos += isword("GOTO") ? 1 : 2;
        if (!is(T_IDENT))
            fatal("%s(%d): label expected after GO TO", src_name, tk(0)->line);
        /* a label constant, or a label variable (resolve.c decides) */
        st->e = parse_ref1();
        if (st->e->nq == 1 && st->e->nsubs == 0)
            st->goto_name = st->e->qn[0];
        expect(T_SEMI, "';'");
        finish(st, from);
        goto done;
    }
    if (isword2("PROC", "PROCEDURE"))
        fatal("%s(%d): a PROCEDURE statement needs a label", src_name, tk(0)->line);
    fatal("%s(%d): statement %s is not supported yet", src_name, tk(0)->line, tk(0)->text);
    return NULL;
done:
    nglabels = 0;
    for (int i = 0; i < nl; i++)
        st->labels[st->nlabels++] = labels[i];
    st->pon = pon;
    st->poff = poff;
    if (st->k == S_BEGIN) {
        st->unit->pon = pon;                 /* (SIZE): BEGIN; is the block's */
        st->unit->poff = poff;
        st->unit->checks = ck;
        st->unit->nchecks = nck;
    }
    return st;
}

/* name: PROCEDURE [(parameters)] [OPTIONS(MAIN)] [RETURNS(attributes)]
 *       [RECURSIVE] [REORDER|ORDER]; ... END [name]; */
static struct proc *parse_proc(int from, struct proc *parent)
{
    struct proc *p = xmalloc(sizeof *p), *saved = cur;
    int main_opt = 0, noexecops = 0, byvalue = 0;

    memset(p, 0, sizeof *p);
    pos = from;
    p->name = tk(0)->text;
    p->line = tk(0)->line;
    p->parent = parent;
    if (parent) {
        struct proc **c = &parent->child;
        while (*c)
            c = &(*c)->sibling;
        *c = p;
        p->depth = parent->depth + 1;
        p->id = ++nproc;
    }
    pos += 3;                                /* name : PROCEDURE */
    if (is(T_LP)) {
        pos++;
        for (;;) {
            if (!is(T_IDENT))
                fatal("%s(%d): parameter name expected", src_name, tk(0)->line);
            if (p->nparams == MAXPARAMS)
                fatal("%s(%d): too many parameters", src_name, p->line);
            p->pnames[p->nparams++] = tk(0)->text;
            pos++;
            if (is(T_COMMA)) { pos++; continue; }
            break;
        }
        expect(T_RP, "')'");
    }
    p->nprim = p->nparams;
    while (!is(T_SEMI)) {
        if (isword("OPTIONS") && tk(1)->kind == T_LP) {
            /* OPTIONS(MAIN), (REENTRANT): our code is reentrant anyway */
            pos += 2;
            for (;;) {
                if (isword("MAIN"))
                    main_opt = 1;
                else if (isword("BYVALUE")) {
                    /* the parameters received by value: for a MAIN
                     * procedure the words R1 points at, and NOEXECOPS
                     * (SC26-3114-01, ENTRY statement options) */
                    beyond_f(tk(0)->line, L_MVS, "BYVALUE");
                    byvalue = 1;
                    noexecops = 1;
                }
                else if (isword("BYADDR"))
                    beyond_f(tk(0)->line, L_MVS, "BYADDR");
                else if (isword("NOEXECOPS")) {
                    /* PL/I for MVS & VM (SC26-3114-01, pp. 103, 122);
                     * in neither GC33-0009-4 nor SC33-0006-7 (1985) */
                    beyond_f(tk(0)->line, L_MVS, "NOEXECOPS");
                    noexecops = 1;         /* Tier 82 */
                }
                else if (!isword("REENTRANT"))
                    fatal("%s(%d): procedure option %s is not supported yet", src_name,
                          tk(0)->line, is(T_IDENT) ? tk(0)->text : "?");
                pos++;
                if (is(T_COMMA)) { pos++; continue; }
                if (is(T_IDENT))
                    continue;              /* OPTIONS(MAIN NOEXECOPS) */
                break;
            }
            expect(T_RP, "')'");
        } else if (isword("RETURNS")) {
            pos++;
            expect(T_LP, "'('");
            attrs(&p->ret, p->line);
            expect(T_RP, "')'");
            p->has_ret = 1;
        } else if (isword("RECURSIVE")) {
            p->recursive = 1;
            pos++;
        } else if (isword("REORDER") || isword("ORDER")) {
            pos++;
        } else if (isword2("RED", "REDUCIBLE") || isword2("IRRED", "IRREDUCIBLE")) {
            /* PL/I (F) takes them in a declaration only (IEM0182I here) */
            beyond_f(tk(0)->line, L_OPT, "REDUCIBLE or IRREDUCIBLE on a PROCEDURE statement");
            pos++;
        } else {
            fatal("%s(%d): procedure option %s is not supported yet", src_name, tk(0)->line,
                  is(T_IDENT) ? tk(0)->text : "?");
        }
    }
    pos++;
    p->is_main = main_opt;
    p->noexecops = noexecops;
    p->byvalue = byvalue;
    if (byvalue && !main_opt)
        error_at(p->line, "OPTIONS(BYVALUE) is supported for a MAIN procedure only");
    if (noexecops && !main_opt)
        error_at(p->line, "NOEXECOPS belongs with OPTIONS(MAIN)");
    if (parent && main_opt)
        error_at(p->line, "OPTIONS(MAIN) belongs on the external procedure");
    if (main_opt && p->nparams > 1 && !opt_plist && !noexecops)
        error_at(p->line, "the main procedure takes one parameter, the PARM (or more with "
                 "-plist or NOEXECOPS)");
    stmt_no++;
    cur = p;
    glabels = &p->name;                      /* END name closes it */
    nglabels = 1;
    p->body = group_body();
    cur = saved;
    return p;
}

/* *PROCESS [options]; between external procedures (PL/I (F)'s batched
 * compilation, Q1EXT): skipped.  The * stands in column 1, outside the
 * margins the lexer reads, so PROCESS is what is left of it. */
static void skip_process(void)
{
    while ((is(T_STAR) && tk(1)->kind == T_IDENT && strcmp(tk(1)->text, "PROCESS") == 0) ||
           (is(T_IDENT) && strcmp(tk(0)->text, "PROCESS") == 0 && tk(1)->kind != T_COLON)) {
        while (!is(T_SEMI) && !is(T_EOF)) {
            /* LANGLVL(SPROG) or (NOSPROG), with OS beside it (PL/I for
             * MVS & VM's compile-time option); the rest is ignored */
            if (is(T_IDENT) && strcmp(tk(0)->text, "LANGLVL") == 0 && tk(1)->kind == T_LP)
                for (int k = 2; tk(k)->kind != T_RP && tk(k)->kind != T_SEMI &&
                                tk(k)->kind != T_EOF; k++)
                    if (tk(k)->kind == T_IDENT && (strcmp(tk(k)->text, "SPROG") == 0 ||
                                                   strcmp(tk(k)->text, "NOSPROG") == 0))
                        opt_sprog = tk(k)->text[0] == 'S';
            /* the Optimizer's INTERRUPT (INT) and NOINTERRUPT (NINT) */
            if (is(T_IDENT) && (strcmp(tk(0)->text, "INTERRUPT") == 0 ||
                                strcmp(tk(0)->text, "INT") == 0)) {
                beyond_f(tk(0)->line, L_OPT, "the INTERRUPT option");
                opt_interrupt = !opt_lang_f;
            } else if (is(T_IDENT) && (strcmp(tk(0)->text, "NOINTERRUPT") == 0 ||
                                       strcmp(tk(0)->text, "NINT") == 0))
                opt_interrupt = 0;
            pos++;
        }
        if (is(T_SEMI))
            pos++;
    }
}

static struct proc *parse_external(void)
{
    struct proc *p;
    int pon = 0, poff = 0, at, nck;
    char **ck;
    skip_process();
    prefix(&pon, &poff);
    ck = pfx_checks;
    nck = pfx_nchecks;
    at = pos;
    if (!(is(T_IDENT) && tk(1)->kind == T_COLON && tk(2)->kind == T_IDENT &&
          (strcmp(tk(2)->text, "PROC") == 0 || strcmp(tk(2)->text, "PROCEDURE") == 0)))
        fatal("%s(%d): an external procedure must begin with  name: PROCEDURE ...;", src_name,
              tk(0)->line);
    p = parse_proc(at, NULL);
    p->pon = pon;
    p->poff = poff;
    p->checks = ck;
    p->nchecks = nck;
    default_pass(p);
    return p;
}

/* R(label): each replaced by the format list of the FORMAT statement
 * with that label, in the procedure or one around it; nested R's too */
static struct stmt *find_format(struct stmt *s, const char *name)
{
    for (; s; s = s->next) {
        struct stmt *f;
        if (s->k == S_FORMAT)
            for (int i = 0; i < s->nlabels; i++)
                if (strcmp(s->labels[i], name) == 0)
                    return s;
        if ((f = find_format(s->body, name)) || (f = find_format(s->els, name)))
            return f;
    }
    return NULL;
}

/* cnt items of in, or all of them for -1: an (expression) group keeps the
 * number of items it repeats once its R items are expanded */
static int expand_fmts(struct fmt *out, int n, const struct fmt *in, int cnt, struct proc *p,
                       int depth, int line)
{
    for (int i = 0; in[i].k != F_END && (cnt < 0 || i < cnt); i++) {
        if (in[i].k == F_REP) {
            int o = n;
            if (n >= MAXFMT - 1)
                fatal("%s(%d): format list too long", src_name, line);
            out[n++] = in[i];
            n = expand_fmts(out, n, in + i + 1, in[i].d, p, depth, line);
            out[o].d = n - o - 1;
            i += in[i].d;
            continue;
        }
        if (in[i].k == F_R) {
            struct stmt *f = NULL;
            for (struct proc *b = p; b && !f; b = b->parent)
                f = find_format(b->body, in[i].pic);
            if (!f)
                fatal("%s(%d): R(%s): no FORMAT statement of that label (a label variable "
                      "is not supported yet)", src_name, line, in[i].pic);
            if (depth > 16)
                fatal("%s(%d): R(%s): FORMAT statements that name each other", src_name, line,
                      in[i].pic);
            n = expand_fmts(out, n, f->fmts, -1, p, depth + 1, line);
            continue;
        }
        if (n >= MAXFMT - 1)
            fatal("%s(%d): format list too long", src_name, line);
        out[n++] = in[i];
    }
    return n;
}

static void expand_remote(struct stmt *s, struct proc *p)
{
    for (; s; s = s->next) {
        if (s->fmts && s->k != S_FORMAT) {
            int has_r = 0;
            for (int i = 0; s->fmts[i].k != F_END; i++)
                if (s->fmts[i].k == F_R)
                    has_r = 1;
            if (has_r) {
                struct fmt *out = xmalloc(MAXFMT * sizeof *out);
                int n = expand_fmts(out, 0, s->fmts, -1, p, 0, s->line);
                out[n].k = F_END;
                s->fmts = out;
                s->nfmts = n;
            }
        }
        expand_remote(s->body, p);
        expand_remote(s->els, p);
    }
}

static void expand_proc(struct proc *p)
{
    expand_remote(p->body, p);
    for (struct proc *c = p->child; c; c = c->sibling)
        expand_proc(c);
}

/* The source: one or more external procedures, one with OPTIONS(MAIN),
 * linked through next_ext; the main one first. */
struct proc *parse_program(struct lexer *lx)
{
    struct proc *first = NULL, **tail = &first, *main = NULL;

    L = lx;
    pos = 0;
    stmt_no = 0;
    cur = NULL;
    do {
        struct proc *p = parse_external();
        if (p->is_main) {
            if (main)
                fatal("%s(%d): a second procedure with OPTIONS(MAIN)", src_name, p->line);
            main = p;
        }
        *tail = p;
        tail = &p->next_ext;
        skip_process();
    } while (!is(T_EOF));
    for (struct proc *p = first; p; p = p->next_ext)
        expand_proc(p);
    /* the main procedure first, id 0; the others numbered on.  A source
     * without one is compiled apart, for a program another source has the
     * main procedure of */
    if (!main) {
        for (struct proc *p = first; p; p = p->next_ext)
            p->id = ++nproc;
        return first;
    }
    if (first != main) {
        struct proc **q = &first;
        while (*q != main)
            q = &(*q)->next_ext;
        *q = main->next_ext;
        main->next_ext = first;
        first = main;
    }
    for (struct proc *p = first->next_ext; p; p = p->next_ext)
        p->id = ++nproc;
    return first;
}
