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
| BYVALUE, BYADDR | procedure and entry options | done for ASSEMBLER entries and MAIN procedures (Tier 90), external PL/I procedures (Tier 123) |
| OPTIONAL | parameter attribute | done (Tier 90) |
| FETCHABLE | procedure option | needs FETCH of PL/I procedures (section 2, #6) |
| EXTERNAL(name) | attribute | done (Tier 89) |
| %PROCESS | compiler statement | done (Tier 86) |
| SYSNULL | built-in function | done (Tier 86) |
| POINTERADD, POINTERVALUE, BINARYVALUE | built-in functions | done (Tier 86) |
| pointer arithmetic: `P + n`, `P - n`, `P2 - P1`, `<` and `>` on pointers | expressions | done (Tier 88), with LANGLVL(SPROG) |
| ENTRYADDR | built-in function and pseudovariable | done (Tier 87) |
| DATETIME | built-in function | done (Tier 86) |
| NOT, OR | compiler options | done (Tier 121) |

## 2. In the Optimizer, not in PL/I (F) (level OPT, missing)

| Item | Kind | Notes |
|---|---|---|
| FETCH, RELEASE | statements | done for assembler entries (Tier 91); PL/I procedures wait for #6 (F-A or F-B) |
| ATTENTION | condition | done (Tier 106; a real attention not yet measured) |
| CONNECTED | attribute | done (Tier 103, no effect) |
| CURRENTSTORAGE | built-in function | done (Tier 103) |
| PLIRETC, PLIRETV | subroutine, built-in function | done (Tier 87) |
| PLIDUMP | subroutine | done (Tier 105) |
| PLISRTA to PLISRTD | subroutines | done (Tier 107) |
| PLICKPT, PLIREST, PLICANC | subroutines | done (Tier 108; a checkpoint taken and a restart not tested) |
| SAMEKEY | built-in function | VSAM (#4, waits for libc370#623 to #625) |
| VSAM, KEYLOC, REUSE, BUFND, BUFNI, BUFSP, PASSWORD, SIS, SKIP | ENVIRONMENT options | VSAM data sets (#4); KEYLOC, BUFND, BUFNI, BUFSP accepted without effect (Tier 122) |
| REREAD, LEAVE, SCALARVARYING, BUFOFF, ASCII, ADDBUFF | ENVIRONMENT options | done (Tier 122): SCALARVARYING with effect, REREAD, LEAVE, ADDBUFF without; BUFOFF and ASCII (ASCII tapes) refused. Also the record format letters with RECSIZE and BLKSIZE |
| ASSEMBLER (with RETCODE, INTER) | entry option | done (Tier 89) |
| COBOL, FORTRAN; NOMAP, NOMAPIN, NOMAPOUT | procedure and entry options | interlanguage calls (#5) |
| PARMSET, COMPILETIME, COUNTER | preprocessor built-in functions | done (Tier 104) |
| RESCAN, NORESCAN | options of %ACTIVATE | done (Tier 104) |

## 3. In PL/I (F) already (level F, missing)

These are gaps against the oracle itself (TODO.md, section 3, has most):

| Item | Kind | Notes |
|---|---|---|
| COMPLEX, REAL, IMAG, CONJG, COMPLEX() | attribute, built-ins | done (Tiers 113 to 120, #2) |
| TASK, EVENT, WAIT, PRIORITY, COMPLETION, STATUS, DELAY | multitasking | EVENT, WAIT, COMPLETION, STATUS, DELAY done; TASK and PRIORITY wait for #3 (A, B or C) |
| DISPLAY ... REPLY | statement | done (Tier 94) |
| TRANSIENT, PENDING | file attribute, condition | teleprocessing |
| EXCLUSIVE, UNLOCK, NOLOCK | record I/O | done (Tier 93) |
| REDUCIBLE, IRREDUCIBLE | attributes | done (Tier 92) |
| ENVIRONMENT options other than REGIONAL(n) and INDEXED | | done (Tiers 95 and 122): the record formats with sizes, CONSECUTIVE, CTLASA, CTL360 and the rest; ASCII and BUFOFF refused |

## 3a. Refused by the compiler, 2026-10-19

The keyword count above cannot see an option or a use the compiler
knows the keyword for and still refuses. Those are the compiler's own
"not supported" messages, 95 of them on 2026-10-19 before Tier 124.
Tier 124 took RETURN out of a BEGIN block, PUT LINE (and the SKIP
pattern behind it), array expressions in PUT LIST, `||` of a bit and a
character string, BIT of a FLOAT or scaled value, MOD of FLOAT and of
scaled values, FLOAT subscripts, STATIC INITIAL of a numeric picture,
INITIAL of more than 256 characters, INITIAL of label variables, LIKE
of members with INITIAL, bit strings as arithmetic operands, and
INITIAL on BASED variables, BASED variables in data-directed lists, the
listing control statements and %NOTE (OPT). What is left, by size:

- **Waiting for a decision:** TASK, PRIORITY (#3); FETCH of PL/I
  procedures, FETCHABLE (#6); COBOL and FORTRAN (#5); VSAM (#4).
- **Small:** keyed record I/O of a VARYING string; PUT DATA of more
  than 3 dimensions; DEFINED with packed bit strings; INITIAL CALL for
  factored names. (Done since: a scalar DEFINED on a subscripted base,
  Tier 126;
  pictures in GET DATA and DO, SUBSTR of a character picture, Tier 125;
  GET DATA of arrays and of arrays of structures, Tier 126.)
- **Limits of the implementation:** a frame over 4095 bytes, 12 KB of
  code per procedure, more than 16 events in a WAIT, a structure with
  REFER in an area or in INTO/FROM/LOCATE.

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
