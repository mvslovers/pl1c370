/* pl1cxq.c -- SQRT of a COMPLEX FLOAT value (#2 C3, rt/pl1cx.h).
 *
 * C28-6590-0: 0 for 0; otherwise t = SQRT((ABS(x) + ABS(x+yI))/2), and
 * u + vI is t + y/(2t)I for x >= 0, y/(2v) + vI with v = t of y's sign
 * (+ for y = 0) for x < 0. */
#include "pl1cx.h"

static void zsqrt(double x, double y, double *r, int lo)
{
    double ax = x < 0 ? -x : x, ab, t;
    (void)lo;
    if (x == 0.0 && y == 0.0) {
        r[0] = r[1] = 0.0;
        return;
    }
    cxal(&x, &y, &ab, 0);
    t = ax * 0.5 + ab * 0.5;
    sql(&t, &t);
    if (x >= 0.0) {
        r[0] = t;
        r[1] = y / (2.0 * t);
    } else {
        if (y < 0.0)
            t = -t;
        r[1] = t;
        r[0] = y / (2.0 * t);
    }
}

CX_ENTRIES(cxqs, cxql, "PL1CXQS", "PL1CXQL", zsqrt)
