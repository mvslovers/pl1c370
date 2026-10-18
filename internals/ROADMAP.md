# PL1C/370 roadmap — DRAFT

This is the plan for the language PL1C/370 will compile. It is a **draft**:
unlike cobc370's roadmap, nothing below has been checked against a compiler,
because there is none yet. It is derived from the research in
`research/pli/report.md` and will be revised when the first measurements on
PL/I (F) come in.

Issue and PR numbers: this repository started with a fresh history on
2026-10-18. The open issues moved here as #1 to #11 (#1 was #179, #2 to
#7 were #227 to #232, #8 #236, #9 #255, #10 #268, #11 #275); every other
number in notes written before that date (#177, PR #254, ...) is one of
the predecessor repository, mvslovers/pl1c370-archive (private).

## Tier 0 -- done 2026-10-12

The first slice through the whole pipeline: source -> `pl1c370` -> S/370
assembler with CC/370 linkage -> as370/ld370 with the run time `rt/` on
LIBC/370 -> MVS, compared with PL/I (F) on the same source.

Language: one main procedure; scalar FIXED BINARY(p), FIXED DECIMAL(p,q),
CHARACTER(n) [VARYING], AUTOMATIC and STATIC, INITIAL; implicit FIXED
BINARY(15) for undeclared I-N names; assignment with the conversions
between them; `+ - *` and prefix minus with the PL/I (F) precision rules
(a decimal integer meeting a binary operand becomes binary); comparisons,
`&` and `|` in conditions; IF/THEN/ELSE; DO groups, DO WHILE, iterative DO
with a FIXED BINARY control variable and constant BY; PUT SKIP[(n)], PAGE
and LIST; LENGTH.

**Eight programs, every one printing byte for byte what PL/I (F) prints**
(`tests/pli/`, JOB00069 on MVSTK5-BLD, `tests/run_mvs.py`): the hello and
PUT LIST probes, binary and decimal arithmetic, conditions, loops,
strings, and implicit declaration with SKIP(3)/SKIP(4) and decimal
operands meeting binary ones.

Deliberately not in Tier 0, each refused with a message: division (its
precision rules are a tier of their own), FLOAT, mixed binary/decimal with a
fraction (scaled binary), arrays, structures, internal procedures, `||`,
NOT, PUT EDIT/DATA, ON-units.  Known gaps of the generated code: one base
register (code and constants together under 4 KB), frame under 4 KB, no
FIXEDOVERFLOW or SIZE detection.

## Tier 1 -- done 2026-10-12

The scalar expressions completed (#3), every rule measured first
(`research/pli/tier1-measurement.md`):

- division in both bases with the PL/I (F) precision rules, scaled binary
  results in arithmetic, assignment, comparison and output, decimal
  fractions meeting binary operands;
- `||`, the NOT operator (sent to PL/I (F) as Latin-1 `¬`), BIT(n) up to 32
  bits with constants, `&` `|` NOT bit by bit, comparisons as BIT(1)
  values, bit strings in IF and PUT LIST;
- numbers converted to strings in assignment and concatenation;
- SUBSTR, INDEX, MOD (integers), ABS, MAX, MIN.

**Thirteen programs identical to PL/I (F)** in one job (JOB00077),
Tier 0's eight and five for Tier 1.

Still refused: MOD with fractions, SUBSTR as a pseudovariable, bit strings
over 32 bits, conversions from bit or character to arithmetic, ON-units.

## Tier 2 -- done 2026-10-12

Arrays and structures (#5), every rule measured first
(`research/pli/tier2-measurement.md`):

- arrays of up to eight dimensions with constant bounds, any lower bound;
  subscripts are expressions;
- structures with level numbers, arrays as members, arrays of structures;
  qualified and partially qualified names, subscripts at any level;
- INITIAL lists with iteration factors, AUTOMATIC and STATIC;
- aggregate assignment: scalar to array or structure, array expressions
  element by element, structure to structure by position; aggregates in
  PUT LIST in PL/I (F)'s order;
- DIM, LBOUND, HBOUND.

The compiler does it in a pass of its own, `src/lower.c`, which turns every
aggregate into DO loops and scalar statements before type checking.

**Sixteen programs identical to PL/I (F)** in one job (JOB00087).

Still refused: `*` cross-sections, BY NAME, LIKE, adjustable bounds, array
expressions in PUT LIST.

## Tier 3 -- done 2026-10-12

Procedures (#7), every rule measured first
(`research/pli/tier3-measurement.md`):

- internal procedures, nested, with their own declarations, seeing the
  variables of the blocks around them (static links between frames);
- CALL; arguments by reference, dummy arguments converted to the
  descriptors of an ENTRY declaration; a mismatch PL/I (F) would pass
  unconverted is refused instead of reproduced;
- functions with RETURNS, typed by the invoking block's declaration;
  RETURN with and without a value; RECURSIVE; arrays passed whole;
- labels and GO TO within a procedure.

A new pass, `src/resolve.c`, binds names across the block structure after
parsing, so procedures and declarations may follow their use.

**Twenty programs identical to PL/I (F)** in one job (JOB00100). The
binary MOD of Tier 1 is corrected on the way.

Still refused: GO TO out of a block, `*` parameter bounds and lengths,
structures as arguments, external procedures, ENTRY variables, BEGIN
blocks, ON-units.

## Tier 4 -- done 2026-10-12

Stream input and output (#9), every rule measured first
(`research/pli/tier4-measurement.md`):

- PUT EDIT with A, A(w), F(w[,d]), X, COL, SKIP, PAGE, LINE; iteration
  factors and groups; the format list restarting; replicated strings;
- GET LIST and GET EDIT from SYSIN and other files;
- FILE declarations, OPEN with TITLE, CLOSE, PUT/GET FILE(...); files that
  are not PRINT, and the 60-line pagination of PRINT files;
- ON ENDFILE with a statement or a BEGIN block, GO TO out of it.

The run time has a file model of its own now (`rt/pl1io.c`).

**Twenty-four programs identical to PL/I (F)** in one job (JOB00114),
the second output file and the input data included.

Still refused or missing: RECORD files, PUT DATA, the STRING option,
ENVIRONMENT, other conditions than ENDFILE, on-units of a calling
procedure.

## Tier 5 -- done 2026-10-12

Record I/O and conversion (#11), every rule measured first
(`research/pli/tier5-measurement.md`):

- RECORD files, READ INTO and WRITE FROM on sequential data sets, ON
  ENDFILE for READ;
- structures mapped as PL/I (F) maps them (pairwise, with a doubleword
  phase), so the records written are byte for byte the same;
- the record form of STREAM files that are not PRINT (a blank after each
  LIST item, at least 10 bytes);
- character strings converted to arithmetic in assignment, expressions and
  comparisons.

**Twenty-eight programs identical to PL/I (F)** in one job (JOB00136),
second output file compared in its bytes.

Still missing: REWRITE/UPDATE, keyed and VSAM files, ENVIRONMENT, the
RECORD condition, BIT strings in mapped structures.

## Tier 6 -- done 2026-10-12

Conditions and on-units (#13), every rule measured first
(`research/pli/tier6-measurement.md`):

- ON, REVERT and SIGNAL for ZERODIVIDE, FIXEDOVERFLOW, CONVERSION,
  ENDFILE, ERROR, FINISH and CONDITION(name); ON ... SYSTEM;
- on-units established dynamically: inherited by called procedures, ended
  with the block that established them; each on-unit a block of its own,
  so it runs from a called procedure or from inside the run time;
- GO TO out of an on-unit or an internal procedure to a label of an
  enclosing block;
- ONCODE, ONCHAR and ONSOURCE, the last two also as pseudovariables that
  change the source field, with the conversion done again;
- the system action: PL/I (F)'s messages on SYSPRINT, placed as it places
  them, ERROR, FINISH and the step code 2000;
- ZERODIVIDE and FIXEDOVERFLOW checked in the generated code, including
  PL/I (F)'s in-place decimal addition;
- character constants that are no number refused at compile time.

The run time has `rt/pl1cond.c` for the conditions. The test runner also
compares the condition codes of both steps, and masks the code offset in
the messages (it belongs to each compiler).

**Forty-two programs identical to PL/I (F)** in one job (JOB00155).

Still missing: condition prefixes and SIZE, SNAP, STOP/EXIT, CONVERSION in
GET EDIT, DEC to BIN(31) beyond 2**31 (PL1C/370 abends 0C9 where PL/I (F)
keeps the low 32 bits).

## Tier 7 -- done 2026-10-12

FLOAT (#15), every rule measured first (`research/pli/tier7-measurement.md`):

- FLOAT DECIMAL and FLOAT BINARY, short and long; bare FLOAT, DECIMAL or
  BINARY alone, and the implicit FLOAT DECIMAL(6) of names outside I-N;
- constants with an exponent, + - * /, prefix minus, comparisons, ABS,
  MAX, MIN, and the base and precision rules of mixed FIXED and FLOAT;
- every conversion as PL/I (F)'s library does it, bit for bit as measured
  (`rt/pl1flt.c`, shared with the compiler for constants);
- PUT LIST, PUT EDIT with E(w,d[,s]) and F(w,d), GET LIST and GET EDIT;
- ZERODIVIDE and UNDERFLOW (products and quotients) for FLOAT; FLOAT in
  structures and records.

**Forty-nine programs identical to PL/I (F)** in one job (JOB00170), a
record with FLOAT members compared in its bytes.

Still missing: OVERFLOW (an exponent overflow abends 0CC; ON OVERFLOW is
refused), `**`, the mathematical built-in functions, MOD of FLOAT.

## Tier 8 -- done 2026-10-12

Blocks and storage (#17), every rule measured first
(`research/pli/tier8-measurement.md`):

- BEGIN blocks with their own declarations, on-units and labels, and GO
  TO out of them;
- STOP and EXIT: FINISH, then step code 1000;
- POINTER, NULL, ADDR; BASED scalars, arrays and structures over a
  pointer; P -> X; ALLOCATE [SET] and FREE; pointers as parameters.

**Fifty-four programs identical to PL/I (F)** in one job (JOB00181),
among them a sorted list built with ALLOCATE from SYSIN.

Still missing: RETURN out of a BEGIN block, INITIAL on BASED, chained
pointer qualifiers, AREA/OFFSET, CONTROLLED, the STORAGE condition.

## Tier 9 -- done 2026-10-12

Built-in functions and exponentiation (#19), every rule measured first
(`research/pli/tier9-measurement.md`):

- VERIFY, TRANSLATE, REPEAT, HIGH, LOW, UNSPEC (up to 32 bits);
- SIGN, FLOOR, CEIL, TRUNC, ROUND; FIXED, FLOAT, BINARY, DECIMAL,
  PRECISION, ADD, MULTIPLY, DIVIDE with PL/I (F)'s defaults and argument
  rules;
- `**` with an integer exponent: FIXED while (p+1)*n-1 fits, else FLOAT
  as PL/I (F)'s library computes it (squaring and multiplying, measured).

**Fifty-eight programs identical to PL/I (F)** in one job (JOB00194).

Still missing: `**` with other exponents and the mathematical functions,
UNSPEC beyond 32 bits; and a procedure of more than 4096 bytes of code.

## Tier 10 -- done 2026-10-12

Procedures beyond 4096 bytes of code (#21). Each procedure now has three
base registers (R12, R11, R10; expression code uses R2 to R9), so its code
and literals may take 12 KB; GO TO out of a block sets all three for the
label's block. The compiler estimates a procedure's size and refuses one
beyond 12 KB with a message instead of the assembler's addressability
errors.

**Fifty-nine programs identical to PL/I (F)** in one job (JOB00196), among
them `t57long`: 10.5 KB of code in one procedure, with GO TO across all of
it and out of a called procedure.

Still missing: procedures beyond 12 KB, AUTOMATIC storage beyond 4095
bytes.

## Tier 11 -- done 2026-10-12

Data-directed and string I/O (#23), every rule measured first
(`research/pli/tier11-measurement.md`):

- PUT DATA with and without a list, GET DATA with PL/I (F)'s fully
  qualified names and its NAME and end-of-file messages;
- PUT STRING with LIST, EDIT and DATA; GET STRING with LIST, EDIT and
  DATA.

**Sixty-three programs identical to PL/I (F)** in one job (JOB00209).

Still missing: ON NAME, GET DATA of multi-dimensional arrays,
STRINGSIZE.

## Tier 12 -- done 2026-10-12

Condition prefixes (#25), every rule measured first
(`research/pli/tier12-measurement.md`):

- prefixes on statements, procedures and BEGIN blocks, the NO forms;
- SIZE, SUBSCRIPTRANGE and STRINGRANGE, with ON, SIGNAL and REVERT, their
  messages and what the target or SUBSTR holds afterwards;
- NOZERODIVIDE, NOFIXEDOVERFLOW, NOCONVERSION, NOUNDERFLOW.

**Sixty-seven programs identical to PL/I (F)** in two jobs (JOB00222,
JOB00223): the test runner now splits at 255 steps per job.

## Tier 13 -- done 2026-10-12

The mathematical built-in functions (#27), measured first
(`research/pli/tier13-measurement.md`):

- SQRT, EXP, LOG, LOG10, LOG2, SIN, COS, TAN, SIND, COSD, TAND, ATAN and
  ATAND (one and two arguments), short and long, computed by the run
  time (`rt/pl1m*.c`, in C since 2026-10-18, see the entry at the end);
- `x ** y` with a FLOAT or scaled exponent;
- their errors, messages and ONCODEs, and OVERFLOW from EXP, with ON
  OVERFLOW and its normal return.

**Seventy-eight programs identical to PL/I (F)** in two jobs (JOB00250,
JOB00251).

Still missing: SINH, COSH, TANH, ATANH, ERF, ERFC (next tier); OVERFLOW
in arithmetic, which still abends 0CC (only EXP raises it).

## Tier 14 -- done 2026-10-12

SINH, COSH, TANH, ATANH, ERF and ERFC (#29), short and long
(`research/pli/tier14-measurement.md`), with ATANH's error.

**Eighty-two programs identical to PL/I (F)** in two jobs (JOB00257,
JOB00258).

Still missing: a program-check exit -- OVERFLOW in arithmetic, and the
interrupts a library routine meets after a normal return of ON
OVERFLOW, still abend 0CC.

## Tier 15 -- done 2026-10-12

A program interruption exit (#31, `research/pli/tier15-measurement.md`):
OVERFLOW from an exponent overflow anywhere -- compiled arithmetic or a
library routine -- with ON OVERFLOW, its normal return and NOOVERFLOW,
and ZERODIVIDE from a divide exception in a routine; the program goes on
with every register as it was.

**Eighty-five programs identical to PL/I (F)** (JOB00269, JOB00270,
JOB00272).

Still missing: OVERFLOW in conversions (emulated in C), NOZERODIVIDE
inside a library routine.

## Tier 16 -- done 2026-10-12

LIKE and DEFINED (#33, `research/pli/tier16-measurement.md`): LIKE with
PL/I (F)'s restrictions; string overlay with POSITION, correspondence and
iSUB defining.

**Eighty-eight programs identical to PL/I (F)** in two jobs (JOB00285,
JOB00286).

Still missing: BIT VARYING, bit strings over 32 bits, BIT defined inside
a byte, DEFINED on structures.

## Tier 17 -- done 2026-10-13

PICTURE (#35, `research/pli/tier17-measurement.md`): numeric character
data with PL/I (F)'s editing (zero suppression, insertion, static and
drifting signs and currency, CR/DB, overpunch), its two values by
context, character pictures with CONVERSION, the P format item.

**Ninety-four programs identical to PL/I (F)** (JOB00300, JOB00301,
JOB00303).

Still missing: floating-point and scaled pictures, GET into pictures.

## Tier 18 -- done 2026-10-13

Label data (#37, `research/pli/tier18-measurement.md`): LABEL variables,
arrays and parameters, GO TO through them out of procedures, on-units
and BEGIN blocks and into the right activation; labels on END.

**Ninety-six programs identical to PL/I (F)** in two jobs (JOB00313,
JOB00314).

Still missing: INITIAL of labels, ENTRY and FILE parameters.

## Tier 19 -- done 2026-10-13

%INCLUDE (#39, `research/pli/tier19-measurement.md`), members read from
`-I` directories, nested; the test runner builds PL/I (F)'s include
library with IEBUPDTE.

**Ninety-seven programs identical to PL/I (F)** in two jobs (JOB00318,
JOB00319).

Still missing: the rest of the preprocessor.

## Tier 20 -- done 2026-10-13

CONTROLLED storage (#41, `research/pli/tier20-measurement.md`):
generations stacked by ALLOCATE, INITIAL each time, FREE, ALLOCATION.

**Ninety-nine programs identical to PL/I (F)** in two jobs (JOB00334,
JOB00335).

Still missing: CONTROLLED with bounds at ALLOCATE, CONTROLLED
parameters.

## Tier 21 -- done 2026-10-13

Long and VARYING bit strings (#43, `research/pli/tier21-measurement.md`):
assignment, `& | ¬ ||`, SUBSTR, comparison, conditions, LENGTH, PUT, and
bits to CHARACTER and FIXED BINARY.

**A hundred and one programs identical to PL/I (F)** in three jobs
(JOB00342 to JOB00344).

Still missing: GET of long bit strings, UNSPEC beyond 32 bits, the B
format.

## Tier 22 -- done 2026-10-13

ENTRY and FILE parameters (#45, `research/pli/tier22-measurement.md`):
procedures and files as arguments; PRINT records in their bytes as PL/I
(F) writes them.

**A hundred and three programs identical to PL/I (F)** in three jobs
(JOB00352 to JOB00354).

## Tier 23 -- done 2026-10-13

UPDATE files and REWRITE (#47, `research/pli/tier23-measurement.md`),
through QSAM in update mode (`rt/pl1qsam.asm`); each OPEN with its own
attributes.

**A hundred and five programs identical to PL/I (F)** in three jobs
(JOB00365 to JOB00367).

Still missing: keyed and direct access, DELETE, UNDEFINEDFILE.

## Tier 24 -- done 2026-10-13

File conditions (#49, `research/pli/tier24-measurement.md`): ENDPAGE
with PAGESIZE and on-units for page headings, UNDEFINEDFILE, RECORD,
ONFILE.

**A hundred and seven programs identical to PL/I (F)** in three jobs
(JOB00375 to JOB00377).

## Tier 25 -- done 2026-10-13

SUBSTR as a pseudovariable, CHAR and BIT (#51,
`research/pli/tier25-measurement.md`).

**A hundred and eight programs identical to PL/I (F)** in three jobs
(JOB00384 to JOB00386).

## Tier 26 -- done 2026-10-13

Secondary entry points (#53, `research/pli/tier26-measurement.md`).

**A hundred and ten programs identical to PL/I (F)** in three jobs
(JOB00393 to JOB00395).

## Tier 27 -- done 2026-10-13

The preprocessor (#55, `research/pli/tier27-measurement.md`): FIXED and
CHARACTER preprocessor variables, replacement with rescanning, %IF, %DO,
%GO TO, %ACTIVATE/%DEACTIVATE, %INCLUDE inside the scan (`src/pp.c`).

**A hundred and twelve programs identical to PL/I (F)** in three jobs
(JOB00402 to JOB00404).

Still missing: preprocessor procedures.

## Tier 28 -- done 2026-10-13

Preprocessor procedures (#57, `research/pli/tier28-measurement.md`).
With Tier 27 the preprocessor is done. (This entry first said that the
definition of done was complete with it. It was not: AREA/OFFSET and
KEYED record I/O remained. Corrected in Tier 29.)

**A hundred and fourteen programs identical to PL/I (F)** in three jobs
(JOB00413 to JOB00415).

## Tier 29 -- done 2026-10-13

AREA and OFFSET (#59, `research/pli/tier29-measurement.md`). Areas can
be static, automatic, based or controlled. Covered: ALLOCATE ... IN(a)
[SET(p)], FREE ... IN(a), offsets converted to and from pointers, NULLO,
EMPTY, area assignment, and the AREA condition with its retry. The
allocation algorithm is PL/I (F)'s own (IHELSP, `rt/pl1area.c`), down
to the control words.

**A hundred and seventeen programs identical to PL/I (F)** in three jobs
(JOB00424 to JOB00426).

Still missing from the definition of done: KEYED record I/O.

## Tier 30 -- done 2026-10-13

KEYED record I/O on REGIONAL(1) data sets (#61,
`research/pli/tier30-measurement.md`). Covered: SEQUENTIAL and DIRECT
creation with PL/I (F)'s dummy records, DIRECT READ KEY, WRITE KEYFROM,
REWRITE KEY and DELETE, SEQUENTIAL INPUT and UPDATE with KEYTO, and the
KEY condition. The access methods are PL/I (F)'s: BSAM in load mode and
BDAM (`rt/pl1bdam.asm`). A test can bring DD statements of its own
(`tests/pli/<name>.dd`).

**A hundred and twenty programs identical to PL/I (F)** in three jobs
(JOB00442 to JOB00444).

Not done: REGIONAL(2) and (3), and INDEXED (Tier 32).

## Tier 31 -- done 2026-10-13

The full iterative DO (#63, `research/pli/tier31-measurement.md`):
several specifications, WHILE in an iterative DO, BY as an expression,
TO omitted, and control variables of every arithmetic type, as
GC28-8201-4 expands them. PUT LIST on a PRINT file now places items at
the end of a line as PL/I (F) does: an item longer than the rest of the
line goes on at position 1 of the next, and an item that ends in
position 120 puts the next one at 25.

**A hundred and twenty-three programs identical to PL/I (F)** in five
jobs (JOB00491 to JOB00495): mvsMF on MVSTK5-BLD now drops the
connection on a job of about 70 KB of JCL, so the runner keeps a job
below 60 KB as well as below 50 programs.

## Tier 32 -- done 2026-10-13

INDEXED record I/O (#65, `research/pli/tier32-measurement.md`): creation
in ascending key order, SEQUENTIAL INPUT and UPDATE with KEYTO, REWRITE
and DELETE, DIRECT READ, REWRITE, WRITE (adding) and DELETE by key, and
the KEY conditions 51 to 53. QISAM and BISAM, as PL/I (F) uses them
(`rt/pl1isam.asm`), for unblocked F records with OPTCD=L.

With REGIONAL(1) and INDEXED, the KEYED record I/O of the definition of
done is there. Every feature of the definition of done that PL/I (F)
can check is now done. Left is the final tier beyond the oracle.

**A hundred and twenty-six programs identical to PL/I (F)** in five
jobs (JOB00530 to JOB00534).

## Tier 33 -- done 2026-10-13

Beyond the oracle, part one (#67, `research/pli/tier33-measurement.md`):
SELECT/WHEN/OTHERWISE, LEAVE, DO WHILE/UNTIL, and UNTIL and REPEAT in
iterative specifications, as GC33-0009-4 defines them. A test is a pair:
`<name>.opt.pli` in the Optimizing Compiler's language is compiled by
PL1C/370, and its twin `<name>.pli`, the documented expansion, runs on
PL/I (F).

**A hundred and twenty-eight programs identical to PL/I (F)** in five
jobs (JOB00538 to JOB00542).

Left: the DEFAULT statement (Tier 34).

## Tier 34 -- done 2026-10-13

Beyond the oracle, part two (#69, `research/pli/tier34-measurement.md`):
the DEFAULT statement. It covers RANGE with identifiers, letter ranges
and `*`, data attributes and VALUE, factored specifications, block
scope with inner ranges overriding outer ones, and implicit
declarations taking the main procedure's defaults. The tests are twins
as in Tier 33.

**A hundred and thirty programs identical to PL/I (F)** in five jobs
(JOB00550 to JOB00554).

With it the roadmap as planned is done: the definition of done and the
final tier beyond the oracle.

## Tier 35 -- done 2026-10-13

Backlog, part one (#73, `research/pli/tier35-measurement.md`): factored
declarations with attributes and level numbers inside the lists,
CHAR(*) and CHAR(*) VARYING parameters, and END label closing the
groups inside the labelled one.

**A hundred and thirty-three programs identical to PL/I (F)** in five
jobs (JOB00580 to JOB00584).

## Tier 36 -- done 2026-10-13

Backlog, part two (#75, `research/pli/tier36-measurement.md`): areas
in structures and arrays, AREA(n) and AREA(*) parameters by reference,
areas in record I/O, UNSPEC of offsets, DEFAULT VALUE(AREA(n)).

**A hundred and thirty-six programs identical to PL/I (F)** in six
jobs (JOB00593 to JOB00598).

## Tier 37 -- done 2026-10-13

Backlog, part three (#77, `research/pli/tier37-measurement.md`): record
I/O through pointers -- LOCATE, READ SET and REWRITE without FROM.

**A hundred and thirty-seven programs identical to PL/I (F)** in six
jobs (JOB00603 to JOB00608).

## Tier 38 -- done 2026-10-13

Backlog, part four (#79, `research/pli/tier38-measurement.md`):
REGIONAL(2) and REGIONAL(3) -- recorded keys, BDAM extended search
from the start of the region's track over the whole data set, KEY 51,
53, 54, 56 and 57, sequential access without the dummy records.

**A hundred and thirty-nine programs identical to PL/I (F)** in six
jobs (JOB00626 to JOB00631).

## Tier 39 -- done 2026-10-13

Backlog, part five (#81, `research/pli/tier39-measurement.md`): the
ONKEY built-in, the key of a KEY or RECORD condition as each
organisation gives it.

**A hundred and forty programs identical to PL/I (F)** in six jobs
(JOB00636 to JOB00641).

## Tier 40 -- done 2026-10-13

Backlog, part six (#83, `research/pli/tier40-measurement.md`): INDEXED
data sets with blocked records and embedded keys (KEY 55, ONCODE 1007
and 1009), and READ KEY positioning a SEQUENTIAL INDEXED file.

**A hundred and forty-two programs identical to PL/I (F)** in six jobs
(JOB00656 to JOB00663).

## Tier 41 -- done 2026-10-13

Backlog, part seven (#85, `research/pli/tier41-measurement.md`): BIT(*)
parameters, array parameters with * bounds (A(*), A(*, *)), and arrays
of CHAR(*) and CHAR(*) VARYING, through a descriptor of the argument.

**A hundred and forty-four programs identical to PL/I (F)** in six
jobs (JOB00686 to JOB00691).

## Tier 42 -- done 2026-10-13

Backlog, part eight (#88, `research/pli/tier42-measurement.md`): LOCATE
KEYFROM on INDEXED files, READ SET with KEY and with KEYTO.

**A hundred and forty-five programs identical to PL/I (F)** in three
jobs (JOB00706 to JOB00708).

## Tier 43 -- done 2026-10-13

Backlog, part nine (#90, `research/pli/tier43-measurement.md`): area
arguments of another size than the parameter (by reference, no dummy,
as PL/I (F) does), and UNSPEC of more than 32 bits.

**A hundred and forty-seven programs identical to PL/I (F)** in three
jobs (JOB00717 to JOB00719).

## Tier 44 -- done 2026-10-13

Backlog, part ten (#92, `research/pli/tier44-measurement.md`): A(*)
parameters as a whole -- assignment, expressions and PUT LIST, in loops
over their bounds at run time.

**A hundred and forty-eight programs identical to PL/I (F)** in three
jobs (JOB00727 to JOB00733).

## Tier 45 -- done 2026-10-13

Backlog, part eleven (#94, `research/pli/tier45-measurement.md`):
unaligned bit strings in structures and arrays packed bit by bit as
PL/I (F) maps them (the Tier 5 algorithm in bits), ALIGNED and
UNALIGNED, and the B format item in PUT and GET EDIT.

**A hundred and fifty-one programs identical to PL/I (F)** in four
jobs (JOB00756 to JOB00759).

## Tier 46 -- done 2026-10-13

Backlog, part twelve (#96, `research/pli/tier46-measurement.md`):
format items whose width or d is an expression, and the E format item
for FIXED values (with PL/I (F)'s lost rounding carry).

**A hundred and fifty-three programs identical to PL/I (F)** in four
jobs (JOB00770 to JOB00773).

## Tier 47 -- done 2026-10-13

Backlog, part thirteen (#98, `research/pli/tier47-measurement.md`):
several external procedures in one source (`*PROCESS`, PL/I (F)'s
batched compilation), calls between them, EXTERNAL variables and file
constants shared by name with PL/I (F)'s rule for the storage. Also two
larger programs of the kind real ones are, identical: `t152rl` (an
inventory report) and `t153wc` (word counts in a BASED list). The test
runner compares REPORT only for a program that declares or opens it.

**A hundred and fifty-seven programs identical to PL/I (F)** in four
jobs (JOB00785 to JOB00788).

## Tier 48 -- done 2026-10-13

Backlog, part fourteen (#100, `research/pli/tier48-measurement.md`):
external procedures compiled apart and linked -- calls through
V-constants with the ENTRY declaration's descriptors (empty ones too),
entry points named after the procedures, EXTERNAL variables as common
areas or control sections, file descriptors as named sections, sources
without OPTIONS(MAIN). An ld370 bug found on the way is with cc370.

**A hundred and fifty-eight programs identical to PL/I (F)** in four
jobs (JOB00794 to JOB00797).

## Tier 49 -- done 2026-10-13

Backlog, part fifteen (#102, `research/pli/tier49-measurement.md`): ON
... SNAP as PL/I (F) writes it, SIGNAL of a disabled condition as a null
statement, and CALL of an undeclared name as an external entry -- with
which IBM's SYS1.SAMPLIB(PL1SAMP) compiles.

**A hundred and sixty-four programs identical to PL/I (F)** in four
jobs (JOB00810 to JOB00813).

## Tier 50 -- done 2026-10-13

Backlog, part sixteen (#104, `research/pli/tier50-measurement.md`):
structure assignment BY NAME and the BOOL built-in, found with a third
program of the kind real ones are (`t165rl3`).

**A hundred and sixty-seven programs identical to PL/I (F)** in four
jobs (JOB00821 to JOB00824).

## Tier 51 -- done 2026-10-13

Backlog, part seventeen (#106, `research/pli/tier51-measurement.md`):
gaps met compiling PL/I programs from teampli.net and Iron Spring -- the
BUILTIN attribute, a function's default RETURNS, OPTIONS(REENTRANT),
ENTRY statements in external procedures; the runner reads REPORT only
for programs that use it.

**A hundred and seventy programs identical to PL/I (F)** in four jobs
(JOB00834 to JOB00837).

## Tier 52 -- done 2026-10-13

Backlog, part eighteen (#108, `research/pli/tier52-measurement.md`):
gaps met in gauss.pli -- FORMAT statements and the R format, iteration
factors without parentheses, a letter right after a number, and
repetitive specifications in GET and PUT data lists.

**A hundred and seventy-two programs identical to PL/I (F)** in four
jobs (JOB00848 to JOB00851).

## Tier 53 -- done 2026-10-13

Backlog, part nineteen (#110, `research/pli/tier53-measurement.md`):
AUTOMATIC storage over 4095 bytes (a zone from offset 4096 of the
frame), a repetition factor before a string in INITIAL, and the
three-procedure EXTERNAL case compiled apart, now that ld370 1.6.2
links it. gauss.pli and printsigns.pli from teampli.net are identical
to PL/I (F).

**A hundred and seventy-six programs identical to PL/I (F)** in four
jobs (JOB00869 to JOB00872).

## Tier 54 -- done 2026-10-13

Backlog, part twenty (#112, `research/pli/tier54-measurement.md`):
entries with RETURNS in a procedure without RETURNS, and entries
returning other attributes, found in suc.pli from teampli.net.

**A hundred and seventy-seven programs identical to PL/I (F)** in four
jobs (JOB00879 to JOB00882).

## Tier 55 -- done 2026-10-13

Backlog, part twenty-one (#114, `research/pli/tier55-measurement.md`):
string parameters of external procedures go by descriptor, so that a
caller compiled apart reaches them whatever length its ENTRY
declaration gives; BIT(*) in ENTRY declarations.

**A hundred and seventy-nine programs identical to PL/I (F)** in four
jobs (JOB00893 to JOB00896), then `t176sus`, `t177str` (JOB00898) and
the new `t178vst` (JOB00900): a hundred and eighty.

## Tier 56 -- done 2026-10-13

Backlog, part twenty-two (#116, `research/pli/tier56-measurement.md`):
iteration factors given by expressions in format lists, external names
of more than eight characters cut as PL/I (F) cuts them, and FLOAT
arguments of the same length to a procedure the caller does not
declare.

**A hundred and eighty-four programs identical to PL/I (F)** in four
jobs (JOB00907 to JOB00910).

## Tier 57 -- done 2026-10-13

Backlog, part twenty-three (#118, `research/pli/tier57-measurement.md`):
E and F format for character strings, B(w) for FIXED values, and
IHE703I for B without a width.

**A hundred and eighty-six programs identical to PL/I (F)** in four
jobs (JOB00920 to JOB00923).

## Tier 58 -- done 2026-10-14

Backlog, part twenty-four (#120, `research/pli/tier58-measurement.md`):
scaled FIXED BINARY variables -- declarations, assignment (a decimal
value converted at the target's scale), STATIC INITIAL, output, GET;
FLOAT constants read into FIXED targets.

**A hundred and eighty-nine programs identical to PL/I (F)**
(JOB00942 to JOB00945, and JOB00947 for the 14 that an S106 of PL/I
(F) flushed).

## Tier 59 -- done 2026-10-14

Backlog, part twenty-five (#122, `research/pli/tier59-measurement.md`):
the array functions SUM, PROD, POLY, ALL and ANY, found missing by a
comparison with Section G of GC28-8201-2; the abbreviations PREC and
UNAL. The same comparison lists what is still missing.

**A hundred and ninety-three programs identical to PL/I (F)**
(JOB00955 to JOB00958).

## Tier 60 -- done 2026-10-14

Backlog, part twenty-six (#124, `research/pli/tier60-measurement.md`):
COUNT, LINENO, DATE, TIME, ONLOC and ONCOUNT; each ENTRY statement a
procedure descriptor of its own, for ONLOC.

**A hundred and ninety-four programs identical to PL/I (F)**
(JOB00971, JOB00972, JOB00976, JOB00977).

## Tier 61 -- done 2026-10-14

Backlog, part twenty-seven (#126, `research/pli/tier61-measurement.md`):
cross sections of arrays (`M(2,*)`): assignment, expressions, PUT and
GET items, array functions, and arguments (copied in and back).

**A hundred and ninety-six programs identical to PL/I (F)**
(JOB00983 to JOB00986; `t66sqrt`, whose PL/I (F) compile step ended with
condition code 16 in JOB00986, in JOB00988).

## Tier 62 -- done 2026-10-14

Backlog, part twenty-eight (#128, `research/pli/tier62-measurement.md`):
the NAME condition -- ON, SIGNAL, REVERT -- and DATAFIELD.

**A hundred and ninety-eight programs identical to PL/I (F)**
(JOB00996 to JOB00999).

## Tier 63 -- done 2026-10-14

Backlog, part twenty-nine (#130, `research/pli/tier63-measurement.md`):
the REFER option for an array bound and a string length; whole arrays
under a pointer qualifier (`P2->A = 7`).

**Two hundred programs identical to PL/I (F)** (JOB01011 to JOB01014).

## Tier 64 -- done 2026-10-14

Backlog, part thirty (#132, `research/pli/tier64-measurement.md`): the
CHECK condition -- prefixes, ON, SIGNAL, REVERT, the system action as
PUT DATA. Each procedure now ends with `DROP 12,11,10`: an on-unit after
a main procedure of 4 KB took its prologue branch through R11 (S0C1).

**Two hundred and one programs identical to PL/I (F)** (JOB01041 to
JOB01045).

## Tier 65 -- done 2026-10-14

Beyond the oracle (#135, `research/pli/tier65-measurement.md`): every
built-in function of ANSI X3.53-1976 that PL/I (F) does not have --
AFTER, BEFORE, DECAT, REVERSE, COPY, COLLATE, ASIN, ACOS, SUBTRACT,
DOT, EVERY, SOME, OFFSET, POINTER, ONFIELD, PAGENO (and its
pseudo-variable), VALID -- tested as twins. Each function carries its
language level in `src/resolve.c`; `docs/language-levels.md` lists
what is beyond PL/I (F) and every deviation.

**Two hundred and four programs identical to PL/I (F)** (JOB01054 to
JOB01058).

## Tier 66 -- done 2026-10-14

A gap against the oracle (#137, `research/pli/tier66-measurement.md`):
STRING as a function and a pseudo-variable, UNSPEC as a pseudo-variable.

**Two hundred and six programs identical to PL/I (F)** (JOB01063 to
JOB01067; `t135loc` in JOB01069).

## Tier 67 -- done 2026-10-14

A gap against the oracle (#139, `research/pli/tier67-measurement.md`):
GENERIC, its members chosen as PL/I (F) chooses them -- exact base,
scale and precision, the kind of string but not its length, the
dimensions of an array, the number of arguments.

**Two hundred and eight programs identical to PL/I (F)** (JOB01075 to
JOB01079).

## Tier 68 -- done 2026-10-14

Gaps against the oracle (#141, `research/pli/tier68-measurement.md`):
READ ... IGNORE(n), GET ... COPY, the TRANSMIT condition and the
UNBUFFERED attribute.

**Two hundred and nine programs identical to PL/I (F)** (JOB01085 to
JOB01089).

## Tier 69 -- done 2026-10-14

Beyond the oracle (#143, `research/pli/tier69-measurement.md`): AFTER,
BEFORE, DECAT, REVERSE and COPY of bit strings, by ANSI's common string
type. Measured as well: PL/I (F) takes only a pointer variable as a
locator qualifier.

**Two hundred and ten programs identical to PL/I (F)** (JOB01094 to
JOB01098).

## Tier 70 -- done 2026-10-14

Beyond the oracle (#145, `research/pli/tier70-measurement.md`): ANSI's
conditions STRINGSIZE (enabled by default, raised when a string is cut)
and STORAGE (raised when ALLOCATE finds no storage).

**Two hundred and eleven programs identical to PL/I (F)** (JOB01103 to
JOB01107).

## Tier 71 -- done 2026-10-14

Beyond the oracle (#147, `research/pli/tier71-measurement.md`): ANSI's
radix factors, bit constants 'x'B1 to 'x'B4 and the format items B1 to
B4. The section once "Deliberately out of scope" now plans those items
(the maintainer's decision: 100 % PL/I (F)).

**Two hundred and twelve programs identical to PL/I (F)** (JOB01113 to
JOB01117; `t45fge` in JOB01119).

## Tier 72 -- done 2026-10-14

LINESIZE(e) and PAGESIZE(e) on OPEN, and ANSI's TAB option and TAB
format item (#149, `research/pli/tier72-measurement.md`). Also fixed: a
number that does not fit in the rest of a LIST line starts the next one,
and LINENO past the page is 1.

**Two hundred and fourteen programs identical to PL/I (F)** (JOB01127 to
JOB01131).

## Tier 73 -- done 2026-10-14

Beyond the oracle (#151, `research/pli/tier73-measurement.md`): ANSI's
GET ... COPY(file), the fields copied as PL/I (F)'s COPY writes them.

**Two hundred and fifteen programs identical to PL/I (F)** (JOB01135 to
JOB01139).

## Tier 74 -- done 2026-10-14

Beyond the oracle (#153, `research/pli/tier74-measurement.md`): ANSI's
attributes CONSTANT, VARIABLE, PARAMETER, LOCAL, STRUCTURE, MEMBER,
NONVARYING, DIMENSION and CONDITION. ENTRY and FILE variables remain.

**Two hundred and sixteen programs identical to PL/I (F)** (JOB01143 to
JOB01147).

## Tier 75 -- done 2026-10-14

ANSI's ENTRY and FILE variables (#155, `research/pli/tier75-measurement.md`).
Against the oracle: a call through an ENTRY parameter converts its
arguments to the parameter's descriptors (Q1EDSC, `t216eds`).

**Two hundred and eighteen programs identical to PL/I (F)** (JOB01159 to
JOB01163).

## Tier 76 -- done 2026-10-14

Beyond the oracle (#157, `research/pli/tier76-measurement.md`): any
pointer-valued reference as a locator qualifier, chained qualifiers,
BASED without a pointer.

**Two hundred and nineteen programs identical to PL/I (F)** (JOB01167 to
JOB01171).

## Tier 77 -- done 2026-10-14

Beyond the oracle (#160, `research/pli/tier77-measurement.md`): ANSI's
GENERIC with WHEN, the first matching member taken; PL/I (F)'s form
unchanged.

**Two hundred and twenty programs identical to PL/I (F)** (JOB01182 to
JOB01190).

## Tier 78 -- done 2026-10-14

INITIAL (#162, `research/pli/tier78-measurement.md`). Against the oracle:
`*`, iteration factors that are expressions, groups `(n)(item, ...)`.
Beyond it: ANSI's parenthesized expressions and references as values.

**Two hundred and twenty-three programs identical to PL/I (F)**
(JOB01202 to JOB01210).

## Tier 79 -- done 2026-10-14

PL/I (F)'s 48-character set, option CHAR48 (#164,
`research/pli/tier79-measurement.md`): `-char48`, and `<name>.parm` for a
test's compiler PARM.

**Two hundred and twenty-four programs identical to PL/I (F)** (JOB01218
to JOB01226).

## Tier 80 -- done 2026-10-14

The main procedure's parameter, passed as PL/I (F)'s IHESAPA does, and an
array element as the control variable of a DO without TO or BY (#168,
`research/pli/tier80-measurement.md`); for the BREXX/370 external
functions (TODO.md). Tests may now set the program's PARM
(`<name>.goparm`).

**Two hundred and twenty-six programs identical to PL/I (F)** (JOB01238
to JOB01246).

## Tier 81 -- done 2026-10-14

Adjustable bounds of AUTOMATIC arrays, evaluated at the block's entry,
the data on the stack past the frame; multiple assignment `A, B = e` (#170,
`research/pli/tier81-measurement.md`). RXPI and RXCUT of
`~/repos/mvs/foo` compile.

**Two hundred and twenty-seven programs identical to PL/I (F)**
(JOB01254 to JOB01262, JOB01265).

## Tier 82 -- done 2026-10-15

External REXX functions: `-plist` (R1 to the main procedure unchanged,
PL/I (F)'s `ENTRY IHESAPB`/`IHESAPD`) and `OPTIONS(MAIN NOEXECOPS)`; the
test driver XDRIVER calls a program as TSO/E REXX calls an external
function (#175, `research/pli/tier82-measurement.md`). RXCUT and RXPI of
`~/repos/mvs/foo` and the function part of FCONCAT ("Practical Usage of
MVS REXX", 14.14.4) are tests.

**Two hundred and thirty programs identical to PL/I (F)**
(JOB01280 to JOB01288, JOB01291).

## Tier 83 -- done 2026-10-15

Language levels: `-lang=F` (PL/I (F)'s language only, anything beyond it
an error), the check in one function (`beyond_f`) called where each
feature is recognized, and every feature beyond PL/I (F) marked OPT, ISO
or V2 after GC33-0009-4 (#181, `research/pli/tier83-measurement.md`,
`docs/language-levels.md`). The runner compiles the 211 programs without
a `.opt.pli` with `-lang=F` and checks that `-lang=F` refuses each of the
19 `.opt.pli`. Found on the way: NOEXECOPS is OS PL/I Version 2's, and
the Optimizer disables STRINGSIZE by default.

**Two hundred and thirty programs identical to PL/I (F)**
(JOB01298 to JOB01306; the XDRIVER steps' codes read again after a fix
to the runner's step names, 0000 in both runs).

## Tier 84 -- done 2026-10-15

The default follows the later IBM rules where they differ from PL/I (F)
(#189, `research/pli/tier84-measurement.md`): STRINGSIZE disabled by
default, its system action the Optimizer's message IBM441I (IBM440I for
SIGNAL); the PARM of a main procedure with one VARYING parameter and no
NOEXECOPS split at the first `/`. `-lang=F` keeps PL/I (F)'s behaviour.
A test `<name>.sem` marks a `.opt.pli` whose source is PL/I (F)'s.

**Two hundred and thirty-two programs identical to PL/I (F)**
(JOB01318 to JOB01326).

## Tier 85 -- done 2026-10-15

Abnormal termination by the system as PL/I (F) handles it (#198,
`research/pli/tier85-measurement.md`): an ESTAE whose exit writes IHE012I
and lets the abend go on; the SPIE given back at the end, so that a
LINKed program leaves none behind; a full data set abends (D37, B37)
where libc370 reports ENOSPC or EIO and the run time used to lose the
output. `<name>.abend` gives such a test a job of its own.

**Two hundred and thirty-three programs identical to PL/I (F)**
(JOB01360 to JOB01370; t91pctx, flushed by a PL/I (F) compiler RC 16,
in JOB01373; t231abn with the final code in JOB01375).

## Tier 86 -- done 2026-10-15

The cheap items of PL/I for MVS & VM (#201, `research/pli/tier86-measurement.md`):
X and BX constants, %PROCESS, SYSNULL, POINTERADD, POINTERVALUE,
BINARYVALUE and DATETIME, each level MVS. A constant's characters
outside printable ASCII are written as `DC X'..'` with their CP037 code
(src/ebcdic.c).

**Two hundred and thirty-four programs identical to PL/I (F)**
(JOB01383 to JOB01393).

## Tier 87 -- done 2026-10-15

PLIRETC and PLIRETV (the Optimizer's return code, added to the 1000 of
STOP and the 2000 of ERROR), ENTRYADDR as function and pseudovariable,
and STATIC pointers with INITIAL(NULL) (OPT; PL/I (F) refuses it,
IEM0512I), SYSNULL or POINTERVALUE(constant) (MVS) (#204,
`research/pli/tier87-measurement.md`). `<name>.cc` names a test's code
where PL/I (F) cannot give it.

**Two hundred and thirty-eight programs identical to PL/I (F)**
(JOB01410 to JOB01420).

## libc370 2.12.0 -- 2026-10-15

A full data set's abend takes its code from libc370's `__fx37` (D37-04,
B37-04); the run time needs libc370 2.12.0 or later. `t238b37` is new.

**Two hundred and thirty-nine programs identical to PL/I (F)**
(JOB01431 to JOB01445).

## Tier 88 -- done 2026-10-15

LANGLVL(SPROG), PL/I for MVS & VM's pointer arithmetic (#207,
`research/pli/tier88-measurement.md`): `-langlvl=SPROG` or LANGLVL in
`%PROCESS`/`*PROCESS`; `P + n`, `P - n`, `P2 - P1`, pointers compared by
`<` and `>`. POINTERADD, POINTERVALUE and BINARYVALUE need it, as IBM's
default is NOSPROG.

**Two hundred and forty programs identical to PL/I (F)**
(JOB01451 to JOB01465; 45 programs flushed by a PL/I (F) compiler S106
in JOB01455 again in JOB01469, t5char, after a compiler RC 16, in
JOB01472).

## Tier 89 -- done 2026-10-15

Calls of assembler routines (#209, `research/pli/tier89-measurement.md`):
OPTIONS(ASSEMBLER) of an ENTRY passes the data's addresses, the last
with its high-order bit, no descriptors; RETCODE keeps R15 as the PL/I
return code; EXTERNAL('name') gives the name outside. The test routine
ASMTST (tests/mvs/asmsub) is linked into both runs (`<name>.asmsub`).

**Two hundred and forty-one programs identical to PL/I (F)**
(JOB01481 to JOB01495).

## Tier 90 -- done 2026-10-15

OPTIONAL arguments and BYVALUE of OPTIONS(ASSEMBLER) entries, and
OPTIONS(MAIN BYVALUE), whose parameters are the words R1 points at
(#211, `research/pli/tier90-measurement.md`). A second test routine,
ASMVAL; FCONCAT with BYVALUE (`t242byv`) runs under XDRIVER.

**Two hundred and forty-three programs identical to PL/I (F)**
(JOB01501 to JOB01515).

## Tier 91 -- done 2026-10-15

FETCH and RELEASE of OPTIONS(ASSEMBLER) entries (#213,
`research/pli/tier91-measurement.md`): the module LOADed at the FETCH or
at the first CALL, DELETEd at the RELEASE (rt/pl1fetch.asm). Fetching a
PL/I procedure needs a run time shared with it and is refused.

**Two hundred and forty-four programs identical to PL/I (F)**
(JOB01525 to JOB01539, the first suite on m38b's HTTPD 4.3.0 and mvsMF
1.3.0).

## Tier 92 -- done

The small gaps of PL/I (F) (#216, `research/pli/tier92-measurement.md`):
INDEX of bit strings, UNSPEC of an expression, REDUCIBLE and
IRREDUCIBLE, and `-lang=F` refusing `GETP -> X`. PL/I (F) refuses
REDUCIBLE on a PROCEDURE statement (JOB01832), so that form is level
OPT, with a test of its own (`t245red`). `t244gap` and `t245red`
identical in JOB01835; the full suite 245 of 246 in JOB01838 to
JOB01852, the one left over `t167ent`, whose PL/I (F) compile took a
program check in phase MA (IEM3856I), as m38b's program fetch did in the
soak; alone it is identical (JOB01855).

## Tier 93 -- done

Record locking of PL/I (F) (#217, `research/pli/tier93-measurement.md`):
the EXCLUSIVE attribute, READ ... NOLOCK and UNLOCK, as one task sees
them: no effect. `t246exc` identical in JOB01883; the full suite 245 of
247 in JOB01890 to JOB01904, the two left over (`t130star`, `t177str`)
lost to PL/I (F) program checks on m38b (IEM3856I) and identical alone
in JOB01907.

## Tier 94 -- done

The DISPLAY statement (#218, `research/pli/tier94-measurement.md`): a
WTO, or with REPLY a WTOR the program waits for, as PL/I (F)'s IHEDSP
does it. DISPLAY ... EVENT waits for event variables (#228).
`tests/run_mvs.py` answers a test's WTORs from `<name>.reply` and
compares the console messages. `t248dsp` identical in JOB01920; the
full suite 248 of 248 (JOB01923 to JOB01941, two again in JOB01944 and
JOB01946).

## Tier 95 -- done

The record format of the ENVIRONMENT attribute (#215,
`research/pli/tier95-measurement.md`): F, V, U, VS, VBS, CTLASA, CTL360
give the data set's DCB, placed before OPEN by LIBC/370 2.13.0 and
winning over the DD as on PL/I (F); a PRINT file's line size from its
record; a record's length from LIBC/370 2.14.0's `fread`. The other
options are accepted without effect. ABI stamp PL1RT002, LIBC/370
2.14.0 required. `t247env` identical in JOB01953; the full suite 249
of 249 in JOB01956 to JOB01974.

## Tier 96 -- done

Sterling data (#219, `research/pli/tier96-measurement.md`): sterling
constants as FIXED DECIMAL pence, sterling pictures (G, M, the fields,
8, 7, 6, H, P, the separators never suppressed) in assignment,
arithmetic, PUT and GET, the P format both ways. On the way: GET into
any numeric picture, the P format in GET EDIT, and SIZE for a numeric
picture target. `t249stl` identical in JOB01991; the full suite 250
of 250 in JOB01994 to JOB02012.

## Tier 97 -- done

The C stack set by the compiler (#177, `research/pli/startup-measurement.md`):
`@@STKLEN` the frames added up and 32 KB, or LIBC/370's 256 KB where
the bound is not visible; `-stack=N`. Measured first with a measuring
build of the run time. Median VIRT of a PL1C/370 step 452K to 236K
over the suite, 250 of 250 identical (JOB02059 to JOB02077).

## Tier 98 -- done

The LINKed case (#220, `research/pli/tier98-measurement.md`): PL/I
(F)'s IHESARC and IHETSAC as PLIRETC; no SYSPRINT DD as on PL/I (F)
(UNDEFINEDFILE at the first PUT, the run time's messages on the
console); `<name>.nosysp` and a second LINK in the test runner;
SYSPRINT no longer emptied by a program that does not write to it
(LIBC/370 2.15.0 opens the streams on first use). Dummy arguments
(#266): (X), and an internal procedure's parameter attributes. The
full suite 255 of 255 (JOB02114 to JOB02132).

## Tier 99 -- done

Factored names in a structure, `2 (A, B) FIXED` (#267,
`research/pli/tier99-measurement.md`). `t255fac` identical in JOB02159; the full suite 255 of 256 in JOB02138
to JOB02156, the one left that test, edited during the run.

## Tier 100 -- done

Reported from hellocp (`research/pli/tier100-measurement.md`): an
internal function's result from its PROCEDURE's RETURNS (OPT, the
default language), the null argument list `F()` (OPT), and INITIAL of
packed bit strings in STATIC storage (PL/I (F)). `t256fun`, `t257bit`
identical in JOB02164 and JOB02167; the full suite 258 of 258
(JOB02170 to JOB02188, three again in JOB02191 after a 106-F).

## Tier 101 -- done

Structure arguments to procedures compiled apart, structure parameter
descriptors (#273, `research/pli/tier101-measurement.md`), reported
from hellocp. `t258sar` identical in JOB02194; the full suite 259 of 259 (JOB02197
to JOB02215).

## Tier 102 -- done

INITIAL CALL (#222, `research/pli/tier102-measurement.md`), OPT: at the
block's entry for AUTOMATIC, at each ALLOCATE for CONTROLLED.
`t259icl` identical in JOB02226; the full suite 260 of 260 (JOB02229
to JOB02247).

## Tier 103 -- done

STORAGE, CURRENTSTORAGE and CONNECTED (#223,
`research/pli/tier103-measurement.md`), OPT. `t260stg` identical in
JOB02250 (its twin's values derived by hand from PL/I (F)'s mapping);
the full suite 261 of 261 (JOB02253 to JOB02271).

## Tier 104 -- done

The Optimizer's preprocessor (#224, `research/pli/tier104-measurement.md`):
INDEX, LENGTH, COUNTER, COMPILETIME, PARMSET, omitted arguments,
NORESCAN, built-in functions in the text. And IEM0578I's rule: a
scalar's INITIAL of more than one value is an error. `t261ppb`
identical in JOB02278; the full suite 262 of 262 (JOB02282 to
JOB02300).

## Tier 105 -- done

PLIDUMP (#225), OPT: a dump in PL/I terms on DD PLIDUMP
or PL1DUMP. `t262dmp` and `t263nod` identical; the full suite 264 of
264 (JOB02340 to JOB02358).

## Tier 106 -- done

The ATTENTION condition and the INTERRUPT option (#226),
OPT: STAX under a TSO terminal, polls at stream I/O, at loops and at GO
TO. `t264att`, `t265int` identical; the full suite 266 of 266 (JOB02368
to JOB02388). A real attention at a terminal is not measured yet
(`test/tso/attn.lua` waits for mbt's TSO logon on TK5).

## Tier 107 -- done

PLISRTA to PLISRTD (#233), OPT, with PL/I (F)'s IHESRTx:
the parameter list and the exits as PL/I (F) uses them, the sort
abandoned when control leaves an exit. And a CHAR(*) parameter of E35 gets the record length as its
bound: such a parameter's temporaries were sized only by the calls in
the compilation. `t266srt` to `t270srx` identical in JOB02426 and
JOB02432; the full suite 271 of 271 (JOB02437 to JOB02457).

## Tier 108 -- done

PLICKPT, PLIREST, PLICANC (#233), OPT, with PL/I (F)'s
IHECKPS, IHECKPT, IHEREST, IHERESN. `t271ckp` and `t272rst` identical
in JOB02460 and JOB02462. No checkpoint can be taken on MVSTK5-BLD
(shared disks, IHJ000I 214), so the tests compare the codes of the
cases that take none, and PLIREST's U4092. The full suite 273 of 273
(JOB02469 to JOB02491, and JOB02495 for the 47 that an S106 of program
fetch on m38b took in JOB02489).

## Tier 109 -- done

COMPLEX, tier C1 (#227, `design/complex.md`): the
attribute (new type kinds), imaginary constants, INITIAL, assignment
with mode conversion, `+`, `-`, `=`, `¬=`, REAL, IMAG, CONJG, COMPLEX as
functions and pseudo-variables, complex to character, PUT LIST and PUT
DATA. check.c splits a complex value into its parts before it types
them, so the real rules give PL/I (F)'s precisions; each part is a
symbol of its own with the variable's storage. `t273cdc` to `t277cad`
identical in JOB02507 and JOB02510; the full suite 278 of 278 (JOB02515
to JOB02539).

## Tier 110 -- done

Event variables without tasks (#228 tier 0 and DISPLAY ... EVENT of
tier 1, `design/multitasking.md`): EVENT, COMPLETION and
STATUS (functions and pseudo-variables), event assignment, WAIT with a
count, DELAY, DISPLAY ... REPLY ... EVENT. WAIT waits on the ECBs of
active events (a WTOR outstanding); an incomplete event that is not
active gives IHE550I and ERROR, as PL/I (F) does. Pseudo-variables of
a one-dimensional array are expanded by lower.c (COMPLETION, STATUS,
REAL, IMAG). UNSPEC of a bit string is that string (PL/I (F): '1'B for
a BIT(1) value). `t278evt` to `t281dse` identical in JOB02542 to
JOB02553; the full suite 282 of 282 (JOB02558 to JOB02584).

## Tier 111 -- done

EVENT I/O (#228 tier 1): READ, WRITE and REWRITE with
EVENT. The transmission happens at the statement; its file conditions
(ENDFILE, KEY, RECORD, TRANSMIT) are kept in the event and raised by
the WAIT that completes it -- the event incomplete in the on-unit,
complete after it or after a GO TO out of it, as PL/I (F) does.
`t282eio` and `t283eiw` identical in JOB02587; the full suite with them
in JOB02592 to JOB02618 (all identical but one test of the #284 fix
that ended its job in an S0C4; the 47 programs it took again identical
afterwards).

## Tier 112 -- done 2026-10-18

No IBM code (maintainer, 2026-10-18). The mathematical built-in
functions are the run time's own C (`rt/pl1m*.c`): textbook reduction
and series, the leading terms in an extended format with a 64-bit
fraction, one rounding at the end; a member per family of functions,
so that a program with SQRT alone links 6.6 KB of them, one with all
of them 27 KB (PL/I (F)'s library code took 6.8 KB in one member). Against values computed to 80 digits
they are correctly rounded, or within 0.6 units of the last digit (ERFC
long 1.4, `x ** y` long 2.3), where PL/I (F)'s library is often several
units off (`research/pli/tier14-measurement.md`). The errors, messages
and ONCODEs stay PL/I (F)'s. A twin whose E-form numbers may differ in
the last digits has a `<name>.math` file, and the runner compares it
within 100 units of the last digit (CLOSE). The probes, their outputs
and the tools for IBM's modules left the repository; the job submitter
and the print-file comparison are `tests/submit.py` and
`tests/compare.py`, probe programs live in a local `probes/`. The full
suite: 275 identical, the 10 mathematical twins differing in the last
digits (JOB02651 to JOB02677); with the `.math` files those 10 CLOSE and
the other mathematical twins identical (JOB02680).

## Definition of done

**A Subset G core, plus the IBM extensions that PL/I (F) V5 accepts.**

The core, after ANSI X3.74 (General-Purpose Subset):

- FIXED DECIMAL up to 15 digits, on packed decimal
- FIXED BINARY (15/31), FLOAT short and long
- CHARACTER, CHARACTER VARYING, BIT
- BASED, POINTER, ALLOCATE … SET, FREE
- ENTRY, LABEL and FILE variables
- ON-units for the Subset G conditions
- LIST- and EDIT-directed stream I/O
- SEQUENTIAL and KEYED record I/O
- %INCLUDE

On top, in rising cost, the IBM features that PL/I (F) programs actually use:

1. I–N default rule and implicit declaration
2. implicit invocation of parameterless functions
3. condition prefixes
4. BIT VARYING, LIKE, DEFINED/iSUB
5. PICTURE beyond the subset
6. GET/PUT DATA
7. CONTROLLED
8. AREA/OFFSET
9. the macro preprocessor (% statements)

Up to here, the target language is a subset of what PL/I (F) V5.5 accepts, so
every feature can be checked against it on TK5.

**Compatibility order:** PL/I (F) V5.5 first, the ANSI standard X3.53-1976
second. Where the two differ, PL1C/370 does what PL/I (F) does, and the
measurement notes the ANSI rule. ANSI decides only where PL/I (F) gives no
answer. The copy of the standard at hand is licensed to one user, so it is
cited by section and not kept in this repository. Where the two differ,
and the gaps that comparison found: `research/pli/ansi-comparison.md`.

**Final tier, beyond the oracle:** SELECT/WHEN/OTHERWISE, LEAVE and DO UNTIL
(Optimizing Compiler Release 3, documented in GC33-0009-4, Oct 1976) and the
DEFAULT statement (Release 1). PL/I (F) does not know them, so they are tested
against expected values taken from GC33-0009-4, not against the oracle.

## Once out of scope, now planned

The maintainer's decision of 2026-10-14: PL1C/370 is to be 100 % PL/I
(F), and as far as the two agree 100 % ANSI X3.53-1976. The items once
left out are planned after the remaining ANSI items, in this order:

1. **Sterling pictures and constants, the 48-character set.** Small, and
   measurable against PL/I (F).
2. **COMPLEX:** the attribute, arithmetic, the built-in functions
   (COMPLEX, REAL, IMAG, CONJG) and the C format item. Measurable against
   PL/I (F); ANSI has it too.
3. **Multitasking:** TASK, EVENT, WAIT, PRIORITY, CALL ... TASK EVENT
   PRIORITY, COMPLETION and STATUS. On MVS 3.8j through ATTACH, measured
   against PL/I (F).
4. **DISPLAY** (to the operator's console; REPLY needs an operator's
   answer, so a batch test can show only that it does not end the
   program) and **teleprocessing** (TRANSIENT files and PENDING, bound to
   TCAM queues the stands do not have).

Where PL/I (F) and ANSI disagree, PL/I (F) decides. An option for ANSI's
semantics may be proposed where a program needs it.

## Test method

Differential, as cobc370 does against IBM ANS COBOL
(`~/repos/mvs/cobc370/docs/DIFFERENTIAL-TESTING.md`): every test program
below the final tier is compiled and run both with PL1C/370 and with PL/I (F)
V5.5 on TK5, and the outputs are compared. Programs run on the host first where
the feature allows it.

Test corpora with usable licenses (see `research/pli/prior_art_compilers.md`):
the pli-llvm tests (MIT) and the Multics sources (permissive). Both were
written for other machines and need review for ASCII and byte-order
assumptions.

## Design decisions open

- **Link compatibility with PL/I (F).** Default: **no.** PL1C/370 has its own
  runtime on LIBC/370 and is compared on output only, as cobc370 is. Binding to
  F's `IHE*` library (conventions in GY28-6801-6) would allow mixing objects
  but ties the project to a 1972 ABI.
- **A later PL/S dialect.** PL/S is a PL/I-derived systems language, and the
  MVS 3.8j source holds about 0.82 million PL/S statements with IBM's generated
  code (`research/pls/report.md`). Cut the lexer, parser and declaration model
  so that a dialect can sit on them later. This costs little now and a lot
  later.
- **mbt integration.** mbt builds COBOL through cobc370 since mbt#252; PL/I
  would follow the same pattern once there is a compiler.

## Before Tier 0

1. ~~**Measure what V5.5 changes over V5.0.**~~ Done 2026-10-12
   (`research/pli/v55-measurement.md`): no difference from the Version 5
   documentation found. All V5 features from the Planning Guide work, every
   Optimizer statement is rejected, every printed option is documented, and
   the compiler is byte-identical on MVSTK5-BLD and MVSTK5-REF. GC28-8201-4
   with GC28-6594-7 is the specification for the oracle part.
2. **Decide whether "Subset G" is to be a formal conformance claim.** If so,
   buy ANSI X3.74-1987; if "a Subset-G-like core" suffices, the free
   description (Prime IDR4031 App. F, Digital Research manuals, ECMA-50 for
   full PL/I) is enough. Open point without the standard text: whether LIKE is
   in Subset G.
3. **The Optimizing Compiler's internal manuals** (LY33-6007/6008/6009) are
   not online; the Centre for Computing History holds LY33-6009-1. Only worth
   pursuing if an Optimizer-compatible runtime is ever wanted.
