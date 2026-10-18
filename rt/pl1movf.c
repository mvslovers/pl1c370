/* pl1movf.c -- OVERFLOW from the mathematical functions (rt/pl1mth.h). */
#include "pl1mth.h"

/* An exponent overflow, which the program interruption exit turns into
 * the OVERFLOW condition. */
void overflow(void)
{
    static const dbl big = {{0x7FFFFFFF, 0xFFFFFFFF}};
    double x = big.d;
    __asm__ volatile("ADR %0,%0" : "+f"(x));
}
