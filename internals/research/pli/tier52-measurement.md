# Tier 52: FORMAT statements, the R format, repetitive data lists

Found in gauss.pli from teampli.net. PL/I (F) compiles it, and PL1C/370
refused it with "only decimal constants are supported". That program is
not in this repository, because its licence is unclear. Measured
2026-10-13 on MVSTK5-BLD with a probe (JOB00841).

The tests:

- `t169rfm`: Q1RFMT without its label variable (below), identical
  (JOB00844);
- `t170rpt`: nested repetitive specifications, BY, WHILE, GET and PUT
  in LIST, EDIT and DATA form, identical (JOB00846).

## The rules

- **A FORMAT statement** (`F2: FORMAT (3 F(8,2), X(2), A);`) names a
  format list. `R(F2)` in another format list stands for it. A FORMAT
  statement may use R itself: F3's `R(F1)`.
- **An iteration factor without parentheses** repeats the item after it:
  `3 F(8,2)`, `6 F(5,1)`, `3 A(2)`. Before a parenthesized list
  (`2 (F(4), X(1))`) it repeats the list.
- **A letter right after a number** is read as if a blank stood between
  them: PL/I (F) writes IEM0009I and goes on, so gauss.pli's `2F(10,3)`
  is `2 F(10,3)`. A `B` right after the digits stays an error in
  PL1C/370, since that is a binary constant.
- **A repetitive specification in a data list**,
  `(A(I) DO I = 1 TO N)`, transmits its items for each value of the
  control variable, as a DO statement would run them:
  - the specification takes TO, BY, WHILE and lists (`DO J = 1, 4`);
  - it may hold several items (`(V(I), C(I) DO I = 1 TO 3)`) and nest
    (`((M(I,J) DO J = 1 TO 4) DO I = 1 TO 3)`);
  - it is taken in GET LIST and EDIT and in PUT LIST, EDIT and DATA
    (GET DATA is not tested); PUT DATA
    writes each element with its subscripts, `V(1)=       1` and so on;
  - an expression is an item too: `(I * I DO I = 1 TO 4)`.
- **GET EDIT reads on in the record it stopped in.** Q1RFMT reads N with
  F(5) from the first record. The F(5,1) items that follow read the
  rest of that record, all blanks, so the first three elements are 0.

## In PL1C/370

- parse.c:
  - keeps a FORMAT statement as one with its format list;
  - after parsing, replaces each `R(label)` by the list of the FORMAT
    statement of that label in the procedure or a containing one;
  - builds a repetitive specification as a loop over the items' list.
- lower.c turns that loop into a DO loop around the items' transmission,
  nested where they nest.

## Not done

- **R with a label variable** (`L = F1; ... (R(L))`): PL/I (F) takes it
  and writes N with F1's format. PL1C/370 expands R while compiling and
  refuses it.
- **gauss.pli itself** now stops at its next gap: more than 4095 bytes
  of AUTOMATIC storage.
