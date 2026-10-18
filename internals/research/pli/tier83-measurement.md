# Tier 83: language levels

The frame for the language target of 2026-10-15 (CLAUDE.md "Language
target"): the default is the Optimizing Compiler's language, `-lang=F`
is PL/I (F)'s, and every feature beyond PL/I (F) carries its level.
`docs/language-levels.md` has the result; this note says how it was
checked.

## The marks, checked against the manuals

The text of GC33-0009-4 (bitsavers, October 1976, Release 3) was read
with `pdftotext -layout`. Each feature was looked up in the keyword list
(Part II, Section C) and, where the list did not decide it, in the rules
of its statement or attribute:

- **OPT,** in the keyword list: SELECT, WHEN, OTHERWISE, LEAVE, UNTIL,
  REPEAT, DEFAULT, ACOS, ASIN, OFFSET(x1,x2), POINTER(x1,x2), VARIABLE,
  CONDITION (attribute), GENERIC with WHEN, COPY[(file-expression)],
  STRINGSIZE (condition and prefix).
- **OPT, by the rules:**
  - "BASED[(element-locator-expression)]" (Section I): BASED without a
    pointer, and `BASED(P(I))`;
  - "A pointer expression is either a pointer variable, which can be
    qualified or subscripted, or a function reference" (Part I,
    chapter 8), and "Multiple Locator Qualification": any pointer
    expression as the qualifier, and chains;
  - INITIAL items "reference | constant | (expression)" (Section I,
    rule 8); STATIC takes constants only (rule 7);
  - ALIGNED and UNALIGNED "for element, array, or structure variables",
    of any type (Part I): UNALIGNED pointers;
  - cross sections with an asterisk "for a subscript", any of them:
    `K(*,3,*)`.
- **ISO,** in neither: AFTER, BEFORE, COLLATE, COPY(s,n), DECAT, DOT,
  EVERY, SOME, ONFIELD, PAGENO, REVERSE, SUBTRACT, VALID; radix factors
  B1 to B4 in constants and format items; TAB (option and format item);
  the STORAGE condition (STORAGE is a built-in function only);
  CONSTANT, PARAMETER, LOCAL, STRUCTURE, MEMBER, NONVARYING, DIMENSION;
  VARIABLE for data and labels ("used only" with ENTRY and FILE);
  precision ranges in a generic description (the Optimizer: "Number of
  digits and scale factor must be specified"); a GENERIC member
  without WHEN.
- **V2:** NOEXECOPS is in neither GC33-0009-4 nor the Programmer's
  Guide SC33-0006-7 (September 1985, Release 5.1). The Optimizing
  Compiler's way for an assembler caller is the entry point PLICALLA
  (SC33-0006-7, chapter 11). NOEXECOPS belongs to OS PL/I Version 2.

## Where the Optimizer differs (found, not yet followed)

- STRINGSIZE: "disabled by default and within the scope of a
  NOSTRINGSIZE condition prefix"; standard system action "A message is
  printed and processing continues" (Section H). PL1C/370 follows ANSI:
  enabled, no system action.
- The PARM split at `/` for run-time options (SC33-0006).

## In PL1C/370

- **`beyond_f(line, level, what)`** (main.c) is the whole of the level
  check: with `-lang=F` it reports "... is not PL/I (F) but the
  Optimizing Compiler's (-lang=F)" (or ISO, or Version 2's) as an
  error, otherwise nothing. `enum level` (pl1c.h) replaces resolve.c's
  L_F/L_ANSI.
- **It is called where each feature is recognized:** parse.c for
  statements, attributes, references, INITIAL, GENERIC, format items,
  OPEN options and conditions; lex.c for radix constants; resolve.c for
  a built-in function whose table entry is not L_F.
- **Without `-lang=F` nothing changes:** the assembler of every test,
  `.opt.pli` included, is byte for byte the same as main's, and with
  `-lang=F` the same as without it.

## The checks

- **Host:** of the 230 tests, 211 have no `.opt.pli`; the 204 of them
  compiled as one source all pass with `-lang=F` (the 7 `.sep` tests go
  through the runner). The PL/I (F) twins of the `.opt.pli` tests pass
  as well, except t208anb's: PL1C/370 refuses INDEX of bit strings with
  or without `-lang=F`, a gap of its own. All 19 `.opt.pli` are refused,
  each at its first feature beyond PL/I (F).
- **The runner** compiles every test without a `.opt.pli` with
  `-lang=F`, and stops if `-lang=F` accepts a `.opt.pli`.
- **MVS:** the full suite, 230 of 230 identical (JOB01298 to
  JOB01306). The three XDRIVER tests first showed no condition code for
  PL/I (F): the runner matched the step names `OnnX` with two digits,
  and in the full suite they are `O141X` to `O143X`. Read again with
  `O\d+X`, both runs ended 0000.

## Not done

- A function reference without arguments as a locator qualifier
  (`GETP -> X`) is not refused under `-lang=F`; PL/I (F) refuses it
  (Tier 69).
- The deviations above; NOEXECOPS's place (V2 in the default, or only
  `-plist` and a PLICALLA form) is for the maintainer.
