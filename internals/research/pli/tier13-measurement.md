# Tier 13 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes (Q2POW
and Q3ERR JOB00224, Q1MATH JOB00225, Q4ERRL JOB00226, Q5ATP JOB00235,
Q6ATB JOB00236, Q7OVF JOB00244). The test programs `tests/pli/t66sqrt`
to `t76ovf` print what PL/I (F) prints and end with the same condition
code, together with Tiers 0 to 12: 78 programs (JOB00250,
JOB00251).

## The functions

SQRT, EXP, LOG, LOG10, LOG2, SIN, COS, TAN, SIND, COSD, TAND, ATAN and
ATAND, short and long, are the run time's routines in `rt/pl1m*.c`
(C since 2026-10-18; see Tier 14's note for the method and the accuracy
measured). They are correctly rounded, or nearly so, where PL/I (F)'s
library is a few units off in the last digit, so a printed last digit can
differ from PL/I (F); the twins print what both agree on.

The call is R1 -> A(argument), A(result). ATAN and ATAND have four
entries each, for one or two arguments, in radians or degrees.

## x ** y

A FLOAT exponent, or a FIXED one with a scale, is the general case:
EXP(y * LOG(x)), with y * LOG(x) kept in extended precision. x = 0 is 0
for y > 0, an error for y <= 0; y = 0 is 1. The result is FLOAT of the
common attributes (Tier 7).

## Types

- One argument: FLOAT of the argument's base and precision (FIXED keeps
  both).
- ATAN(y,x), ATAND(y,x): the common attributes, if both are short or both
  long: `ATAN(S6, 3E0)` has 6 digits, `ATAN(C16, C16)` 16,
  `ATAND(C16, C8)` 16. **A short and a long argument make it FLOAT BINARY
  of 31 to 33 bits**, whatever the bases: `ATAN(C16, 3E0)`,
  `ATAN(C8, S3)`, `ATAN(S6, B53)`, `ATAN(B53, B10)` all print 10 digits,
  and `ATAN(C16, S6) + B25` (FLOAT BIN(25)) keeps 10 -- a FLOAT DEC(10)
  would make it BIN(34), 11 digits (JOB00235, JOB00236). The three widths
  print alike; PL1C/370 takes 33. The value is the long one.

## Errors

The error codes, the messages and the ONCODEs (JOB00224, JOB00226; the
long SIN and TAN messages from the test runs, JOB00230):

| Code | Message | ONCODE |
|------|---------|--------|
| 0100 / 0108 | IHE200I IHESQS / IHESQL: SQRT of a negative value | 1500 / 1501 |
| 0102 / 010A | IHE202I IHELNS / IHELNL: LOG of X LE 0 | 1504 / 1505 |
| 0103 / 010B | IHE212I IHESNS / IHE203I IHESNL: ABS(X) too large in SIN, COS | 1506 / 1507 |
| 0104 / 010C | IHE213I IHETNS / IHE204I IHETNL: ABS(X) too large in TAN | 1508 / 1509 |
| 0105 / 010D | IHE206I IHEATS / IHEATL: ATAN(0,0) | 1510 / 1511 |
| 0112 / 0113 | IHE209I IHEXXS / IHEXXL: x = 0, y <= 0 in x ** y | 1552 / 1553 |

The long SIN and TAN messages are IHE203I and IHE204I, not the short
ones' numbers. Each raises ERROR (step code 2000 without an on-unit).
EXP beyond the range is OVERFLOW, ONCODE 300 (JOB00224): without an
on-unit `IHE300I OVERFLOW`, then ERROR; after a normal return of an
on-unit the program goes on (JOB00244). The routine stores no result
then; what the target holds afterwards is not measured. ON OVERFLOW is accepted now.

A system-action message longer than 120 characters is broken at its last
blank before that, the rest indented by 8 (`... FROM ENTRY` / `POINT
T71MERR`); `compare.py` masks the offset in either form.

## Not done

SINH, COSH, TANH, ATANH, ERF and ERFC: the next tier. COMPLEX arguments stay out of scope.
