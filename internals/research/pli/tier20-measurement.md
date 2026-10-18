# Tier 20 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probes (Q1CTL,
the test `tests/pli/t96ctl`, JOB00322; Q2A to Q2D, JOB00329 and
JOB00330). With `t97ctl2` and Tiers 0 to 19: 99 programs identical
(JOB00334, JOB00335).

## CONTROLLED

- `ALLOCATE X` gives X a new generation and stacks the one before;
  references see the latest; `FREE X` frees it and the one before is
  back with its value (3, 2, 1 after three ALLOCATEs and FREEs).
- INITIAL is applied at each ALLOCATE (`C CHAR(5) CTL INIT('FIRST')` is
  `FIRST` in every new generation, `S.N INIT(7)` 7 again).
- `ALLOCATE S, X` and `FREE S, X` take several variables.
- `ALLOCATION(X)` is `'1'B` while X has a generation.
- A generation outlives the block that allocated it, and one variable
  declared in an internal procedure keeps its stack across the calls of
  that procedure (t97ctl2).
- Scalars, arrays, structures, CHARACTER VARYING; PUT DATA writes the
  names as for any variable; GET and arguments see the latest
  generation.

PL1C/370 keeps a static anchor per CONTROLLED variable. A generation is
a block from the run time's storage: A(the generation before), a word,
then the data (a structure at its doubleword phase); the anchor points
at the latest. ALLOCATE and FREE push and pop in the generated code.

## A failure of PL/I (F)

PL/I (F) V5.5 cannot address the second element of a CONTROLLED array of
FIXED BINARY(15) (halfword) elements: PUT LIST or PUT DATA ends with
`IHE803I PROTECTION VIOLATION` after the first element -- a plain array,
an array member of a structure, an array of structures alike (JOB00324,
JOB00326, JOB00328, JOB00329). FIXED BINARY(31) works (JOB00330), as does
FIXED DECIMAL (t96ctl). PL1C/370 addresses such arrays; the tests avoid
them, since the oracle cannot run them.

## Not done

CONTROLLED with bounds or lengths given at ALLOCATE (`DCL A(*) CTL`,
`ALLOCATE A(N)`), CONTROLLED parameters, the STORAGE condition.
