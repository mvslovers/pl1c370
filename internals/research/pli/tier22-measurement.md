# Tier 22 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probes (Q1EFP,
JOB00345; Q2FPAR, JOB00348), which are also the tests
`tests/pli/t100efp` and `t101fpa` (JOB00350). With Tiers 0 to 21: 103
programs identical (JOB00352 to JOB00354).

## ENTRY and FILE parameters

- A parameter declared `ENTRY [RETURNS(...)]` takes a procedure: it is
  invoked as a function (`F(X)`) or by CALL (`CALL E;`).
- A procedure's name written without an argument list as an argument is
  passed as the procedure, not invoked: with the parameter declared
  ENTRY in the ENTRY declaration silently, without one PL/I (F) says so
  (IEM0786I "name, not value, of function GREET passed as argument").
- A parameter declared FILE takes a file: PUT FILE(G) writes to the file
  passed, SYSPRINT or a file of the program.

PL1C/370 passes a procedure as 8 bytes: its address and the frame of its
parent block (the static link it is called with). A file is passed by
reference, so a FILE parameter's address is the file's descriptor.

## PRINT files, in their bytes

A PRINT file's records keep the blank after their last LIST or DATA item
(`' MAIN                    ONE '`), trailing blanks are not removed
(`'TWO  '` of a CHAR(5) stays), and a record is at least 10 bytes with
its ASA character: the page eject of OPEN is `'1'` and nine blanks
(JOB00348, REPORT read in its bytes). This is the rule Tier 5 measured
for STREAM files that are not PRINT; earlier tests compared PRINT output
as text only, where it does not show. `rt/pl1io.c` writes PRINT records
so now.

## Not done

ENTRY and FILE variables (not parameters), an ENTRY parameter invoked
without arguments as a function, ENTRY parameters with descriptors of
their own parameters.
