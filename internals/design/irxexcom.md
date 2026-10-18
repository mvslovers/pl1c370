# Design note: IRXEXCOM for the book's FCONCAT (#7)

Status: facts, options and questions only, for a talk with the
maintainer first (TODO.md, block D). 2026-10-17. Nothing built,
nothing submitted. Marks: **[read]** taken from the named file;
**[inf]** inferred; **[unverified]** needs a check.

## 1. What is required

The book "Practical Usage of MVS REXX", section 14.14.4, pp. 269-270,
gives FCONCAT, an external REXX function in PL/I. It does two things
(transcribed in `~/repos/mvs/foo/book/FCONCAT.pli`, with the book's
slips listed in `foo/book/README.md:15-26`) **[read]**:

1. It concatenates its arguments into the EVALBLOCK (lines 41-49). That
   part runs today as test `t226fct` (tests/pli/t226fct.opt.pli:3-6
   says the second half was left out).
2. **It sets the REXX variable CONCAT** through IRXEXCOM (lines 3,
   21-31, 51-59):
   - `DCL IRXEXCOM EXTERNAL OPTIONS(RETCODE,INTER,ASSEMBLER);` (note: no
     ENTRY keyword);
   - an UNALIGNED SHVBLOCK of 32 bytes: SHVNEXT, SHVUSER, SHVCODE,
     SHVRET, 2 filler, SHVBUFL, SHVNAMA, SHVNAML, SHVVALA, SHVVALL;
   - `SHVCODE='S'`, the name `CONCAT` (6 bytes), the value the
     EVALBLOCK data;
   - `FETCH IRXEXCOM;` then `CALL IRXEXCOM('IRXEXCOM',DUMMY,DUMMY,
     SHVBLOCK);`, four arguments, no ENVBLOCK;
   - `IF PLIRETV > 0 THEN PUT ...`.

**Without an ENVBLOCK**, the TSO/E services find the non-reentrant
environment themselves. foo/README.md:232-233 cites SA32-0972-40 near
p. 244 for this; that manual is not in this repository **[read there,
unverified here]**. A PL/I program cannot reach R0 at entry, so it
could not pass the ENVBLOCK anyway (foo/README.md:231, marked
inferred there).

## 2. What exists today

### 2.1 pl1c370

- **External REXX function**, done:
  - `-plist` and `OPTIONS(MAIN NOEXECOPS)` hand R1, the EFPL, to the
    main procedure (CLAUDE.md "Language target").
  - Tests t226-t228 and t242byv run under XDRIVER
    (tests/mvs/xdriver/xdriver.asm:1-15).
  - XDRIVER LINKs with **R0 = 0** (xdriver.asm:7), as BREXX/370 does.
- **The calling pieces FCONCAT needs exist**:
  - OPTIONS(ASSEMBLER) and RETCODE (Tier 89);
  - FETCH of an assembler entry (Tier 91, rt/pl1fetch.asm);
  - PLIRETV.
- **But the book's source does not compile.** The host compiler was
  run on FCONCAT.pli, shifted one column for the margins, as a trial
  compile, 2026-10-17 **[read]**:
  1. Line 56: "FETCH names an OPTIONS(ASSEMBLER) entry".
     - `DCL IRXEXCOM EXTERNAL OPTIONS(...)` without ENTRY is not taken
       as an entry (parse.c:1744; `entry || hasret`).
     - **The manuals disagree.** GC33-0009-4 p. 432: "The OPTIONS
       attribute implies the ENTRY attribute". SC26-3114-01 p. 142:
       "The use of the OPTIONS attribute does not imply ENTRY".
     - The book's form is the Optimizer's.
  2. With `ENTRY` added, line 57: "structure SHVBLOCK cannot be used
     here" (check.c:1762).
     - A structure argument to an ASSEMBLER entry is refused.
     - SC33-0006-7 ch. 11 says the list holds "the addresses of actual
       data items ... irrespective of the types of data"
       (tier89-measurement.md, "The rules").
     - So the structure should be passed as the address of its first
       byte. ilc.md needs the same fix for OPTIONS(COBOL).
  - `DUMMY` is undeclared in the book and gets implicit attributes.
    That is accepted.
- **tier82-measurement.md:95-96** already lists IRXEXCOM as "needs
  FETCH or a call to an assembler entry with PLIRETV". Both exist now.
  What is left is the two gaps above, plus something to call.

### 2.2 Which REXX on MVS 3.8j offers IRXEXCOM? None today.

| REXX | External functions (EFPL) | IRXEXCOM | Source |
|---|---|---|---|
| **TSO/E REXX** (IBM) | yes | yes | **Not on MVS 3.8j** **[inf]**: brexx370/README.md:55-60 says MVS 3.8j "pre-dates IBM's official release of TSO/CMS Rexx"; no file here records it on any stand |
| **BREXX/370** | yes: SVC 6 LINK, R1 = A(EFPL) with high bit, EVALBLOCK 4096 bytes (brexx370/src/external.c:10-75) | **removed** | `svcParams.R0 = 0; /* no ENVBLOCK: BREXX has no IRXEXCOM (#353) */` (external.c:53); TODO.md:389-392: "IRXEXCOM dropped (maintainer, 2026-10-05; #355 closes #151): with BREXX/370 and REXX/370 side by side ECTENVBK and the IRX names belong to REXX/370". BREXX/370 is in maintenance mode, no new features (root CLAUDE.md) |
| **rexx370** | **not yet**: a CALL tries an internal label, then a built-in, then "routine not found" (src/irx#bvm.c:2729-2749; **[not re-read]**); WP-60 "TSO/E external functions" OPEN (internals/workpackages.md:44) | **declared, not implemented** | prototype only (include/irxfunc.h:132-135; **[not re-read]**); the IRXEXTE slot `irxexcom` (include/irx.h:210) "stay[s] NULL" (src/irx#init.c:597-600); WP-42 "IRXEXCOM variable sharing" OPEN (workpackages.md:40); planned behaviour in one sentence (internals/architecture.md:354-356) |

- **SHVBLOCK sizes differ** **[read]**:
  - rexx370 `SHVBLEN 0x28 /* 40 bytes */` (include/irx.h:283). It adds
    `shvnamelen` and `shvvalelen` after SHVVALL **[not re-read]**.
  - BREXX/370's macro, the book's structure and FCONCAT.pli:21-31 all
    have 32 bytes (brexx370/maclib/SHVCB.mac:22).
  - A caller passing a 32-byte block to an IRXEXCOM that reads 40
    bytes is harmless if the two extra words are output only.
    **[unverified]**: rexx370 has no implementation to read.
- **How IRXEXCOM is found**:
  - FCONCAT FETCHes it by name, which is LOAD EP=IRXEXCOM from the
    module search.
  - BREXX/370's old macros did the same: `LOAD EP=IRXEXCOM`
    (brexx370/maclib/RXPGCALL.mac:14; **[not re-read]**).
  - rexx370 plans only the IRXEXTE vector entry, reached from the
    ENVBLOCK (include/irx.h:208-211). No load module named IRXEXCOM is
    planned in any file read.
- **httprexx** maps ENVBLOCK and IRXEXTE but does not call IRXEXCOM.
  Pool variables are "a follow-up" (httprexx/doc/rexx370-bindings.md:
  234-236; **[not re-read]**).

## 3. Options

1. **Wait for rexx370** (WP-42 IRXEXCOM, WP-60 external functions).
   pl1c370's part is then only the two compiler gaps in 2.1.
   - It works with the book's 4-argument call only if rexx370 offers
     an IRXEXCOM load module that finds the current environment by
     itself, as TSO/E does. Otherwise a PL/I caller cannot supply the
     ENVBLOCK (R0).
2. **BREXX/370**: closed. The maintainer dropped IRXEXCOM there on
   2026-10-05, and BREXX/370 takes no new features (root CLAUDE.md).
   Listed only so that it is not proposed again.
3. **A test stub for now.**
   - An assembler module IRXEXCOM in the development library, beside
     XDRIVER and ASMTST (mbt.toml modules).
   - It checks the id 'IRXEXCOM', prints SHVCODE, the name and the
     value from the SHVBLOCK, and returns 0 in R15.
   - FCONCAT then runs end to end as an XDRIVER test, with its twin.
   - It tests pl1c370's side (structure argument, FETCH, RETCODE,
     PLIRETV) without any REXX. It sets no variable anywhere.
4. **R0 at entry, kept for the program.**
   - Have the `-plist` / `NOEXECOPS` prologue save R0 (the ENVBLOCK,
     when the caller passes one) where the program can read it, for
     example a built-in or a parameter.
   - Only useful with a REXX that passes R0 and expects the ENVBLOCK.
     XDRIVER and BREXX/370 pass 0.
   - Beyond every IBM PL/I. A new level mark would be needed, and the
     maintainer has deferred extras (#10, TODO.md "Parked").
   - Not proposed unless the maintainer wants it.

## 4. Tiers, tests, probes (if the maintainer agrees)

- **Probe, host only**: compile FCONCAT.pli after each compiler step.
  Done once already; the two errors are recorded in 2.1.
- **X1, compiler only**:
  - pass a structure argument to an ASSEMBLER entry by the address of
    its first byte (check.c:1762);
  - decide the OPTIONS-implies-ENTRY question.
  - Test: extend t240asm with a structure argument. ASMTST would need a
    function that reads it. The twin prints the values (PL/I (F) passes
    structures through a dope vector, tier89-measurement.md).
- **X2, with option 3**:
  - the IRXEXCOM stub module;
  - `t2xxexc.opt.pli`, the book's FCONCAT with only its slips that
    would fail fixed (see foo/book/README.md);
  - `.xdrv` `1,234`. The twin is t226fct plus the stub's lines printed.
- **X3**: a real REXX, once rexx370 has WP-42 and WP-60. Not
  testable before that.

## 5. Cost and memory

- **X1**: small, tens of lines in check.c and gen.c. No run-time
  storage.
- **X2**: a stub of about 200 bytes in the development library; test
  only, never shipped.
- **At run time** a FETCHed IRXEXCOM takes one slot of the 16 in
  PL1FTCH's table (12 bytes) and a 32-byte SHVBLOCK in the program.
- Nothing else. The REXX side's cost is rexx370's.

## 6. Questions for the maintainer

1. **Target REXX**: is rexx370 the intended host for PL/I external
   functions that set variables? Is there a date or order for WP-42 and
   WP-60?
2. **How will rexx370 expose IRXEXCOM**:
   - a load module that FETCH and LOAD find by name (the book's way)?
   - or only the IRXEXTE vector entry, which needs the ENVBLOCK that a
     PL/I program cannot reach?
3. **Without an ENVBLOCK**, will rexx370's IRXEXCOM find the current
   environment itself (TSO/E's behaviour, foo/README.md:232-233)?
4. **SHVBLOCK** of 40 bytes in rexx370 against 32 in the book, in
   BREXX's macro and in IBM's layout: will rexx370 accept a 32-byte
   block?
5. **OPTIONS implies ENTRY**: follow GC33-0009-4 (the book's source
   compiles unchanged, level OPT) or SC26-3114-01 (the default's
   reference; the book needs `ENTRY`)? Under CLAUDE.md's compatibility
   order the MVS & VM manual decides in the default. Accepting the
   Optimizer form as well costs nothing visible.
6. **Option 3** (the stub): build it now, so that FCONCAT is covered
   end to end on pl1c370's side, or wait for rexx370?
7. **Option 4** (R0 kept): wanted at all?
