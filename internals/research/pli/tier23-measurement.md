# Tier 23 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probe (JOB00355), which is also the test `tests/pli/t102upd` (JOB00359); with
`t103upd2` (JOB00363) and Tiers 0 to 22: 105 programs identical
(JOB00365 to JOB00367).

## UPDATE and REWRITE

- A RECORD file opened (or declared) UPDATE [SEQUENTIAL] is read with
  READ; REWRITE FILE(f) FROM(x) puts x in place of the record read last.
  The program wrote five records of a structure, rewrote the second and
  the fourth, and read them back changed, the others as they were; the
  data set's bytes are the same as PL/I (F)'s.
- One file may be opened OUTPUT, closed, opened UPDATE, closed and opened
  INPUT: each OPEN has the declared attributes and its own, not those of
  the OPENs before.
- ENDFILE is raised for an UPDATE file as for an INPUT one.

PL1C/370 cannot use LIBC/370 here: its update mode (`+`) is for byte
streams and refuses record mode (`@@fpmode.c`). `rt/pl1qsam.asm` does
what PL/I (F) does itself: a DCB opened UPDAT, GET in locate mode,
PUTX for REWRITE, CLOSE; `rt/pl1io.c` uses it for UPDATE files. A
rewritten record keeps its length.

An UPDATE file opened implicitly by its first READ takes the DD of its
name; with no such DD PL/I (F) raises UNDEFINEDFILE (`IHE150I FILE G -
CANNOT BE OPENED (INCORRECT OR NO DD CARD)`, then ERROR, JOB00361), where
PL1C/370 stops with a message: UNDEFINEDFILE is not done.

## Not done

DELETE, KEYED and DIRECT access (REGIONAL, INDEXED), LOCATE and READ
SET, UNDEFINEDFILE.
