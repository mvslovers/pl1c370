# TODO — PL1C/370

What is next, in order. `internals/ROADMAP.md` records what is done;
this file says what comes and where work stopped. Update it after
every merge.

Issue and PR numbers: this repository started with a fresh history on
2026-10-18. The open issues moved here as #1 to #11 (#1 was #179, #2 to
#7 were #227 to #232, #8 #236, #9 #255, #10 #268, #11 #275); every other
number in notes written before that date (#177, PR #254, ...) is one of
the predecessor repository, mvslovers/pl1c370-archive (private).

The language target (CLAUDE.md "Language target"): PL/I for MVS & VM by
default, without what exists only in Language Environment and without
DBCS; `-lang=F` is strict PL/I (F). `docs/language-levels.md` marks
every feature F, OPT, MVS or ISO.

## The plan by block (GitHub milestones, 2026-10-15)

Worked through in this order, a tier per issue; A to C without asking,
D only after a design note and the maintainer's decision.

- **A -- PL/I (F) complete** (milestone 1)
  - [x] #215 ENVIRONMENT's record format (Tier 95, PR #259; LIBC/370
        2.14.0 required, ABI PL1RT002)
  - [x] #216 small gaps: INDEX of bit strings, UNSPEC of an expression,
        REDUCIBLE/IRREDUCIBLE, `-lang=F` refusing `GETP -> X` (Tier 92,
        PR #244)
  - [x] #217 EXCLUSIVE, UNLOCK, NOLOCK (Tier 93, PR #252)
  - [x] #218 DISPLAY ... REPLY (Tier 94, PR #254; EVENT waits for #3)
  - [x] #219 sterling data (Tier 96)
- **B -- Run time and size** (milestone 2)
  - [x] #177 start-up and size (`internals/research/pli/startup-measurement.md`):
        measured, the stack set by the compiler (Tier 97, median VIRT
        452K to 236K). What is left is LIBC/370's start-up, which links
        all of stdio (72 KB for an empty `main`): with the maintainer
  - [x] #220 the LINKed case (Tier 98): IHESARC, a second LINK, no
        SYSPRINT DD; LIBC/370 2.15.0 (streams opened on first use)
  - [x] #266 dummy arguments: (X), an internal procedure's parameter
        attributes (Tier 98, reported from hellocp)
  - [x] #267 factored names in a structure (Tier 99)
  - [x] internal functions' RETURNS, `F()`, packed STATIC INITIAL
        (Tier 100, reported from hellocp)
  - [x] #273 structure arguments to external procedures, structure
        parameter descriptors (Tier 101, reported from hellocp)
  - [ ] #11 the run time without stdio for stream output: on hold
        until LIBC/370's own size fixes are measured (its
        internals/size-analysis.md, F1 to F5)
  - [x] #221 `make test`: the host tests build and pass again
  - [ ] #9 STORAGE: LIBC/370's malloc writes "Out of memory" and a
        traceback to the console (t209ans); PL/I (F) does not
  - [x] #284 a CHAR(*) parameter of a procedure reached only through an
        ENTRY variable or parameter: at least 256 (STAR_EXPR) when the
        procedure is used as a value (PR #290; closed in the archive
        2026-10-17). Left: an argument longer than that through the
        variable still overflows (temporaries sized at run time)
- **C -- the rest of the Optimizer's language** (milestone 3)
  - [x] #222 INITIAL CALL (Tier 102)
  - [x] #223 CURRENTSTORAGE, CONNECTED (Tier 103; STORAGE too)
  - [x] #224 preprocessor: COMPILETIME, COUNTER, PARMSET, RESCAN (Tier 104)
  - [x] #225 PLIDUMP (Tier 105, PR #281)
  - [x] #226 ATTENTION, and INTERRUPT (Tier 106); a real attention
        at a terminal measured with `mbt test --tso` (`test/tso/attn.lua`)
- **D -- large projects, design first** (milestone 4)
  - [x] #2 COMPLEX: complete (Tiers 109, 114 to 117, 119 binary
        constants, 120 PICTURE COMPLEX). Decided on the way (each in
        its PR): the format kinds stay where they are, K_C added, the
        ABI stamp kept (Q-8); Q-6 settled by the goal (2026-10-19); a
        complex constant into a real target gives its real part only in
        a source with COMPLEX data, its imaginary part checked as in
        PL/I (F); ABS of COMPLEX FIXED truncated exactly, MULTIPLY of
        COMPLEX FIXED BINARY right where PL/I (F) is wrong; ROUND's
        precision in the default PL/I for MVS & VM's (Tier 118)
  - [ ] #3 multitasking (TASK, EVENT, WAIT, PRIORITY): tiers 0 and 1
        done (Tiers 110, 111); tier 2 on needs the maintainer's A, B or
        C
  - [ ] #4 VSAM: Q1 settled (maintainer, 2026-10-19) -- LIBC/370's
        `vs*` API is extended for it, mvslovers/libc370#623, #624,
        #625; the tiers wait for those (the other open questions of
        `internals/design/vsam.md` come with the tiers; fixture catalogs
        in a throwaway container)
  - [ ] #5 COBOL and FORTRAN interlanguage calls
  - [ ] #6 FETCH of PL/I procedures, FETCHABLE: waits for the
        maintainer's choice between F-A (an anchor block; each fetched
        module carries its own run time, about 100 KB) and F-B (a run-time
        vector; the FETCHing program links all of pl1rt, about 76 KB),
        `internals/design/fetch.md` section 7. BYVALUE of external
        procedures (tier F1, convention B1) done in Tier 123
  - [ ] #7 IRXEXCOM for the book's FCONCAT (to be talked over with
        the maintainer first)
  - [x] #233 PLISRTx (Tier 107) and PLICKPT, PLIREST, PLICANC (Tier
        108); a checkpoint taken and a restart are not testable on
        m38b (shared disks)
  - [ ] #1 PL/I (F)'s library (SYS1.PL1LIB) as the run time under
        `-lang=F`

## The compiler's refusals (2026-10-19)

The "not supported" messages of the compiler, worked off where no
decision is needed (`internals/research/pli/mvsvm-inventory.md`,
section 3a, has the list).

- [x] Tier 124: RETURN out of BEGIN, PUT LINE and SKIP(n), array
      expressions in PUT LIST, bit || char, BIT of FLOAT, MOD of FLOAT,
      FLOAT subscripts, STATIC picture INITIAL, label INITIAL, LIKE with
      INITIAL, long INITIAL strings, bit strings as numbers; INITIAL on
      BASED, BASED in data lists, listing control statements, %NOTE
      (OPT). Small decisions taken: BIT of a value needing more than 31
      bits stays refused (BIT(FLOAT DEC(16)) would need 54); a bit
      string as the base of `**` is FIXED BIN(15), matching PL/I (F)
      for n = 2 and not for n = 3 (documented deviation)
- [x] Tier 125: a picture as DO control variable, SUBSTR of a character
      picture as a target, GET DATA of a scalar numeric picture
      (JOB00303); SUBSTR of a numeric picture stays refused, as in
      PL/I (F) (IEM1622I, JOB00354)
- [x] Tier 126: GET DATA of arrays of more than one dimension, of
      arrays of structures and of arrays of pictures; a scalar DEFINED
      on a subscripted base (OPT)
- [x] Tier 127: a cross section as the base of DEFINED, INITIAL CALL
      for factored names (OPT)
- [ ] keyed record I/O of a VARYING string, PUT DATA beyond 3
      dimensions, DEFINED with packed bit strings
- [ ] 12 KB of code per procedure (three base registers): a large main
      procedure must be split today; lifting it is a design question
      (base reloads per 4 KB section, or a fourth base register)

## PL/I in MBT (no milestone)

Agreed with the MBT session: the run time stays a library, linked by
autocall; one pin brings compiler and run time. MBT's side is mbt#270.

- [x] #237 `rt/pl1abi.h` and the `V(PL1RT001)` version stamp (on
      main; suite 244 of 244, JOB01815 to JOB01829)
- [x] #239 release workflow (PR #247): a tag gives the host compiler
      (Linux static amd64/arm64, macOS arm64/amd64) and
      `pl1c370-<version>-lib.tar.gz` (`lib/pl1rt.a`, `include/pl1abi.h`)
- [x] v0.1.0 released 2026-10-15 (PR #250, tag v0.1.0); main is
      0.2.0-dev. MBT pins it for mbt#270
- [x] #238 `-MD`/`-MF` dependency file for `%INCLUDE` (on main)
- [x] #256 `make install` into the cc370 sysroot (PR #258; MBT's design
      change for mbt#270, 2026-10-16: compiler and run time in the
      sysroot, `[toolchain] pl1c370`, no `[dependencies]`); MBT runs it
- [x] 0.2.0 released 2026-10-16 in the archive (PR #261, tag v0.2.0):
      PL1RT002, LIBC/370 >= 2.14.0
- [x] 0.3.0 released 2026-10-18 (PR #14, tag v0.3.0), this
      repository's first release: the mathematical functions in C
      (Tier 112), ABI PL1RT002; installed into the sysroot (~/.local)
      from the release assets; main is 0.4.0-dev

## To talk over with the maintainer (2026-10-18)

0. ~~**Going public**~~ done 2026-10-18: no IBM code and no notes on
   IBM's modules in the tree (Tier 112); this repository is public with
   a fresh history, the old one is mvslovers/pl1c370-archive (private).
   #12, the size of the mathematical functions: first step done
   (Tier 113, SQRT 7,128 to 4,668 bytes). Left: a short SQRT without
   the extended format (checked over every short argument), MUL128
   without `@@muldi3`, fewer copies of the 16-byte values.

Block D stops here until this is decided:

1. ~~**#2 COMPLEX, before tier C2**~~ (`internals/design/complex.md`):
   settled by the goal of 2026-10-18 (PL/I for MVS & VM first).
   - Q-4: COMPLEX FIXED is in scope beyond constants: SC26-3114-01
     defines ABS of a complex fixed-point value and its precision
     (MIN(N,p+1),q), so `*`, `/`, ABS and powers in fixed point come
     with C2.
   - Q-3: the arithmetic in C, as `rt/pl1m*.c`; no IBM code.
2. **#3 multitasking, before tier 2** (`internals/design/multitasking.md`):
   the end state A (real MVS subtasks, a run time per TCB), B
   (cooperative tasks on one TCB, the note's recommendation, with C as
   its first step) or C (each task run to completion). PL/I (F)'s
   tasking modules are in SYS1.PL1LIB; whether they run under MVS 3.8j
   is not yet tried.

After those come #4's fixture catalog (which volume on m38b), #6's
F-A or F-B and #5's scope.

Also open, not waiting on the maintainer: #177, the run time's size.
Re-measured against LIBC/370 2.17.0 (2026-10-17): no gain, the libc
share stays at 77 KB in every test module (76.8 KB on 2.16.0), because
the run time always reaches the stream layer and printf (SYSPRINT by
fopen, messages by snprintf/fprintf). The lever is on our side, #11
(output without stdio).

## Now: m38b after the soak

The soak ran 17 times (2026-10-15 16:00 to 21:01, stopped early on the
maintainer's word): 13 runs 244 of 244, the rest lost to program fetch
on m38b (106-F twice, IEB139I, a PL/I (F) phase that ran wrong, an mvsMF
S0C4 at module start), not to
PL1C/370. Results on m38b carry that caveat until the I/O side is
understood (httpd session).

Block C is done. Block D goes in the order of `internals/design/README.md`,
which the maintainer agreed to (2026-10-17): #233 (done), #2 (C1
done), #3 tiers 0 and 1, #4, #6, #5, #7. The open questions of each note are
taken with the note's recommendation where they are small, and each
tier's PR says which it took. The large ones go to the maintainer when
their tier comes: #2's Q-4 (COMPLEX FIXED beyond constants, before
C2), #3's end state (A, B or C), #4's fixture catalog on m38b,
#5's scope, #6's F-A or F-B. The suite runs against LIBC/370
2.19.0 in the sysroot since 2026-10-18 (285 programs: 275 identical,
the 10 mathematical twins CLOSE, JOB02686-02712).

## Parked (no issue)

- #10 calling C functions from PL/I (asked from hellocp): deferred by
  the maintainer (2026-10-16) until everything else is done -- no
  extras before that.

- Teleprocessing (TRANSIENT, PENDING): of little use on MVS 3.8j.
- ANSI: FORMAT variables; an option for where PL/I (F) and ANSI conflict
  (COPY's character copy, for one).
- A common run-time core in libc370 for all our compilers
  (`internals/research/le-light-study.md`): a research result, no plan
  (maintainer, 2026-10-15); section 6 says where to resume.
- BREXX/370's RXCOMM for today's calling form lives in `~/repos/mvs/foo`
  (`ihesap/`); RXPI, RXCUT and FCONCAT are tests t226 to t228.
