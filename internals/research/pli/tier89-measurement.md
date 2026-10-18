# Tier 89: OPTIONS(ASSEMBLER), RETCODE, EXTERNAL('name')

Calls of assembler routines, from the inventory (`mvsvm-inventory.md`).

## The rules

- **OPTIONS(ASSEMBLER)** of an ENTRY (the Optimizer's: SC33-0006-7,
  chapter 11, "Arguments, Parameters, Returned Values and Return Codes";
  SC26-3114-01, chapter 5, "OPTIONS Attribute"):
  - "the parameter list set up by PL/I is to contain the addresses of
    actual data items, rather than the addresses of control blocks,
    irrespective of the types of data": an array's first element, an
    unaligned bit string's first byte, a VARYING string's 2-byte length
    prefix;
  - "The last fullword in the list has its high-order bit turned on";
  - invoked only by CALL, not as a function; from zero up to as many
    arguments as the declaration has, none omitted in between.
- **RETCODE:** "on return from the non-PL/I routine, the fullword value
  in register 15 is to be saved as the PL/I return code", which PLIRETV
  reads.
- **EXTERNAL('name')** (SC26-3114-01, chapter 6): the name the entry has
  outside, "only for OPTIONS(ASSEMBLER) entry constants", up to 8
  characters, not monocased.

## In PL1C/370

- **parse.c:** an ENTRY declaration takes OPTIONS(ASSEMBLER|ASM, RETCODE,
  INTER, BYADDR) and EXTERNAL('name'); another option is refused.
- **check.c:** an assembler entry's arguments are variables by address,
  other expressions a dummy -- of the descriptor's type where one is
  declared and differs. As a function reference it is an error.
- **gen.c, `gen_asmcall`:** the list of addresses, `OI` of X'80' on the
  last word, R1 at it (0 without arguments), `L 15,=V(name)` with the
  EXTERNAL name if given, BALR; with RETCODE R15 is stored in PL1RTCD.
- **The step's code with RETCODE:** the main procedure returns the PL/I
  return code at its end (Tier 87), so a RETCODE value set last becomes
  the step's code. The manuals call both "the PL/I return code"; that
  the step takes a RETCODE value is inferred from that, not stated.

## The test

`tests/mvs/asmsub/asmtst.asm`, ASMTST: the first argument a function code
(1: C = A + B; 2: 'HELLO' to the five bytes at the second address; 3:
nothing), R15 the number of arguments, counted by the high-order bit.
It is a module of its own in the development library, for PL/I (F)'s
link-edit to find by autocall (`//LKED.SYSLIB` with the library first,
SYS1.PL1LIB after it), and linked into the PL1C/370 module
(`<name>.asmsub`).

`t240asm`: the four FIXED BINARY(31) cases also run under PL/I (F), which
passes arithmetic arguments by address as well; the CHARACTER and
VARYING ones the twin prints as the manual gives them. Over the VARYING
string's prefix, 'HELLO' leaves the length X'C8C5' (-14139) and the
characters 'LLO'. The step ends with 2, the last RETCODE value
(`t240asm.cc`). Identical in JOB01478; the full suite 241 of 241
(JOB01481 to JOB01495).

## Not done

- OPTIONAL, BYVALUE (TODO.md).
- RETCODE without ASSEMBLER (for a PL/I entry it would read R15 too).
