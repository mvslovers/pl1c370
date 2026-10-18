# Tier 47: external procedures (batched) and EXTERNAL variables

Measured 2026-10-13 on MVSTK5-BLD. The probes are:

- Q1EXT (JOB00778)
- Q2EXT (JOB00781)

The tests are identical:

- `t154ext`, which is Q1EXT: JOB00780
- `t155ex2`, which is Q2EXT with INNER returning FIXED BIN(15): JOB00783

## What PL/I (F) does

- **Batched compilation.** One PL1LFCLG step compiles several external
  procedures, each after a `*PROCESS;` card. Each gets IEM0526I ("OPTION
  MAIN HAS NOT BEEN SPECIFIED") if it has no OPTIONS(MAIN), and the
  linkage editor binds them into one load module, each a control
  section of its own.
- **Calls between them** work with an ENTRY declaration and its
  descriptors, which convert the arguments: `CALL ADDTO(N, 7)` with
  `ENTRY(FIXED BIN(31), FIXED BIN(31))`. An external procedure may call
  another one, contain internal procedures, and write SYSPRINT. The
  lines of all of them go into the one print file.
- **An EXTERNAL variable** is one storage for its name in all of them.
  Each is a control section named after the variable (COUNT, SHARED,
  LEVEL, NAME in the linkage map):
  - INITIAL in the main procedure sets it (COUNT starts at 100), and so
    does INITIAL in another procedure (LEVEL starts at 10).
  - **The exception (Q2EXT):** `DCL NAME CHAR(8) EXTERNAL` without
    INITIAL in the main procedure, and `INIT('INITIAL')` in SHOWIT, leaves
    NAME as eight X'00' bytes. The map shows the main procedure's NAME as
    the control section that holds it. An arithmetic scalar without
    INITIAL (LEVEL in the main procedure) did not win over the INITIAL in
    SHOWIT. The rule this suggests: an arithmetic scalar without INITIAL
    is a common area, everything else is a control section, and the first
    control section in the link order holds the storage.

## In PL1C/370

parse.c reads one or more external procedures, skipping `PROCESS`. The
`*` stands in column 1, outside the margins the lexer reads. Exactly one
procedure has OPTIONS(MAIN); it is put first, and the others are linked
after it (`next_ext`). How the passes treat them:

- **resolve.c:** each is a block of its own, with its own implicit
  declarations and DEFAULT. A name that is another external procedure of
  the source becomes a call to it, declared ENTRY or not.
- **check.c:** finds the bounds of CHAR(*) parameters from the calls in
  all of them before it checks any.
- **gen.c:** gives an external procedure no parent frame and no main
  prologue. EXTERNAL variables and file constants share one label per
  name; the storage goes with the declaration the rule above picks.

`EXTERNAL`/`EXT` and `INTERNAL`/`INT` are attributes. EXTERNAL implies
STATIC.

## Not done

- External procedures compiled apart and bound by the linkage editor
  (calls through V-constants, EXTERNAL storage as common areas): only a
  source that holds all of them is supported.
- External names of more than eight characters.
- ENTRY statements (secondary entry points) in an external procedure:
  refused, as for the main procedure.
