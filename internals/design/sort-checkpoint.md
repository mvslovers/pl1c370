# Design note: PLISRTx and checkpoint/restart (issue #233)

Status: built. The sort is Tier 107 (which
also answers Q1 to Q6). Checkpoint and restart are Tier 108
(Q7 and Q8): no checkpoint can be taken on
MVSTK5-BLD, whose disks are shared, so C2's restart tests stay
undone. Question 6 is answered that way: no restart tests. Question
7: PLICKPT in a sort exit is not refused; CHKPT decides. Of the open questions in section 7, Tier 107 took: 1 PL/I (F)'s
names in both languages; 2 the abandon protocol; 3 the interface taken
from measurement; 4 LINK's S806 where there is no sort; 5 the SPIE as
PL/I (F) has it (Q4: PL/I's SPIE is in force in an exit); 8 no stamp, the new entry
PL1SORT fails the link of an old run time; 9 the sort's own checkpoint
passes through. Below, the draft as it was written.

Original status: draft for the maintainer's decision (block D). Nothing
here is built or measured by this note. File:line references were read from
pl1c370 `main` at 11ece42 on 2026-10-17; the tree was changing during
the write. "Unverified" marks every statement that
no manual read for this note and no measurement supports.

Short form: both are thin interfaces to system programs, and PL/I (F)
has both (IHESRTx, IHECKPx, IHERESx), so PL/I (F) is the oracle. The
checkpoint part is small: CHKPT (SVC 63) and an ABEND. The sort part is
small too, except for one thing that is new to PL1C/370: the sort calls
PL/I procedures back (exits E15 and E35) while it is LINKed. Today
nothing foreign enters a PL/I procedure except the system's start of the
main procedure and the SPIE exit. A GO TO out of an exit, or STOP or
ERROR inside one, must not leave the sort's request block behind; PL/I
(F)'s library has a mechanism for exactly that (GY28-6801-6 pp. 15-17),
and PL1C/370 needs its own.

## 1. What the language requires

Sources (pdftotext output of the PDFs in `internals/manuals/`):

- SC33-0006-7 (Optimizer Programmer's Guide, Sep 1985): chapter 12 "The
  Sort Program", pp. 315-337; chapter 13 "Checkpoint/Restart",
  pp. 339-342.
- GC33-0009-4 (Optimizer Language Reference): built-in subroutines,
  p. 112 -- PLICKPT, PLIREST, PLICANC, PLISRTA to PLISRTD are built-in
  subroutines, "not reserved words", invoked by CALL.
- SC26-3114 (PL/I for MVS & VM Language Reference, pdftotext output of
  -0): PLICANC, PLICKPT, PLIREST, PLISRTA..D as built-in subroutines,
  pp. 366-369 (details deferred to the Programming Guide).
- GC28-6594-7 (PL/I (F) Programmer's Guide, Jan 1971), chapter 14 "Other
  Facilities of the Operating System": checkpoint/restart pp. 178-183,
  sort interface pp. 184-197.
- GY28-6801-6 (PL/I (F) Subroutine Library Program Logic, 1972),
  chapter 2: "Sort/Merge PL/I Interface" and "Checkpoint/Restart",
  pp. 15-17.

### 1.1 PLISRTA to PLISRTD (Optimizer)

    CALL PLISRTA (sort, record, storage, retcode [, prefix, msglevel, technique]);
    CALL PLISRTB (sort, record, storage, retcode, e15 [, prefix, msglevel, technique]);
    CALL PLISRTC (sort, record, storage, retcode, e35 [, ...]);
    CALL PLISRTD (sort, record, storage, retcode, e15, e35 [, ...]);

(SC33-0006-7 Fig. 141, pp. 326-327.)

- **sort**: a CHARACTER expression, `' SORT FIELDS=(start,length,form,seq,...)[,options] '`,
  blanks before and after required, one between SORT and FIELDS, no
  others (Fig. 142, pp. 329-330). Forms CH, ZD, PD, FI, BI, FL; A or D;
  options FILSZ, SKIPREC, CKPT, EQUALS/NOEQUALS, DYNALLOC -- depending
  on the sort program.
- **record**: `' RECORD TYPE=F|V|D[,LENGTH=(l1,,,l4,l5)] '`; V lengths
  include the 4-byte prefix (Fig. 143, pp. 331-332).
- **storage**: FIXED BINARY, main storage for the sort.
- **retcode**: FIXED BINARY(31) variable: 0 successful, 16 failed, 20
  message data set missing (p. 320).
- **prefix**: 4 characters replacing SORT in SORTIN, SORTOUT, SORTWKnn,
  SORTCNTL, SORTCKPT; a null string when a later argument is given.
  **msglevel**: NO, AP, CP. **technique**: PEER, BALN, CRCX, OSCL, POLY
  (p. 327).
- **E15** (input exit, PLISRTB and D): a PL/I procedure, internal or
  external, with RETURNS(CHARACTER(n) [VARYING]); it returns the record
  and calls PLIRETC(12) to include it, PLIRETC(8) when all records have
  been passed (a record returned with 8 is ignored), PLIRETC(16) to stop
  the sort (retcode 16). A VARYING result with TYPE=F is padded with
  blanks (pp. 321-322, Fig. 138).
- **E35** (output exit, PLISRTC and D): a procedure with one CHARACTER
  parameter, CHAR(n) for TYPE=F, CHAR(*) for V; it calls PLIRETC(4) for
  the next record, PLIRETC(16) to stop. "There is no indication from
  Sort to the output handling routine that the last record has been
  reached" (pp. 322-323, Fig. 140).
- The exits "are called from the sort program itself and will
  effectively become part of it" (p. 315). AUTOMATIC variables do not
  keep their values between calls (p. 321).
- PLIRETC's value is also the program's return code; the manual advises
  `CALL PLIRETC(RETCODE)` after the sort (p. 321, 324).
- Sort is LINKed; only SORT and RECORD statements are passed; more via
  SORTCNTL (p. 315). Data sets: SORTLIB (tape work files only, for
  DFSORT), SYSOUT (messages), SORTWK01-16, SORTIN, SORTOUT, SORTCKPT,
  SORTCNTL; "must not be open when Sort is called" (pp. 324-325).
- The sort programs named: "OS/VS Sort/Merge, its follow on DFSORT, or a
  program with the same interface", program product 5740-SM1 (p. 315).

### 1.2 IHESRTA to IHESRTD (PL/I (F))

The same four entry points, as external entries the program declares,
e.g. `DCL IHESRTA ENTRY(CHAR(35),CHAR(27),FIXED BIN(31,0),FIXED BIN(31,0))`
(GC28-6594-7 p. 189). Differences from the Optimizer:

- The return code to the sort is set by `CALL IHESARC(n)` (or IHETSAC
  with TASK) (p. 191). PL1C/370 already maps IHESARC to PLIRETC.
- E35 codes: 4 accepted, next please; 8 "stop passing records, even if
  there are still more to come"; with no code "the sort program
  continues to pass records until all have been passed" (p. 193).
- Only the prefix as optional fifth argument (a string of any length,
  first 4 characters used, null for the standard names); "PL/I (F) does
  not permit a variable number of arguments", so every CALL in one
  compilation must pass it (p. 186).
- Sort program: "IBM System/360 Operating System Sort/Merge" (GC28-6543,
  bibliography, GC28-6594-7 front matter; GY28-6801-6 p. 15). SORTLIB
  (`DSNAME=SYS1.SORTLIB`) and SYSOUT are "always required" (p. 186).
  Minimum main storage 16,000 bytes (PCP, MFT), 26,000 (MVT) (p. 185).
  Record length 18 to about 32,000 bytes; up to 64 control fields
  (p. 184).
- Each exit call "involves a substantial time overhead necessary to
  establish a PL/I environment" (p. 184).
- The E15 example ends with `CALL IHESARC(8); GOTO ENDE15; ... ENDE15:
  END E15A;` -- the function reaches its END without RETURN (p. 192).

**How IHESRT does it** (GY28-6801-6 pp. 15-17): IHESRT builds the sort's
parameter list from the arguments and LINKs to Sort/Merge. It allocates
a DSA of its own so that the save-area chain from an exit goes through
DSAs only. Before the LINK it (1) saves the registers in the external
save area of the PL/I procedure and replaces them with registers that
lead back into IHESRT, used when an exit procedure ends by an error
before the sort has ended, or when a GO TO leaves an exit for a
procedure at or below the caller; the sort is then completed, "any user
exit calls to the now non-existent PL/I exit procedures are deleted",
and control goes to the program; (2) sets the PICA to system action; (3)
gives R13 a save area with a zero back chain. When an exit is taken,
"the PL/I environment is reestablished", R13 is the IHESRT DSA, the exit
procedure is called; before returning to the sort the PICA and R13 are
reset to their values at entry to the exit.

### 1.3 PLICKPT, PLIREST, PLICANC (Optimizer)

SC33-0006-7 pp. 339-342:

    CALL PLICKPT [(ddname [, check-id [, org [, code]]])];
    CALL PLIREST;
    CALL PLICANC;

- **ddname**: CHARACTER, default SYSCHK. **check-id**: CHARACTER; omitted,
  the system makes one and prints it on the console. **org**: CHAR(2),
  PS or PO, default PS. **code**: FIXED BINARY(31): 0 checkpoint taken,
  4 restart made, 8 not taken (check the statement), 12 not taken
  (missing DD, I/O error, no space; also with DISPLAY REPLY incomplete or
  multitasking), 16 taken but ENQs outstanding. An omitted argument
  before a given one is a null string.
- Uses the Advanced Checkpoint/Restart Facility; not under TSO; RD on
  JOB/EXEC decides restart activity.
- The data set: PS or PO; NEW/OLD keeps only the last checkpoint, MOD
  keeps them all; for PO the check-id is the member; a record is "about
  5000 bytes longer than the area of main storage allocated to the step"
  (p. 340).
- **PLIREST**: requests an automatic restart; "the compiler terminates
  the program abnormally, with a system completion code of 4092"; 4092
  must be an eligible code (p. 341). PL/I (F) says "user completion code
  of 4092" (GC28-6594-7 p. 182).
- **PLICANC**: cancels automatic restart from checkpoints taken; RD=R or
  RNC can still restart the step (p. 342).
- Deferred restart: a new job with `RESTART=(stepname[,check-id])` and a
  SYSCHK DD before the first EXEC (p. 341).

### 1.4 IHECKPS, IHECKPT, IHEREST, IHERESN (PL/I (F))

GC28-6594-7 pp. 178-183: `CALL IHECKPS(ddname, checkid, org, code)` --
the same four arguments, declared by the program as `ENTRY(CHAR(8)VAR,
CHAR(16)VAR, CHAR(2), FIXED BIN(31))`; leading arguments must be present
(null string for the default) if a later one is; check-id up to 16
characters (PS) or 8 (PO, alphanumeric, alphabetic first); code: 0, 4,
8, 12, 16 (Fig. 14-1). `CALL IHECKPT;` the old form: SYSCHK only, no
check-id, no code. `CALL IHEREST;` ABEND with user code 4092.
`CALL IHERESN;` the CANCEL option of CHKPT. IHECKP "invokes the standard
operating system CHKPT macro" with a DCB it builds from a dummy (GY28-6801-6
p. 16); the program must not open or declare the checkpoint data set
itself. Restrictions: no checkpoint in a tasking program or between
DISPLAY REPLY EVENT and its WAIT; open data sets are repositioned, their
contents not restored (p. 183).

### 1.5 Levels

PLISRTA..D, PLICKPT, PLIREST, PLICANC: OPT (GC33-0009-4 p. 112).
`internals/research/pli/standards_language_levels.md:147` records that
the Optimizer does not recognise the PL/I (F) names (IHESRTx, IHECKPx,
IHERESx). PL1C/370 already treats PL/I (F)'s IHESARC/IHETSAC as PLIRETC
(`src/resolve.c:305-315`); the same treatment for IHESRTx and IHECKPx in
both modes is proposed (section 3.1).

## 2. Which sort program, and what PL1C/370 has today

### 2.1 The sort on TK5

- The manuals: PL/I (F) calls "IBM System/360 Operating System
  Sort/Merge", GC28-6543 (GC28-6594-7 p. 184; GY28-6801-6 p. 15), with
  SORTLIB = SYS1.SORTLIB (GC28-6594-7 p. 186). The Optimizer names
  OS/VS Sort/Merge or DFSORT (5740-SM1) "or a program with the same
  interface" (SC33-0006-7 p. 315). GC28-6543 is not in
  `internals/manuals/`.
- Measured by the cobc370 session: "OS/360 Sort/Merge 1.05 on TK5",
  reached by `LINK EP=SORT` with the control statements in a parameter
  list and E15/E35 exits in the program
  (`~/repos/mvs/cobc370/docs/COBOL74-CONFORMANCE.md:337-356`;
  `src/cobc370.c:12945-12999` the list and the LINK; exits
  `src/cobc370.c:14865-14892`; the JCL adds `SORTLIB DD
  DSN=SYS1.SORTLIB` and three SORTWK, `bin/cobc-regress:146-152`). Its
  list: halfword length 24, A(SORT statement first byte), A(its last
  byte), A(RECORD first), A(RECORD last), A(E15), A(E35); R1 points at a word
  X'80',AL3(list). The sort's messages there carry the prefix IER
  (`IER059A` for a record too short for the work files,
  `docs/COBOL74-CONFORMANCE.md:357-359`). Which TK5 stand cobc370
  measured on is not stated there.
- **IERRCO00 is not confirmed by anything read for this note.** That the
  sort's entry is a member SORT (alias) in SYS1.LINKLIB, and what its
  primary name is, is probe Q1 below.
- **MVS/CE has no sort**: "no SYS1.SORTLIB, no SORT in SYS1.LINKLIB,
  SYS2.LINKLIB or MVSSRC.BLD.LOAD" (`~/repos/mvs/mvs38src/tools/lmdreport.py:9-14`).
  A PL1C/370 program calling PLISRTx there gets S806 at the LINK unless
  the run time looks first (open question 4). The oracle runs on TK5
  anyway (pl1c370's project notes, "The oracle").
- The cobc370 session also found that the 3.8 sort cannot be asked for a
  MERGE from a program (`docs/COBOL74-CONFORMANCE.md:362-366`). PL/I has
  no merge interface, so that does not matter here.

### 2.2 PL1C/370 today

- **PLIRETC** is a built-in subroutine (`src/resolve.c:243`); the run
  time keeps the code in `pl1_retcode` (`rt/pl1cond.c:79-84`, set at
  `:837-842`); the step ends with it plus 1000 or 2000
  (`rt/pl1cond.c:413-422`, `:846-851`, `:873-881`). `CALL IHESARC(n)`
  and IHETSAC become PLIRETC unless the program has a procedure of that
  name (`src/resolve.c:305-315`). So an exit's code is already where the
  run time can read it.
- **Built-in subroutines with optional arguments** exist: PLIDUMP, 0 to 2
  arguments, CALL only (`src/resolve.c:246`, `:357`, `:374`).
- **Calls out** to non-PL/I code: OPTIONS(ASSEMBLER) entries, R1 at a
  list of addresses, Tier 89 (`docs/language-levels.md`, attributes
  table); FETCH/RELEASE of such modules by LOAD/DELETE, Tier 91
  (`rt/pl1fetch.asm`).
- **Entries from foreign code** are only two:
  - the main procedure, started by the system through LIBC/370's start-up;
    `-plist` and NOEXECOPS pass R1 unchanged (`rt/pl1parm.c:31-55`,
    `src/main.c:145`);
  - the SPIE exit, which the supervisor enters with the program's
    registers: `rt/pl1pie.asm` takes a frame from the NAB at +76 of the
    current save area, chains it, and calls C (`rt/pl1pie.asm:80-103`).
  No PL/I procedure other than main is ever entered from foreign code
  today. FETCH of PL/I procedures (#6) would be the second such path.
- **Calling convention** (what an exit glue has to build): the argument
  list has two words for the run time -- the parent's frame (static
  link) and A(result) -- then one word per argument (`rt/pl1parm.c:73-75`,
  `src/gen.c:323-328`). A CHAR(*) parameter, and every string parameter
  of an external procedure, is passed by a descriptor `A(data), F(length)`
  (`src/gen.c:257-268`, `:320-328`). An entry value is A(procedure),
  A(parent frame) (`src/pl1c.h:93`). A frame begins with the save area,
  NAB at +76, and the run time's words from +88 (`rt/pl1abi.h:17-26`).
- **GO TO out of a block** calls PL1GOTO with the target frame (it
  unlinks on-units and active conditions above it), then loads R13 with
  the frame and branches (`src/gen.c:5581-5610`, `rt/pl1cond.c:140-161`).
  Nothing in it knows about a foreign program in between.
- **The end of the program** is `exit()` from `terminate`, `pl1i_quit`
  and `pl1_stop` (`rt/pl1cond.c:413-422`, `:846-851`, `:873-881`).
  LIBC/370 finds its run-time anchor by the TCB (`__crtget`: PSATOLD
  against the PPA's CRT list, libc370 `src/mvs/crt/@@crtget.c`), so a
  LINKed program on the same TCB does not hide it.
- **ESTAE**: the main procedure's prologue sets one; its exit writes
  IHE012I and percolates (`rt/pl1cond.c:234-296`). An abend in the sort
  or in an exit reaches it and goes on.
- **CHKPT** is in the maclib (`~/.local/cc370/maclib370/macros/CHKPT.macro`:
  `CHKPT dcb,id,lng` and `CHKPT CANCEL`, SVC 63).
- Not present: any of PLISRTx, PLICKPT, PLIREST, PLICANC, IHESRTx,
  IHECKPx, IHERESx (`grep -n PLISRT\|PLICKPT\|IHESRT\|IHECKP src rt`:
  no hits).

## 3. Proposed design

### 3.1 Compiler

- `src/resolve.c` builtins table: PLISRTA, PLISRTB, PLISRTC, PLISRTD,
  PLICKPT (0 to 4 args), PLIREST, PLICANC (no args), all L_OPT, CALL
  only, like PLIDUMP. `beyond_f` refuses them under `-lang=F`.
- In both modes, `CALL IHESRTA..D`, `IHECKPS`, `IHECKPT`, `IHEREST`,
  `IHERESN` with no procedure of that name in the program become the
  built-ins, exactly as IHESARC does (`src/resolve.c:305-315`); the
  program's own ENTRY declaration of them is accepted and its parameter
  descriptors are not used for the call. This is what lets the PL/I (F)
  twin and the `-lang=F` run be one source. (Open question 1: should
  the default language accept the F names at all?)
- `src/check.c`: argument types (CHARACTER for statements, prefix,
  ddname, check-id, org; FIXED BINARY variable for retcode and code;
  entry constants or variables for the exits, internal or external).
  E15 must have RETURNS(CHARACTER ...), E35 one CHARACTER parameter --
  checked when the procedure is known; an ENTRY variable is checked at
  run time only by what happens.
- `src/gen.c`: CALL PLISRTx builds an argument block (layout in
  `rt/pl1abi.h`) and calls `PL1SORT`:

      A(sort desc), A(record desc), F storage, A(retcode),
      for E15: A(entry), A(parent frame), F flags+length of its RETURNS
               (VARYING or not), 0 if none
      for E35: A(entry), A(parent frame), F flags (by descriptor or by
               address: CHAR(*) or external), F declared length, 0 if none
      A(prefix desc) A(msglevel desc) A(technique desc), 0 when omitted

  The descriptors are the string descriptors gen already builds for
  CHAR(*) arguments. The block is in the caller's frame (temporaries).
  CALL PLICKPT builds `A(ddname desc) A(checkid desc) A(org desc)
  A(code)` (0 for each omitted or null) and calls `PL1CKPT`; PLIREST
  `PL1REST`; PLICANC `PL1CANC`.
- `rt/pl1abi.h`: the two blocks' layouts; `PL1_ABI_STAMP` stays (new
  layouts for new entry points break no existing pair) unless the
  maintainer prefers a bump for every layout added (open question 8).

### 3.2 Run time: the sort

Members of their own (`rt/pl1sort.c`, `rt/pl1srt.asm`), linked only when
`PL1SORT` is referenced.

**PL1SORT (C)**: copy the SORT and RECORD statements into one area
(their addresses and last bytes go into the list, so a temporary must
live until the sort returns); parse LENGTH and TYPE from the RECORD
statement for the E15 buffer (the buffer is max(RETURNS length,
LENGTH l1) bytes plus 4 for V, allocated here and freed after the sort);
build the parameter list in the form the TK5 sort takes (cobc370's
24-byte form, plus whatever carries storage, prefix and message level
-- that part is not known: probe Q2 measures it); refuse a
nested PLISRT (ERROR, as there is one set of exit state); then call
`pl1_srtlink` (asm), store the sort's return code into retcode, and act
on a pending GO TO or end (below).

**pl1_srtlink (asm)**: saves R2-R13 and the NAB of its own frame in a
static area (the exits resume the PL/I stack from it), LINK EP=SORT with
R1 at the list, returns R15.

**The exits (asm, PL1SE15 and PL1SE35)**, entered by the sort with R13
at the sort's save area, R14 its return, R1 at the exit's list:

1. STM 14,12 into the sort's save area; remember the sort's R13.
2. Take a frame from the NAB saved at the LINK (the PL/I stack goes on
   above `pl1_srtlink`'s frame, as `rt/pl1pie.asm` takes one from the
   NAB). Chain its back pointer to `pl1_srtlink`'s frame, not to the
   sort's save area -- IHESRT's rule ("the chain-back is through the DSAs
   only"), and what PLIDUMP's trace and any back-chain walk need.
3. E35 only: the record address is word 0 of the sort's list; when it is
   0 (end of data) return 8 at once without calling PL/I: "there is no
   indication ... that the last record has been reached" (SC33-0006-7
   p. 323; cobc370 reads the same word, `src/cobc370.c:14883`).
4. Call the C dispatcher, which builds the PL/I argument list -- word 0
   the exit's parent frame from its entry value, word 1 A(result buffer)
   for E15, then for E35 A(record) or A(descriptor: data, length) by the
   flags; for V records the data is record+4 and the length the RDW less
   4 -- and calls the procedure.
5. R15 = `pl1_retcode`. E15 with 12: R1 = A(buffer); for TYPE=V an RDW
   (length + 4) in front of the data; a VARYING result for TYPE=F padded
   with blanks to l1 (p. 322).
6. Restore the sort's registers except R15 (and R1 for E15) and BR 14.

Whether `pl1_retcode` is reset to 0 before each exit call, and what the
sort does with a code it does not expect, is PL/I (F)'s to say (probe
Q3): PLIRETC is sticky in PL1C/370 today.

**SPIE**: IHESRT sets the PICA to system action before the LINK and
re-establishes the PL/I one inside each exit (GY28-6801-6 pp. 16-17).
PL1C/370's SPIE (`rt/pl1pie.asm`) catches only exponent overflow and
floating divide, which a sort without FL fields will not cause. Proposal:
leave the SPIE alone unless probe Q4 shows the sort issues one of its
own; then do as IHESRT does (PL1PIOF before the LINK, PL1PION and back at
each exit, two SVC 14 per exit call).

**Leaving an exit other than by returning.** A GO TO from an exit to a
block below the sort's caller, or STOP, EXIT, ERROR's system action or
PLIDUMP's S/E inside an exit, must not run on with the sort's PRB still
on the chain: the next `exit()` or SVC 3 would end the sort's RB and
resume after the LINK, and a GO TO would leave the sort's storage and RB
behind. Proposal, following IHESRT's "complete the sort, delete the exit
calls":

- `pl1_goto` (C) and the three end paths check a static "sort active,
  glue frame f0". If the target frame is below f0 (or the program is
  ending), they record the pending action -- for GO TO the 16 registers
  the GO TO code had at its call of PL1GOTO (they are in the caller's
  save area: copy them away), for an end the condition code -- and
  return into the exit glue's saved state (set at exit entry, as a
  setjmp) with "abandon".
- The exit glue then answers the sort: E15 8 (no more input) or 16
  (stop), E35 16 or 8 -- which of them, and what retcode the program
  then sees, is PL/I (F)'s to say (probe Q5). Every later exit call
  is answered at once without calling PL/I.
- When the LINK returns, `pl1_srtlink` sees the pending action: for a
  GO TO it runs `pl1_goto`'s cleanup for the target frame and resumes the
  saved registers at the instruction after the PL1GOTO call, so the
  generated code goes on to load R13 and branch (`src/gen.c:5591-5609`);
  for an end it calls `exit()` with the recorded code, now under the
  program's own RB.
- The GO TO code generation does not change. The cost is one compare
  in `pl1_goto` and in the end paths, and the static state in the sort
  member (referenced weakly, so programs without PLISRT do not link the
  sort member: how cc370 does weak externs, as libc370's `@@exit.c`
  does for fclose, is to be checked).

An abend in the sort or in an exit goes to the ESTAE (IHE012I) and on;
nothing special.

### 3.3 Run time: checkpoint and restart

`rt/pl1ckpt.asm` (and a few lines of C for the arguments), linked only
when referenced:

- **PL1CKPT**: a model DCB `DSORG=PS|PO, MACRF=(W)` copied to a work
  area below the line, DDNAME from the ddname argument (default SYSCHK,
  cut to 8); check-id cut to 16 (PS) or 8 (PO); `CHKPT dcb,id,len`
  (or `CHKPT dcb` for a system check-id); R15 to the code argument. The
  CHKPT opens and closes the DCB itself (GC28-6594-7 p. 180: the
  checkpoint "data set to be opened and closed at the appropriate
  times"). A restart resumes after the SVC 63 with R15 = 4 and all
  registers and storage restored, so the glue must not depend on
  anything C keeps outside its frame across the SVC; written in
  assembler for that reason. Argument errors (org not PS/PO, check-id
  not alphanumeric for PO): code 8 without a CHKPT, as PL/I (F) does
  -- to be measured (Q7).
- **PL1REST**: ABEND 4092 (user code, as GC28-6594-7 p. 182 says; the
  Optimizer's "system completion code" wording to be checked against
  PL/I (F)'s actual code, Q8). The ESTAE writes IHE012I first if PL/I
  (F)'s STAE does (Q8).
- **PL1CANC**: `CHKPT CANCEL`.
- Refuse PLICKPT inside a sort exit (code 12): the sort is another
  program on the RB chain (unverified whether CHKPT allows it).

## 4. Tiers, twins and probes

**Twins.** PL/I (F) is the oracle for everything here. Each test is
`<name>.opt.pli` with PLISRTx / PLICKPT / PLIREST / PLICANC and
`<name>.pli` with IHESRTx / IHESARC / IHECKPS / IHEREST / IHERESN, which
PL1C/370 also compiles with `-lang=F` (section 3.1), so both the
built-ins and the F names are tested against the same oracle run. The
sort's own messages go to SYSOUT, not SYSPRINT; they are not compared
(they carry times and counts), but the condition codes are.

**S0 -- probes (listed, not submitted).**
- Q1 read-only member lists (mvsMF, no job): SYS1.LINKLIB(SORT) and its
  alias/primary name (IERRCO00 or other); SYS1.SORTLIB's members;
  SYS1.PL1LIB(IHESRT), (IHECKP), (IHERES) present.
- Q2 PL/I (F)'s interface to the sort, measured: the parameter list
  the sort receives (storage, prefix, message level, technique), the
  exits (return-code reset, RDW for V, padding), what happens when
  control leaves an exit. Also checkpoint: the CHKPT form, the DCB,
  argument checks.
- Q3 PL/I (F) probes, one job each where an abend or a restart is
  possible:
  - IHESRTA, 80-byte F records, SORTIN and SORTOUT data sets, RC 0; with
    no SYSOUT DD (RC 20?); with no SORTIN (RC 16?); the program's step
    code after the sort.
  - IHESRTB, C, D with internal and external exits; E15 that calls
    IHESARC(12) only once and then returns without calling it; E35 that
    calls IHESARC(8) early; E35 without any IHESARC; no input at all
    (E15's first call says 8: is E35 called?); V records; a prefix
    argument ('TASK' with TASKIN, TASKOUT, TASKWKnn).
  - the step's condition code after an E35's last IHESARC(4).
- Q4 whether the sort issues SPIE (a PL/I (F) E35 that divides a
  floating value by zero with an ON ZERODIVIDE: does the on-unit run?).
- Q5 GO TO from E15 and from E35 to the caller; STOP in E35; SIGNAL
  ERROR in E15 without an on-unit: what SYSPRINT, retcode and step code
  show, and which code IHESRT answers the sort (the SYSOUT messages tell).
- Q6 a PL/I (F) function that reaches END without RETURN (the manual's
  E15 does): no condition? And PL1C/370 the same (it is today unchecked:
  no `grep` hit for such a rule in `src/`).
- Q7 IHECKPS with SYSCHK DD present (code 0), absent (12?), org 'XX'
  (8?), PO data set with a member check-id; IHECKPT; IHERESN.
- Q8 IHEREST without RD: the completion code (U4092 or S...), IHE012I
  or not; with `RD=R` on EXEC: the IEF225D "SHOULD ... RESTART" WTOR and
  the restart (needs an operator reply: the runner answers only WTORs
  that begin with the test's name, `tests/run_mvs.py` docstring); whether
  4092 is an eligible restart code on TK5.
- Q9 a checkpoint with SYSPRINT open (JES2 SYSOUT) and a restart: what
  happens to the lines printed between checkpoint and failure, under
  PL/I (F) (QSAM) and PL1C/370 (LIBC/370's BSAM, buffered in storage that
  the restart restores).

**S1 -- PLISRTA.** The built-in, the F mapping, the argument block, the
parameter list, LINK, retcode; no exits. Tests: sort to SORTOUT, read
back by the program through a second DD naming the same data set
(GC28-6594-7 p. 186: "there must not be any PL/I files with the same
names or titles" as the sort's data sets, so not FILE(SORTOUT)); two fields; descending; prefix; missing
SORTIN (retcode 16, step code).

**S2 -- exits.** PLISRTB, C, D: internal and external exits, an exit as
ENTRY variable, F and V records, VARYING E15 result with TYPE=F
(padding), CHAR(*) E35 parameter, early stop (8, 16), no input, PLIRETC
reset after the sort, an exit that does stream output to SYSPRINT and
record I/O (the Optimizer manual's examples print from E15 and E35).

**S3 -- leaving exits.** GO TO out of E15 and E35, STOP and SIGNAL ERROR
inside them, an on-unit established in the caller raised in an exit and
returning normally, an on-unit doing GO TO out. Only after Q5.

**C1 -- PLICKPT, PLICANC.** Codes 0, 8, 12; PO with member check-id;
PLICANC; IHECKPT. A test job with a SYSCHK DD in `<name>.dd`.

**C2 -- PLIREST and restart.** PLIREST without RD (`<name>.abend`,
completion code compared); automatic restart with RD=R (a job per
compiler, the runner answering IEF225D: new runner support); code 4
after the restart, and the program's output after it. Deferred restart
(a second job with RESTART=) as a probe only, unless the maintainer
wants the runner to do it.

## 5. Cost and memory

Code (estimates): compiler 200-300 lines (resolve, check, gen for two
argument blocks and the F-name mapping); `rt/pl1sort.c` 250-350 lines,
`rt/pl1srt.asm` 150-250, `rt/pl1ckpt.asm` 80-120; changes to `pl1_goto`
and three end paths, a few lines each; runner support for IEF225D
replies and a restart job (C2) 50-100 lines. Five tiers (S1-S3, C1-C2)
after the probes. S3 and C2 carry the risk; the rest is plain.

Memory:
- A program without PLISRT or PLICKPT: nothing. The members are linked
  only when referenced; the checks in `pl1_goto` and the end paths read
  a static that must not pull in the sort member (weak reference, or a
  word in a member every program links -- one word).
- With PLISRT: the members (estimated 2-3 KB); per sort: the parameter
  list (under 64 bytes), the statement copies (their lengths), one E15
  buffer of the record length + 4, freed when the sort returns; exit
  frames on the PL/I stack above the caller's, as for any call.
- The sort's own storage is the program's choice (the storage argument;
  the manuals' examples use 25,000 and 256,000 bytes), GETMAINed by the
  sort in the region, plus the sort's modules loaded from SORTLIB or
  LINKLIB (sizes unverified). PL/I (F)'s minimum figure is 26,000 bytes
  under MVT (GC28-6594-7 p. 185). The region must hold the program
  (median VIRT 236K after Tier 97, `internals/research/pli/startup-measurement.md:123`)
  plus that. Proposal: pass the storage argument through unchanged;
  never substitute a larger default. DFSORT's 'MAX' encoding
  (SC33-0006-7 p. 328) means nothing to the 3.8 sort (unverified) and is
  passed through as a number.
- With PLICKPT: about 0.5 KB code and an 88-byte DCB per call (on the
  stack or a static work area). The checkpoint record goes to DASD, "about
  5000 bytes longer" than the step's storage (SC33-0006-7 p. 340): with
  LIBC/370's 256 KB default stack in the region
  (`internals/research/le-light-study.md:254`), a few hundred KB per
  checkpoint. A smaller stack (Tier 97 sets it per program) makes the
  checkpoint smaller too.

## 6. Risks

- **The exits are PL1C/370's first foreign entry into PL/I procedures.**
  The stack switch, the back chain, PL1CURF and the active-condition
  list must all be right when a procedure runs on top of another
  program's RB. The SPIE exit (`rt/pl1pie.asm`) is the only working
  precedent, and it calls C, not PL/I.
- **Leaving an exit** (S3) touches `pl1_goto` and the program's end, the
  two most central paths of the run time. A mistake there breaks every
  program, not only sorting ones; the change must keep the no-sort path
  to one compare.
- **The parameter list**: only the 24-byte form is measured (cobc370).
  Storage, prefix and message level depend on measuring PL/I (F) (Q2)
  or on GC28-6543, which is not at hand.
- **MVS/CE has no sort**: tests run on TK5 only; a program moved to
  MVS/CE abends S806 unless the run time checks first.
- **Checkpoint/restart with LIBC/370**: whether 3.8j restores the
  program's ESTAE and SPIE at restart, repositions JES2 SYSOUT data sets
  and LIBC/370's BSAM DCBs, and whether the region comes back at the
  same addresses, is unverified (Q8, Q9). A restart that resumes with
  the ESTAE gone would end an abend without IHE012I.
- **Restart tests need the operator**: IEF225D must be answered, the job
  is re-queued, and m38b is a shared stand. U4092 must be an eligible
  code at sysgen.
- **PLIRETC is sticky**: the step's condition code after a sort is the
  last exit's code unless the program resets it -- the Optimizer manual
  says so; the twins will show whether PL1C/370 matches PL/I (F).

## 7. Open questions for the maintainer

1. Accept PL/I (F)'s names (IHESRTx, IHECKPS, IHECKPT, IHEREST, IHERESN)
   in both modes, as IHESARC is now, or only under `-lang=F`? The
   Optimizer did not recognise them.
2. The abandon protocol for GO TO and program end inside an exit
   (section 3.2): acceptable, given it touches `pl1_goto` and the end
   paths? The alternative is to refuse (ERROR with a message) a GO TO
   out of an exit, which PL/I (F) allows.
3. Measure PL/I (F)'s sort and checkpoint interface (Q2) before
   building?
4. A PLISRT call where no SORT module exists (MVS/CE): S806 as LINK gives
   it, or BLDL first and retcode 16 with a message?
5. SPIE around the sort (section 3.2): only if Q4 shows the sort sets
   one?
6. Restart tests (C2): build runner support for IEF225D and restart jobs
   on m38b, or keep automatic and deferred restart as documented probes
   only?
7. PLICKPT inside a sort exit: refuse with code 12, or let CHKPT decide?
8. ABI stamp: bump for the two new argument blocks, or leave PL1RT00n
   since no existing layout changes?
9. Sort checkpoints (the CKPT option and SORTCKPT DD) are the sort's
   business and pass through untouched: agreed?
