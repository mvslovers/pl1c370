# Design note: FETCH of PL/I procedures, FETCHABLE, BYVALUE of external procedures (#6)

Status: draft for the maintainer, 2026-10-17. Nothing built, nothing
submitted. Marks: **[read]** taken from the named file or page;
**[inf]** inferred; **[unverified]** needs a check or a probe.

Page numbers are the printed ones. GC33-0009-4 is the Optimizer's
Language Reference (1976). SC33-0006-7 is its Programmer's Guide
(1985). SC33-0025-3 is its Execution Logic (1985). SC26-3114-01 is the
PL/I for MVS & VM Language Reference (1995).

## 1. What is required

### 1.1 FETCH and RELEASE of a PL/I procedure

Sources: GC33-0009-4 pp. 69-70; SC26-3114-01 pp. 113-114 **[read]**.

- **Loading**:
  - FETCH loads the procedure unless a copy is already in storage.
  - A CALL or function reference loads it too, if its entry name
    appears in a FETCH or RELEASE anywhere in the calling procedure.
  - It is not an error if the procedure is already loaded.
  - RELEASE frees the storage.
- **The restrictions** (numbered as in SC26-3114-01):
  1. Only external procedures can be fetched.
  2. No EXTERNAL variables in a fetched procedure.
  3. No CONTROLLED variables in a fetched procedure, unless they are
     parameters.
  4. No FILE variables except SYSPRINT, unless they are parameters.
     - A file opened explicitly in the fetched procedure must be closed
       there.
     - A file opened implicitly there must be closed only in the
       fetching procedure, before the RELEASE.
     - A file passed open must not be closed in the fetched procedure.
  5. STATIC is allocated at FETCH and freed at RELEASE. INITIAL is
     applied again at every load, and STATIC without INITIAL is
     uninitialized.
  6. FETCH, RELEASE and CALL name entry constants. Such an entry cannot
     be assigned to an entry variable. The name has at most 7
     characters (GC33-0009-4 p. 70; SC26-3114-01 omits the 7).
  7. A fetched procedure cannot fetch further procedures.
  8. (MVS & VM only) Rules on sharing a fetched module between a task
     and its subtasks. These matter only with #3.
  - Violations are not diagnosed: "random errors".
- **FETCHABLE** (SC26-3114-01 p. 104; level MVS):
  - The option on an external PROCEDURE names the procedure that gets
    control when the load module is fetched.
  - A MAIN procedure in the same module takes precedence.
  - With several FETCHABLE procedures the linkage editor picks one.
- **Linking** (SC33-0006-7 pp. 87-88):
  - The Optimizer links a fetchable module with `ENTRY procedure-name`
    and `NAME`.
  - Optionally it uses `REPLACE PLISTART` and other library CSECTs to
    save space.
  - The module goes into a library on JOBLIB or STEPLIB.
- **Interaction with ILC**: a fetched procedure may not call COBOL or
  FORTRAN (SC33-0006-7 pp. 343, 460). See ilc.md.
- **Shared library**: "If the FETCH statement is used, both load
  modules must use (or neither can use) the shared library option"
  (SC33-0006-7 p. 404).

### 1.2 How the Optimizer shares its run time with a fetched module

Source: SC33-0025-3 **[read]**.

- **The fetched module carries its own copy** of the resident library
  routines it uses. Fig. 31 removes only PLISTART and a few others
  (SC33-0006-7 p. 88).
- **The state is shared through the TCA**, addressed by R12
  (SC33-0025-3 p. 51, `L 15,288(12)` for the attention poll).
- **The pseudo-register vector (PRV)** holds files, CONTROLLED
  variables and fetched entry points (pp. 27-28).
  - Its offsets are fixed by the linkage editor *per load module*.
  - Hence: "FETCHed procedure can not use controlled variables or
    files, except SYSPRINT" (p. 28).
  - **That is where restrictions 3 and 4 come from [inf].**
- **IBMBPFR** implements FETCH and RELEASE (pp. 51-52):
  - It LOADs the module and keeps its address in the PRV.
  - It builds an FECB on a chain from the TIA (FECB layout p. 368).
  - RELEASE is DELETE, and the PRV slot is reset.

So the Optimizer's answer is **code duplicated, state shared through a
register-addressed anchor**. Restrictions 2-4 exist because
per-module, link-time offsets could not be shared.

### 1.3 BYVALUE of external procedures

Source: SC26-3114-01 p. 103 (PROCEDURE) and p. 131 (OPTIONS attribute)
**[read]**.

- BYVALUE "can be specified for EXTERNAL PROCEDURE statements, but ...
  not ... for internal procedures or ENTRY statements".
- Only scalar parameters and return values, POINTER or REAL FIXED
  BINARY(31,0).
- Not for procedures called with TASK, EVENT or PRIORITY.
- On a MAIN procedure it implies NOEXECOPS.
- The ENTRY declaration of the caller says OPTIONS(BYVALUE), with the
  same type limits.
- Level MVS. The Optimizer has no BYVALUE (GC33-0009-4).
- **The manual does not say how values travel.** It does not say
  whether the list is plain words or whether the result comes back in
  R15 (the LE Programming Guide is referred to and not available) **[open]**.

## 2. What exists today

### 2.1 FETCH and RELEASE (Tier 91, not 85 or 89)

- Tier 91 per internals/ROADMAP.md:1113-1122 and rt/pl1fetch.asm:1-3.
  Tier 89 is OPTIONS(ASSEMBLER); Tier 85 is unrelated **[read]**.
- **rt/pl1fetch.asm**:
  - `PL1FTCH(name)`: a table of 16 slots, 12 bytes each
    (`FTAB DC 16XL12'00'`, pl1fetch.asm:69). LOAD EPLOC (SVC 8) when
    the module is absent (:36-40). U4091 past 16 modules (:34-35).
  - `PL1RLSE(name)`: DELETE (SVC 9), then the slot is freed (:60-62).
- **parse.c:3496-3514**: FETCH and RELEASE statements, level OPT.
- **resolve.c:505-520**: every name must be an OPTIONS(ASSEMBLER) entry.
  Anything else: "a PL/I procedure is not fetched yet" (resolve.c:516).
- **gen.c:3290-3313 (`fetch_name`, `gen_asmcall`)**: a fetched entry's
  address comes from PL1FTCH before the argument list is built.
- **A module that is not found** ends in S806, which the ESTAE reports
  as IHE012I (tier91-measurement.md). MVS & VM has ONCODEs 9250-9255
  for this (le-light-study.md 2.2) **[not re-read]**.

### 2.2 The state a fetched PL/I module would have to share

**Generated code reaches the run time by V-con.** Every procedure's
prologue and epilogue (gen.c:6270-6279, 6311-6314) do:

- `L 15,=V(PL1CURF)`; save the caller's current frame; store R13;
- `L 15,=V(PL1NOCV)`; save and set the NOCONVERSION/NOOVERFLOW bits.

The main procedure returns `PL1RTCD` (gen.c:6315-6318). Every run-time
call is `L 15,=V(PL1xxxx)`.

- A separately linked module resolves all of these against **its own
  copy** of pl1rt. That copy is complete but has its own statics.
  **This is the whole problem.**

**pl1rt file-scope state** (`LC_ALL=C grep` over rt/*.c, 2026-10-17;
regex-based, so not guaranteed complete) **[read]**:

| File:line | State | Must be shared? |
|---|---|---|
| pl1cond.c:67 `chain` | the established on-units, newest first | yes: an on-unit in the caller must see a condition raised in the fetched procedure |
| pl1cond.c:68 `acts` | the on-units running (ONCODE, ONSOURCE, ONLOC) | yes |
| pl1cond.c:69, 84, 87, 236 `terminating`, `signaling`, `pic_addr`, `in_abend` | condition processing flags | yes |
| pl1cond.c:72 `PL1CURF` | the running block's frame | yes: the SPIE exit (main's copy) and the ESTAE (pl1cond.c:269-276) read it to name the block |
| pl1cond.c:76 `PL1NOCV` | NOCONVERSION/NOOVERFLOW in effect | yes, or the bits leak between copies |
| pl1cond.c:83 `PL1RTCD` | the PL/I return code (PLIRETC/PLIRETV) | yes: the step's code |
| pl1cond.c:525-533, 907-908 `onfile`, `pendkey`, `datafield` | ONFILE, ONKEY, the CONVERSION field | yes (about 530 bytes) |
| pl1io.c:44 `files`, `sysprint_f` | the open files, SYSPRINT's state (line and page count) | **SYSPRINT yes**: restriction 4 allows it in the fetched procedure, and line/page numbering must continue |
| pl1io.c:45-59 `cur`, `mode`, `fmt`, `fi`, `rst`, `rsp`, `dprefix`, `data_mode` | the stream statement in progress | per statement; private is acceptable while a statement cannot span the call **[inf]** |
| pl1io.c:53, 197-203 `finishing`, `registered` | `atexit(finish_all)` at the first open | **hazard, see below** |
| pl1io.c:904, 1248 `pl1i_nosysprint`, `pl1i_failed_fp` | SYSPRINT missing; the write that failed | yes |
| pl1parm.c:20-21 `parm_area`, `r1_list` | the PARM | main only |
| pl1ansi.c:161, pl1ioget.c:140/220, pl1dump.c:46-47 | scratch, GET state, the PLIDUMP data set | private is acceptable **[inf]** |
| pl1fetch.asm:69 `FTAB` | the fetch table | main only (restriction 7) |
| pl1pie.asm (PIESAVE and others) | the SPIE exit's save areas | main's copy only: PL1PION is called only from MAIN (gen.c:6233-6240) |

**Two hazards beyond plain sharing [inf]:**

- **atexit into a released module.** `open_file` registers
  `finish_all` at the first open of any file (pl1io.c:197-203), in the
  copy that runs.
  - If the fetched procedure's PUT is the program's first, its own
    `finish_all` is registered.
  - After RELEASE, `exit()` calls into freed storage.
  - The same holds for any `atexit` in pl1rt that a fetched copy
    reaches (pl1cond.c:340, 350-352 are main-only today).
- **RELEASE with live references into the module.** On-units
  established by the fetched procedure are unlinked at its block exit
  (`PL1BEXT`, gen.c:6305-6310). That is safe for a normal return.
  - A RELEASE reached while the fetched procedure is still active is a
    problem: from an on-unit, or through a call back into the caller.
    It leaves `chain` entries and frames pointing into freed storage.
  - The manual's "random errors" covers it. Refusing it at run time is
    cheap: an active-count per slot.

**LIBC/370 state is mostly shared already [read, partly inf]:**

- CRT, GRT and PPA are found from the TCB, not from a static:
  - `@@PPAGET` looks at 8(TCBFSAB), then the owner TCBs, then walks the
    save-area back chain (libc370 src/mvs/crt/@@ppaget.asm:1-50);
  - `include/mvs/crt.h`: PPA `ppagrt` at +X'10', GRT
    stdin/stdout/stderr at +X'24' to +X'2C', `grtexit` (atexit) at
    +X'10'.
- So the fetched module's copy of LIBC/370 uses the caller's streams,
  heap subpool and atexit table, as long as no second `@@CRT0` runs.
  The module's entry must be the PL/I procedure, never `@@CRT0`.
- **LIBC/370 writable statics** go through `__wsaget(key, len)`, keyed
  by the address of the initializer (src/mvs/wsa/@@wsaget.c:11-52).
  - A fetched copy gets **its own** WSA, which is not shared.
  - After RELEASE that WSA is never freed. A re-fetch at another
    address adds a new one: **a leak per FETCH/RELEASE cycle**.
  - Which libc370 members use the WSA at all is **[unverified]**.
- **Application slots** exist: `grtapp1..3` (crt.h, GRT +X'34' to
  +X'3C', "for use by applications") and `crtapp1/2` (CRT +X'158',
  +X'15C') **[read]**.
- **The stack is shared without effort.** PDPPRLG takes the callee's
  frame from 76(R13) of the caller (pdpprlg.macro:49-59). A fetched
  procedure called from PL/I lands on the same NAB stack.
- **crtm** (the nested C start-up) would be wrong here. It ends through
  `__exit()` on the shared GRT and closes the parent's files
  (libc370 docs/startup.md:186-193).
- **"There must not be two concurrent PL/I environments"**
  (SC33-0006-7 p. 357). A fetched module with OPTIONS(MAIN) would run
  a second `@@CRT0`. Refuse MAIN in a module compiled as fetchable.
  FETCHABLE's "MAIN wins" (SC26-3114-01 p. 104) is LE behaviour; with
  our start-up it cannot work.

**Linking.** mbt 3 already takes a per-module entry point: `entry =
"XDRIVER"` (pl1c370 mbt.toml:52, also :60, :66). A fetchable module
would use `entry = "<procedure>"` **[read]**. Whether ld370 still pulls
`@@CRT0` into such a module by autocall is **[unverified]**: only if
something references it.

### 2.3 BYVALUE today

- **On a PROCEDURE**: only with MAIN (parse.c:3808-3813; refused
  otherwise at parse.c:3858-3859). The parameters are the words R1
  points at, through `PL1MPLS` with 65536+n (gen.c:6241-6253;
  tier90-measurement.md).
- **On an ENTRY declaration**: only with ASSEMBLER (parse.c:1329-1331;
  refused otherwise at parse.c:1748-1750). The values go into the list
  and the last word has no high-order bit (gen.c:3325-3351).
- **Checks**: POINTER or FIXED BINARY(31) only (check.c:400-412 for
  arguments, check.c:2831-2840 for parameters).
- **Not done**, per tier90-measurement.md: "BYVALUE of an external
  procedure other than MAIN (both sides of the call would need it)".

## 3. Design options for FETCH

### F-A: an anchor block (the Optimizer's model)

- Move every "yes" row of the table into one `struct pl1rt_state`.
- **Why not the PPA**: `CLIBPPA` is 48 bytes and every field is
  assigned (prev, next, crt, grt, save, tiot, pscb, flags, subpools,
  stack length, exita, cppl; libc370 include/mvs/crt.h:161-183). It has
  no application slot, so the anchor must hang off the GRT
  (`grtapp1..3`) or the CRT (`crtapp1/2`).
- The main procedure's start-up allocates it, or it is a static in the
  main module. Its address is stored in **`grtapp1`**. That needs
  libc370's agreement that pl1c370 owns the slot.
- pl1rt reaches the state through a module-local pointer `RT`, set
  once per module.
- **Main module**: `RT` is set in PL1STON's neighbourhood at start-up.
- **Fetched module**:
  - The FETCHABLE procedure's prologue calls `PL1FENT`, a member of
    its own.
  - PL1FENT finds the GRT (`__grtget`) and sets the module's `RT` from
    `grtapp1`.
  - That costs one `@@PPAGET` walk per entry into the fetched module.
- **Generated code**: `=V(PL1CURF)` and the others become `L 15,=V(RT)`
  plus an offset. That is one more load per prologue and epilogue, and
  four more bytes of code each.
- **atexit**: `finish_all` is registered by the main module's start-up
  (a fixed point), and never by `open_file`.
- **What it costs in memory**:
  - The state block: about 1.4 KB **[inf: the table's sizes]**.
  - **Each fetched module still carries its own copy** of the pl1rt
    and LIBC/370 members it references. The TZ77 map, a 159 KB program,
    held pl1rt 76,364 bytes and libc370 74,520 bytes
    (le-light-study.md 4.1).
  - A fetched procedure with PUT LIST pulls pl1io, pl1cond and stdio.
    Roughly 100 KB or more per fetched module below the line
    **[inf]**.
  - FETCH is used to *save* storage (SC26-3114-01 p. 113).
- **Side benefit**: the same refactor is the reentrancy prerequisite
  costed in le-light-study.md 3.6 (stage 4) and the multitasking state
  split for #3.

### F-B: a run-time vector (the shared-library model, in-process)

- A module compiled `-fetchable` contains **no pl1rt and no LIBC/370**.
- Every run-time reference goes through a vector:
  - The compiler emits `L 15,=V(PL1RTV)` / `L 15,n(,15)` / `BALR 14,15`
    instead of `L 15,=V(PL1xxxx)`.
  - The data words PL1CURF, PL1NOCV and PL1RTCD become vector slots
    holding addresses.
- **The vector is a CSECT in the fetched module**, filled by PL1FTCH
  right after LOAD:
  - from a table in the main module listing every pl1rt entry and
    data word;
  - the ABI stamp checked first. `PL1_ABI_STAMP` (rt/pl1abi.h:14) is
    the existing contract.
  - The in-repo precedent for an indirect run-time hook is
    `V(PL1KHOK)` in a file descriptor (CLAUDE.md, rt/pl1abi.h:39-42).
- **The main module must contain every member a fetched module could
  call**, because the vector's table references them all.
  - That is the whole of pl1rt, about 76 KB, in every program that
    FETCHes a PL/I procedure, even if the fetched code uses little of
    it.
  - Mitigation: the main program's link names only the members that
    fetched modules need. That needs a per-program list (a compiler
    `-fetchrt=` file, or weak references with a run-time check of
    empty slots). Neither exists **[inf]**.
- **What it costs in memory**:
  - A fetched module is only its own code and static: the point of
    FETCH.
  - The vector is about 4 bytes per entry, roughly 150 entries (rt/pl1rt.h
    names; count not taken), 0.6 KB per fetched module **[inf]**.
- **Constraint**: the "both or neither" rule (SC33-0006-7 p. 404). A
  fetchable module is linked for one pl1rt level, and the ABI stamp
  catches a mismatch at FETCH instead of at the link.
- It also needs the generated code to call LIBC/370 **only through
  pl1rt** **[unverified]**: a direct `=V(memcpy)` from gen.c would have
  no slot.

### F-C: restricted FETCH, no sharing

- A fetched PL/I procedure may not use SYSPRINT or conditions.
- It would then be close to OPTIONS(ASSEMBLER). It still pulls
  PL1CURF and PL1NOCV through the prologue, so it is still wrong.
- Not proposed. It would fail restriction 4's explicit allowance of
  SYSPRINT.

### Common to all options

- **The entry point**: `FETCHABLE` marks the procedure. The compiler
  could also emit a comment or an ENTRY hint. mbt's `entry =` sets it.
- **Refuse in a `-fetchable` compile**, with a diagnostic, which the
  Optimizer could not do:
  - MAIN, EXTERNAL variables, CONTROLLED non-parameters, FILE constants
    other than SYSPRINT;
  - FETCH statements (restrictions 2-4, 7).
- **FETCH of a PL/I entry**: resolve.c:510-520 accepts a stub that is
  not ASSEMBLER.
  - gen_fcall takes its entry point from PL1FTCH, as gen_asmcall does
    (gen.c:3302-3313).
  - The rest of the PL/I call is unchanged: word 0 = 0 (external),
    word 1 the result, then the arguments (gen.c:3398-3420).
- **STATIC reinitialization** follows from LOAD after DELETE: a
  non-reentrant module whose use count reached 0 is read again from
  the library **[inf; probe P2]**.
- **Bootstrap without a PL/I main** (a PLICALLA-like entry): needed for
  ilc.md option C2 (a COBOL main calling PL/I). It is *not* needed for
  FETCH, whose caller is always a PL/I program. Keep it separate. If it
  is built, model it on LIBC/370's `@@CCALL` (@@ccall.asm:1-70: build
  PPA, stack, CRT and GRT on the first call; suspend the caller's
  SPIE; `CCTERM`).

## 4. Design for BYVALUE of external procedures

Both sides are PL1C/370's, so the convention is ours to choose.

- **B1 (keep the PL/I list)**:
  - Word 0 = 0, word 1 = the result area, then the *values* in the
    argument words, not their addresses.
  - The callee's prologue copies them into cells in its frame, and the
    parameters are those cells.
  - A RETURNS(POINTER or FIXED BIN(31)) result goes through word 1 as
    today.
  - Smallest change: one flag on each side, at gen_fcall and the
    prologue at gen.c:6207-6230.
- **B2 (an OS list of values, result in R15)**:
  - R1 points at the values, no header words, no high-order bit; the
    result comes back in R15.
  - **This is what cc370 C code does** with `int f(int, void *)`
    **[inf: cc370's list holds values and returns an int in R15;
    check against cc370 internals]**. A BYVALUE external procedure
    would then be callable from C, and a BYVALUE ENTRY could call C.
  - That is #10, which the maintainer has deferred (TODO.md "Parked").
  - B2 is the bigger change: the procedure no longer has the PL/I
    header, so internal calls of it need the B2 form too.
- **Recommendation**: B1 for #6. Mention B2 only as the hook for #10.
  *Done as B1 in Tier 123 (`t307byv`).*
- **Rules to check** (SC26-3114-01 p. 103):
  - not on internal procedures or ENTRY statements;
  - scalars POINTER or FIXED BIN(31,0), parameters and RETURNS;
  - the caller's ENTRY must say OPTIONS(BYVALUE). A mismatch is
    undetectable across compilations, as with any ENTRY descriptor.
- **Level MVS**: `beyond_f` already exists at parse.c:1330 and 3812.

## 5. Tiers, tests, probes

### 5.1 Probes to run first (listed, not submitted; m38b)

- **P1.** A PL1C/370 program FETCHes a module whose code is a PL1C/370
  external procedure, linked separately with `entry =`, that does a PUT
  LIST. Today that needs a compiler change, so this probe is in-house
  only. It shows the atexit hazard and the second SYSPRINT state.
  Expected: a duplicated or garbled line or page count, or an S0C1/S0C4
  at the end after RELEASE **[inf]**.
- **P2.** STATIC re-initialization: FETCH, change a STATIC, RELEASE,
  FETCH, print. Uses an assembler module (ASMTST-like) with a counter
  in its own CSECT. It can run on today's Tier 91 path.
- **P3.** Inspect the ld370 map of a module linked with `entry =
  <procedure>`: are `@@CRT0`, `@@PPA` and the others present? Host
  only, no MVS.
- **P4.** Read-only: which libc370 members call `__wsaget` (the WSA
  leak per fetch cycle)? Host grep over the libc370 source.

### 5.2 Tiers

| Tier | Content | Test |
|---|---|---|
| F1 | BYVALUE external procedures (B1): parse, check, gen both sides | `t2xxbyv.opt.pli` with `.sep` (two external procedures compiled apart); twin prints the values (PL/I (F) has no BYVALUE) |
| F2 | pl1rt state behind an anchor (F-A) or the vector table (F-B), no FETCH yet; `finish_all` registered by start-up only | whole suite unchanged; `make test` host tests |
| F3 | `-fetchable` / FETCHABLE: compile-time restrictions, no MAIN, entry point; runner convention `<name>.fetch` (a second source compiled fetchable and linked as its own module in the development library) | `t2xxfpl.opt.pli` + fetched source: FETCH, CALL with arguments, PUT in both, an ON condition in the caller that catches a SIGNAL in the fetched procedure, RELEASE, CALL again (reloaded, STATIC reinitialized), PLIRETC in the fetched procedure seen by the caller |
| F4 | RELEASE while active refused; ONCODE 9250-9255 instead of S806 (level MVS) | negative tests |

- No oracle: PL/I (F) has no FETCH (Tier 91 called ASMTST linked in as
  its twin, tier91-measurement.md). The twin of F3 calls the procedure
  linked in statically and prints the same lines.

## 6. Cost and memory

| Item | F-A (anchor) | F-B (vector) |
|---|---|---|
| Work | larger: every access to about 20 statics in pl1cond.c/pl1io.c rewritten through `RT`; generated prologue changes | larger in the compiler (every `=V(PL1...)` site), smaller in pl1rt |
| Main module | about 1.4 KB more (state block), same code | **whole of pl1rt linked** (about 76 KB) unless a member list is built |
| Each fetched module | its own pl1rt + libc370 members, about 100 KB or more **[inf]** | its own code + about 0.6 KB vector |
| Per FETCH/RELEASE cycle | libc370 WSA leak if libc370 members with WSA are in the module (P4) | none |
| Run-time cost | +1 load per prologue/epilogue; one PPA walk per entry into a fetched module | +1 load per run-time call |
| BYVALUE (B1) | small: tens of lines each side, no storage | same |

- **Memory is the deciding factor.** F-B is what makes FETCH save
  storage. F-A makes a fetched module about as big as a program
  **[inf]**. On MVS 3.8j a region holds the caller, every fetched
  module in use and the stack (Tier 97 median 236K VIRT for a small
  program, TODO.md #177).

## 7. Risks and open questions for the maintainer

1. **F-A or F-B?** F-A is closer to the Optimizer, and doubles as the
   reentrancy and multitasking refactor. F-B is the one that saves
   storage, at the price of linking all of pl1rt into a FETCHing main.
2. **`grtapp1`** (or a CRT slot) for the anchor: is pl1c370 allowed to
   own it? libc370 should record that, so another consumer does not
   take the slot.
3. **The atexit hazard** exists today for any future separately linked
   PL/I code. Should `finish_all` move to a fixed registration point
   now, independent of #6?
4. **libc370 WSA leak** per FETCH/RELEASE cycle (if any libc370
   member in the fetched module uses `__wsaget`): accept, or ask
   libc370 for a release-by-key call?
5. **BYVALUE convention**: B1 now, B2 only with #10?
6. **MAIN in a fetched module**: refuse (proposed), unlike LE's "MAIN
   wins"?
7. **Risk**: recovery paths. The SPIE exit and ESTAE live in the main
   module's copy and read PL1CURF. Under F-A they see the fetched
   procedure; under F-B they always did. A program check inside a
   fetched procedure must be measured, not inferred.
8. **Unverified**: ld370 autocall of `@@CRT0` into an `entry =` module
   (P3); STATIC re-initialization by reload (P2); which libc370 members
   use the WSA (P4); the full list of pl1rt statics (regex grep, not
   exhaustive).
