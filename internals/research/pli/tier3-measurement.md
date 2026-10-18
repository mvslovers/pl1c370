# Tier 3 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes
`P10SCOP.pli`, oracle output beside them (JOB00088). The test programs
`tests/pli/t15proc` to `t18mod` print byte for byte what PL/I (F) prints
(JOB00100, together with Tiers 0 to 2).

## Arguments and the caller's view

PL/I (F) does not look at an internal procedure's PROCEDURE statement or
parameter declarations when it compiles a call. What it uses is the
**ENTRY declaration in the invoking block**, if there is one:

- With `DCL f ENTRY(descriptors)`, an argument whose attributes differ
  from its descriptor is converted into a dummy argument (IEM0802I "...
  DOES NOT MATCH THE PARAMETER. A DUMMY ARGUMENT HAS BEEN CREATED"):
  `TWICE(A)` with A FIXED BIN(31) and a FIXED DEC(7,2) parameter gives
  `18.00`.
- Without one, a variable is passed as it is, and a constant or expression
  as a dummy of its own attributes. A mismatch with the parameter then
  gives wrong values, with no message: `INC(4)` for a FIXED BIN(15)
  parameter passes the packed constant, and INC returns 19457 (JOB00088).
  **PL1C/370 refuses such a call**, naming the declaration to add, rather
  than imitate garbage.
- The value a function returns is taken with the attributes the invoking
  block declares (`DCL f RETURNS(...)`), else those of its name's first
  letter (I-N: FIXED BINARY(15)) -- measured in Tier 0, JOB00052. A
  mismatch with the procedure's RETURNS is refused by PL1C/370.

Variables are passed by reference: `CALL SWAP(A, B)` exchanges them,
`CALL SWAP(W(1), W(3))` array elements. An array can be passed whole.

## Blocks

- An internal procedure sees the variables of the blocks around it; its
  own declarations hide outer ones of the same name (`P10SCOP`: OUTER's N
  hides the main procedure's N, INNER changes OUTER's).
- A RECURSIVE procedure gets new AUTOMATIC storage per invocation
  (`FACT(10)` = 3628800).
- STATIC storage in a procedure keeps its value between calls.
- GO TO a label of the same procedure, backwards and forwards; RETURN
  leaves a procedure early.

## MOD

`MOD(x,y)` lies in [0,|y|) for both signs of y: MOD(17,5) = 2,
MOD(-17,5) = 3, MOD(17,-5) = 2, MOD(-17,-5) = 3 (JOB00096, t18mod). This
corrects a defect in Tier 1, where a positive remainder with a binary MOD
had y added (XR sets the condition code by zero, not by sign).

## Refused by PL/I (F)

- An empty argument list: `NEXTNO()` is IEM0031I "operand missing". A
  function without parameters is invoked by its name alone.
- A main procedure name longer than seven characters is truncated
  (IEM2867I, an error: condition code 8).

## Not measured yet

- GO TO out of a block; ON-units; BEGIN blocks.
- Parameters with `*` bounds or lengths; structures as arguments.
- ENTRY variables; external procedures.
