/* pl1mint.c -- integral parts for the argument reductions of the
 * mathematical functions (rt/pl1mth.h). */
#include "pl1mth.h"

/* The integer part of x, exactly. */
double trunc_d(double x)
{
    dbl u;
    int e = hexp(x), bits;
    u.d = x;
    if (e <= 0)
        return 0.0;
    if (e >= 14)
        return x;
    bits = 56 - 4 * e;               /* fraction bits right of the point */
    if (bits >= 32) {
        u.w[1] = 0;
        u.w[0] &= ~((1u << (bits - 32)) - 1);
    } else
        u.w[1] &= ~((1u << bits) - 1);
    return u.d;
}

/* The nearest integer to x. */
double nint_d(double x)
{
    return x < 0 ? -trunc_d(0.5 - x) : trunc_d(x + 0.5);
}

/* n mod 4 for an integral n. */
int mod4(double n)
{
    double r = n - 4.0 * trunc_d(n * 0.25);
    int q = r < 0 ? -(int)-r : (int)r;
    return q & 3;
}
