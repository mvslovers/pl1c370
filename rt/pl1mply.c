/* pl1mply.c -- the polynomial tails of the mathematical functions, and
 * an integer in the extended format (rt/pl1mth.h). */
#include "pl1mth.h"

double poly(double z, const dbl *c, int n)
{
    double p = c[0].d;
    for (int i = 1; i < n; i++)
        p = p * z + c[i].d;
    return p;
}

xf xi(int n)
{
    return xnorm(n < 0, 64, (u64)(n < 0 ? -n : n));
}
