# Tier 82: external REXX functions

A PL/I main procedure called as an external REXX function: R1 points at
the EFPL (TSO/E's form, BREXX/370 since V2R5M2), the procedure takes its
six words as parameters, and it answers in the evaluation block. TODO.md
item 1; the measurements behind it are JOB01232/1233
(`main-parameter-measurement.md`) and JOB01267/1269 (IHESAPB, IHESAPD).

Measured 2026-10-15 on MVSTK5-BLD: the three tests identical in
JOB01277, and after a test ending with 2000 in JOB01291 (the XDRIVER
step's COND names its own PL1L and LKED steps).

## The forms

- **PL/I (F)** has one form: a main procedure linked with
  `ENTRY IHESAPB` (OPT=0) or `ENTRY IHESAPD` (OPT=1/2) gets R1
  unchanged (JOB01267).
- **PL1C/370 `-plist`** is that form. The program's parameters are R1's
  first n words, the end mark (the high bit) dropped.
- **`OPTIONS(MAIN NOEXECOPS)`** says the same in the source, when the
  procedure has more than one parameter or one that is not CHARACTER
  VARYING. With one CHARACTER VARYING parameter it keeps the PARM. Its
  level is open: TODO.md item 0 checks it against GC33-0009-4.
- The procedure options may be separated by blanks as well as commas
  (`OPTIONS(MAIN NOEXECOPS)`).

## In PL1C/370

- **rt/pl1parm.c:** `__premain` keeps R1 at the program's entry (libc370
  hands it over as `pgmr1`); `pl1_main_plist(n)` (`PL1MPLS`) builds the
  main procedure's argument list from its first n words.
- **gen.c:** the main procedure's prologue calls PL1MPLS with -plist or
  NOEXECOPS, PL1MPRM (the PARM) otherwise.
- **parse.c:** NOEXECOPS; a main procedure takes more than one
  parameter only with one of the two.

## The test driver

`tests/mvs/xdriver/xdriver.asm`, module XDRIVER in the development
library:

- `PARM='MODULE,ARG1,ARG2,...'`: it builds the argument table (address
  and length per argument, X'FF' at the end) and an evaluation block
  with 1,024 bytes of data (EVSIZE 130), then `LINK EPLOC` with R0 = 0
  and R1 -> EFPL with the high bit, as TSO/E REXX does.
- After the call it opens SYSPRINT with EXTEND and writes
  `RC=nnnnn  EVLEN=nnnnn` and the result, 100 characters a line.
- A test `<name>.xdrv` holds the arguments (empty: none). The runner
  then compiles with `-plist` and runs XDRIVER for the program. For
  PL/I (F) it links with `ENTRY IHESAPB` (`IHESAPD` for OPT=1/2 in the
  `.parm`), skips the GO step and runs XDRIVER with `PARM='GO,...'`. The
  condition code compared is XDRIVER's step.

## The tests

| Test | Source | Call | Result |
|---|---|---|---|
| t226fct | "Practical Usage of MVS REXX" 14.14.4, FCONCAT, the function part | `FCONCAT('1','234')` | `1234`, EVLEN 5 (the book's off-by-one) |
| t227cut | brexx370 RXCUT, RXCOMM for R1 -> EFPL (`inc/rxcomm.pli`) | `RXCUT('ABCDEFGHIJ','2')` | `BDFHJ` |
| t228pi | brexx370 RXPI | `RXPI()` | 500 digits of pi, EVLEN 501 |

t226fct is an `.opt.pli` test. Its PL/I (F) twin differs from the book
in three places, each refused by PL/I (F):
- `OPTIONS(MAIN,NOEXECOPS)`: NOEXECOPS is not PL/I (F)'s;
- `BASED(ARGSTRING_PTR(I))`, a subscripted pointer as the base:
  IEM0603I. The twin assigns it to a pointer variable first;
- `UNALIGNED` on a structure with POINTER members: IEM3181I, an error.
  The structure is not used.

PL1C/370 takes all three; `-lang=F` will have to refuse the last two as
well (TODO.md item 0).

## RXPI's run time

JOB01277, the steps' CPU time (XDRIVER and the step around it included):

| | CPU | VIRT |
|---|---|---|
| PL/I (F), OPT=2, ENTRY IHESAPD | 0.34 s | 56K |
| PL1C/370 | 0.44 s | 420K |

The old figure, 0.49 s (brexx370 `v2.5-jcc`, `doc/external_function.md`),
is from another machine and includes the REXX interpreter. PL1C/370's
420K is mostly its stack, 256 KB by default (libc370 `@@CRT0`, set
through `__stklen`): a function called by REXX takes it in the caller's
region at every call.

## Not done

- **The LINKed case beyond one call:** R15 as the return code
  (XDRIVER reports it, the tests return 0), a second LINK in the same
  address space.
- **The PARM split at `/`** without NOEXECOPS (the Optimizer's
  run-time options): TODO.md item 0.
- **IRXEXCOM** (the book's second half, setting a REXX variable): needs
  FETCH or a call to an assembler entry with PLIRETV.
