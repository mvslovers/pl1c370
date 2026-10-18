# Design note: COBOL and FORTRAN interlanguage calls (#230)

Status: draft for the maintainer, 2026-10-17. Nothing built, nothing
submitted. Marks: **[read]** taken from the named file or manual page;
**[inf]** inferred; **[unverified]** not checked, needs a probe.

Page numbers are the printed ones. GC33-0009-4 = OS PL/I Checkout and
Optimizing Compiler Language Reference (1976); SC33-0006-7 = OS PL/I
Optimizing Compiler Programmer's Guide (1985); SC26-3114-01 = PL/I for
MVS & VM Language Reference (1995).

## 1. What is required

### 1.1 The language

- **Calling COBOL or FORTRAN from PL/I** (GC33-0009-4 ch. 19,
  pp. 289-293; SC33-0006-7 ch. 14, pp. 343-353) **[read]**:
  - The entry is declared `OPTIONS(COBOL)` or `OPTIONS(FORTRAN)`, never
    both, on an entry constant, entry variable or parameter, possibly
    inside GENERIC (p. 292, points 1-4).
  - COBOL is invoked by CALL only. FORTRAN by CALL or as a function.
    RETURNS is not allowed with COBOL (p. 292, point 2).
  - The name is 1 to 8 characters; a longer one is cut to 8 (p. 292,
    point 5).
  - Arguments use "the addressing mechanism ... used by the invoked
    language" (p. 290): addresses of the data, as for
    OPTIONS(ASSEMBLER).
  - An aggregate whose PL/I mapping differs from the other language's
    is passed as a **dummy**, mapped by the other language's rules,
    assigned member by member before the call and back after it
    (p. 291). The other language's rules are: COBOL SYNCHRONIZED for a
    structure; FORTRAN column-major order for an array of more than
    one dimension, or a one-dimensional array that is not connected or
    not aligned the same.
  - `NOMAP`: no dummy. `NOMAPIN`: a dummy, not filled before the call.
    `NOMAPOUT`: a dummy, not copied back. `ARGn` limits each to listed
    arguments (pp. 291-292). Example: `OPTIONS(COBOL,NOMAP(ARG1,ARG3))`.
  - `INTER`: program interruptions that the COBOL or FORTRAN routine
    does not handle go to PL/I on-units and standard system action
    (p. 291, pp. 294-295).
  - A VARYING string is passed as the address of its 2-byte prefix;
    the prefix becomes the first two bytes of the COBOL item (p. 297).
  - A structure with an AREA or a bit string is not remapped, with a
    diagnostic (p. 297).
  - Compile-time diagnostics per attribute: Fig. 19.5, p. 300.
- **PL/I procedures called from COBOL or FORTRAN** (pp. 292-293)
  **[read]**:
  - `OPTIONS(COBOL)` or `OPTIONS(FORTRAN)` on the PROCEDURE or ENTRY
    statement, at most one of MAIN, COBOL, FORTRAN (p. 293, point 1).
  - String lengths, area sizes and array bounds of parameters must be
    decimal integer constants (point 2). Reason **[inf]**: the callee
    builds its own descriptors from them.
  - No RETURNS on an entry invoked by COBOL (point 3).
  - NOMAP, NOMAPIN, NOMAPOUT with a parameter-name list.
- **Common storage**: a STATIC EXTERNAL variable shares storage with a
  FORTRAN COMMON block of the same name (p. 293). No equivalent for
  COBOL.
- **Environment** (GC33-0009-4 p. 294; SC33-0006-7 pp. 356-357)
  **[read]**:
  - With a PL/I main procedure the PL/I environment exists from the
    start.
  - Otherwise it is set up when the first PL/I procedure is invoked
    and lasts until the routine that invoked it ends. Ending it
    releases CONTROLLED and BASED storage and closes all PL/I files.
  - "There must not be two concurrent PL/I environments" (SC33-0006-7
    p. 357).
  - FORTRAN's environment, with its message file FT06F001, is
    initialized at the first call and ends with the calling PL/I
    procedure.
- **GO TO** may cross at most one interlanguage boundary (p. 296).
- **Termination**:
  - FORTRAN STOP and COBOL STOP RUN end the whole program. Inside a
    PL/I environment that ending may leave PL/I files unclosed
    (p. 297).
  - A FORTRAN RETURN to outside the PL/I environment ends that
    environment and closes the files.
- **Multitasking**: no COBOL or FORTRAN routine as a task. Only one
  task at a time may have COBOL or FORTRAN active (p. 297).
- **Restrictions in SC33-0006-7 p. 343** **[read]**:
  - A FETCHed PL/I procedure may not call COBOL or FORTRAN.
  - COBOL dynamic CALL may not be used in an interlanguage environment.
  - COBOL or FORTRAN modules cannot be loaded by PL/I FETCH (p. 88).

### 1.2 The data equivalents

These tables are in GC33-0009-4, Fig. 19.2 and 19.4, pp. 298-299
**[read]**.

| COBOL | Length | PL/I |
|---|---|---|
| COMP, 1-4 digits | 2 | FIXED BINARY(15,0) |
| COMP, 5-9 digits | 4 | FIXED BINARY(31,0) |
| COMP, 10-18 digits | 8 | no equivalent |
| COMP-1 | 4 | FLOAT DECIMAL(6) |
| COMP-2 | 8 | FLOAT DECIMAL(16) |
| COMP-3 | CEIL((n+1)/2) | FIXED DECIMAL |
| DISPLAY | any | CHARACTER (numeric DISPLAY: PICTURE, Fig. 19.3) |

| FORTRAN | PL/I |
|---|---|
| INTEGER*2, INTEGER*4 | FIXED BINARY(15,0), (31,0) |
| REAL*4, *8, *16 | FLOAT DECIMAL(6), (16), (33) |
| COMPLEX*8, *16, *32 | COMPLEX FLOAT DECIMAL(6), (16), (33) |
| LOGICAL*1, *4 | BIT(8), BIT(32) |

- **Alignment.** PL/I ALIGNED corresponds to COBOL SYNCHRONIZED and to
  FORTRAN's normal alignment. UNALIGNED corresponds to COBOL
  unsynchronized; FORTRAN has no equivalent (p. 291).
- **FORTRAN COMMON** is always byte-aligned (Fig. 19.4, note 1).
- **COMPLEX** depends on #227. FLOAT DECIMAL(33), the extended
  precision, is not supported by PL/I (F).

### 1.3 Language level: a question, not decided here

- **SC26-3114-01 p. xvii [read]**: "PL/I for MVS & VM does not support
  ILC with FORTRAN or OS/VS COBOL".
  - Its `OPTIONS(COBOL)` on PROCEDURE (p. 102) and ENTRY (p. 130), and
    NOMAP/NOMAPIN/NOMAPOUT (pp. 104, 131-132), are LE ILC with COBOL
    for MVS & VM. Both pages defer to the LE Programming Guide.
  - `INTER` "is syntax checked and ignored" (p. 132).
- **The pre-LE form is therefore the Optimizer's**: OPTIONS(COBOL),
  OPTIONS(FORTRAN), NOMAP*, INTER (GC33-0009-4 ch. 19). That would make
  it level **OPT**.
- `OPTIONS(FORTRAN)` and a working INTER exist only at OPT level.
- **Under CLAUDE.md "Language target"**, LE's ILC with other LE
  languages is excluded, while what the Optimizer had without LE is
  included. That points to OPT.
- **The maintainer decides** whether FORTRAN ILC belongs in the default
  at all. The current PL/I for MVS & VM manual dropped it.

## 2. What exists today

### 2.1 pl1c370

- **OPTIONS(ASSEMBLER) is the nearest precedent** (Tier 89-91), and
  covers most of PL/I calling COBOL **[read]**:
  - parse.c:1318-1340: an ENTRY's OPTIONS takes ASSEMBLER, RETCODE,
    BYVALUE, INTER, BYADDR. Any other option, COBOL and FORTRAN
    included, is `fatal` (parse.c:1333-1335).
  - check.c:377-428: CALL only; arguments by address; a dummy of the
    descriptor's type where the types differ; OPTIONAL; BYVALUE.
  - gen.c:3284-3361, `gen_asmcall`:
    - builds the list of data addresses;
    - sets the high-order bit on the last word (`OI X'80'`);
    - `L 15,=V(name)`, or the entry point from PL1FTCH for a fetched
      module;
    - with RETCODE, stores R15 into PL1RTCD.
  - `stk_unknown = 1` (gen.c:3299) for the stack estimate.
- **Gap, found by compiling** (2026-10-17, host compiler, a trial
  compile): a structure argument to an ASSEMBLER entry is
  refused with "structure SHVBLOCK cannot be used here" (check.c:1762).
  OPTIONS(COBOL) needs exactly this case.
- **The PL/I-to-PL/I call is not an OS list**:
  - gen_fcall (gen.c:3366-3530): word 0 is the parent frame, word 1 the
    result area, the arguments from word 2. No high-order bit.
  - External string parameters go by descriptor, `A(data)` plus length
    (gen.c:258-268 `ext_string_par`, gen.c:3511-3522).
  - `*` arrays go by descriptor (gen.c:3470-3493).
  - So a PL/I procedure entered by COBOL needs a prologue that turns an
    OS list into this canonical list. The ENTRY-statement prologue
    already rebuilds a canonical list from R1 (gen.c:6207-6230), and is
    the place to do it.
- **Frames.** Every procedure takes its frame from the NAB:
  `PDPPRLG ... L 15,76(,13)` (cc370 `macros/pdpprlg.macro:49-51`)
  **[read]**. Entered from a COBOL save area, which has no word at +76,
  the frame address is garbage.
- **Start-up and recovery live in the main procedure's prologue only**
  (gen.c:6233-6240, `PL1PION` at 6236, `PL1STON` at 6238):
  - `PL1PION`, the SPIE for codes 12 and 15 (rt/pl1pie.asm:1-6);
  - `PL1STON`, the ESTAE (rt/pl1cond.c:229-353).
  - Neither runs when no PL/I MAIN is in the program.
- **STATIC EXTERNAL**:
  - Emitted as a CSECT named after the variable. An arithmetic scalar
    without INITIAL becomes `COM` (gen.c:6032-6050).
  - That is the shape a FORTRAN COMMON block needs **[inf]**.
  - One snag: a structure's data starts at its *phase* inside the
    CSECT (gen.c:6041-6046). COMMON starts at offset 0.
- **ENVIRONMENT(COBOL)** is accepted and has no effect
  (docs/language-levels.md:310). The manual says it is unrelated to ILC
  (GC33-0009-4 p. 290). Do not confuse the two.
- **Conditions and GO TO**:
  - `pl1_goto` unwinds by comparing frame addresses on the stack
    (le-light-study.md 2.1, 3.4).
  - COBOL's static save areas are not on that stack.

### 2.2 cobc370 (`~/repos/mvs/cobc370`, present)

COBOL-74 compiler, one source file src/cobc370.c. It has its own
848-byte run time COBRT and no LIBC/370 (README.md:1-8, 144). It has
no CLAUDE.md.

- **Entry** is standard OS linkage (cobc370.c:11898-11915) **[read]**:
  - `ST 13,SAVEAREA+4`, `LA 0,SAVEAREA`, `ST 0,8(13)`, `LR 13,0`.
  - It writes the caller's +8.
  - PDPPRLG also writes +8 of the caller (pdpprlg.macro:51). Whether
    pl1rt reads +8 anywhere is **[unverified]**: no hit for `+ 8)` in
    pl1cond.c or pl1dump.c, but this was not checked exhaustively.
- **Save area.** One static `SAVEAREA DS 18F` (cobc370.c:15622) and
  nothing at +76 **[read]**. Not reentrant.
- **SPIE** (cobc370.c:11962-11982) **[read]**:
  - `SPIE COBSPIE,((1,7),9,(11,12),15)`, issued once per load module
    (SPIEDONE).
  - Never cancelled. The comment says so: "after a callee has run, its
    exit stays armed".
  - It **replaces PL1PIE's SPIE** (codes 12 and 15). After one COBOL
    call, a PL/I OVERFLOW or floating ZERODIVIDE becomes `ABEND
    U3012`/`U3015` instead of the condition **[inf]**: one SPIE per
    task, the newest wins.
  - `-s` compiles without it.
- **Program mask.** It clears it on every entry (`SPM`,
  cobc370.c:11953-11959) and never restores it. pl1pie.asm says the
  program mask is 0 in PL/I anyway (rt/pl1pie.asm:5-6). The mathematical
  routines (rt/pl1mxf.c) set it themselves with SPM. Probably harmless **[unverified]**.
- **Calls out**: static CALL by V-con with the high bit on the last
  word. Dynamic CALL through COBDCAL (LOAD, 16 entries). R15 goes into
  RETURN-CODE as a halfword (cobc370.c lines
  14031-14072, 10424-10466, 10645-10654) **[not re-read]**.
- **Data** **[not re-read]**, except SYNC below:
  - COMP is a halfword for 1-4 digits and a fullword for 5-9; more
    than 9 digits is refused.
  - COMP-3 is digits/2+1 bytes.
  - DISPLAY numeric is zoned with a trailing overpunch sign.
  - 01 items are doubleword-aligned, with no padding.
- **SYNC** is accepted only where no slack byte would be needed, and
  is refused otherwise (cobc370.c:3188-3196) **[read]**. IBM inserts
  slack bytes.
  - So "COBOL SYNCHRONIZED mapping" (GC33-0009-4 p. 291) means IBM's
    algorithm.
  - cobc370 compiles only the subset where its layout and IBM's agree.
- **As a subprogram** of a non-COBOL caller: it needs no
  initialization; COBRT opens SYSOUT on demand. COBTERM closes it if
  the program never runs STOP RUN. This is inferred from cobc370.c
  **[not re-read]**; no document says it is supported **[unverified]**.

### 2.3 fort370

**Not present** under `~/repos/mvs` (searched to depth 3, 2026-10-17).
It is known only from le-light-study.md:297-305:

- GitHub brazilofmux/fort370, FORTRAN 77, run time in C on LIBC/370.
- Each prologue passes the C stack word on (`MVC 76(4,15),76(13)`), so
  its frames keep the NAB **[read there, unverified here]**.
- No SPIE or ESTAE. About 290 KB of run time per program.
- How it passes CHARACTER arguments, how it returns function results
  and how it lays out COMMON are all **unknown**.

### 2.4 IBM compilers on TK5

- **ANS COBOL IKFCBL00: installed.** It is cobc370's oracle.
  - Procedures COBUC, COBUCL, COBUCLG, COBUPC are captured in
    cobc370/docs/baseline/procs.
  - COBUCLG runs `PGM=IKFCBL00`, then IEWL with `SYSLIB
    DSN=SYS1.COBLIB` **[not re-read]**.
  - The Optimizer supported Full ANS COBOL V4 and OS/VS COBOL
    (GC33-0009-4 p. 289; SC33-0006-7 p. 343). Which level TK5's
    IKFCBL00 is, and whether its subprograms behave like those, is
    **[unverified]**.
  - The manual says routines from compilers "that [do] not implement
    American National Standard COBOL" give undefined results (p. 296).
- **FORTRAN G (IEYFORT), H (IEKAA00), SYS1.FORTLIB**: no file in any
  repo here names them for any stand **[unverified]**. Not one claim
  about IBM FORTRAN on TK5 can be made until it is measured (probe P1).
- **PL/I (F) has no ILC** (GC28-6594-7 ch. 15: data set interchange
  only).
  - It does pass arithmetic arguments by address, so a PL/I (F)
    program can CALL a COBOL subprogram with FIXED BINARY scalars. That
    is the same partial twin as Tier 89's t240asm
    (internals/research/pli/tier89-measurement.md, "The test").
  - Strings, structures and FORTRAN arrays have no oracle: the twin
    prints the expected values.

### 2.5 LIBC/370's precedent for the reverse direction

`@@CCALL` (libc370 src/mvs/crt/@@ccall.asm:1-70; docs/consumer-notes.md
"Calling C from COBOL") **[not re-read; header read in the LE
study]**:

- It is reached through a CCALL stub.
- On the first call it builds PPA, stack, CRT and GRT, or finds an
  existing C frame.
- It cancels the caller's SPIE during the call and restores it after.
- `CCTERM` releases the environment.

This is the model for a COBOL program calling a PL/I procedure (option
C2 below). fetch.md section 3 works out the same bootstrap for FETCH,
and the two should be one mechanism.

## 3. Design options

### 3.1 PL/I calling COBOL (OPTIONS(COBOL) on an ENTRY)

**A1 -- reuse gen_asmcall (recommended).**

- OPTIONS(COBOL) behaves as ASSEMBLER plus:
  - the dummy rules;
  - the SPIE re-arm;
  - RETCODE implied **[inf]**. The manual does not say this. cobc370
    returns RETURN-CODE in R15 only when it is referenced
    (cobc370.c:11829-11851) **[not re-read]**.
- **The SPIE re-arm.** Around each COBOL call:
  - take the current PICA (SPIE returns the old one in R1);
  - after the call, `SPIE MF=(E,(1))` with it. This is @@CCALL's
    technique (@@ccall.asm:202-214) used the other way round.
  - Cost: one SVC 14 per call **[inf]**.
  - Without it, PL1PIE is lost after the first COBOL call.
  - Because COBSPIE stays armed only once per module (SPIEDONE), a
    *second* call into the same COBOL module runs under PL/I's SPIE.
    A 0C7 in COBOL is then not reported by COBOL. It becomes a system
    abend, which PL1STON reports as IHE012I.
- **INTER.**
  - Without INTER, the manual hands the interrupt to the operating
    system (p. 295), and the re-arm gives that.
  - With INTER, PL/I would have to take every code COBOL does not.
    While COBSPIE owns the single SPIE slot that cannot be done. It
    would need PL/I's own SPIE with a code list covering 1-15 during
    the call, and that changes cobc370's line messages.
  - **Proposal**: accept INTER, document it as "no effect", as LE does
    (SC26-3114-01 p. 132).
- **Dummies.** Do not generate member-by-member code in line. The
  12 KB per procedure limit (CLAUDE.md) would be reached quickly.
  - The compiler emits a **mapping table**: per elementary member, its
    PL/I offset, its COBOL offset, length and type.
  - A small run-time routine `PL1MAPC(table, pl1, dummy, direction)`
    copies in and out.
  - The COBOL layout is computed in the compiler: IBM's SYNCHRONIZED
    rule, slack bytes before each SYNC binary item relative to the 01
    start, plus inter-occurrence slack in OCCURS. That needs a COBOL
    manual for the exact rule; IBM's ANS COBOL Programmer's Guide is
    not in `internals/manuals/` **[open]**.
  - "If the compiler can determine that the mappings are identical, the
    argument is passed directly" (p. 291). Compare the two layouts at
    compile time.
- **Limits** (pp. 297-298):
  - at most 3 OCCURS levels;
  - no level-01 array;
  - no AREA or BIT members: diagnostic and no remap.

**A2 -- OPTIONS(COBOL) as a plain alias of ASSEMBLER, no remapping.**

- Cheapest. It is NOMAP for everything.
- It is wrong for any ALIGNED structure whose layout differs, silently.
- Only acceptable with a compile-time error where the layouts differ.
  That needs A1's layout comparison anyway, so it saves only the copy
  routine.

### 3.2 COBOL calling PL/I (OPTIONS(COBOL) on a PROCEDURE)

**C1 -- inside a PL/I program only.**

- A PL/I main calls COBOL, which calls back into PL/I.
- The environment exists, and the NAB is still in PL/I's frames.
- COBOL's save area is in the way: its +76 is not a NAB.
- Workable: the OPTIONS(COBOL) prologue finds the NAB by walking the
  back chain to the nearest PL/I or C frame. @@CCALL's "two passes to
  the C frame" do the same.
- Then it rebuilds the canonical list from the OS list, with
  constant-length descriptors (manual p. 293, point 2).

**C2 -- COBOL main program, no PL/I main (PLICALLA-style).**

- The OPTIONS(COBOL) entry calls a bootstrap that does the following.
  It is shared with fetch.md option F-B.
  - Build PPA, stack, CRT and GRT, as @@CCALL does. Or reuse an
    existing environment.
  - Establish PL1PION and PL1STON for the call. Drop them on return,
    giving COBOL back its SPIE.
  - Mark "PL/I environment active" so that a second entry does not
    build a second one (p. 357).
- **Ending.** The manual ends the environment when the *invoking*
  routine ends (p. 294). There is no hook for that in a cobc370 or
  IKFCBL00 program. Options:
  - (a) end at the return of the outermost PL/I procedure, every call:
    simple, but the stack is GETMAINed per call, which is slow, and
    files are closed per call;
  - (b) keep it until an explicit `CALL 'PLITERM'`, as CCTERM does.
    Output is lost if the COBOL program forgets it, because LIBC/370
    buffers;
  - (c) patch the invoker's return address. Intrusive; not proposed.
  - Proposal: (a) by default, with (b) as an option.

### 3.3 FORTRAN

- **Measure first** (probe P1). Nothing is known about the stand.
- **Calling IBM FORTRAN G/H from PL/I**: OS lists of addresses; a
  function result in R0 or FPR0 **[inf, from general IBM FORTRAN
  convention; to be checked]**.
- **FORTRAN library initialization.** The Optimizer calls "FORTRAN
  initialization routines" on the first call (SC33-0006-7 p. 357). For
  the G/H library that is IHCFCOMH/IBCOM# **[unverified]**. Without it
  FT06F001 and FORTRAN's error monitor are not there.
- **Arrays**: a multi-dimensional dummy with the subscripts reversed,
  via the same table routine (a transpose loop).
  - The dummy can be large: FLOAT DEC(6) (100,100) is 40 KB. Take
    large dummies from the heap (GETMAIN), not the frame.
- **COMMON**:
  - STATIC EXTERNAL of the same name, already a CSECT or COM.
  - A structure must start at offset 0 for COMMON, not at its phase
    (gen.c:6041-6046).
  - Data types and alignment must match (p. 293). COMMON is
    byte-aligned (Fig. 19.4 note 1), so the PL/I side may need
    UNALIGNED.
- **fort370**: frames with +76 make a call close to a PL/I-to-C call.
  It is not our repository, and its conventions are not known here.

## 4. Tiers, tests, probes

### 4.1 Probes to run first (listed, not submitted; m38b only)

- **P1. Inventory, read-only.**
  - Member list of SYS1.LINKLIB, SYS2.LINKLIB and SYS2.PROCLIB for
    IEYFORT, IEKAA00, FORTGCLG and FORTHCLG.
  - Does SYS1.FORTLIB exist, and with which IHC* members?
  - The IKFCBL00 level, from the COBUCLG listing header.
- **P2. COBOL subprogram called from PL/I (F)** with FIXED BINARY(15)
  and (31) arguments. Two variants: compiled by IKFCBL00 and by
  cobc370. Then a PL/I OVERFLOW after the call.
  - It shows whether COBSPIE (and IBM's ILBO run time) takes PL/I (F)'s
    program checks after the call.
  - It also shows what IKFCBL00's subprogram does at entry: STAE?
    SPIE? COBOL environment set-up? **[unverified]**
- **P3. Mapping probe.**
  - One COBOL subprogram with a SYNC group needing slack bytes.
    Compiled by IKFCBL00 only, since cobc370 refuses it. It DISPLAYs
    the offsets it sees, or writes the bytes.
  - Gives the IBM layout to check the compiler's COBOL mapping against.
- **P4. The same COBOL subprogram compiled by cobc370**, called from a
  PL1C/370 program built today with OPTIONS(ASSEMBLER). Shows the +8
  and SPIE interaction with PL1PIE.
  - It is a probe, not a test: PL1C/370 refuses a structure argument
    today, so scalars only.

### 4.2 Tiers

| Tier | Content | Test | Twin |
|---|---|---|---|
| I1 | OPTIONS(COBOL) on ENTRY: parse, level OPT, scalars by address, high-order bit, RETCODE, SPIE re-arm after the call; structure argument by address (closes check.c:1762 for ASSEMBLER too); `-lang=F` refuses it | `t2xxcob.opt.pli` plus a new runner convention `<name>.cobsub` (as `.asmsub`): a cobc370 subprogram built for the PL1C/370 step and the PL/I (F) step | PL/I (F) CALLs the same subprogram with FIXED BINARY arguments (P2); the rest printed |
| I2 | Dummies: COBOL SYNC layout, comparison, PL1MAPC, NOMAP/NOMAPIN/NOMAPOUT(ARGn) | as I1, COBOL subprogram from IKFCBL00 (slack bytes) | printed values; layout from P3 |
| I3 | OPTIONS(COBOL) on PROCEDURE, case C1 (PL/I main to COBOL to PL/I): OS list to canonical list, NAB search | cobc370 subprogram that CALLs back | printed |
| I4 | Case C2: COBOL main, PL/I subprogram, bootstrap (shared with FETCH, fetch.md tier F2) | job with cobc370 main | printed |
| I5 | FORTRAN: only after P1, and only if the maintainer keeps FORTRAN in scope | -- | -- |

- Each tier adds `docs/language-levels.md` rows and a `beyond_f`
  call at the OPTIONS keywords.

## 5. Cost and memory

- **I1**: small. About 100 lines in parse, check and gen. The SPIE
  re-arm is about 20 bytes of code per call site. At run time it is one
  SVC per call, and no storage.
- **I2**:
  - A mapping table of about 8 bytes per elementary member, in the
    literals or static of the caller.
  - PL1MAPC, a run-time member of its own: about 0.5 KB **[inf]**.
    Autocall takes it only into programs that remap.
  - The dummy lives in the caller's frame, at the COBOL size of the
    structure, for the duration of the call. A frame is below the line,
    and the stack is set by the compiler (Tier 97). So a large
    structure argument raises every frame size estimate. NOMAP avoids
    the dummy.
- **I3**: a back-chain walk per entry from COBOL. No storage.
- **I4**:
  - The bootstrap allocates the PL/I and C environment per call (3.2
    (a)): stack plus PPA, the compiler-set stack size, 236K median VIRT
    after Tier 97 (TODO.md #177).
  - Every COBOL-to-PL/I call pays a GETMAIN and FREEMAIN of that size
    unless (b) keeps it.
  - COBRT adds 848 bytes per COBOL load module.
- **FORTRAN**: fort370 brings about 290 KB of run time
  (le-light-study.md 2.1). IBM FORTRAN's library size is unknown.
  Array dummies can be tens of KB, so they need the heap.

## 6. Risks and open questions for the maintainer

1. **Level and scope.** Is ILC OPT (the Optimizer's form), given that
   PL/I for MVS & VM dropped FORTRAN and OS/VS COBOL (SC26-3114-01
   p. xvii)? Is FORTRAN in scope at all?
2. **Which COBOL**: cobc370 only, IKFCBL00 too, or both? cobc370
   refuses slack-byte SYNC, so the remap is only fully testable with
   IKFCBL00.
3. **SPIE ownership.** Re-arming PL/I's SPIE after every COBOL call
   changes how a later COBOL program check is reported (no COBC370
   line message). Alternatives:
   - compile COBOL with `-s`;
   - a common recovery core (le-light-study.md stage 1, a research
     result with no plan).
   Which does the maintainer want?
4. **INTER**: accept and ignore (as LE), or try to implement while
   COBSPIE exists?
5. **COBOL-main ending** (3.2): per-call environment, or an explicit
   PLITERM?
6. **COBOL manual.** IBM's SYNC slack-byte rule needs an IBM ANS COBOL
   Programmer's Guide (OS Full ANS COBOL V4 or an earlier one). None is
   in `internals/manuals/`. Can one be added?
7. **Risk: GO TO out of an on-unit across a COBOL frame** (manual: at
   most one boundary). pl1_goto knows only stack frames. Abandoning a
   COBOL routine leaves its files open and its SPIE armed.
8. **Risk: no oracle.** All PL1C/370 behaviour beyond FIXED BINARY
   scalars is checked only against printed expectations. A wrong
   layout rule would pass its own test.
9. **Unverified**: whether pl1rt reads +8 of a save area (cobc370 and
   PDPPRLG both write it); IKFCBL00 subprogram entry behaviour; FORTRAN
   on TK5.
