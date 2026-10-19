/* pl1cx.h -- what the members of the COMPLEX FLOAT arithmetic share
 * (rt/pl1cxm.c, rt/pl1cxp.c, rt/pl1cxa.c).  Not part of the ABI: the
 * compiler calls the entries by name (src/gen.c gen_cxop).
 *
 * The routines compute in plain short or long floating point, in the
 * order PL/I (F)'s library does (C28-6590-0, "Complex Operations"),
 * and so give its bits: the products of a short multiplication are
 * long in the register and truncated by the short subtraction, as the
 * library's (measured on every probe value, internals/research/pli/
 * c2-measurement.md).  Conditions come from the machine as they do
 * there: an exponent overflow and a floating-point divide by zero
 * interrupt (rt/pl1pie.asm), once for each instruction that meets one,
 * so a product that overflows raises OVERFLOW four times and a
 * division by 0+0I ZERODIVIDE three times, as in PL/I (F).  Exponent
 * underflow cannot interrupt (the program mask is clear), so each
 * product and quotient is tested as the generated code tests its own
 * (src/gen.c check_ufl), and UNDERFLOW raised as often as there are
 * such, after the routine's last operation: where PL/I (F) raises it
 * in between, before an OVERFLOW or ZERODIVIDE of a later operation,
 * the order of the on-units differs (not seen in a probe). */
#ifndef PL1CX_H
#define PL1CX_H
#include <stddef.h>
#include "pl1rt.h"

/* the last argument: the conditions enabled in the caller */
#define CX_UFL 1               /* UNDERFLOW */

void cxms(const float *a, const float *b, const float *c, const float *d, float *r, int m)
    asm("PL1CXMS");
void cxml(const double *a, const double *b, const double *c, const double *d, double *r,
          int m) asm("PL1CXML");
void cxds(const float *a, const float *b, const float *c, const float *d, float *r, int m)
    asm("PL1CXDS");
void cxdl(const double *a, const double *b, const double *c, const double *d, double *r,
          int m) asm("PL1CXDL");
void cxps(const float *a, const float *b, int n, float *r, int m) asm("PL1CXPS");
void cxpl(const double *a, const double *b, int n, double *r, int m) asm("PL1CXPL");
void cxas(const float *a, const float *b, float *r, int m) asm("PL1CXAS");
void cxal(const double *a, const double *b, double *r, int m) asm("PL1CXAL");

/* UNDERFLOW n times, if it is enabled: once for each product or
 * quotient of the routine that is 0 of nonzero operands, raised when
 * the routine has its result (rt/pl1cxu.c) */
void cx_ufl(int n, int m) asm("PL1CXUF");

#endif
