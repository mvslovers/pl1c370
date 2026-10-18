# Start-up cost and size (#177, step 1: measure first)

Where PL1C/370's programs spend more than PL/I (F)'s, measured
2026-10-16 on MVSTK5-BLD (Hercules 4.10.0).

## Over the whole suite

The step statistics (IEF374I) of the 250 programs of the full suite
(JOB01994 to JOB02012), the PL/I (F) GO step against the PL1C/370 step
of each program:

| | PL/I (F) | PL1C/370 |
|---|---|---|
| CPU per program, median | 0.01 s | 0.03 s |
| CPU, sum over 250 | 5.38 s | 12.98 s |
| VIRT, median | 44K | 452K |
| VIRT, range | 0 to 108K | 420K to 804K |

No PL1C/370 program takes less than 0.02 s or 420K, whatever it does:
the difference is start-up and a fixed amount of storage, not
computation.

## Start-up alone (JOB02015)

One job, each program twice: the four-line `p0hello` (one PUT) under
PL/I (F) and PL1C/370 (`TP0HELLO`), and a C program on LIBC/370 alone
that writes one line (`tests/mvs/chello`), with the default stack
(`CHELLO`) and with a 16 KB one (`CHELLOS`, `__stklen`).

| Program | CPU | VIRT | load module |
|---|---|---|---|
| PL/I (F), PL1LFCLG GO | 0.01 s, 0.01 s | 40K | |
| PL1C/370 `TP0HELLO` | 0.02 s, 0.03 s | 452K | 124 KB of text |
| C on LIBC/370, `CHELLO` | 0.02 s, 0.03 s | 404K | 87 KB file |
| the same, 16 KB stack, `CHELLOS` | 0.02 s, 0.02 s | 164K | 87 KB file |

- **CPU:** a C program on LIBC/370 alone costs what the PL/I one does.
  The extra 0.01 to 0.02 s is LIBC/370's start-up, not PL1C/370's run
  time.
- **VIRT:** 240K of the 452K is the C stack, LIBC/370's default of
  256 KB (`@@STKLEN`/`__stklen` sets it; `CHELLOS` takes 164K).
  PL1C/370's run time adds 48K over the C program.

## The load module (ld370 map of TP0HELLO)

`ld370 --map` of the one-PUT program, the same link as mbt's: 123,951
bytes of text.

- **LIBC/370: 77,657 bytes in 160 members.** stdio (`fopen` with its
  dynamic allocation members `@@tx*`, `@@fpopen`, `@@aopen`, buffers),
  the printf family (`vvprintf`, `vsnprint`, `@@dblcvt`), 64-bit
  arithmetic (`@@64*`), the environment and time zone (`loadenv`,
  `setenv`, `tzset`), WTO and its traceback, `@@examin` (4.7 KB).
  Most of it comes with the start-up itself: the C program that writes
  one line is 87 KB.
- **PL1C/370's run time: 45,948 bytes in 5 members:** `pl1io.o`
  20,772, `pl1cond.o` 14,908, `pl1rt.o` 9,464, `pl1qsam.o` 552. Each is
  linked whole (autocall takes members), so a program with one PUT
  carries all of record I/O, OPEN and CLOSE.
- RXPI (`t228pi`, Tier 82) is 127,694 bytes: its own code is 2,973.

## What follows

In the order of what they save:

1. **The stack** (240K of VIRT per program; done, below): the compiler sets
   `__stklen` to what the program needs. LIBC/370 does not check the
   stack (its Programmer's Guide, "The stack is not checked"), so the
   size must be an upper bound: the frames of the PL/I procedures along
   the deepest call chain, plus the run time's and LIBC/370's own
   frames below them, measured. A recursive procedure or storage of
   unknown size on the stack keeps the default.
2. **LIBC/370's start-up** (the CPU, and most of the 78 KB): a question
   for the libc370 session -- what of `@@RTINIT` (streams, `SYSENV`,
   `tzset`) a program that does not use it could leave out.
3. **The run time in smaller members** (some of the 46 KB): `pl1io.c`
   split so that stream output does not bring record I/O.
4. **SYSPRINT without stdio** helps only after 2: the start-up links
   stdio anyway.

## Step 2: the stack, set by the compiler (done)

**Measured first** with a measuring build of the run time
(`-DPL1_STACKHW`, rt/pl1cond.c): the stack above the main procedure's
first run-time call filled with a pattern, the highest byte used
reported by WTO at the end and on the abend path. Over the suite with
the default stack (JOB02017 to JOB02035, 248 programs; the two that
abend were not reported then):

- most programs use 3 to 5 KB; the largest are `t171big` 69,232
  (RECURSIVE), `t228pi` 16,364, `t227cut` 10,976, `t81ovf` 8,176;
- what the run time and LIBC/370 take below the PL/I frames, the
  stack used less every procedure's frame added up: 1,160 to 7,708
  bytes in the programs without RECURSIVE (`t228pi`, `t81ovf`,
  `t209ans` at the top).

**The rule** (gen.c, `@@STKLEN` in the main procedure's section):
every procedure's frame added up, an upper bound for a call chain that
enters no procedure twice, plus 32 KB for the run time and LIBC/370
(four times the most measured), in 4 KB steps; LIBC/370's 256 KB stays
when the bound is not below it or cannot be seen: a RECURSIVE
procedure, an adjustable AUTOMATIC array (its storage is on the stack
past the frame), a call to a procedure compiled apart, an assembler or
FETCHed routine, or through an ENTRY variable. `-stack=N` sets the
size, `-stack=0` leaves the default.

What the bound does not see: an on-unit entered again while it is
active (an UNDEFINEDFILE on-unit whose OPEN raises it again) recurses
without RECURSIVE; the 32 KB reserve takes some levels of it. A
procedure entered again without RECURSIVE was not valid PL/I (F)
either; under 256 KB such a program may have run by accident, and may
now overrun the stack, which LIBC/370 does not check.

**Checked** with the measuring build and the new sizes (JOB02038 to
JOB02056): every program that got a computed size keeps at least
28,736 bytes of it unused (`t81ovf`: 8,176 of 36,912); the abend path
of `t231abn` used 3,224. 21 programs keep the default.

**Result** (the full suite, JOB02059 to JOB02077, 250 of 250 identical):

| PL1C/370 step | before (JOB01994-02012) | after |
|---|---|---|
| VIRT, median | 452K | 236K |
| VIRT, least | 420K | 200K |
| VIRT, sum over 250 | 111,272K | 61,116K |
| CPU, sum | 12.98 s | 12.95 s |

The CPU is LIBC/370's start-up and does not move (step 2 above; the
libc370 session has a lazy start-up -- the streams, SYSENV and tzset on
first use -- to put to the maintainer).

## The rest (2026-10-16): what is left is LIBC/370's

- **LIBC/370 2.15.0** opens the standard streams on first use: the CPU
  of the opens is gone for a stream the program does not use, but not
  the size. An empty C `main` still links 72,660 bytes in 143 sections,
  `puts` 73,096 (ld370 maps): the start-up's references bring the
  whole of stdio whether a stream opens or not. The one-PUT PL/I
  program is 124,937 bytes, 77,653 of them LIBC/370's.
- **SYSPRINT without stdio** would therefore save nothing: the start-up
  links stdio anyway. Dropped, unless LIBC/370's start-up comes to link
  stdio only when a stream is used (told to the libc370 session).
- **pl1io.c in smaller members:** record I/O is about 300 of its 1,280
  lines, some 5 KB of the 21 KB; everything else serves any program with
  a PUT. Not worth a split.

So PL1C/370's side of #177 is done: the stack (step 2) was the lever in
its hands. The start-up's size is LIBC/370's, with the maintainer.
