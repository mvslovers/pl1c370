# Tier 51: BUILTIN, default RETURNS, OPTIONS(REENTRANT), ENTRY in external procedures

The gaps below were met compiling programs from teampli.net and Iron
Spring with PL1C/370. Those programs are not in this repository, because
their licences are unclear. Measured 2026-10-13 on MVSTK5-BLD: a probe (JOB00825), which is also the test `t166blt`.
That test and `t167ent`/`t168ens` are identical (JOB00827).

## The rules

- **BUILTIN.** `DCL (SUBSTR, LENGTH, NULL, INDEX) BUILTIN;` declares the
  names as the built-in functions. A declaration in an inner block hides
  it there: `DCL SUBSTR FIXED BIN(15) INIT(5)` in a BEGIN block gives 5.
- **A function without RETURNS** returns a value of the attributes its
  name's first letter gives: I to N FIXED BINARY(15), otherwise FLOAT
  DECIMAL(6). KOUNT(7) returns 21 and XSQ(1.5E0) returns 2.25E+00. The
  invoking block assumes the same attributes, so no ENTRY ... RETURNS
  declaration is needed.
- **A secondary entry point** (`HALF: ENTRY(V);` in TWICE) works as in
  Tier 26: HALF(9) gives 4.5000000000.
- **OPTIONS(REENTRANT)**, as in avltree.pli, is taken and changes nothing:
  PL1C/370's code is reentrant anyway. OPTIONS may now be a list
  (`OPTIONS(MAIN, REENTRANT)`).
- **ENTRY statements in an external procedure** other than the main one,
  as in the string routines from teampli.net:
  - `t167ent`: a stack with PUSH, POP and SHOW, compiled together;
  - `t168ens`: the same compiled apart.

  Each name (the procedure's and each entry's) is an entry point of its
  own. Entered there, R0 is set to the entry's number before the
  procedure's prologue, as a call within the source sets it. So a caller
  compiled apart reaches the right entry through `=V(name)`.

## Also: the REPORT data sets of the test runner

Every test step gets a REPORT DD, but only a program that declares or
opens REPORT ever writes it. A data set never opened has no end of file.
mvsMF's read of it logged `MVSMF106E I/O ERROR ... ERRNO=5` and still
answered 200 with stale data (mvsmf#421). REPORT is now allocated
DSORG=PS, and read only for the programs that use it.

## Not done

Found in the same programs, and left for later:

- **Adjustable extents for AUTOMATIC:** `CHAR(LENGTH(A))` in a BEGIN
  block.
- **More than 4095 bytes of AUTOMATIC storage.**
- **Entries with RETURNS in a procedure without RETURNS** (SETSUC/FAIL/SUC).
- **gauss.pli's FORMAT statements:** PL1C/370 refuses them with "only
  decimal constants are supported". The cause is not looked into.
- **What lies beyond PL/I (F):** `BASED` without a pointer, CONNECTED,
  `%REPLACE`, STRINGSIZE.
