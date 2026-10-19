# COMPLEX tier C3: the mathematical functions of complex values

Probes Q9CMAT (short), Q9CMAL (long), Q9EREF (compile only) and the
error probes Q9E... and Q9L... (`internals/design/complex.md` section
4, C3), run on PL/I (F) V5.5 in a throwaway TK5 container of this
project's own, JOB00148 and JOB00150 (2026-10-19).

## Values

SQRT, EXP, LOG, SIN, COS, SINH, COSH, TAN, TANH, ATAN and ATANH at
1+1I, -1+1I, 0.5-2I, -3-0.25I, 2, -2, 0.25+0.5I and -0.75I, short and
long; SQRT and LOG at 1E-30+2E-30I and 1E70-3E70I; z1**z2 with a
complex exponent, a real exponent that is not an integer, a real base,
and a FLOAT integral exponent (2.5E0). They became the twins
`t291cmf`, `t292cml` and `t293cxx`.

PL1C/370 computes them by the methods of C28-6590-0 in long precision
on its own real functions. Against PL/I (F) (JOB00153) 19 of 205 short
numbers and 37 of 205 long ones differ, by at most 4 units of the last
digit printed; in the three worst cases PL1C/370's is the correctly
truncated value (Python's `cmath`): TANH(-3-0.25I) imaginary part
-2.36644E-03 (PL/I (F) -2.36647E-03), COS(-3-0.25I) imaginary part
-3.564865218399044E-02 (...048), (0.3+0.1I)**(2-1I) real part
-3.064357724077473E-02 (...471).

Points noted:

- ATANH(2) is 0.549306+1.570796I and ATANH(-2) -0.549306+1.570796I
  (the imaginary part ATAN(0, -3)/2 in both).
- (-1)**(0.5+0I) prints `3.13E-07+1.00E+00I`: the constants make it
  FLOAT DEC(2), so short, whose cosine of pi/2 is 3.13E-07; no twin
  compares a value that small.

## Errors (no on-unit: the message; ERROR's on-unit: ONCODE)

| Expression | Message | ONCODE |
|---|---|---|
| ATAN(1I), ATANH(1) short | `IHE211I IHEATW Z=+I OR -I IN ATAN(Z) OR Z=+1 OR -1 IN ATANH(Z)` | 1558 |
| the same, long | `IHE211I IHEATZ ...` | 1559 |
| LOG(0+0I) short | `IHE206I IHEATS X=Y=0 IN ATAN(Y,X) OR ATAND(Y,X)` | 1510 |
| the same, long | `IHE206I IHEATL ...` | 1511 |
| 0**(0+1I), 0**(-1+0I) short | `IHE210I IHEXXW X=0,Y NOT POSITIVE REAL IN X**Y` | 1556 |
| the same, long | `IHE210I IHEXXZ ...` | 1557 |
| SIN(1E7+0I), EXP(0+1E7I) | `IHE212I IHESNS ABS(X) GE (2**18)*K IN SIN(X) ...` | 1506 |
| TAN(1E7+1I) | `IHE213I IHETNS ABS(X) GE (2**18)*K IN TAN(X) ...` | 1508 |

So LOG of 0 is the error of the real ATAN(Y,X), not of LOG, and the
large arguments are the real routines' errors. 0**(2+0I) and 0**1.5
are 0.

## Refused at compile time

LOG10, LOG2, ATAND, SIND and ERF of a complex value: IEM1056I "INVALID
ARGUMENT IS BEING PASSED TO ENTRY NAME ..."; ATAN(z,z): IEM1062I
(Q9EREF).
