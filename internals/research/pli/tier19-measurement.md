# Tier 19 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. The test program `tests/pli/t95inc`
with its members in `tests/pli/inc/` prints what PL/I (F) prints
(JOB00316); with Tiers 0 to 18: 97 programs identical (JOB00318,
JOB00319).

## %INCLUDE

PL/I (F) does %INCLUDE in its preprocessor: the compile option MACRO,
and the members in a library -- fixed records of at most 100 bytes,
blocked by five at most -- named by the ddname in the statement or by
SYSLIB (Programmer's Guide GC28-6594-7, "The %INCLUDE Statement" and
"Source Statement Library"). `%INCLUDE DECLS;`, `%INCLUDE SYSLIB(STMTS),
STMTS;` and a member that includes another all put the members' records
where the statement stands; the source margins (2,72) apply to them.

PL1C/370 splices each member's records into the source stream after the
statement (`lex.c`), so included text may itself include. A member is
`<dir>/<member>.pli`, the name in lower case, from the `-I` directories;
the ddname is not looked at.

The test runner builds the library for a
job whose programs include: an IEBUPDTE step puts every member of
`tests/pli/inc/` into the temporary `&&INCLIB` (RECFM=FB, LRECL=80,
BLKSIZE=400, `DD DATA,DLM=@@`, since a PL/I comment begins `/*`), and
each such compile step gets `PARM='LOAD,NODECK,MACRO'` and
`PL1L.SYSLIB DD DSN=&&INCLIB`.

## Not done

The other preprocessor statements (%DECLARE, % assignment, %IF, %DO,
%GO TO, %ACTIVATE, preprocessor procedures): PL1C/370 refuses them.
