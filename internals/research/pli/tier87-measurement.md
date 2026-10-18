# Tier 87: PLIRETC, PLIRETV, ENTRYADDR, STATIC pointer INITIAL

The next items of the inventory (`mvsvm-inventory.md`).

## The rules

- **PLIRETC, PLIRETV** (the Optimizer's, SC33-0006-7, "Return Codes";
  SC26-3114-01 chapter 16): `CALL PLIRETC(n)` sets the PL/I return code,
  PLIRETV (or `PLIRETV()`) returns it as FIXED BINARY(31,0). The step's
  code is the thousands of the way the program ended -- 0000 normal,
  1000 STOP or EXIT, 2000 ERROR -- plus the return code, which is set to
  999 when more is given (with a message the manual does not quote).
  PL/I (F) ends STOP with 1000 and ERROR with 2000 as well (JOB00171).
- **ENTRYADDR(x):** a pointer, the address of the first instruction of
  the external entry x. As a pseudovariable, `ENTRYADDR(v) = p` sets the
  entry variable v to the code at p (an internal procedure there is
  "unpredictable").
- **INITIAL of a STATIC pointer:** the Optimizer allows NULL there
  (GC33-0009-4, INITIAL rule 7); PL/I (F) does not -- `IEM0512I
  IDENTIFIER 'NULL' ... IN INITIAL ATTRIBUTE LIST IS NOT A KNOWN LABEL`,
  CC 12 (probe). PL/I for MVS & VM adds
  SYSNULL and POINTERVALUE(constant).

## In PL1C/370

- **PLIRETC** is a built-in subroutine (resolve.c accepts it as a CALL
  target only); gen.c calls PL1RETC, which keeps 0 to 999 in PL1RTCD.
  The main procedure returns PL1RTCD in R15 at its normal end, and
  pl1_stop and terminate add it to 1000 and 2000. A program LINKed to,
  an external REXX function, should so return its PLIRETC value in R15
  (libc370's @@CRT0 passes main's value on; not tested with XDRIVER).
- **ENTRYADDR** loads `=A(@Pn)` for a procedure of the compilation,
  `=V(name)` for one compiled apart, or an ENTRY variable's first word;
  the pseudovariable stores the pointer there and clears the frame word.
- **STATIC pointers:** check.c takes NULL, NULLO, SYSNULL and
  POINTERVALUE(constant) for a pointer's STATIC INITIAL, and dc_value
  writes X'FF000000', F'0' or F'n'. An OFFSET's INITIAL stays refused.
- **The runner:** PL/I (F) cannot set a return code, so a test's
  `<name>.cc` names the code PL1C/370 must end with; the twin's own code
  is then not compared.

## Tests

- `t234ret`: PLIRETV 0, then 999 after PLIRETC(1234), then 12; the step
  ends with 12. `t235ret`: PLIRETC(5), STOP: 1005. Both identical in
  JOB01399.
- `t236ead`: ENTRYADDR of an external procedure and of an ENTRY
  variable set to it, compared, and a CALL through a variable set by the
  pseudovariable. Identical in JOB01402.
- `t237sin`: STATIC pointers with INITIAL(NULL), (SYSNULL),
  (POINTERVALUE(16)). Identical in JOB01407.
- The full suite: 238 of 238 identical (JOB01410 to JOB01420).

## Not done

- The message for a PLIRETC value above 999.
- OPTIONS(RETCODE) for assembler entries, which PLIRETV would read too.
- An OFFSET's STATIC INITIAL(NULLO).
