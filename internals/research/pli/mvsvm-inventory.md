# PL/I for MVS & VM: what PL1C/370 does not have yet

The default language is PL/I for MVS & VM's since 2026-10-15 (CLAUDE.md
"Language target"). This is the inventory of that language against
PL1C/370, by keyword, made on 2026-10-15.

## Method

- **The keywords of the target:** the headings of the table of contents
  of SC26-3114-01 (statements, attributes, conditions, built-in
  functions, subroutines and pseudovariables, options), and the keyword
  list of GC33-0009-4 (Part II, Section C) where the word also occurs in
  SC26-3114-01.
- **Known to PL1C/370:** the keyword occurs as a quoted string in
  `src/*.c`. Condition prefixes with NO are handled by a rule, not by
  name, and are not counted as missing.
- **Where each one comes from:** a whole-word count in the text of
  GC28-8201-4 (PL/I (F)), GC33-0009-4 (Optimizer, Release 3) and
  SC33-0006-7 (Optimizer Programmer's Guide, Release 5.1), all from
  `pdftotext -layout` of the files in `internals/manuals/`. The two older
  ones are OCR, so a count of 0 can be a misread; each 0 that decides a
  level was looked at in SC26-3114-01 as well.
- **Limits:** a keyword PL1C/370 knows can still lack options or uses;
  this list does not see those. An option of a known statement shows up
  only when the option is a keyword of its own.

## 1. In PL/I for MVS & VM, not in the Optimizer (level MVS)

| Item | Kind | Notes |
|---|---|---|
| X constants: `'C1'X` | constant | done (Tier 86) |
| BX | constant | done (Tier 86) |
| NOEXECOPS | procedure option | done (Tier 82) |
| BYVALUE, BYADDR | procedure and entry options | done for ASSEMBLER entries and MAIN procedures (Tier 90) |
| OPTIONAL | parameter attribute | done (Tier 90) |
| FETCHABLE | procedure option | needs FETCH (section 2) |
| EXTERNAL(name) | attribute | done (Tier 89) |
| %PROCESS | compiler statement | done (Tier 86) |
| SYSNULL | built-in function | done (Tier 86) |
| POINTERADD, POINTERVALUE, BINARYVALUE | built-in functions | done (Tier 86) |
| pointer arithmetic: `P + n`, `P - n`, `P2 - P1`, `<` and `>` on pointers | expressions | done (Tier 88), with LANGLVL(SPROG) |
| ENTRYADDR | built-in function and pseudovariable | done (Tier 87) |
| DATETIME | built-in function | done (Tier 86) |
| NOT, OR | compiler options | the characters for NOT and OR |

## 2. In the Optimizer, not in PL/I (F) (level OPT, missing)

| Item | Kind | Notes |
|---|---|---|
| FETCH, RELEASE | statements | done for assembler entries (Tier 91); PL/I procedures need a shared run time |
| ATTENTION | condition | done (Tier 106; a real attention not yet measured) |
| CONNECTED | attribute | a parameter in connected storage |
| CURRENTSTORAGE | built-in function | storage of a VARYING item's current length |
| PLIRETC, PLIRETV | subroutine, built-in function | done (Tier 87) |
| PLIDUMP | subroutine | done (Tier 105) |
| PLISRTA to PLISRTD | subroutines | done (Tier 107) |
| PLICKPT, PLIREST, PLICANC | subroutines | done (Tier 108; a checkpoint taken and a restart not tested) |
| SAMEKEY | built-in function | VSAM: the next record has the same key |
| VSAM, KEYLOC, REUSE, BUFND, BUFNI, BUFSP, PASSWORD, SIS, SKIP | ENVIRONMENT options | VSAM data sets (MVS 3.8j has VSAM) |
| REREAD, LEAVE, SCALARVARYING, BUFOFF, ASCII, ADDBUFF | ENVIRONMENT options | tape and data set handling |
| ASSEMBLER (with RETCODE, INTER) | entry option | done (Tier 89) |
| COBOL, FORTRAN; NOMAP, NOMAPIN, NOMAPOUT | procedure and entry options | interlanguage calls |
| PARMSET, COMPILETIME, COUNTER | preprocessor built-in functions | |
| RESCAN, NORESCAN | options of %ACTIVATE | |

## 3. In PL/I (F) already (level F, missing)

These are gaps against the oracle itself (TODO.md, section 3, has most):

| Item | Kind | Notes |
|---|---|---|
| COMPLEX, REAL, IMAG, CONJG, COMPLEX() | attribute, built-ins | complex arithmetic |
| TASK, EVENT, WAIT, PRIORITY, COMPLETION, STATUS, DELAY | multitasking | |
| DISPLAY ... REPLY | statement | the operator console |
| TRANSIENT, PENDING | file attribute, condition | teleprocessing |
| EXCLUSIVE, UNLOCK, NOLOCK | record I/O | locking in DIRECT UPDATE files |
| REDUCIBLE, IRREDUCIBLE | attributes | optimization hints |
| ENVIRONMENT options other than REGIONAL(n) and INDEXED | | PL1C/370 refuses them all: CONSECUTIVE, the record formats F, FB, V, VB, U with sizes, CTLASA, CTL360, GENKEY, INDEXAREA, NCP, TRKOFL, TOTAL, BACKWARDS and the rest |

## 4. Excluded from the default

The list with the reasons and what might come later: `docs/language-levels.md`,
"Excluded from the default".

| Item | Why |
|---|---|
| GRAPHIC, MIXED, G and GX constants, MPSTR, the DBCS built-ins | DBCS (CLAUDE.md) |
| PLITEST, CEE callable services, LE's messages and MSGFILE, CEEUOPT | Language Environment only (CLAUDE.md). The Optimizer's run-time options (PLIXOPT, ISASIZE, STAE, REPORT) are not excluded |
| PLITDLI, ASMTDLI, CEETDLI, SYSTEM(CICS), SYSTEM(IMS) | IMS and CICS, which MVS 3.8j does not have |

What depends on LE and what the Optimizer had already:
`../le-light-study.md`.

## What follows

- The MVS items of section 1 are small and independent; most are a
  tier each or less. X and BX constants, %PROCESS, SYSNULL, POINTERADD,
  POINTERVALUE, BINARYVALUE and DATETIME are the cheap ones.
- PLIRETC and PLIRETV matter for external REXX functions and callers
  that read R15 (TODO.md item 1, "the LINKed case").
- BYVALUE, OPTIONAL and FETCH/FETCHABLE belong together with the
  run-time questions (the LE study, #177, #179).
