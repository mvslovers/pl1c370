# Tier 53: AUTOMATIC storage over 4095 bytes, string repetition in INITIAL

Two more gaps from the programs of teampli.net, both compiled by PL/I
(F): gauss.pli has 10400 bytes of AUTOMATIC arrays, and printsigns.pli has
5000 bytes of them plus `CHAR(17) INIT((17)'K')`. Those programs are not
in this repository, because their licences are unclear. Compared on
MVSTK5-BLD with input of our own, both are now identical: gauss.pli in
JOB00858, printsigns.pli in JOB00865.

The probes are:

| Probe | Job | Test | Job |
|---|---|---|---|
| Q1BIGA | JOB00852 | `t171big` | JOB00854 |
| Q3INA | JOB00861 | `t172ini` | JOB00865 |
| Q4INB, Q5INC, Q6IND | JOB00861 | `t173inb` (the three together) | JOB00865 |

## AUTOMATIC storage over 4095 bytes

Q1BIGA has a main procedure with 18493 bytes of AUTOMATIC variables:

- a 40 by 40 FLOAT array;
- 2000 FIXED BIN(15) elements;
- an array of ten structures of 508 bytes;
- a CHAR(3000) VARYING.

It also has a recursive procedure with 5800 bytes of its own, five
levels deep, and a function with 6000. PL/I (F) compiles it with
warnings only (CC 4) and runs it with CC 0. PL1C/370's output is
identical.

**In PL1C/370:** a displacement reaches 4095 bytes of the frame. If a
block's variables end beyond 3072, those that do not fit below 3072 go
into a zone from offset 4096 of the frame on. That zone lies above the
temporaries, which stay below 4096. The address of such a variable is
built in a register, as for a displacement out of range before
(gen_place). A structure in the zone is cleared through R1. The prologue
macro takes a frame of any size (`A 15,=A(&FRAME)` from 4096 on).

A block whose variables end at 3072 or below is laid out as before.

## A repetition factor before a string in INITIAL

In INITIAL, `(n)` before a string constant is the string's repetition
factor, not an iteration factor (Q3INA, Q6IND):

| Declaration | Value |
|---|---|
| `CHAR(8) INIT((3)'AB')` | `'ABABAB  '` |
| `CHAR(8) VARYING INIT((3)'AB')` | `'ABABAB'`, length 6 |
| `CHAR(8) STATIC INIT((3)'AB')` | `'ABABAB  '` |
| `BIT(8) INIT((4)'1'B)` | `'11110000'B` |
| `BIT(8) STATIC INIT((4)'10'B)` | `'10101010'B` |
| `(4) CHAR(4) INIT((3)'AB')` | element 1 `'ABAB'` (truncated) |
| `(3) CHAR(6) STATIC INIT((2)'Q', 'R', 'S')` | `'QQ'`, `'R'`, `'S'` |

- **An iteration factor for strings** needs the string in parentheses:
  `(2)((2)'XY')` sets two elements to `'XYXY'` (Q5INC).
- **Before a number**, `(n)` is the iteration factor:
  `FIXED BIN(15) INIT((3)7)` sets three elements (Q4INB).
- **`INIT(((3)'AB'))` on a scalar** is refused: IEM0139I, the INITIAL
  attribute deleted (JOB00859).
- **The cases of Q3INA to Q6IND together in one procedure**, with that
  line removed, made PL/I (F) itself fail in phase CO (IEM3856I, program
  check type 1, JOB00860). `t173inb`, without Q3INA's scalars, compiles.

**PL1C/370** used to read `(3)'AB'` as an iteration factor, so the
scalar got `'AB'`. Now it keeps an `(n)` that is followed by a string or
bit constant as part of the value.

## Also: three procedures compiled apart, after ld370's fix

`t174sep` is Tier 48's Q2EXT, compiled apart. Of the case Tier 48 had
to avoid, the order of the procedures is:

1. the main procedure declares LEVEL without INITIAL (a common area);
2. SHOWIT declares it with INITIAL (a control section);
3. SQ declares it once more without INITIAL (a common area).

ld370 used to turn that control section back into a common area. With
cc370 1.6.2, which fixes this, `t174sep` is identical (JOB00867). It
differs from Q2EXT in one place, as `t156sep` does: INNER returns FIXED
BIN(15), what its invocation assumes from the name's first letter.
PL1C/370 refuses Q2EXT's FIXED BIN(31) there.

## Not done

- A variable of more than 4095 bytes is tested only through its
  elements (R in Q1BIGA, 5079 bytes). Operations on it as a whole are
  not tested.
