# Tier 90: OPTIONAL, BYVALUE

From the inventory (`mvsvm-inventory.md`), after SC26-3114-01 (#211).

## The rules

- **OPTIONAL** ("OPTIONAL Attribute", chapter 5): in the parameter
  descriptors of an ENTRY with OPTIONS(ASSEMBLER); `*` in the CALL omits
  the argument. "An omitted argument is indicated by a word of zeros in
  the argument list. If the omitted argument is also the last argument,
  the high order bit of the zero word is set to '1'B." Not with BYVALUE
  on the same parameter.
- **BYVALUE** (OPTIONS attribute, ENTRY statement): "parameters are
  received by value"; only scalar POINTER or REAL FIXED BINARY(31,0).
  For a procedure: "BYVALUE can be specified for EXTERNAL PROCEDURE
  statements ... Specifying BYVALUE for a MAIN procedure implies the
  specification of NOEXECOPS ... the parameter list is passed as is".

## In PL1C/370

- **parse.c:** OPTIONAL among a descriptor's attributes (`struct type`
  carries it); BYVALUE among an ENTRY's OPTIONS, with ASSEMBLER only;
  BYVALUE (and BYADDR) among a procedure's, on the MAIN procedure only.
- **check.c:** an `*` argument needs an OPTIONAL descriptor; a BYVALUE
  argument is POINTER or FIXED BINARY(31) (the descriptor's type where
  one is given); a BYVALUE MAIN procedure's parameters likewise.
- **gen.c, `gen_asmcall`:** an omitted argument is XC'd to zeros, or
  X'80000000' when it is the last; a BYVALUE argument's value is stored
  in the list; the last word of a list of values gets no high-order bit.
  That last point the manual does not state: a value with its high bit
  set would not be the value.
- **rt/pl1parm.c:** `PL1MPLS` with 65536 + n copies the n words R1
  points at as they are, the high bit included, and passes their
  addresses as the parameters.

## Tests

- `t241opt`: ASMTST's new function 5 returns ten times the number of
  arguments plus the number of words of zeros: `(5, *, *)` 32, `(5, A,
  *)` and `(5, *, A)` 31, `(5)` 10. ASMVAL (tests/mvs/asmsub) returns
  the sum of its two BYVALUE words: 42. PL/I (F) can do neither; the
  twin prints the values.
- `t242byv`: t226fct's FCONCAT with OPTIONS(MAIN BYVALUE), called by
  XDRIVER: EFPLARG is the argument table's address, EFPLEVAL the
  address of the word that holds the evaluation block's. The twin is
  t226fct's.
- Both, and t240asm, identical in JOB01498; the full suite 243 of 243
  (JOB01501 to JOB01515).

## Not done

- BYVALUE of an external procedure other than MAIN (both sides of the
  call would need it).
