# Design note: multitasking (#3)

Status: tier 0 built (Tier 110). P0 found PL/I
(F)'s tasking modules in SYS1.PL1LIB, so tiers 2 to 4 may have an
oracle. Tier 1 needs no decision; the end state (A, B or C, section 11)
is the maintainer's before tier 2.

Status: draft for the maintainer's decision. Nothing here is built or
measured on MVS. Every claim about IBM's products cites a manual page;
every claim about our code cites file:line (pl1c370 and libc370 trees as
of 2026-10-17). Claims marked **(unverified)** are inferences or
recollection and need a probe or a read of the named source before
anyone builds on them.

Manuals, short names used below:

- **LRM** GC28-8201-4, PL/I (F) Language Reference (Dec 1972)
- **PG** GC28-6594-7, PL/I (F) Programmer's Guide (Jan 1971)
- **PLM** GY28-6801-6, PL/I Subroutine Library Program Logic (Jun 1972)
- **OPT-LRM** GC33-0009-4, Checkout and Optimizing Compiler Language
  Reference (Oct 1976)
- **OPT-PG** SC33-0006-7, Optimizing Compiler Programmer's Guide (Sep 1985)
- **MVS-LRM** SC26-3114-01, **MVS-PG** SC26-3113-01 (PL/I for MVS & VM)

## 1. Summary

- The language part splits cleanly in two. **Events without tasks**
  (EVENT variables, COMPLETION/STATUS, WAIT, DELAY, DISPLAY ... REPLY ...
  EVENT, READ/WRITE ... EVENT) need no second flow of control. They can
  be built on today's single-threaded run time and twin-tested exactly.
- **Tasks** (CALL ... TASK/EVENT/PRIORITY, EXIT in a subtask, PRIORITY)
  collide with three things: the run time's static state (about 40
  cells, section 5), the generated code's direct use of two of them
  (`PL1CURF`, `PL1NOCV`, `src/gen.c:6271-6281`, `6312-6315`), and the
  termination path, which ends the program with `exit()` from wherever
  it is (`rt/pl1cond.c:379`, `804`, `834`).
- Three designs are laid out (section 6): **A** real MVS subtasks
  (ATTACH, a run-time context per TCB, PL/I (F)'s own shape);
  **B** PL/I tasks as cooperative contexts on the one job-step TCB,
  switched only at WAIT, DELAY, a reply wait and task end;
  **C** a run-to-completion subset. The recommendation, for the
  maintainer to decide, is tiers 0-1 first, then B. A stays possible
  later: the first refactoring step (gather the per-task state into one
  structure) is common to A and B.

## 2. What the language requires

### 2.1 The pieces

| Item | Form | Reference |
|---|---|---|
| TASK attribute | `DCL T TASK;` priority value, FIXED BIN(15); arrays, scope, storage class, DEFINED on task names; no conversion; no assignment of aggregates containing tasks; no dummy argument | LRM p. 352 |
| EVENT attribute | `DCL E EVENT;` completion BIT(1) and status FIXED BIN(15); event-to-event assignment only; values undefined on allocation; cannot be UNALIGNED | LRM pp. 337-339 |
| OPTIONS(TASK) | on the external PROCEDURE; not on internal ones | LRM p. 195; PG pp. 163-164 |
| CALL options | `TASK[(t)]`, `EVENT(e)`, `PRIORITY(expr)`, any combination; each makes the call asynchronous | LRM pp. 195-196 |
| WAIT | `WAIT (e[,e]...) [(expr)];` names events, not tasks | LRM p. 199, pp. 392-393 |
| DELAY | `DELAY (expr);` milliseconds | LRM p. 200, p. 372 |
| EXIT | ends the task and the tasks it attached; in the major task it is STOP | LRM p. 377 |
| COMPLETION, STATUS | built-in functions and pseudo-variables; an event array gives an array | LRM pp. 303-304 |
| PRIORITY | built-in function and pseudo-variable, relative to the current task | LRM p. 303, p. 197 |
| DISPLAY ... REPLY ... EVENT | the reply is awaited by a later WAIT | LRM p. 373; PLM p. 74 |
| READ/WRITE/REWRITE/DELETE ... EVENT | UNBUFFERED SEQUENTIAL CONSECUTIVE or REGIONAL, and DIRECT CONSECUTIVE, INDEXED, REGIONAL | LRM p. 120 |
| EXCLUSIVE, NOLOCK, UNLOCK | record locks between tasks | LRM pp. 198-199 (Fig. 15-2), p. 339 |

### 2.2 Semantics that matter for the design

- **Creation.** A CALL with a tasking option attaches a subtask that
  runs the procedure concurrently. Blocks it activates synchronously
  belong to it. The attached task is a subtask of the attaching task
  (LRM p. 195).
- **On-units are inherited** at attachment, "just as if the initial
  block of the subtask had been synchronously invoked" (LRM p. 195).
  PL/I (F) copies the attaching task's ON field into the subtask's
  storage (PLM p. 241, "ON field").
- **Data.** STATIC is shared. AUTOMATIC is shared wherever it is known,
  passed or based (LRM p. 197). Of CONTROLLED, only the latest
  generation at attachment is shared; later ALLOCATEs are task-local; a
  task can free only its own generations (LRM pp. 197-198). BASED
  allocations are freed when the allocating task ends, except inside an
  AREA of another task (LRM p. 198, item 4).
- **Files.** A file open at attachment is shared. The subtask must not
  close it. A file opened in a task is closed when that task ends
  (LRM p. 198; p. 201 item 2). A file not open at attachment is not
  shared: each task opens its own (guaranteed only for DIRECT files).
- **Events.** CALL ... EVENT sets the event incomplete and status 0
  before the procedure is entered; task end sets it complete, and
  status 1 if the end was abnormal and the status still 0 (LRM p. 196,
  p. 338 rules 9-10). An active event cannot be reused or set by
  COMPLETION or assignment (LRM p. 338 rule 7).
- **WAIT.** Waits for all named events, or for `expr` of them. An I/O
  event is completed only by a WAIT in the task that started it; its
  conditions are raised in the WAIT, in the order TRANSMIT or ENDFILE,
  KEY, RECORD (LRM p. 120; pp. 392-393 rules 5-10). Non-I/O events are
  set complete before I/O events (rule 9).
- **Termination** (LRM pp. 200-201). A task ends by RETURN/END of its
  initial procedure (normal), by EXIT, by STOP in any task, by the end
  of the block that attached it, or by the end of its attaching task
  (all abnormal). At the end: incomplete I/O events are set complete
  with status 1; files it opened are closed with I/O conditions
  disabled; its CONTROLLED and BASED allocations are freed; its active
  blocks and subtasks end; its event is set complete; its record locks
  are released.
- **STOP, EXIT, ERROR.** FINISH is raised by STOP in any task or EXIT
  in the major task (LRM p. 320). The PG says that STOP in a subtask
  raises FINISH *in the subtask* (PG p. 173 item 1). ERROR without an
  on-unit in a subtask prints a message, does not raise FINISH, ends
  the subtask, and sets its event abnormal (PG p. 173 item 4). An abend
  in a subtask ends the subtask only, with a message on SYSPRINT or the
  console; ERROR is not raised (PG p. 173).
- **Priority.** The major task starts at `16 * job step priority + 10`,
  which is also the maximum; a computed priority above it is clamped,
  below 0 becomes 0 (LRM p. 197; PG p. 166). PRIORITY returns the
  difference to the current task. PL/I (F) computes the new value as
  `MAX(0, MIN(limit-1, P+N))` and keeps it in the task variable
  (PLM p. 62). PRIORITY as pseudo-variable without an argument means the
  current task (PLM p. 62; PG p. 167, Fig. 13-2 `PRIORITY=-7;`), while
  the LRM says the multitasking built-ins "all require arguments"
  (LRM p. 303). Probe item.
- **Requirements.** PL/I (F) multitasking needs MVT (PG p. 163). The
  minimum overhead is "about 3.5K bytes plus 2K bytes for each subtask
  attached" and about 70 ms per attach on a model 40 (PG p. 163).
  At most 255 active tasks (PG p. 494, IHE560I).
- **Messages** with their texts: IHE550I to IHE560I, IHE571I to
  IHE577I (PG pp. 492-496). Twin-testable where the program provokes
  them.

### 2.3 How PL/I (F) does it (PLM chapter 5)

- With OPTIONS(TASK), IHETSA replaces IHESAP (PLM p. 54, Fig. 25),
  and multitasking variants replace six I/O modules (IHEOCT, IHECTT,
  IHEPTT, IHEIBT, IHEDDT, IHEINT; PLM p. 63). A program without
  OPTIONS(TASK) pays nothing.
- A **control task** is the job step's first task. It attaches the
  major task (through IHESUB) one priority lower, and every PL/I task
  later, so that every PL/I task is its subtask (PLM pp. 54-55, 57).
  That is how any task can CHAP any other, and how STOP in a subtask
  can end the major task (PLM p. 54).
- "Soft code" (attach, task end, block end with tasks, PRIORITY,
  COMPLETION/event assignment, WAIT, OPEN, CLOSE, EXCLUSIVE chaining)
  runs one task at a time, serialized by the control task through ECBs
  (PLM p. 55).
- Each task has its own **PRV** (pseudo-register vector, the library's
  per-task static) and LWS (PLM p. 241, Fig. 79). That is the PL/I (F)
  equivalent of the per-task block in section 5.
- Each DSA has a word for the chain of task variables attached in that
  block, so that block end can terminate them (PLM p. 237, Fig. 77).
- **SYSPRINT** PUT statements are serialized by ENQ on the SYSPRINT FCB
  with a per-DSA resource counter, so that a PUT inside a function
  called from a PUT does not deadlock (PLM pp. 24-25).
- Layouts: the event variable is 32 bytes (flags, internal ECB, status,
  statement number, external ECB; PLM p. 239, Fig. 78); the task
  variable 28 bytes (flags, PRV, TCB, symbol table entry, event,
  limit and dispatching priority, chain; PLM p. 243, Fig. 80); the
  control task's area X'4D0' bytes with a 256-entry ECB list (PLM
  p. 235, Fig. 76).
- CONTROLLED storage in multitasking goes through IHETCV, which stamps
  each generation with the task's invocation count (PLM p. 62).
- DELAY is STIMER WAIT; DISPLAY with REPLY and EVENT returns at once and
  the WAIT waits for the reply (PLM p. 74).

### 2.4 Later levels

- OPTIONS(TASK) only documents the use; the CALL options suffice
  (MVS-LRM p. 105; OPT-LRM p. 466 note a). PL/I (F) assumes it when a
  CALL carries a tasking option (PG p. 164).
- PL/I for MVS & VM runs tasks on LE and OpenEdition (MVS-PG p. 422).
  PLITASKCOUNT, THREADHEAP and NONIPSTACK are LE run-time options
  (MVS-PG Chapter 19): excluded by the language target.
- ONCODEs 3901-3915 for tasking errors (MVS-LRM p. 335). PL/I (F) has
  the IHE55xI messages; which ONCODEs it uses is a probe item.
- The Optimizer's TASKHEAP and the third ISASIZE argument (maximum
  active tasks) are run-time options without LE (OPT-PG p. 43).

## 3. What PL1C/370 has today

Compiler:

- No TASK or EVENT type: `enum tkind` (`src/pl1c.h:97`) has none.
- `OPTIONS(TASK)` is refused: every option other than MAIN, BYVALUE,
  BYADDR, NOEXECOPS and REENTRANT is a fatal error
  (`src/parse.c:3824-3826`).
- `CALL` takes a reference and a semicolon, no options
  (`src/parse.c:3696-3702`).
- `EXIT` is parsed as STOP (`src/parse.c:3645-3650`) and generates
  `PL1STOP` (`src/gen.c:5262-5265`). Right for the major task only.
- `DISPLAY ... REPLY ... EVENT` is parsed and then refused
  (`src/parse.c:3594-3611`).
- No WAIT, DELAY, COMPLETION, STATUS or PRIORITY.

Generated code (`src/gen.c`):

- Every procedure's prologue saves `PL1CURF` into the frame and stores
  R13 into it; it saves and sets `PL1NOCV` (`6271-6281`); the epilogue
  restores both (`6312-6315`). The main procedure returns `PL1RTCD`
  (`6316-6320`). These are V-constants to single static cells in the
  run time (`rt/pl1cond.c:72`, `76`, `83`).
- The main procedure's prologue issues the SPIE (`PL1PION`) and the
  ESTAE (`PL1STON`) (`6233-6240`).
- A block that establishes on-units calls `PL1BEXT` at its end
  (`6306-6311`); nothing else happens at block end.
- GO TO out of a block calls `PL1GOTO` with the target frame
  (`5557-5585`).
- The C stack size is the sum of all frames plus a 32 KB reserve, or
  LIBC/370's 256 KB when unknown (`636-645`, `6153`, `6433-6443`).
- An internal procedure is called with R1 at a list: static link,
  result address, argument addresses (`src/gen.c:13-15`). The list and
  dummy arguments live in the caller's frame.
- The generated CSECT's only writable data are STATIC variables and
  CONTROLLED anchors (`src/gen.c:6004-6052`); AUTOMATIC variables,
  temporaries and argument lists live in the frame (`src/gen.c:9-11`).
  So the generated code itself is safe for several tasks at once; the
  shared state is the run time's (section 5) plus the CONTROLLED
  anchors.

Run time:

- On-units: a chain of establishment records that live in the
  establishing frames (`rt/pl1cond.c:37-45`, `67`, `101-115`), and a
  chain of active on-units (`struct act`, on the C stack,
  `rt/pl1cond.c:49-57`, `68`, `582-600`).
- `pl1_goto` unlinks every establishment whose frame address is above
  the target frame, and every `act` above it (`rt/pl1cond.c:139-155`).
  **That assumes one stack**: with a second stack the address order
  between tasks means nothing.
- The I/O statement state is already a unit that is saved and restored
  around an on-unit (`struct pl1i_iostate`, `rt/pl1int.h:69-72`;
  `rt/pl1io.c:968-985`; used at `rt/pl1cond.c:596-598`).
- Termination: `terminate()`, `pl1i_quit()` and `pl1_stop()` call
  `pl1_finish()` and `exit()` (`rt/pl1cond.c:372-380`, `799-805`,
  `827-835`). There are 33 `exit()` calls in `rt/*.c` (code lines; no comment matches).
- Recovery: one ESTAE (`rt/pl1cond.c:342-353`); its exit writes IHE012I
  and **closes stdout** (`rt/pl1cond.c:242-296`, `fclose(stdout)` at
  `287`), guarded by a static `in_abend` (`236`). One SPIE with a
  static save area for the interrupted PSW and registers (`PIESAVE`,
  `PIERES`, `OLDPICA`, `rt/pl1pie.asm:121-123`).
- DISPLAY waits for the WTOR on an ECB on its own stack
  (`rt/pl1dsp.c:44`, `64`, `73-74`).
- EXCLUSIVE, NOLOCK and UNLOCK lock nothing "as one task sees them"
  (`docs/language-levels.md:300-304`).
- Storage: `pl1_allocate` is `calloc`, `pl1_free` is `free`
  (`rt/pl1rt.c:237-249`); nothing records which task allocated what.
- CONTROLLED generations are stacked on a static anchor
  (`src/pl1c.h:153`, `src/gen.c:6018-6019`).

## 4. What LIBC/370 offers and forbids

Offers:

- **Threads**: `cthread_create_ex(func, arg1, arg2, stacksize)`
  ATTACHes the driver `CTHREAD` (`include/mvs/thread.h:98`;
  `src/mvs/thread/@@ctcrtx.c:84-101`). The driver builds a CLIBCRT for
  the new TCB, calls `func(arg1, arg2)` on a stack inside the CTHDTASK
  block, and ends through `@@CTEXIT`, which runs the thread's cleanup
  and frees the CLIBCRT (`src/mvs/thread/@@cthrd.asm:23-107`).
- Defaults: 64 KB stack (`@@ctcrtx.c:31`), allocated with `calloc`
  together with the 44-byte CTHDTASK (`@@ctcrtx.c:111`;
  `include/mvs/thread.h:76-88`); `DPMOD=-1` is fixed in the ATTACH
  (`@@ctcrtx.c:94`); the end-of-task ECB is `termecb`.
- `CTHREAD` is linked only when a program creates threads; `@@CRT0`
  IDENTIFYs it through a weak reference (`docs/startup.md:23-27`,
  `122-135`).
- **Anchors**: CLIBPPA per program, CLIBCRT per TCB (392 bytes,
  `include/mvs/crt.h:100-145`), CLIBGRT per process with the open-file
  array and the standard streams (`include/mvs/crt.h:36-76`).
- **ECBs and timers**: `ecb_wait`, `ecb_waitlist`, `ecb_timed_waitlist`
  (`include/mvs/ecb.h:14-39`). `ecb_waitlist` waits for **any** ECB
  of a list; there is no count operand. `WAIT (e1,e2)(2)` needs the
  WAIT macro's count form or a loop.
- **Locks** are ENQ (`src/mvs/lock/@@lk.c:9-22`): rc 8 means this task
  holds it already, so they nest within a task (`include/mvs/lock.h:17-20`).
- **stdio is task-aware**: each FILE call takes the FILE's ENQ when
  another task can reach the stream, decided from the TCB tree
  (`src/stdio/@@flock.c:30-62`). Without subtasks it costs nothing; with
  one, every stdio call of the program pays an ENQ/DEQ pair, "about 120
  microseconds a call" (`@@flock.c:56-59`, #453).
- **ESTAE is per TCB**: `__estae` keeps its stack in the CLIBCRT
  (`crtestct`, `crtestpl`; `src/mvs/recovery/@@estae.c:55-65`;
  `include/mvs/crt.h:112-113`).
- **Heap**: `malloc` is a GETMAIN per call through `@@GETM`, rounded to
  64 bytes (`src/stdlib/malloc.c:16-43`; `src/mvs/storage/@@getm.asm`).
  "cthread TCBs carry no PPA at all" and allocate from subpool 0
  (`@@getm.asm:16-19`).

Forbids, or makes dangerous:

- **`exit()` from a subtask.** `__exit` runs every `atexit` function,
  closes every file of the process and frees the GRT
  (`src/mvs/crt/@@exit.c:16-111`); `@@EXITA` then frees the PPA and
  stack found from the TCB's first save area (`@@exita.asm:10-50`). From
  a thread that tears down the parent's run time. A subtask must end
  through `cthread_exit` (`include/mvs/thread.h:125`).
- **`__crtget()` per call is expensive**: it takes `lock(ppa,1)` (an
  ENQ, then a DEQ) on every call (`src/mvs/crt/@@crtget.c:26-40`). A
  per-task anchor reached that way in every PL/I prologue is ruled out
  on cost.
- **Known defect**: cthread teardown can DETACH a live worker
  (libc370 #11, deferred; `internals/priorities.md:168-170`).
- **Not checked** (unverified): whether a FILE opened by one TCB may be
  read or written by another (QSAM/BSAM DCB use across tasks), and what
  `__exit` does with a FILE whose DCB MVS already closed at the end of
  the subtask that opened it. PG p. 170 names S001 and S301 for
  unsynchronized access from more than one task.

## 5. What would have to become per task

The run time keeps its state in static cells. Each row says whether the
cell describes a task (must be per task in any design with tasks), or
the program (stays shared, may need a lock). The last column is what
design B needs: only state that can be live across a switch point
(WAIT, DELAY, a reply wait, task end; possibly inside an on-unit or a
function called from a PUT list).

| Cell | Where | Scope | Live across a switch (B)? |
|---|---|---|---|
| `pl1_curframe` (PL1CURF) | `rt/pl1cond.c:72`, written by generated code `src/gen.c:6271-6274`, `6312-6313` | task | yes |
| `pl1_noconv` (PL1NOCV) | `rt/pl1cond.c:76`; `src/gen.c:6277-6281`, `6314-6315`, `4704-4709` | task | yes |
| `chain` (established on-units) | `rt/pl1cond.c:67` | task; inherited by copy at attach | yes |
| `acts` (on-units running) | `rt/pl1cond.c:68` | task | yes |
| `terminating`, `signaling`, `snap_trace` | `rt/pl1cond.c:69`, `84`, `184` | task | yes |
| `pic_addr` | `rt/pl1cond.c:87` | task | yes |
| `in_abend` | `rt/pl1cond.c:236` | task (ESTAE per TCB in A) | n/a in B |
| `onfile`, `onfile_len` | `rt/pl1cond.c:525-526` | task | yes |
| `pendkey`, `pendkeylen` (256 B) | `rt/pl1cond.c:532-533` | task | yes |
| `datafield`, `datafield_len` (256 B) | `rt/pl1cond.c:907-908` | task | yes |
| statement state `cur`, `mode`, `fmt`, `fi`, `pl1x_bits`, `rst`, `rsp`, `data_mode` | `rt/pl1io.c:45-52`, `59` | task | yes (already `pl1i_iostate`) |
| `dprefix`, `dplen` (320 B) | `rt/pl1io.c:57-58` | task | yes, if a PUT DATA item calls a function that waits |
| `count_file` | `rt/pl1io.c:359` | task | yes |
| `string_f`, `s_target` (GET/PUT STRING) | `rt/pl1io.c:423-424` | task | yes |
| `recbuf`, `copy_desc` | `rt/pl1io.c:995`, `1109` | task (statement scratch) | no (no switch inside) |
| `last_pic`, `last_picn`, `last_kind` | `rt/pl1ioget.c:139-140`, `220` | task | yes, inside an on-unit |
| `ub[3][4096]` (12 KB) | `rt/pl1ansi.c:161` | task in A | no |
| `t[256]` | `rt/pl1ansi.c:113` | task in A | no |
| `date_buf`, `time_buf`, `dt_buf` | `rt/pl1date.c:12`, `73` | task in A | no |
| `PIESAVE`, `PIERES`, `OLDPICA` | `rt/pl1pie.asm` data part | task | yes (an on-unit for an interruption may wait) |
| `pl1_retcode` (PL1RTCD) | `rt/pl1cond.c:83` | program | shared |
| `files`, `sysprint_f`, `finishing`, `pl1i_nosysprint`, `pl1i_failed_fp`, `pl1i_place_num` | `rt/pl1io.c:44`, `53`, `704`, `904`, `1248` | program; each pfile needs "opened in task X" | shared, needs a lock in A |
| `dfp`, `tried` (PLIDUMP file) | `rt/pl1dump.c:46-47` | program | shared |
| `parm_area`, `r1_list` and the PARM lists | `rt/pl1parm.c:20-21`, `36-37`, `66-68` | program (main only) | shared |
| CONTROLLED anchors (generated STATIC) | `src/gen.c:6018-6019` | task-local generations above the attach point | yes |
| heap allocations | `rt/pl1rt.c:237-249` | owner task, for freeing at task end | per task list |

Sizes: the task cells add up to roughly 1.3 KB, plus `string_f`
(a `struct pfile`, `rt/pl1iof.h:20-60`) **(estimate, not counted with
sizeof)**. `ub` alone is 12 KB.

## 6. Design options

### 6.1 Option A: real MVS subtasks, a run-time context per TCB

Each PL/I task is a TCB. This is PL/I (F)'s shape.

- **Control task.** With multitasking linked, `@@MAIN` does not run
  the main procedure on the job-step TCB. The job-step TCB becomes the
  control task: it ATTACHes the major task and every later task, does
  CHAP, and ends the program on STOP (PLM pp. 54-58). Then STOP in a
  subtask can end the major task, any task can change any task's
  priority (CHAP works only on the issuing task or its subtasks,
  **unverified** for MVS 3.8j), and `exit()` runs only on the
  job-step TCB, where `@@CRT0` built the run time.
- **Per-task anchor.** Generated code must reach the task's `CURF` and
  `NOCV`. Choices: (a) a new frame word, copied from caller to callee
  like the static link, holding the task block: one MVC per prologue,
  a new `PL1_FR_*` in `rt/pl1abi.h` and a new `PL1_ABI_STAMP`; but the
  run time calls on-units from C frames (`rt/pl1cond.c:597`), which
  have no such word, so the run time must pass it. (b) A TCB-keyed
  lookup. Through `__crtget()` it costs an ENQ/DEQ per call (section 4)
  and is ruled out. A direct read is cheap: PSATOLD (X'21C') -> TCB ->
  first save area (TCBFSAB, TCB+X'71', as `@@flock.c:8-9` reads it) ->
  word at +8. For a CTHREAD TCB that word is the thread stack inside
  the CTHDTASK (`@@cthrd.asm:32-35`), for the main TCB the PPA
  (`docs/startup.md:102-109`). Depending on that needs libc370 to
  document it, or PL1C/370 to ATTACH with a driver of its own.
  (c) The C run time takes the same anchor at entry of each external
  function: about 170 entry points (`rt/pl1rt.h`, 174 `asm("PL1...")` names), mechanical.
- **Locks.** Every PUT, GET and record statement takes a statement-level
  lock on its pfile; nested PUTs in the same task pass because ENQ
  rc 8 means "already held" (`include/mvs/lock.h:17-20`). That is
  PL/I (F)'s resource counter in another form (PLM pp. 24-25), but it
  must be released on GO TO out of an on-unit.
- **Recovery per task.** Each task's driver needs its own ESTAE
  (`__estae` is per TCB already), an abend exit that does not close the
  shared SYSPRINT, and its own SPIE. The SPIE save area in
  `rt/pl1pie.asm` must move into the task block. Whether a subtask
  inherits the mother's SPIE is **unverified** (assembler probe P2,
  section 8).
- **Termination.** Subtasks end through `cthread_exit` or by returning
  to the driver, never `exit()`. Files opened in the task are closed in
  the task. Abnormal end: the control task sees the completion code in
  `termecb` and writes IHE571I-style messages (PG pp. 494-496).
- **Gains:** real concurrency, real priorities, an abend ends only the
  subtask (as in PL/I (F), PG p. 173), synchronous I/O in one task does
  not stop the others.
- **Costs:** the ABI change; the anchor in every prologue; the ENQ cost
  on every stdio call once a subtask exists; per-TCB recovery code (the
  most expensive code to get wrong, `internals/research/le-light-study.md`
  section 4.3); libc370 extensions (priority in ATTACH, maybe a driver
  hook); and the open questions on DCB use across TCBs.

### 6.2 Option B: cooperative PL/I tasks on one TCB

Each PL/I task is a context in the run time: its own C stack and its
own copy of the "live across a switch" cells of section 5. One MVS task
runs all of them. A scheduler in the run time switches only at:

- WAIT, when the awaited events are not complete;
- DELAY;
- DISPLAY ... REPLY without EVENT (the WTOR is issued with an ECB and
  the task blocks, so other tasks run: needed for the LRM's BANKER
  example, LRM pp. 202-203);
- a record lock held by another task (EXCLUSIVE);
- CALL with a tasking option, when the new task's priority is higher;
  PRIORITY pseudo-variable, when another ready task becomes higher;
- task end.

What it needs:

- An assembler switch routine: save R2-R14 into the current context,
  load the next one's, return. A new task starts on a fresh stack with
  a first frame whose NAB word (offset 76) points past it, the C
  convention of `docs/startup.md:78-88`.
- At a switch, copy the per-task cells out and in. With the cells
  gathered in one structure that is one `memcpy` each way (about
  1.3 KB, at a switch only). `PL1CURF` and `PL1NOCV` are saved and
  restored the same way, so **the switch itself changes neither the
  generated code nor `rt/pl1abi.h`**. The chain of tasks attached in
  a block (section 7) is kept by the run time in a side list keyed by
  the attaching frame's address, not in a frame word, so B needs no
  ABI change at all; it needs the block-end call of section 7.
- When no task is ready: wait on the ECB list of outstanding WTORs, and
  a timer ECB for the earliest DELAY (`ecb_timed_waitlist`,
  `include/mvs/ecb.h:33`, 0.01-second units).
- Priorities kept in the task variables and computed as PL/I (F) does
  (`MAX(0, MIN(limit-1, P+N))`, PLM p. 62); the scheduler picks the
  highest ready one. PRIORITY returns the same numbers as PL/I (F) if
  the probes confirm the starting values.
- On-unit inheritance by copy at attach, as PL/I (F) does (PLM p. 241).
  `pl1_goto` then works within one task's stack only, which is what it
  assumes (`rt/pl1cond.c:139-155`). Copied entries are marked so that
  no frame comparison ever unlinks them.
- No stdio locks: one TCB, and `@@flock` keeps its fast path because
  no subtask exists (`@@flock.c:33-50`).
- One ESTAE, one SPIE, as today. `PIESAVE`/`PIERES` are per context.

Limits, each a deviation to document:

- No concurrency. A task in synchronous I/O, or computing without a
  switch point, holds the others. A loop that polls `COMPLETION(E)`
  for another task to set never ends. (On a one-CPU stand PL/I (F)
  behaves the same for a poller at equal or higher priority, since MVS
  does not time-slice within an address space by default; **unverified**
  for the TK5 settings.) A switch point in COMPLETION and STATUS would
  cure the poller at a small cost.
- An abend in any task ends the program, not the task. Recovering a
  task through ESTAE retry into the scheduler is possible in principle
  and is the riskiest part of B; leave it out at first.
- EVENT I/O overlaps nothing (it does not in tiers 0-1 either).

### 6.3 Option C: run to completion

CALL with a tasking option runs the procedure at once on the caller's
stack, as an ordinary call; at its end the event is set complete. This
is a legal schedule as long as the subtask never waits for something
its attacher does later. A WAIT that can never be satisfied ends the
program with a message. No stacks, no switch, no per-task state beyond
the event. Useful as the first tasking tier of B (B's tier 2 below),
not as the end state: the LRM's own example does not run under it.

### 6.4 Comparison

| | A ATTACH | B cooperative | C run to completion |
|---|---|---|---|
| Generated code / ABI change | yes | no (block-end call only) | no |
| Run-time refactoring | all task cells behind an anchor | task cells in one struct, copied at a switch | almost none |
| libc370 changes | priority in ATTACH, documented anchor, maybe a driver | none | none |
| Concurrency, real priority | yes | no; priority order at switch points | no |
| Abend in a subtask | ends the subtask | ends the program | ends the program |
| stdio cost | ENQ per call once a subtask exists | none | none |
| Recovery code per task | yes | no | no |
| Programs that run | all | all without busy-waiting or abending subtasks | only those whose subtasks never wait for the attacher |
| Twin-test determinism | only with forced ordering | deterministic | deterministic |

**Recommendation (for the maintainer's decision):** tiers 0 and 1
first; they need no decision on A or B. Then B, starting with C as its
first step. The refactoring that gathers the per-task cells into one
structure is the first step of A as well, so A stays open.

## 7. Termination and abends (any design with tasks)

- **Normal end of a subtask** (END/RETURN of its first procedure):
  close the files it opened, free its CONTROLLED generations and its
  BASED allocations (needs an owner tag per allocation; today
  `pl1_allocate` has none, `rt/pl1rt.c:237-243`), set its event
  complete. Active subtasks of its own end abnormally, with IHE577I if
  PL/I (F) writes it there (PG p. 172; probe).
- **Block end with active subtasks**: the run time must know the tasks
  attached in each block. PL/I (F) keeps the chain in a DSA word
  (PLM p. 237). PL1C/370 can do the same with a new `PL1_FR_*` word
  (an ABI stamp change), or keep a side list keyed by the attaching
  frame's address (no ABI change; the choice assumed for B in 6.2).
  `PL1BEXT` is emitted only for blocks with on-units (`src/gen.c:6306-6311`);
  a block with a CALL ... TASK needs it too, and `pl1_goto` must end the
  tasks of every block it leaves.
- **EXIT** in a subtask: FINISH is not raised (LRM p. 377 says FINISH
  only for the major task); the task and its descendants end with
  status 1. EXIT in the major task stays `PL1STOP`. The parser must
  stop mapping EXIT to STOP (`src/parse.c:3645-3650`).
- **STOP** in a subtask: FINISH (in which task: probe P6), then the
  whole program ends with step code 1000 + PLIRETC. In A the control
  task does the `exit()`; in B the scheduler does.
- **ERROR without an on-unit** in a subtask: the message, no FINISH, the
  task ends with status 1, the program goes on (PG p. 173 item 4).
  `terminate()` (`rt/pl1cond.c:372-380`) must branch on "major task".
- **Abend**: A: the subtask's ESTAE writes the message, MVS ends the
  TCB, `termecb` carries the code. B: today's IHE012I path, the program
  ends. The abend exit must not close SYSPRINT from a subtask
  (`rt/pl1cond.c:287`; the subtask did not open it, LRM p. 198).
- **The major task ends while subtasks run**: they end abnormally
  (LRM p. 200 item 5). PL/I (F) writes IHE577I (PG p. 496); probe P8.

## 8. Tiers, twin tests and probes

### Test method for tasks

- Twins compare SYSPRINT byte for byte. Output from two tasks
  interleaves in an order nobody controls in PL/I (F), even with its
  PUT-level ENQ (PLM p. 24). Every twin forces the order: only one task
  writes, or each PUT follows a WAIT that orders it.
- A wrong WAIT hangs the job, and with it a 50-program run
  (`tests/run_mvs.py:59`). Neither `tests/run_mvs.py` nor
  `tests/submit.py` puts `TIME=` on a step (grep, no match).
  `TIME=` limits CPU time and does not end a waiting step; that is
  MVS's job wait limit (S522, the stands' SMF JWT value, **unverified**).
  Prerequisite: tasking twins and probes run in jobs of their own, or
  each step gets a wait limit, before the first one is submitted.
- Multitasking is PL/I (F) language, level F: every twin must pass under
  `-lang=F`. `docs/language-levels.md:300-306` (EXCLUSIVE "locks
  nothing", DISPLAY EVENT refused) is rewritten, not appended to.
- Probes go through `tests/submit.py` on MVSTK5-BLD
  (the project rule: MVSTK5-BLD takes work, MVSTK5-REF and MVSCE-DEV do not). None is submitted by this note.

### Tier 0: event variables without tasks

Compiler: the EVENT type (32 bytes, fullword aligned, as PLM p. 239, so
that a structure containing one has PL/I (F)'s layout); contextual
declaration from WAIT and EVENT options; COMPLETION and STATUS as
built-ins and pseudo-variables, element and array; event assignment,
element and array; WAIT with and without the count; DELAY; the
conversion refusals. Run time: WAIT on events that are complete, or
fail as PL/I (F) fails; DELAY with STIMER.

Probes first:

- **P0** (read-only, no job): list SYS1.PL1LIB for IHETSA, IHESUB,
  IHETSW, IHETEV, IHETPR, IHETCV, IHEOCT, IHECTT, IHEPTT, IHEIBT,
  IHEDDT, IHEINT (PLM pp. 54, 62-63). PL/I (F) multitasking needs MVT
  (PG p. 163); whether TK5's library carries it, and whether it works on
  MVS 3.8j, is **unverified**. If the modules are missing, tiers 2-4
  have no oracle.
- **Q1EVT**: an EVENT program without OPTIONS(TASK): set COMPLETION and
  STATUS, assign events, PUT both values, WAIT on a complete event,
  `WAIT (E1,E2)(1)` with one complete. Does it link without IHETSA?
  The step code.
- **Q2EVT**: WAIT on an inactive incomplete event (IHE550I, PG p. 492):
  ERROR or a hang? ONCODE, message text. (Run alone, wait limit.)
- **Q3EVT**: the count expression 0, negative, larger than the list.
- **Q4EVT**: the compile messages for event conversion, `E = '1'B`,
  UNSPEC(E), an EVENT parameter, an UNALIGNED EVENT, PRIORITY without
  argument on the right-hand side.
- **Q5EVT**: DELAY(0), DELAY(-1), DELAY(10.7); STORAGE-free checks only.

Twins: `t2xxevt` (Q1EVT's program), `t2xxwtc` (WAIT counts),
`t2xxdly` (DELAY returns); a `.opt.pli` is not needed (level F).

### Tier 1: DISPLAY ... EVENT and EVENT I/O, still one task

- DISPLAY ... REPLY ... EVENT: the ECB moves from the stack
  (`rt/pl1dsp.c:44`) into the event variable, no `wait1`, and WAIT
  waits for it. The reply area must stay valid until the WAIT.
- EVENT I/O: perform the transmission at the statement, keep its
  conditions in the event, raise them at the WAIT in PL/I (F)'s order
  (LRM p. 120; p. 393 rules 5-7). Observably the same as PL/I (F)
  except timing.
- UNBUFFERED is accepted and behaves as BUFFERED today
  (`docs/language-levels.md`, UNBUFFERED entry); EVENT needs the
  attribute checks LRM p. 120 lists.

Probes first:

- **Q1DSE**: DISPLAY ... REPLY ... EVENT, a PUT, then WAIT and PUT the
  reply (a `.reply` test). And the same program ending without the
  WAIT: IHE575I (PG p. 495) or nothing for the major task?
- **Q1EIO**: READ ... EVENT on an UNBUFFERED CONSECUTIVE file to its
  end: is ENDFILE raised at the WAIT, not at the READ? ONCODE, ONFILE.
- **Q2EIO**: a DIRECT REGIONAL(1) READ ... KEY ... EVENT with a missing
  key: KEY at the WAIT; ONKEY.
- **Q3EIO**: reusing an active event in a second READ (IHE557I), and
  CLOSE with an event in progress (LRM p. 338 rule 5g): the event's
  values afterwards.
- **Q4EIO**: EVENT on a BUFFERED SEQUENTIAL file: compile-time or
  run-time refusal, which message.

Twins: one per probe that passes, with `.dd` and `.reply` files as the
runner already supports (`tests/run_mvs.py` docstring).

### Tier 2: CALL with tasking options, run to completion (option C)

Compiler: the TASK type (28 bytes, PLM p. 243); `OPTIONS(TASK)`
accepted; CALL options TASK, EVENT, PRIORITY; EXIT separated from STOP.
Run time: the task variable and event bookkeeping, the procedure called
synchronously, the event set at its end; arguments copied (the list and
the dummy arguments, which live in the caller's frame,
`src/gen.c:13-15`).

Probes first:

- **Q1TSK**: `OPTIONS(MAIN,TASK)`; `CALL P TASK(T) EVENT(E); WAIT(E);`
  P sets a STATIC; the main PUTs it. Step code. The smoke test of
  PL/I (F) multitasking on MVS 3.8j.
- **Q2TSK**: the same without OPTIONS(TASK) (assumed, PG p. 164?).
- **Q3TSK**: arguments: a variable, an expression (dummy), the parent's
  AUTOMATIC through the static link.
- **Q4TSK**: two tasks, `WAIT (E1,E2)(1)`, then `(2)`; COMPLETION and
  STATUS of both after.
- **Q5TSK**: EXIT in a subtask: STATUS 1, COMPLETION '1'B, the program
  goes on; EXIT in the major task: FINISH and step code 1000.
- **Q6TSK**: STOP in a subtask with a FINISH on-unit in both tasks:
  which one runs (LRM p. 320 against PG p. 173), step code.
- **Q7TSK**: ERROR in a subtask without an on-unit: the message text,
  no FINISH, STATUS 1, the main goes on (PG p. 173).
- **Q8TSK**: inherited on-units: ON CONDITION(X) before the CALL,
  SIGNAL in the subtask; REVERT in the attacher after the CALL does not
  affect the subtask.
- **Q9TSK**: CONTROLLED: the subtask sees the latest generation; its
  own ALLOCATE is not seen by the attacher; its FREE of the attacher's
  generation does nothing (LRM pp. 197-198).
- **Q10TSK**: the tasking error messages one by one: active task
  variable (IHE551I), active event in CALL (IHE557I), COMPLETION of an
  active event (IHE554I), assignment to an active event (IHE556I).

Twins: Q1, Q3, Q4, Q5, Q7, Q8, Q9 and Q10 programs, all written so that
the subtask never waits for its attacher (C's limit). A twin that needs
a wait goes to tier 3.

### Tier 3: real switching (option B), or ATTACH (option A)

Per-task stacks, the switch, the scheduler, WAIT on task events that
are not complete, DELAY and reply waits as switch points, block end and
GO TO terminating tasks, PRIORITY.

Probes first:

- **Q11TSK**: a producer and a consumer handing over through two events
  (each WAITs for the other); the order of PUTs forced.
- **Q12TSK**: the end of the attaching block while the subtask waits:
  IHE577I text and the subtask's event (PG p. 496).
- **Q13TSK**: the major task ends while a subtask waits.
- **Q14TSK**: PRIORITY: `CALL P TASK(T) PRIORITY(n)` for n = -2, 0, 2,
  50, then `PRIORITY(T)`; `PRIORITY(T) = k` on an inactive T, then the
  CALL without PRIORITY; `PRIORITY = -7;` without an argument
  (PLM p. 62; PG p. 167). This pins the starting value under MVS 3.8j,
  where the `16 * PRTY + 10` formula (LRM p. 197) is MVT-era and
  **unverified**.
- **Q15TSK**: SNAP in a subtask: the task name in brackets after the
  entry point (PG p. 168).
- **Q16TSK**: DISPLAY ... REPLY without EVENT in a subtask while the
  major task goes on (the BANKER pattern, LRM p. 202).
- **P2** (assembler, `.asmsub`-style, not PL/I): a mother that SPIEs,
  an ATTACHed daughter that divides by zero: does the daughter get the
  mother's exit or S0C9? Needed for A only.

### Tier 4: task-end cleanup and EXCLUSIVE between tasks

Files opened in a task closed at its end, allocations freed, record
locks between tasks (READ on a record another task locked waits,
LRM p. 199 Fig. 15-2), I/O events of an ended task set abnormal.

Probes first:

- **Q17TSK**: a subtask opens a file and ends without CLOSE; the
  attacher then opens the same file.
- **Q18TSK**: two tasks on one EXCLUSIVE DIRECT UPDATE REGIONAL file:
  READ by task 2 of a record task 1 holds; UNLOCK; the order of
  completion forced by events.
- **Q19TSK**: an abend in a subtask (an `.asmsub` routine that ABENDs
  U0123): IHE571I text (PG p. 494), the main goes on. Under B this
  becomes a documented deviation unless ESTAE retry is built.

## 9. Cost and memory on MVS

Memory is the first constraint. All figures per task, below the line
(MVS 3.8j has nothing above it).

| Item | PL/I (F) | A | B |
|---|---|---|---|
| Fixed overhead for multitasking | about 3.5 KB (PG p. 163); control task area X'4D0' = 1,232 B (PLM p. 235) | control task stack and context; libc370 thread code (estimate 10-12 KB of text, from `internals/size-analysis.md` p10thread 27,666 B total, **unverified split**) | scheduler and switch (estimate 3-5 KB of text, unbuilt) |
| Per subtask, library | about 2 KB (PG p. 163) | CTHDTASK 44 B + CLIBCRT 392 B + task context about 1.3 KB (+12 KB if `ub` is per task) | task context about 1.3 KB |
| Per subtask, MVS | TCB, RB in LSQA | TCB, RB in LSQA (unmeasured) | none |
| Per subtask, stack | DSAs in the region | stack (below) | stack (below) |
| TASK / EVENT variable | 28 / 32 B (PLM pp. 243, 239) | same | same |
| CPU per attach | about 70 ms, model 40 (PG p. 163) | ATTACH + driver | a GETMAIN and a switch |
| stdio once a subtask exists | ENQ per SYSPRINT PUT (PLM p. 24) | ENQ/DEQ per stdio call, about 120 us (`@@flock.c:56-59`) | none |

**Stacks** are the large item. Today the main stack is the sum of all
frames plus a 32 KB reserve, or 256 KB when the compiler cannot bound it
(`src/gen.c:636-645`, `6433-6443`). The reserve covers the run time's
and LIBC/370's frames, measured at most 7,708 bytes over the suite
(`internals/research/pli/startup-measurement.md`, quoted at
`src/gen.c:642-644`). A subtask's stack can be bounded the same way from
the frames reachable from the procedure it starts, with a smaller
reserve. Ten tasks at 16 KB reserve plus their frames cost about 200 KB;
at libc370's default of 64 KB each, 640 KB. When the bound is unknown
(RECURSIVE, ENTRY variables, procedures compiled apart; `src/gen.c:636-640`)
there is no safe small default. That needs a run-time option, as the
Optimizer's ISASIZE second argument (OPT-PG p. 43), or a fixed limit.

**Single-task programs pay nothing** in any option if the tasking code
is reached through a weak reference, the pattern of `@@STKLEN` and
`CTHREAD` (`docs/startup.md:122-128`): the CALL ... TASK entry is in a
member of its own, and the start-up looks for it. That is PL/I (F)'s
IHESAP/IHETSA split (PLM p. 54) in autocall form.

## 10. Risks

- **SYSPRINT.** In A, two tasks in one PUT corrupt the line buffer of
  the pfile; libc370's FILE lock protects only each `fwrite`. The
  statement lock must be released by GO TO out of an on-unit. A
  subtask that opens SYSPRINT first: PL/I (F) warns that messages may be
  lost (PG p. 172).
- **Other shared files.** DCB use from a TCB that did not open it, and
  CLOSE at the end of the opening task while the FILE stays in libc370's
  array: **unverified**; S001/S301 named in PG pp. 170-171.
- **Abends in subtasks** (A): the CLIBCRT of a dead TCB stays in the
  PPA's array; files it opened are closed by MVS under libc370's
  FILE structures. libc370 #11 (DETACH of a live worker) sits in the
  same area.
- **ESTAE and SPIE per task** (A): the most expensive code to get
  wrong; the SPIE save area is static today (`rt/pl1pie.asm`).
- **Arguments.** Dummy arguments and the argument list live in the
  caller's frame; if the attacher goes on, they are overwritten. They
  must be copied into the task. Whether PL/I (F) copies dummies is not
  in the PLM pages read (only the list, PLM p. 241); probe Q3TSK.
- **Frame-address comparisons** in `pl1_goto` and the `acts` chain
  (`rt/pl1cond.c:143`, `150`) are meaningless across stacks; any task
  design must keep each chain within one task.
- **Busy waiting and deadlock** (B): documented limits, not detected,
  unless a switch point in COMPLETION/STATUS is added.
- **Hanging test jobs** without a wait limit (section 8).
- **The oracle itself**: PL/I (F) multitasking was written for MVT; on
  MVS 3.8j it is untried here (P0, Q1TSK).

## 11. Open questions for the maintainer

1. A, B or C as the end state? (Recommendation: B, with C as its first
   step; A only if real concurrency is wanted.)
2. Under B, is "an abend in a subtask ends the program" an acceptable
   documented deviation, or must ESTAE retry into the scheduler be
   built?
3. Under B, should COMPLETION and STATUS be switch points (cures polling
   loops, changes nothing a correct program sees)?
4. A deadlock (every task waiting, no WTOR or timer outstanding): hang
   as PL/I (F) would, or end with a message? Under B it can be
   detected for free.
5. Stack per subtask when the compiler cannot bound it: a run-time
   option (ISASIZE-like, outside LE), a compiler option, or a fixed
   size?
6. Maximum number of tasks: PL/I (F)'s 255 (IHE560I) or less?
7. Under A: may PL1C/370 rely on the CTHREAD stack at +8 of the first
   save area, or does libc370 provide an accessor, an ATTACH with a
   priority, and an ESTAE in the driver? (A libc370 issue.)
8. EVENT and TASK layouts: PL/I (F)'s 32 and 28 bytes so that
   structures match, or smaller? (Recommendation: PL/I (F)'s.)
9. Which JWT or TIME limit for tasking test jobs on MVSTK5-BLD?
10. Are tiers 0 and 1 wanted ahead of the decision? They stand on their
    own.

## 12. Unverified, collected

- CHAP on MVS 3.8j limited to the issuing task and its subtasks.
- Whether an ATTACHed subtask inherits the mother's SPIE (P2).
- DCB use across TCBs and close at the opening task's end with the
  FILE still in libc370's array.
- MVS 3.8j does not time-slice tasks of one address space by default
  on the TK5 stands.
- The starting priority of the major task under MVS 3.8j (Q14TSK).
- SYS1.PL1LIB on TK5 carries the multitasking modules, and they run on
  MVS 3.8j (P0, Q1TSK).
- Sizes: the task context (about 1.3 KB), libc370 thread code (10-12 KB),
  scheduler (3-5 KB): estimates, not counted.
- The job wait limit (S522, JWT) on the stands.
