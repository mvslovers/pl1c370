# Tier 27 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probe (JOB00396, compiled with MACRO), which is also the test `tests/pli/t109pp`
(JOB00398); `t110pp2` (JOB00400). With Tiers 0 to 26: 112 programs
identical (JOB00402 to JOB00404).

## The preprocessor

The rules are the Language Reference's (GC28-8201-4, chapter 16), and
the probes run as it describes them:

- `%DECLARE (N, I) FIXED, (A, T) CHARACTER;` makes preprocessor
  variables and activates them; `%N = 3;` gives a value. An active
  variable with a value in the program's text is replaced by it -- not in
  strings or comments (`'A IN A STRING'`, `'/* N */'` stay) -- and the
  replacement is scanned again: `%A = 'B+C';` makes `X = A;` `X = B+C`.
- A replacement is text, not a string constant: `%T = SUBSTR('ABCDEF', 2,
  3);` and `PUT LIST(T)` write the variable BCD (an implicit FLOAT:
  `0.00000E+00`). A CHARACTER variable may hold whole statements
  (`%S = 'PUT SKIP LIST(''GENERATED'', NEG);';`).
- `%IF e %THEN unit [%ELSE unit]`: a unit is a preprocessor statement or
  a `%DO; ... %END;` group (a program statement there is an error).
  `%DO I = 1 TO 2; ... %END;` repeats its text. `%GO TO L` continues
  at `%L: ;`. `%DEACTIVATE N` stops the replacement of N (a program
  variable N may then be declared), `%ACTIVATE N` starts it again.
- Arithmetic is DECIMAL(5,0) integer arithmetic; character strings are
  compared padded with blanks (`'ABC' < 'ABD'`).
- A member brought in by %INCLUDE is scanned too, its own %DECLARE and
  assignments included.

PL1C/370 runs the preprocessor on the source stream before forming
tokens (`src/pp.c`); without preprocessor statements its output is the
input (all 110 earlier tests compile to the same assembler). The test
runner compiles a program with % statements with MACRO.

## Not done

Preprocessor procedures (%PROCEDURE, RETURN, functions in replacement),
bit-string constants in preprocessor expressions.
