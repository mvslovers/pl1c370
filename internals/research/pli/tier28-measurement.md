# Tier 28 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probe (JOB00405), which is also the test `tests/pli/t111ppro` (JOB00407);
`t112ppr2` (JOB00411). With Tiers 0 to 27: 114 programs identical
(JOB00413 to JOB00415).

## Preprocessor procedures

- `%DECLARE VALUE ENTRY(CHARACTER, FIXED) RETURNS(CHARACTER);` makes
  VALUE a preprocessor function; `%VALUE: PROCEDURE(ARG1, ARG2)
  RETURNS(CHARACTER); ... %END;` may stand anywhere in the source and is
  no part of the preprocessed text. Its statements have no %:
  DECLARE, assignment, IF/THEN/ELSE, DO groups and iterative DO, GO TO
  and RETURN(expression).
- In a preprocessor statement the arguments are expressions:
  `%N = TWICE(21);`. In the program's text they are the text up to a
  comma or parenthesis outside nested ones, scanned for replacement and
  procedure references first, then converted to the declared types:
  `Q = 6 + VALUE(A, 3);` with `%A = 'Z'` is `Q = 6 + Z(3)` (10),
  `MAX2(7, TWICE(5))` is 10. The value replaces the reference, a blank
  on each side, and is scanned again.
- Missing arguments are 0 or the null string (`PAD(4)`: 104).
- A procedure may generate statements (`GEN(S, 3)`: `S1= 10;S2= 20;
  S3= 30;`) and use variables of its own.
- An entry name is replaced like a variable while active: an active PAD
  in a DECLARE is invoked there; `%DEACTIVATE PAD` leaves the name to
  the program.
- A procedure may not invoke itself: `IEM4452I RECURSIVE USE OF
  PROCEDURE FACT ... IS DISALLOWED. PROCESSING TERMINATED.` (a terminal
  error, JOB00409); PL1C/370 refuses it.

## Not done

Bit-string constants in preprocessor expressions; a procedure reference
inside a replacement value (only in the scanned text and in
expressions).
