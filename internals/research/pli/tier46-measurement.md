# Tier 46: format widths from expressions, E format for FIXED values

Measured 2026-10-13 on MVSTK5-BLD: a probe (JOB00760), which is also the test `t150fex` (identical, JOB00764). The
test `t151efx` (JOB00766 and, identical, JOB00768) measured the E
format of FIXED values.

## Widths and d from expressions

A format item's width, and the d of F and E, may be an expression:

- `A(N)` and `A(N + 2)`
- `F(N + 4, D)`, `F(8, D + 1)`
- `E(N + 8, D + 1)`
- `X(K)`, `B(N)`, `COLUMN(N * 3)`, `SKIP(K - 1)`
- in GET EDIT, `A(N)` and `F(K)`

They take the values the variables have when the statement runs. In a
loop, `F(K + 1), X(K), F(K + 2)` gives a new layout on every pass.

PL1C/370 handles this in two steps:

- **parse.c** gives each such expression a variable of its own (`#Fn`,
  FIXED BINARY(31)) and assigns it just before the statement. The
  assignments and the statement form a DO group.
- **gen.c** (`format_begin`) stores the values into the halfwords of the
  statement's format table before PL1FBEG. For E with d from an
  expression, it computes the d + 256·s word at run time (s given, or
  d + 1).

The expressions are evaluated once, at the start of the statement. An
iterative data list inside the statement that changes the variables
does not change the widths.

## The E format of FIXED values

PL/I (F) edits a FIXED value in E(w,d[,s]) as it edits a FLOAT one:

- s significant digits, rounded half up;
- s - d of them before the point;
- the exponent after them.

12.345 in E(12,2) is `1.23E+01`, and 123456 (FIXED BIN(31)) in E(12,3)
is `1.235E+05`.

**A carry out of the first digit is lost**, and the exponent is kept:

- -999.96 in E(10,2) is ` -0.00E+02`, not -1.00E+03;
- 987654321 in E(9,0) is `0E+08`.

`rt/pl1ioed.c e_field_dec` does the same. Zero is `0.00E+00`.

## Not done

- Expressions as repetition factors of format items.
- E for character strings.
- B for arithmetic values.
