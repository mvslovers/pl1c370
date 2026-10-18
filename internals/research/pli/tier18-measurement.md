# Tier 18 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probe (JOB00304, its first form; then the test `tests/pli/t93lab`, JOB00309).
With `t94lab2` and Tiers 0 to 17: 96 programs identical (JOB00313,
JOB00314).

## Label data

- `DCL L LABEL`, label arrays and label members of structures take label
  constants and each other's values; GO TO a variable or an array element
  goes to the label it holds: a switch (`GO TO LA(K)`), out of a called
  procedure, out of an on-unit, to a label of a BEGIN block.
- A label parameter (`DCL X LABEL` in the procedure) passes a label on,
  through as many calls as needed.
- A label value names the activation it was taken in: in a recursive
  procedure, `SAVED = HERE` at depth 2 and `GO TO SAVED` from depth 0
  continue at depth 2 (t94lab2: `HERE AT 2`, then depth 3 runs on).
- Labels cannot be compared: `IF L2 = A1` and `IF L2 ¬= B1` are severe
  errors (IEM1024I, "illegal use", JOB00304). PL1C/370 refuses them.
- A label on the END of a group is a point at the end of the group:
  `GO TO NXT` with `NXT: END;` ends the iteration.

PL1C/370 keeps a label value in 8 bytes: the address of a stub in the
label's block and the frame of the block's activation. GO TO a variable
ends the blocks above that frame (PL1GOTO) and enters the stub with R13
at the frame; the stub loads the block's three base registers and
branches to the label. A block has stubs only for the labels used as
values.

## The test runner

A REPORT data set that the program never opens has no end of file, and
reads back as what an earlier data set left on its tracks: t93lab, which
writes no REPORT, showed another program's PUT DATA lines (JOB00307).
`tests/run_mvs.py` now compares REPORT only for programs that use it.

## Not done

INITIAL of a label variable, ENTRY and FILE variables (PL/I (F) has
entry and file parameters, not variables; not measured), labels in PUT
or GET.
