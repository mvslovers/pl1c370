# PL1C/370 (pl1c370)

A PL/I compiler for MVS 3.8j, in the making. PL1C/370 is meant to run on the
host (macOS, Linux) and translate PL/I source into S/370 assembler, which as370
assembles and ld370 links into an MVS load module, with a runtime of its own on
top of LIBC/370. It follows the shape of the COBOL compiler cobc370.

**Status: Tier 109.** Programs with internal procedures and stream I/O
compile and run on MVS: the arithmetic of both bases with PL/I (F)'s
precision rules (division and scaled binary included), strings, bit
strings (long and VARYING too), the common built-in functions (string, arithmetic and
conversion ones, and the mathematical ones, correctly rounded or
nearly so) and `**`, arrays and structures (with LIKE and DEFINED), PICTURE, label variables, the preprocessor (%INCLUDE, % variables, %IF, %DO, % procedures), procedures
with arguments (procedures and files among them), functions, recursion and nested scopes, GET/PUT in
LIST, EDIT and DATA form on SYSIN, SYSPRINT, other files and strings, record I/O (with UPDATE and REWRITE, and KEYED on REGIONAL(1), (2), (3) and INDEXED data sets) with
structures mapped as PL/I (F) maps them, FLOAT with PL/I (F)'s own
conversions, BEGIN blocks, BASED variables with pointers, ALLOCATE and
FREE, AREA and OFFSET variables, CONTROLLED variables, and conditions with on-units
(ZERODIVIDE, FIXEDOVERFLOW, OVERFLOW, UNDERFLOW, CONVERSION, ENDFILE, ENDPAGE,
UNDEFINEDFILE, RECORD, KEY, AREA, ERROR, FINISH,
CONDITION, SIZE, SUBSCRIPTRANGE, STRINGRANGE, and condition prefixes) down to PL/I (F)'s messages and step code, the sort program interface and checkpoint, and the first part of COMPLEX. Two hundred and seventy-eight test
programs print -- and write -- byte for byte what PL/I (F) does for the
same source and input, and end with the same condition code; only the code
offset in a message of the system action differs, since it belongs to each
compiler's code. See [`internals/ROADMAP.md`](internals/ROADMAP.md).

## Building and testing

```
make                    # the compiler: build/host/pl1c370
make test               # host test of the run time against PL/I (F) output
build/host/pl1c370 [-I incdir] prog.pli -o prog.asm
tests/run_mvs.py        # compile tests/pli/*.pli, build, deploy, and compare
                        # with PL/I (F) on MVS (needs mbt 3 and a target)
mbt build               # the run time: build/pl1rt.a
make install [PREFIX=dir]
                        # both into the cc370 sysroot (default ~/.local):
                        # bin/pl1c370, cc370/lib/pl1rt.a, cc370/include/pl1abi.h
```

## Options

| Option | Effect |
|---|---|
| `-lang=F` | PL/I (F)'s language only |
| `-langlvl=SPROG` | pointer arithmetic, POINTERADD, POINTERVALUE, BINARYVALUE |
| `-plist` | R1 to the main procedure unchanged (PL/I (F)'s `ENTRY IHESAPB`) |
| `-char48` | the 48-character set |
| `-interrupt` | the Optimizer's INTERRUPT: under a TSO terminal an attention raises ATTENTION (also `*PROCESS INTERRUPT;`) |
| `-stack=N` | the C stack in bytes (`K` for kilobytes); `0` leaves LIBC/370's default of 256 KB. Without it the compiler sets the stack to the frames of the program's procedures added up and 32 KB, when it can see them all: not with RECURSIVE, an adjustable AUTOMATIC array, or a call to a routine compiled apart, an assembler routine or an ENTRY variable. LIBC/370 does not check the stack |
| `-I dir` | a directory for `%INCLUDE` |
| `-MD [-MF file]` | a make dependency file |

## Why

The only PL/I compiler that runs on a hobbyist MVS 3.8j system is IBM's
OS/360 PL/I (F), Version 5.5, from the early 1970s. Its successors — the OS PL/I
Optimizing and Checkout Compilers and OS PL/I Version 2 — are licensed IBM
program products and are not legitimately obtainable. No free PL/I compiler
targets S/370.

## Language level

By default, the language of IBM PL/I for MVS & VM as its manuals define
it (SC26-3114-01, SC26-3113-01), without what needs Language
Environment and without DBCS, with some extensions from the standard
ANSI X3.53-1976. It includes the language of the OS PL/I Optimizing
Compiler. With `-lang=F`, exactly the language of PL/I (F): anything
beyond it is an error.

Everything PL/I (F) shares is tested differentially against PL/I (F) on
the target system, compiled with `-lang=F`. The features beyond it are
tested against PL/I (F) running the expansions and the explicit
declarations the later IBM manuals give.
[`docs/language-levels.md`](docs/language-levels.md) lists each feature
with its level, and every deviation.

## Documentation

| Where | What |
|---|---|
| [`docs/language-levels.md`](docs/language-levels.md) | the language levels (F, OPT, MVS, ISO), what is beyond PL/I (F), the deviations |
| [`internals/ROADMAP.md`](internals/ROADMAP.md) | target level, exclusions, tiers, test method (draft) |
| [`internals/research/pli/`](internals/research/pli/) | IBM PL/I timeline to 1987, standards, manuals, prior art |
| [`internals/research/pls/`](internals/research/pls/) | IBM's PL/S systems language and the PL/S output in the MVS 3.8j source, for a possible later PL/S dialect |

## License

MIT, see [`LICENSE`](LICENSE).
