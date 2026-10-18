# Tier 86: the cheap items of PL/I for MVS & VM

From the inventory (`mvsvm-inventory.md`, section 1), after
SC26-3114-01 (chapters 2, 7 and 16). Each is level MVS: `-lang=F`
refuses it. There is no compiler to measure them on; the test's PL/I (F)
twin prints the values the manual gives.

## The items

- **X constants** `'D7D361C9'X`: a character string, two hexadecimal
  digits a character, its EBCDIC code; an even number of digits; `''X`
  is the null string.
- **BX constants:** the same as B4.
- **%PROCESS:** a synonym of `*PROCESS`, the `%` in the first data
  position of the record.
- **SYSNULL, SYSNULL():** the system null pointer, whose binary value is
  0 (`BINVALUE(SYSNULL())` is 0); NULL and SYSNULL "do not compare
  equal".
- **POINTERADD(x, y), PTRADD:** x a pointer, y BIT, REAL FIXED BIN(p,0)
  or REAL FIXED DEC(p,0), converted to FIXED BIN(31); usable as a
  locator; a minus sign subtracts.
- **POINTERVALUE(x), PTRVALUE:** the pointer of a BIT or REAL FIXED
  (p,0) value.
- **BINARYVALUE(x), BINVALUE:** FIXED BIN(31) of a pointer.
- **DATETIME, DATETIME():** CHARACTER(17), yyyymmddhhmmssttt.

## In PL1C/370

- **lex.c:** after a string's closing quote, `X` makes it an X constant.
  The compiler keeps strings as Latin-1, and as370 turns a `C'..'`
  constant into EBCDIC with CP037. So each EBCDIC code is taken back to
  its Latin-1 character (`ebc_to_lat`, the new src/ebcdic.c, generated
  from Python's cp037 codec). `BX` goes the B4 way.
- **gen.c, `dc_chars`:** a character outside printable ASCII is written
  as `DC X'..'` with its CP037 code (`lat_to_ebc`), so that an X
  constant's control characters reach the object as they are. No test's
  assembler changed by it: none has such a character in a constant.
- **pp.c:** `%PROCESS` leaves `PROCESS ...;` to the parser, which takes
  it as it takes `*PROCESS`.
- **The built-ins** (resolve.c, check.c, gen.c): SYSNULL is `SR r,r`;
  POINTERADD adds the FIXED BIN(31) of y to the pointer; POINTERVALUE is
  the integer; BINARYVALUE the pointer's bits. DATETIME (rt/pl1date.c,
  PL1DTTM) takes date and time from one TIME macro, the century from the
  C digit of 0CYYDDDF, and the milliseconds as TIME has them, the
  hundredths times ten.
- **parse.c:** `SYSNULL()` and `DATETIME()` take the empty argument list
  SC26-3114-01 allows; any other name keeps PL/I (F)'s refusal
  (IEM0031I).

## Test

`t233mvs` (with its twin): every item, POINTERADD with a negative
operand and as a locator qualifier, DATETIME against DATE. Identical in
JOB01380; the full suite 234 of 234 (JOB01383 to JOB01393).

## Not done

- POINTERVALUE(constant) in a STATIC INITIAL (SC26-3114-01 allows it).
- GX constants (DBCS, excluded).
- If the compiler itself compares two string constants, it compares
  Latin-1, not EBCDIC; whether it ever does was not checked. An X
  constant is where that could show.
