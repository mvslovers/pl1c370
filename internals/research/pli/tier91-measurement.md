# Tier 91: FETCH and RELEASE of assembler entries

The Optimizer's FETCH and RELEASE (GC33-0009-4; SC26-3114-01, chapter 5,
"Dynamic Loading of an External Procedure"), for OPTIONS(ASSEMBLER)
entries.

## The rules

- FETCH loads the named procedures "unless a copy already exists in
  main storage"; a CALL of an entry whose name "appears, somewhere in
  the calling procedure, in a FETCH or RELEASE statement" loads it too.
  "In neither case is it an error if the procedure has already been
  loaded." RELEASE frees the storage.
- FETCH, RELEASE and CALL "must specify entry constants".
- EXTERNAL('name') gives the name "by which the procedure to be fetched
  is known to the operating system" (`Dcl ET entry external('FF')
  options(asm); Fetch ET;`).

## In PL1C/370

- **parse.c, resolve.c:** FETCH and RELEASE take entry constants; each
  must be an OPTIONS(ASSEMBLER) entry, which is then marked `fetched`.
  A PL/I procedure is refused: fetched, it would have to share the
  program's run time -- SYSPRINT, conditions, the frame chain --
  which is stage 2 of `../le-light-study.md`.
- **rt/pl1fetch.asm:** PL1FTCH(name) returns the entry point from a table
  of 16, LOADing the module (SVC 8, EPLOC) when it is not in it;
  PL1RLSE(name) DELETEs it (SVC 9) and frees the slot. A module not
  found ends in LOAD's S806, which the ESTAE reports with IHE012I.
- **gen.c:** FETCH and RELEASE call those; a CALL of a fetched entry
  takes its entry point from PL1FTCH before the argument list is built.

## Test

`t243fet`: FETCH ASMTST, CALL, RELEASE, CALL again (loaded again at the
CALL), a CALL of ASMVAL loaded at its first CALL (RELEASE names it
later), RELEASE of both, and a RELEASE of what is no longer loaded. The
modules come from the RUN step's STEPLIB, the development library. The
twin calls ASMTST linked in. Identical in JOB01522 (after the HTTPD
4.3.0 / mvsMF 1.3.0 install on m38b); the full suite 244 of 244
(JOB01525 to JOB01539).

## Not done

- FETCH of PL/I procedures, FETCHABLE (see above).
