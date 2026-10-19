/* pl1cxm.c -- COMPLEX FLOAT multiplication (#2, rt/pl1cx.h): (a+bI)*(c+dI)
 * is a*c-b*d + (b*c+a*d)I, as C28-6590-0 gives it for PL/I (F). */
#include "pl1cx.h"

/* 1 if the product p of x and y underflowed to 0 */
#define UF(p, x, y) ((p) == 0 && (x) != 0 && (y) != 0)

void cxms(const float *a, const float *b, const float *c, const float *d, float *r, int m)
{
    float ac = *a * *c, bd = *b * *d, bc = *b * *c, ad = *a * *d;
    int n = UF(ac, *a, *c) + UF(bd, *b, *d) + UF(bc, *b, *c) + UF(ad, *a, *d);
    r[0] = ac - bd;
    r[1] = bc + ad;
    if (n)
        cx_ufl(n, m);
}

void cxml(const double *a, const double *b, const double *c, const double *d, double *r, int m)
{
    double ac = *a * *c, bd = *b * *d, bc = *b * *c, ad = *a * *d;
    int n = UF(ac, *a, *c) + UF(bd, *b, *d) + UF(bc, *b, *c) + UF(ad, *a, *d);
    r[0] = ac - bd;
    r[1] = bc + ad;
    if (n)
        cx_ufl(n, m);
}
