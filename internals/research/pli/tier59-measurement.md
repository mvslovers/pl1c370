# Tier 59: the array functions SUM, PROD, POLY, ALL and ANY

Found by comparing the built-in functions of Section G of the Language
Reference with PL1C/370's table. The comparison used GC28-8201-2 (October
1969), whose OCR text can be searched. Its list of the five array
functions was not checked against the page images of GC28-8201-4.

Measured 2026-10-14 on MVSTK5-BLD with the probes
(Q1ARR, Q2BIT, Q3POLY, all in JOB00948). The tests are identical in
JOB00953:

| Test | Covers |
|---|---|
| `t188sum` | Q1ARR without its cross sections; SUM in a labelled statement, an IF unit, an array assignment |
| `t189all` | Q2BIT, with an INITIAL that sets both elements |
| `t190pol` | Q3POLY and PREC |
| `t191ord` | SUM, PROD and POLY of inexact values, short and long |

## The rules

- **SUM and PROD** return FLOAT, of the base and precision the elements
  take as FLOAT.
  - FIXED DEC(5,2) elements give FLOAT DEC(5): SUM is `5.5000E+00`.
  - FIXED BIN(15) elements give FLOAT BIN(15), which prints five digits.
  - FIXED BIN(31) elements give FLOAT BIN(31), which prints ten.
  - CHAR elements are FIXED DECIMAL(15) first: SUM of `'12'`, `'3'` and
    `'45'` is `6.00000000000000E+01`.
  - The argument may be an array expression. `SUM(B * 2)` has the
    precision of `B * 2`, FIXED BIN(20), and prints seven digits.
- **POLY(a, x)** is a(m) + a(m+1)·x(p) + a(m+2)·x(p)·x(p+1) + ...
  - The last element of x stands for the elements beyond it.
  - An element x stands for each of them, so POLY(a, 2) is
    a(m) + 2a(m+1) + 4a(m+2) + ....
  - One element of a gives a(m).
  - The result is FLOAT, of both arguments' FLOAT type: FIXED DEC(5,1)
    with FLOAT DEC(6) is FLOAT DEC(6).
- **ALL and ANY** are the & and | of the elements as bit strings. The
  result is as long as the longest element, the shorter ones extended
  with zeros.
  - VARYING `'1'B`, `'101'B` and `'11111'B` give `'10000'B` and
    `'11111'B`.
  - FIXED BIN(15) elements are 15 bits; CHAR elements are converted to
    bits.
- **Evaluation:** `t191ord` gives the same digits as PL/I (F) for inexact
  short and long values. PL1C/370 adds and multiplies in element order,
  in the precision of the result, and evaluates POLY from a(m) upwards.
- **In an array assignment** the function is evaluated again for each
  element, after the elements assigned before it. `Q = Q / SUM(Q)` with
  Q = 1, 3, 4 gives `1.25000E-01  4.21052E-01  8.79884E-01`, which is
  1/8, then 3/7.125, then 4/4.546.
- **PREC** is PRECISION, and **UNAL** is UNALIGNED.
- **Not an array function:** in Q2BIT, `INIT((2)'1'B)` is a string
  repetition factor. It sets the first element to `'11'B` and leaves
  the second unset, as Tier 53 found.

## In PL1C/370

- **lower.c** turns each array function into a temporary, computed by
  loops over the elements before the statement that uses it. The
  statement's labels move to the first of them. In an array assignment
  the loops go inside the assignment's loop, before each element.
- **The temporary's type** comes from check.c (`elem_type`,
  `reduce_type`), from a copy of one element of the argument.
- **ALL and ANY** set the first element and & or | the others into it.

The first run (JOB00951) ended with S0C4 in `t188sum`, before the second
fix. The temporaries had no storage map, since the layout runs before
lowering, and a long FLOAT sum overwrote its neighbours. The run also
showed the re-evaluation in an array assignment. Both are fixed.

## The suite

All 193 programs are identical: JOB00955 to JOB00958.

## Not done

- **Restrictions in PL1C/370:**
  - An array function in a WHILE or UNTIL condition (a compile-time
    message).
  - SUM or PROD of bit strings.
  - POLY with arrays of more than one dimension, which PL/I (F) refuses
    too (not measured).
- **Cross sections** (`SUM(M(2,*))`): PL1C/370 has none at all.
- **Found missing in the same comparison:**
  - functions COUNT, LINENO, DATE, TIME, ONLOC, ONCOUNT and DATAFIELD;
  - the CHECK condition;
  - the DISPLAY statement;
  - REFER.
- **Out of scope** (ROADMAP, "Deliberately out of scope"): multitasking
  (COMPLETION, PRIORITY, STATUS) and COMPLEX (COMPLEX, CONJG, IMAG, REAL).
