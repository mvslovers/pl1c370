# Tier 67: GENERIC

`DCL CALC GENERIC (CFX ENTRY(FIXED DEC(5)), CFL ENTRY(FLOAT DEC(6)), ...)`
names a family of entry points. A reference to CALC invokes the member
whose parameters match the arguments in number and attributes
(GC28-8201-2, "Generic Names and References"). PL1C/370 refused the
attribute (`ansi-comparison.md`).

Measured 2026-10-14 on MVSTK5-BLD with the probes:

| Probe | Job | Shows |
|---|---|---|
| Q1GEN | JOB01070 | CALL and function references, five members of CALC, two of F2 |
| Q2GEN | JOB01070 | arguments no member matches exactly |
| Q3GEN | JOB01071 | CHAR(4), BIT(\*), CHAR(\*) VARYING, an array, no arguments, three arguments |
| Q4GEN | JOB01071 | a CHAR(3) argument and members CHAR(4) and CHAR(5) |

The tests `t205gen` (Q1GEN) and `t206gen` (Q3GEN) are identical in
JOB01073.

## The rules

- **The number of arguments** must be the member's number of
  parameters. `ENTRY` with no list takes a call with no arguments.
- **An arithmetic argument** must have the member's base, scale and
  precision. These find no member among FIXED DEC(5) and FLOAT DEC(6)
  (IEM1065I, "NO GENERIC SELECTION POSSIBLE"):
  - FIXED DEC(7) and FIXED DEC(5,2);
  - FLOAT DEC(16);
  - FIXED BIN(15);
  - the constants 5 (FIXED DEC(1)) and 2.5E0 (FLOAT DEC(2)).
- **A string** must be of the member's kind, CHARACTER or BIT, and
  VARYING or not. Its length is not compared:
  - CHAR(4) members take a CHAR(4) argument, and so do CHAR(\*) ones;
  - `CHAR(*) VARYING` takes the VARYING argument;
  - a CHAR(3) argument with members CHAR(4) and CHAR(5) is ambiguous
    (IEM1066I, "MORE THAN ONE GENERIC SELECTION IS POSSIBLE").
- **An array argument** matches a member with as many dimensions:
  `(*) FIXED DEC(5)`.
- **No member, or more than one,** is an error at compile time (CC 12).

## In PL1C/370

- **parse.c:** each member's `ENTRY(...)` and `RETURNS(...)` becomes an
  entry declaration of the block, as `DCL CFX ENTRY(...)` would. A
  descriptor may have a dimension list, counted for the selection.
- **resolve.c** finds each member's procedure as a call through its
  declaration would. A reference to the generic name becomes a call
  whose member is still open.
- **check.c** (`gen_select`) compares the arguments with each member and
  takes the one that matches. It does so before the bounds of CHAR(\*)
  parameters are settled (`star_call`).

## Not done

- **A generic name as an argument**, or assigned to an entry variable.
- **Members that are themselves generic.**

## The suite

All 208 programs are identical: JOB01075 to JOB01079.
