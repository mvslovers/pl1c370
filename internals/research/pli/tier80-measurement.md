# Tier 80: the main procedure's parameter, an element as control variable

Two things PL/I (F) takes and PL1C/370 refused, both met in the
BREXX/370 external functions of `~/repos/mvs/foo` (TODO.md item 1):
- `PROCEDURE(EFPL_PTR) OPTIONS(MAIN)`;
- `DO ARG(AI) = ...;` in RXCOMM.

## The main procedure's parameter

Measured before this tier (`main-parameter-measurement.md`, JOB01232,
JOB01233). PL/I (F)'s IHESAPA passes the PARM area as a string dope
vector:
- the characters' address, the area + 2;
- the maximum length 100;
- the current length.

A CHAR(100) VARYING parameter is the PARM. Any other type lies over the
vector, so a POINTER gets the characters' address.

The tests compare it with the program's own PARM (`<name>.goparm`, new:
`EXEC PGM=...,PARM='...'` and `PARM.GO`). Both are identical in
JOB01235:
- `t223mpm`, PARM `HELLO WORLD, 12`: LENGTH(PARM) is 15;
- `t224mpp`, a POINTER parameter: the 11 characters of `HELLO WORLD`
  read through it.

In PL1C/370:
- **parse.c:** a main procedure takes one parameter.
- **rt/pl1parm.c:**
  - `__premain`, libc370's startup hook (cc370#10), keeps the PARM area
    the startup found in the first word of R1's list;
  - `PL1MPRM` builds the argument list: for a VARYING parameter the
    area itself, which is PL1C/370's VARYING layout, otherwise the dope
    vector.
- **gen.c** calls PL1MPRM in the main procedure's prologue and puts the
  list in the frame's argument list word. Only such a program refers to
  it, so only it links the member.

## An element as the control variable

`DO A(K) = 7;` with one specification and no TO, BY or REPEAT runs its
group once. The same holds with WHILE, and the element is assigned
even when the WHILE is false:

```
DO A(3) = 9 WHILE(K = 5);
```

leaves A(3) = 9 in PL/I (F) too (`t223mpm`). RXCOMM's `ELSE DO` without
its semicolon is that form: `ELSE DO A(I) = I * 10;` in `t223mpm`.

In PL1C/370, **parse.c** recognizes a reference before `=` in DO
(`ref_then_eq`). Such a DO becomes `ref = e; [IF w THEN] DO; ...; END;`.

## Not done

- **A subscripted control variable with TO, BY, REPEAT or more than one
  specification:** refused.
- **A program LINKed as BREXX/370 does,** R1 at a list whose first word
  is A(EFPL) - 2. That needs a driver like the probe's of Tier 81 and comes
  with RXPI and RXCUT as tests (TODO.md item 1).

## The suite

All 226 programs are identical: JOB01238 to JOB01246.
