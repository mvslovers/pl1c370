/* pp.c -- the preprocessor: PL/I (F)'s compile-time facilities.
 *
 * The source stream (columns 2-72 of every record, lex.c) is scanned
 * once, as PL/I (F)'s preprocessor scans it (Language Reference
 * GC28-8201-4, chapter 16; measured, Tier 27):
 *   - strings, comments and numbers go to the output as they are;
 *   - an identifier that is an active preprocessor variable with a value
 *     is replaced by the value, a blank on each side, and the value is
 *     scanned again for replacements;
 *   - a statement beginning with % is executed and replaced by a blank:
 *     %DECLARE of FIXED (DECIMAL(5,0)) and CHARACTER (VARYING) variables,
 *     assignment, %IF ... %THEN unit [%ELSE unit], %DO; and %DO i = a TO
 *     b [BY c]; groups ended by %END, %GO TO a %label, %ACTIVATE,
 *     %DEACTIVATE, %INCLUDE (its members are scanned too) and the null
 *     statement.
 * A FIXED value as text is 8 characters, right-justified.  A
 * preprocessor procedure (%name: PROCEDURE ... %END;), declared by
 * %DECLARE name ENTRY(...) RETURNS(...), is invoked from expressions and
 * from the program's text; its statements have no %.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "pl1c.h"

struct ppvar {
    char  name[32];
    int   ischar, active, hasval;
    long  fix;
    char *str;
    int   isentry, nparm, pchar[16], retchar;  /* a preprocessor procedure */
    int   running;           /* being invoked: no recursion (IEM4452I) */
    int   set;               /* a parameter: its argument given (PARMSET) */
    int   norescan;          /* %ACTIVATE ... NORESCAN: the value not scanned again */
};

/* The preprocessor built-in functions beyond SUBSTR, the Optimizer's
 * (GC33-0009-4, chapter 16; SC26-3114-01, "Preprocessor Built-In
 * Functions"): always active in preprocessor statements, in the
 * program's text once %ACTIVATE or %DECLARE ... BUILTIN names them.
 * PL/I (F) has none of them (IEM4130I, Q1PPB, Q3PPC). */
static const char *const bi_names[] = { "INDEX", "LENGTH", "COUNTER", "COMPILETIME",
                                        "PARMSET", "SUBSTR" };
static int bi_active[6];
static long counter;

static int bi_of(const char *w)
{
    for (int i = 0; i < 6; i++)
        if (strcmp(w, bi_names[i]) == 0)
            return i;
    return -1;
}

#define MAXPPVARS 256
static struct ppvar vars[MAXPPVARS];
static int nvars;

static struct lexer *L;         /* the input, %INCLUDE splices into it */
static char *os;                /* the output stream */
static int *olo, on, ocap;
static int curline;
static int jumped;              /* a %GO TO is on its way out of groups */

/* a name into a variable's name[32]: cut at 31 characters, as before */
static void set_name(char *dst, size_t n, const char *src)
{
    size_t l = strlen(src);
    if (l >= n)
        l = n - 1;
    memcpy(dst, src, l);
    dst[l] = 0;
}

static void out_ch(char c)
{
    if (on + 2 > ocap) {
        ocap = ocap ? 2 * ocap : 8192;
        os = realloc(os, (size_t)ocap);
        olo = realloc(olo, (size_t)ocap * sizeof *olo);
        if (!os || !olo)
            fatal("out of memory");
    }
    os[on] = c;
    olo[on++] = curline;
}

static int isidst(int c)
{
    return isalpha(c) || c == '$' || c == '#' || c == '@';
}

static int isidch(int c)
{
    return isalnum(c) || c == '_' || c == '$' || c == '#' || c == '@';
}

static int lframe;               /* where a procedure's own variables begin */

/* the latest one: a procedure's variables hide the others */
static struct ppvar *var(const char *name)
{
    for (int i = nvars - 1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0)
            return &vars[i];
    return NULL;
}

static void pp_error(const char *fmt, const char *a)
{
    char m[200];
    snprintf(m, sizeof m, fmt, a);
    fatal("%s(%d): %s", src_name, curline, m);
}

/* ---- the input --------------------------------------------------------- */

static int skipb(int p)
{
    for (;;) {
        while (p < L->n && L->s[p] == ' ')
            p++;
        if (p + 1 < L->n && L->s[p] == '/' && L->s[p + 1] == '*') {
            p += 2;
            while (p + 1 < L->n && !(L->s[p] == '*' && L->s[p + 1] == '/'))
                p++;
            p += 2;
            continue;
        }
        return p;
    }
}

/* the identifier at p, upper case, into w; returns after it */
static int ident(int p, char *w, int wn)
{
    int k = 0;
    while (p < L->n && isidch((unsigned char)L->s[p])) {
        if (k < wn - 1)
            w[k++] = (char)toupper((unsigned char)L->s[p]);
        p++;
    }
    w[k] = '\0';
    return p;
}

static int is_word(int p, const char *w)
{
    size_t n = strlen(w);
    return p + (int)n <= L->n && strncasecmp(L->s + p, w, n) == 0 &&
           !isidch((unsigned char)L->s[p + n]);
}

/* past a string at p (its quotes doubled inside) */
static int past_string(int p)
{
    for (p++; p < L->n; p++)
        if (L->s[p] == '\'') {
            if (p + 1 < L->n && L->s[p + 1] == '\'')
                p++;
            else
                return p + 1;
        }
    pp_error("%s", "a string is not closed");
    return p;
}

/* past the ; ending a statement, strings and comments skipped */
static int past_semi(int p)
{
    while (p < L->n) {
        if (L->s[p] == '\'')
            p = past_string(p);
        else if (L->s[p] == '/' && p + 1 < L->n && L->s[p + 1] == '*')
            p = skipb(p);
        else if (L->s[p++] == ';')
            return p;
    }
    pp_error("%s", "a preprocessor statement without ';'");
    return p;
}

/* ---- expressions --------------------------------------------------------- */

struct val { int ischar; long n; char *s; int omitted; };

static char *xsdup(const char *s, int n)
{
    char *r = xmalloc((size_t)n + 1);
    memcpy(r, s, (size_t)n);
    r[n] = '\0';
    return r;
}

/* a FIXED value as text: 8 characters, right-justified (chapter 16) */
static char *fix_text(long n)
{
    char b[32];
    snprintf(b, sizeof b, "%8ld", n);
    return xstrdup(b);
}

static long to_fix(struct val v)
{
    if (!v.ischar)
        return v.n;
    {
        char *e;
        long n;
        const char *s = v.s;
        while (*s == ' ')
            s++;
        n = strtol(s, &e, 10);
        while (*e == ' ')
            e++;
        if (*e)
            pp_error("'%s' is not a decimal integer", v.s);
        return n;
    }
}

static char *to_char(struct val v)
{
    return v.ischar ? v.s : fix_text(v.n);
}

static struct val fixv(long n)
{
    struct val v;
    v.ischar = 0;
    v.n = n % 100000;            /* DECIMAL(5,0) */
    v.s = NULL;
    v.omitted = 0;
    return v;
}

static struct val charv(char *s)
{
    struct val v;
    v.ischar = 1;
    v.n = 0;
    v.s = s;
    v.omitted = 0;
    return v;
}

static int ep;                   /* the expression's position */

/* A built-in function of bi_names, k, with its arguments. */
static struct val bi_eval(int k, struct val *a, int na, const char *pname)
{
    static const int nargs[] = { 2, 1, 0, 0, 1, -1 };
    char w[64];
    if (nargs[k] != na)
        pp_error("%s: the wrong number of arguments", bi_names[k]);
    snprintf(w, sizeof w, "the preprocessor built-in function %s", bi_names[k]);
    beyond_f(curline, L_OPT, w);
    switch (k) {
    case 0: {                                   /* INDEX(x, y) */
        char *x = to_char(a[0]), *y = to_char(a[1]), *f;
        if (!*x || !*y)
            return fixv(0);
        f = strstr(x, y);
        return fixv(f ? (long)(f - x) + 1 : 0);
    }
    case 1:                                     /* LENGTH(x) */
        return fixv((long)strlen(to_char(a[0])));
    case 2: {                                   /* COUNTER: 00001, ... */
        char b[8];
        if (++counter > 99999) {
            counter = 0;
            pp_error("%s", "COUNTER beyond 99999");
        }
        snprintf(b, sizeof b, "%05ld", counter);
        return charv(xstrdup(b));
    }
    case 3: {                                   /* COMPILETIME: DD MMM YY HH.MM.SS */
        static const char *mon[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG",
                                     "SEP", "OCT", "NOV", "DEC" };
        time_t t = time(NULL);
        struct tm *m = localtime(&t);
        char b[24];
        snprintf(b, sizeof b, "%2d %s %02d %02d.%02d.%02d", m->tm_mday, mon[m->tm_mon],
                 m->tm_year % 100, m->tm_hour, m->tm_min, m->tm_sec);
        return charv(xstrdup(b));
    }
    case 4: {                                   /* PARMSET(x): '1' or '0' */
        /* the parameter itself: the frame's first entries, before the
         * body's DECLARE of it */
        for (int i = lframe; i < nvars; i++)
            if (strcmp(vars[i].name, pname) == 0)
                return charv(xstrdup(vars[i].set ? "1" : "0"));
        pp_error("PARMSET: %s is not a parameter of this procedure", pname);
        return fixv(0);
    }
    }
    return fixv(0);
}

static struct val expr_or(void);
static struct val invoke(struct ppvar *v, struct val *args, int nargs);

static struct val primary(void)
{
    ep = skipb(ep);
    if (ep >= L->n)
        pp_error("%s", "an expression ends too early");
    if (L->s[ep] == '(') {
        struct val v;
        ep++;
        v = expr_or();
        ep = skipb(ep);
        if (L->s[ep] != ')')
            pp_error("%s", "')' expected");
        ep++;
        return v;
    }
    if (L->s[ep] == '\'') {
        int q = past_string(ep), k = 0;
        char *s = xmalloc((size_t)(q - ep));
        for (int i = ep + 1; i < q - 1; i++) {
            s[k++] = L->s[i];
            if (L->s[i] == '\'')
                i++;
        }
        s[k] = '\0';
        ep = q;
        return charv(s);
    }
    if (isdigit((unsigned char)L->s[ep])) {
        long n = 0;
        while (ep < L->n && isdigit((unsigned char)L->s[ep]))
            n = n * 10 + (L->s[ep++] - '0');
        return fixv(n);
    }
    if (isidst((unsigned char)L->s[ep])) {
        char w[32];
        struct ppvar *v;
        ep = ident(ep, w, sizeof w);
        if (strcmp(w, "SUBSTR") == 0) {
            struct val s, i, j;
            char *t;
            long ii, jj, len;
            ep = skipb(ep);
            if (L->s[ep] != '(')
                pp_error("%s", "SUBSTR(");
            ep++;
            s = expr_or();
            ep = skipb(ep);
            if (L->s[ep] != ',')
                pp_error("%s", "SUBSTR needs a position");
            ep++;
            i = expr_or();
            ep = skipb(ep);
            t = to_char(s);
            len = (long)strlen(t);
            ii = to_fix(i);
            jj = len - ii + 1;
            if (L->s[ep] == ',') {
                ep++;
                j = expr_or();
                jj = to_fix(j);
                ep = skipb(ep);
            }
            if (L->s[ep] != ')')
                pp_error("%s", "')' expected");
            ep++;
            if (ii < 1 || jj < 0 || ii - 1 + jj > len)
                pp_error("%s", "SUBSTR out of the string");
            return charv(xsdup(t + ii - 1, (int)jj));
        }
        v = var(w);
        if ((!v || v->isentry == 0) && bi_of(w) >= 0 && !(v && v->hasval)) {
            /* a built-in function (a procedure of that name wins) */
            int k = bi_of(w), na = 0;
            struct val args[2];
            char pname[32] = "";
            ep = skipb(ep);
            if (k == 4) {
                /* PARMSET(x): x a parameter's name, not its value */
                if (L->s[ep] != '(')
                    pp_error("%s", "PARMSET(");
                ep = skipb(ident(skipb(ep + 1), pname, sizeof pname));
                if (L->s[ep] != ')')
                    pp_error("%s", "')' expected");
                ep++;
                na = 1;
            } else if (L->s[ep] == '(' && k != 2 && k != 3) {
                ep++;
                for (;;) {
                    if (na == 2)
                        pp_error("%s: too many arguments", w);
                    args[na++] = expr_or();
                    ep = skipb(ep);
                    if (L->s[ep] == ',') { ep++; continue; }
                    if (L->s[ep] != ')')
                        pp_error("%s", "')' expected");
                    ep++;
                    break;
                }
            }
            {
                int save = ep;
                struct val r = bi_eval(k, args, na, pname);
                ep = save;
                return r;
            }
        }
        if (!v)
            pp_error("%s is not a preprocessor variable", w);
        if (v->isentry) {
            /* a preprocessor function: its arguments are expressions */
            struct val args[16];
            int na = 0;
            ep = skipb(ep);
            if (L->s[ep] == '(') {
                ep++;
                for (;;) {
                    if (na == 16)
                        pp_error("%s: too many arguments", w);
                    ep = skipb(ep);
                    if (L->s[ep] == ',' || L->s[ep] == ')') {
                        /* an omitted argument: PARMSET is 0 for it */
                        beyond_f(curline, L_OPT, "an omitted argument");
                        args[na] = charv("");
                        args[na++].omitted = 1;
                    } else {
                        args[na++] = expr_or();
                    }
                    ep = skipb(ep);
                    if (L->s[ep] == ',') { ep++; continue; }
                    if (L->s[ep] != ')')
                        pp_error("%s", "')' expected");
                    ep++;
                    break;
                }
            }
            {
                int save = ep;
                struct val r = invoke(v, args, na);
                ep = save;
                return r;
            }
        }
        if (!v->hasval)
            pp_error("%s has no value", w);
        return v->ischar ? charv(v->str) : fixv(v->fix);
    }
    pp_error("%s", "an operand expected");
    return fixv(0);
}

static struct val prefix(void)
{
    ep = skipb(ep);
    if (L->s[ep] == '-') {
        ep++;
        return fixv(-to_fix(prefix()));
    }
    if (L->s[ep] == '+') {
        ep++;
        return fixv(to_fix(prefix()));
    }
    if (L->s[ep] == '^') {
        ep++;
        return fixv(!to_fix(prefix()));
    }
    return primary();
}

static struct val mult(void)
{
    struct val a = prefix();
    for (;;) {
        ep = skipb(ep);
        if (L->s[ep] == '*' && L->s[ep + 1] != '*') {
            ep++;
            a = fixv(to_fix(a) * to_fix(prefix()));
        } else if (L->s[ep] == '/' && L->s[ep + 1] != '*') {
            long d;
            ep++;
            d = to_fix(prefix());
            if (d == 0)
                pp_error("%s", "division by zero");
            a = fixv(to_fix(a) / d);
        } else {
            return a;
        }
    }
}

static struct val add(void)
{
    struct val a = mult();
    for (;;) {
        ep = skipb(ep);
        if (L->s[ep] == '+') {
            ep++;
            a = fixv(to_fix(a) + to_fix(mult()));
        } else if (L->s[ep] == '-') {
            ep++;
            a = fixv(to_fix(a) - to_fix(mult()));
        } else {
            return a;
        }
    }
}

static struct val cat(void)
{
    struct val a = add();
    for (;;) {
        ep = skipb(ep);
        if (L->s[ep] == '|' && L->s[ep + 1] == '|') {
            char *x, *y, *r;
            ep += 2;
            x = to_char(a);
            y = to_char(add());
            r = xmalloc(strlen(x) + strlen(y) + 1);
            strcpy(r, x);
            strcat(r, y);
            a = charv(r);
        } else {
            return a;
        }
    }
}

static int cmp_vals(struct val a, struct val b)
{
    if (a.ischar && b.ischar) {
        /* the shorter padded with blanks */
        size_t la = strlen(a.s), lb = strlen(b.s), n = la > lb ? la : lb;
        for (size_t i = 0; i < n; i++) {
            int x = i < la ? (unsigned char)a.s[i] : ' ', y = i < lb ? (unsigned char)b.s[i] : ' ';
            if (x != y)
                return x < y ? -1 : 1;
        }
        return 0;
    }
    {
        long x = to_fix(a), y = to_fix(b);
        return x < y ? -1 : x > y;
    }
}

static struct val compare(void)
{
    struct val a = cat();
    for (;;) {
        int op = 0;
        ep = skipb(ep);
        if (L->s[ep] == '<' && L->s[ep + 1] == '=') { op = 1; ep += 2; }
        else if (L->s[ep] == '>' && L->s[ep + 1] == '=') { op = 2; ep += 2; }
        else if (L->s[ep] == '^' && L->s[ep + 1] == '=') { op = 3; ep += 2; }
        else if (L->s[ep] == '<') { op = 4; ep++; }
        else if (L->s[ep] == '>') { op = 5; ep++; }
        else if (L->s[ep] == '=') { op = 6; ep++; }
        else return a;
        {
            int c = cmp_vals(a, cat());
            int r = op == 1 ? c <= 0 : op == 2 ? c >= 0 : op == 3 ? c != 0 : op == 4 ? c < 0 :
                    op == 5 ? c > 0 : c == 0;
            a = fixv(r);
        }
    }
}

static struct val expr_and(void)
{
    struct val a = compare();
    for (;;) {
        ep = skipb(ep);
        if (L->s[ep] == '&') {
            ep++;
            a = fixv(to_fix(a) && to_fix(compare()));
        } else {
            return a;
        }
    }
}

static struct val expr_or(void)
{
    struct val a = expr_and();
    for (;;) {
        ep = skipb(ep);
        if (L->s[ep] == '|' && L->s[ep + 1] != '|') {
            ep++;
            a = fixv(to_fix(a) || to_fix(expr_and()));
        } else {
            return a;
        }
    }
}

/* ---- replacement ------------------------------------------------------- */

static void emit_text(const char *t, int depth);

static void replace(struct ppvar *v, int depth)
{
    char *t = v->ischar ? v->str : fix_text(v->fix);
    if (depth > 50)
        pp_error("the replacement of %s does not end", v->name);
    out_ch(' ');
    if (v->norescan)
        for (int i = 0; t[i]; i++)
            out_ch(t[i]);                       /* NORESCAN: as it is */
    else
        emit_text(t, depth + 1);
    out_ch(' ');
}

/* text of a replacement value, scanned again for active variables */
static void emit_text(const char *t, int depth)
{
    for (int i = 0; t[i];) {
        unsigned char c = (unsigned char)t[i];
        if (c == '\'') {
            out_ch(t[i++]);
            while (t[i] && t[i] != '\'')
                out_ch(t[i++]);
            if (t[i])
                out_ch(t[i++]);
        } else if (isdigit(c)) {
            while (t[i] && isidch((unsigned char)t[i]))
                out_ch(t[i++]);
        } else if (isidst(c)) {
            char w[32];
            int k = 0, j = i;
            struct ppvar *v;
            while (t[j] && isidch((unsigned char)t[j])) {
                if (k < 31)
                    w[k++] = (char)toupper((unsigned char)t[j]);
                j++;
            }
            w[k] = '\0';
            v = var(w);
            if (v && v->active && v->hasval) {
                replace(v, depth);
            } else {
                for (; i < j; i++)
                    out_ch(t[i]);
            }
            i = j;
        } else {
            out_ch(t[i++]);
        }
    }
}

/* ---- statements ---------------------------------------------------------- */

static int run(int p, int until_end);

/* the % statement at p as a whole: its end, %DO groups included */
static int skip_unit(int p)
{
    int q = skipb(p + 1);
    char w[32];
    while (isidst((unsigned char)L->s[q])) {
        int r = skipb(ident(q, w, sizeof w));
        if (L->s[r] != ':')
            break;
        q = skipb(r + 1);
    }
    if (is_word(q, "DO")) {
        /* to the matching %END */
        int depth = 1, r = past_semi(q);
        while (r < L->n && depth > 0) {
            if (L->s[r] == '\'') {
                r = past_string(r);
            } else if (L->s[r] == '/' && L->s[r + 1] == '*') {
                r = skipb(r);
            } else if (L->s[r] == '%') {
                int s = skipb(r + 1);
                while (isidst((unsigned char)L->s[s])) {
                    int t = skipb(ident(s, w, sizeof w));
                    if (L->s[t] != ':')
                        break;
                    s = skipb(t + 1);
                }
                if (is_word(s, "DO")) {
                    depth++;
                    r = past_semi(s);
                } else if (is_word(s, "END")) {
                    depth--;
                    r = past_semi(s);
                } else {
                    r = s;    /* %IF ... %THEN %DO: the %DO counts too */
                }
            } else {
                r++;
            }
        }
        return r;
    }
    if (is_word(q, "IF")) {
        /* %IF e %THEN unit [%ELSE unit] */
        int r = q + 2;
        while (r < L->n && !(L->s[r] == '%' && is_word(skipb(r + 1), "THEN"))) {
            if (L->s[r] == '\'')
                r = past_string(r);
            else
                r++;
        }
        r = skip_unit(skipb(skipb(r + 1) + 4));     /* the unit after %THEN */
        {
            int e = skipb(r);
            if (L->s[e] == '%' && is_word(skipb(e + 1), "ELSE"))
                r = skip_unit(skipb(skipb(e + 1) + 4));
        }
        return r;
    }
    return past_semi(q);
}

/* the position of %label: */
static int find_label(const char *name)
{
    for (int p = 0; p < L->n; p++) {
        if (L->s[p] == '\'') {
            p = past_string(p) - 1;
        } else if (L->s[p] == '/' && L->s[p + 1] == '*') {
            p = skipb(p) - 1;
        } else if (L->s[p] == '%') {
            int q = skipb(p + 1);
            char w[32];
            while (isidst((unsigned char)L->s[q])) {
                int r = skipb(ident(q, w, sizeof w));
                if (L->s[r] != ':')
                    break;
                if (strcmp(w, name) == 0)
                    return p;
                q = skipb(r + 1);
            }
        }
    }
    pp_error("%%GO TO %s: no such label", name);
    return 0;
}

static void assign(struct ppvar *v, struct val x);

static void declare(int q)
{
    for (;;) {
        char names[16][32];
        int nn = 0, ischar = 0;
        q = skipb(q);
        if (L->s[q] == '(') {
            q++;
            for (;;) {
                q = skipb(q);
                if (nn == 16)
                    pp_error("%s", "too many names");
                q = skipb(ident(q, names[nn++], 32));
                if (L->s[q] == ',') { q++; continue; }
                if (L->s[q] != ')')
                    pp_error("%s", "')' expected");
                q++;
                break;
            }
        } else {
            q = skipb(ident(q, names[nn++], 32));
        }
        q = skipb(q);
        if (is_word(q, "BUILTIN")) {
            /* %DECLARE COUNTER BUILTIN: the built-in function, active in
             * the program's text (GC33-0009-4) */
            for (int i = 0; i < nn; i++) {
                if (bi_of(names[i]) < 0)
                    pp_error("%.31s is not a preprocessor built-in function", names[i]);
                beyond_f(curline, L_OPT, "a preprocessor built-in function declared");
                bi_active[bi_of(names[i])] = 1;
            }
            q = skipb(q + 7);
            if (L->s[q] == ',') { q++; continue; }
            return;
        }
        if (is_word(q, "ENTRY")) {
            /* name ENTRY [(attributes)] [RETURNS(attribute)] */
            struct ppvar e;
            memset(&e, 0, sizeof e);
            e.isentry = 1;
            q = skipb(q + 5);
            if (L->s[q] == '(') {
                q++;
                for (;;) {
                    q = skipb(q);
                    if (e.nparm == 16)
                        pp_error("%s", "too many parameters");
                    if (is_word(q, "FIXED")) {
                        e.pchar[e.nparm++] = 0;
                        q += 5;
                    } else if (is_word(q, "CHARACTER") || is_word(q, "CHAR")) {
                        e.pchar[e.nparm++] = 1;
                        q += is_word(q, "CHAR") ? 4 : 9;
                    } else {
                        pp_error("%s", "a parameter is FIXED or CHARACTER");
                    }
                    q = skipb(q);
                    if (L->s[q] == ',') { q++; continue; }
                    if (L->s[q] != ')')
                        pp_error("%s", "')' expected");
                    q++;
                    break;
                }
            }
            q = skipb(q);
            if (is_word(q, "RETURNS")) {
                char a[32];
                q = skipb(q + 7);
                if (L->s[q] != '(')
                    pp_error("%s", "RETURNS(");
                q = skipb(ident(skipb(q + 1), a, sizeof a));
                e.retchar = strcmp(a, "CHARACTER") == 0 || strcmp(a, "CHAR") == 0;
                if (L->s[q] != ')')
                    pp_error("%s", "')' expected");
                q++;
            } else {
                /* the result's type from the %PROCEDURE statement alone
                 * (SC26-3114-01); PL/I (F) needs RETURNS here too */
                beyond_f(curline, L_OPT, "%DECLARE ENTRY without RETURNS");
            }
            for (int i = 0; i < nn; i++) {
                struct ppvar *v = var(names[i]);
                if (!v) {
                    if (nvars == MAXPPVARS)
                        pp_error("%s", "too many preprocessor variables");
                    v = &vars[nvars++];
                }
                e.active = 1;
                set_name(e.name, sizeof e.name, names[i]);
                *v = e;
            }
            q = skipb(q);
            if (L->s[q] == ',') { q++; continue; }
            break;
        }
        if (is_word(q, "FIXED")) {
            q += 5;
        } else if (is_word(q, "CHARACTER") || is_word(q, "CHAR")) {
            ischar = 1;
            q += is_word(q, "CHAR") ? 4 : 9;
        } else {
            pp_error("%s", "a preprocessor variable is FIXED or CHARACTER");
        }
        for (int i = 0; i < nn; i++) {
            struct ppvar *v = var(names[i]);
            if (v && v - vars < lframe)
                v = NULL;           /* in a procedure: one of its own */
            if (!v) {
                if (nvars == MAXPPVARS)
                    pp_error("%s", "too many preprocessor variables");
                v = &vars[nvars++];
                memset(v, 0, sizeof *v);
                set_name(v->name, sizeof v->name, names[i]);
            } else if (v->hasval && v->ischar != ischar) {
                /* a parameter bound before its DCL in the procedure
                 * (%DECLARE name ENTRY without attributes): its value
                 * converted to the declared type */
                struct val x = v->ischar ? charv(v->str) : fixv(v->fix);
                v->ischar = ischar;
                assign(v, x);
            }
            v->ischar = ischar;
            v->active = 1;
        }
        q = skipb(q);
        if (L->s[q] == ',') { q++; continue; }
        break;
    }
}

static void assign(struct ppvar *v, struct val x)
{
    if (v->ischar) {
        v->str = to_char(x);
    } else {
        v->fix = to_fix(x) % 100000;
    }
    v->hasval = 1;
}

/* NOTE(message [, code]): a diagnostic of the program's own, severity
 * I, W, E, S or U for 0, 4, 8, 12, 16 (another code: the next lower,
 * U beyond 0 to 16); E and S fail the compilation, U ends it now
 * (GC33-0009-4, SC26-3114-01 "%NOTE Statement") */
static int note(int q)
{
    static const char sev[] = "IWESU";
    char *msg;
    long code = 0;
    int k;
    beyond_f(curline, L_OPT, "%NOTE");
    q = skipb(q + 4);
    if (L->s[q] != '(')
        pp_error("%s", "'(' expected after NOTE");
    ep = q + 1;
    msg = to_char(expr_or());
    ep = skipb(ep);
    if (L->s[ep] == ',') {
        ep++;
        code = to_fix(expr_or());
        ep = skipb(ep);
    }
    if (L->s[ep] != ')')
        pp_error("%s", "')' expected");
    k = code < 0 || code > 16 ? 4 : (int)code / 4;
    if (code >= 0 && code <= 16 && code % 4)
        fprintf(stderr, "%s(%d): %%NOTE code %ld is not 0, 4, 8, 12 or 16: %c taken\n",
                src_name, curline, code, sev[k]);
    if (k >= 2)
        error_at(curline, "%c %s", sev[k], msg);
    else
        fprintf(stderr, "%s(%d): %c %s\n", src_name, curline, sev[k], msg);
    if (k == 4)
        exit(8);
    return past_semi(ep);
}

/* the % statement at p: executed; returns where the scan goes on */
static int exec(int p)
{
    int q = skipb(p + 1);
    char w[32];

    curline = L->lineof[p < L->n ? p : L->n - 1];
    /* labels */
    while (isidst((unsigned char)L->s[q])) {
        int r = skipb(ident(q, w, sizeof w));
        if (L->s[r] != ':')
            break;
        q = skipb(r + 1);
    }
    if (L->s[q] == ';')
        return q + 1;
    if (is_word(q, "INCLUDE"))
        return pp_include(L, p, curline);    /* the members are scanned */
    if (is_word(q, "PAGE") || is_word(q, "SKIP") || is_word(q, "PRINT") ||
        is_word(q, "NOPRINT") || is_word(q, "CONTROL")) {
        /* the listing control statements: no source listing here, so no
         * effect (GC33-0009-4, "Listing Control Statements") */
        beyond_f(curline, L_OPT, "a listing control statement");
        return past_semi(q);
    }
    if (is_word(q, "NOTE"))
        return note(q);
    if (is_word(q, "PROCESS")) {
        /* %PROCESS, PL/I for MVS & VM's synonym of *PROCESS: the text
         * from PROCESS on goes on, as *PROCESS leaves it (parse.c) */
        beyond_f(curline, L_MVS, "%PROCESS");
        return q;
    }
    if (is_word(q, "DECLARE") || is_word(q, "DCL")) {
        declare(q + (is_word(q, "DCL") ? 3 : 7));
        return past_semi(q);
    }
    if (is_word(q, "ACTIVATE") || is_word(q, "DEACTIVATE")) {
        int on = is_word(q, "ACTIVATE");
        q += on ? 8 : 10;
        for (;;) {
            struct ppvar *v;
            q = skipb(ident(skipb(q), w, sizeof w));
            v = var(w);
            if (!v && bi_of(w) >= 0) {
                beyond_f(curline, L_OPT, "a preprocessor built-in function activated");
                bi_active[bi_of(w)] = on;
            } else if (!v) {
                pp_error("%s is not a preprocessor variable", w);
            } else {
                v->active = on;
            }
            if (on && (is_word(q, "RESCAN") || is_word(q, "NORESCAN"))) {
                /* GC33-0009-4: the value scanned again, or not */
                beyond_f(curline, L_OPT, "RESCAN or NORESCAN");
                if (v)
                    v->norescan = is_word(q, "NORESCAN");
                q = skipb(q + (is_word(q, "RESCAN") ? 6 : 8));
            }
            if (L->s[q] == ',') { q++; continue; }
            break;
        }
        return past_semi(q);
    }
    if (is_word(q, "GO") || is_word(q, "GOTO")) {
        int r = skipb(q + 2);
        if (is_word(q, "GOTO"))
            r = skipb(q + 4);
        else if (is_word(r, "TO"))
            r = skipb(r + 2);
        ident(r, w, sizeof w);
        jumped = 1;
        return find_label(w);
    }
    if (is_word(q, "IF")) {
        int r, then, unit, e;
        struct val c;
        ep = q + 2;
        c = expr_or();
        r = skipb(ep);
        if (L->s[r] != '%' || !is_word(skipb(r + 1), "THEN"))
            pp_error("%s", "%THEN expected");
        then = skipb(skipb(r + 1) + 4);
        if (L->s[then] != '%')
            pp_error("%s", "the unit of %THEN is a preprocessor statement");
        unit = then;
        if (to_fix(c)) {
            r = exec(unit);
            if (jumped)
                return r;
            e = skipb(r);
            if (L->s[e] == '%' && is_word(skipb(e + 1), "ELSE"))
                r = skip_unit(skipb(skipb(e + 1) + 4));
            return r;
        }
        r = skip_unit(unit);
        e = skipb(r);
        if (L->s[e] == '%' && is_word(skipb(e + 1), "ELSE"))
            return exec(skipb(skipb(e + 1) + 4));
        return r;
    }
    if (is_word(q, "DO")) {
        int r = skipb(q + 2);
        if (L->s[r] == ';')
            return run(r + 1, 1);            /* a group: on to its %END */
        {
            /* %DO i = a TO b [BY c]; */
            struct ppvar *v;
            long a, b, step = 1;
            int body, endp;
            r = skipb(ident(r, w, sizeof w));
            v = var(w);
            if (!v || v->ischar)
                pp_error("%s: the %%DO variable is a FIXED preprocessor variable", w);
            if (L->s[r] != '=')
                pp_error("%s", "'=' expected");
            ep = r + 1;
            a = to_fix(expr_or());
            ep = skipb(ep);
            if (!is_word(ep, "TO"))
                pp_error("%s", "TO expected");
            ep += 2;
            b = to_fix(expr_or());
            ep = skipb(ep);
            if (is_word(ep, "BY")) {
                ep += 2;
                step = to_fix(expr_or());
                ep = skipb(ep);
            }
            if (L->s[ep] != ';')
                pp_error("%s", "';' expected");
            body = ep + 1;
            endp = skip_unit(p);
            for (v->fix = a, v->hasval = 1; step > 0 ? v->fix <= b : v->fix >= b;
                 v->fix += step) {
                int e = run(body, 1);
                if (jumped)
                    return e;
            }
            return endp;
        }
    }
    if (is_word(q, "END"))
        pp_error("%s", "%END without %DO");
    if (is_word(q, "PROC") || is_word(q, "PROCEDURE")) {
        /* a preprocessor procedure: run when invoked, not here; it ends
         * at its %END */
        int r = past_semi(q);
        while (r < L->n && L->s[r] != '%') {
            if (L->s[r] == '\'')
                r = past_string(r);
            else
                r++;
        }
        return past_semi(r + 1);
    }
    if (isidst((unsigned char)L->s[q])) {
        /* assignment */
        int r = skipb(ident(q, w, sizeof w));
        struct ppvar *v = var(w);
        if (L->s[r] != '=')
            pp_error("%s: a preprocessor statement expected", w);
        if (!v)
            pp_error("%s is not a preprocessor variable", w);
        ep = r + 1;
        assign(v, expr_or());
        ep = skipb(ep);
        if (L->s[ep] != ';')
            pp_error("%s", "';' expected");
        return ep + 1;
    }
    pp_error("%s", "a preprocessor statement expected");
    return p;
}

/* ---- preprocessor procedures ------------------------------------------- */

/* The body's statements have no %; a procedure ends at its %END. */

static int pstmt(int p, int end, struct val *ret, int *done);

/* past the THEN of the IF at p: strings and comments skipped, THEN taken
 * only as a word of its own */
static int past_then(int p)
{
    p += 2;
    while (p < L->n) {
        if (L->s[p] == '\'')
            p = past_string(p);
        else if (L->s[p] == '/' && p + 1 < L->n && L->s[p + 1] == '*')
            p = skipb(p);
        else if (L->s[p] == ';')
            break;
        else if (isidst((unsigned char)L->s[p])) {
            if (is_word(p, "THEN"))
                return p + 4;
            while (p < L->n && isidch((unsigned char)L->s[p]))
                p++;
        } else
            p++;
    }
    pp_error("%s", "THEN expected");
    return p;
}

/* past the body statement at p: a DO group with its END, an IF with its
 * THEN unit and its ELSE unit, each unit one of these again */
static int skip_pstmt(int p, int end)
{
    int q = skipb(p);
    char w[32];
    while (isidst((unsigned char)L->s[q])) {
        int r = skipb(ident(q, w, sizeof w));
        if (L->s[r] != ':')
            break;
        q = skipb(r + 1);
    }
    if (is_word(q, "IF")) {
        int r = skip_pstmt(past_then(q), end), e = skipb(r);
        return is_word(e, "ELSE") ? skip_pstmt(e + 4, end) : r;
    }
    if (!is_word(q, "DO"))
        return past_semi(q);
    q = past_semi(q);
    for (;;) {
        int s = skipb(q);
        if (s >= end)
            pp_error("%s", "DO without END in a preprocessor procedure");
        while (isidst((unsigned char)L->s[s])) {
            int r = skipb(ident(s, w, sizeof w));
            if (L->s[r] != ':')
                break;
            s = skipb(r + 1);
        }
        if (is_word(s, "END"))
            return past_semi(s);
        q = skip_pstmt(s, end);
    }
}

/* the position of label: among the body's statements */
static int body_label(int start, int end, const char *name)
{
    for (int q = start; q < end;) {
        int s = skipb(q);
        char w[32];
        while (isidst((unsigned char)L->s[s])) {
            int r = skipb(ident(s, w, sizeof w));
            if (L->s[r] != ':')
                break;
            if (strcmp(w, name) == 0)
                return s;
            s = skipb(r + 1);
        }
        q = past_semi(s);
    }
    pp_error("GO TO %s: no such label in the procedure", name);
    return start;
}

static int pbody_start, pbody_end;
static int pgoto = -1;           /* a GO TO in a procedure: its target */

/* statements from p to their END (until_end) or to the body's end */
static int prun(int p, int end, int until_end, struct val *ret, int *done)
{
    while (!*done) {
        int q = skipb(p);
        char w[32];
        if (q >= end)
            break;
        {
            int s = q;
            while (isidst((unsigned char)L->s[s])) {
                int r = skipb(ident(s, w, sizeof w));
                if (L->s[r] != ':')
                    break;
                s = skipb(r + 1);
            }
            if (is_word(s, "END")) {
                if (until_end)
                    return past_semi(s);
                pp_error("%s", "END without DO in a preprocessor procedure");
            }
        }
        p = pstmt(q, end, ret, done);
        if (pgoto >= 0) {
            if (until_end)
                return p;
            p = pgoto;
            pgoto = -1;
        }
    }
    return p;
}

static int pstmt(int p, int end, struct val *ret, int *done)
{
    int q = skipb(p);
    char w[32];
    while (isidst((unsigned char)L->s[q])) {
        int r = skipb(ident(q, w, sizeof w));
        if (L->s[r] != ':')
            break;
        q = skipb(r + 1);
    }
    if (L->s[q] == ';')
        return q + 1;
    if (is_word(q, "DECLARE") || is_word(q, "DCL")) {
        declare(q + (is_word(q, "DCL") ? 3 : 7));
        return past_semi(q);
    }
    if (is_word(q, "NOTE"))
        return note(q);
    if (is_word(q, "RETURN")) {
        ep = skipb(q + 6);
        *ret = expr_or();
        *done = 1;
        return past_semi(ep);
    }
    if (is_word(q, "GO") || is_word(q, "GOTO")) {
        int r = skipb(q + (is_word(q, "GOTO") ? 4 : 2));
        if (is_word(r, "TO"))
            r = skipb(r + 2);
        ident(r, w, sizeof w);
        pgoto = body_label(pbody_start, pbody_end, w);
        return pgoto;
    }
    if (is_word(q, "IF")) {
        struct val c;
        int r, e;
        ep = q + 2;
        c = expr_or();
        r = skipb(ep);
        if (!is_word(r, "THEN"))
            pp_error("%s", "THEN expected");
        r = skipb(r + 4);
        if (to_fix(c)) {
            r = pstmt(r, end, ret, done);
            if (*done || pgoto >= 0)
                return r;
            e = skipb(r);
            if (is_word(e, "ELSE"))
                r = skip_pstmt(skipb(e + 4), end);
            return r;
        }
        r = skip_pstmt(r, end);
        e = skipb(r);
        if (is_word(e, "ELSE"))
            return pstmt(skipb(e + 4), end, ret, done);
        return r;
    }
    if (is_word(q, "DO")) {
        int r = skipb(q + 2);
        if (L->s[r] == ';')
            return prun(r + 1, end, 1, ret, done);
        {
            struct ppvar *v;
            long a, b, step = 1;
            int body, endp;
            r = skipb(ident(r, w, sizeof w));
            v = var(w);
            if (!v || v->ischar || L->s[r] != '=')
                pp_error("%s: DO i = a TO b", w);
            ep = r + 1;
            a = to_fix(expr_or());
            ep = skipb(ep);
            if (!is_word(ep, "TO"))
                pp_error("%s", "TO expected");
            ep += 2;
            b = to_fix(expr_or());
            ep = skipb(ep);
            if (is_word(ep, "BY")) {
                ep += 2;
                step = to_fix(expr_or());
                ep = skipb(ep);
            }
            body = past_semi(ep);
            endp = skip_pstmt(q, end);
            for (v->fix = a, v->hasval = 1; step > 0 ? v->fix <= b : v->fix >= b;
                 v->fix += step) {
                int e2 = prun(body, end, 1, ret, done);
                if (*done || pgoto >= 0)
                    return e2;
            }
            return endp;
        }
    }
    if (isidst((unsigned char)L->s[q])) {
        int r = skipb(ident(q, w, sizeof w));
        struct ppvar *v = var(w);
        if (L->s[r] != '=' || !v)
            pp_error("%s: a statement of a preprocessor procedure expected", w);
        ep = r + 1;
        assign(v, expr_or());
        return past_semi(ep);
    }
    pp_error("%s", "a statement of a preprocessor procedure expected");
    return p;
}

/* Invoke procedure v: arguments bound to its parameters (fewer: 0 or the
 * null string, more: ignored), its statements run to a RETURN. */
static struct val invoke(struct ppvar *v, struct val *args, int nargs)
{
    int p = find_label(v->name), q, save = nvars, savef = lframe, saveline = curline;
    int sbs = pbody_start, sbe = pbody_end, done = 0;
    char w[32], pn[16][32];
    int np = 0, retchar = v->retchar;
    struct val r;

    if (v->running)
        /* PL/I (F): IEM4452I, a terminal error (JOB00409) */
        pp_error("recursive use of preprocessor procedure %s is not allowed", v->name);
    q = skipb(p + 1);
    while (isidst((unsigned char)L->s[q])) {
        int t = skipb(ident(q, w, sizeof w));
        if (L->s[t] != ':')
            break;
        q = skipb(t + 1);
    }
    if (!is_word(q, "PROCEDURE") && !is_word(q, "PROC"))
        pp_error("%s is not a preprocessor procedure", v->name);
    q = skipb(q + (is_word(q, "PROC") ? 4 : 9));
    if (L->s[q] == '(') {
        q++;
        for (;;) {
            if (np == 16)
                pp_error("%s", "too many parameters");
            q = skipb(ident(skipb(q), pn[np++], 32));
            if (L->s[q] == ',') { q++; continue; }
            if (L->s[q] != ')')
                pp_error("%s", "')' expected");
            q++;
            break;
        }
    }
    /* RETURNS(CHARACTER|FIXED) on the PROCEDURE statement gives the result's
     * type; %DECLARE name ENTRY has no RETURNS (SC26-3114-01) */
    for (q = skipb(q); q < L->n && L->s[q] != ';'; q = skipb(q)) {
        char a[32];
        int t = skipb(ident(q, w, sizeof w));
        if (t == q)
            pp_error("%s: a PROCEDURE option expected", v->name);
        q = t;
        if (strcmp(w, "RETURNS") != 0)
            continue;
        if (L->s[q] != '(')
            pp_error("%s", "RETURNS(");
        q = skipb(ident(skipb(q + 1), a, sizeof a));
        if (strcmp(a, "CHARACTER") == 0 || strcmp(a, "CHAR") == 0)
            retchar = 1;
        else if (strcmp(a, "FIXED") == 0)
            retchar = 0;
        else
            pp_error("%s: RETURNS(CHARACTER) or RETURNS(FIXED)", v->name);
        if (L->s[q] != ')')
            pp_error("%s", "')' expected");
        q++;
    }
    q = past_semi(q);
    pbody_start = q;
    for (pbody_end = q; pbody_end < L->n && L->s[pbody_end] != '%'; ) {
        if (L->s[pbody_end] == '\'')
            pbody_end = past_string(pbody_end);
        else
            pbody_end++;
    }
    lframe = nvars;
    for (int i = 0; i < np; i++) {
        struct ppvar *pv;
        if (nvars == MAXPPVARS)
            pp_error("%s", "too many preprocessor variables");
        pv = &vars[nvars++];
        memset(pv, 0, sizeof *pv);
        set_name(pv->name, sizeof pv->name, pn[i]);
        pv->ischar = i < v->nparm ? v->pchar[i] : 1;
        if (i < nargs && !args[i].omitted) {
            assign(pv, args[i]);
            pv->set = 1;
        } else {
            assign(pv, pv->ischar ? charv("") : fixv(0));
        }
    }
    r = fixv(0);
    v->running = 1;
    prun(pbody_start, pbody_end, 0, &r, &done);
    v->running = 0;
    if (!done)
        pp_error("%s: no RETURN", v->name);
    nvars = save;
    lframe = savef;
    pbody_start = sbs;
    pbody_end = sbe;
    curline = saveline;
    return retchar ? charv(to_char(r)) : fixv(to_fix(r));
}

static int text_invoke(struct ppvar *v, int p);
static int text_builtin(int k, int p);

/* The program's text from p to end (an argument), with its
 * replacements and procedure references, to the output. */
static void scan_range(int p, int end)
{
    while (p < end) {
        unsigned char c = (unsigned char)L->s[p];
        if (c == '\'') {
            int q = past_string(p);
            while (p < q)
                out_ch(L->s[p++]);
        } else if (isdigit(c)) {
            while (p < end && isidch((unsigned char)L->s[p]))
                out_ch(L->s[p++]);
        } else if (isidst(c)) {
            char w[32];
            int q = ident(p, w, sizeof w);
            struct ppvar *v = var(w);
            if (v && v->active && v->isentry) {
                p = text_invoke(v, q);
            } else if (!v && bi_of(w) >= 0 && bi_active[bi_of(w)]) {
                p = text_builtin(bi_of(w), q);
            } else if (v && v->active && v->hasval) {
                replace(v, 0);
                p = q;
            } else {
                while (p < q)
                    out_ch(L->s[p++]);
            }
        } else {
            out_ch(L->s[p++]);
        }
    }
}

/* A reference in the program's text: name(arguments), the arguments the
 * text up to a comma or parenthesis outside nested ones, each scanned
 * for replacement, then converted (chapter 16).  Returns after it. */
static int text_args(int p, const struct ppvar *v, struct val *args, int *nap);

static int text_invoke(struct ppvar *v, int p)
{
    struct val args[16];
    int na = 0;
    p = text_args(p, v, args, &na);
    {
        struct val r = invoke(v, args, na);
        out_ch(' ');
        emit_text(to_char(r), 1);
        out_ch(' ');
    }
    return p;
}

/* An active built-in function in the program's text: its arguments as
 * a procedure's are taken, its value to the output. */
static int text_builtin(int k, int p)
{
    struct val args[16];
    int na = 0;
    if (k != 2 && k != 3)
        p = text_args(p, NULL, args, &na);
    {
        struct val r = k == 5 ? fixv(0) : bi_eval(k, args, na, na ? to_char(args[0]) : "");
        if (k == 5) {
            /* SUBSTR(x, y [, z]) */
            char *t;
            long ii, jj, len;
            if (na < 2 || na > 3)
                pp_error("%s", "SUBSTR: the wrong number of arguments");
            t = to_char(args[0]);
            len = (long)strlen(t);
            ii = to_fix(args[1]);
            jj = na == 3 ? to_fix(args[2]) : len - ii + 1;
            if (ii < 1 || jj < 0 || ii - 1 + jj > len)
                pp_error("%s", "SUBSTR out of the string");
            r = charv(xsdup(t + ii - 1, (int)jj));
        }
        out_ch(' ');
        emit_text(to_char(r), 1);
        out_ch(' ');
    }
    return p;
}

/* The arguments of a reference in the program's text: name(arguments),
 * the arguments the text up to a comma or parenthesis outside nested
 * ones, each scanned for replacement, then converted (chapter 16).
 * Returns after them. */
static int text_args(int p, const struct ppvar *v, struct val *args, int *nap)
{
    int na = 0, q = skipb(p);
    if (L->s[q] == '(') {
        q++;
        for (;;) {
            int a = q, depth = 0;
            char *sv_os = os;
            int *sv_lo = olo, sv_on = on, sv_cap = ocap;
            while (q < L->n && (depth > 0 || (L->s[q] != ',' && L->s[q] != ')'))) {
                if (L->s[q] == '\'') {
                    q = past_string(q);
                    continue;
                }
                if (L->s[q] == '(')
                    depth++;
                else if (L->s[q] == ')')
                    depth--;
                q++;
            }
            if (na == 16)
                pp_error("%s: too many arguments", v ? v->name : "a built-in function");
            /* the argument's text with its replacements made */
            os = NULL;
            olo = NULL;
            on = ocap = 0;
            {
                int sp = 0;
                scan_range(a, q);
                out_ch('\0');
                while (os[sp] == ' ')
                    sp++;
                args[na] = charv(xstrdup(os + sp));
                if (!os[sp])
                    args[na].omitted = 1;     /* the null string: not set */
                free(os);
                free(olo);
            }
            os = sv_os;
            olo = sv_lo;
            on = sv_on;
            ocap = sv_cap;
            if (v && na < v->nparm && !v->pchar[na] && !args[na].omitted)
                args[na] = fixv(to_fix(args[na]));
            na++;
            if (L->s[q] == ',') { q++; continue; }
            q++;
            break;
        }
        p = q;
    }
    *nap = na;
    return p;
}

/* Scan from p: to the end, or (until_end) past the %END of the group. */
static int run(int p, int until_end)
{
    while (p < L->n) {
        unsigned char c = (unsigned char)L->s[p];
        curline = L->lineof[p];
        if (c == '\'') {
            int q = past_string(p);
            while (p < q)
                out_ch(L->s[p++]);
        } else if (c == '/' && p + 1 < L->n && L->s[p + 1] == '*') {
            int q = skipb(p);
            while (p < q && p < L->n)
                out_ch(L->s[p++]);
        } else if (c == '%') {
            int q = skipb(p + 1);
            char w[32];
            while (isidst((unsigned char)L->s[q])) {
                int r = skipb(ident(q, w, sizeof w));
                if (L->s[r] != ':')
                    break;
                q = skipb(r + 1);
            }
            if (is_word(q, "END")) {
                if (!until_end)
                    pp_error("%s", "%END without %DO");
                out_ch(' ');
                return past_semi(q);
            }
            p = exec(p);
            out_ch(' ');
            if (jumped) {
                if (until_end)
                    return p;               /* out of the group */
                jumped = 0;
            }
        } else if (isdigit(c)) {
            while (p < L->n && isidch((unsigned char)L->s[p]))
                out_ch(L->s[p++]);
        } else if (isidst(c)) {
            char w[32];
            int q = ident(p, w, sizeof w);
            struct ppvar *v = var(w);
            if (v && v->active && v->isentry) {
                p = text_invoke(v, q);
            } else if (!v && bi_of(w) >= 0 && bi_active[bi_of(w)]) {
                p = text_builtin(bi_of(w), q);
            } else if (v && v->active && v->hasval) {
                replace(v, 0);
                p = q;
            } else {
                while (p < q)
                    out_ch(L->s[p++]);
            }
        } else {
            out_ch(L->s[p++]);
        }
    }
    if (until_end)
        pp_error("%s", "%DO without %END");
    return p;
}

void preprocess(struct lexer *lx)
{
    L = lx;
    os = NULL;
    olo = NULL;
    on = ocap = 0;
    nvars = 0;
    jumped = 0;
    curline = 1;
    run(0, 0);
    out_ch('\0');
    on--;
    free(lx->s);
    free(lx->lineof);
    lx->s = os;
    lx->lineof = olo;
    lx->n = on;
}
