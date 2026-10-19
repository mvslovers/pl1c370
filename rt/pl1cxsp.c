/* pl1cxsp.c -- a complex value as characters, read (#2 C4b): GET of
 * complex targets and of a complex constant into a real one, and the
 * conversion of a string to a complex value.
 *
 * The form is [+|-]real{+|-}imaginaryI, or a real or an imaginary
 * constant alone, blanks around it, none inside: "A blank must not
 * precede the central + or -" (GC28-8201-4, "Complex Data"); a null
 * string is 0.  What PL/I (F) refuses, and the character it names
 * (ONCHAR), measured: 1 +2I the blank, 1+2 the central sign, +I the
 * sign, 4+ZZI the first Z (Q12CCV2, JOB00248). */
#include <stddef.h>
#include "pl1rt.h"
#include "pl1int.h"

int pl1i_cpx_split(const char *t, int n, int *rs, int *rn, int *is, int *in) asm("PL1CXSP");

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/* t[a..b) a real constant [sign]digits[.digits][E[sign]digits]: -1, or
 * the index of the first character that is wrong */
static int number(const char *t, int a, int b)
{
    int i = a, nd = 0;
    if (i < b && (t[i] == '+' || t[i] == '-'))
        i++;
    while (i < b && is_digit(t[i]))
        i++, nd++;
    if (i < b && t[i] == '.')
        for (i++; i < b && is_digit(t[i]); i++)
            nd++;
    if (!nd)
        return i < b ? i : a;
    if (i < b && t[i] == 'E') {
        int ed = 0;
        i++;
        if (i < b && (t[i] == '+' || t[i] == '-'))
            i++;
        while (i < b && is_digit(t[i]))
            i++, ed++;
        if (!ed)
            return i < b ? i : i - 1;
    }
    return i < b ? i : -1;
}

/* The real and imaginary parts of t[0..n) (a length 0: the part is 0):
 * -1, or the index of the character that is wrong. */
int pl1i_cpx_split(const char *t, int n, int *rs, int *rn, int *is, int *in)
{
    int a = 0, b = n, c = -1, e;
    while (a < b && t[a] == ' ')
        a++;
    while (b > a && t[b - 1] == ' ')
        b--;
    *rs = *is = a;
    *rn = *in = 0;
    for (int i = a; i < b; i++)
        if (t[i] == ' ')
            return i;
    if (a == b)
        return -1;
    /* the central sign: a sign after the first character, not an
     * exponent's */
    for (int i = (t[b - 1] == 'I' ? b - 2 : b - 1); i > a; i--)
        if ((t[i] == '+' || t[i] == '-') && t[i - 1] != 'E') {
            c = i;
            break;
        }
    if (t[b - 1] != 'I') {
        if (c >= 0)
            return c;
        *rn = b - a;
        return number(t, a, b);
    }
    if (c < 0) {
        *in = b - 1 - a;
        return number(t, a, b - 1);
    }
    *rn = c - a;
    *is = c;
    *in = b - 1 - c;
    e = number(t, a, c);
    return e >= 0 ? e : number(t, c, b - 1);
}

/* A program with COMPLEX data (gen.c, the main procedure's prologue, and
 * each GET of a complex target): GET and the conversion of a string to a
 * real value take a complex constant, its real part (Q11CGE2, Q12CCV2) */
void pl1_cx_on(void) asm("PL1CXON");
void pl1_cx_on(void)
{
    pl1x_cxsplit = pl1i_cpx_split;
}
