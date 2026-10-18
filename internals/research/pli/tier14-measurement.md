# Tier 14 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. The test programs `tests/pli/t77hyp`
to `t80hyer` print what PL/I (F) prints and end with the same condition
code, together with Tiers 0 to 13: 82 programs (JOB00257, JOB00258).
Probe (JOB00253, run as the first form of
t80hyer).

## The routines

SINH, COSH, TANH, ATANH, ERF and ERFC, short and long, are the run
time's routines in `rt/pl1m*.c`, as are the functions of Tier 13.

### Method (rt/pl1m*.c, 2026-10-18)

Each function is reduced to a small range by textbook identities
(C28-6590-0 names the same kinds of method for PL/I (F)): SQRT by
Newton's iteration; EXP by n*LN2 + r; LOG by 2**k * m and the series of
2*ATANH((m-1)/(m+1)); SIN, COS and TAN by multiples of pi/2 (of 90 for
the degree forms, exactly); ATAN by 1/x above 1 and a turn by pi/6
above TAN(pi/12); SINH, COSH and TANH through EXP and EXP(x)-1; ATANH
through LOG; ERF by its Taylor series below 1, ERFC by Laplace's
continued fraction above. The leading terms are computed in an extended
format with a 64-bit fraction, the small tails in long floating point,
and the result is rounded once. The errors, messages and ONCODEs are
PL/I (F)'s; EXP beyond the largest number raises OVERFLOW by an exponent
overflow.

### Accuracy

Measured on MVSTK5-BLD against values computed to 80 digits, 1,500
random arguments per function and form over the usual ranges, in units
of the last hexadecimal digit (mean / maximum):

| | short, PL/I (F) | short, PL1C/370 | long, PL/I (F) | long, PL1C/370 |
|---|---|---|---|---|
| SQRT | 0.25 / 0.58 | 0.25 / 0.50 | 0.25 / 0.50 | 0.25 / 0.50 |
| EXP | 0.25 / 0.60 | 0.25 / 0.50 | 0.41 / 1.30 | 0.26 / 0.55 |
| LOG | 0.31 / 0.74 | 0.26 / 0.50 | 0.69 / 1.53 | 0.25 / 0.50 |
| LOG2 | 3.16 / 4.97 | 0.25 / 0.50 | 1.42 / 3.12 | 0.25 / 0.50 |
| LOG10 | 0.36 / 5.04 | 0.25 / 0.50 | 0.34 / 6.58 | 0.25 / 0.50 |
| SIN | 0.77 / 12.9 | 0.26 / 0.50 | 8.06 / 2350 | 0.25 / 0.51 |
| COS | 0.75 / 12.9 | 0.25 / 0.50 | 12.4 / 3086 | 0.25 / 0.51 |
| SIND | 0.80 / 62.5 | 0.25 / 0.50 | 1.09 / 3.99 | 0.25 / 0.51 |
| TAN | 0.82 / 8.07 | 0.26 / 0.50 | 4225 / 6.3E6 | 0.25 / 0.51 |
| TAND | 0.80 / 8.70 | 0.26 / 0.50 | 0.91 / 5.93 | 0.25 / 0.55 |
| ATAN | 0.80 / 4.12 | 0.24 / 0.50 | 0.52 / 2.25 | 0.25 / 0.50 |
| ATAND | 3.45 / 5.82 | 0.25 / 0.50 | 2.93 / 8.23 | 0.25 / 0.50 |
| ATAN(y,x) | 1.02 / 11.5 | 0.25 / 0.50 | 0.86 / 7.27 | 0.24 / 0.50 |
| SINH | 0.71 / 4.14 | 0.25 / 0.50 | 1.53 / 13.0 | 0.25 / 0.59 |
| COSH | 0.74 / 4.27 | 0.25 / 0.50 | 1.58 / 15.1 | 0.25 / 0.58 |
| TANH | 0.40 / 2.24 | 0.21 / 0.50 | 0.41 / 1.46 | 0.21 / 0.64 |
| ATANH | 1.84 / 15.0 | 0.25 / 0.50 | 1.68 / 10.9 | 0.25 / 0.52 |
| ERF | 0.40 / 2.11 | 0.21 / 0.50 | 0.43 / 1.39 | 0.22 / 0.60 |
| ERFC | 17.9 / 233 | 0.25 / 0.50 | 21.4 / 225 | 0.25 / 1.36 |
| x ** y | 42.4 / 342 | 0.25 / 0.50 | 54.4 / 573 | 0.28 / 2.25 |

(SIN and COS long on |x| <= 10; TAN long likewise, where PL/I (F) loses
digits near the poles.)

## Types

FLOAT of the argument's base and precision, as for the other
mathematical functions.

## Errors

| Code | Message | ONCODE |
|------|---------|--------|
| 0107 / 010F | IHE208I IHEHTS / IHEHTL ABS(X) GT OR = 1 IN ATANH(X) | 1514 / 1515 |

Each raises ERROR. SINH and COSH beyond the range of EXP raise OVERFLOW
in EXP: without an on-unit, `IHE300I OVERFLOW`, then ERROR.

## Not done

After a normal return of an ON OVERFLOW unit from SINH or COSH, PL/I
(F) goes on with the value EXP left undefined, and the routine's next
instructions trap: short SINH(200) raises ZERODIVIDE (IHE330I, then
ERROR), long COSH(200) a second OVERFLOW (JOB00253). Those interrupts
come from the hardware, and PL1C/370 has no program-check exit: it
abends 0CC there.
