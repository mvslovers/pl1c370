/* lex.c -- the PL/I source stream and its tokens.
 *
 * PL/I (F) reads columns SORMGIN=(2,72) of every record as one continuous
 * stream: a record shorter than 72 counts as padded with blanks, and a
 * string or identifier may run on into the next record.  Keywords are not
 * reserved, so every word is an identifier here; the parser decides.
 * The NOT sign is accepted as '^' or as UTF-8 U+00AC.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pl1c.h"

#define FIRSTCOL 2
#define LASTCOL  72

static void push(struct lexer *lx, struct token t)
{
    if (lx->ntok == lx->cap) {
        lx->cap = lx->cap ? lx->cap * 2 : 256;
        lx->toks = realloc(lx->toks, (size_t)lx->cap * sizeof *lx->toks);
        if (!lx->toks)
            fatal("out of memory");
    }
    lx->toks[lx->ntok++] = t;
}

/* The directories %INCLUDE members are read from (-I). */
const char *include_dirs[16];
const char *dep_paths[256];
int n_dep_paths;
int n_include_dirs;

int opt_char48;
int opt_plist;
int opt_stack = -1;

static int isidch(int c);

static int hexval(int c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 :
           c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

/* The 48-character set (CHAR48; GC28-8201-4, Part II, Section B) put
 * into the 60-character set before anything else reads the stream, as
 * PL/I (F)'s 48-character-set processor does: outside strings and
 * comments '..' is ':', ',.' is ';' unless a digit follows, '//' is
 * '%', and the words GT GE LT LE NE NL NG CAT OR AND NOT PT are their
 * operators.  Characters of the 60-character set stay as they are. */
static void char48(struct lexer *lx)
{
    static const struct { const char *w, *op; } words[] = {
        {"GT", ">"}, {"GE", ">="}, {"LT", "<"}, {"LE", "<="}, {"NE", "^="}, {"NL", "^<"},
        {"NG", "^>"}, {"CAT", "||"}, {"OR", "|"}, {"AND", "&"}, {"NOT", "^"}, {"PT", "->"}};
    char *o = xmalloc((size_t)lx->n * 2 + 2);
    int *ol = xmalloc(((size_t)lx->n * 2 + 2) * sizeof(int)), n = 0;
    const char *s = lx->s;
    for (int i = 0; i < lx->n;) {
        int line = lx->lineof[i];
        if (s[i] == '\'') {
            /* a string, its '' included, as it stands */
            do {
                o[n] = s[i]; ol[n++] = lx->lineof[i]; i++;
                if (i < lx->n && s[i] == '\'' && i + 1 < lx->n && s[i + 1] == '\'') {
                    o[n] = s[i]; ol[n++] = lx->lineof[i]; i++;
                    o[n] = s[i]; ol[n++] = lx->lineof[i]; i++;
                    continue;
                }
            } while (i < lx->n && s[i] != '\'');
            if (i < lx->n) { o[n] = s[i]; ol[n++] = lx->lineof[i]; i++; }
            continue;
        }
        if (s[i] == '/' && i + 1 < lx->n && s[i + 1] == '*') {
            /* a comment, as it stands */
            o[n] = s[i]; ol[n++] = line; i++;
            o[n] = s[i]; ol[n++] = line; i++;
            while (i < lx->n && !(s[i] == '*' && i + 1 < lx->n && s[i + 1] == '/')) {
                o[n] = s[i]; ol[n++] = lx->lineof[i]; i++;
            }
            for (int k = 0; k < 2 && i < lx->n; k++) { o[n] = s[i]; ol[n++] = lx->lineof[i]; i++; }
            continue;
        }
        if (s[i] == '.' && i + 1 < lx->n && s[i + 1] == '.') {
            o[n] = ':'; ol[n++] = line; i += 2;
            continue;
        }
        if (s[i] == ',' && i + 1 < lx->n && s[i + 1] == '.' &&
            !(i + 2 < lx->n && isdigit((unsigned char)s[i + 2]))) {
            o[n] = ';'; ol[n++] = line; i += 2;
            continue;
        }
        if (s[i] == '/' && i + 1 < lx->n && s[i + 1] == '/') {
            o[n] = '%'; ol[n++] = line; i += 2;
            continue;
        }
        if (isalpha((unsigned char)s[i]) || s[i] == '$' || s[i] == '#' || s[i] == '@') {
            int j = i, hit = -1;
            while (j < lx->n && isidch((unsigned char)s[j]))
                j++;
            for (int w = 0; w < (int)(sizeof words / sizeof *words); w++)
                if ((int)strlen(words[w].w) == j - i &&
                    strncasecmp(s + i, words[w].w, (size_t)(j - i)) == 0)
                    hit = w;
            if (hit >= 0 && !(i > 0 && isdigit((unsigned char)s[i - 1]))) {
                for (const char *p = words[hit].op; *p; p++) { o[n] = *p; ol[n++] = line; }
            } else {
                for (; i < j; i++) { o[n] = s[i]; ol[n++] = lx->lineof[i]; }
            }
            i = j;
            continue;
        }
        o[n] = s[i]; ol[n++] = line; i++;
    }
    o[n] = '\0';
    free(lx->s);
    free(lx->lineof);
    lx->s = o;
    lx->lineof = ol;
    lx->n = n;
}

static void read_stream(struct lexer *lx, const char *path)
{
    FILE *f = fopen(path, "r");
    char  rec[512];
    int   cap = 4096, line = 0;

    if (!f)
        fatal("cannot open %s", path);
    lx->s = xmalloc((size_t)cap);
    lx->lineof = xmalloc((size_t)cap * sizeof(int));
    lx->n = 0;
    while (fgets(rec, sizeof rec, f)) {
        int len = (int)strcspn(rec, "\r\n"), c;
        char col[LASTCOL - FIRSTCOL + 1];

        line++;
        /* UTF-8 NOT sign (C2 AC) becomes '^'. */
        for (c = 0; c + 1 < len; c++)
            if ((unsigned char)rec[c] == 0xC2 && (unsigned char)rec[c + 1] == 0xAC) {
                rec[c] = '^';
                memmove(rec + c + 1, rec + c + 2, (size_t)(len - c - 1));
                len--;
            }
        for (c = 0; c < LASTCOL - FIRSTCOL + 1; c++) {
            int i = FIRSTCOL - 1 + c;
            char ch = i < len ? rec[i] : ' ';
            col[c] = ch == '\t' ? ' ' : ch;
        }
        if (lx->n + (int)sizeof col + 1 > cap) {
            cap *= 2;
            lx->s = realloc(lx->s, (size_t)cap);
            lx->lineof = realloc(lx->lineof, (size_t)cap * sizeof(int));
            if (!lx->s || !lx->lineof)
                fatal("out of memory");
        }
        for (c = 0; c < (int)sizeof col; c++) {
            lx->s[lx->n] = col[c];
            lx->lineof[lx->n++] = line;
        }
    }
    fclose(f);
    lx->s[lx->n] = '\0';
    if (opt_char48)
        char48(lx);
}

static int isidch(int c)
{
    return isalnum(c) || c == '_' || c == '$' || c == '#' || c == '@';
}

/* %INCLUDE [ddname](member), ... ;  at stream position i: each member's
 * records (columns 2..72, as the source's) are put into the stream after
 * the statement, which goes.  A member is <dir>/<member>.pli, the name in
 * lower case, from the -I directories; the ddname is not looked at (PL/I
 * (F) reads SYSLIB or the ddname given). Returns where lexing goes on. */
int pp_include(struct lexer *lx, int i, int line)
{
    char names[16][9];
    int nn = 0, j = i + 1, end;
    while (j < lx->n && lx->s[j] == ' ')
        j++;
    j += 7;                                  /* INCLUDE */
    for (;;) {
        char w[9];
        int k = 0;
        while (j < lx->n && lx->s[j] == ' ')
            j++;
        while (j < lx->n && isidch((unsigned char)lx->s[j])) {
            if (k < 8)
                w[k++] = (char)tolower((unsigned char)lx->s[j]);
            j++;
        }
        w[k] = '\0';
        while (j < lx->n && lx->s[j] == ' ')
            j++;
        if (j < lx->n && lx->s[j] == '(') {
            /* ddname(member): the member */
            k = 0;
            for (j++; j < lx->n && lx->s[j] == ' '; j++)
                ;
            while (j < lx->n && isidch((unsigned char)lx->s[j])) {
                if (k < 8)
                    w[k++] = (char)tolower((unsigned char)lx->s[j]);
                j++;
            }
            w[k] = '\0';
            while (j < lx->n && lx->s[j] == ' ')
                j++;
            if (j >= lx->n || lx->s[j] != ')')
                fatal("%s(%d): %%INCLUDE ddname(member) expected", src_name, line);
            j++;
            while (j < lx->n && lx->s[j] == ' ')
                j++;
        }
        if (!w[0] || nn == 16)
            fatal("%s(%d): %%INCLUDE needs member names", src_name, line);
        strcpy(names[nn++], w);
        if (j < lx->n && lx->s[j] == ',') {
            j++;
            continue;
        }
        break;
    }
    if (j >= lx->n || lx->s[j] != ';')
        fatal("%s(%d): %%INCLUDE ends with ';'", src_name, line);
    end = j + 1;
    /* the members' streams, in order, in place of the statement */
    for (int m = nn - 1; m >= 0; m--) {
        struct lexer inc;
        char path[1024];
        int found = 0;
        for (int d = 0; d < n_include_dirs && !found; d++) {
            FILE *f;
            snprintf(path, sizeof path, "%s/%s.pli", include_dirs[d], names[m]);
            if ((f = fopen(path, "r")) != NULL) {
                fclose(f);
                found = 1;
            }
        }
        if (!found)
            fatal("%s(%d): %%INCLUDE member %s not found (give its directory with -I)",
                  src_name, line, names[m]);
        {
            /* a dependency of the output, for -MD (once each) */
            int seen = 0;
            for (int k = 0; k < n_dep_paths && !seen; k++)
                seen = strcmp(dep_paths[k], path) == 0;
            if (!seen && n_dep_paths < 256) {
                char *c = xmalloc(strlen(path) + 1);
                strcpy(c, path);
                dep_paths[n_dep_paths++] = c;
            }
        }
        memset(&inc, 0, sizeof inc);
        read_stream(&inc, path);
        {
            int n = lx->n - end + inc.n;
            char *s = xmalloc((size_t)(i + n + 1));
            int *lo = xmalloc((size_t)(i + n + 1) * sizeof(int));
            memcpy(s, lx->s, (size_t)i);
            memcpy(lo, lx->lineof, (size_t)i * sizeof(int));
            memcpy(s + i, inc.s, (size_t)inc.n);
            for (int k = 0; k < inc.n; k++)
                lo[i + k] = line;            /* errors at the %INCLUDE */
            memcpy(s + i + inc.n, lx->s + end, (size_t)(lx->n - end));
            memcpy(lo + i + inc.n, lx->lineof + end, (size_t)(lx->n - end) * sizeof(int));
            s[i + n] = '\0';
            free(lx->s);
            free(lx->lineof);
            free(inc.s);
            free(inc.lineof);
            lx->s = s;
            lx->lineof = lo;
            lx->n = i + n;
            end = i;                         /* the next member goes before it */
        }
    }
    if (nn == 0)
        return end;
    return i;
}
/* A sterling constant at s[i]: digits . digits . digits [. digits] L,
 * the L not followed by a letter or digit.  Made the decimal constant
 * of its value in pence: pounds * 240 + shillings * 12 + pence, the
 * pence's fraction kept (2.4.6L is 534, 0.0.2.5L is 2.5).  The end in
 * sterling_end. */
static int sterling_end;

static int sterling(struct lexer *lx, int i, struct token *t)
{
    const char *s = lx->s;
    int j = i, start[4], len[4] = { 0, 0, 0, 0 }, nf = 3;
    long long v[3];
    for (int f = 0; f < 3; f++) {
        if (f > 0) {
            if (j >= lx->n || s[j] != '.')
                return 0;
            j++;
        }
        start[f] = j;
        while (j < lx->n && isdigit((unsigned char)s[j]))
            j++;
        len[f] = j - start[f];
        if (len[f] == 0)
            return 0;
    }
    if (j < lx->n && s[j] == '.') {
        /* the pence's fraction */
        start[3] = ++j;
        while (j < lx->n && isdigit((unsigned char)s[j]))
            j++;
        len[3] = j - start[3];
        nf = 4;
    }
    if (j >= lx->n || toupper((unsigned char)s[j]) != 'L' ||
        (j + 1 < lx->n && isidch((unsigned char)s[j + 1])))
        return 0;
    if (len[0] > 13)
        error_at(t->line, "the pounds of a sterling constant have 13 digits at most");
    for (int f = 0; f < 3; f++) {
        v[f] = 0;
        for (int k = 0; k < len[f]; k++)
            v[f] = v[f] * 10 + (s[start[f] + k] - '0');
    }
    if (v[1] > 19 || v[2] > 11)
        error_at(t->line, "a sterling constant has 0 to 19 shillings and 0 to 11 pence");
    {
        char b[64];
        int n = snprintf(b, sizeof b, "%lld", v[0] * 240 + v[1] * 12 + v[2]);
        if (nf == 4 && len[3] > 0)
            snprintf(b + n, sizeof b - (size_t)n, ".%.*s", len[3], s + start[3]);
        t->kind = T_NUM;
        t->text = xstrdup(b);
    }
    sterling_end = j + 1;
    return 1;
}


void lex_file(struct lexer *lx, const char *path)
{
    memset(lx, 0, sizeof *lx);
    read_stream(lx, path);
    preprocess(lx);                          /* % statements, src/pp.c */
    for (int i = 0; i < lx->n;) {
        char c = lx->s[i];
        struct token t;

        memset(&t, 0, sizeof t);
        t.line = lx->lineof[i];
        t.pos = i;
        if (c == ' ') {
            i++;
            continue;
        }
        if (c == '%') {
            int j = i + 1;
            while (j < lx->n && lx->s[j] == ' ')
                j++;
            if (lx->n - j >= 7 && strncasecmp(lx->s + j, "INCLUDE", 7) == 0 &&
                !isidch((unsigned char)lx->s[j + 7])) {
                i = pp_include(lx, i, t.line);
                continue;
            }
            fatal("%s(%d): preprocessor statements other than %%INCLUDE are not supported yet",
                  src_name, t.line);
        }
        if (c == '/' && lx->s[i + 1] == '*') {
            int j = i + 2;
            while (j + 1 < lx->n && !(lx->s[j] == '*' && lx->s[j + 1] == '/'))
                j++;
            if (j + 1 >= lx->n)
                fatal("%s(%d): comment not closed", src_name, t.line);
            i = j + 2;
            continue;
        }
        if (isalpha((unsigned char)c) || c == '$' || c == '#' || c == '@') {
            int j = i;
            while (j < lx->n && isidch((unsigned char)lx->s[j]))
                j++;
            t.kind = T_IDENT;
            t.text = xmalloc((size_t)(j - i + 1));
            for (int k = 0; k < j - i; k++)
                t.text[k] = (char)toupper((unsigned char)lx->s[i + k]);
            t.text[j - i] = '\0';
            push(lx, t);
            i = j;
            continue;
        }
        if (isdigit((unsigned char)c) && sterling(lx, i, &t)) {
            /* pounds.shillings.pence L: a FIXED DECIMAL constant of
             * pence (GC28-8201-4, "Sterling Fixed-Point Data") */
            push(lx, t);
            i = sterling_end;
            continue;
        }
        if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)lx->s[i + 1]))) {
            int j = i, point = 0;
            while (j < lx->n && (isdigit((unsigned char)lx->s[j]) || (lx->s[j] == '.' && !point))) {
                if (lx->s[j] == '.')
                    point = 1;
                j++;
            }
            if (j < lx->n && toupper((unsigned char)lx->s[j]) == 'E') {
                /* the exponent of a FLOAT constant */
                int k = j + 1;
                if (k < lx->n && (lx->s[k] == '+' || lx->s[k] == '-'))
                    k++;
                if (k < lx->n && isdigit((unsigned char)lx->s[k])) {
                    while (k < lx->n && isdigit((unsigned char)lx->s[k]))
                        k++;
                    j = k;
                }
            }
            if (j < lx->n && toupper((unsigned char)lx->s[j]) == 'B' &&
                (!(j + 1 < lx->n && isidch((unsigned char)lx->s[j + 1])) ||
                 (toupper((unsigned char)lx->s[j + 1]) == 'I' &&
                  !(j + 2 < lx->n && isidch((unsigned char)lx->s[j + 2]))))) {
                /* a binary constant, 101.1B, 1.1E5B, 11BI (GC28-8201-4,
                 * "Binary Fixed-Point" and "Binary Floating-Point Data");
                 * its digits 0 and 1 (the exponent is decimal) */
                for (int k = i; k < j && toupper((unsigned char)lx->s[k]) != 'E'; k++)
                    if (lx->s[k] != '0' && lx->s[k] != '1' && lx->s[k] != '.')
                        error_at(t.line, "a binary constant has the digits 0 and 1 only");
                j++;
            }
            if (j < lx->n && toupper((unsigned char)lx->s[j]) == 'I' &&
                !(j + 1 < lx->n && (isalnum((unsigned char)lx->s[j + 1]) ||
                                    lx->s[j + 1] == '_' || lx->s[j + 1] == '#' ||
                                    lx->s[j + 1] == '@' || lx->s[j + 1] == '$')))
                j++;                   /* an imaginary constant, 2I (#2) */
            /* any other letter: PL/I (F) assumes a blank before it
               (IEM0009I), as in gauss.pli's 2F(10,3) and iSUB */
            t.kind = T_NUM;
            t.text = xmalloc((size_t)(j - i + 1));
            for (int k = 0; k < j - i; k++)
                t.text[k] = (char)toupper((unsigned char)lx->s[i + k]);
            t.text[j - i] = '\0';
            push(lx, t);
            i = j;
            continue;
        }
        if (c == '\'') {
            int j = i + 1, n = 0;
            char *v = xmalloc((size_t)(lx->n - i + 1));
            for (;;) {
                if (j >= lx->n)
                    fatal("%s(%d): string not closed", src_name, t.line);
                if (lx->s[j] == '\'') {
                    if (lx->s[j + 1] == '\'') {
                        v[n++] = '\'';
                        j += 2;
                        continue;
                    }
                    break;
                }
                v[n++] = lx->s[j++];
            }
            j++;
            if (j < lx->n && (lx->s[j] == 'X' || lx->s[j] == 'x') &&
                !isidch((unsigned char)lx->s[j + 1])) {
                /* 'C1'X: characters in hexadecimal, two digits each, the
                 * EBCDIC code (PL/I for MVS & VM, SC26-3114-01 chapter 2);
                 * kept as the Latin-1 character as370 makes that code of */
                int m = 0;
                beyond_f(t.line, L_MVS, "a hexadecimal character constant (X)");
                if (n % 2)
                    error_at(t.line, "an X constant needs an even number of digits");
                for (int k = 0; k + 1 < n; k += 2) {
                    int hi = hexval(v[k]), lo = hexval(v[k + 1]);
                    if (hi < 0 || lo < 0)
                        error_at(t.line, "%c%c is not a hexadecimal digit pair", v[k], v[k + 1]);
                    v[m++] = (char)ebc_to_lat[((hi < 0 ? 0 : hi) << 4) | (lo < 0 ? 0 : lo)];
                }
                v[m] = '\0';
                t.kind = T_STR;
                t.str = v;
                t.len = m;
                push(lx, t);
                i = j + 1;
                continue;
            }
            if (j + 1 < lx->n && (lx->s[j] == 'B' || lx->s[j] == 'b') &&
                (lx->s[j + 1] == 'X' || lx->s[j + 1] == 'x') &&
                !isidch((unsigned char)lx->s[j + 2])) {
                /* BX, PL/I for MVS & VM's synonym of B4: the B4 path,
                 * which also reports its level */
                lx->s[j + 1] = '4';
            }
            if (j < lx->n && (lx->s[j] == 'B' || lx->s[j] == 'b') &&
                (!isidch((unsigned char)lx->s[j + 1]) ||
                 (lx->s[j + 1] >= '1' && lx->s[j + 1] <= '4' &&
                  !isidch((unsigned char)lx->s[j + 2])))) {
                /* 'bits'B, or ANSI's radix factors B1 to B4: each digit
                 * m bits, 0-1, 0-3, 0-7, 0-F (ANSI CL22, CL23, 4.4.7) */
                int m = lx->s[j + 1] >= '1' && lx->s[j + 1] <= '4' ? lx->s[j + 1] - '0' : 1;
                if (lx->s[j + 1] == '4')       /* PL/I for MVS & VM's B4 */
                    beyond_f(t.line, L_MVS, "a bit string constant B4");
                else if (lx->s[j + 1] >= '1' && lx->s[j + 1] <= '3')
                    beyond_f(t.line, L_ISO, "a bit string constant with a radix factor");
                if (m > 1) {
                    char *bv = xmalloc((size_t)(n * m + 1));
                    for (int k = 0; k < n; k++) {
                        int d = v[k] >= '0' && v[k] <= '9' ? v[k] - '0' :
                                v[k] >= 'A' && v[k] <= 'F' ? v[k] - 'A' + 10 : 99;
                        if (d >= 1 << m)
                            error_at(t.line, "%c is not a digit of a B%d constant", v[k], m);
                        for (int b = 0; b < m; b++)
                            bv[k * m + b] = (d >> (m - 1 - b)) & 1 ? '1' : '0';
                    }
                    v = bv;
                    n *= m;
                    j++;                       /* the digit of B2, B3, B4 */
                } else if (lx->s[j + 1] == '1') {
                    j++;                       /* B1 is B */
                }
                for (int k = 0; k < n; k++)
                    if (v[k] != '0' && v[k] != '1')
                        error_at(t.line, "a bit string constant holds only 0 and 1");
                if (n > MAXBITS)
                    error_at(t.line, "a bit string constant of more than %d bits", MAXBITS);
                j++;
                t.kind = T_BITC;
                v[n] = '\0';
                t.str = v;
                t.len = n;
                push(lx, t);
                i = j;
                continue;
            }
            t.kind = T_STR;
            t.str = v;
            t.len = n;
            push(lx, t);
            i = j;
            continue;
        }
        {
            char d = lx->s[i + 1];
            int w = 1;
            switch (c) {
            case '(': t.kind = T_LP; break;
            case ')': t.kind = T_RP; break;
            case ',': t.kind = T_COMMA; break;
            case ';': t.kind = T_SEMI; break;
            case ':': t.kind = T_COLON; break;
            case '.': t.kind = T_DOT; break;
            case '=': t.kind = T_EQ; break;
            case '+': t.kind = T_PLUS; break;
            case '-':
                if (d == '>') { t.kind = T_ARROW; w = 2; } else t.kind = T_MINUS;
                break;
            case '&': t.kind = T_AND; break;
            case '%': t.kind = T_PCT; break;
            case '*':
                if (d == '*') { t.kind = T_POWER; w = 2; } else t.kind = T_STAR;
                break;
            case '/': t.kind = T_SLASH; break;
            case '|':
                if (d == '|') { t.kind = T_CAT; w = 2; } else t.kind = T_OR;
                break;
            case '<':
                if (d == '=') { t.kind = T_LE; w = 2; } else t.kind = T_LT;
                break;
            case '>':
                if (d == '=') { t.kind = T_GE; w = 2; } else t.kind = T_GT;
                break;
            case '^':
                if (d == '=') { t.kind = T_NE; w = 2; }
                else if (d == '<') { t.kind = T_NLT; w = 2; }
                else if (d == '>') { t.kind = T_NGT; w = 2; }
                else t.kind = T_NOT;
                break;
            default:
                fatal("%s(%d): character '%c' not valid in PL/I source", src_name, t.line, c);
            }
            push(lx, t);
            i += w;
        }
    }
    {
        struct token t;
        memset(&t, 0, sizeof t);
        t.kind = T_EOF;
        t.line = lx->n ? lx->lineof[lx->n - 1] : 0;
        t.pos = lx->n;
        push(lx, t);
    }
}
