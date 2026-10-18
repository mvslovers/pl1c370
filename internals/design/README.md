# Block D: design notes (#2 to #233)

Block D (milestone 4) holds the large pieces. Each one gets a design
note first and is built only after the maintainer decides (TODO.md).
The notes were written on 2026-10-17. Their file:line references were
read from `main` between a88c600 and 11ece42, so a few may have drifted
since. None of the probes they list has been run. Claims a note could
not check are marked unverified in it.

| Issue | Note | Oracle | Proposed first step | Rough size |
|---|---|---|---|---|
| #2 COMPLEX | [complex.md](complex.md) | PL/I (F) | C1: data, constants, + - =, PUT LIST | 5 tiers. Run time about 9 KB if all of it is linked, under 2 KB for a typical FLOAT program |
| #3 Multitasking | [multitasking.md](multitasking.md) | PL/I (F), if SYS1.PL1LIB has its tasking modules (probe P0) | Tier 0: EVENT, WAIT, COMPLETION, STATUS, DELAY, with no tasks | 5 tiers. A design choice is needed before tier 2 |
| #4 VSAM | [vsam.md](vsam.md) | none (OPT); the twins spell out the output, and IDCAMS PRINT checks the result | V0: a fixture catalog and runner support | 7 tiers. New back end, ABI stamp to PL1RT003 |
| #5 COBOL/FORTRAN calls | [ilc.md](ilc.md) | none, except IKFCBL00 for COBOL layouts | a COBOL call with scalar arguments | Depends on the scope: FORTRAN is not on any stand |
| #6 FETCH of PL/I, BYVALUE | [fetch.md](fetch.md) | none (OPT) | the run-time sharing decision (F-A or F-B) | Architectural. Touches every run-time global |
| #7 IRXEXCOM | [irxexcom.md](irxexcom.md) | TSO/E REXX is absent | to be talked over: no REXX on MVS 3.8j offers IRXEXCOM today | Small on our side, blocked on rexx370 |
| #233 PLISRTx, checkpoint | [sort-checkpoint.md](sort-checkpoint.md) | PL/I (F) (IHESRTx, IHECKPx) | Q1: which SORT is on TK5, then S1 (PLISRTA) | 4 tiers. Exits are the first foreign callers of a PL/I procedure |

## The decisions each note asks for, in short

- **COMPLEX:**
  - new type kinds or a mode flag (new kinds recommended);
  - a split pass that breaks complex values into their parts;
  - whether COMPLEX FIXED is in scope beyond constants;
  - the arithmetic in C (settled 2026-10-18: no IBM code).
- **Multitasking:**
  - the end state: A, real MVS subtasks with a run time per TCB; B,
    cooperative tasks on one TCB (recommended, with C as its first
    step); or C, each task run to completion;
  - whether tiers 0 and 1 (events without tasks) go ahead before the
    decision;
  - a wait limit for tasking test jobs.
- **VSAM:**
  - our own assembler glue, or LIBC/370's `vs*` API extended;
  - a fixture user catalog on m38b: on which volume, and who sets it up
    after a DASD checkpoint (cobc370 broke a catalog three times with
    condition code 0);
  - BKWD, alternate indexes and SAMEKEY in scope or not.
- **COBOL/FORTRAN:**
  - the level (OPT; PL/I for MVS & VM dropped both) and whether FORTRAN
    is in scope at all;
  - cobc370, IKFCBL00, or both;
  - who owns the SPIE (cobc370 replaces PL/I's and never restores it).
- **FETCH:**
  - F-A, an anchor block (the Optimizer's model, which doubles as the
    groundwork for multitasking A), or F-B, a run-time vector;
  - the `atexit` hazard, which exists for any separately linked PL/I
    code today.
- **IRXEXCOM:** whether rexx370 is the host, and when its WP-42 and
  WP-60 come; a stub on our side now or later; whether "OPTIONS implies
  ENTRY" follows the Optimizer.
- **Sort and checkpoint:**
  - PL/I (F)'s names accepted in both modes, as IHESARC is;
  - the protocol for a GO TO or the program's end inside an exit;
  - measuring PL/I (F)'s sort and checkpoint interface first;
  - restart tests that need operator replies.

## A proposed order

The order follows oracle first, then risk.

1. **#233 sort and checkpoint.** PL/I (F) is the oracle and the scope is
   small. The exit protocol is new, but it is contained.
2. **#2 COMPLEX.** PL/I (F) is the oracle. It is large but
   self-contained, and a program without COMPLEX links nothing new.
3. **#3 tiers 0 and 1.** Events without tasks. Then the A/B/C
   decision.
4. **#4 VSAM.** Once a fixture catalog exists.
5. **#6 FETCH.** Its anchor-block option lays groundwork that #3 A
   would reuse.
6. **#5 COBOL/FORTRAN calls.** No oracle; FORTRAN is missing from the
   stands.
7. **#7 IRXEXCOM.** Waits for rexx370.
