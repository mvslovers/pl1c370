/* pl1cxu.c -- UNDERFLOW for the COMPLEX FLOAT routines (rt/pl1cx.h), a
 * member of its own: each of them links it, none links another for it. */
#include "pl1cx.h"

void cx_ufl(int n, int m)
{
    if (m & CX_UFL)
        while (n-- > 0)
            pl1_signal(C_UNDERFLOW, NULL, 330);
}
