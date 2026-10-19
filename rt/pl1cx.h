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

/* ---- the mathematical functions of complex values (C3) ----
 *
 * The methods are C28-6590-0's for PL/I (F) ("Functions with Complex
 * Arguments"), computed in long precision through the run time's own
 * real functions (rt/pl1m*.c), the short entries rounding the long
 * result: as accurate as PL/I (F)'s or more, the last digit may
 * differ, as for the real functions.  An argument that the real
 * routine PL/I (F) calls would refuse is refused with that routine's
 * short or long message and ONCODE (cx_sinchk), as PL/I (F) does
 * (JOB00148, JOB00150). */
void snls(const double *x, double *r) asm("PL1SNLS");
void snlc(const double *x, double *r) asm("PL1SNLC");
void exl(const double *x, double *r) asm("PL1EXL0");
void lnle(const double *x, double *r) asm("PL1LNLE");
void shls(const double *x, double *r) asm("PL1SHLS");
void shlc(const double *x, double *r) asm("PL1SHLC");
void thl(const double *x, double *r) asm("PL1THL0");
void tnlr(const double *x, double *r) asm("PL1TNLR");
void htl(const double *x, double *r) asm("PL1HTL0");
void atl2(const double *y, const double *x, double *r) asm("PL1ATL2");
void sql(const double *x, double *r) asm("PL1SQL0");

static inline double cx_sin(double x) { double r; snls(&x, &r); return r; }
static inline double cx_cos(double x) { double r; snlc(&x, &r); return r; }

/* ABS(x) >= 2**18*pi (short) or 2**50*pi (long): the error of the real
 * SIN routine (code 0x0103 or 0x010B), or of TAN (0x0104, 0x010C) */
static inline void cx_sinchk(double x, int lo, int code)
{
    /* the limits of rt/pl1msin.c: 823549.66... and 3537118876014220 */
    static const union { unsigned w[2]; double d; } lim[2] = {
        {{0x45C90FDA, 0xA22168C2}}, {{0x4DC90FDA, 0xA22168C2}}
    };
    if ((x < 0 ? -x : x) >= lim[lo].d)
        pl1_math_error(lo ? code + 8 : code);
}

/* EXP(x+yI) into r, of rt/pl1cxe.c, for ** too */
void cx_exp(double x, double y, double *r, int lo) asm("PL1CXEX");
/* LOG(x+yI) into r, of rt/pl1cxl.c */
void cx_log(double x, double y, double *r, int lo) asm("PL1CXLG");

/* the short and the long entry of a function f(x, y, r, lo) */
#define CX_ENTRIES(fs, fl, sn, ln, f)                                          \
    void fs(const float *a, const float *b, float *r, int m) asm(sn);          \
    void fl(const double *a, const double *b, double *r, int m) asm(ln);       \
    void fs(const float *a, const float *b, float *r, int m)                   \
    {                                                                          \
        double o[2];                                                           \
        (void)m;                                                               \
        f(*a, *b, o, 0);                                                       \
        r[0] = (float)o[0];                                                    \
        r[1] = (float)o[1];                                                    \
    }                                                                          \
    void fl(const double *a, const double *b, double *r, int m)                \
    {                                                                          \
        (void)m;                                                               \
        f(*a, *b, r, 1);                                                       \
    }

#endif
