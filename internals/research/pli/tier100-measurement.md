# Tier 100: internal functions, null argument lists, packed STATIC bits

Three gaps reported from a learning project (hellocp), each against the
language it belongs to.

## The rules and the measurements

- **An internal function's result.** SC26-3114-01 (RETURNS attribute):
  "If the RETURNS attribute is not specified for an external entry
  constant or an entry variable, the attributes for the value returned
  are set by default"; for an internal entry constant the RETURNS of
  its PROCEDURE applies. PL/I (F) takes the invoking block's default
  instead (CLAUDE.md; Q1BLT), so PL1C/370 refused the call without a
  declaration. Now the default language takes the PROCEDURE's RETURNS;
  `-lang=F` keeps the refusal. A declaration of the internal entry,
  which PL/I (F) needs, stays accepted (a deviation, SC26-3114-01 p. 19
  forbids it).
- **A null argument list, `F()`.** GC33-0009-4: a function reference
  with no arguments. PL/I (F) refuses it: IEM0031I, operand missing,
  and IEM0791I (probe with a line `IF NXT()
  THEN PUT SKIP LIST('ONE');` before `IF NXT THEN`, JOB02161; the
  probe as kept is the second run, without it). OPT level.
- **INITIAL of packed bit strings in STATIC storage** (arrays of BIT(n)
  and unaligned bit members of a structure): PL/I (F) has it,
  `BIT(10) STATIC INIT('1011'B)` is `'1011000000'B` (Q1EMP,
  JOB02162). PL1C/370 refused it. The values share bytes, so gen.c now
  gathers them in an image of the root, at the bit offsets gen_place()
  uses, and writes the touched bytes once.

## Tests

`t256fun` (`.opt.pli`: functions without declarations, `F()`; its
twin declares them and calls without `()`), `t257bit` (BIT(10) and
BIT(3) scalars, an array of BIT(10), a structure of BIT(3), BIT(10),
BIT(5)(2) and CHAR(2)): identical in JOB02164 and JOB02167. The full suite 255 of 258 in
JOB02170 to JOB02188, the three left lost to a 106-F fetching PL/I
(F)'s IEMGC on m38b and identical alone in JOB02191.
