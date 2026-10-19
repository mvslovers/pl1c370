# CLAUDE.md — PL1C/370 (pl1c370)

Extends the root `~/repos/mvs/CLAUDE.md`; never contradicts it.

## What this is

PL1C/370 is a host cross-compiler for PL/I targeting MVS 3.8j: PL/I source →
S/370 assembler → as370 → ld370, with its own runtime on LIBC/370. The pattern
is cobc370 (`~/repos/mvs/cobc370`, a COBOL-74 compiler written in C), whose
`docs/COBOL74-ROADMAP.md` and `docs/DIFFERENTIAL-TESTING.md` are the templates
for how this project defines a target level and tests against it.

**Status: Tier 91 done** (2026-10-15), the roadmap as planned complete, the backlog in progress: see `internals/ROADMAP.md`.

**The NOT sign in test sources is `¬` (UTF-8).** `submit.py` sends it as
Latin-1 X'AC', the only form mvsMF delivers to PL/I (F) as NOT; `^` and `~`
arrive as invalid characters (JOB00070).

Issue and PR numbers: this repository started with a fresh history on
2026-10-18. The open issues moved here as #1 to #11 (#1 was #179, #2 to
#7 were #227 to #232, #8 #236, #9 #255, #10 #268, #11 #275); every other
number in notes written before that date (#177, PR #254, ...) is one of
the predecessor repository, mvslovers/pl1c370-archive (private).

## Layout and commands

- `src/` the compiler (host C99, `make` -> `build/host/pl1c370`): lex, pp
  (the preprocessor, on the source stream), parse,
  resolve (names across blocks), lower (aggregates to loops), check (PL/I
  (F) type rules), gen
- `rt/pl1abi.h` what the compiler and the run time must agree on (frame
  offsets, PL1NOCV bits, the procedure descriptor's flags, file
  attributes, the conditions), included by both sides; change
  `PL1_ABI_STAMP` with every change of a layout there: the compiler
  references it in every external procedure, the run time defines it,
  so a mismatched pair fails at the link
- `rt/` the run time, C on LIBC/370, linked into every program (mbt
  `[internal]`, by autocall: a module takes only the members it
  references, so keep rarely used parts in files of their own):
  `pl1rt.c` conversions and strings (`pl1rtfl.c` its FLOAT and PICTURE
  part), `pl1io.c` files, PUT LIST and record I/O (`pl1ioed.c` EDIT,
  PICTURE and FLOAT output, `pl1ioget.c` GET, `pl1iokey.c` REGIONAL and
  INDEXED, reached through the descriptor hook `V(PL1KHOK)` the compiler
  emits for such files; `pl1iof.h` what they share), `pl1cond.c`
  conditions and on-units, `pl1flt.c` PL/I (F)'s
  floating-point conversions (compiled into the compiler too, for
  constants), `pl1m*.c` the mathematical functions, a member per
  family and `pl1mxf.c` what they share (`pl1mth.h`): computed in an
  extended format with a 64-bit fraction and rounded once, at least as
  accurate as PL/I (F)'s, the last digit may differ (Tier 14's note),
  `pl1cx.c` COMPLEX values as characters, `pl1cxm.c`, `pl1cxd.c`,
  `pl1cxa.c`, `pl1cxp.c` the COMPLEX FLOAT `*`, `/`, ABS and `z**n`
  (`pl1cx.h`): plain short or long arithmetic in PL/I (F)'s order,
  which gives its bits and its conditions; `pl1cxf.c` COMPLEX FIXED
  `*`, `/` and ABS, exact in a wide integer,
  `pl1pic.c` PICTURE editing, sterling pictures included (compiled
  into the compiler too, for a picture's attributes), `pl1qsam.asm` QSAM in update mode for UPDATE
  files (LIBC/370 has no record update), `pl1date.c` and `pl1time.asm`
  DATE and TIME, `pl1ansi.c` ANSI's string functions, COLLATE and VALID, `pl1pie.asm` the program
  interruption exit (SPIE: OVERFLOW,
  ZERODIVIDE from interrupts), `pl1dsp.c` the DISPLAY statement (WTO, WTOR), `pl1cons.c` the
  messages on the console when SYSPRINT has no DD, `pl1area.c` allocation in AREA
  variables (the layout of GY28-6801-6), `pl1bdam.asm` BSAM load
  mode and BDAM for REGIONAL(1) files, `pl1bdk.asm` BDAM with keys
  and extended search for REGIONAL(2) and (3), `pl1isam.asm` QISAM and BISAM
  for INDEXED files; the
  external names are fixed in `rt/pl1rt.h` and `rt/pl1flt.h`
- `tests/pli/` programs compared with PL/I (F), `<name>.in` their SYSIN,
  `<name>.sep` marks a source whose external procedures the runner
  compiles apart, one per `*PROCESS`, and links together,
  `<name>.parm` PL/I (F)'s compiler PARM for the test (`CHAR48` in it
  gives PL1C/370 `-char48`), `<name>.goparm` the program's own PARM for
  both runs, `<name>.asmsub` the assembler routines (tests/mvs/asmsub) a program
  calls, linked into the PL1C/370 module and found by PL/I (F)'s
  link-edit in the development library, `<name>.cc` the condition code PL1C/370 must end with where PL/I (F)
  cannot give it (PLIRETC), `<name>.abend` a program that ends in an abend (a job of its own, the
  PL1C/370 step with COND=EVEN), `<name>.sem` marks a `<name>.opt.pli` whose source is PL/I (F)'s and
  only means something else in the default language (the runner then
  does not require `-lang=F` to refuse it), `<name>.xdrv` makes the
  program an external REXX function
  that XDRIVER calls with the arguments the file holds (PL/I (F) links
  it with `ENTRY IHESAPB`, PL1C/370 compiles it with `-plist`),
  `<name>.math` a test of the mathematical built-in functions, whose
  E-form numbers may differ from PL/I (F)'s in the last digits (at most
  100 units; `tests/compare.py --math`, the runner prints CLOSE),
  `<name>.reply` the operator's replies to its DISPLAY ... REPLY, one a
  line (a job of its own; the runner answers at m38b's console only the
  WTORs whose text begins with the test's name, and compares the
  console messages of both steps from the job log),
  `<name>.nosysp` runs both without a SYSPRINT DD (console messages and
  condition codes compared), a `<name>.xdrv` beginning `2*` has XDRIVER
  LINK twice,
  `<name>.dd` DD statements of their own (a REGIONAL data set; the
  runner adds them to both steps, `submit.py` to a probe's GO step),
  `<name>.opt.pli` a program beyond PL/I (F) that PL1C/370 compiles in
  place of `<name>.pli`, which spells out its GC33-0009-4 expansion for
  PL/I (F),
  `inc/<member>.pli` the %INCLUDE members (`pl1c370 -I tests/pli/inc`;
  the runner builds PL/I (F)'s SYSLIB from them with IEBUPDTE);
  `tests/run_mvs.py` compiles, builds, deploys and compares them in jobs of
  at most 50 programs (a job takes at most 255 steps) and 150 KB of JCL
  (mvsMF before 1.2.0 dropped the connection from about 70 KB on, mvsmf#412;
  1.2.0 took 150 KB, JOB00694)
  (SYSPRINT, and DD REPORT when written: as text and in its bytes; a
  program must write SYSPRINT, an empty print file counts as a failure);
  `make test` is the host test; `tests/host/depfile.sh` checks `-MD`/`-MF`
  (a make dependency file of the source and the %INCLUDE members read,
  for mbt)
- `tests/mvs/seam/seam0.asm` the hand-written seam test (module P0HELLO)
- `tests/mvs/xdriver/xdriver.asm` XDRIVER, which calls a program as TSO/E
  REXX calls an external function (R1 -> EFPL) and prints RC, EVLEN
  and the result
- `tests/submit.py` submits jobs to MVSTK5-BLD through mvsMF (the
  runner and probes use it), `tests/compare.py` compares two print
  files

**The run time needs libc370 2.15.0 or later** in the cc370 sysroot:
2.12.0 for `__fx37` (the code of a full data set's abend; older leaves
it unresolved at the link), 2.13.0 for DCB attributes from the fopen
mode (ENVIRONMENT, libc370#554), 2.14.0 for `fread()` returning a
record's length (libc370#559), 2.15.0 for the standard streams opened
on first use (SYSPRINT not emptied by a program that does not write to
it, #220). On an older one a record read gets nothing wrong at the link
and everything wrong at run time.

**mbt's default target is `dev1` = MVSCE-DEV, which is never written to.**
Always deploy with `MBT_TARGET=m38b` (MVSTK5-BLD); `tests/run_mvs.py` sets
it. The development library is `PL1C370.DEV.LINKLIB` on TSO001;
`tests/run_mvs.py` deletes it and lets `mbt deploy` allocate it anew on
every run. Not `--reallocate`: it grows the request each time (128, then
32768 tracks: IEF257I, and the library was already deleted).

A procedure may have 12 KB of code and literals (base registers R12, R11,
R10; expressions use R2-R9); the compiler refuses a larger one.

Test programs keep the main procedure name to seven characters: PL/I (F)
truncates a longer one with IEM2867I, an error (condition code 8).

Differential tests compare SYSPRINT data sets read as text (the ASA byte is
kept that way; mvsMF drops it from spool files) and the condition codes of
both steps. The code offset in a system-action message (`AT OFFSET +nnnnn`)
is masked: it belongs to each compiler. Each PL1LFCLG step needs a load
library and an object data set of its own (`submit.py` does both).

A probe whose on-unit does GO TO a label from which the condition is
raised again loops until the print data set is full (SB37): three did,
JOB00138, JOB00139, JOB00141. Count the entries into an on-unit and leave
after the first.

## The oracle: PL/I (F) V5.5 on TK5

IBM OS/360 PL/I (F), Version 5.5, is installed on the TK5 stands. Measured
read-only on MVSTK5-BLD, 2026-10-11:

- compiler phases `IEMAA`..`IEMYY`, 256 members, in `SYS2.LINKLIB`
- runtime library `SYS1.PL1LIB` (RECFM=U, 520 members, all `IHE*`)
- procedures in `SYS2.PROCLIB`: `PL1LFCLG`, `PL1LFC`, `PL1DFC`, … — all `PGM=IEMAA`
- the listing header in phase `IEMAB` reads "VERSION 5.5 ... OS/360 PL/I COMPILER (F)"

The string `360SAS037` in those modules is the IDR record of Assembler (F),
not a PL/I component number. The 256 phases on MVSTK5-REF are byte-identical
to BLD (2026-10-12). V5.5 behaves as the Version 5 manuals describe
(`internals/research/pli/v55-measurement.md`): GC28-8201-4 and GC28-6594-7 are
the specification.

**Probes** go through `tests/submit.py` (mvsMF, MSGCLASS=H, saves
every spool file). Probe programs and their output stay local, in
`probes/` (gitignored); a measurement is recorded in the notes with its
job numbers. PL/I (F) does not take a function's RETURNS or parameter
attributes from an internal PROCEDURE at the call site: declare
`ENTRY(...) RETURNS(...)` in the caller, or the probe fails for reasons of its
own (IEM0567I, FIXEDOVERFLOW).

**Where jobs may run** is decided by `~/repos/mvs/mvs38src/tools/systems.json`,
not here. MVSTK5-BLD takes work. MVSTK5-REF is cc370's pinned oracle: do not
send PL/I probes there. MVSCE-DEV is never submitted to. Submitting is
confirm-first under the root rules.

## Language target

**The target is the language of IBM PL/I for MVS & VM** (maintainer,
2026-10-15), and it is the default. It is the last of the line PL/I
(F), OS PL/I Optimizing Compiler, OS PL/I Version 2, PL/I for MVS & VM.
PL/I (F) V5.5 is the base it is measured on, because PL/I (F) is the
compiler we have.

- **The reference is its manuals:** SC26-3114-01 (Language Reference,
  Release 1.1, 1995) and SC26-3113-01 (Programming Guide). There is no
  such compiler to run, so these texts decide where it goes beyond PL/I
  (F) or differs from it. No OS PL/I Version 2 language manual is
  online (`internals/manuals/README.md`).
- **Excluded from the default:**
  - what exists only in Language Environment, which MVS 3.8j does not
    have: the CEE callable services, LE's message file (MSGFILE, the
    IBMnnnnx messages), CEEUOPT, LE's condition tokens and its ILC
    with other LE languages;
  - DBCS: GRAPHIC, MIXED and G strings.
  - **Not excluded:** what the Optimizing Compiler had without LE, though
    PL/I for MVS & VM hands it to LE -- the run-time options before `/`
    in the PARM, PLIXOPT, ISASIZE, STAE/SPIE, REPORT, FETCH
    (SC33-0006-7, "Execution-Time Options"; `internals/research/
    le-light-study.md`).
- **`-lang=F`** is the strict mode, for purists:
  - exactly what PL/I (F) V5.5 accepts;
  - the compile stops at the first thing beyond it, as PL/I (F) would
    refuse it.
  - Everything measured on the oracle must pass under `-lang=F`.
- **There is no ANSI level.** The ANSI X3.53-1976 (ISO 6160) features
  built so far stay in the default, documented as ISO extensions in
  `docs/language-levels.md`.
- **`-langlvl=SPROG`** (or LANGLVL(SPROG) in `%PROCESS`/`*PROCESS`) is
  PL/I for MVS & VM's option for pointer arithmetic and POINTERADD,
  POINTERVALUE, BINARYVALUE; the default is NOSPROG, as IBM's.
- **`-plist`** (R1 to the main procedure unchanged) is the `-lang=F`
  form of PL/I (F)'s `ENTRY IHESAPB`/`IHESAPD`. In the default,
  `OPTIONS(MAIN NOEXECOPS)` says it in the source (SC26-3114-01, pp. 103,
  122).

Exclusions and tiers: `internals/ROADMAP.md`; the plan: `TODO.md`.

**Language levels:** every feature beyond PL/I (F) is marked with its
level, for the manual and for `-lang=F` (`enum level`, `src/pl1c.h`):
- OPT, already the Optimizer's (checked against GC33-0009-4);
- MVS, PL/I for MVS & VM's and not the Optimizer's (SC26-3114-01);
- ISO, an ANSI extension neither IBM compiler has.

The marks go into `docs/language-levels.md`, and a built-in function's
level into its entry in `src/resolve.c`. Every deviation goes into
`docs/language-levels.md` as well.

**A new feature beyond PL/I (F) calls `beyond_f(line, level, what)`**
where the compiler recognizes it; that is the whole of `-lang=F`. Add a
test `<name>.opt.pli` for it: the runner compiles every other test with
`-lang=F` and checks that `-lang=F` refuses each `.opt.pli`.

**Compatibility order:**
1. PL/I for MVS & VM (its manuals) in the default.
2. The Optimizing Compiler (GC33-0009-4, SC33-0006) where the PL/I for
   MVS & VM manuals leave a question open.
3. PL/I (F) (the oracle) for everything they share with it, and in
   `-lang=F`.
4. ANSI X3.53-1976 only for the ISO extensions.

**The goal, in this order** (maintainer, 2026-10-18): first compatible
with PL/I for MVS & VM Release 1.1; only then the others, the
Optimizer, then PL/I (F), then ISO. So what exists only in PL/I (F)
(teleprocessing) or only in ANSI (FORMAT variables) waits until the
MVS & VM gaps are closed.

**Working mode** (maintainer, 2026-10-18): everything that needs no
important decision of the maintainer is worked off without asking;
small decisions are taken here, and each is written into the tier's PR
and TODO.md, so that it can be vetoed. In throwaway containers for TK5
and MVS/CE of this session's own (`mvs-docker`, `mvsce-target`) the
work on MVS needs no confirmation; the real stands (m38b and the
others) keep the root CLAUDE.md's rules, and slots there go through
MBT.

The maintainer's copy of the standard is licensed to one user: cite its
sections, never commit it.

**The manuals are in `internals/manuals/`,** local only: the PDFs are in
`.gitignore`, the README lists each with its source URL. Put a new one
there and add its line.
GC28-8201-2 (October 1969) has an OCR text that can be searched (link in
`internals/research/pli/manuals_inventory.md`); check a finding there
against GC28-8201-4.

## Research

`internals/research/pli/report.md` (timeline, standards, manuals, licensing,
prior art) and `internals/research/pls/report.md` (PL/S, the MVS 3.8j corpus).
Each sits beside the notes it cites. Both are in German.

Licensing in one line: PL/I (F) was a bundled OS/360 component and is treated
as freely usable; every later IBM PL/I is a licensed program product. Manuals
are on bitsavers; compilers are not to be obtained. Do not add IBM manuals or
other scans to this repository — link them.

**No IBM code in this repository** (maintainer, 2026-10-18): no code taken
from IBM's modules, in any language, and no notes on the inside of IBM's
modules. The run time is written from the manuals and from measured behaviour
(the twins, the probes); where PL/I (F) is the less accurate, the run time
may be more accurate and differ in the last digit (`rt/pl1m*.c`).

## Measurement rule for the MVS source

In the maintainer's shell `grep` is a function that silently skips files
containing X'AC' (the PL/S `¬`). It undercounted the PL/S corpus by up to a
factor of five. Count over raw bytes (Python) or with `LC_ALL=C /usr/bin/grep`,
and check a count against a module counted by hand.
