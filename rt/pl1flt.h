/* pl1flt.h -- exact conversions of System/370 floating point (pl1flt.c),
 * shared by the compiler and the run time. */
/* The external names are eight characters, fixed by the asm labels (the
 * MVS names cc370 would make of these collide). */
#ifndef PL1FLT_H
#define PL1FLT_H

/* The digits d[0..nd-1] times 10**e10 as a short (longf 0, 4 bytes) or
 * long (8 bytes) float, as the library converts (IHEVPA).  Returns 0, or
 * 1 for an exponent overflow; an underflow gives 0. */
int pl1f_from_decimal(const char *d, int nd, int e10, int neg, int longf, unsigned char *out)
    __asm__("PL1FFDEC");

/* A constant of a program, as the compiler converts it: its digits
 * padded to seventeen, then as pl1f_from_decimal.  Returns 2 for more
 * than seventeen digits. */
int pl1f_constant(const char *d, int nd, int e10, int neg, int longf, unsigned char *out)
    __asm__("PL1FCONS");

/* v / 2**q as a float, truncated. */
int pl1f_from_binary(long long v, int q, int longf, unsigned char *out)
    __asm__("PL1FFBIN");

/* The first n significant digits of the float as the library converts it
 * (IHEVFA), truncated (round 0) or rounded half up (round 1): the value
 * is d1.d2... * 10**e10.  Returns 0 for zero (n zeros, e10 0). */
int pl1f_to_digits(const unsigned char *in, int longf, int n, int round, char *d, int *e10,
                   int *neg)
    __asm__("PL1FTDIG");

/* The integer |value| * 10**q, truncated or rounded, as decimal digits
 * (at most maxd, the rightmost ones); returns their number. */
int pl1f_to_fixed(const unsigned char *in, int longf, int q, int round, char *d, int maxd,
                  int *neg)
    __asm__("PL1FTFIX");

/* value * 2**q truncated towards zero, as an integer (the low bits). */
long long pl1f_to_binary(const unsigned char *in, int longf, int q)
    __asm__("PL1FTBIN");

/* x ** n as PL/I (F)'s library computes it (IHEXIS, IHEXIL): squares
 * and products from the high bit of |n|, truncating, 1/x**|n| for a
 * negative n.  Returns 1 for 0 ** n with n <= 0. */
int pl1f_power(const unsigned char *in, int longf, int n, unsigned char *out)
    __asm__("PL1FPOWR");

/* FLOOR (mode 0), CEIL (1) and TRUNC (2) of a float. */
void pl1f_integer(const unsigned char *in, int longf, int mode, unsigned char *out)
    __asm__("PL1FINTG");

#endif
