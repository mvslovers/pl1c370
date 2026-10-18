# Tier 33: SELECT, LEAVE, UNTIL, REPEAT -- beyond the oracle

PL/I (F) does not have these statements; they came with the OS PL/I
Checkout and Optimizing Compilers. The specification is GC33-0009-4
(October 1976, V1R3): chapter 5 ("SELECT Statement", "DO Statement",
"LEAVE Statement") and section J (the DO statement's general formats,
figure J.3, and its expansions).

## How it is tested

Each test is a pair:

- `tests/pli/<name>.opt.pli` is written in the Optimizing Compiler's
  language. PL1C/370 compiles it.
- `tests/pli/<name>.pli` is its twin. It spells out the expansion that
  GC33-0009-4 gives for each construct, in PL/I (F)'s language: IF, GO
  TO, labels and temporaries. PL/I (F) runs it.

`tests/run_mvs.py` compares the two as it compares any test, so the
expected output comes from PL/I (F) running the documented expansion.
`t125sel` (SELECT) and `t126leav` (LEAVE, UNTIL, REPEAT) are identical
(JOB00536).

## The rules implemented

- **SELECT (e).** e is evaluated once and kept. The WHEN values are
  evaluated in order and compared with it as `=` compares; the first
  that is equal selects its action, and no later value is evaluated.
  `SELECT(F(2)); WHEN(F(1), F(2), F(3))` calls F three times, not four.
  If none is equal, the OTHERWISE action runs.
- **SELECT without e.** Each WHEN value is a condition: true if any bit
  is 1.
- **The action** is one statement: a group, a select-group or a BEGIN
  block among them. After it, control goes to the end of the
  select-group.
- **No WHEN selected and no OTHERWISE** raises ERROR. GC33-0009-4
  names no ONCODE for it; PL1C/370 gives 3, the code of "SIGNAL ERROR
  in place of a statement in error". The tests do not print it.
- **LEAVE** goes to the end of the innermost do-group: iterative, DO
  WHILE/UNTIL, or a plain `DO;`. `LEAVE label` goes to the end of the
  do-group whose DO statement has that label. LEAVE never leaves a
  block; PL1C/370 refuses a LEAVE that would. A select-group is not a
  do-group, so LEAVE in a WHEN action leaves the DO around it.
- **DO WHILE(a) UNTIL(b)** (either order): WHILE is tested before each
  pass, UNTIL after it. DO UNTIL without WHILE makes at least one pass.
- **In a specification**, `UNTIL(u)` is tested after each pass. If it
  is true, that specification ends and the next one begins.
  `REPEAT e` takes the place of TO and BY: after each pass the control
  variable is assigned e. A locator can be the control variable of
  REPEAT: `DO P = HEAD REPEAT P->NX WHILE(P ¬= NULL)` walks a list.

## Not done

END with a label closing several groups at once (`END A;` over an
unclosed inner group), and a pseudovariable as control variable. The
DEFAULT statement is Tier 34.
