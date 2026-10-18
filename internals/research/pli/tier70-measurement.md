# Tier 70: the ANSI conditions STRINGSIZE and STORAGE

ANSI X3.53-1976 has two conditions PL/I (F) does not:
- STRINGSIZE, a computational condition;
- STORAGE.

Beyond the oracle, tested as a twin pair (`t209ans`, identical in
JOB01101): PL1C/370 compiles the ANSI program, PL/I (F) a twin that
prints what the standard makes the program print.

## What the standard says

- **STRINGSIZE** is raised when a value is converted to a string type
  with a maximum length shorter than the value. That happens when a
  string is assigned to a shorter target (9.3, convert, step 8.3). After
  the on-unit the value is cut to the target (step 8.4).
- **It is enabled by default.** Test-enablement (6.4.1.2) disables only
  SIZE, STRINGRANGE and SUBSCRIPTRANGE when no prefix names them. The
  prefixes are STRINGSIZE and NOSTRINGSIZE, abbreviated STRZ and NOSTRZ
  (CM9, CM10, 2.7).
- **Its system action does nothing** (6.4.4, case 1). A program without
  an on-unit for STRINGSIZE sees no difference, so enabling it by
  default changes nothing for a PL/I (F) program.
- **STORAGE** is raised when an allocation finds no storage. After a
  normal return the allocation is tried again (7.2.5). Its system action
  is implementation-defined (6.4.4, case 4).
- **ONCODE** of either is implementation-defined.

## What the test shows

- `C3 = 'ABCDE'` for CHAR(3) C3 enters the on-unit, ONCODE 150, and C3
  is `ABC`. So do a VARYING target, a BIT(4) target (cut in a register)
  and a BIT(40) one (cut in storage).
- `C3 = 'AB'` raises nothing.
- `(NOSTRZ): BEGIN;` and `(NOSTRINGSIZE): QUIET: PROC;` disable it.
- SIGNAL STRINGSIZE enters the on-unit. After REVERT a cut raises nothing
  visible.
- SIGNAL STORAGE gives ONCODE 454.
- ALLOCATE of a CONTROLLED CHAR(32000) in a loop reaches the end of the
  region; the on-unit for STORAGE leaves the loop by GO TO, and all
  generations are freed.

## In PL1C/370

- **STRINGSIZE:** the run time's assignments that cut a string call
  `pl1i_strsize` (PL1SZCK). These are PL1ACHR, PL1AVCH, PL1BASG and
  PL1BPUT.
  - It raises the condition only while it is enabled (bit 4 of PL1NOCV,
    which the generated code sets for NOSTRINGSIZE) and an on-unit is
    established.
  - A bit string cut in a register is checked only when the program has
    an ON STRINGSIZE.
- **STORAGE:** `pl1_allocate` raises it while calloc fails. Before, the
  program ended.
- **The choices left to the implementation** are listed in
  `docs/language-levels.md`: ONCODE 150 and 454, and STORAGE's message
  and ERROR.

## The suite

All 211 programs are identical: JOB01103 to JOB01107.
