/* main.c -- PL1C/370 command line.
 *
 *     pl1c370 [-o out.asm] [-I dir]... source.pli
 *
 * -I names a directory of %INCLUDE members, <member>.pli in lower case.
 *
 * Writes S/370 assembler for as370; the default output is the source name
 * with .asm.  Exit 0 on success, 8 when the program has errors.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pl1c.h"

#ifndef PL1C_VERSION
#define PL1C_VERSION "unknown"
#endif

const char *src_name;
int n_errors;

void fatal(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "pl1c370: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(8);
}

void error_at(int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s(%d): ", src_name, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    n_errors++;
}

int opt_lang_f;
int opt_sprog;
int opt_interrupt;

void beyond_f(int line, enum level lv, const char *what)
{
    static const char *const names[] = {
        "PL/I (F)", "the Optimizing Compiler's", "PL/I for MVS & VM's",
        "ISO (ANSI X3.53-1976)"
    };
    if (opt_lang_f && lv != L_F)
        error_at(line, "%s is not PL/I (F) but %s (-lang=F)", what, names[lv]);
}

void *xmalloc(size_t n)
{
    void *p = calloc(1, n ? n : 1);   /* zeroed: new fields start out 0 */
    if (!p)
        fatal("out of memory");
    return p;
}

char *xstrdup(const char *s)
{
    char *p = xmalloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

/* -MD: the output's dependencies in make's syntax -- the source and
 * every %INCLUDE member the preprocessor read -- and an empty rule for
 * each member, so that a member removed does not stop make (gcc -MP).
 * The file is the output's name with .d for .asm, or -MF's. */
static void dep_name(FILE *f, const char *s)
{
    for (; *s; s++) {
        if (*s == ' ' || *s == '#')
            fputc('\\', f);
        if (*s == '$')
            fputc('$', f);
        fputc(*s, f);
    }
}

static void write_deps(const char *depname, const char *outname, const char *in)
{
    char *def = NULL;
    FILE *f;
    if (!depname) {
        const char *dot = strrchr(outname, '.');
        size_t n = dot ? (size_t)(dot - outname) : strlen(outname);
        def = xmalloc(n + 3);
        memcpy(def, outname, n);
        strcpy(def + n, ".d");
        depname = def;
    }
    f = fopen(depname, "w");
    if (!f)
        fatal("cannot write %s", depname);
    dep_name(f, outname);
    fputs(":", f);
    fputs(" ", f);
    dep_name(f, in);
    for (int i = 0; i < n_dep_paths; i++) {
        fputs(" \\\n ", f);
        dep_name(f, dep_paths[i]);
    }
    fputs("\n", f);
    for (int i = 0; i < n_dep_paths; i++) {
        fputs("\n", f);
        dep_name(f, dep_paths[i]);
        fputs(":\n", f);
    }
    if (fclose(f) != 0)
        fatal("error writing %s", depname);
    free(def);
}

int main(int argc, char **argv)
{
    const char *in = NULL, *outname = NULL, *depname = NULL;
    int opt_md = 0;
    char *def = NULL;
    struct lexer lx;
    struct proc *p;
    FILE *f;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("pl1c370 %s (run time ABI %s)\n", PL1C_VERSION, PL1_ABI_STAMP);
            return 0;
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            outname = argv[++i];
        else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
            if (n_include_dirs == 16)
                fatal("too many -I directories");
            include_dirs[n_include_dirs++] = argv[++i];
        }
        else if (strcmp(argv[i], "-plist") == 0)
            opt_plist = 1;            /* PL/I (F)'s ENTRY IHESAPB/IHESAPD */
        else if (strcmp(argv[i], "-MD") == 0)
            opt_md = 1;               /* a make dependency file beside the output */
        else if (strcmp(argv[i], "-MF") == 0 && i + 1 < argc)
            depname = argv[++i];      /* ... under this name */
        else if (strcmp(argv[i], "-lang=F") == 0)
            opt_lang_f = 1;           /* PL/I (F)'s language only */
        else if (strcasecmp(argv[i], "-langlvl=SPROG") == 0)
            opt_sprog = 1;            /* PL/I for MVS & VM's LANGLVL(SPROG) */
        else if (strcasecmp(argv[i], "-langlvl=NOSPROG") == 0)
            opt_sprog = 0;
        else if (strncmp(argv[i], "-stack=", 7) == 0) {
            /* the C stack: bytes, K for kilobytes; 0 leaves LIBC/370's
             * default of 256 KB (#177) */
            char *end;
            long v = strtol(argv[i] + 7, &end, 10);
            if (*end == 'K' || *end == 'k')
                v *= 1024, end++;
            if (*end || v < 0 || (v > 0 && v < 4096) || v > 8 * 1024 * 1024)
                fatal("-stack=N takes 0, or 4096 to 8M bytes (K for kilobytes)");
            opt_stack = (int)v;
        }
        else if (strcmp(argv[i], "-char48") == 0)
            opt_char48 = 1;           /* PL/I (F)'s CHAR48 option */
        else if (strcmp(argv[i], "-interrupt") == 0)
            opt_interrupt = 1;        /* the Optimizer's INTERRUPT option */
        else if (argv[i][0] == '-')
            fatal("unknown option %s (usage: pl1c370 [--version] [-lang=F] [-langlvl=SPROG] [-plist] [-char48] "
                  "[-interrupt] [-stack=N] [-I dir] [-o out.asm] source.pli)", argv[i]);
        else if (!in)
            in = argv[i];
        else
            fatal("more than one source file");
    }
    if (!in)
        fatal("usage: pl1c370 [-lang=F] [-langlvl=SPROG] [-plist] [-char48] [-I dir] [-MD [-MF f]] "
              "[-o out.asm] source.pli");
    src_name = in;
    if (opt_interrupt && opt_lang_f)
        fatal("-interrupt is the Optimizing Compiler's option, not PL/I (F)'s (-lang=F)");
    if (!outname) {
        const char *dot = strrchr(in, '.');
        size_t n = dot ? (size_t)(dot - in) : strlen(in);
        def = xmalloc(n + 5);
        memcpy(def, in, n);
        strcpy(def + n, ".asm");
        outname = def;
    }
    lex_file(&lx, in);
    p = parse_program(&lx);
    if (n_errors)
        return 8;
    resolve_program(p);
    if (n_errors)
        return 8;
    lower_program(p);
    check_program(p);
    if (n_errors)
        return 8;
    f = fopen(outname, "w");
    if (!f)
        fatal("cannot write %s", outname);
    gen_program(p, f);
    if (fclose(f) != 0)
        fatal("error writing %s", outname);
    if (opt_md && !n_errors)
        write_deps(depname, outname, in);
    return n_errors ? 8 : 0;
}
